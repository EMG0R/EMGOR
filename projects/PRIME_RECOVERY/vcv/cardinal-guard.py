#!/home/gorpi7v5/kivy-venv/bin/python
"""cardinal-guard: if csound's graph deadline misses (pw-top ERR on csound6) rise while Cardinal
runs, take away Cardinal's priority, then Cardinal itself. NEVER acts on csound, PipeWire, the HAT
or any service. PROPOSED, NOT RUN.

Escalation (all on Cardinal's own process tree):
  L0 always   : any Cardinal thread with RT prio > RT_CAP is lowered to RT_CAP (chrt -f -p).
                (belt and braces for LimitRTPRIO=60 + rt.prio=50)
  L1 ERR delta >= 1 on csound6 within WINDOW_S while Cardinal is alive:
                Cardinal threads -> SCHED_OTHER (chrt -o -p 0) and renice 15.
  L2 ERR delta >= 3 within WINDOW_S after L1 (or 1 more within L2_GRACE_S):
                write PAUSE file; cardinal-ctl stops Cardinal (SIGTERM, clean graph exit) and keeps
                it down until the file is removed by a human. (No SIGSTOP: a stopped *sync* JACK
                client would stall the graph. Only allowed if node.async is verified; see plan.)
Baseline: the ERR counter is cumulative since node creation, so only deltas count; the first sample
after csound6 appears is the baseline. A csound6 restart resets the counter (negative delta -> rebaseline).

Source: `pw-top -b -n 2` (batch, second iteration). The column layout is UNVERIFIED on this box; if the
csound6 row cannot be parsed the guard logs it once and runs L0 only.
"""
import os, re, subprocess, sys, time, glob

RT_CAP = 60
WINDOW_S = 60.0
L2_GRACE_S = 120.0
STATE_DIR = os.path.expanduser("~/.local/state/cardinal-ctl")
PAUSE = os.path.join(STATE_DIR, "PAUSE")
LOG = os.path.expanduser("~/PRIME_RECOVERY/logs/cardinal-guard.log")
POLL_S = 5.0


def log(m):
    line = f"{time.strftime('%F %T')} {m}"
    print(line, flush=True)
    try:
        with open(LOG, "a") as f:
            f.write(line + "\n")
    except OSError:
        pass


def parse_err(text: str, name: str = "csound6"):
    """Return ERR (int) of the LAST iteration's row that contains `name`, or None."""
    err = None
    for ln in text.splitlines():
        if name not in ln:
            continue
        t = ln.split()
        # first all-digit token is the node ID; ERR is 7 columns later (ID QUANT RATE WAIT BUSY W/Q B/Q ERR)
        for k, tok in enumerate(t[:3]):
            if tok.isdigit() and k + 7 < len(t) and t[k + 7].isdigit():
                err = int(t[k + 7])
                break
    return err


def cardinal_pids():
    out = subprocess.run(["pgrep", "-f", "/opt/demiurge/cardinal/bin/Cardinal"], capture_output=True, text=True).stdout.split()
    return [int(x) for x in out]


def tids(pid):
    return [int(os.path.basename(p)) for p in glob.glob(f"/proc/{pid}/task/*")]


def sched_of(tid):
    try:
        f = open(f"/proc/{tid}/stat").read().rsplit(")", 1)[1].split()
        return int(f[38]), int(f[39])        # policy, rt_priority (fields 41,40)
    except Exception:
        return 0, 0


def run(*a):
    return subprocess.run(a, capture_output=True, text=True)


def cap_rt():
    n = 0
    for pid in cardinal_pids():
        for tid in tids(pid):
            pol, pr = sched_of(tid)
            if pol in (1, 2) and pr > RT_CAP:
                run("chrt", "-f", "-p", str(RT_CAP), str(tid)); n += 1
    return n


def demote():
    for pid in cardinal_pids():
        for tid in tids(pid):
            run("chrt", "-o", "-p", "0", str(tid))
            run("renice", "-n", "15", "-p", str(tid))


def selftest():
    sample = "S   ID  QUANT   RATE    WAIT    BUSY   W/Q   B/Q  ERR FORMAT           NAME\nR  150    128  48000   10.0us  25.0us  0.00  0.01   3  F32P 8 48000     csound6\n"
    assert parse_err(sample) == 3, parse_err(sample)
    sample2 = "R  150    128  48000   10.0us  25.0us  0.00  0.01   4  F32P 8 48000     csound6\n"
    assert parse_err(sample + sample2) == 4
    assert parse_err("nothing") is None
    print("selftest ok")


def main():
    if "--selftest" in sys.argv:
        return selftest()
    os.makedirs(STATE_DIR, exist_ok=True)
    base = None; hist = []; level = 0; l1_at = 0.0; warned = False
    while True:
        time.sleep(POLL_S)
        pids = cardinal_pids()
        if not pids:
            level = 0; hist.clear(); base = None
            continue
        n = cap_rt()
        if n:
            log(f"L0: lowered {n} Cardinal thread(s) to FIFO {RT_CAP}")
        r = subprocess.run(["pw-top", "-b", "-n", "2"], capture_output=True, text=True, timeout=10)
        e = parse_err(r.stdout)
        if e is None:
            if not warned:
                log("cannot parse csound6 ERR from pw-top; L0 only"); warned = True
            continue
        if base is None or e < base:
            base = e; continue
        now = time.monotonic()
        if e > base:
            hist.append((now, e - base)); base = e
        hist[:] = [(t, d) for t, d in hist if now - t <= WINDOW_S]
        recent = sum(d for _, d in hist)
        if level == 0 and recent >= 1:
            log(f"L1: csound6 ERR +{recent} in {WINDOW_S:.0f}s with Cardinal up -> demote Cardinal to SCHED_OTHER nice 15")
            demote(); level = 1; l1_at = now; hist.clear()
        elif level == 1 and (recent >= 3 or (recent >= 1 and now - l1_at < L2_GRACE_S)):
            log(f"L2: csound6 ERR +{recent} after demotion -> PAUSE Cardinal (remove {PAUSE} to allow it back)")
            open(PAUSE, "w").write(time.strftime("%F %T guard\n")); level = 2
        elif level == 1 and now - l1_at > 600 and recent == 0:
            level = 0                   # quiet for 10 min: stay demoted (do not re-promote), re-arm escalation only


if __name__ == "__main__":
    sys.exit(main() or 0)
