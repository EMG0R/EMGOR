#!/usr/bin/env python3
"""HAT watchdog: keep the DAC8x/ADC8x output alive without restarting csound.

Polls the HAT's ALSA DMA status (/proc/asound/<card>/pcm0{p,c}/sub0/status)
every 250 ms. If a stream leaves RUNNING, or its hardware pointer stops
advancing, for longer than STALL_S, it runs `hat-reset` (PipeWire profile
off -> pro-audio, csound untouched) and logs the event. A cooldown stops it
looping while PipeWire reopens the card.

Events: journalctl -u hat-watchdog, plus ~/PRIME_RECOVERY/logs/hat-watchdog.log
"""
import os, subprocess, time

CARD = "/proc/asound/sndrpihifiberry"
POLL_S, STALL_S, COOLDOWN_S = 0.25, 0.75, 8.0
LOG = os.path.expanduser("~/PRIME_RECOVERY/logs/hat-watchdog.log")


def status(direction):
    try:
        with open(f"{CARD}/pcm0{direction}/sub0/status") as f:
            txt = f.read()
    except OSError:
        return None, None
    state, hw = "CLOSED", None
    for line in txt.splitlines():
        if line.startswith("state:"):
            state = line.split(":", 1)[1].strip()
        elif line.startswith("hw_ptr"):
            hw = int(line.split(":", 1)[1])
    return state, hw


def log(msg):
    line = time.strftime("%F %T ") + msg
    print(line, flush=True)
    with open(LOG, "a") as f:
        f.write(line + "\n")


def main():
    last = {"p": (None, time.time()), "c": (None, time.time())}
    bad_since, cool_until, resets = None, 0.0, 0
    log("watchdog up")
    while True:
        now = time.time()
        reasons = []
        for d in ("p", "c"):
            st, hw = status(d)
            prev_hw, moved_at = last[d]
            if hw is not None and hw != prev_hw:
                last[d] = (hw, now)
            elif now - moved_at > STALL_S and st == "RUNNING":
                reasons.append(f"{'out' if d == 'p' else 'in'} hw_ptr stalled {now - moved_at:.1f}s")
            if st not in ("RUNNING", None, "CLOSED"):
                reasons.append(f"{'out' if d == 'p' else 'in'} state {st}")
        # a fully closed card is someone else's business (engine stopped / reset in progress)
        if reasons and now >= cool_until:
            bad_since = bad_since or now
            if now - bad_since >= STALL_S:
                resets += 1
                log(f"HAT DROP ({'; '.join(reasons)}) -> hat-reset #{resets}")
                r = subprocess.run(["/usr/local/bin/hat-reset"], capture_output=True, text=True)
                log("  " + (r.stdout.strip() or r.stderr.strip()))
                cool_until, bad_since = time.time() + COOLDOWN_S, None
                for d in last:
                    last[d] = (None, time.time())
        elif not reasons:
            bad_since = None
        time.sleep(POLL_S)


if __name__ == "__main__":
    main()
