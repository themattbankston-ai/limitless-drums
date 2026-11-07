# Printed faces for EMBER: the VU dial (cream, backlit amber in the render).
import math, sys, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np
out = sys.argv[1] if len(sys.argv) > 1 else 'tex'; F = sys.argv[2] if len(sys.argv) > 2 else '/tmp/claude-0/mk/fonts'
os.makedirs(out, exist_ok=True)
OS = lambda s: ImageFont.truetype(F + '/Oswald[wght].ttf', s); CZ = lambda s: ImageFont.truetype(F + '/CinzelDecorative-Bold.ttf', s)
W, H = 1024, 540
yy, xx = np.mgrid[0:H, 0:W]; r = np.hypot((xx - W / 2) / W, (yy - H * 0.95) / H)
px = np.array([0.98, 0.9, 0.7])[None, None, :] * (1 - 0.35 * np.clip(r - 0.25, 0, 1))[..., None]
px += np.random.default_rng(4).normal(0, 0.012, (H, W, 1))
im = Image.fromarray((np.clip(px, 0, 1) * 255).astype('uint8')); d = ImageDraw.Draw(im)
cx, cy, R = W / 2, H * 1.18, H * 0.92; ink = (34, 22, 12); red = (160, 30, 16)
a0, a1 = math.radians(-128), math.radians(-52)
def P(a, rr): return (cx + math.cos(a) * rr, cy + math.sin(a) * rr)
for k in range(500):
    a = a0 + (a1 - a0) * k / 500; d.ellipse([*(v - 2.5 for v in P(a, R)), *(v + 2.5 for v in P(a, R))], fill=ink)
for k in range(200):
    a = a0 + (a1 - a0) * (0.7 + 0.3 * k / 200)
    for t in range(16): d.point(P(a, R + 6 + t), fill=red)
marks = [('20', 0), ('10', 0.22), ('7', 0.36), ('5', 0.47), ('3', 0.58), ('1', 0.66), ('0', 0.7), ('1', 0.8), ('2', 0.9), ('3', 1.0)]
for t, v in marks:
    a = a0 + (a1 - a0) * v; c = red if v >= 0.7 else ink
    d.line([P(a, R), P(a, R + 36)], fill=c, width=6); d.text(P(a, R + 72), t, font=OS(44), fill=c, anchor='mm')
for k in range(41):
    v = k / 40; a = a0 + (a1 - a0) * v; d.line([P(a, R), P(a, R + 16)], fill=red if v >= 0.7 else ink, width=3)
d.text((cx, H * 0.62), 'VU', font=CZ(80), fill=ink, anchor='mm')
d.text((cx, H * 0.8), 'EMBER  ·  PROGRAM LEVEL', font=OS(30), fill=(110, 80, 50), anchor='mm')
im.filter(ImageFilter.GaussianBlur(0.7)).save(os.path.join(out, 'ember_vu.png')); print('ok')
