# Renders the physical parts of the Limitless Drums interface with Blender (Cycles), as PNGs with alpha:
# the faceplate, a recessed tile well, machined button caps and screws. Everything that glows, moves or
# shows data is drawn live in code on top of these.
# Usage: python3 render_parts.py <out dir> [scale]      (needs the bpy module: pip install bpy)
import bpy, math, sys, os
from mathutils import Vector

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
SCALE = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
os.makedirs(OUT, exist_ok=True)

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.device = 'CPU'
    s.cycles.samples = 96
    s.cycles.use_denoising = True
    s.render.film_transparent = True
    s.render.image_settings.file_format = 'PNG'
    s.render.image_settings.color_mode = 'RGBA'
    s.view_settings.view_transform = 'AgX'
    s.view_settings.look = 'AgX - Medium High Contrast'
    w = bpy.data.worlds.new('w'); s.world = w; w.use_nodes = True
    bg = w.node_tree.nodes['Background']; bg.inputs[0].default_value = (0.012, 0.012, 0.016, 1); bg.inputs[1].default_value = 1.0
    return s

def camera(s, w_px, h_px, ortho_w):
    s.render.resolution_x = int(w_px * SCALE); s.render.resolution_y = int(h_px * SCALE)
    cam = bpy.data.cameras.new('cam'); cam.type = 'ORTHO'; cam.ortho_scale = ortho_w
    o = bpy.data.objects.new('cam', cam); s.collection.objects.link(o); s.camera = o
    o.location = (0, 0, 10); o.rotation_euler = (0, 0, 0)
    return o

def area(s, name, loc, size, energy, color=(1, 1, 1), rot=(0, 0, 0)):
    l = bpy.data.lights.new(name, 'AREA'); l.size = size; l.energy = energy; l.color = color
    o = bpy.data.objects.new(name, l); s.collection.objects.link(o); o.location = loc
    d = Vector((0, 0, 0)) - Vector(loc); o.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    return o

def studio(s, k=1.0):
    # a big soft key from the top left, a cool fill, a thin rim from behind: the studio light of the references
    area(s, 'key', (-6, 7, 9), 7, 900 * k)
    area(s, 'fill', (7, -3, 6), 6, 160 * k, (0.75, 0.85, 1.0))
    area(s, 'rim', (0, 9, 2.5), 10, 260 * k)

def metal(name, base, rough, aniso=0.0, brushed=False, scale=60.0, dark=False):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*base, 1)
    p.inputs['Metallic'].default_value = 1.0
    p.inputs['Roughness'].default_value = rough
    if 'Anisotropic' in p.inputs: p.inputs['Anisotropic'].default_value = aniso
    if brushed:
        # fine brushing: stretched noise into roughness and a whisper of bump
        tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping')
        mp.inputs['Scale'].default_value = (scale, scale * 0.02, 1)
        nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 40; nz.inputs['Detail'].default_value = 8
        ramp = nt.nodes.new('ShaderNodeMapRange'); ramp.inputs['To Min'].default_value = rough - 0.08; ramp.inputs['To Max'].default_value = rough + 0.08
        bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.035
        nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], nz.inputs['Vector'])
        nt.links.new(nz.outputs['Fac'], ramp.inputs['Value']); nt.links.new(ramp.outputs['Result'], p.inputs['Roughness'])
        nt.links.new(nz.outputs['Fac'], bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def radial_brushed(name, base, rough):
    # concentric machining rings on a knob face
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*base, 1); p.inputs['Metallic'].default_value = 1.0
    p.inputs['Roughness'].default_value = rough
    if 'Anisotropic' in p.inputs:
        p.inputs['Anisotropic'].default_value = 0.85
    tc = nt.nodes.new('ShaderNodeTexCoord')
    wv = nt.nodes.new('ShaderNodeTexWave'); wv.wave_type = 'RINGS'; wv.inputs['Scale'].default_value = 26; wv.inputs['Distortion'].default_value = 1.2; wv.inputs['Detail'].default_value = 4
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.06
    nt.links.new(tc.outputs['Object'], wv.inputs['Vector']); nt.links.new(wv.outputs['Fac'], bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def rounded_box(name, w, h, d, r, bevel_r, segs=10):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.object; o.name = name; o.scale = (w, h, d); bpy.ops.object.transform_apply(scale=True)
    b = o.modifiers.new('round', 'BEVEL'); b.width = r; b.segments = segs; b.affect = 'EDGES'
    b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='round')
    b2 = o.modifiers.new('edge', 'BEVEL'); b2.width = bevel_r; b2.segments = 6; b2.limit_method = 'ANGLE'
    bpy.ops.object.shade_smooth()
    return o

def render(s, name):
    s.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)
    print('rendered', name)

