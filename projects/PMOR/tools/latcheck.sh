#!/usr/bin/env bash
# latcheck.sh -- hardware smoke test for pad latency: reads STATUS windows from a live PMOR and FAILS if any main-loop section, the whole
# pass, the keypad poll gap or the USB-service cost exceeds its budget. Needs the board on USB (CDC); nothing is flashed or changed.
#
#   tools/latcheck.sh [PORT] [WINDOWS=6] [SECONDS_PER_WINDOW=5]
#   PORT defaults to the one /dev/cu.usbmodem* that is not a Bela (name containing 4488 is NEVER opened).
#   Wait >= 40 s after a flash first: the boot store migration inflates patchTick for ~15 s.
#
# Budgets (us) -- each is the measured worst case on 2026-11-11 with ~2x headroom, and the reason is the pad path:
#   a pad press is delivered at best one poll after it happens, so the worst-case detection delay is
#     (keypad poll gap) + (reply chain ~1.0 ms x 2) + (audio block <= 1.3 ms to the voice, +1.3 ms out)
#   and the poll gap is KEY_POLL (2.0 ms) + the longest single loop piece that was running when it fell due. So every piece is held to ~1 ms.
#     lp0 keypad phase     <=  700  (measured ~400: one request or one reply, never a spin)
#     lp1 sensors step     <= 1400  (~750: one IMU burst / one lidar read)
#     lp2 patchTick        <= 1000  (~400: the live-patch CRC scan every 250 ms)
#     lp3 oled render      <=  800  (~290)
#     lp4 oled piece       <=  800  (~380: one 32-byte piece; was 9.6 ms for a whole frame)
#     lp5 trellis render   <=  600  (~180)
#     lp6 trellisTick      <= 1500  (~170 typ; one LED chunk ~0.9 ms, bus recovery probe when down is ~1.3 ms)
#     lp7 nav tick         <=  600  (~165)
#     pass (lp2_us[0])     <= 3500  (~1.5-1.9: sum of the pieces one pass can hold)
#     pass head            <= 1500  (~360: USB/CDC/clock/studio work before the keypad poll)
#     poll gap (lp2_us[3]) <= 4500  (~2.7-3.0 on a nominal 2.0): the number that bounds pad-detection delay
#     pad event handled    <= 1500  (poll reply -> voice trigger / UI event; only present if a pad was hit during the window)
#     svc_us avg           <=   20  (~1: pmor_usb_service runs once per I2C byte; it was 72 before the fxMidiPoll fix)
#     svc_us max           <=  600  (~170)
#   plus: no bus stretch errors / trellis drops, no audio overruns, IMU probe failures 0, trellis up.
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-}"; WIN="${2:-6}"; SECS="${3:-5}"
if [ -z "$PORT" ]; then
  for p in /dev/cu.usbmodem*; do case "$p" in *4488*) ;; *) [ -e "$p" ] && PORT="$p" && break ;; esac; done
fi
case "$PORT" in *4488*) echo "latcheck: refusing to open $PORT (Bela)"; exit 2 ;; esac
[ -n "$PORT" ] && [ -e "$PORT" ] || { echo "latcheck: no PMOR CDC port found"; exit 2; }
CTL="$HERE/build_host/pmorctl"; [ -x "$CTL" ] || { echo "latcheck: $CTL missing (run check.sh --host)"; exit 2; }

args=("HELLO EDIT 1" "STATUS")
for _ in $(seq "$WIN"); do args+=("sleep $SECS" "STATUS"); done
args+=("BYE")
OUT="$("$CTL" port "$PORT" script "${args[@]}" 2>&1)" || true

printf '%s\n' "$OUT" | python3 -I -c '
import re, sys
B = dict(lp0=700, lp1=1400, lp2=1000, lp3=800, lp4=800, lp5=600, lp6=1500, lp7=600,
         pass_=3500, head=1500, gap=4500, padev=1500, svc_avg=20, svc_max=600, led=15000)
rows = []
for line in sys.stdin:
    if not line.startswith("  OK STATUS"): continue
    def f(k, n):
        m = re.search(k + r"=([0-9/]+)", line)
        return [int(x) for x in m.group(1).split("/")] if m else [0] * n
    rows.append(dict(lp=f("lp_us", 8), lp2=f("lp2_us", 4), svc=f("svc_us", 2), kp=f("kp", 3), sens=f("sens", 3),
                     snserr=f("snserr", 2), led=f("led_us", 2), ovr=f("audio_overruns", 1)[0]))
# the first reply is a stale/zero window; windows with no keypad polls are stale too
rows = [r for r in rows[1:] if r["kp"][0] > 0]
if len(rows) < 2:
    print("latcheck: FAIL  fewer than 2 usable STATUS windows (%d)" % len(rows)); sys.exit(1)
mx = lambda g: max(g(r) for r in rows)
res = [("lp0 keypad phase", mx(lambda r: r["lp"][0]), B["lp0"]), ("lp1 sensors step", mx(lambda r: r["lp"][1]), B["lp1"]),
       ("lp2 patchTick", mx(lambda r: r["lp"][2]), B["lp2"]), ("lp3 oled render", mx(lambda r: r["lp"][3]), B["lp3"]),
       ("lp4 oled piece", mx(lambda r: r["lp"][4]), B["lp4"]), ("lp5 trellis render", mx(lambda r: r["lp"][5]), B["lp5"]),
       ("lp6 trellisTick", mx(lambda r: r["lp"][6]), B["lp6"]), ("lp7 nav tick", mx(lambda r: r["lp"][7]), B["lp7"]),
       ("whole pass", mx(lambda r: r["lp2"][0]), B["pass_"]), ("pass head", mx(lambda r: r["lp2"][1]), B["head"]),
       ("keypad poll gap", mx(lambda r: r["lp2"][3]), B["gap"]), ("pad event handled", mx(lambda r: r["lp2"][2]), B["padev"]),
       ("svc_us avg", mx(lambda r: r["svc"][0]), B["svc_avg"]), ("svc_us max", mx(lambda r: r["svc"][1]), B["svc_max"]),
       ("pad event -> LED shown (0 = no pad pressed)", mx(lambda r: r["led"][0]), B["led"])]
bad = 0
for name, v, b in res:
    ok = v <= b; bad += (not ok)
    print("  %-4s %-20s %6d us  (budget %d)" % ("ok" if ok else "FAIL", name, v, b))
last = rows[-1]
checks = [("bus stretch errors", last["kp"][1], 0), ("trellis drops", last["kp"][2], 0), ("audio overruns", last["ovr"], 0), ("IMU probe failures", last["snserr"][0], 0)]
for name, v, b in checks:
    ok = v <= b; bad += (not ok)
    print("  %-4s %-20s %6d      (budget %d)" % ("ok" if ok else "FAIL", name, v, b))
if not all(r["sens"][0] == 1 for r in rows): print("  FAIL trellis not up"); bad += 1
if not all(r["sens"][1] == 1 and r["sens"][2] == 1 for r in rows): print("  note: IMU or lidar reported down (sens=%s) -- not a latency failure" % rows[-1]["sens"])
print("latcheck: %s (%d windows, %d polls in the last)" % ("ALL OK" if not bad else "FAILED (%d)" % bad, len(rows), last["kp"][0]))
sys.exit(1 if bad else 0)
'
