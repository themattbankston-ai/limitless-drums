# Printed faces for the Skull Cutter render: the VU dial (aged paper, scale, red zone).
import math, sys, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np
out = sys.argv[1] if len(sys.argv) > 1 else 'tex'; F = sys.argv[2] if len(sys.argv) > 2 else '/tmp/claude-0/mk/fonts'
N = 1024; im = Image.new('RGB', (N, N)); px = np.zeros((N, N, 3))
yy, xx = np.mgrid[0:N, 0:N] / N
rng = np.random.default_rng(3)
# aged paper: warm cream, darker toward the rim, blotches
r = np.hypot(xx - 0.5, yy - 0.62)
base = np.array([0.93, 0.84, 0.64])
noise = rng.normal(0, 1, (N // 16, N // 16)); noise = np.array(Image.fromarray(((noise - noise.min()) / np.ptp(noise) * 255).astype('uint8')).resize((N, N), Image.BICUBIC)) / 255
grain = rng.normal(0, 0.025, (N, N))
shade = 1 - 0.55 * np.clip(r - 0.25, 0, 1) - 0.1 * (noise - 0.5) + grain
px = base[None, None, :] * shade[..., None]
px[..., 2] -= 0.08 * noise   # yellowed patches
im = Image.fromarray((np.clip(px, 0, 1) * 255).astype('uint8'))
d = ImageDraw.Draw(im)
cx, cy, R = N / 2, N * 0.80, N * 0.42
ink = (38, 26, 16); red = (150, 22, 14)
fnt = ImageFont.truetype(F + '/Oswald[wght].ttf', 44); fsm = ImageFont.truetype(F + '/Oswald[wght].ttf', 30); ftitle = ImageFont.truetype(F + '/MetalMania-Regular.ttf', 84)
a0, a1 = math.radians(-140), math.radians(-40)
def P(a, rr): return (cx + math.cos(a) * rr, cy + math.sin(a) * rr)
# arc
for k in range(400):
    a = a0 + (a1 - a0) * k / 400; d.ellipse([*(v - 2.5 for v in P(a, R)), *(v + 2.5 for v in P(a, R))], fill=ink)
# red zone band
for k in range(160):
    a = a0 + (a1 - a0) * (0.69 + 0.31 * k / 160)
    for t in range(14): x, y = P(a, R + 6 + t); d.point((x, y), fill=red)
marks = [('20', 0), ('10', 0.2), ('7', 0.33), ('5', 0.44), ('3', 0.55), ('2', 0.61), ('1', 0.65), ('0', 0.69), ('1', 0.78), ('2', 0.87), ('3', 1.0)]
for i, (t, v) in enumerate(marks):
    a = a0 + (a1 - a0) * v; c = red if v >= 0.69 else ink
    d.line([P(a, R - 2), P(a, R + 34)], fill=c, width=6)
    x, y = P(a, R + 70); d.text((x, y), t, font=fnt, fill=c, anchor='mm')
for k in range(41):
    v = k / 40; a = a0 + (a1 - a0) * v; d.line([P(a, R), P(a, R + 16)], fill=red if v >= 0.69 else ink, width=3)
x, y = P(a0, R - 40); d.text((x + 20, y), '−', font=fnt, fill=ink, anchor='mm')
x, y = P(a1, R - 40); d.text((x - 20, y), '+', font=fnt, fill=red, anchor='mm')
d.text((cx, cy - R * 0.38), 'VU', font=ftitle, fill=ink, anchor='mm')
d.text((cx, cy - R * 0.2), 'BLOOD LEVEL', font=fsm, fill=(90, 60, 40), anchor='mm')
# smudges & a coffee ring
d.ellipse([N * 0.62, N * 0.18, N * 0.86, N * 0.42], outline=(196, 160, 110), width=3)
im = im.filter(ImageFilter.GaussianBlur(0.8))
os.makedirs(out, exist_ok=True); im.save(os.path.join(out, 'skull_vu.png')); print('ok')