# ---------------------------------------------------------------------------- faceplate
def faceplate():
    # 1652 x 286 px panel, black anodised, brushed, bevelled edge
    s = reset(); W, H = 1652, 286; u = 16.52
    camera(s, W, H, u)
    o = rounded_box('plate', u - 0.06, H / 100 - 0.06, 0.3, 0.12, 0.025)
    o.location = (0, 0, -0.15)
    o.data.materials.append(metal('anod', (0.035, 0.036, 0.042), 0.38, brushed=True, scale=30))
    studio(s, 1.0)
    render(s, 'faceplate.png')

# ---------------------------------------------------------------------------- recessed well (one tile pocket)
def well(name, w_px, h_px):
    s = reset(); pad = 14
    W, H = w_px + 2 * pad, h_px + 2 * pad
    camera(s, W, H, W / 100)
    plate = rounded_box('plate', W / 100 + 0.4, H / 100 + 0.4, 0.3, 0.001, 0.001)
    plate.location = (0, 0, -0.15)
    cut = rounded_box('cut', w_px / 100, h_px / 100, 0.6, 0.1, 0.02)
    cut.location = (0, 0, 0.05)
    m = plate.modifiers.new('pocket', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cut
    bpy.context.view_layer.objects.active = plate; bpy.ops.object.modifier_apply(modifier='pocket')
    cut.hide_render = True
    plate.data.materials.append(metal('anod', (0.035, 0.036, 0.042), 0.38, brushed=True, scale=30))
    # the pocket floor: deep black glass
    bpy.ops.mesh.primitive_plane_add(size=1); fl = bpy.context.object; fl.scale = (w_px / 100, h_px / 100, 1); fl.location = (0, 0, -0.24)
    g = bpy.data.materials.new('glass'); g.use_nodes = True; gp = g.node_tree.nodes['Principled BSDF']
    gp.inputs['Base Color'].default_value = (0.004, 0.004, 0.006, 1); gp.inputs['Roughness'].default_value = 0.08
    if 'Coat Weight' in gp.inputs: gp.inputs['Coat Weight'].default_value = 1.0
    fl.data.materials.append(g)
    studio(s, 1.0)
    render(s, name)

# ---------------------------------------------------------------------------- machined button cap
def cap(name, px, ring=True):
    s = reset(); W = px + 20
    camera(s, W, W, W / 100)
    r = px / 200
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=r, depth=0.3, location=(0, 0, 0))
    c = bpy.context.object
    b = c.modifiers.new('b', 'BEVEL'); b.width = r * 0.12; b.segments = 8; b.limit_method = 'ANGLE'
    bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth()
    c.data.materials.append(radial_brushed('face', (0.10, 0.105, 0.115), 0.32))
    if ring:
        # a dark collar the coloured light ring sits in
        bpy.ops.mesh.primitive_torus_add(major_radius=r * 1.06, minor_radius=r * 0.06, major_segments=128, location=(0, 0, -0.12))
        t = bpy.context.object; bpy.ops.object.shade_smooth()
        t.data.materials.append(metal('collar', (0.02, 0.02, 0.025), 0.25))
    studio(s, 1.0)
    render(s, name)

