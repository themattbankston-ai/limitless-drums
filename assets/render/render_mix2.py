# The Mix step, second design: the rack (TONE with Standard / All flat, SHAPE as a transient designer, REFERENCE
# with match curve and match envelope, twin REVERB units) and one mixer channel strip. Cycles.
# Usage: python3 render_mix2.py <out dir> [scale]
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import render_rack as R
from render_rack import P, U, box, cut, module, window, button, knob, mat_principled

def mats():
    R.ANOD = P.metal('anod', (0.045, 0.046, 0.052), 0.3, brushed=True, scale=28)
    R.STEEL = P.metal('steel', (0.6, 0.61, 0.64), 0.26)
    R.GLASS = mat_principled('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    R.COLLAR = mat_principled('collar', (0.008, 0.008, 0.01), 0.5)
    R.ACRYLIC = mat_principled('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    R.BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    R.SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    R.BUTTON = mat_principled('button', (0.022, 0.023, 0.027), 0.34, coat=0.6)

def lights(s):
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))

def cam(s, W, H):
    c = P.camera(s, W, H, max(W, H) * U * 1.01)
    t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)

def rack():
    R.W, R.H = 318, 664
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = 80
    cam(s, 318, 664); mats()
    box('cavity', 0, 0, 318, 664, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    for rx in (2, 318 - 16):
        box('rail', rx, 0, 14, 664, 0.05, -0.15, 0.002, 0.006, P.metal('rail', (0.08, 0.08, 0.09), 0.4, brushed=True, scale=10))
    R.ANOD = P.metal('anods2', (0.42, 0.43, 0.46), 0.5, brushed=True, scale=28)     # the reference deck: light brushed aluminium
    mref = module('ref', 364, 100)
    R.ANOD = P.metal('anodp', (0.07, 0.012, 0.06), 0.28, brushed=True, scale=28)   # the reverbs: deep plum
    mverb = module('verb', 470, 188)
    R.ANOD = P.metal('anodc', (0.055, 0.03, 0.016), 0.28, brushed=True, scale=28)  # the strip: dark copper
    mtone = module('tone', 6, 196)
    R.ANOD = P.metal('anodt', (0.008, 0.05, 0.058), 0.28, brushed=True, scale=28)  # the transient designer: deep teal, cool against the copper
    mshape = module('shape', 208, 150)
    for (x, y, w, h), m in [((208, 30, 84, 24), mtone), ((26, 60, 266, 72), mtone), ((26, 228, 266, 40), mshape),
                            ((26, 382, 150, 48), mref), ((30, 492, 122, 22), mverb), ((166, 492, 122, 22), mverb)]:
        window(m, x, y, w, h, depth=0.06)
    # the line between the two reverb units
    bpy.ops.mesh.primitive_cube_add(size=1, location=R.at(159, 564, 0.0)); g = bpy.context.object
    g.scale = (0.012, 176 * U, 0.03); cut(mverb, g)
    for k in [(26, 30, 84, 24), (117, 30, 84, 24), (26, 438, 104, 20), (134, 438, 76, 20), (214, 438, 78, 20)]: button(*k)
    for x in (59, 126, 193, 260): knob(x, 160, 15)
    knob(70, 312, 22); knob(248, 312, 22); knob(136, 312, 13); knob(182, 312, 13)
    knob(206, 404, 14); knob(262, 404, 14)
    for ox in (26, 162):
        knob(ox + 65, 548, 20)
        for i, dx in enumerate((18, 50, 82, 112)): knob(ox + dx, 610, 9)
    lights(s)
    P.render(s, 'rack_mix2.png')

def strip():
    W, H = 112, 690
    R.W, R.H = W, H
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = 80
    cam(s, W, H); mats()
    box('cavity', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    plate = P.metal('anods', (0.05, 0.051, 0.058), 0.42, brushed=True, scale=28)
    box('body', 3, 3, W - 6, H - 6, 0.14, -0.07, 0.03, 0.01, plate)
    # flush dark glass: the name, the readout, the meter; a recessed slot for the fader
    glass = mat_principled('sglass', (0.004, 0.004, 0.006), 0.12, coat=1.0)
    slot = mat_principled('slot', (0.003, 0.003, 0.004), 0.6)
    for (x, y, w, h) in [(8, 8, 96, 22), (20, 652, 72, 22), (84, 314, 10, 326)]:
        box('g', x, y, w, h, 0.012, 0.004, 0.02, 0.004, glass)
    box('slot', 52, 314, 8, 326, 0.012, 0.004, 0.02, 0.003, slot)
    # the insert slots: five low keys
    for i in range(5): button(8, 38 + i * 26, 96, 22)
    knob(32, 196, 11); knob(80, 196, 11); knob(56, 250, 13)
    button(12, 282, 42, 20); button(58, 282, 42, 20)
    lights(s)
    P.render(s, 'strip.png')


if __name__ == "__main__":
    if os.environ.get("LD_ONLY") != "strip": rack()
    if os.environ.get("LD_ONLY") != "rack": strip()
