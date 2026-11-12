#!/usr/bin/env python3
"""Mine the hyperGuitar Bela rt_printf diag log (md/st lines) for tracker behaviour.

Usage: python3 mine_diag.py bela-live-diag.txt [--tail N]
Line formats (shifter.cpp):
  md s<str> <from>-><to> T<period> c<conf> nl<n> rl<n> g<gate> E<E0>   (every shifter mode change)
  st s<str> T<period> c<conf> m<mode> e<env> fA<x> g<gate> E<E0>      (every ~2 s per sounding string)
Modes 0=GRAINS 1=TRANS 2=DRIFT. Gate 0 ok, 1 release-blank, 2 silence/bleed, 3/4 no clean period, 6 waiting.
"""
import re, sys, math
from collections import Counter, defaultdict

OPEN_T = [674, 505, 378, 283, 225, 169]          # samples @44.1k, nominal C-F-Bb-Eb-G-C
NAMES = ["s0 C", "s1 F", "s2 Bb", "s3 Eb", "s4 G", "s5 C"]
TUNE_MARGIN, MAXFRET = 1.12, 4.5
GATE_E = 9e-8                                      # GATE_RMS^2
CONF = 0.92

MD = re.compile(r'^md s(\d) (\d)->(\d) T(-?[\d.]+) c([\d.]+) nl(\d+) rl(\d+) g(\d) E([\d.e+-]+)')
ST = re.compile(r'^st s(\d) T(-?[\d.]+) c([\d.]+) m(\d) e([\d.]+) fA([\d.]+) g(\d) E([\d.e+-]+)')

def cents(a, b):
    return 1200.0 * math.log2(a / b)

def pct(n, d):
    return "%.1f%%" % (100.0 * n / d) if d else "n/a"

