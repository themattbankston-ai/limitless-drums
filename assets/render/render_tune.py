# The Tune step's rack: a strobe tuner in three modules, rendered in Blender (Cycles).
#   TUNE       the read-from selector, a round strobe window, the readout, the cents meter, Tune it / Read again,
#              one ratio / even out RRs, and the target, fine and A4 knobs with a Nearest key
#   PITCH MAP  every hit, as recorded and tuned
#   LISTEN     original, tuned, A/B, recorded note, target note
# Everything that lights up is drawn live in code on top. Layout in panel pixels (318 x 664), origin top left.
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import render_rack as R
from render_rack import P, U, W, H, at, box, cut, module, window, button, knob, mat_principled

T = dict(y=6, h=386); M = dict(y=398, h=150); L = dict(y=554, h=104)
WINDOWS = [(162, 34, 130, 26), (160, 70, 132, 118), (26, 198, 266, 30), (26, 416, 266, 120)]
KEYS = [(26, 240, 130, 28), (162, 240, 130, 28), (26, 276, 130, 26), (162, 276, 130, 26), (230, 318, 62, 26),
        (26, 572, 84, 26), (117, 572, 84, 26), (208, 572, 84, 26), (26, 606, 130, 26), (162, 606, 130, 26)]
KNOBS = [(59, 330), (126, 330), (193, 330)]
STROBE = (88, 129, 58)

def strobe(plate, px, py, r):
    """a round, deep window with a machined bezel ring: the strobe disc spins behind the glass (drawn in code)"""
    x, y, _ = at(px, py)
    bpy.ops.mesh.primitive_cylinder_add(vertices=200, radius=r * U, depth=0.6, location=(x, y, 0.05 + 0.3 - 0.12))
    c = bpy.context.object; cut(plate, c)
    bpy.ops.mesh.primitive_circle_add(vertices=200, radius=r * U, fill_type='NGON', location=(x, y, -0.12 + 0.005))
    fl = bpy.context.object; fl.data.materials.append(R.GLASS)
    bpy.ops.mesh.primitive_torus_add(major_radius=(r + 4) * U, minor_radius=4.2 * U, major_segments=220, minor_segments=32, location=(x, y, 0.07))
    t = bpy.context.object; t.scale = (1, 1, 0.6); bpy.ops.object.shade_smooth(); t.data.materials.append(R.SILVER)

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
    mm = module('pitch', M['y'], M['h']); ml = module('listen', L['y'], L['h'])
    # the tuner wears Tune's colour: a deep olive-green anodised faceplate, like a stage strobe tuner
    R.ANOD = P.metal('anodg', (0.03, 0.05, 0.025), 0.28, brushed=True, scale=28)
    mt = module('tune', T['y'], T['h'])
    for (x, y, w, h) in WINDOWS:
        window(mt if y < 398 else mm, x, y, w, h, depth=0.06)
    strobe(mt, *STROBE)
    for k in KEYS: button(*k)
    for k in KNOBS: knob(*k, 18)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    P.render(s, 'rack_tune.png')

build()
