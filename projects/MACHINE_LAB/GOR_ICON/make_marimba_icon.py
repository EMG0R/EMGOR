#!/usr/bin/env python3
"""Realistic wooden marimba app icon (1024x1024): rosewood bars, grain, zoomed
in, no mallets. Pillow only."""
import math, random
from PIL import Image, ImageDraw, ImageFilter

S = 1024
OUT = "marimba_icon_1024.png"
random.seed(7)

# ---------- rounded-square warm-dark background ----------
bg = Image.new("RGB", (S, S))
d = ImageDraw.Draw(bg)
top = (40, 28, 20)      # warm espresso
bot = (14, 9, 6)        # near-black brown
for y in range(S):
    t = y / (S - 1)
    d.line([(0, y), (S, y)],
           fill=tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)))
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1],
                                       radius=int(S * 0.225), fill=255)
img.paste(bg, (0, 0), mask)


def make_bar(w, h, base):
    """A rounded rosewood bar: 3D shading + wood grain."""
    w, h = int(w), int(h)
    bar = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = bar.load()
    br, bg_, bb = base
    for x in range(w):
        # across-width shading: bright center, dark edges (rounded look)
        u = x / (w - 1)
        edge = 1.0 - (abs(u - 0.5) * 2) ** 2 * 0.55
        for y in range(h):
            v = y / (h - 1)
            # slight length-wise tone drift + faint banding for wood feel
            drift = 1.0 + 0.06 * math.sin(v * 9 + x * 0.02)
            s = edge * (0.9 + 0.15 * (1 - v)) * drift
            px[x, y] = (int(br * s), int(bg_ * s), int(bb * s), 255)
    # wood grain: wavy darker streaks running along the length
    g = ImageDraw.Draw(bar)
    for _ in range(int(w / 22)):
        gx = random.uniform(0, w)
        amp = random.uniform(1.5, 5)
        wob = random.uniform(0.01, 0.03)
        shade = random.randint(30, 70)
        pts = [(gx + amp * math.sin(y * wob + gx), y) for y in range(0, h, 6)]
        g.line(pts, fill=(shade, int(shade * 0.55), int(shade * 0.35), 90),
               width=random.choice([2, 3]))
    # top sheen
    g.rounded_rectangle([w * 0.14, h * 0.015, w * 0.5, h * 0.05],
                        radius=w * 0.1, fill=(255, 210, 160, 70))
    # round the corners
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w - 1, h - 1],
                                        radius=int(w * 0.34), fill=255)
    bar.putalpha(m)
    # mounting holes at nodal points
    hd = ImageDraw.Draw(bar)
    hr = w * 0.09
    for ny in (0.22, 0.80):
        cy = h * ny
        hd.ellipse([w/2 - hr, cy - hr, w/2 + hr, cy + hr], fill=(20, 12, 8, 235))
        hd.ellipse([w/2 - hr*0.5, cy - hr*0.7, w/2 + hr*0.1, cy], fill=(120, 80, 55, 120))
    return bar


# ---------- lay out big, zoomed-in bars ----------
n = 7
mx = S * 0.055
usable = S - 2 * mx
gap = usable * 0.022
bw = (usable - gap * (n - 1)) / n
top_y = S * 0.05
# rosewood tones, subtly varied per bar
tones = [(158, 86, 50), (150, 80, 46), (146, 78, 44), (140, 74, 42),
         (150, 82, 48), (156, 88, 52), (148, 80, 46)]

# shadow pass
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(shadow)

bars = []
for i in range(n):
    x0 = mx + i * (bw + gap)
    frac = 1 - (i / (n - 1)) * 0.10          # subtle, realistic length variation
    bh = (S * 0.92) * frac
    y0 = top_y
    y1 = top_y + bh
    bars.append((x0, y0, bw, bh, tones[i]))
    sd.rounded_rectangle([x0 + 12, y0 + 20, x0 + bw + 12, y1 + 20],
                         radius=bw * 0.34, fill=(0, 0, 0, 150))
shadow = shadow.filter(ImageFilter.GaussianBlur(18))
img.alpha_composite(shadow)

# the bars
for (x0, y0, bw_, bh, tone) in bars:
    bar = make_bar(bw_, bh, tone)
    img.alpha_composite(bar, (int(x0), int(y0)))

# faint inner rim
ImageDraw.Draw(img).rounded_rectangle([4, 4, S - 5, S - 5],
    radius=int(S * 0.225) - 4, outline=(255, 220, 180, 22), width=3)

img.save(OUT)
print("wrote", OUT)
