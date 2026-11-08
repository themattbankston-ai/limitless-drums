# Turns the plugin faceplate renders into what the app draws: opaque plates as JPEG, glow layers as additive JPEG
# (black = no light), parts with alpha as PNG, and for Skull Cutter the spinning saw as frames composited over the
# plate (one tooth period, sharp and motion-blurred), so the app only swaps pictures.
# Usage: python3 make_plugin_assets.py <render out dir> <app assets dir>
import sys, os, math
import numpy as np
from PIL import Image, ImageFilter

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
SX, SY = 1 / 1.01, 0.98163 / 1.01

def load(p): return Image.open(os.path.join(src, p)).convert('RGBA')
def jpg(im, name, q=90): im.convert('RGB').save(os.path.join(dst, name + '.jpg'), quality=q, optimize=True); print('wrote', name)
def png(im, name): im.save(os.path.join(dst, name + '.png'), optimize=True); print('wrote', name)
def add_layer(hot, cold, tint=None):
    a = np.asarray(hot).astype(np.float32)[..., :3] * (np.asarray(hot).astype(np.float32)[..., 3:4] / 255)
    b = np.asarray(cold).astype(np.float32)[..., :3] * (np.asarray(cold).astype(np.float32)[..., 3:4] / 255)
    h = np.clip(a - b, 0, 255) / 255
    if tint == 'ember':
        l = h.max(axis=2); t = np.clip((l - 0.45) / 0.55, 0, 1)[..., None]
        h = (np.array([1.0, 0.22, 0.04]) * (1 - t) + np.array([1.0, 0.62, 0.32]) * t) * np.clip(l * 1.15, 0, 1)[..., None]
    return Image.fromarray((np.clip(h, 0, 1) * 255).astype('uint8'))

# ---------------------------------------------------------------- Skull Cutter
sk = os.path.join(src, 'sk')
if os.path.isdir(sk):
    src0 = src; src = sk
    plate, saw, skull = load('skull_plate.png'), load('skull_saw.png'), load('skull_skull.png')
    W, H = plate.size; k = W / 980
    jpg(plate, 'skull_plate')
    jpg(add_layer (load('skull_plate_cracks.png'), plate, 'ember'), 'skull_cracks')
    png(skull, 'skull_skull')
    jpg(add_layer (load('skull_skull_eyes.png'), skull, 'ember'), 'skull_eyes')
    # the saw: centre (panel 490, 222, raised 0.04), one tooth = 360/54 degrees
    cx = (980 / 2 + (490 - 980 / 2) * SX) * k
    cy = (544 / 2 + (222 - 544 / 2) * SY - 0.04 * 100 * math.sin(math.radians(11)) / 1.01) * k
    R = int(170 * k)
    # the top stops short of the model tags so the saw frames never lay a seam across them
    box = (int(cx - R), max(int(cy - R), int(124 * k / 2)), int(cx + R), int(cy + R))
    ox, oy = cx - box[0], cy - box[1]
    base = plate.crop(box)
    sawc = saw.crop(box)
    # shadow: the blade's alpha, blurred, offset down-right
    sh = Image.new('RGBA', base.size, (0, 0, 0, 0))
    a = sawc.split()[3].filter(ImageFilter.GaussianBlur(9 * k))
    sh.putalpha(a.point(lambda v: int(v * 0.72)))
    base_sh = Image.alpha_composite(base, Image.new('RGBA', base.size, (0, 0, 0, 0)))
    tmp = Image.new('RGBA', base.size, (0, 0, 0, 0)); tmp.paste(sh, (int(10 * k), int(16 * k)), sh)
    base_sh = Image.alpha_composite(base, tmp)
    def rot(im, deg):
        # rotate on the panel (undo the camera's squash, rotate, squash again)
        w, h = im.size
        s = SX / SY
        big = im.resize((w, int(h * s)), Image.BICUBIC).rotate(-deg, resample=Image.BICUBIC, center=(ox, oy * s))
        return big.resize((w, h), Image.BICUBIC)
    N = 8; tooth = 360 / 54
    for f in range(N):
        sharp = rot(sawc, tooth * f / N)
        jpg(Image.alpha_composite(base_sh, sharp), 'skull_saw_%d' % f, 88)
        # motion blur: average of 12 rotations across one tooth
        acc = np.zeros((sawc.size[1], sawc.size[0], 4), np.float32)
        for j in range(12): acc += np.asarray(rot(sawc, tooth * (f / N + j / 12))).astype(np.float32)
        blur = Image.fromarray((acc / 12).astype('uint8'))
        jpg(Image.alpha_composite(base_sh, blur), 'skull_sawb_%d' % f, 88)
    with open(os.path.join(dst, 'skull_saw_box.txt'), 'w') as fh: fh.write('%d %d %d %d %d %d\n' % (*box, W, H))
    print('saw box', box)
    src = src0

