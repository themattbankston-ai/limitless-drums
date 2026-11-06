# The parts the app draws the track headers and the big fader with, so they fit any window size:
# a module (top, a stretchable middle row, bottom), the pan knobs (mono, stereo), the fader caps (small, big),
# the big fader body without its cap, and the title plate. Same materials and light as the rack.
# Usage: python3 render_appparts.py <out dir> [scale]
import os, sys, math
os.environ['LD_NO_BUILD'] = '1'
sys.path.insert(0, os.path.dirname(__file__))
out = sys.argv[1] if len(sys.argv) > 1 else '.'
scale = sys.argv[2] if len(sys.argv) > 2 else '3'
sys.argv = [sys.argv[0], out, scale]
import render_heads as R
P = R.P

def scene(w, h, transparent=True):
    s = P.reset(); s.render.film_transparent = transparent; s.cycles.samples = 96
    R.W, R.H = w, h
    t = math.radians(9)
    cam = P.camera(s, w, h, max(w, h) * R.U * (math.cos(t) if h >= w else 1))
    cam.location = (0, -10 * math.sin(t), 10 * math.cos(t)); cam.rotation_euler = (t, 0, 0)
    R.COLLAR = R.mat('collar', (0.008, 0.008, 0.01), 0.5)
    R.ACRYLIC = R.mat('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    R.BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    R.SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    return s

def lights(s):
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))

def anod(): return P.metal('anod', (0.045, 0.046, 0.052), 0.3, brushed=True, scale=28)
def capm(): return P.metal('cap', (0.72, 0.73, 0.77), 0.2, brushed=True, scale=60)

# a module, 228 x 120: sliced later into top (0-66), middle (one row at 90), bottom (110-120)
s = scene(228, 120)
plate = R.box('mod', 0, 0, 228, 120, 0.14, 0.0, 0.05, 0.012, anod())
R.well(plate, 12, 6.5, 134, 22, 0.05, 0.04, R.mat('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0))
B = R.mat('button', (0.022, 0.023, 0.027), 0.34, coat=0.6)
R.box('s', 12, 35.5, 20, 22, 0.07, 0.105, 0.035, 0.01, B)
R.box('m', 36, 35.5, 20, 22, 0.07, 0.105, 0.035, 0.01, B)
lights(s); P.render(s, 'module.png')

for name, r in (('knob_mono.png', 9.5), ('knob_stereo.png', 8.5)):
    side = round(r * 3.2)
    s = scene(side, side)
    R.knob(side / 2, side / 2, r)
    lights(s); P.render(s, name)

s = scene(32, 20); R.box('cap', 4, 4, 24, 12, 0.1, 0.12, 0.03, 0.012, capm()); lights(s); P.render(s, 'cap_small.png')
s = scene(60, 40); R.box('cap', 6, 6, 48, 28, 0.16, 0.16, 0.06, 0.02, capm()); lights(s); P.render(s, 'cap_big.png')

s = scene(236, 40); R.box('title', 0, 0, 236, 40, 0.12, 0.0, 0.03, 0.008, anod()); lights(s); P.render(s, 'title.png')
