#!/usr/bin/env bash
# hwtest.sh -- one command for the PMOR-on-the-Mac hardware check. Run it with PMOR plugged in and NOTHING else holding its CDC port.
#   max/hwtest.sh [--port /dev/cu.usbmodemXXXX] [--bpm 124] [--bars 16] [--no-follow] [--yes] [--until-live] [--follow-only]
#   --until-live  run steps 1-3, then print the Live steps and stop;   --follow-only  skip steps 2-3 (run after Live is set up, add --yes to skip the prompt)
# 1. finds the PMOR CDC port (USB VID cafe / PID 4f70, "EMGOR SYNTH"), refuses to start if any process holds it (lsof)
# 2. pmorctl port test            (protocol suite on hardware)
# 3. hostclient_hw                (max/pmor_host.h against the real device)
# 4. follower test vs Ableton: you set Live to <bpm>, route Live's MIDI clock/sync OUT to the PMOR USB-MIDI port, then press PLAY; one STUDIO session polls
#    STATUS; the reading is taken >= <bars> bars after Start (the firmware ring = last 256 phase corrections, one per 16th = 16 bars, resets on Start).
#    ferr_us = |next grid step - now - step_period| wrapped to +-step/2, measured PMOR-side (usb/clock_engine.c). Baseline vs the NEPTR clock at 124 BPM:
#    mean 0.31 ms / p99 1.02 ms.  (Host-side simulation of the same code: tools/follower_sim.sh.)
# Only this script's own tools open the port. Nothing is written to a patch (NOSAVE 1), no firmware is touched.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/.."; B="${PMOR_BUILD:-$root/build_host}"
PORT=""; BPM=124; BARS=16; FOLLOW=1; YES=0; SUITE=1; UNTIL_LIVE=0
while [ $# -gt 0 ]; do case "$1" in
  --port) PORT="$2"; shift 2;; --bpm) BPM="$2"; shift 2;; --bars) BARS="$2"; shift 2;; --no-follow) FOLLOW=0; shift;; --yes) YES=1; shift;; --follow-only) SUITE=0; shift;; --until-live) UNTIL_LIVE=1; shift;;
  -h|--help) sed -n 2,16p "$0"; exit 0;; *) echo "unknown arg $1"; exit 2;; esac; done
BASE_MEAN=0.31; BASE_P99=1.02
say() { printf '%s\n' "$*"; }
die() { say "ABORT: $*"; exit 2; }

# ---- tools
if [ ! -x "$B/pmorctl" ] || { [ $SUITE = 1 ] && [ ! -x "$B/hostclient_hw" ]; }; then
  say "building host tools..."; cmake -S "$root" -B "$B" -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build "$B" -j8 >/dev/null || die "build failed"
fi

# ---- 1. find the port
if [ -z "$PORT" ]; then
  # ioreg: the IOCalloutDevice that appears after an idVendor=0xcafe (51966) / idProduct 0x4f70 (20336) USB node, before the next USB node
  PORT=$(ioreg -l -w0 2>/dev/null | python3 -c "
import sys, re
f = 0
for l in sys.stdin:
    m = re.search(r'\"idVendor\" = (\d+)', l)
    if m: f = 1 if m.group(1) == '51966' else 0
    m = re.search(r'\"IOCalloutDevice\" = \"(/dev/cu\.usbmodem[^\"]+)\"', l)
    if m and f: print(m.group(1)); break
")
  if [ -z "$PORT" ]; then
    cand=$(ls /dev/cu.usbmodem* 2>/dev/null); n=$(printf '%s' "$cand" | grep -c .)
    [ "$n" = 1 ] && PORT="$cand" && say "note: PMOR not matched by USB id; using the only usbmodem port"
  fi
fi
[ -n "$PORT" ] && [ -e "$PORT" ] || die "no PMOR CDC port found (plug it in, or pass --port). Ports seen: $(ls /dev/cu.usbmodem* 2>/dev/null | tr '\n' ' ')"
holders=$(lsof "$PORT" "${PORT/cu./tty.}" 2>/dev/null | awk 'NR>1{print $1" (pid "$2")"}' | sort -u)
[ -z "$holders" ] || die "$PORT is held by: $holders. Close it (Max devices with connect on, screen, other tools) first; only this script may open the port."
say "PMOR port: $PORT (free)"
res=(); fail=0
note() { res+=("$1"); case "$1" in FAIL*) fail=$((fail+1));; esac; }

if [ $SUITE = 1 ]; then
# ---- 2. protocol suite
say; say "== pmorctl port test"
out=$("$B/pmorctl" port "$PORT" test 2>&1); rc=$?; echo "$out" | tail -4
line=$(echo "$out" | grep -E "pmorctl port: [0-9]+ passed" | tail -1)
[ $rc -eq 0 ] && note "PASS  protocol suite: ${line#pmorctl port: }" || note "FAIL  protocol suite: ${line:-no summary (rc=$rc)}"
sleep 1
# ---- 3. host client
say; say "== hostclient_hw"
out=$("$B/hostclient_hw" "$PORT" 2>&1); rc=$?; echo "$out" | tail -4
line=$(echo "$out" | grep -E "passed" | tail -1)
[ $rc -eq 0 ] && note "PASS  host client: ${line}" || note "FAIL  host client: ${line:-no summary (rc=$rc)}"
sleep 1
fi

