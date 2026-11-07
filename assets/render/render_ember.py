# EMBER, the tube EQ's faceplate, rendered in Blender (Cycles).
# Oxblood hammertone enamel with walnut cheeks, a caged window of three glass valves (they glow with Drive), an
# amber-backlit VU, two transformer cans, black Bakelite knobs with chrome skirts, jewel pilot lamps over chrome
# push buttons, a round green CRT for the EQ curve, a brass nameplate.
# LD_HOT: 'tubes' (filaments and plates burning, glow spilling on the cage and enamel).
# Usage: python3 render_ember.py <out dir> [scale] [samples]
import bpy, math, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_skull as SK
from render_skull import mix, mrange, math_, noise, cyl, emissive
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
HOT = os.environ.get('LD_HOT', '')
FONTS = os.environ.get('LD_FONTS', '/tmp/claude-0/mk/fonts')
W, H = 1060, 544     # wider than the others so the EQ knobs' scales have room
R.W, R.H = W, H
at = R.at

# layout (panel px), shared with the overlay
VU = (205, 98, 230, 122)                         # centre, w, h of the meter window
TUBES = (530, 96, 330, 128)                      # centre, w, h of the valve window
CANS = [(808, 96, 44), (922, 96, 44)]
PLATE = (530, 196, 190, 34)                      # brass nameplate
KNOBS_A = [100, 220, 340, 480, 600, 720, 860, 980]; KYA = 310; KRA = 26
DRIVE = (120, 462, 30); OUTPUT = (940, 462, 30)
BTN = [(270, 476), (350, 476), (430, 476), (670, 476), (750, 476)]   # EQP, MID EQ, TUBE, NATURAL, LINEAR
JEWEL_DY = -34
CRT = (530, 466, 46)

def enamel():
    m = bpy.data.materials.new('enamel'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); vor = nt.nodes.new('ShaderNodeTexVoronoi'); vor.inputs['Scale'].default_value = 45
    nt.links.new(tc.outputs['Object'], vor.inputs['Vector'])
    ham = vor.outputs['Distance']                                   # hammertone dimples
    col = mix(nt, mrange(nt, ham, 0.0, 0.6), (0.085, 0.012, 0.01), (0.15, 0.022, 0.018))
    wear = mrange(nt, noise(nt, 3, 12, 0.7), 0.68, 0.71)
    col = mix(nt, wear, col, (0.4, 0.3, 0.2))
    nt.links.new(col, p.inputs['Base Color']); p.inputs['Roughness'].default_value = 0.4
    if 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = 0.35
    nt.links.new(wear, p.inputs['Metallic'])
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.35; bump.inputs['Distance'].default_value = 0.02
    nt.links.new(ham, bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def walnut():
    m = bpy.data.materials.new('walnut'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (8, 0.6, 1)
    wv = nt.nodes.new('ShaderNodeTexWave'); wv.wave_type = 'BANDS'; wv.bands_direction = 'X'; wv.inputs['Scale'].default_value = 6; wv.inputs['Distortion'].default_value = 9; wv.inputs['Detail'].default_value = 6
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], wv.inputs['Vector'])
    col = mix(nt, wv.outputs['Fac'], (0.07, 0.035, 0.018), (0.2, 0.1, 0.05))
    nt.links.new(col, p.inputs['Base Color']); p.inputs['Roughness'].default_value = 0.35
    if 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = 0.7
    return m

def image_mat(name, path, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); im = nt.nodes.new('ShaderNodeTexImage'); im.image = bpy.data.images.load(path)
    nt.links.new(tc.outputs['Generated'], im.inputs['Vector']); nt.links.new(im.outputs['Color'], p.inputs['Base Color'])
    p.inputs['Roughness'].default_value = rough
    if emit: nt.links.new(im.outputs['Color'], p.inputs['Emission Color']); p.inputs['Emission Strength'].default_value = emit
    return m

def window(plate, cx, cy, w, h, depth, r=0.06):
    c = P.rounded_box('win', w * U, h * U, 0.6, r, 0.01); c.location = at(cx, cy, 0.0); cut(plate, c)
    bpy.ops.mesh.primitive_plane_add(size=1); f = bpy.context.object; f.scale = (w * U, h * U, 1); f.location = at(cx, cy, -depth)
    return f