def analyse(lines, label):
    md, st = [], []
    for i, ln in enumerate(lines):
        m = MD.match(ln)
        if m:
            s, f, t, T, c, nl, rl, g, E = m.groups()
            md.append(dict(i=i, s=int(s), f=int(f), t=int(t), T=float(T), c=float(c),
                           nl=int(nl), rl=int(rl), g=int(g), E=float(E)))
            continue
        m = ST.match(ln)
        if m:
            s, T, c, mo, e, fA, g, E = m.groups()
            st.append(dict(i=i, s=int(s), T=float(T), c=float(c), m=int(mo), e=float(e),
                           fA=float(fA), g=int(g), E=float(E)))
    out = []
    P = out.append
    P(f"\n{'='*78}\n{label}: {len(lines)} lines, {len(md)} md, {len(st)} st, "
      f"{sum(1 for l in lines if l.startswith('==='))} restarts, "
      f"{sum(1 for l in lines if l.startswith('Underrun'))} underrun lines")

    # ---------- A. transitions per string, by type, audible vs silent ----------
    P("\n[A] shifter mode transitions per string (md lines). audible = E>=9e-8 (GATE_RMS^2)")
    P(f"{'string':7} {'all':>6} {'->TRANS(pick)':>13} {'TR->GR':>7} {'TR->DR':>7} {'GR->DR':>7} {'DR->GR':>7} "
      f"{'audible':>8} {'g2(bleed/sil)':>13} {'conf>=.92':>9}")
    for s in range(6):
        ms = [m for m in md if m['s'] == s]
        n = len(ms)
        P(f"{NAMES[s]:7} {n:6} {sum(1 for m in ms if m['t']==1):13} "
          f"{sum(1 for m in ms if m['f']==1 and m['t']==0):7} {sum(1 for m in ms if m['f']==1 and m['t']==2):7} "
          f"{sum(1 for m in ms if m['f']==0 and m['t']==2):7} {sum(1 for m in ms if m['f']==2 and m['t']==0):7} "
          f"{pct(sum(1 for m in ms if m['E']>=GATE_E), n):>8} {pct(sum(1 for m in ms if m['g']==2), n):>13} "
          f"{pct(sum(1 for m in ms if m['c']>=CONF), n):>9}")

    # ---------- B. T distribution on confident transitions ----------
    P("\n[B] period T on CONFIDENT (c>=0.92) transitions vs the string's expected range")
    P("    below_open = T > 1.00*open (note flatter than nominal open: flat tuning or neighbour's lower note)")
    P("    out_of_range = T > 1.12*open or T < open/4.5 (should be impossible: period is clamped)")
    P(f"{'string':7} {'n':>6} {'in_range':>8} {'below_open':>10} {'out_rng':>7} {'near_open±3%':>12} "
      f"{'oct_above_open':>14} {'median_cents_vs_open':>20} {'p10':>6} {'p90':>6}")
    conf_T = defaultdict(list)
    for m in md:
        if m['c'] >= CONF and m['T'] > 0:
            conf_T[m['s']].append(m['T'])
    for s in range(6):
        Ts = conf_T[s]; n = len(Ts); o = OPEN_T[s]
        if not n:
            P(f"{NAMES[s]:7} {0:6}"); continue
        inr = sum(1 for T in Ts if o / MAXFRET <= T <= o * TUNE_MARGIN)
        below = sum(1 for T in Ts if T > o * 1.0)
        outr = sum(1 for T in Ts if T > o * TUNE_MARGIN or T < o / MAXFRET)
        near = sum(1 for T in Ts if abs(T - o) < 0.03 * o)
        oct_ = sum(1 for T in Ts if abs(T - o / 2) < 0.03 * o / 2)
        cs = sorted(cents(o, T) for T in Ts)   # + = above open pitch
        P(f"{NAMES[s]:7} {n:6} {pct(inr,n):>8} {pct(below,n):>10} {pct(outr,n):>7} {pct(near,n):>12} "
          f"{pct(oct_,n):>14} {cs[n//2]:>20.0f} {cs[n//10]:>6.0f} {cs[(9*n)//10]:>6.0f}")
    # semitone histogram per string (confident T)
    P("\n    semitone histogram of confident T relative to nominal open (negative = below open). top 8 bins/string")
    for s in range(6):
        Ts = conf_T[s]; o = OPEN_T[s]
        if not Ts: continue
        h = Counter(int(round(cents(o, T) / 100.0)) for T in Ts)
        tot = len(Ts)
        P(f"    {NAMES[s]:7} " + "  ".join(f"{k:+d}st:{pct(v,tot)}" for k, v in sorted(h.items(), key=lambda kv: -kv[1])[:8]))

    # ---------- C. co-locks with neighbours ----------
    P("\n[C] CO-LOCKS: confident transition whose T matches (±2%) a neighbour's most recent T (any md/st line "
      "within the previous/next 12 lines). oct = neighbour T is 2:1 or 1:2 (±2%).")
    # build per-line event list ordered by index
    ev = sorted([dict(i=m['i'], s=m['s'], T=m['T'], c=m['c']) for m in md if m['T'] > 0] +
                [dict(i=x['i'], s=x['s'], T=x['T'], c=x['c']) for x in st if x['T'] > 0], key=lambda e: e['i'])
    idx_by_line = {}
    for k, e in enumerate(ev):
        idx_by_line[e['i']] = k
    WIN = 12
    co = defaultdict(lambda: Counter())
    pair = Counter()
    pairT = defaultdict(Counter)
    for m in md:
        if m['c'] < CONF or m['T'] <= 0: continue
        k = idx_by_line[m['i']]
        s = m['s']
        seen = set()
        kk = k - 1
        while kk >= 0 and m['i'] - ev[kk]['i'] <= WIN:
            e = ev[kk]; kk -= 1
            if e['s'] == s or e['s'] in seen: continue
            seen.add(e['s'])
            r = e['T'] / m['T']
            d = abs(e['s'] - s)
            if abs(r - 1) < 0.02:
                co[s]['uni_adj' if d == 1 else ('uni_d2' if d == 2 else 'uni_far')] += 1
                if d == 1:
                    pair[(min(s, e['s']), max(s, e['s']))] += 1
                    pairT[(min(s, e['s']), max(s, e['s']))][int(round(m['T']))] += 1
            elif abs(r - 2) < 0.04 or abs(r - 0.5) < 0.01:
                co[s]['oct_adj' if d == 1 else 'oct_other'] += 1
        kk = k + 1
        while kk < len(ev) and ev[kk]['i'] - m['i'] <= WIN:
            e = ev[kk]; kk += 1
            if e['s'] == s or e['s'] in seen: continue
            seen.add(e['s'])
            r = e['T'] / m['T']
            d = abs(e['s'] - s)
            if abs(r - 1) < 0.02:
                co[s]['uni_adj' if d == 1 else ('uni_d2' if d == 2 else 'uni_far')] += 1
                if d == 1:
                    pair[(min(s, e['s']), max(s, e['s']))] += 1
                    pairT[(min(s, e['s']), max(s, e['s']))][int(round(m['T']))] += 1
            elif abs(r - 2) < 0.04 or abs(r - 0.5) < 0.01:
                co[s]['oct_adj' if d == 1 else 'oct_other'] += 1
        co[s]['n'] += 1
    P(f"{'string':7} {'conf_trans':>10} {'unison_adj':>10} {'unison_d2':>9} {'unison_far':>10} {'oct_adj':>8} {'oct_other':>9}")
    for s in range(6):
        c = co[s]; n = c['n']
        P(f"{NAMES[s]:7} {n:10} {pct(c['uni_adj'],n):>10} {pct(c['uni_d2'],n):>9} {pct(c['uni_far'],n):>10} "
          f"{pct(c['oct_adj'],n):>8} {pct(c['oct_other'],n):>9}")
    P("    adjacent unison pairs (count, top shared T values):")
    for (a, b), n in sorted(pair.items(), key=lambda kv: -kv[1]):
        top = ", ".join(f"T{T}x{c}" for T, c in pairT[(a, b)].most_common(6))
        P(f"      s{a}-s{b}: {n:6}   {top}")

    # confident transitions where T is actually a NEIGHBOUR's open-string note (within ±3%)
    P("\n    confident T equal (±3%) to an ADJACENT string's nominal open period (the classic bleed lock):")
    for s in range(6):
        Ts = conf_T[s]; n = len(Ts)
        if not n: continue
        row = []
        for o in (s - 1, s + 1):
            if 0 <= o < 6:
                k = sum(1 for T in Ts if abs(T - OPEN_T[o]) < 0.03 * OPEN_T[o])
                row.append(f"=open(s{o}) {pct(k,n)}")
        P(f"    {NAMES[s]:7} " + "   ".join(row))

    # ---------- D. attack resolution: TRANS exits ----------
    P("\n[D] how TRANS (dry attack pass-through) ends. rl>=3 = relocked; rl<3 at ->GRAINS = dead-zone escape "
      "(transAge>25 hops = 73 ms, conf hovered 0.45..0.92); ->DRIFT = never resolved (nl>18) or dead zone w/o marks")
    P(f"{'string':7} {'TRexits':>7} {'->GR rl>=3':>10} {'->GR deadzone':>13} {'->DRIFT':>8} {'->DR audible':>12} "
      f"{'median c@->DR':>13} {'->DR g0':>8} {'->DR g2':>8} {'->DR g3/4':>9} {'->DR g6':>8}")
    for s in range(6):
        ex = [m for m in md if m['s'] == s and m['f'] == 1]
        n = len(ex)
        gr = [m for m in ex if m['t'] == 0]
        dr = [m for m in ex if m['t'] == 2]
        cs = sorted(m['c'] for m in dr)
        P(f"{NAMES[s]:7} {n:7} {pct(sum(1 for m in gr if m['rl']>=3),n):>10} "
          f"{pct(sum(1 for m in gr if m['rl']<3),n):>13} {pct(len(dr),n):>8} "
          f"{pct(sum(1 for m in dr if m['E']>=GATE_E),n):>12} "
          f"{(cs[len(cs)//2] if cs else float('nan')):>13.2f} "
          f"{pct(sum(1 for m in dr if m['g']==0),len(dr)):>8} {pct(sum(1 for m in dr if m['g']==2),len(dr)):>8} "
          f"{pct(sum(1 for m in dr if m['g'] in (3,4)),len(dr)):>9} {pct(sum(1 for m in dr if m['g']==6),len(dr)):>8}")
    P("    nl at TRANS->DRIFT (hops of conf<0.45 since pick), all strings: " +
      ", ".join(f"{k}:{v}" for k, v in sorted(Counter(m['nl'] for m in md if m['f']==1 and m['t']==2).items())[:12]))
    P("    rl at TRANS->GRAINS: " +
      ", ".join(f"{k}:{v}" for k, v in sorted(Counter(m['rl'] for m in md if m['f']==1 and m['t']==0).items())))

    # ---------- E. DRIFT during real signal ----------
    P("\n[E] falls into DRIFT (mode 2) from GRAINS, with the tracker saying g0 (real signal, no clean lock) and E audible")
    P(f"{'string':7} {'GR->DR':>7} {'g0':>6} {'g0&audible':>11} {'g2':>6} {'g3/4':>6} {'g1':>5} {'median E (g0)':>14} {'median c (g0)':>14}")
    for s in range(6):
        d = [m for m in md if m['s'] == s and m['f'] == 0 and m['t'] == 2]
        n = len(d)
        g0 = [m for m in d if m['g'] == 0]
        Es = sorted(m['E'] for m in g0); cs = sorted(m['c'] for m in g0)
        P(f"{NAMES[s]:7} {n:7} {pct(len(g0),n):>6} {pct(sum(1 for m in g0 if m['E']>=GATE_E),n):>11} "
          f"{pct(sum(1 for m in d if m['g']==2),n):>6} {pct(sum(1 for m in d if m['g'] in (3,4)),n):>6} "
          f"{pct(sum(1 for m in d if m['g']==1),n):>5} "
          f"{(Es[len(Es)//2] if Es else float('nan')):>14.1e} {(cs[len(cs)//2] if cs else float('nan')):>14.2f}")

    # ---------- F. steady-state st lines: lock quality while sounding ----------
    P("\n[F] 'st' snapshots while env>0.01 (sounding): mode residency, confidence, gate")
    P(f"{'string':7} {'n':>6} {'GRAINS':>7} {'TRANS':>6} {'DRIFT':>6} {'c>=.92':>7} {'c<.45':>6} {'g0':>6} {'g2':>6} "
      f"{'g3/4':>6} {'median env':>10} {'median E':>10} {'median c':>9}")
    for s in range(6):
        xs = [x for x in st if x['s'] == s and x['e'] > 0.01]
        n = len(xs)
        if not n:
            P(f"{NAMES[s]:7} {0:6}"); continue
        es = sorted(x['e'] for x in xs); Es = sorted(x['E'] for x in xs); cs = sorted(x['c'] for x in xs)
        P(f"{NAMES[s]:7} {n:6} {pct(sum(1 for x in xs if x['m']==0),n):>7} {pct(sum(1 for x in xs if x['m']==1),n):>6} "
          f"{pct(sum(1 for x in xs if x['m']==2),n):>6} {pct(sum(1 for x in xs if x['c']>=CONF),n):>7} "
          f"{pct(sum(1 for x in xs if x['c']<0.45),n):>6} {pct(sum(1 for x in xs if x['g']==0),n):>6} "
          f"{pct(sum(1 for x in xs if x['g']==2),n):>6} {pct(sum(1 for x in xs if x['g'] in (3,4)),n):>6} "
          f"{es[n//2]:>10.3f} {Es[n//2]:>10.1e} {cs[n//2]:>9.2f}")
    # st lines in DRIFT with g0 (audible dropout of the shifted chain)
    P("    st in DRIFT while sounding, by gate: " + ", ".join(
        f"g{g}:{c}" for g, c in sorted(Counter(x['g'] for x in st if x['e'] > 0.01 and x['m'] == 2).items())))
    # st confident T semitone histogram (steady-state notes actually held)
    P("    st confident (c>=.92) T, semitones vs nominal open, top 6 bins:")
    for s in range(6):
        Ts = [x['T'] for x in st if x['s'] == s and x['c'] >= CONF and x['T'] > 0]
        if not Ts: continue
        h = Counter(int(round(cents(OPEN_T[s], T) / 100.0)) for T in Ts); tot = len(Ts)
        below = sum(1 for T in Ts if T > OPEN_T[s] * 1.0)
        P(f"    {NAMES[s]:7} n={tot:5} below_open={pct(below,tot):>6}  " +
          "  ".join(f"{k:+d}st:{pct(v,tot)}" for k, v in sorted(h.items(), key=lambda kv: -kv[1])[:6]))

    # ---------- G. simultaneous picks (onsets) on neighbours = bleed-triggered onsets ----------
    P("\n[G] onsets (->TRANS): fraction with an ADJACENT string's onset within ±3 lines (strum or bleed-triggered)")
    ons = [m for m in md if m['t'] == 1]
    ons_by_line = sorted(ons, key=lambda m: m['i'])
    for s in range(6):
        mine = [m for m in ons if m['s'] == s]
        n = len(mine); k = 0; kq = 0
        for m in mine:
            near = [o for o in ons_by_line if abs(o['i'] - m['i']) <= 3 and o['s'] != m['s']]
            if any(abs(o['s'] - s) == 1 for o in near): k += 1
            if m['E'] < GATE_E: kq += 1
        P(f"    {NAMES[s]:7} onsets={n:6}  adj_onset_within_3_lines={pct(k,n):>6}  onset_with_E<gate={pct(kq,n):>6}")

    return "\n".join(out)


