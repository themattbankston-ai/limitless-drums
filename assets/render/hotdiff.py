# Hot layers for the plugin faceplates: (hot render - cold render), premultiplied, clipped at zero, so the app can
# add them over the cold image by an amount (Drive, Destroy...). Optional ember tint.
# Usage: python3 hotdiff.py <render dir> <base name> <out dir> <variant>[:tint] ...
import sys, numpy as np
from PIL import Image
d, base, out = sys.argv[1:4]
def load(n):
    im = Image.open(f'{d}/{n}.png').convert('RGBA'); a = np.asarray(im).astype(np.float32) / 255
    return a[..., :3] * a[..., 3:4]
c = load(base)
for v in sys.argv[4:]:
    name, _, tint = v.partition(':')
    h = np.clip(load(f'{base}_{name}') - c, 0, 1)
    if tint == 'ember':
        l = h.max(axis=2); t = np.clip((l - 0.45) / 0.55, 0, 1)[..., None]
        h = (np.array([1.0, 0.22, 0.04]) * (1 - t) + np.array([1.0, 0.62, 0.32]) * t) * np.clip(l * 1.15, 0, 1)[..., None]
    if tint == 'green':
        l = h.max(axis=2); t = np.clip((l - 0.5) / 0.5, 0, 1)[..., None]
        h = (np.array([0.15, 1.0, 0.45]) * (1 - t) + np.array([0.7, 1.0, 0.8]) * t) * np.clip(l * 1.1, 0, 1)[..., None]
    if tint == 'amber':
        l = h.max(axis=2); t = np.clip((l - 0.5) / 0.5, 0, 1)[..., None]
        h = (np.array([1.0, 0.5, 0.03]) * (1 - t) + np.array([1.0, 0.8, 0.45]) * t) * np.clip(l * 1.1, 0, 1)[..., None]
    Image.fromarray((np.clip(h, 0, 1) * 255).astype('uint8')).save(f'{out}/{base}_{name}_add.png')
Image.open(f'{d}/{base}.png').save(f'{out}/{base}.png'); print('ok', base)
