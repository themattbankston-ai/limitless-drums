# THE VISE, the compressor plugin's faceplate, rendered in Blender (Cycles).
# Worn hazard-yellow steel with oil grime, a hydraulic press in the middle (pillars, crossbeam, cylinder) over a
# recessed screen, a riveted pressure gauge for gain reduction, chunky industrial knobs, a warning plate, a beacon.
# Layers (LD_LAYER): '' / 'plate' (ram hidden from the camera), 'ram' (the ram head and rod alone: the app slides it
# down as it compresses). LD_HOT='beacon' lights the beacon (it flashes on heavy gain reduction).
# Usage: python3 render_vise.py <out dir> [scale] [samples]
import bpy, math, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_skull as SK
from render_skull import node, mix, mrange, math_, noise, cyl, emissive
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
HOT = os.environ.get('LD_HOT', ''); LAYER = os.environ.get('LD_LAYER', '')
FONTS = os.environ.get('LD_FONTS', '/tmp/claude-0/mk/fonts')
W, H = 980, 544
R.W, R.H = W, H; SK.W, SK.H = W, H
at = R.at
PXs = lambda v: W / 2 + (v - W / 2) / 1.01
PYs = lambda v: H / 2 + (v - H / 2) * 0.98163 / 1.01

# layout (panel px); the overlay code uses the same numbers
GAUGE = (160, 214, 104)
PRESS = dict(x0=330, x1=690, beam_y=(66, 112), cyl=(458, 522, 40, 128), screen=(360, 160, 300, 196))
RAM = dict(head=(384, 132, 252, 22), rod=(478, 24, 24, 112))
INPUT = (845, 300, 50)
BEACON = (368, 40, 20)
MODEL = (845, 128, 30)
TITLE = (40, 22)
WARN = (40, 362, 260, 52)

