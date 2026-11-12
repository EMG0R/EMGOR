#!/usr/bin/env python3
"""Generate the PMOR app icon.

Drawn from the real device: a hand-poured mint/teal shell, a dark OLED with a
bright graphic, a chain across the middle, a row of white LEDs, and a 4x4 grid
of backlit pads glowing pink and blue.

Deliberately drawn rather than photo-traced: at 32px in a Dock the photo turns
to mud, while a flat high-contrast rendering of the same silhouette and colours
stays readable. The pad grid is the thing that makes it recognisable, so it
gets the most contrast.

Run:  ./venv/bin/python make_icon.py     -> PMOR.iconset/ + PMOR.icns
"""
import os
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFilter

S = 1024  # master size; macOS downsamples the rest

# Palette lifted from the device photo
SHELL      = (150, 226, 214)   # mint/teal body
SHELL_EDGE = (104, 190, 179)   # shaded lower edge
SCREEN_BG  = (14, 18, 26)      # near-black OLED
SCREEN_ART = (232, 246, 255)   # the bright graphic on the OLED
CHAIN      = (128, 138, 150)   # steel chain across the middle
LED_WARM   = (255, 246, 236)   # white LED row
PAD_PINK   = (255, 74, 186)
PAD_BLUE   = (86, 116, 255)
PAD_HOT    = (255, 255, 255)   # the one lit-white pad
BG_TOP     = (24, 28, 38)
BG_BOT     = (10, 12, 18)


def rounded(draw, box, r, fill):
    draw.rounded_rectangle(box, radius=r, fill=fill)


def glow(img, box, colour, radius, alpha=170):
    """Soft additive bloom under a pad — what sells 'backlit' at small sizes."""
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle(box, radius=radius, fill=colour + (alpha,))
    layer = layer.filter(ImageFilter.GaussianBlur(S * 0.018))
    img.alpha_composite(layer)


def build():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # backdrop: subtle vertical gradient so the mint shell pops on dark Docks
    for y in range(S):
        t = y / S
        c = tuple(int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * t) for i in range(3))
        d.line([(0, y), (S, y)], fill=c + (255,))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
    img.putalpha(mask)

    # ── body ────────────────────────────────────────────────────────────
    bx0, by0, bx1, by1 = int(S * 0.20), int(S * 0.08), int(S * 0.80), int(S * 0.94)
    rounded(d, [bx0, by0 + int(S * 0.012), bx1, by1], int(S * 0.17), SHELL_EDGE)
    rounded(d, [bx0, by0, bx1, by1 - int(S * 0.012)], int(S * 0.17), SHELL)

    # ── OLED ────────────────────────────────────────────────────────────
    sx0, sy0 = int(S * 0.28), int(S * 0.15)
    sx1, sy1 = int(S * 0.72), int(S * 0.40)
    rounded(d, [sx0, sy0, sx1, sy1], int(S * 0.02), SCREEN_BG)

    # graphic on the screen: a faceted diamond, echoing the device photo
    cx, cy = (sx0 + sx1) // 2, (sy0 + sy1) // 2
    w, h = int(S * 0.115), int(S * 0.072)
    d.polygon([(cx, cy - h), (cx + w, cy - h // 3), (cx, cy + h), (cx - w, cy - h // 3)],
              outline=SCREEN_ART, width=max(3, S // 200))
    d.line([(cx - w, cy - h // 3), (cx + w, cy - h // 3)], fill=SCREEN_ART, width=max(2, S // 320))
    d.line([(cx, cy - h), (cx, cy + h)], fill=SCREEN_ART, width=max(2, S // 320))

    # ── chain band ──────────────────────────────────────────────────────
    chy = int(S * 0.455)
    link_r = int(S * 0.020)
    x = bx0 + int(S * 0.035)
    while x < bx1 - int(S * 0.03):
        d.ellipse([x, chy - link_r, x + link_r * 2, chy + link_r], outline=CHAIN,
                  width=max(3, S // 220))
        x += int(link_r * 1.75)

    # ── white LED row ───────────────────────────────────────────────────
    ly = int(S * 0.535)
    lw, lh = int(S * 0.085), int(S * 0.038)
    gap = int(S * 0.022)
    total = lw * 4 + gap * 3
    lx = (S - total) // 2
    for i in range(4):
        box = [lx + i * (lw + gap), ly, lx + i * (lw + gap) + lw, ly + lh]
        glow(img, box, LED_WARM, int(S * 0.008), alpha=120)
        rounded(d, box, int(S * 0.008), LED_WARM)

    # ── 4x4 pad grid — the recognisable part ────────────────────────────
    # geometry note: 4*pw + 3*pgap must clear the body's bottom edge (0.94S).
    # 4*0.075 + 3*0.016 = 0.348, starting at 0.585 -> ends 0.933. Fits.
    pw = int(S * 0.075)
    pgap = int(S * 0.016)
    grid = pw * 4 + pgap * 3
    gx = (S - grid) // 2
    gy = int(S * 0.585)
    for r in range(4):
        for c in range(4):
            x0 = gx + c * (pw + pgap)
            y0 = gy + r * (pw + pgap)
            box = [x0, y0, x0 + pw, y0 + pw]
            if r < 2:
                colour = PAD_PINK
            else:
                colour = PAD_BLUE
            if r == 2 and c == 2:          # the single blown-out white pad
                colour = PAD_HOT
            glow(img, box, colour, int(S * 0.012), alpha=190)
            rounded(d, box, int(S * 0.012), colour)

    return img


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    icon = build()
    png = os.path.join(here, "PMOR-1024.png")
    icon.save(png)

    iconset = os.path.join(here, "PMOR.iconset")
    shutil.rmtree(iconset, ignore_errors=True)
    os.makedirs(iconset)
    # the sizes macOS actually asks for, including @2x
    for size in (16, 32, 128, 256, 512):
        for scale, suffix in ((1, ""), (2, "@2x")):
            px = size * scale
            icon.resize((px, px), Image.LANCZOS).save(
                os.path.join(iconset, f"icon_{size}x{size}{suffix}.png"))
    subprocess.run(["iconutil", "-c", "icns", iconset,
                    "-o", os.path.join(here, "PMOR.icns")], check=True)
    print("wrote PMOR.icns and PMOR-1024.png")


if __name__ == "__main__":
    main()
