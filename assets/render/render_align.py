# The Align step's rack: an oscilloscope instrument in three modules, rendered in Blender (Cycles).
#   ALIGN   the Align again key, a readout window, the three timing keys (tight, like the pros, as recorded)
#   SCOPE   two selector windows (which mic, what it lines up to), a deep CRT screen, a big knurled jog wheel
#           with nudge keys either side, snap / this hit, and the polarity, phase EQ and L=R keys
#   LISTEN  the sum-by-frequency window, the pair keys, hear what you gain, back to measured
# Everything that lights up is drawn live in code on top. Layout in panel pixels (318 x 664), origin top left.
# Usage: python3 render_align.py <out dir> [scale]
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import render_rack as R
from render_rack import P, U, W, H, at, box, cut, module, window, button, mat_principled

A = dict(y=6, h=118); S = dict(y=130, h=330); L = dict(y=466, h=192)
WINDOWS = [(204, 40, 88, 30), (26, 150, 130, 28), (162, 150, 130, 28), (26, 500, 266, 74)]
SCOPE = (26, 186, 266, 150)
KEYS = [(26, 40, 170, 30),                                        # align again
        (26, 82, 86, 26), (116, 82, 86, 26), (206, 82, 86, 26),   # timing
        (26, 350, 40, 26), (70, 350, 40, 26), (208, 350, 40, 26), (252, 350, 40, 26),   # nudge
        (26, 386, 84, 26), (208, 386, 84, 26),                    # this hit / spread, snap
        (26, 424, 84, 26), (117, 424, 84, 26), (208, 424, 84, 26), # flip, phase EQ, L=R
        (26, 586, 130, 28), (162, 586, 130, 28), (26, 620, 130, 28), (162, 620, 130, 28)]
JOG = (159, 380, 36)

def crt(plate, px, py, w, h):
    """a deep screen: a stepped bezel, then curved glass sitting low"""
    window(plate, px, py, w, h, depth=0.12)
    bez = P.rounded_box('bez', (w + 10) * U, (h + 10) * U, 0.03, 0.08, 0.01)
    bez.location = at(px + w / 2, py + h / 2, 0.072); bez.data.materials.append(R.BLACKMETAL)
    hole = P.rounded_box('bh', w * U, h * U, 0.2, 0.05, 0.01); hole.location = at(px + w / 2, py + h / 2, 0.07); cut(bez, hole)

def jog(px, py, r):
    x, y, _ = at(px, py); Rr = r * U
    # recessed well the wheel sits in
    bpy.ops.mesh.primitive_cylinder_add(vertices=160, radius=Rr * 1.18, depth=0.02, location=(x, y, 0.001))
    w = bpy.context.object; w.data.materials.append(R.COLLAR); bpy.ops.object.shade_smooth()
    # the frosted ring the code lights with the delay
    bpy.ops.mesh.primitive_torus_add(major_radius=Rr * 1.1, minor_radius=Rr * 0.05, major_segments=200, minor_segments=24, location=(x, y, 0.014))
    t = bpy.context.object; t.scale = (1, 1, 0.55); bpy.ops.object.shade_smooth(); t.data.materials.append(R.ACRYLIC)
    # a heavy knurled aluminium wheel
    n = 240
    bpy.ops.mesh.primitive_cylinder_add(vertices=n, radius=Rr, depth=0.13, location=(x, y, 0.075))
    sk = bpy.context.object
    for v in sk.data.vertices:
        a = math.atan2(v.co.y, v.co.x)
        if int(round((a + math.pi) / (2 * math.pi) * n)) % 2: v.co.x *= 0.975; v.co.y *= 0.975
    b = sk.modifiers.new('b', 'BEVEL'); b.width = Rr * 0.05; b.segments = 5; b.limit_method = 'ANGLE'; b.angle_limit = math.radians(50)
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth(); sk.data.materials.append(R.SILVER)
    # the face: dark, slightly dished, with a finger dimple off centre
    bpy.ops.mesh.primitive_cylinder_add(vertices=200, radius=Rr * 0.86, depth=0.02, location=(x, y, 0.145))
    face = bpy.context.object
    b = face.modifiers.new('b', 'BEVEL'); b.width = Rr * 0.05; b.segments = 4; b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth(); face.data.materials.append(R.BLACKMETAL)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=Rr * 0.2, location=(x + Rr * 0.48, y + Rr * 0.3, 0.165))
    d = bpy.context.object; d.scale = (1, 1, 0.35); cut(face, d)
    bpy.ops.object.shade_smooth()

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
    ma = module('align', A['y'], A['h']); ml = module('listen', L['y'], L['h'])
    # the scope itself wears Align's colour: deep violet anodising, like a lab instrument's coloured faceplate
    R.ANOD = P.metal('anodv', (0.05, 0.028, 0.095), 0.28, brushed=True, scale=28)
    ms = module('scope', S['y'], S['h'])
    for (x, y, w, h) in WINDOWS:
        window(ma if y < 130 else ms if y < 466 else ml, x, y, w, h, depth=0.06)
    crt(ms, *SCOPE)
    for k in KEYS: button(*k)
    jog(*JOG)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    P.render(s, 'rack_align.png')

build()