def bezel(cx, cy, w, h, t, mat, r=0.08, z=0.0):
    o = P.rounded_box('bez', (w + 2 * t) * U, (h + 2 * t) * U, 0.06, r + t * U, 0.01); o.location = at(cx, cy, z + 0.03)
    c = P.rounded_box('bzc', w * U, h * U, 0.5, r, 0.01); c.location = at(cx, cy, 0.0); cut(o, c)
    o.data.materials.clear(); o.data.materials.append(mat); return o

def valve(x, y, length, rad):
    """A glass valve lying in the window (axis along the panel's y): envelope, plates, mica, getter, filament, socket."""
    X, Y, _ = at(x, y); L = length * U; Rr = rad * U; zc = -0.02
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=Rr, depth=L, location=(X, Y, zc)); env = bpy.context.object
    env.rotation_euler = (math.pi / 2, 0, 0); bpy.ops.object.shade_smooth(); env.data.materials.append(GLASS)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=Rr, location=(X, Y + L / 2, zc)); cap = bpy.context.object
    cap.scale = (1, 0.8, 1); bpy.ops.object.shade_smooth(); cap.data.materials.append(GLASS)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=Rr * 0.75, location=(X, Y + L / 2 - Rr * 0.1, zc)); gt = bpy.context.object
    gt.scale = (1, 0.5, 1); gt.data.materials.append(GETTER)                                   # silver getter flash at the top
    for dx in (-0.45, 0.45):                                                                      # the two plate assemblies
        b = box('plate_v', x + dx * rad * 1.0 - rad * 0.28, y - length * 0.22, rad * 0.56, length * 0.5, Rr * 1.0, zc, 0.004, 0.003, PLATEM)
    for yy in (y - length * 0.27, y + length * 0.3):                                             # mica spacers
        box('mica', x - rad * 0.95, yy - 1.5, rad * 1.9, 3, Rr * 1.6, zc, 0.004, 0.002, MICA)
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.012, depth=L * 0.5, location=(X, Y - L * 0.02, zc + Rr * 0.2)); fil = bpy.context.object
    fil.rotation_euler = (math.pi / 2, 0, 0); fil.data.materials.append(FILAMENT)
    # the inner glow volume: a soft emissive core seen through the plates
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1, location=(X, Y - L * 0.02, zc)); gl = bpy.context.object
    gl.scale = (Rr * 0.5, L * 0.28, Rr * 0.3); gl.data.materials.append(CORE)
    # socket ring (bottom)
    cyl(X, Y - L / 2 - 0.02, Rr * 1.2, 0.08, zc, 64, BAKELITE, 0.01).rotation_euler = (math.pi / 2, 0, 0)

def can(px, py, r):
    x, y, _ = at(px, py); R_ = r * U
    c = cyl(x, y, R_, 0.3, 0.12, 128, CANM, R_ * 0.06)
    cyl(x, y, R_ * 1.06, 0.05, 0.0, 128, CHROME, 0.01)                                  # base flange
    bpy.ops.mesh.primitive_torus_add(major_radius=R_ * 0.98, minor_radius=0.018, major_segments=128, location=(x, y, 0.27)); t = bpy.context.object; t.data.materials.append(CHROME)
    cyl(x, y, R_ * 0.3, 0.03, 0.285, 64, CHROME, 0.006)                                  # centre boss
    for i in range(3):
        a = math.pi / 2 + i * 2 * math.pi / 3; SK.hexbolt(x + math.cos(a) * R_ * 0.7, y + math.sin(a) * R_ * 0.7, 0.028, 0.28)

