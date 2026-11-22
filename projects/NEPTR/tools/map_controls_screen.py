#!/usr/bin/env python3
"""On-screen version of map_controls.py for the NEPTR panel (480x800 portrait).

Shows each step, a progress bar and what the last gesture sent, so the whole
run happens at the instrument with nothing else watching. Same capture logic
and the same JSON output as map_controls.py (it imports it).

Launch with tools/map_controls_screen.sh, which frees the panel (stops the UI
and engine) and brings both back when this exits.
"""
import os, sys, time, json
os.environ.setdefault("SDL_VIDEODRIVER", "kmsdrm")
os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
import pygame
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map_controls as mc

W, H = 480, 800
BG, FG, DIM, ACC, BAD = (27, 1, 30), (235, 225, 255), (140, 120, 170), (170, 90, 255), (255, 90, 110)


def wrap(font, text, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if font.size(t)[0] <= width:
            cur = t
        else:
            lines.append(cur); cur = w
    if cur:
        lines.append(cur)
    return lines


class Screen:
    def __init__(self):
        pygame.init()
        pygame.mouse.set_visible(False)
        self.s = pygame.display.set_mode((W, H), pygame.FULLSCREEN)
        self.big = pygame.font.Font(None, 54)
        self.mid = pygame.font.Font(None, 34)
        self.small = pygame.font.Font(None, 24)

    def draw(self, title, body, i=0, n=1, last=None, bad=False):
        for _ in pygame.event.get():
            pass
        s = self.s
        s.fill(BG)
        y = 30
        s.blit(self.small.render("NEPTR CONTROL MAP", True, DIM), (24, y)); y += 36
        pygame.draw.rect(s, DIM, (24, y, W - 48, 14), 1)
        pygame.draw.rect(s, ACC, (25, y + 1, int((W - 50) * i / max(1, n)), 12)); y += 26
        s.blit(self.small.render(f"step {i} / {n}", True, DIM), (24, y)); y += 50
        for ln in wrap(self.big, title, W - 48):
            s.blit(self.big.render(ln, True, BAD if bad else FG), (24, y)); y += 54
        y += 16
        for ln in body:
            for l2 in wrap(self.mid, ln, W - 48):
                s.blit(self.mid.render(l2, True, FG), (24, y)); y += 36
        if last:
            y = H - 30 - 26 * len(last) - 30
            s.blit(self.small.render("last gesture sent:", True, DIM), (24, y)); y += 28
            for ln in last:
                s.blit(self.small.render(ln, True, ACC), (24, y)); y += 26
        pygame.display.flip()


def short(summ):
    if not summ:
        return ["NOTHING received"]
    out = []
    for k, vals in summ.items():
        k = k.replace("NEPTR Serial Bridge", "bridge").replace("Teensy MIDI", "usb")
        u = sorted(set(v for v in vals if v is not None))
        out.append(f"{k} x{len(vals)} {u[:3]}{'..' if len(u) > 3 else ''}")
    return out[:6]


def main():
    scr = Screen()
    ports = mc.open_ports()
    steps = mc.STEPS
    start = os.environ.get("MAP_START", "")      # resume from the first step containing this text
    if start:
        steps = steps[next(i for i, x in enumerate(steps) if start.lower() in x.lower()):]
    results, n = [], len(steps)
    last = None
    for i, step in enumerate(steps, 1):
        name, _, act = step.partition(": ")
        mc.wait_quiet(ports)
        scr.draw(act.upper() if act else name.upper(), [name if act else "", "Do it once, then let go."], i - 1, n, last)
        summ = mc.capture(ports, None)      # no timer: waits for a real control
        last = [f"#{i} {step[:38]}"] + short(summ)
        results.append({"step": step, "events": summ})
    ts = time.strftime("%Y%m%d-%H%M%S")
    path = os.path.expanduser(f"~/NEPTR_phase4/docs/control_capture_{ts}.json")
    with open(path, "w") as f:
        json.dump(results, f, indent=1)
    dead = [r["step"] for r in results if not r["events"]]
    scr.draw("DONE", [f"Saved {os.path.basename(path)}", f"{n - len(dead)} of {n} steps sent something.",
                      "UI and audio come back now."], n, n, last)
    print("saved", path, flush=True)
    time.sleep(3)
    pygame.quit()


if __name__ == "__main__":
    main()