live_steps() {
  say "  Live (EMGOR, 3 steps):"
  say "   1. Preferences > Link, Tempo & MIDI: on the PMOR USB-MIDI port set Output > Sync ON (Track and Remote off)."
  say "   2. Set the tempo to ${BPM} BPM, transport stopped at bar 1."
  say "   3. Press PLAY, leave it for >= ${BARS} bars (~$((BARS * 4 * 60 / BPM + 2)) s)."
}
if [ $UNTIL_LIVE = 1 ]; then
  say; say "== stopped before the Live clock step"; live_steps
  say "  Then run:  max/hwtest.sh --follow-only --yes --bpm ${BPM}   (start it, then press PLAY within 20 s; it polls STATUS for ~$((BARS * 4 * 60 / BPM + 45)) s)"
  FOLLOW=0
fi
# ---- 4. follower vs Live clock
if [ $FOLLOW = 1 ]; then
  say; say "== follower vs Ableton clock (${BPM} BPM, >=${BARS} bars)"
  live_steps
  say "  Press Enter, then PLAY in Live within 20 s."
  [ $YES = 1 ] || read -r -p "  Enter when Live is ready... " _
  secs=$(( BARS * 4 * 60 / BPM + 45 ))
  args=("HELLO STUDIO 1" "NOSAVE 1"); i=0; while [ $i -lt $secs ]; do args+=("sleep 1" "STATUS"); i=$((i+1)); done
  log=$(mktemp); "$B/pmorctl" port "$PORT" script "${args[@]}" > "$log" 2>&1
  pyout=$(python3 - "$log" "$BPM" "$BARS" "$BASE_MEAN" "$BASE_P99" <<'PY'
import re, sys
log, bpm, bars, bm, bp = sys.argv[1], float(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
rows = []
for l in open(log, errors="replace"):
    if "ferr_us=" not in l: continue
    g = lambda k: (re.search(r"\b%s=(\S+)" % k, l) or [None, None])[1]
    f = re.match(r"(\d+)/(\d+)/(\d+)", g("ferr_us") or "")
    if not f: continue
    rows.append(dict(run=g("running"), n=int(g("ferr_n") or 0), mean=int(f[1]) / 1000, p99=int(f[2]) / 1000, mx=int(f[3]) / 1000, bpm=g("bpm"), cm=g("cm"), clk=g("clk"), iv=g("iv_us"), rx=g("rx"), late=g("late_us")))
if not rows: print("RESULT FAIL follower: no STATUS lines with ferr_us (see %s)" % log); sys.exit(0)
first = next((i for i, r in enumerate(rows) if r["run"] == "1"), None)
if first is None: print("RESULT FAIL follower: transport never started (no running=1). cm=%s clk=%s rx=%s. Is Live's clock routed to PMOR?" % (rows[-1]["cm"], rows[-1]["clk"], rows[-1]["rx"])); sys.exit(0)
need = int(bars * 4 * 60 / bpm) + 2                      # seconds after Start for a full 256-correction ring (one STATUS per second)
cand = rows[first + need] if len(rows) > first + need else rows[-1]
full = cand["n"] >= 256
print("   t+%ds after Start: ferr_n=%d  bpm=%s cm=%s iv_us=%s rx=%s late_us=%s" % (min(need, len(rows) - 1 - first), cand["n"], cand["bpm"], cand["cm"], cand["iv"], cand["rx"], cand["late"]))
print("   ferr_us mean %.2f ms  p99 %.2f ms  max %.2f ms   (baseline NEPTR clock: mean %.2f / p99 %.2f)" % (cand["mean"], cand["p99"], cand["mx"], bm, bp))
traj = [rows[first + t] for t in (5, 10, 20, 30) if len(rows) > first + t]
print("   trajectory mean/p99 ms @ +5/+10/+20/+30 s: " + "  ".join("%.2f/%.2f" % (r["mean"], r["p99"]) for r in traj))
ratio = cand["mean"] / bm if bm else 0
v = "PASS" if cand["mean"] <= 2 * bm and cand["p99"] <= 2 * bp else "FAIL"
print("RESULT %s follower @%g BPM: mean %.2f ms (%.1fx baseline) p99 %.2f ms (%.1fx) max %.2f ms%s" % (v, bpm, cand["mean"], ratio, cand["p99"], cand["p99"] / bp, cand["mx"], "" if full else "  [ring not full: ferr_n=%d<256]" % cand["n"]))
PY
)
  echo "$pyout" | grep -v '^RESULT'
  follow_res=$(echo "$pyout" | grep '^RESULT' | sed 's/^RESULT //')
fi

# ---- summary
say; say "=================== PMOR hwtest summary ==================="
say "port $PORT"
for r in ${res[@]+"${res[@]}"}; do say "  $r"; done
[ -n "${follow_res:-}" ] && say "  $follow_res"
say "  baseline (NEPTR clock, 124 BPM): ferr mean ${BASE_MEAN} ms / p99 ${BASE_P99} ms"
say "==========================================================="
fr="${follow_res:-}"; [ $fail -eq 0 ] && [ "${fr:0:4}" != "FAIL" ] && exit 0 || exit 1