def main():
    path = sys.argv[1]
    tail = None
    if '--tail' in sys.argv:
        tail = int(sys.argv[sys.argv.index('--tail') + 1])
    with open(path, errors='replace') as f:
        lines = [l.rstrip('\n') for l in f]
    print(analyse(lines, "ALL SESSIONS"))
    if tail:
        print(analyse(lines[-tail:], f"LAST {tail} LINES (most recent, tracker v2 era)"))


if __name__ == '__main__':
    main()


# ---------------------------------------------------------------------------
# [H] extra queries appended after the first run
# ---------------------------------------------------------------------------
def extra(lines, label):
    md = []
    st = []
    for i, ln in enumerate(lines):
        m = MD.match(ln)
        if m:
            s, f, t, T, c, nl, rl, g, E = m.groups()
            md.append(dict(i=i, s=int(s), f=int(f), t=int(t), T=float(T), c=float(c),
                           nl=int(nl), rl=int(rl), g=int(g), E=float(E)))
            continue
        m = ST.match(ln)
        if m:
            s, T, c, mo, e, fA, g, E = m.groups()
            st.append(dict(i=i, s=int(s), T=float(T), c=float(c), m=int(mo), e=float(e), g=int(g), E=float(E)))
    out = []
    P = out.append
    P(f"\n{'='*78}\n[H] EXTRA — {label}")
    # H1: TRANS->GRAINS with rl=0: which exit path? c at exit, gate, audible
    P("\n[H1] TRANS->GRAINS exits by rl. rl=0 can only be the pitchIdle (no pitch shift active, >6 hops) path or the "
      "dead-zone (>25 hops) path; c at that hop is <0.92 by construction.")
    P(f"{'string':7} {'rl=0':>6} {'c<.45':>6} {'.45-.75':>8} {'.75-.92':>8} {'g0':>6} {'g2':>6} {'g3/4':>6} {'g6':>5} {'audible':>8} "
      f"| {'rl1-2':>6} {'rl=3':>6} {'rl3 audible':>11} {'rl3 g0':>7}")
    for s in range(6):
        ex0 = [m for m in md if m['s'] == s and m['f'] == 1 and m['t'] == 0 and m['rl'] == 0]
        ex12 = [m for m in md if m['s'] == s and m['f'] == 1 and m['t'] == 0 and 1 <= m['rl'] <= 2]
        ex3 = [m for m in md if m['s'] == s and m['f'] == 1 and m['t'] == 0 and m['rl'] >= 3]
        n = len(ex0); n3 = len(ex3)
        P(f"{NAMES[s]:7} {n:6} {pct(sum(1 for m in ex0 if m['c']<0.45),n):>6} "
          f"{pct(sum(1 for m in ex0 if 0.45<=m['c']<0.75),n):>8} {pct(sum(1 for m in ex0 if 0.75<=m['c']<0.92),n):>8} "
          f"{pct(sum(1 for m in ex0 if m['g']==0),n):>6} {pct(sum(1 for m in ex0 if m['g']==2),n):>6} "
          f"{pct(sum(1 for m in ex0 if m['g'] in (3,4)),n):>6} {pct(sum(1 for m in ex0 if m['g']==6),n):>5} "
          f"{pct(sum(1 for m in ex0 if m['E']>=GATE_E),n):>8} | {len(ex12):6} {n3:6} "
          f"{pct(sum(1 for m in ex3 if m['E']>=GATE_E),n3):>11} {pct(sum(1 for m in ex3 if m['g']==0),n3):>7}")
    # H2: full nl histogram at TRANS->DRIFT
    dr = [m for m in md if m['f'] == 1 and m['t'] == 2]
    h = Counter(m['nl'] for m in dr)
    P(f"\n[H2] TRANS->DRIFT: n={len(dr)}  nl>18 (never resolved, >=52 ms aperiodic): {pct(sum(v for k,v in h.items() if k>18),len(dr))}  "
      f"nl<=18 (dead zone >73 ms, marks dead): {pct(sum(v for k,v in h.items() if k<=18),len(dr))}")
    P("     nl hist: " + ", ".join(f"{k}:{v}" for k, v in sorted(h.items())))
    P("     by string, share of TR->DR that is SILENT (E<gate) = onset fired on a string with no note of its own: " +
      ", ".join(f"s{s}:{pct(sum(1 for m in dr if m['s']==s and m['E']<GATE_E), sum(1 for m in dr if m['s']==s))}" for s in range(6)))
    # H3: all TRANS exits that are silent (E<gate) or g2 -> bleed-triggered onsets
    P("\n[H3] ALL TRANS exits (= onsets) whose exit hop is E<GATE_RMS^2 or g2: onsets on a string that was not really sounding")
    for s in range(6):
        ex = [m for m in md if m['s'] == s and m['f'] == 1]
        n = len(ex)
        P(f"    {NAMES[s]:7} onsets={n:6}  E<gate={pct(sum(1 for m in ex if m['E']<GATE_E),n):>6}  g2={pct(sum(1 for m in ex if m['g']==2),n):>6}  "
          f"E<gate&&->GRAINS rl3 (confident lock on a silent string!)={sum(1 for m in ex if m['E']<GATE_E and m['t']==0 and m['rl']>=3)}")
    # H4: strict co-lock: both confident md lines within 12 lines, adjacent strings
    P("\n[H4] STRICT co-locks: two CONFIDENT (c>=.92) md lines on ADJACENT strings within 12 lines with T within ±2%")
    conf = [m for m in md if m['c'] >= CONF and m['T'] > 0]
    conf.sort(key=lambda m: m['i'])
    strict = Counter(); tot = Counter(); strictE = defaultdict(list)
    for k, m in enumerate(conf):
        tot[m['s']] += 1
        hit = False
        for j in range(k - 1, -1, -1):
            e = conf[j]
            if m['i'] - e['i'] > 12: break
            if abs(e['s'] - m['s']) == 1 and abs(e['T'] / m['T'] - 1) < 0.02:
                hit = True; break
        if not hit:
            for j in range(k + 1, len(conf)):
                e = conf[j]
                if e['i'] - m['i'] > 12: break
                if abs(e['s'] - m['s']) == 1 and abs(e['T'] / m['T'] - 1) < 0.02:
                    hit = True; break
        if hit:
            strict[m['s']] += 1
            strictE[m['s']].append(m['E'])
    for s in range(6):
        Es = sorted(strictE[s])
        P(f"    {NAMES[s]:7} confident={tot[s]:6}  strict_adjacent_unison={pct(strict[s],tot[s]):>6}  "
          f"median E of co-locked={Es[len(Es)//2] if Es else float('nan'):.1e}  share E<gate={pct(sum(1 for E in Es if E<GATE_E),len(Es))}")
    # H5: E (level) of confident locks per string: how loud is the string when it claims a note
    P("\n[H5] level E on confident md lines, per string (p10/p50/p90) and share under 10x gate")
    for s in range(6):
        Es = sorted(m['E'] for m in md if m['s'] == s and m['c'] >= CONF)
        n = len(Es)
        if n:
            P(f"    {NAMES[s]:7} p10={Es[n//10]:.1e} p50={Es[n//2]:.1e} p90={Es[9*n//10]:.1e}  E<9e-7:{pct(sum(1 for E in Es if E<9e-7),n)}  E<9e-8:{pct(sum(1 for E in Es if E<GATE_E),n)}")
    return "\n".join(out)


if __name__ == '__main__' and '--extra' in sys.argv:
    path = sys.argv[1]
    with open(path, errors='replace') as f:
        lines = [l.rstrip('\n') for l in f]
    print(extra(lines, "ALL"))
    print(extra(lines[-110000:], "LAST 110000"))
