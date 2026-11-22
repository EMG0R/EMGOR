#!/usr/bin/env python
"""Read the HYPERTRUMPET V2 benchmark stream from the Seed (or a saved .jsonl) and print a table.
  ../../.venv/bin/python bench_read.py                 # auto-detect the Seed CDC port, collect until state=done, print table
  ../../.venv/bin/python bench_read.py --run           # also send 'run' (restart the whole run) after each (re)connect
  ../../.venv/bin/python bench_read.py --file bench_capture.jsonl
Robust by design: asserts DTR/RTS, reconnects when the port disappears or goes quiet for 6 s (USB re-enumeration, fault reset), sends
'hello' after every connect (records are replayed round-robin, so a late start still gets them), appends EVERY raw line to
bench_capture.jsonl as it arrives, and prints the table from whatever was collected on Ctrl-C / timeout / done.
Options: --port P  --capture F (default bench_capture.jsonl)  --tone 0|1  --timeout S (default 600)
Columns: avg/peak = % of the 1 ms block budget (cycles of FxEngine::process / (48/48000 s * clk)); net = avg minus the 'none' baseline;
m_max = libDaisy CpuLoadMeter peak over the WHOLE callback; over = blocks > budget; OVER = overload guard tripped (>95 % avg over 100 blocks, case aborted)."""
import sys, json, time, glob, argparse

ap = argparse.ArgumentParser()
ap.add_argument('--port'); ap.add_argument('--file'); ap.add_argument('--capture', default='bench_capture.jsonl'); ap.add_argument('--run', action='store_true')
ap.add_argument('--tone', type=int); ap.add_argument('--timeout', type=float, default=600)
a = ap.parse_args()

recs = {}; hello = {}; faults = []

def handle(l):
    global hello
    try: m = json.loads(l)
    except Exception: return
    t = m.get('t')
    if t == 'hello':
        hello = m
        if m.get('fault') and m['fault'] not in faults:
            faults.append(m['fault']); print('\n!! FAULT reported by firmware: %s' % json.dumps(m['fault']), file=sys.stderr)
        if m.get('state') == 'running':
            print('\r[%d/%d] %s   ' % (m.get('case', 0), m.get('ncase', 0), m.get('cur', '')), end='', file=sys.stderr, flush=True)
    elif t == 'r': recs[m['i']] = m

def finished():
    return hello.get('state') == 'done' and recs and len(recs) >= hello.get('nrec', 1)

def port_name():
    if a.port: return a.port if glob.glob(a.port) else None
    return next((p for p in sorted(glob.glob('/dev/cu.usbmodem*')) if '4488BED64B3B' not in p), None)   # never the Bela

def collect_live():
    import serial
    cap = open(a.capture, 'a'); t_end = time.time() + a.timeout; first = True
    try:
        while time.time() < t_end and not finished():
            p = port_name()
            if not p: time.sleep(0.3); continue
            try:
                s = serial.Serial(p, 115200, timeout=0.3); s.dtr = True; s.rts = True
            except Exception: time.sleep(0.3); continue
            print('\nconnected %s' % p, file=sys.stderr)
            time.sleep(0.3)
            try:
                if a.tone is not None and first: s.write(b'tone %d\n' % a.tone)
                if a.run and first: s.write(b'run\n')
                s.write(b'hello\n'); first = False
                buf = b''; last = time.time()
                while time.time() < t_end and not finished():
                    d = s.read(4096)
                    if d: last = time.time(); buf += d
                    elif time.time() - last > 6.0: print('\nno data for 6 s, reconnecting', file=sys.stderr); break
                    while b'\n' in buf:
                        l, buf = buf.split(b'\n', 1); l = l.decode('utf8', 'replace').strip()
                        if l: cap.write(l + '\n'); cap.flush(); handle(l)
            except Exception as e:
                print('\nport error (%s), reconnecting' % e, file=sys.stderr)
            finally:
                try: s.close()
                except Exception: pass
            time.sleep(0.5)
    except KeyboardInterrupt: print('\ninterrupted', file=sys.stderr)
    finally: cap.close()

if a.file:
    for l in open(a.file): handle(l.strip())
else:
    collect_live()
print(file=sys.stderr)
if not recs: sys.exit('no records (capture in %s)' % a.capture)
base = recs.get(0, {}).get('avg', 0.0)
print('fw %s  clk %.0f MHz  budget %d cyc/block  src %s  looper %d s/layer  trips %s  state %s  records %d/%s' % (hello.get('fw'), hello.get('clk', 0) / 1e6, hello.get('budget_cyc', 0), hello.get('src'), hello.get('loop_s', 0), hello.get('trips'), hello.get('state'), len(recs), hello.get('ncase')))
for f in faults: print('FAULT:', json.dumps(f), ' -> arm-none-eabi-addr2line -f -C -e build/hx_bench.elf', f.get('pc'))
print('| # | case | avg % | net % | peak % | m_max % | over | slots avg % | note |'); print('|---|---|---|---|---|---|---|---|---|')
for i in sorted(recs):
    r = recs[i]; sl = '/'.join('%.1f' % x for x in r['slot']) if r['fx'] and len(r['fx']) == 4 else ''
    extra = (' map_us=%.1f' % r['map_us']) if r.get('map_us') else ''
    flag = ' **OVER (aborted)**' if r.get('trip') else ('' if r['ok'] else ' **NOT OK**')
    print('| %d | %s | %.2f | %.2f | %.2f | %.1f | %d | %s | %s%s%s |' % (i, r['label'], r['avg'], r['avg'] - base if i else 0, r['peak'], r['m_max'], r['over'], sl, r['note'], extra, flag))
