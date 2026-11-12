#!/home/gorpi7v5/kivy-venv/bin/python
"""Receives csound/spike_probe.orc packets on 127.0.0.1:9177 and logs them with
wall-clock time to ~/PRIME_RECOVERY/logs/spikes.log.

/spike   dt_ms menu lp1_speed_idx preset_recall osc_refresh ir_row0 active_instances
/spkstat max_ms mean_ms cycles   (once per second; logged when max > 1.5 ms, else every 30 s)
"""
import os, time
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import BlockingOSCUDPServer

LOG = os.path.expanduser("~/PRIME_RECOVERY/logs/spikes.log")
last_quiet = [0.0]


def w(line):
    with open(LOG, "a") as f:
        f.write(time.strftime("%F %T") + f".{int(time.time() * 1000) % 1000:03d} " + line + "\n")


def spike(_, dt, menu, sp, rec, refr, ir, act):
    w(f"SPIKE {dt:6.2f} ms  menu={int(menu)} lp1spd={int(sp)} preset_recall={int(rec)} "
      f"osc_refresh={int(refr)} ir0={int(ir)} active={int(act)}")


def stat(_, mx, mean, n):
    now = time.time()
    if mx > 1.5 or now - last_quiet[0] >= 30:
        last_quiet[0] = now
        w(f"stat  max {mx:6.2f} ms  mean {mean:5.2f} ms  cycles {int(n)}")


d = Dispatcher()
d.map("/spike", spike)
d.map("/spkstat", stat)
w("spike logger up")
BlockingOSCUDPServer(("127.0.0.1", 9177), d).serve_forever()
