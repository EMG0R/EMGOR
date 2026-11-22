#!/usr/bin/env python3
"""draw callout labels on renders that have <name>.labels.json (made by hypertrumpet_v4_blend.py). venv python with pillow."""
import sys, os, json
from PIL import Image, ImageDraw, ImageFont
d = sys.argv[1]
try: font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 26)
except Exception: font = ImageFont.load_default()
for f in sorted(os.listdir(d)):
    if not f.endswith(".labels.json"): continue
    png = os.path.join(d, f[:-12]); im = Image.open(png).convert("RGB"); dr = ImageDraw.Draw(im)
    for txt, x, y, off in json.load(open(os.path.join(d, f))):
        tx, ty = x + off[0], y + off[1]; dr.line([(x, y), (tx, ty)], fill=(255, 235, 120), width=3); dr.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(255, 235, 120), width=3)
        w = dr.textlength(txt, font=font); dr.rectangle([tx - 6, ty - 18, tx + w + 6, ty + 18], fill=(20, 18, 30)); dr.text((tx, ty), txt, fill=(255, 235, 120), font=font, anchor="lm")
    im.save(png)
