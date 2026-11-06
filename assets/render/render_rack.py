# The right-hand rack, rendered in Blender (Cycles): rack cavity, three modules with ears and screws, glass
# screens, knurled knobs with silver caps sitting in frosted acrylic rings, physical buttons and silicone pads.
# Everything that lights up (the knob rings, the pad backlight, LEDs, the screens) is drawn live in code on top,
# so the light can follow the values. Layout in panel pixels (318 x 664), origin top left.
# Usage: python3 render_rack.py <out dir> [scale]
import bpy, math, sys, os
from mathutils import Vector
sys.path.insert(0, os.path.dirname(__file__))
sys.argv = [sys.argv[0], sys.argv[1] if len(sys.argv) > 1 else '.', sys.argv[2] if len(sys.argv) > 2 else '2', 'none']
import render_parts as P

W, H = 318, 664
MIX = bool(os.environ.get('LD_MIX'))
U = 0.01   # blender units per px

def at(px, py, z=0.0): return ((px - W / 2) * U, (H / 2 - py) * U, z)

LEVEL = dict(y=6, h=338)
PADS = dict(y=350, h=190)
OUT_ = dict(y=546, h=112)
SCREEN = (26, 44, 266, 132)
BUTTONS = [(26, 190, 60, 30), (90, 190, 60, 30), (166, 190, 60, 30), (230, 190, 60, 30)]
KNOBS = [(59, 276), (126, 276), (193, 276), (260, 276)]
KNOB_R = 21
PADS_AT = [(30 + (i % 3) * 93, 384 + (i // 3) * 72, 80, 60) for i in range(6)]
METERS = [(26, 586, 126, 26), (166, 586, 126, 26)]

def mat_principled(name, base, rough, metallic=0.0, coat=0.0, sss=0.0, transmission=0.0, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*base, 1); p.inputs['Roughness'].default_value = rough; p.inputs['Metallic'].default_value = metallic
    if coat and 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = coat
    if sss and 'Subsurface Weight' in p.inputs: p.inputs['Subsurface Weight'].default_value = sss
    if transmission and 'Transmission Weight' in p.inputs: p.inputs['Transmission Weight'].default_value = transmission
    return m

def box(name, px, py, w, h, d, z, r, bevel, mat):
    o = P.rounded_box(name, w * U, h * U, d, r, bevel)
    o.location = at(px + w / 2, py + h / 2, z)
    o.data.materials.append(mat)
    return o

def cut(target, cutter):
    m = target.modifiers.new('cut', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cutter
    bpy.context.view_layer.objects.active = target; bpy.ops.object.modifier_apply(modifier='cut')
    cutter.hide_render = True
    for f in target.data.polygons: f.material_index = 0

def screw(px, py):
    x, y, _ = at(px, py)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=0.045, location=(x, y, 0.02))
    h = bpy.context.object; h.scale = (1, 1, 0.42); bpy.ops.object.shade_smooth()
    h.data.materials.append(STEEL)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.045)); sl = bpy.context.object; sl.scale = (0.075, 0.011, 0.05); sl.rotation_euler = (0, 0, math.radians(35))
    cut(h, sl)

def module(name, y, h):
    plate = box(name, 4, y, 310, h, 0.14, -0.07, 0.04, 0.012, ANOD)
    # ear slots (rack mounting), left and right
    for ex in (12, 306):
        for sy in (y + 18, y + h - 18):
            bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.032, depth=0.5, location=at(ex, sy, 0))
            c = bpy.context.object; c.scale = (1, 1.9, 1); cut(plate, c)
            screw(ex, sy)
    # thin engraved line inside the ears
    for gx in (21, 297):
        bpy.ops.mesh.primitive_cube_add(size=1, location=at(gx, y + h / 2, 0.0)); g = bpy.context.object
        g.scale = (0.012, (h - 16) * U, 0.03); cut(plate, g)
    return plate

def window(plate, px, py, w, h, depth=0.09):
    c = P.rounded_box('win', w * U, h * U, 0.5, 0.05, 0.012); c.location = at(px + w / 2, py + h / 2, 0.05 + 0.25 - depth)
    cut(plate, c)
    bpy.ops.mesh.primitive_plane_add(size=1); fl = bpy.context.object; fl.scale = (w * U, h * U, 1); fl.location = at(px + w / 2, py + h / 2, -depth + 0.005)
    fl.data.materials.append(GLASS)

