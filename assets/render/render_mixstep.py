# The Mix step's rack: a channel strip in four modules, rendered in Blender (Cycles).
#   TONE       source keys (libraries / studio), the track selector, the tone curve screen, five knobs
#   SHAPE      the envelope screen, five knobs
#   REFERENCE  the reference deck window, the amount knob, reference vs mix, change, remove
#   CONSOLE    three insert slots; standard start, all flat, before / after
# Everything that lights up is drawn live in code on top. Layout in panel pixels (318 x 664), origin top left.
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import render_rack as R
from render_rack import P, U, W, H, at, box, cut, module, window, button, knob, mat_principled

TO = dict(y=6, h=248); SH = dict(y=260, h=150); RF = dict(y=416, h=132); CO = dict(y=554, h=104)
WINDOWS = [(162, 30, 130, 24), (26, 62, 266, 92), (26, 282, 266, 46), (26, 436, 200, 52)]
KEYS = [(26, 30, 64, 24), (94, 30, 64, 24),
        (26, 496, 130, 24), (162, 496, 62, 24), (230, 496, 62, 24),
        (26, 572, 84, 26), (117, 572, 84, 26), (208, 572, 84, 26),
        (26, 608, 84, 26), (117, 608, 84, 26), (208, 608, 84, 26)]
KNOBS = [(x, 190) for x in (47, 103, 159, 215, 271)] + [(x, 362) for x in (47, 103, 159, 215, 271)]

def build():
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = 80
    cam = P.camera(s, W, H, max(W, H) * U * 1.01)
    t = math.radians(11); cam.location = (0, -10 * math.sin(t), 10 * math.cos(t)); cam.rotation_euler = (t, 0, 0)
    R.ANOD = P.metal('anod', (0.045, 0.046, 0.052), 0.3, brushed=True, scale=28)
    R.STEEL = P.metal('steel', (0.6, 0.61, 0.64), 0.26)
    R.GLASS = mat_principled('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    R.COLLAR = mat_principled('collar', (0.008, 0.008, 0.01), 0.5)
    R.ACRYLIC = mat_principled('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    R.BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    R.SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    R.BUTTON = mat_principled('button', (0.022, 0.023, 0.027), 0.34, coat=0.6)
    box('cavity', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    for rx in (2, W - 16):
        box('rail', rx, 0, 14, H, 0.05, -0.15, 0.002, 0.006, P.metal('rail', (0.08, 0.08, 0.09), 0.4, brushed=True, scale=10))
    mr = module('ref', RF['y'], RF['h']); mc = module('console', CO['y'], CO['h'])
    # the channel strip wears Mix's colour: dark copper anodising, like a vintage console's module
    R.ANOD = P.metal('anodc', (0.055, 0.03, 0.016), 0.28, brushed=True, scale=28)
    mt = module('tone', TO['y'], TO['h']); ms = module('shape', SH['y'], SH['h'])
    for (x, y, w, h) in WINDOWS:
        window(mt if y < 260 else ms if y < 416 else mr, x, y, w, h, depth=0.06)
    for k in KEYS: button(*k)
    for k in KNOBS: knob(*k, 16)
    knob(259, 458, 15)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    P.render(s, 'rack_mixstep.png')

build()