# ---------------------------------------------------------------------------- screw
def screw():
    s = reset(); W = 28
    camera(s, W, W, W / 100)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=0.11, location=(0, 0, -0.06))
    h = bpy.context.object; h.scale = (1, 1, 0.45); bpy.ops.object.shade_smooth()
    h.data.materials.append(metal('steel', (0.55, 0.56, 0.6), 0.28))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.0)); sl = bpy.context.object; sl.scale = (0.2, 0.025, 0.1); sl.rotation_euler = (0, 0, math.radians(35))
    m = h.modifiers.new('slot', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = sl
    bpy.context.view_layer.objects.active = h; bpy.ops.object.modifier_apply(modifier='slot'); sl.hide_render = True
    studio(s, 1.0)
    render(s, 'screw.png')

what = sys.argv[3].split(',') if len(sys.argv) > 3 else ['faceplate', 'tile', 'auto', 'mode', 'cap', 'play', 'nav', 'screw', 'lcd', 'ab']
if 'faceplate' in what: faceplate()
if 'tile' in what: well('well_tile.png', 128, 142)
if 'auto' in what: well('well_auto.png', 206, 142)
if 'mode' in what: well('well_mode.png', 76, 62)
if 'lcd' in what: well('well_lcd.png', 286, 64)
if 'ab' in what: well('well_ab.png', 146, 44)
if 'cap' in what: cap('cap.png', 58)
if 'play' in what: cap('cap_play.png', 64)
if 'nav' in what: cap('cap_nav.png', 40, ring=False)
if 'screw' in what: screw()

# ---------------------------------------------------------------------------- the whole header, for the mockup
# Everything in panel pixels (1652 x 286): pockets cut into one plate so the light and shadows agree.
HEADER = {
    'size': (1652, 286),
    'pockets': [(272 + i * 138, 18, 128, 142, 12) for i in range(7)] + [(1244, 18, 214, 142, 12),
                (222, 200, 290, 64, 10), (528, 210, 150, 44, 10)] + [(694 + i * 84, 201, 76, 62, 9) for i in range(6)],
    'caps': [(1506, 62, 58, True), (1578, 62, 58, True), (52, 232, 40, False), (118, 232, 68, True), (184, 232, 40, False)],
    'groove': 177,
    'screws': [(14, 14), (1638, 14), (14, 272), (1638, 272)],
}

def header():
    s = reset(); s.cycles.samples = int(os.environ.get('LD_SAMPLES', '128'))
    W, H = HEADER['size']
    camera(s, W, H, W / 100)
    X = lambda x: (x - W / 2) / 100
    Y = lambda y: (H / 2 - y) / 100
    plate = rounded_box('plate', W / 100 - 0.04, H / 100 - 0.04, 0.3, 0.14, 0.03)
    plate.location = (0, 0, -0.15)
    cutters = []
    for (x, y, w, h, r) in HEADER['pockets']:
        c = rounded_box('cut', w / 100, h / 100, 0.6, r / 100, 0.015)
        c.location = (X(x + w / 2), Y(y + h / 2), 0.1)
        cutters.append(c)
    g = rounded_box('groove', W / 100 - 0.4, 0.03, 0.6, 0.01, 0.004); g.location = (0, Y(HEADER['groove']), 0.24); cutters.append(g)
    bpy.ops.object.select_all(action='DESELECT')
    for c in cutters: c.select_set(True)
    bpy.context.view_layer.objects.active = cutters[0]; bpy.ops.object.join()
    cut = bpy.context.object
    if not os.environ.get('LD_NOCUT'):
        m = plate.modifiers.new('pockets', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cut; m.solver = 'EXACT'
        bpy.context.view_layer.objects.active = plate; bpy.ops.object.modifier_apply(modifier='pockets')
        # the cut faces need clean normals, or the flat top shades like a curved one and catches every light
        bpy.ops.object.select_all(action='DESELECT'); plate.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.shade_smooth_by_angle(angle=0.6)
    cut.hide_render = True; cut.hide_viewport = True
    # black anodised: a dark dielectric with a satin coat, faintly brushed
    a = metal('anod', (0.017, 0.018, 0.022), 0.42, brushed=True, scale=30)
    pa = a.node_tree.nodes['Principled BSDF']; pa.inputs['Metallic'].default_value = 0.0
    if 'Specular IOR Level' in pa.inputs: pa.inputs['Specular IOR Level'].default_value = 0.6
    if 'Coat Weight' in pa.inputs: pa.inputs['Coat Weight'].default_value = 0.25; pa.inputs['Coat Roughness'].default_value = 0.35
    plate.data.materials.clear(); plate.data.materials.append(a)
    for f in plate.data.polygons: f.material_index = 0   # the cut faces came in with another slot index
    # pocket floors: deep black glass
    gl = bpy.data.materials.new('glass'); gl.use_nodes = True; gp = gl.node_tree.nodes['Principled BSDF']
    gp.inputs['Base Color'].default_value = (0.003, 0.003, 0.005, 1); gp.inputs['Roughness'].default_value = 0.12
    if 'Coat Weight' in gp.inputs: gp.inputs['Coat Weight'].default_value = 1.0; gp.inputs['Coat Roughness'].default_value = 0.05
    for (x, y, w, h, r) in HEADER['pockets']:
        bpy.ops.mesh.primitive_plane_add(size=1); fl = bpy.context.object
        fl.scale = (w / 100 - 0.02, h / 100 - 0.02, 1); fl.location = (X(x + w / 2), Y(y + h / 2), -0.195); fl.data.materials.append(gl)
    face = radial_brushed('face', (0.10, 0.105, 0.115), 0.32)
    collar = metal('collar', (0.015, 0.015, 0.02), 0.25)
    for (x, y, d, ring) in HEADER['caps']:
        rr = d / 200
        bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=rr, depth=0.22, location=(X(x), Y(y), 0.11))
        c = bpy.context.object; b = c.modifiers.new('b', 'BEVEL'); b.width = rr * 0.12; b.segments = 8; b.limit_method = 'ANGLE'
        bpy.ops.object.modifier_apply(modifier='b'); bpy.ops.object.shade_smooth(); c.data.materials.append(face)
        if ring:
            bpy.ops.mesh.primitive_torus_add(major_radius=rr * 1.1, minor_radius=rr * 0.07, major_segments=128, location=(X(x), Y(y), 0.0))
            t = bpy.context.object; bpy.ops.object.shade_smooth(); t.data.materials.append(collar)
    steel = metal('steel', (0.55, 0.56, 0.6), 0.28)
    for (x, y) in HEADER['screws']:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=0.055, location=(X(x), Y(y), 0.0))
        h = bpy.context.object; h.scale = (1, 1, 0.45); bpy.ops.object.shade_smooth(); h.data.materials.append(steel)
    studio(s, 1.5)
    render(s, 'header.png')

if 'header' in what: header()
