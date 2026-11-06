# The mic header column, rendered in Blender (Cycles) in the rack's materials: a machined module per mic with a
# recessed glass name window, physical S and M buttons, knurled pan knob(s) in frosted acrylic rings (two on a
# stereo mic, as Pro Tools does), a fader slot milled into the plate with a real cap, and recessed meter windows.
# Names, values, LEDs, the knob arcs and meter bars are drawn live on top.
# Layout in pixels: the column is 236 wide; rows below a 42 px top bar, 24 px foot.
# Usage: python3 render_heads.py <out dir> [scale] [faders dB, comma separated] [stereo flags 0/1]
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
args = sys.argv[1:]
outdir = args[0] if args else '.'
scale = args[1] if len(args) > 1 else '2'
FDB = [float(v) for v in (args[2] if len(args) > 2 else '0,-2.5,-4,-6,-5.5,-8,-7').split(',')]
ST = [v == '1' for v in (args[3] if len(args) > 3 else '0,0,1,0,1,1,1').split(',')]
sys.argv = [sys.argv[0], outdir, scale, 'none']
import render_parts as P

W, TOP, FOOT = 236, 42, 24
H = 632
LH = (H - TOP - FOOT) / len(FDB)
U = 0.01
def at(px, py, z=0.0): return ((px - W / 2) * U, (H / 2 - py) * U, z)
def fpos(db): return max(0.0, min(1.0, (db + 60) / 66))

def mat(name, base, rough, metallic=0.0, coat=0.0, sss=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*base, 1); p.inputs['Roughness'].default_value = rough; p.inputs['Metallic'].default_value = metallic
    if coat and 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = coat
    if sss and 'Subsurface Weight' in p.inputs: p.inputs['Subsurface Weight'].default_value = sss
    return m

def box(name, px, py, w, h, d, z, r, bevel, m):
    o = P.rounded_box(name, w * U, h * U, d, r, bevel); o.location = at(px + w / 2, py + h / 2, z); o.data.materials.append(m); return o

def cut(target, cutter):
    md = target.modifiers.new('cut', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = cutter
    bpy.context.view_layer.objects.active = target; bpy.ops.object.modifier_apply(modifier='cut'); cutter.hide_render = True
    for f in target.data.polygons: f.material_index = 0

def well(plate, px, py, w, h, depth, r, floor):
    c = P.rounded_box('w', w * U, h * U, 0.5, r, 0.008); c.location = at(px + w / 2, py + h / 2, 0.07 + 0.25 - depth); cut(plate, c)
    bpy.ops.mesh.primitive_plane_add(size=1); f = bpy.context.object; f.scale = (w * U, h * U, 1); f.location = at(px + w / 2, py + h / 2, 0.07 - depth + 0.004)
    f.data.materials.append(floor)

def knob(px, py, r):
    x, y, _ = at(px, py); R = r * U
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=R * 1.45, depth=0.02, location=(x, y, 0.071)); bpy.context.object.data.materials.append(COLLAR)
    bpy.ops.mesh.primitive_torus_add(major_radius=R * 1.26, minor_radius=R * 0.11, major_segments=128, minor_segments=20, location=(x, y, 0.082))
    t = bpy.context.object; t.scale = (1, 1, 0.55); bpy.ops.object.shade_smooth(); t.data.materials.append(ACRYLIC)
    n = 72
    bpy.ops.mesh.primitive_cylinder_add(vertices=n, radius=R, depth=0.1, location=(x, y, 0.12)); sk = bpy.context.object
    for v in sk.data.vertices:
        a = math.atan2(v.co.y, v.co.x)
        if int(round((a + math.pi) / (2 * math.pi) * n)) % 2: v.co.x *= 0.95; v.co.y *= 0.95
    b = sk.modifiers.new('b', 'BEVEL'); b.width = R * 0.1; b.segments = 4; b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth(); sk.data.materials.append(BLACKMETAL)
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=R * 0.78, depth=0.02, location=(x, y, 0.172)); cp = bpy.context.object
    b = cp.modifiers.new('b', 'BEVEL'); b.width = R * 0.1; b.segments = 5; b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth(); cp.data.materials.append(SILVER)

