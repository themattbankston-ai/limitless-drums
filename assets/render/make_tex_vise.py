# Printed faces for THE VISE: the pressure gauge dial (gain reduction) and the warning plate.
import math, sys, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np
out = sys.argv[1] if len(sys.argv) > 1 else 'tex'; F = sys.argv[2] if len(sys.argv) > 2 else '/tmp/claude-0/mk/fonts'
os.makedirs(out, exist_ok=True)
OS = lambda s: ImageFont.truetype(F + '/Oswald[wght].ttf', s); OPS = lambda s: ImageFont.truetype(F + '/BlackOpsOne-Regular.ttf', s)
rng = np.random.default_rng(5)
# ---- gauge: white enamel, black print, red zone; scale 0..20 dB over 270 degrees, needle pivot at centre
N = 1024; yy, xx = np.mgrid[0:N, 0:N] / N; r = np.hypot(xx - 0.5, yy - 0.5)
px = np.ones((N, N, 3)) * np.array([0.95, 0.94, 0.9]) * (1 - 0.25 * np.clip(r - 0.3, 0, 1))[..., None]
px += rng.normal(0, 0.01, (N, N, 1))
im = Image.fromarray((np.clip(px, 0, 1) * 255).astype('uint8')); d = ImageDraw.Draw(im)
cx = cy = N / 2; R = N * 0.4; ink = (20, 20, 22); red = (190, 24, 18)
a0, a1 = math.radians(135), math.radians(405)
def P(a, rr): return (cx + math.cos(a) * rr, cy + math.sin(a) * rr)
# red zone arc 12..20
for k in range(300):
    a = a0 + (a1 - a0) * (12 / 20 + 8 / 20 * k / 300)
    for t in range(26): d.point(P(a, R - 26 + t), fill=red)
for k in range(201):
    v = k / 200; a = a0 + (a1 - a0) * v; major = k % 20 == 0; mid = k % 10 == 0
    l = 50 if major else 34 if mid else 18; w = 7 if major else 4 if mid else 2
    d.line([P(a, R), P(a, R - l)], fill=red if v >= 0.6 else ink, width=w)
for i in range(11):
    v = i / 10; a = a0 + (a1 - a0) * v
    d.text(P(a, R - 100), str(i * 2), font=OPS(64), fill=red if v >= 0.6 else ink, anchor='mm')
d.text((cx, cy + R * 0.42), 'dB', font=OPS(56), fill=ink, anchor='mm')
d.text((cx, cy + R * 0.62), 'GAIN REDUCTION', font=OS(34), fill=(70, 70, 74), anchor='mm')
d.text((cx, cy - R * 0.36), 'PRESSURE', font=OPS(40), fill=ink, anchor='mm')
d.ellipse([cx - R - 4, cy - R - 4, cx + R + 4, cy + R + 4], outline=ink, width=5)
im = im.filter(ImageFilter.GaussianBlur(0.7)); im.save(os.path.join(out, 'vise_gauge.png'))
# ---- warning plate: yellow, black border, crush pictogram, text
Wp, Hp = 1024, 400
im = Image.new('RGB', (Wp, Hp), (236, 178, 12)); d = ImageDraw.Draw(im)
d.rounded_rectangle([10, 10, Wp - 10, Hp - 10], 30, outline=(15, 15, 15), width=16)
d.rectangle([10, 10, Wp - 10, 110], fill=(15, 15, 15))
d.text((Wp / 2 + 30, 62), 'WARNING', font=OPS(70), fill=(236, 178, 12), anchor='mm')
d.polygon([(250, 92), (310, 92), (280, 34)], fill=(236, 178, 12)); d.text((280, 74), '!', font=OPS(40), fill=(15, 15, 15), anchor='mm')
# pictogram: the ram coming down on a transient
d.rectangle([60, 132, 260, 172], fill=(15, 15, 15)); d.rectangle([145, 118, 175, 132], fill=(15, 15, 15))
for ax in (95, 225): d.polygon([(ax - 14, 182), (ax + 14, 182), (ax, 204)], fill=(15, 15, 15))
pts = [(60, 300)]
for k in range(40):
    x = 60 + k * 5; amp = 82 * math.exp(-k / 9) * (1 if k % 2 else -0.6); pts.append((x, 300 - max(amp, -20)))
pts += [(260, 300)]; d.line(pts, fill=(15, 15, 15), width=7, joint='curve')
d.rectangle([60, 300, 260, 312], fill=(15, 15, 15))
d.text((640, 185), 'CRUSH HAZARD', font=OPS(66), fill=(15, 15, 15), anchor='mm')
d.text((640, 262), 'KEEP TRANSIENTS CLEAR', font=OS(44), fill=(15, 15, 15), anchor='mm')
d.text((640, 322), 'OF THE RAM WHILE IN OPERATION', font=OS(36), fill=(15, 15, 15), anchor='mm')
a = np.asarray(im).astype(np.float32) / 255
grime = np.array(Image.fromarray((rng.random((Hp // 8, Wp // 8)) * 255).astype('uint8')).resize((Wp, Hp), Image.BICUBIC)) / 255
a *= (1 - 0.18 * grime)[..., None]
Image.fromarray((a * 255).astype('uint8')).filter(ImageFilter.GaussianBlur(0.6)).save(os.path.join(out, 'vise_warning.png'))
print('ok')