def knob(px, py, r):
    x, y, _ = at(px, py)
    R = r * U
    # recessed collar in the plate the acrylic ring sits in
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=R * 1.42, depth=0.02, location=(x, y, 0.001))
    col = bpy.context.object; col.data.materials.append(COLLAR); bpy.ops.object.shade_smooth()
    # frosted acrylic light ring (code tints it with the value arc)
    bpy.ops.mesh.primitive_torus_add(major_radius=R * 1.24, minor_radius=R * 0.1, major_segments=160, minor_segments=24, location=(x, y, 0.016))
    t = bpy.context.object; t.scale = (1, 1, 0.55); bpy.ops.object.shade_smooth(); t.data.materials.append(ACRYLIC)
    # knurled skirt
    n = 144
    bpy.ops.mesh.primitive_cylinder_add(vertices=n, radius=R, depth=0.16, location=(x, y, 0.09))
    sk = bpy.context.object
    for v in sk.data.vertices:
        a = math.atan2(v.co.y, v.co.x); k = int(round((a + math.pi) / (2 * math.pi) * n)) % 2
        if k: v.co.x *= 0.955; v.co.y *= 0.955
    b = sk.modifiers.new('b', 'BEVEL'); b.width = R * 0.08; b.segments = 5; b.limit_method = 'ANGLE'; b.angle_limit = math.radians(50)
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth()
    sk.data.materials.append(BLACKMETAL)
    # silver cap, slightly domed, concentric machining
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=R * 0.8, depth=0.03, location=(x, y, 0.18))
    cp = bpy.context.object
    b = cp.modifiers.new('b', 'BEVEL'); b.width = R * 0.1; b.segments = 6; b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth()
    cp.data.materials.append(SILVER)

def button(px, py, w, h):
    o = box('btn', px, py, w, h, 0.07, 0.035, 0.05, 0.012, BUTTON)

def pad(px, py, w, h):
    o = box('pad', px, py, w, h, 0.09, 0.03, 0.07, 0.025, SILICONE)

def build():
    global ANOD, STEEL, GLASS, COLLAR, ACRYLIC, BLACKMETAL, SILVER, BUTTON, SILICONE
    s = P.reset()
    s.render.film_transparent = False
    s.cycles.samples = 80
    cam = P.camera(s, W, H, max(W, H) * U * 1.01)
    t = math.radians(11)   # looking up a little: the sides of knobs, buttons and pads show
    cam.location = (0, -10 * math.sin(t), 10 * math.cos(t)); cam.rotation_euler = (t, 0, 0)
    ANOD = P.metal('anod', (0.045, 0.046, 0.052), 0.3, brushed=True, scale=28)
    STEEL = P.metal('steel', (0.6, 0.61, 0.64), 0.26)
    GLASS = mat_principled('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    COLLAR = mat_principled('collar', (0.008, 0.008, 0.01), 0.5)
    ACRYLIC = mat_principled('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    BUTTON = mat_principled('button', (0.022, 0.023, 0.027), 0.34, coat=0.6)
    SILICONE = mat_principled('silicone', (0.018, 0.019, 0.023), 0.45, sss=0.3, coat=0.5)
    # the rack cavity behind the modules, with the two rails
    cav = box('cavity', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    for rx in (2, W - 16):
        box('rail', rx, 0, 14, H, 0.05, -0.15, 0.002, 0.006, P.metal('rail', (0.08, 0.08, 0.09), 0.4, brushed=True, scale=10))
    lv = module('level', LEVEL['y'], LEVEL['h'])
    pd = module('pads', PADS['y'], PADS['h'])
    ou = module('out', OUT_['y'], OUT_['h'])
    for m in METERS: window(ou, *m, depth=0.06)
    if MIX:
        # TONE: a screen, five knobs, two buttons; SHAPE: an envelope strip and four knobs
        window(lv, 26, 44, 266, 112)
        for b in [(26, 300, 60, 28), (90, 300, 60, 28), (232, 300, 60, 28)]: button(*b)
        for x in (47, 103, 159, 215, 271): knob(x, 222, 17)
        window(pd, 26, 372, 266, 40)
        for x in (59, 126, 193, 260): knob(x, 470, 18)
    else:
        window(lv, *SCREEN)
        for b in BUTTONS: button(*b)
        for k in KNOBS: knob(*k, KNOB_R)
        for p in PADS_AT: pad(*p)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    P.render(s, 'rack_mix.png' if MIX else 'rack.png')

build()