def build():
    global COLLAR, ACRYLIC, BLACKMETAL, SILVER
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = 80
    t = math.radians(9)
    cam = P.camera(s, W, H, max(W, H) * U * math.cos(t))
    cam.location = (0, -10 * math.sin(t), 10 * math.cos(t)); cam.rotation_euler = (t, 0, 0)
    ANOD = P.metal('anod', (0.045, 0.046, 0.052), 0.3, brushed=True, scale=28)
    GLASS = mat('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    SLOT = mat('slot', (0.004, 0.004, 0.005), 0.6)
    COLLAR = mat('collar', (0.008, 0.008, 0.01), 0.5)
    ACRYLIC = mat('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    CAPM = P.metal('cap', (0.72, 0.73, 0.77), 0.2, brushed=True, scale=60)
    BUTTON = mat('button', (0.022, 0.023, 0.027), 0.34, coat=0.6)
    # the column behind: dark cavity; top bar and foot plates
    box('cav', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat('cav', (0.006, 0.006, 0.008), 0.7))
    box('topbar', 0, 0, W, TOP - 2, 0.12, -0.06, 0.01, 0.006, ANOD)
    box('foot', 0, H - FOOT + 2, W, FOOT - 2, 0.12, -0.06, 0.01, 0.006, ANOD)
    for i, db in enumerate(FDB):
        y = TOP + i * LH
        plate = box('mod%d' % i, 4, y + 2.5, W - 8, LH - 5, 0.14, 0.0, 0.05, 0.012, ANOD)
        well(plate, 16, y + 9, 134, 22, 0.05, 0.04, GLASS)                     # name window
        fh = LH - 16
        well(plate, 175, y + 14, 4, fh - 12, 0.08, 0.019, SLOT)               # fader slot
        for k, mx in enumerate([200, 208] if ST[i] else [200]):               # meters
            well(plate, mx, y + 14, 6, fh - 12, 0.05, 0.02, GLASS)
        box('s%d' % i, 16, y + 38, 20, 22, 0.07, 0.105, 0.035, 0.01, BUTTON)
        box('m%d' % i, 40, y + 38, 20, 22, 0.07, 0.105, 0.035, 0.01, BUTTON)
        if ST[i]: knob(77, y + 49, 8.5); knob(103, y + 49, 8.5)
        else: knob(82, y + 49, 9.5)
        cy = y + 8 + (1 - fpos(db)) * (fh - 12)
        box('cap%d' % i, 165, cy, 24, 12, 0.1, 0.12, 0.03, 0.012, CAPM)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 420)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    P.render(s, 'heads.png')


def big():
    # the big fader that pops out while a mini fader is held: same plate, slot and cap, larger
    global W, H
    W, H = 96, 316
    s = P.reset(); s.render.film_transparent = True; s.cycles.samples = 80
    t = math.radians(9)
    cam = P.camera(s, W, H, max(W, H) * U * math.cos(t)); cam.location = (0, -10 * math.sin(t), 10 * math.cos(t)); cam.rotation_euler = (t, 0, 0)
    ANOD = P.metal('anod', (0.05, 0.051, 0.058), 0.42, brushed=True, scale=28)
    GLASS = mat('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    SLOT = mat('slot', (0.004, 0.004, 0.005), 0.6)
    CAPM = P.metal('cap', (0.72, 0.73, 0.77), 0.2, brushed=True, scale=60)
    plate = box('plate', 2, 2, W - 4, H - 4, 0.16, 0.0, 0.12, 0.016, ANOD)
    for sx in (12, W - 12):
        for sy in (12, H - 12):
            x, y, _ = at(sx, sy)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.04, location=(x, y, 0.07)); h = bpy.context.object; h.scale = (1, 1, 0.42)
            bpy.ops.object.shade_smooth(); h.data.materials.append(P.metal('steel', (0.6, 0.61, 0.64), 0.26))
    if not os.environ.get('LD_NOCAP'): box('cap', 24, 88, 48, 28, 0.16, 0.16, 0.06, 0.02, CAPM)
    P.area(s, 'key', (-4, 6, 6), 5, 1300)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'kick', (5, 3, 2), 2, 200, (1.0, 0.9, 0.8))
    P.render(s, 'bigfader.png')

if os.environ.get('LD_NO_BUILD'): pass
elif os.environ.get('LD_BIG'): big()
else: build()