def bakelite_knob(px, py, r):
    x, y, _ = at(px, py); R_ = r * U
    cyl(x, y, R_ * 1.32, 0.04, 0.01, 128, CHROME, R_ * 0.05)                            # chrome skirt
    body = cyl(x, y, R_, 0.24, 0.15, 128, None, 0.0)
    for i in range(12):
        a = 2 * math.pi * i / 12
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=R_ * 0.16, depth=0.5, location=(x + math.cos(a) * R_ * 1.06, y + math.sin(a) * R_ * 1.06, 0.15)); cut(body, bpy.context.object)
    b = body.modifiers.new('b', 'BEVEL'); b.width = R_ * 0.07; b.segments = 4; b.limit_method = 'ANGLE'; b.angle_limit = math.radians(40)
    bpy.ops.object.shade_smooth(); body.data.materials.clear(); body.data.materials.append(BAKELITE)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=32, radius=R_ * 0.78, location=(x, y, 0.27)); cp = bpy.context.object
    cp.scale = (1, 1, 0.14); bpy.ops.object.shade_smooth(); cp.data.materials.append(BAKELITE)

def jewel(px, py, mat):
    x, y, _ = at(px, py)
    cyl(x, y, 0.12, 0.05, 0.02, 64, CHROME, 0.01)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.085, location=(x, y, 0.06)); j = bpy.context.object
    j.scale = (1, 1, 0.75); j.data.materials.append(mat)

def push(px, py):
    x, y, _ = at(px, py)
    cyl(x, y, 0.2, 0.04, 0.02, 96, CHROME, 0.01)
    cyl(x, y, 0.15, 0.12, 0.08, 96, CHROME, 0.03)

