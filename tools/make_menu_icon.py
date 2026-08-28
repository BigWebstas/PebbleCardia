#!/usr/bin/env python3
"""Generate the Cardia launcher icon: a heart with an ECG trace cut through it.

Output: resources/images/menu_icon.png  (25x25 RGBA, black shape on transparent -
the launcher tints it). Run from the repo root:  python3 tools/make_menu_icon.py
"""
import math
import os
from PIL import Image, ImageDraw, ImageChops

S = 400          # supersampled working canvas
OUT = 25         # Pebble menuIcon max is 25x25
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(ROOT, "resources", "images", "menu_icon.png")


def heart_mask(size, scale=0.86, cx=0.5, cy=0.46):
    m = Image.new("L", (size, size), 0)
    pts = []
    for i in range(721):
        t = math.radians(i * 0.5)
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((cx * size + x / 17 * scale * size / 2,
                    cy * size - y / 17 * scale * size / 2))
    ImageDraw.Draw(m).polygon(pts, fill=255)
    return m


def ecg_points(size):
    base = 0.47
    wave = [
        (0.00, base), (0.26, base),
        (0.33, base), (0.37, base + 0.05),   # shallow Q
        (0.42, base),
        (0.48, base - 0.30),                 # R spike (up)
        (0.53, base + 0.16),                 # modest S (down)
        (0.58, base),
        (1.00, base),
    ]
    return [(x * size, y * size) for x, y in wave]


heart = heart_mask(S)
ecg = Image.new("L", (S, S), 0)
ImageDraw.Draw(ecg).line(ecg_points(S), fill=255, width=int(S * 0.058), joint="curve")

cut = ImageChops.subtract(heart, ecg)          # heart minus the trace
outside = ImageChops.subtract(ecg, heart)      # trace where it isn't on the heart
alpha = ImageChops.lighter(cut, outside)       # continuous line, knocked out of the heart

icon = Image.composite(Image.new("RGBA", (S, S), (0, 0, 0, 255)),
                       Image.new("RGBA", (S, S), (0, 0, 0, 0)), alpha)
icon = icon.resize((OUT, OUT), Image.LANCZOS)

os.makedirs(os.path.dirname(DEST), exist_ok=True)
icon.save(DEST)
print("wrote", DEST, icon.size, icon.mode)
