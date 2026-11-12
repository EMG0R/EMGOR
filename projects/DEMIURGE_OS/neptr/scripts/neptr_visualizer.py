#!/usr/bin/env python3
"""
NEPTR Monitor — live top-down visualization of the NEPTR MIDI controller.

Reads serial telemetry from a Teensy 4.1 over USB (115200 baud) and renders
a real-time graphical representation of every sensor: dual LiDARs, IMU,
joystick, arcade button, 8 diamond buttons, 7 encoders, contact mics,
expression pedal, FSRs.

# Install deps:
pip3 install pygame pyserial

# Run (auto-detects Teensy port):
python3 neptr_visualizer.py

# Or specify port:
python3 neptr_visualizer.py --port /dev/cu.usbmodemXXXXXXXX

Esc or close the window to quit.
"""

from __future__ import annotations

import argparse
import colorsys
import glob
import math
import queue
import re
import sys
import threading
import time

import pygame

try:
    import serial  # pyserial
except ImportError:
    print("Missing dependency: pyserial. Run: pip3 install pyserial", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Physical layout (millimetres) and rendering scale
# ---------------------------------------------------------------------------

DEVICE_W_MM = 248
DEVICE_H_MM = 207
SCALE = 4  # px per mm
PADDING_PX = 22

WIN_W = DEVICE_W_MM * SCALE + PADDING_PX * 2  # 1036
WIN_H = DEVICE_H_MM * SCALE + PADDING_PX * 2  # 872

CENTER = (WIN_W // 2, WIN_H // 2)

# Colours
BG = (20, 20, 26)
SHELL_FILL = (32, 34, 42)
SHELL_OUTLINE = (78, 82, 96)
TEXT = (210, 214, 224)
TEXT_DIM = (130, 134, 144)
TEXT_FAINT = (90, 94, 104)
ACCENT = (90, 200, 255)
WARN = (255, 170, 60)
DEAD = (60, 64, 74)

# Field defaults
DEFAULT_STATE = {
    "tof1": None,           # int | "timeout" | "not initialized" | None
    "tof2": None,
    "accel": (0, 0, 0),
    "joy": (512, 511),
    "joy_btn": 0,
    "btns": [0] * 9,        # index 0 = arcade, 1..8 = diamond
    "exp": 0,
    "fsr": (0, 0),
    "mics": (0, 0),
    "encs": ["."] * 7,
    "raw": "",
    "last_update": 0.0,
}

# ---------------------------------------------------------------------------
# Serial port discovery & parsing
# ---------------------------------------------------------------------------


def find_serial_port() -> str | None:
    """Pick the first /dev/cu.usbmodem* device on macOS."""
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    return candidates[0] if candidates else None


# Regexes — each field is parsed independently so a malformed field can't
# break the rest of the line.
_RE_TOF = {
    1: re.compile(r"TOF1:\s*(\d+\s*mm|not\s+initialized|timeout)", re.I),
    2: re.compile(r"TOF2:\s*(\d+\s*mm|not\s+initialized|timeout)", re.I),
}
_RE_ACCEL = re.compile(r"ACCEL\s*X/Y/Z:\s*(-?\d+)\s+(-?\d+)\s+(-?\d+)", re.I)
_RE_JOY = re.compile(r"JOY\s*X/Y:\s*(\d+)\s+(\d+)\s+BTN:\s*(\d+)", re.I)
_RE_BTNS = re.compile(r"BTNS:\s*((?:-?\d+\s+){8}-?\d+)", re.I)
_RE_EXP = re.compile(r"EXP\(p41\):\s*(-?\d+)", re.I)
_RE_FSR = re.compile(r"FSR:\s*(-?\d+)\s+(-?\d+)", re.I)
_RE_MICS = re.compile(r"MICS\s*L/R:\s*(-?\d+)\s+(-?\d+)", re.I)
_RE_ENCS = re.compile(r"ENCS:\s*([\.\sLRB]+?)\s*$", re.I)


def _parse_tof(text: str, which: int):
    m = _RE_TOF[which].search(text)
    if not m:
        return None
    val = m.group(1).strip().lower()
    if "not" in val:
        return "not initialized"
    if "timeout" in val:
        return "timeout"
    # "250 mm"
    digits = re.match(r"(\d+)", val)
    return int(digits.group(1)) if digits else None


def parse_line(line: str) -> dict | None:
    """Parse one telemetry line. Returns dict of fields (any may be missing)
    or None if absolutely nothing parsed."""
    out: dict = {}
    try:
        t1 = _parse_tof(line, 1)
        if t1 is not None:
            out["tof1"] = t1
    except Exception:
        pass
    try:
        t2 = _parse_tof(line, 2)
        if t2 is not None:
            out["tof2"] = t2
    except Exception:
        pass
    try:
        m = _RE_ACCEL.search(line)
        if m:
            out["accel"] = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    except Exception:
        pass
    try:
        m = _RE_JOY.search(line)
        if m:
            out["joy"] = (int(m.group(1)), int(m.group(2)))
            out["joy_btn"] = 1 if int(m.group(3)) else 0
    except Exception:
        pass
    try:
        m = _RE_BTNS.search(line)
        if m:
            parts = m.group(1).split()
            if len(parts) == 9:
                out["btns"] = [int(p) for p in parts]
    except Exception:
        pass
    try:
        m = _RE_EXP.search(line)
        if m:
            out["exp"] = max(0, min(127, int(m.group(1))))
    except Exception:
        pass
    try:
        m = _RE_FSR.search(line)
        if m:
            out["fsr"] = (int(m.group(1)), int(m.group(2)))
    except Exception:
        pass
    try:
        m = _RE_MICS.search(line)
        if m:
            out["mics"] = (int(m.group(1)), int(m.group(2)))
    except Exception:
        pass
    try:
        m = _RE_ENCS.search(line)
        if m:
            chars = [c for c in m.group(1) if c in ".LRB"]
            if len(chars) >= 7:
                out["encs"] = chars[:7]
    except Exception:
        pass

    return out if out else None


# ---------------------------------------------------------------------------
# Coordinate helpers
# ---------------------------------------------------------------------------


def mm_to_px(x_mm: float, y_mm: float) -> tuple[int, int]:
    """Convert device-mm (origin at device centre, +Y back-of-device)
    to screen px (Y inverted so +Y mm = up on screen)."""
    cx, cy = CENTER
    return (int(cx + x_mm * SCALE), int(cy - y_mm * SCALE))


def lerp_color(c1, c2, t: float):
    t = max(0.0, min(1.0, t))
    return (
        int(c1[0] + (c2[0] - c1[0]) * t),
        int(c1[1] + (c2[1] - c1[1]) * t),
        int(c1[2] + (c2[2] - c1[2]) * t),
    )


# ---------------------------------------------------------------------------
# Serial reader thread
# ---------------------------------------------------------------------------


class SerialReader(threading.Thread):
    def __init__(self, port_override: str | None, out_queue: queue.Queue):
        super().__init__(daemon=True)
        self.port_override = port_override
        self.q = out_queue
        self.stop_flag = threading.Event()
        self.status = "STARTING"
        self.current_port: str | None = None

    def run(self) -> None:
        while not self.stop_flag.is_set():
            port = self.port_override or find_serial_port()
            if not port:
                self.status = "NO PORT — retrying"
                self.current_port = None
                time.sleep(2.0)
                continue
            try:
                self.status = f"CONNECTING {port}"
                ser = serial.Serial(port, 115200, timeout=1.0)
                self.current_port = port
                self.status = f"CONNECTED {port}"
                buf = b""
                while not self.stop_flag.is_set():
                    chunk = ser.read(256)
                    if not chunk:
                        continue
                    buf += chunk
                    while b"\n" in buf:
                        raw, _, buf = buf.partition(b"\n")
                        line = raw.decode("utf-8", errors="replace").strip()
                        if not line:
                            continue
                        parsed = parse_line(line)
                        # Always push raw too so UI can show last raw line
                        try:
                            self.q.put_nowait(("line", line, parsed))
                        except queue.Full:
                            pass
            except Exception as e:
                self.status = f"DISCONNECTED ({type(e).__name__}) — retrying"
                self.current_port = None
                time.sleep(2.0)

    def stop(self) -> None:
        self.stop_flag.set()


# ---------------------------------------------------------------------------
# Drawing primitives
# ---------------------------------------------------------------------------


def draw_rounded_rect(surf, rect, color, radius=24, width=0):
    pygame.draw.rect(surf, color, rect, width=width, border_radius=radius)


def draw_shell(surf):
    rect = pygame.Rect(PADDING_PX, PADDING_PX, WIN_W - 2 * PADDING_PX, WIN_H - 2 * PADDING_PX)
    # subtle shadow
    shadow = rect.move(0, 6)
    pygame.draw.rect(surf, (8, 8, 12), shadow, border_radius=34)
    pygame.draw.rect(surf, SHELL_FILL, rect, border_radius=34)
    pygame.draw.rect(surf, SHELL_OUTLINE, rect, width=2, border_radius=34)
    # inner highlight stroke
    inner = rect.inflate(-12, -12)
    pygame.draw.rect(surf, (44, 47, 58), inner, width=1, border_radius=28)


def draw_perimeter_ring(surf, t: float, n: int = 80):
    """Decorative slow rainbow around the shell perimeter."""
    rect = pygame.Rect(PADDING_PX, PADDING_PX, WIN_W - 2 * PADDING_PX, WIN_H - 2 * PADDING_PX)
    inset = 14
    inner = rect.inflate(-inset * 2, -inset * 2)
    # parametric position around a rounded rect — approximate by walking the
    # perimeter of `inner` (sharp corners; close enough visually).
    w, h = inner.width, inner.height
    perim = 2 * (w + h)
    for i in range(n):
        u = (i / n) * perim
        if u < w:
            px = inner.left + u
            py = inner.top
        elif u < w + h:
            px = inner.right
            py = inner.top + (u - w)
        elif u < 2 * w + h:
            px = inner.right - (u - w - h)
            py = inner.bottom
        else:
            px = inner.left
            py = inner.bottom - (u - 2 * w - h)
        hue = (i / n + t * 0.05) % 1.0
        r, g, b = colorsys.hsv_to_rgb(hue, 0.55, 0.55)
        pygame.draw.circle(surf, (int(r * 255), int(g * 255), int(b * 255)),
                           (int(px), int(py)), 2)


def draw_label(surf, font, text, pos, color=TEXT, center=True):
    img = font.render(text, True, color)
    if center:
        rect = img.get_rect(center=pos)
    else:
        rect = img.get_rect(topleft=pos)
    surf.blit(img, rect)


def draw_imu(surf, font, accel: tuple[int, int, int]):
    pos = mm_to_px(0, 0)
    r = 22
    # outer glow tied to Z magnitude (vertical accel)
    az = accel[2]
    z_norm = max(-1.0, min(1.0, az / 17000.0))
    glow = lerp_color((50, 60, 90), (90, 220, 255), abs(z_norm))
    pygame.draw.circle(surf, glow, pos, r + 4, 2)
    pygame.draw.circle(surf, (40, 45, 60), pos, r)
    pygame.draw.circle(surf, SHELL_OUTLINE, pos, r, 2)

    # bubble: X right, Y up (inverted on screen). Smoothing happens upstream.
    ax = max(-17000, min(17000, accel[0]))
    ay = max(-17000, min(17000, accel[1]))
    max_off = r - 6
    dx = (ax / 17000.0) * max_off
    dy = -(ay / 17000.0) * max_off  # screen y inverted
    bubble = (int(pos[0] + dx), int(pos[1] + dy))
    pygame.draw.circle(surf, (220, 240, 255), bubble, 4)

    # small Z bar to the right of the IMU
    bar_x = pos[0] + r + 8
    bar_top = pos[1] - r
    bar_h = r * 2
    pygame.draw.rect(surf, (28, 30, 38), (bar_x, bar_top, 6, bar_h))
    mid = bar_top + bar_h // 2
    fill_h = int((bar_h // 2) * abs(z_norm))
    if z_norm >= 0:
        pygame.draw.rect(surf, (90, 220, 255), (bar_x, mid - fill_h, 6, fill_h))
    else:
        pygame.draw.rect(surf, (255, 140, 90), (bar_x, mid, 6, fill_h))
    pygame.draw.line(surf, TEXT_FAINT, (bar_x, mid), (bar_x + 6, mid), 1)

    draw_label(surf, font, "IMU", (pos[0], pos[1] - r - 12), TEXT_DIM)


def draw_arcade(surf, font, pressed: bool):
    pos = mm_to_px(-51, 0)
    r = 25
    base = (60, 10, 14)
    lit = (255, 70, 80)
    fill = lit if pressed else (30, 14, 18)
    outline = lit if pressed else base
    pygame.draw.circle(surf, fill, pos, r)
    pygame.draw.circle(surf, outline, pos, r, 3)
    # inner ring detail
    pygame.draw.circle(surf, (250, 180, 180) if pressed else (90, 30, 36),
                       pos, r - 6, 1)
    draw_label(surf, font, "ARCADE", (pos[0], pos[1] + r + 12), TEXT_DIM)


def draw_joystick(surf, font, joy: tuple[int, int], btn: bool):
    pos = mm_to_px(51, 0)
    r = 25
    pygame.draw.circle(surf, (28, 30, 38), pos, r)
    pygame.draw.circle(surf, SHELL_OUTLINE, pos, r, 2)
    # cardinal tick marks
    for ang in (0, 90, 180, 270):
        rad = math.radians(ang)
        x1 = pos[0] + math.cos(rad) * (r - 4)
        y1 = pos[1] + math.sin(rad) * (r - 4)
        x2 = pos[0] + math.cos(rad) * (r - 1)
        y2 = pos[1] + math.sin(rad) * (r - 1)
        pygame.draw.line(surf, TEXT_FAINT, (x1, y1), (x2, y2), 1)

    jx = max(0, min(1023, joy[0]))
    jy = max(0, min(1023, joy[1]))
    nx = (jx - 511.5) / 511.5
    ny = (jy - 511.5) / 511.5
    max_off = 20
    dot = (int(pos[0] + nx * max_off), int(pos[1] - ny * max_off))
    if btn:
        pygame.draw.circle(surf, (90, 240, 255), dot, 7)
        pygame.draw.circle(surf, (220, 250, 255), dot, 7, 1)
    else:
        pygame.draw.circle(surf, (180, 200, 220), dot, 5)
    draw_label(surf, font, "JOY", (pos[0], pos[1] + r + 12), TEXT_DIM)


def draw_mic_vu(surf, font, x_mm: int, label: str, value: int):
    pos = mm_to_px(x_mm, 0)
    bar_h = 60
    bar_w = 12
    rect = pygame.Rect(0, 0, bar_w, bar_h)
    rect.center = pos
    pygame.draw.rect(surf, (24, 26, 32), rect)
    pygame.draw.rect(surf, SHELL_OUTLINE, rect, 1)

    v = max(0, min(127, value))
    fh = int((v / 127.0) * (bar_h - 2))
    if fh > 0:
        # gradient stack — split into thirds
        third = (bar_h - 2) // 3
        # green
        gh = min(fh, third)
        pygame.draw.rect(surf, (90, 220, 120),
                         (rect.left + 1, rect.bottom - 1 - gh, bar_w - 2, gh))
        # yellow
        if fh > third:
            yh = min(fh - third, third)
            pygame.draw.rect(surf, (240, 220, 80),
                             (rect.left + 1, rect.bottom - 1 - third - yh, bar_w - 2, yh))
        # red
        if fh > 2 * third:
            rh = fh - 2 * third
            pygame.draw.rect(surf, (255, 90, 80),
                             (rect.left + 1, rect.bottom - 1 - 2 * third - rh, bar_w - 2, rh))
    draw_label(surf, font, label, (pos[0], pos[1] + bar_h // 2 + 12), TEXT_DIM)
    draw_label(surf, font, str(v), (pos[0], pos[1] - bar_h // 2 - 10), TEXT_DIM)


def draw_lidar(surf, font, x_mm: int, label: str, value):
    """value can be int mm, 'timeout', 'not initialized', or None."""
    pos = mm_to_px(x_mm, -66)
    bar_w = 78
    bar_h = 12
    rect = pygame.Rect(0, 0, bar_w, bar_h)
    rect.center = pos
    pygame.draw.rect(surf, (24, 26, 32), rect)
    pygame.draw.rect(surf, SHELL_OUTLINE, rect, 1)

    if isinstance(value, int):
        v = max(0, min(2000, value))
        fw = int(((bar_w - 2) * v) / 2000.0)
        # near = bright, far = dim
        t = v / 2000.0
        col = lerp_color((120, 240, 200), (60, 110, 160), t)
        pygame.draw.rect(surf, col, (rect.left + 1, rect.top + 1, fw, bar_h - 2))
        txt = f"{label}: {v} mm"
        col_txt = TEXT
    elif value == "timeout":
        txt = f"{label}: timeout"
        col_txt = WARN
    elif value == "not initialized":
        txt = f"{label}: not init"
        col_txt = (255, 100, 100)
    else:
        txt = f"{label}: --"
        col_txt = TEXT_FAINT

    draw_label(surf, font, txt, (pos[0], pos[1] - 14), col_txt)


def draw_front_placeholder(surf, font, x_mm: int):
    pos = mm_to_px(x_mm, -66)
    r = 10
    pygame.draw.circle(surf, (28, 30, 38), pos, r)
    pygame.draw.circle(surf, TEXT_FAINT, pos, r, 1)
    draw_label(surf, font, "front", (pos[0], pos[1] + r + 10), TEXT_FAINT)


def draw_encoder(surf, font, x_mm: int, idx: int, char: str,
                 last_lr_time: float, now: float):
    pos = mm_to_px(x_mm, 50)
    r = 15
    pygame.draw.circle(surf, (28, 30, 38), pos, r)
    # decide visual state — char is current, but L/R also persist briefly
    age = now - last_lr_time
    fade = max(0.0, 1.0 - age / 0.5) if last_lr_time > 0 else 0.0

    if char == "B":
        pygame.draw.circle(surf, (40, 200, 220), pos, r)
        pygame.draw.circle(surf, (180, 240, 250), pos, r, 2)
    elif char in ("L", "R") or fade > 0:
        # show yellow tint scaled by fade (1.0 if char is current)
        intensity = 1.0 if char in ("L", "R") else fade
        col = lerp_color((40, 45, 56), (240, 220, 80), intensity)
        pygame.draw.circle(surf, col, pos, r)
        pygame.draw.circle(surf, SHELL_OUTLINE, pos, r, 2)
        # arrow
        direction = char if char in ("L", "R") else None
        if direction is None:
            # fallback to last direction implied by char history — we don't
            # track that explicitly; just skip the arrow when faded.
            pass
        if direction == "R":
            _draw_arrow(surf, pos, r, clockwise=True, alpha=intensity)
        elif direction == "L":
            _draw_arrow(surf, pos, r, clockwise=False, alpha=intensity)
    else:
        pygame.draw.circle(surf, SHELL_OUTLINE, pos, r, 2)
        # tiny shaft indicator
        pygame.draw.circle(surf, TEXT_FAINT, pos, 2)

    draw_label(surf, font, f"E{idx + 1}", (pos[0], pos[1] + r + 11), TEXT_DIM)


def _draw_arrow(surf, center, r, clockwise: bool, alpha: float = 1.0):
    col = lerp_color((40, 45, 56), (30, 20, 20), alpha)
    col = (30, 30, 30)
    # draw an arc + arrowhead
    cx, cy = center
    rect = pygame.Rect(cx - r + 4, cy - r + 4, 2 * (r - 4), 2 * (r - 4))
    if clockwise:
        pygame.draw.arc(surf, col, rect, math.radians(30), math.radians(300), 2)
        # arrowhead at ~30deg (right side)
        ang = math.radians(30)
        tip = (cx + math.cos(ang) * (r - 4), cy - math.sin(ang) * (r - 4))
        pygame.draw.polygon(surf, col, [
            tip,
            (tip[0] - 4, tip[1] - 2),
            (tip[0] - 2, tip[1] + 4),
        ])
    else:
        pygame.draw.arc(surf, col, rect, math.radians(-120), math.radians(150), 2)
        ang = math.radians(150)
        tip = (cx + math.cos(ang) * (r - 4), cy - math.sin(ang) * (r - 4))
        pygame.draw.polygon(surf, col, [
            tip,
            (tip[0] + 4, tip[1] - 2),
            (tip[0] + 2, tip[1] + 4),
        ])


def draw_button(surf, font, x_mm: int, y_mm: int, label: str, pressed: bool,
                radius: int = 11):
    pos = mm_to_px(x_mm, y_mm)
    if pressed:
        pygame.draw.circle(surf, (90, 220, 255), pos, radius)
        pygame.draw.circle(surf, (220, 250, 255), pos, radius, 1)
    else:
        pygame.draw.circle(surf, (30, 32, 40), pos, radius)
        pygame.draw.circle(surf, SHELL_OUTLINE, pos, radius, 2)
    draw_label(surf, font, label, (pos[0], pos[1] + radius + 10), TEXT_DIM)


def draw_bar(surf, font, x_mm: int, y_mm: int, length_px: int, height_px: int,
             label: str, value: int, vmax: int, color=(150, 220, 255)):
    pos = mm_to_px(x_mm, y_mm)
    rect = pygame.Rect(0, 0, length_px, height_px)
    rect.center = pos
    pygame.draw.rect(surf, (24, 26, 32), rect)
    pygame.draw.rect(surf, SHELL_OUTLINE, rect, 1)
    v = max(0, min(vmax, value))
    if vmax > 0:
        fw = int(((length_px - 2) * v) / vmax)
        if fw > 0:
            pygame.draw.rect(surf, color,
                             (rect.left + 1, rect.top + 1, fw, height_px - 2))
    draw_label(surf, font, f"{label} {v}", (pos[0], pos[1] - height_px // 2 - 9),
               TEXT_DIM)


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description="NEPTR live visualizer")
    ap.add_argument("--port", help="Serial port (default: auto /dev/cu.usbmodem*)")
    args = ap.parse_args()

    pygame.init()
    pygame.display.set_caption("NEPTR Monitor")
    screen = pygame.display.set_mode((WIN_W, WIN_H))
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("menlo,monaco,consolas,monospace", 12)
    font_big = pygame.font.SysFont("menlo,monaco,consolas,monospace", 14)

    q: queue.Queue = queue.Queue(maxsize=200)
    reader = SerialReader(args.port, q)
    reader.start()

    state = dict(DEFAULT_STATE)
    state["btns"] = list(state["btns"])
    state["encs"] = list(state["encs"])

    # Smoothed accel for the bubble level
    smooth_accel = [0.0, 0.0, 0.0]
    # Per-encoder last L/R timestamps for fade
    enc_last_lr = [0.0] * 7

    running = True
    t_start = time.time()
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False

        # Drain serial queue
        drained = 0
        while drained < 32:
            try:
                kind, raw_line, parsed = q.get_nowait()
            except queue.Empty:
                break
            drained += 1
            state["raw"] = raw_line
            state["last_update"] = time.time()
            if not parsed:
                continue
            for key, val in parsed.items():
                state[key] = val
            # Update encoder L/R timestamps
            if "encs" in parsed:
                now = time.time()
                for i, c in enumerate(parsed["encs"][:7]):
                    if c in ("L", "R"):
                        enc_last_lr[i] = now

        # Smooth accel
        ax, ay, az = state["accel"]
        alpha = 0.25
        smooth_accel[0] += (ax - smooth_accel[0]) * alpha
        smooth_accel[1] += (ay - smooth_accel[1]) * alpha
        smooth_accel[2] += (az - smooth_accel[2]) * alpha

        # ----- draw -----
        screen.fill(BG)
        draw_shell(screen)
        draw_perimeter_ring(screen, time.time() - t_start)

        # IMU at centre
        draw_imu(screen, font, (int(smooth_accel[0]), int(smooth_accel[1]),
                                int(smooth_accel[2])))

        # Arcade + Joystick flank the IMU
        draw_arcade(screen, font, bool(state["btns"][0]))
        draw_joystick(screen, font, state["joy"], bool(state["joy_btn"]))

        # Contact mics at left/right rim
        mics_l, mics_r = state["mics"]
        draw_mic_vu(screen, font, -120, "MIC L", mics_l)
        draw_mic_vu(screen, font, +120, "MIC R", mics_r)

        # Front edge — 5 spots at y = -66
        draw_lidar(screen, font, -84, "L1", state["tof1"])
        draw_lidar(screen, font, +84, "L2", state["tof2"])
        for x in (-43, 0, 43):
            draw_front_placeholder(screen, font, x)

        # 7 encoders at y = +50, x = -90..+90 evenly spaced
        now = time.time()
        for i in range(7):
            x_mm = -90 + (180 * i / 6)
            draw_encoder(screen, font, int(round(x_mm)), i, state["encs"][i],
                         enc_last_lr[i], now)

        # 8 diamond buttons at y = -30, x = -90..+90 evenly spaced
        for i in range(8):
            x_mm = -90 + (180 * i / 7)
            pressed = bool(state["btns"][i + 1])  # BTNS index 1..8
            draw_button(screen, font, int(round(x_mm)), -30, f"B{i + 2}",
                        pressed, radius=11)

        # FSRs upper-left, EXP upper-right
        draw_bar(screen, font, -95, 98, 60, 10, "FSR1", state["fsr"][0], 1023,
                 color=(255, 180, 110))
        draw_bar(screen, font, -95, 84, 60, 10, "FSR2", state["fsr"][1], 1023,
                 color=(255, 180, 110))
        draw_bar(screen, font, 95, 90, 80, 12, "EXP", state["exp"], 127,
                 color=(180, 140, 255))

        # ---- overlays ----
        status = reader.status
        status_color = ACCENT if status.startswith("CONNECTED") else WARN
        draw_label(screen, font_big, status, (10, 8), status_color, center=False)

        # data-staleness flag
        if state["last_update"] > 0:
            age = time.time() - state["last_update"]
            if age > 1.5:
                draw_label(screen, font, f"stale ({age:.1f}s)",
                           (10, 26), WARN, center=False)

        raw = state["raw"]
        if raw:
            # truncate to fit window
            max_chars = (WIN_W - 20) // 7
            if len(raw) > max_chars:
                raw = raw[: max_chars - 1] + "..."
            draw_label(screen, font, raw, (10, WIN_H - 18), TEXT_FAINT,
                       center=False)
        draw_label(screen, font,
                   "NEPTR Monitor — Esc to quit",
                   (WIN_W - 220, WIN_H - 18), TEXT_FAINT, center=False)

        pygame.display.flip()
        clock.tick(60)

    reader.stop()
    pygame.quit()


if __name__ == "__main__":
    main()