def paint_yellow():
    m = bpy.data.materials.new('yellow'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    patch = mrange(nt, noise(nt, 3.0, 12, 0.72), 0.64, 0.68)
    scr1 = mrange(nt, noise(nt, 2.0, 2, 0.5, 1, 60, 0.4), 0.74, 0.76)
    scr2 = mrange(nt, noise(nt, 2.4, 2, 0.5, 1, 70, -1.0), 0.75, 0.77)
    scrm = mrange(nt, noise(nt, 1.5, 3), 0.48, 0.6)
    wear = math_(nt, 'MAXIMUM', patch, math_(nt, 'MULTIPLY', math_(nt, 'MAXIMUM', scr1, scr2), scrm))
    paint = mix(nt, mrange(nt, noise(nt, 30, 8), 0.3, 0.7), (0.62, 0.36, 0.012), (0.72, 0.44, 0.02))
    oil = mrange(nt, noise(nt, 2.2, 10, 0.65), 0.55, 0.75)          # oily grime, darker toward patches
    paint = mix(nt, math_(nt, 'MULTIPLY', oil, 0.7), paint, (0.09, 0.06, 0.02))
    steel = mix(nt, mrange(nt, noise(nt, 30, 6), 0.3, 0.7), (0.22, 0.21, 0.2), (0.34, 0.33, 0.31))
    col = mix(nt, wear, paint, steel)
    nt.links.new(col, p.inputs['Base Color']); nt.links.new(wear, p.inputs['Metallic'])
    nt.links.new(mrange(nt, wear, 0, 1, 0.5, 0.32), p.inputs['Roughness'])
    if 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = 0.25
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.4; bump.inputs['Distance'].default_value = 0.02
    nt.links.new(math_(nt, 'SUBTRACT', math_(nt, 'MULTIPLY', noise(nt, 220, 4), 0.05), math_(nt, 'MULTIPLY', wear, 0.4)), bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def hazard():
    m = bpy.data.materials.new('hazard'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); wv = nt.nodes.new('ShaderNodeTexWave'); wv.wave_type = 'BANDS'; wv.bands_direction = 'DIAGONAL'
    wv.inputs['Scale'].default_value = 3.2; wv.wave_profile = 'SAW'; nt.links.new(tc.outputs['Object'], wv.inputs['Vector'])
    stripe = mrange(nt, wv.outputs['Fac'], 0.49, 0.51)
    wear = mrange(nt, noise(nt, 6, 10, 0.7), 0.62, 0.66)
    col = mix(nt, stripe, (0.012, 0.012, 0.012), (0.7, 0.42, 0.015))
    col = mix(nt, wear, col, (0.3, 0.29, 0.28))
    nt.links.new(col, p.inputs['Base Color']); nt.links.new(wear, p.inputs['Metallic']); p.inputs['Roughness'].default_value = 0.45
    return m

def image_mat(name, path, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); im = nt.nodes.new('ShaderNodeTexImage'); im.image = bpy.data.images.load(path)
    nt.links.new(tc.outputs['Generated'], im.inputs['Vector']); nt.links.new(im.outputs['Color'], p.inputs['Base Color'])
    p.inputs['Roughness'].default_value = rough
    if emit: nt.links.new(im.outputs['Color'], p.inputs['Emission Color']); p.inputs['Emission Strength'].default_value = emit
    return m

def bolt_row(x0, x1, y, n, r=0.04, z=0.1):
    for i in range(n):
        x = x0 + (x1 - x0) * i / max(1, n - 1); xx, yy, _ = at(x, y); SK.hexbolt(xx, yy, r, z)

def machine_knob(px, py, r):
    """Knurled aluminium skirt, black bakelite cap with a flat machined top."""
    x, y, _ = at(px, py); R_ = r * U
    cyl(x, y, R_ * 1.45, 0.03, 0.0, 128, SK.COLLAR, R_ * 0.05)
    n = 160
    bpy.ops.mesh.primitive_cylinder_add(vertices=n, radius=R_ * 1.08, depth=0.1, location=(x, y, 0.06)); sk = bpy.context.object
    for v in sk.data.vertices:
        a = math.atan2(v.co.y, v.co.x); k = int(round((a + math.pi) / (2 * math.pi) * n)) % 2
        if k: v.co.x *= 0.96; v.co.y *= 0.96
    b = sk.modifiers.new('b', 'BEVEL'); b.width = R_ * 0.06; b.segments = 3; b.limit_method = 'ANGLE'; bpy.ops.object.modifier_apply(modifier='b')
    bpy.ops.object.shade_smooth(); sk.data.materials.append(ALU)
    body = cyl(x, y, R_ * 0.9, 0.2, 0.2, 128, BAKELITE, R_ * 0.12)
    cyl(x, y, R_ * 0.62, 0.02, 0.305, 128, P.radial_brushed('kcap%d' % px, (0.5, 0.5, 0.52), 0.25), R_ * 0.02)

def lamp_key(px, py, w, h):
    """A chunky rubber key in a steel bezel."""
    box('kbz', px - 4, py - 4, w + 8, h + 8, 0.05, 0.0, 0.06, 0.008, STEEL)
    box('key', px, py, w, h, 0.08, 0.04, 0.05, 0.014, RUBBER)

def build():
    global ALU, BAKELITE, STEEL, RUBBER
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    SK.STEEL_DIRTY = P.metal('steeld', (0.36, 0.35, 0.33), 0.36, brushed=True, scale=20)
    SK.COLLAR = P.metal('collar', (0.05, 0.048, 0.046), 0.34)
    STEEL = P.metal('steel', (0.3, 0.3, 0.31), 0.3, brushed=True, scale=16)
    CHROME = P.metal('chrome', (0.8, 0.8, 0.82), 0.08)
    ALU = P.metal('alu', (0.62, 0.62, 0.64), 0.3)
    BAKELITE = mat_principled('bakelite', (0.012, 0.011, 0.011), 0.3, coat=0.6)
    RUBBER = mat_principled('rubber', (0.02, 0.02, 0.022), 0.62)
    GLASS = mat_principled('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    DARKSTEEL = P.metal('dsteel', (0.07, 0.07, 0.075), 0.35, brushed=True, scale=18)
    # the plate
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.06, 0.016, paint_yellow())
    # hazard band along the bottom
    HAZ = hazard()
    box('hazard', 6, H - 30, W - 12, 20, 0.02, 0.0, 0.004, 0.003, HAZ)
    for xx in range(30, W - 10, 60):
        SK.rivet(xx, 10)
    # GAUGE: chrome bezel ring with bolts, printed dial, glass
    gx, gy, gr = GAUGE; x, y, _ = at(gx, gy); R_ = gr * U
    cc = cyl(x, y, R_ * 1.0, 0.4, 0.0, 160); cut(plate, cc)
    ring = cyl(x, y, R_ * 1.2, 0.14, 0.04, 160, None, 0.0, smooth=False); cc = cyl(x, y, R_ * 0.99, 0.5, 0.0, 160); cut(ring, cc)
    b = ring.modifiers.new('b', 'BEVEL'); b.width = 0.03; b.segments = 5; b.limit_method = 'ANGLE'; bpy.ops.object.shade_smooth()
    ring.data.materials.clear(); ring.data.materials.append(CHROME)
    for i in range(6):
        a = 2 * math.pi * i / 6 + math.pi / 6; SK.hexbolt(x + math.cos(a) * R_ * 1.1, y + math.sin(a) * R_ * 1.1, 0.045, 0.12)
    bpy.ops.mesh.primitive_circle_add(vertices=160, radius=R_ * 0.99, fill_type='NGON', location=(x, y, -0.03)); f = bpy.context.object
    f.data.materials.append(image_mat('gauge', os.path.join(HERE, 'tex', 'vise_gauge.png'), 0.35, 0.08))
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=R_ * 0.99, location=(x, y, -0.03)); gd = bpy.context.object
    gd.scale = (1, 1, 0.12); bpy.ops.object.shade_smooth()
    dm = mat_principled('dome', (1, 1, 1), 0.02, transmission=1.0); dm.node_tree.nodes['Principled BSDF'].inputs['IOR'].default_value = 1.12; gd.data.materials.append(dm)
    # PRESS: pillars, crossbeam, hydraulic cylinder, the recessed screen
    x0, x1 = PRESS['x0'], PRESS['x1']; by0, by1 = PRESS['beam_y']
    for px_ in (x0, x1 - 26):
        box('pillar', px_, by0, 26, 300, 0.22, 0.11, 0.03, 0.01, DARKSTEEL)
        for yy in (150, 230, 310, 352): xx, y_, _ = at(px_ + 13, yy); SK.hexbolt(xx, y_, 0.04, 0.23)
    box('beam', x0 - 10, by0, x1 - x0 + 20, by1 - by0, 0.26, 0.13, 0.03, 0.012, DARKSTEEL)
    bolt_row(x0 + 16, x0 + 100, (by0 + by1) / 2, 3, 0.045, 0.27); bolt_row(x1 - 100, x1 - 16, (by0 + by1) / 2, 3, 0.045, 0.27)
    cx0, cx1, cy0, cy1 = PRESS['cyl']; x, y, _ = at((cx0 + cx1) / 2, (cy0 + cy1) / 2)
    cy = box('cyl', cx0, cy0, cx1 - cx0, cy1 - cy0, 0.36, 0.2, 0.3, 0.02, STEEL)   # rounded body reads as a cylinder
    for yy in (cy0 + 10, cy1 - 10): box('band', cx0 - 4, yy - 5, cx1 - cx0 + 8, 10, 0.4, 0.2, 0.05, 0.01, CHROME)
    # hydraulic line from the cylinder off to the right, with a fitting
    bpy.ops.curve.primitive_bezier_curve_add(); hose = bpy.context.object
    sp = hose.data.splines[0]; p0, p1 = sp.bezier_points
    p0.co = (*at(cx1, cy0 + 30)[:2], 0.32); p0.handle_left = p0.co; p0.handle_right = (*at(cx1 + 90, cy0 + 20)[:2], 0.4)
    p1.co = (*at(x1 + 8, by0 + 8)[:2], 0.2); p1.handle_right = p1.co; p1.handle_left = (*at(x1 - 40, by0 - 30)[:2], 0.4)
    hose.data.bevel_depth = 0.04; hose.data.bevel_resolution = 6; hose.data.materials.append(RUBBER)
    sx0, sy0, sw, sh = PRESS['screen']
    win = P.rounded_box('win', sw * U, sh * U, 0.5, 0.05, 0.01); win.location = at(sx0 + sw / 2, sy0 + sh / 2, 0.0); cut(plate, win)
    bpy.ops.mesh.primitive_plane_add(size=1); fl = bpy.context.object; fl.scale = (sw * U, sh * U, 1); fl.location = at(sx0 + sw / 2, sy0 + sh / 2, -0.07); fl.data.materials.append(GLASS)
    # the base block under the screen (the anvil)
    box('anvil', x0 + 6, 362, x1 - x0 - 12, 18, 0.2, 0.1, 0.02, 0.01, DARKSTEEL)
    # RAM: head plate with ribs, rod up into the cylinder
    hx, hy, hw, hh = RAM['head']; rx, ry, rw, rh = RAM['rod']
    rod = box('ram_rod', rx, ry, rw, rh, 0.2, 0.17, 0.1, 0.02, CHROME)
    head = box('ram_head', hx, hy, hw, hh, 0.2, 0.2, 0.02, 0.01, HAZ)
    for i in range(7):
        box('ram_rib', hx + 18 + i * 36, hy - 8, 8, 8, 0.14, 0.2, 0.01, 0.004, DARKSTEEL)
    box('ram_face', hx + 2, hy + hh - 5, hw - 4, 5, 0.22, 0.2, 0.004, 0.003, CHROME)
    # INPUT knob, other knobs, ratio
    machine_knob(INPUT[0], INPUT[1], INPUT[2])
    for x_ in (82, 194, 306, 676, 788, 904): machine_knob(x_, 470, 23)
    # model selector
    machine_knob(*MODEL)
    # beacon: base, amber dome, wire cage
    bx_, by_, br = BEACON; x, y, _ = at(bx_, by_); R_ = br * U
    cyl(x, y, R_ * 1.25, 0.08, 0.3, 96, BAKELITE, 0.01)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=R_, location=(x, y, 0.34)); dmb = bpy.context.object
    dmb.scale = (1, 1, 0.9); bpy.ops.object.shade_smooth()
    dmb.data.materials.append(emissive('beacon', (1.0, 0.45, 0.02), 14.0 if HOT == 'beacon' else 0.0, base=(0.85, 0.32, 0.01), rough=0.15, coat=1.0, sss=0.5))
    for i in range(4):
        a = i * math.pi / 4
        bpy.ops.mesh.primitive_torus_add(major_radius=R_ * 1.05, minor_radius=0.008, major_segments=96, location=(x, y, 0.34)); tr = bpy.context.object
        tr.rotation_euler = (math.pi / 2, 0, a); tr.scale = (1, 0.9, 1); tr.data.materials.append(STEEL)
    if HOT == 'beacon':
        l = bpy.data.lights.new('bl', 'POINT'); l.energy = 30; l.color = (1.0, 0.5, 0.05); lo = bpy.data.objects.new('bl', l); s.collection.objects.link(lo); lo.location = (x, y, 0.6)
    # warning plate
    wx, wy, ww, wh = WARN
    box('warnp', wx, wy, ww, wh, 0.012, 0.005, 0.02, 0.003, image_mat('warn', os.path.join(HERE, 'tex', 'vise_warning.png'), 0.55))
    for (a_, b_) in ((wx + 8, wy + 8), (wx + ww - 8, wy + 8), (wx + 8, wy + wh - 8), (wx + ww - 8, wy + wh - 8)): SK.rivet(a_, b_, 0.03)
    # transfer curve screen
    bpy.ops.mesh.primitive_plane_add(size=1); tsc = bpy.context.object
    win = P.rounded_box('win2', 118 * U, 66 * U, 0.5, 0.04, 0.008); win.location = at(490, 456, 0.0); cut(plate, win)
    tsc.scale = (118 * U, 66 * U, 1); tsc.location = at(490, 456, -0.06); tsc.data.materials.append(GLASS)
    # title: cast black steel letters
    bpy.ops.object.text_add(location=at(TITLE[0], TITLE[1] + 30, 0.02)); tx = bpy.context.object
    tx.data.body = 'THE VISE'; tx.data.font = bpy.data.fonts.load(os.path.join(FONTS, 'BlackOpsOne-Regular.ttf'))
    tx.data.size = 0.5; tx.data.align_x = 'LEFT'; tx.data.align_y = 'CENTER'; tx.data.extrude = 0.03; tx.data.bevel_depth = 0.005; tx.data.bevel_resolution = 3
    tx.data.materials.append(mat_principled('castblack', (0.015, 0.014, 0.014), 0.35, coat=0.3))
    # lights: cold factory key, warm fill, a rim
    P.area(s, 'key', (-5, 6, 7), 4, 650, (0.95, 0.97, 1.0))
    P.area(s, 'fill', (6, -4, 5), 6, 150, (1.0, 0.85, 0.65))
    P.area(s, 'rim', (0, 9, 1.4), 9, 260)
    P.area(s, 'kick', (6, 3, 2), 2, 200, (0.9, 0.95, 1.0))
    P.area(s, 'soft', (0, 3.2, 9), 7, 70, (0.9, 0.92, 1.0))
    ram_objs = [o for o in s.objects if o.name.startswith('ram_')]
    if LAYER == 'plate':
        for o in ram_objs: o.visible_camera = False; o.visible_shadow = False
    elif LAYER == 'ram':
        s.render.film_transparent = True
        for o in s.objects:
            if o.type in ('MESH', 'CURVE', 'FONT') and o not in ram_objs: o.visible_camera = False
    name = 'vise' + ('_' + LAYER if LAYER else '') + ('_' + HOT if HOT else '') + '.png'
    s.render.filepath = os.path.join(OUTDIR, name); bpy.ops.render.render(write_still=True); print('rendered', name)

if __name__ == '__main__':
    build()
