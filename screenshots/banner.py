#!/usr/bin/env python3
"""Build the Cardia store banner (720x320) as SVG."""

W, H = 720, 320
YB = 264  # ECG baseline

def complex_pts(x0, yb, r_amp, t_amp=16, p_amp=10):
    return [
        (x0,       yb),
        (x0 + 16,  yb),
        (x0 + 24,  yb - p_amp),
        (x0 + 32,  yb),
        (x0 + 48,  yb),
        (x0 + 53,  yb + 10),
        (x0 + 58,  yb - r_amp),
        (x0 + 64,  yb + 20),
        (x0 + 70,  yb),
        (x0 + 86,  yb),
        (x0 + 94,  yb - t_amp),
        (x0 + 102, yb),
    ]

# x advance and R amplitude per beat. Middle beats are irregular (AF hint).
beats = [
    (112, 52), (112, 52), (112, 50),   # regular
    (72, 40), (134, 60), (64, 33), (150, 56),  # irregularly irregular
    (112, 52), (112, 52), (112, 50),   # regular again
]

pts = [(-20, YB)]
x = 40
for adv, r in beats:
    pts += complex_pts(x, YB, r)
    x += adv
pts.append((W + 20, YB))

path = "M " + " L ".join(f"{px:.0f},{py:.0f}" for px, py in pts)

heart = ("M 0,6 C 0,2 -4,-4 -11,-4 C -20,-4 -26,3 -26,12 "
         "C -26,26 -12,38 0,48 C 12,38 26,26 26,12 "
         "C 26,3 20,-4 11,-4 C 4,-4 0,2 0,6 Z")

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#181b21"/>
      <stop offset="1" stop-color="#0b0c0f"/>
    </linearGradient>
    <radialGradient id="vig" cx="0.32" cy="0.4" r="0.9">
      <stop offset="0" stop-color="#20242c" stop-opacity="0.55"/>
      <stop offset="1" stop-color="#000000" stop-opacity="0"/>
    </radialGradient>
    <filter id="glow" x="-20%" y="-80%" width="140%" height="260%">
      <feGaussianBlur stdDeviation="2.4" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
  </defs>

  <rect width="{W}" height="{H}" fill="url(#bg)"/>
  <rect width="{W}" height="{H}" fill="url(#vig)"/>

  <g transform="translate(602,150) scale(3.0)" fill="#ff453a" opacity="0.08">
    <path d="{heart}"/>
  </g>

  <path d="{path}" fill="none" stroke="#ff453a" stroke-width="2.6"
        stroke-linejoin="round" stroke-linecap="round" opacity="0.22"/>
  <path d="{path}" fill="none" stroke="#ff453a" stroke-width="1.9"
        stroke-linejoin="round" stroke-linecap="round" filter="url(#glow)"/>

  <g font-family="DejaVu Sans, sans-serif">
    <text x="56" y="132" font-size="82" font-weight="bold" fill="#ffffff"
          letter-spacing="-1">Cardia</text>
    <text x="59" y="170" font-size="22" fill="#9aa0a8">A closer watch on your heart rhythm</text>
    <text x="60" y="205" font-size="14" fill="#c92f27" letter-spacing="2.5"
          font-weight="bold">SUSTAINED HIGH  <tspan fill="#5f6670">&#183;</tspan>  SUSTAINED LOW  <tspan fill="#5f6670">&#183;</tspan>  IRREGULAR RHYTHM</text>
    <text x="664" y="300" font-size="13" fill="#5b616b" text-anchor="end">PebbleOS &#183; not a medical device</text>
  </g>
</svg>'''

with open("banner.svg", "w") as f:
    f.write(svg)
print("wrote banner.svg")
