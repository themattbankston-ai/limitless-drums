import sys, numpy as np
from PIL import Image
d = sys.argv[1]; base = sys.argv[2]; out = sys.argv[3]
c = np.asarray(Image.open(f'{d}/{base}.png').convert('RGB')).astype(np.float32)
for v in sys.argv[4:]:
    h = np.asarray(Image.open(f'{d}/{base}_{v}.png').convert('RGB')).astype(np.float32)
    Image.fromarray(np.clip(h - c, 0, 255).astype('uint8')).save(f'{out}/{base}_{v}_add.png')
Image.fromarray(c.astype('uint8')).save(f'{out}/{base}.png'); print('ok')
