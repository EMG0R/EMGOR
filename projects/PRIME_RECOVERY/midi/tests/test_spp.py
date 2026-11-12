"""Offline test of TempoMeter SPP handling (no MIDI, no network).
Run: python3 tests/test_spp.py  (needs rtmidi + pythonosc importable, as in the relay venv)"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import importlib.util
spec = importlib.util.spec_from_file_location("relay", os.path.join(os.path.dirname(__file__), "..", "pmor-clock-relay.py"))
r = importlib.util.module_from_spec(spec); spec.loader.exec_module(r)

T = 60 / 120 / 24     # 120 BPM tick period
def ticks(m, n, t0):
    for i in range(n):
        m.on_tick(t0 + i * T)
    return t0 + n * T

def beats(m):
    out = [b for b, _ in m.beats]; m.beats.clear(); return out

# 1. old protocol: SPP 0 then Start -> beat 0 at next tick
m = r.TempoMeter(); t = ticks(m, 100, 0)       # clock flowing, stopped
m.on_spp(0, now=t); m.on_transport(0xFA, now=t)
assert m.playing and m.tick_n == -1
t = ticks(m, 24 * 6 + 1, t)
assert m.tick_n == 24 * 6, m.tick_n
# beats only emitted once the 48-tick fit exists
assert beats(m)[-1] == 6

# 2. SPP + Continue at abs16 = 20 (beat 5, bar 1 beat 1)
m = r.TempoMeter(); t = ticks(m, 100, 0)
m.on_transport(0xFC, now=t)
m.on_spp(20, now=t); m.on_transport(0xFB, now=t + 0.001)
assert m.playing and m.tick_n == 20 * 6 - 1
t = ticks(m, 1, t); assert m.tick_n == 120 and 120 // 24 == 5 and (120 // 24) % r.BEATS_PER_BAR == 1

# 3. Continue with no SPP (old firmware): not playing, no invented position
m = r.TempoMeter(); t = ticks(m, 100, 0)
m.on_transport(0xFB, now=t)
assert not m.playing; ticks(m, 50, t); assert not m.beats and m.tick_n == -1

# 4. per-bar SPP agreeing: nothing changes, window kept
m = r.TempoMeter(); t = ticks(m, 100, 0); m.on_spp(0, now=t); m.on_transport(0xFA, now=t)
t = ticks(m, 96, t); n = len(m.ticks)
m.on_spp(16, now=t)                            # next tick = 96 = 16*6
assert m.tick_n == 95 and len(m.ticks) >= n - 5 and m.spp_fixes == 0
assert m.idx[-1] == 95

# 5. disagreeing SPP corrects tick_n, keeps ticks
m.on_spp(48, now=t + 1)                        # claims 288, predicted 96
assert m.tick_n == 48 * 6 - 1 and m.spp_fixes == 1 and len(m.ticks) >= n - 5

# 6. first SPP while ticks already flowing (restart mid-song), no Start
m = r.TempoMeter(); t = ticks(m, 200, 0)
m.on_spp(1000, now=t)
assert m.playing and m.tick_n == 6000 - 1
t = ticks(m, 1, t); assert m.tick_n == 6000 and 6000 // 24 == 250

# 7. wrap: counting through 16384 sixteenths, SPP value wraps to small -> never backward
m = r.TempoMeter(); t = ticks(m, 50, 0)
m.on_spp(16380, now=t); m.on_transport(0xFB, now=t)
ticks(m, 4 * 6, t)                              # 4 sixteenths later: next idx = 16384*6
t2 = t + 24 * T
m.on_spp(0, now=t2)                             # wrapped value; predicted 98304
assert m.tick_n + 1 == 16384 * 6 and m.spp_fixes == 0, (m.tick_n, m.spp_fixes)
print("OK")