def build():
    global GLASS, GETTER, PLATEM, MICA, FILAMENT, CORE, BAKELITE, CHROME, CANM
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    hot = HOT == 'tubes'
    CHROME = P.metal('chrome', (0.8, 0.8, 0.82), 0.1); SK.STEEL_DIRTY = CHROME
    BAKELITE = mat_principled('bakelite', (0.012, 0.01, 0.009), 0.28, coat=0.8)
    CANM = P.metal('can', (0.05, 0.05, 0.055), 0.4)
    GLASS = mat_principled('vglass', (0.95, 0.97, 0.96), 0.03, transmission=1.0); GLASS.node_tree.nodes['Principled BSDF'].inputs['IOR'].default_value = 1.4
    GETTER = P.metal('getter', (0.75, 0.74, 0.72), 0.05)
    PLATEM = P.metal('vplate', (0.18, 0.17, 0.16), 0.55)
    MICA = mat_principled('mica', (0.6, 0.52, 0.4), 0.4, sss=0.3)
    FILAMENT = emissive('fil', (1.0, 0.42, 0.08), 90.0 if hot else 0.6, base=(0.2, 0.1, 0.05))
    CORE = emissive('core', (1.0, 0.32, 0.05), 30.0 if hot else 0.0, base=(0, 0, 0)); CORE.node_tree.nodes['Principled BSDF'].inputs['Alpha'].default_value = 1.0
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.04, 0.012, enamel())
    # walnut cheeks
    for x0 in (0, W - 26): box('cheek', x0, 0, 26, H, 0.3, 0.0, 0.03, 0.01, walnut())
    # chrome trim lines
    for y0 in (12, H - 16): box('trim', 30, y0, W - 60, 4, 0.03, 0.01, 0.004, 0.002, CHROME)
    # VU: window with backlit printed face, chrome bezel, glass
    vx, vy, vw, vh = VU; f = window(plate, vx, vy, vw, vh, 0.05); f.data.materials.append(image_mat('vu', os.path.join(HERE, 'tex', 'ember_vu.png'), 0.6, 0.55))
    bezel(vx, vy, vw, vh, 8, CHROME)
    # valve window: dark chassis floor, three valves, the cage in front
    tx, ty, tw, th = TUBES; fl = window(plate, tx, ty, tw, th, 0.3)
    fl.data.materials.append(mat_principled('chassis', (0.02, 0.018, 0.016), 0.6))
    bezel(tx, ty, tw, th, 8, CHROME)
    for i, dx in enumerate((-100, 0, 100)): valve(tx + dx, ty + 4, 92, 22)
    for i in range(23):
        x = tx - tw / 2 + 8 + (tw - 16) * i / 22; X, Y, _ = at(x, ty)
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.008, depth=th * U, location=(X, Y, 0.06)); w_ = bpy.context.object; w_.rotation_euler = (math.pi / 2, 0, 0); w_.data.materials.append(CHROME)
    for yy in (ty - th / 2 + 10, ty + th / 2 - 10):
        X, Y, _ = at(tx, yy); bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.012, depth=tw * U, location=(X, Y, 0.07)); w_ = bpy.context.object; w_.rotation_euler = (0, math.pi / 2, 0); w_.data.materials.append(CHROME)
    if hot:
        for dx in (-100, 0, 100):
            l = bpy.data.lights.new('valve', 'POINT'); l.energy = 18; l.color = (1.0, 0.45, 0.12); l.shadow_soft_size = 0.2
            lo = bpy.data.objects.new('valve', l); s.collection.objects.link(lo); lo.location = (*at(tx + dx, ty)[:2], 0.05)
        l = bpy.data.lights.new('spill', 'AREA'); l.energy = 12; l.size = 3; l.color = (1.0, 0.5, 0.18)
        lo = bpy.data.objects.new('spill', l); s.collection.objects.link(lo); lo.location = (*at(tx, ty + 60)[:2], 0.8)
    # transformer cans, nameplate
    for (x, y, r) in CANS: can(x, y, r)
    nx, ny, nw, nh = PLATE
    box('name', nx - nw / 2, ny - nh / 2, nw, nh, 0.03, 0.015, 0.03, 0.006, P.metal('brass', (0.62, 0.42, 0.16), 0.24, brushed=True, scale=14))
    for (a, b) in ((nx - nw / 2 + 9, ny), (nx + nw / 2 - 9, ny)): SK.rivet(a, b, 0.03)
    bpy.ops.object.text_add(location=at(nx, ny + 1, 0.032)); tx_ = bpy.context.object
    tx_.data.body = 'Ember'; tx_.data.font = bpy.data.fonts.load(os.path.join(FONTS, 'CinzelDecorative-Bold.ttf'))
    tx_.data.size = 0.27; tx_.data.align_x = 'CENTER'; tx_.data.align_y = 'CENTER'; tx_.data.extrude = 0.006
    tx_.data.materials.append(mat_principled('blackfill', (0.02, 0.012, 0.008), 0.4, coat=0.5))
    # knobs, buttons, jewels, CRT
    for x in KNOBS_A: bakelite_knob(x, KYA, KRA)
    bakelite_knob(*DRIVE); bakelite_knob(*OUTPUT)
    JEWEL = mat_principled('jewel', (1.0, 0.45, 0.05), 0.05, transmission=0.8, coat=1.0)
    for (x, y) in BTN: push(x, y); jewel(x, y + JEWEL_DY, JEWEL)
    cx_, cy_, cr = CRT; x, y, _ = at(cx_, cy_)
    hole = cyl(x, y, cr * U, 0.6, 0.0, 128); cut(plate, hole)
    rg = cyl(x, y, cr * U * 1.22, 0.08, 0.03, 128, None, 0.0, smooth=False); hc = cyl(x, y, cr * U * 0.98, 0.5, 0.0, 128); cut(rg, hc)
    bb = rg.modifiers.new('b', 'BEVEL'); bb.width = 0.02; bb.segments = 4; bpy.ops.object.shade_smooth(); rg.data.materials.clear(); rg.data.materials.append(CANM)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=cr * U, location=(x, y, -0.06)); scr = bpy.context.object
    scr.scale = (1, 1, 0.18); bpy.ops.object.shade_smooth(); scr.data.materials.append(mat_principled('crt', (0.01, 0.03, 0.02), 0.08, coat=1.0))
    # lights: warm key, cool fill, rim
    P.area(s, 'key', (-5, 6, 7), 4, 620, (1.0, 0.93, 0.84))
    P.area(s, 'fill', (6, -4, 5), 6, 130, (0.7, 0.8, 1.0))
    P.area(s, 'rim', (0, 9, 1.4), 9, 240)
    P.area(s, 'kick', (6, 3, 2), 2, 180, (1.0, 0.9, 0.8))
    P.area(s, 'soft', (0, 3.2, 9), 7, 70, (0.95, 0.92, 0.9))
    name = 'ember' + ('_' + HOT if HOT else '') + '.png'
    s.render.filepath = os.path.join(OUTDIR, name); bpy.ops.render.render(write_still=True); print('rendered', name)

if __name__ == '__main__':
    build()