# ---------------------------------------------------------------- The Vise
vd = os.path.join(src, 'vise')
if os.path.isdir(vd):
    src0 = src; src = vd
    plate, ram = load('vise_plate.png'), load('vise_ram.png')
    jpg(plate, 'vise_plate')
    jpg(add_layer(load('vise_plate_beacon.png'), plate), 'vise_beacon')
    # the ram: only its own box (it slides down in the app); box written beside it
    bb = ram.split()[3].getbbox()
    png(ram.crop(bb), 'vise_ram')
    with open(os.path.join(dst, 'vise_ram_box.txt'), 'w') as fh: fh.write('%d %d %d %d %d %d\n' % (*bb, *ram.size))
    print('ram box', bb)
    src = src0

# ---------------------------------------------------------------- Portcullis
pd = os.path.join(src, 'port')
if os.path.isdir(pd):
    src0 = src; src = pd
    plate, gate = load('port_plate.png'), load('port_gate.png')
    jpg(plate, 'port_plate')
    jpg(add_layer(load('port_plate_open.png'), plate), 'port_open')
    jpg(add_layer(load('port_plate_closed.png'), plate), 'port_closed')
    bb = gate.split()[3].getbbox()
    png(gate.crop(bb), 'port_gate')
    with open(os.path.join(dst, 'port_gate_box.txt'), 'w') as fh: fh.write('%d %d %d %d %d %d\n' % (*bb, *gate.size))
    print('gate box', bb)
    # the arch's stones around the opening, cut from the plate: drawn over the raised gate so its bars stay behind the arch
    from PIL import ImageDraw
    k = plate.size[0] / 980
    def P(px, py, z=0): return ((980 / 2 + (px - 980 / 2) * SX) * k, (544 / 2 + (py - 544 / 2) * SY - z * 100 * math.sin(math.radians(11)) / 1.01) * k)
    x0, x1, ys, yb, ry, grow = 340, 640, 182, 338, 76, 6
    cx, rx = (x0 + x1) / 2, (x1 - x0) / 2 + grow
    pts = [P(x0 - grow, yb + grow)] + [P(cx + math.cos(math.pi + math.pi * i / 48) * rx, ys + math.sin(math.pi + math.pi * i / 48) * (ry + grow)) for i in range(49)] + [P(x1 + grow, yb + grow)]
    top = P(0, ys - ry - grow - 40)[1]
    box = (int(P(x0 - grow - 4, 0)[0]), int(top), int(P(x1 + grow + 4, 0)[0]) + 1, int(P(0, yb + grow + 2)[1]) + 1)
    mask = Image.new('L', plate.size, 255); ImageDraw.Draw(mask).polygon(pts, fill=0)
    corners = plate.copy(); corners.putalpha(mask)
    png(corners.crop(box), 'port_arch')
    with open(os.path.join(dst, 'port_arch_box.txt'), 'w') as fh: fh.write('%d %d %d %d\n' % box)
    print('arch box', box)
    src = src0

# ---------------------------------------------------------------- Ember
ed = os.path.join(src, 'ember')
if os.path.isdir(ed):
    src0 = src; src = ed
    cold = load('ember.png')
    jpg(cold, 'ember_plate')
    jpg(add_layer(load('ember_tubes.png'), cold), 'ember_hot')
    src = src0
