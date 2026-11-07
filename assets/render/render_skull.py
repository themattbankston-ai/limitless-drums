# SKULL CUTTER, the saturation plugin's faceplate, rendered in Blender (Cycles).
# Battered black powder coat with worn steel and paint cracks, a cast bone skull on a saw blade, a riveted VU
# porthole, fluted bone knobs, a red PUNISH dome. Everything that moves (needle, pointers, values) is drawn live.
# Variants (LD_HOT): '' cold, 'eyes' (sockets burning, light spilling), 'cracks' (cracks and PUNISH glowing).
# The app crossfades the hot renders in by Drive and Punish.
# Usage: python3 render_skull.py <out dir> [scale] [samples]
import bpy, math, sys, os
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
HOT = os.environ.get('LD_HOT', '')
FONTS = os.environ.get('LD_FONTS', '/tmp/claude-0/mk/fonts')
W, H = 980, 544
R.W, R.H = W, H
at = R.at

# ------------------------------------------------------------------ materials
def node(nt, kind, **kw):
    n = nt.nodes.new(kind)
    for k, v in kw.items(): setattr(n, k, v)
    return n
def mix(nt, fac, a, b):
    m = nt.nodes.new('ShaderNodeMix'); m.data_type = 'RGBA'
    for inp, src in (('Factor_Float', fac), ('A_Color', a), ('B_Color', b)):
        sock = next(i for i in m.inputs if i.identifier == inp)
        if isinstance(src, (tuple, list)): sock.default_value = (*src, 1) if len(src) == 3 else src
        elif isinstance(src, (int, float)): sock.default_value = src
        else: nt.links.new(src, sock)
    return next(o for o in m.outputs if o.identifier == 'Result_Color')
def mrange(nt, src, a, b, c=0.0, d=1.0):
    m = nt.nodes.new('ShaderNodeMapRange'); m.inputs['From Min'].default_value = a; m.inputs['From Max'].default_value = b
    m.inputs['To Min'].default_value = c; m.inputs['To Max'].default_value = d; m.clamp = True; nt.links.new(src, m.inputs['Value'])
    return m.outputs['Result']
def math_(nt, op, a, b):
    m = nt.nodes.new('ShaderNodeMath'); m.operation = op
    for i, s in enumerate((a, b)):
        if isinstance(s, (int, float)): m.inputs[i].default_value = s
        else: nt.links.new(s, m.inputs[i])
    return m.outputs[0]
def noise(nt, scale, detail=6, rough=0.6, sx=1, sy=1, rot=0.0, w=None):
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping')
    mp.inputs['Scale'].default_value = (sx, sy, 1); mp.inputs['Rotation'].default_value = (0, 0, rot)
    nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = scale; nz.inputs['Detail'].default_value = detail; nz.inputs['Roughness'].default_value = rough
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], nz.inputs['Vector'])
    return nz.outputs['Fac']

def powdercoat():
    m = bpy.data.materials.new('powder'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    # wear: patches + two sets of scratches; cracks: voronoi edges in patches
    patch = mrange(nt, noise(nt, 3.2, 12, 0.72), 0.665, 0.69)
    scr1 = mrange(nt, noise(nt, 2.0, 2, 0.5, 1, 60, 0.5), 0.74, 0.76)
    scr2 = mrange(nt, noise(nt, 2.3, 2, 0.5, 1, 70, -0.9), 0.75, 0.77)
    scrm = mrange(nt, noise(nt, 1.5, 3), 0.5, 0.62)  # scratches only in some areas
    wear = math_(nt, 'MAXIMUM', patch, math_(nt, 'MULTIPLY', math_(nt, 'MAXIMUM', scr1, scr2), scrm))
    tc = nt.nodes.new('ShaderNodeTexCoord'); vor = nt.nodes.new('ShaderNodeTexVoronoi'); vor.feature = 'DISTANCE_TO_EDGE'; vor.inputs['Scale'].default_value = 7.0
    vmp = nt.nodes.new('ShaderNodeMapping'); vmp.inputs['Scale'].default_value = (1, 1.3, 1)
    nt.links.new(tc.outputs['Object'], vmp.inputs['Vector'])
    warp = noise(nt, 4, 4)  # wobble the crack lines
    add = nt.nodes.new('ShaderNodeVectorMath'); add.operation = 'ADD'; nt.links.new(vmp.outputs['Vector'], add.inputs[0])
    sc = nt.nodes.new('ShaderNodeCombineXYZ'); nt.links.new(math_(nt, 'MULTIPLY', warp, 0.12), sc.inputs[0]); nt.links.new(math_(nt, 'MULTIPLY', warp, 0.09), sc.inputs[1]); nt.links.new(sc.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], vor.inputs['Vector'])
    crack_line = mrange(nt, vor.outputs['Distance'], 0.0, 0.024, 1.0, 0.0)
    # cracks spread out from behind the saw, plus a few patches toward the corners
    dist = nt.nodes.new('ShaderNodeVectorMath'); dist.operation = 'DISTANCE'; nt.links.new(tc.outputs['Object'], dist.inputs[0])
    dist.inputs[1].default_value = (0, 0.6, 0)
    radial = mrange(nt, math_(nt, 'ADD', dist.outputs['Value'], math_(nt, 'MULTIPLY', warp, 0.9)), 1.75, 2.45, 1.0, 0.0)
    corners = mrange(nt, noise(nt, 1.1, 3, 0.5), 0.6, 0.66)
    region = math_(nt, 'MAXIMUM', math_(nt, 'MULTIPLY', radial, mrange(nt, noise(nt, 2.2, 3), 0.4, 0.5)), corners)
    crack = math_(nt, 'MULTIPLY', crack_line, region)
    paint = mix(nt, mrange(nt, noise(nt, 40, 8), 0.3, 0.7), (0.016, 0.015, 0.015), (0.03, 0.027, 0.025))
    steel = mix(nt, mrange(nt, noise(nt, 30, 6), 0.3, 0.7), (0.2, 0.19, 0.18), (0.32, 0.3, 0.28))
    rust = mrange(nt, noise(nt, 9, 6), 0.62, 0.7)
    steel = mix(nt, math_(nt, 'MULTIPLY', rust, 0.55), steel, (0.24, 0.09, 0.03))
    col = mix(nt, wear, paint, steel)
    col = mix(nt, crack, col, (0.004, 0.002, 0.002))
    nt.links.new(col, p.inputs['Base Color'])
    nt.links.new(math_(nt, 'MULTIPLY', wear, math_(nt, 'SUBTRACT', 1.0, crack)), p.inputs['Metallic'])
    nt.links.new(mrange(nt, wear, 0, 1, 0.62, 0.34), p.inputs['Roughness'])
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.45; bump.inputs['Distance'].default_value = 0.02
    h = math_(nt, 'SUBTRACT', math_(nt, 'MULTIPLY', noise(nt, 260, 4), 0.06), math_(nt, 'ADD', crack, math_(nt, 'MULTIPLY', wear, 0.35)))
    nt.links.new(h, bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    if HOT == 'cracks':
        glow = math_(nt, 'POWER', crack, 2.2)
        nt.links.new(mix(nt, 0.0, (1.0, 0.1, 0.02), (1, 1, 1)), p.inputs['Emission Color'])
        nt.links.new(math_(nt, 'MULTIPLY', glow, 9.0), p.inputs['Emission Strength'])
    return m

def bone(name='bone', base=(0.78, 0.68, 0.5), grime=(0.07, 0.045, 0.025), rough=0.42):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    ao = nt.nodes.new('ShaderNodeAmbientOcclusion'); ao.inputs['Distance'].default_value = 0.08; ao.samples = 16
    mott = mix(nt, mrange(nt, noise(nt, 14, 8), 0.35, 0.7), base, tuple(c * 0.78 for c in base))
    col = mix(nt, mrange(nt, ao.outputs['AO'], 0.35, 0.95, 1, 0), mott, grime)
    stain = mrange(nt, noise(nt, 5, 6), 0.58, 0.7)
    col = mix(nt, math_(nt, 'MULTIPLY', stain, 0.6), col, (0.32, 0.2, 0.09))
    nt.links.new(col, p.inputs['Base Color'])
    p.inputs['Roughness'].default_value = rough
    if 'Coat Weight' in p.inputs: p.inputs['Coat Weight'].default_value = 0.2
    if 'Subsurface Weight' in p.inputs: p.inputs['Subsurface Weight'].default_value = 0.08
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.25
    nt.links.new(noise(nt, 90, 8), bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def emissive(name, col, strength, base=None, rough=0.2, sss=0.0, coat=0.0):
    m = mat_principled(name, base or tuple(c * 0.2 for c in col), rough, coat=coat, sss=sss)
    p = m.node_tree.nodes['Principled BSDF']; p.inputs['Emission Color'].default_value = (*col, 1); p.inputs['Emission Strength'].default_value = strength
    return m

# ------------------------------------------------------------------ parts
def cyl(x, y, r, depth, z, verts=96, mat=None, bevel=0.0, smooth=True):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=(x, y, z)); o = bpy.context.object
    if bevel:
        b = o.modifiers.new('b', 'BEVEL'); b.width = bevel; b.segments = 4; b.limit_method = 'ANGLE'; bpy.ops.object.modifier_apply(modifier='b')
    if smooth: bpy.ops.object.shade_smooth()
    if mat: o.data.materials.append(mat)
    return o

def rivet(px, py, r=0.05):
    x, y, _ = at(px, py)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=r, location=(x, y, 0.012)); h = bpy.context.object
    h.scale = (1, 1, 0.5); bpy.ops.object.shade_smooth(); h.data.materials.append(STEEL_DIRTY)

def hexbolt(x, y, r, z):
    o = cyl(x, y, r, 0.05, z, verts=6, mat=STEEL_DIRTY, bevel=r * 0.12, smooth=False)
    o.rotation_euler[2] = 0.3
    cyl(x, y, r * 0.55, 0.02, z + 0.03, verts=32, mat=STEEL_DIRTY)

def fluted_knob(px, py, r, dark=False, collar=True):
    x, y, _ = at(px, py); R_ = r * U
    if collar:
        c = cyl(x, y, R_ * 1.42, 0.03, 0.0, 128, COLLAR, R_ * 0.05)
    skirt = cyl(x, y, R_ * 1.16, 0.05, 0.035, 128, BLACKBONE if dark else BONE_DK, R_ * 0.08)
    body = cyl(x, y, R_, 0.26, 0.18, 128, None, 0.0)
    nfl = 10 if r < 40 else 12
    for i in range(nfl):
        a = 2 * math.pi * i / nfl
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=R_ * 0.22, depth=0.4, location=(x + math.cos(a) * R_ * 1.1, y + math.sin(a) * R_ * 1.1, 0.15))
        cut(body, bpy.context.object)
    b = body.modifiers.new('b', 'BEVEL'); b.width = R_ * 0.06; b.segments = 4; b.limit_method = 'ANGLE'; b.angle_limit = math.radians(40)
    bpy.ops.object.shade_smooth(); body.data.materials.clear(); body.data.materials.append(BLACKBONE if dark else BONE)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=32, radius=R_ * 0.8, location=(x, y, 0.31)); cp = bpy.context.object
    cp.scale = (1, 1, 0.16); bpy.ops.object.shade_smooth(); cp.data.materials.append(BONE_INSET if dark else BONE)

def skull(px, py, height_px):
    """The cast skull: a scanned-style skull mesh (models/skull.obj), subdivided, in grimy bone, embers in the sockets."""
    x0, y0, _ = at(px, py)
    bpy.ops.wm.obj_import(filepath=os.path.join(HERE, 'models', 'skull.obj')); sk = bpy.context.selected_objects[0]
    sk.rotation_euler = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
    k = height_px * U / sk.dimensions[1]
    sd = sk.modifiers.new('sd', 'SUBSURF'); sd.levels = 1; sd.render_levels = 2
    bpy.ops.object.shade_smooth(); sk.data.materials.clear(); sk.data.materials.append(BONE)
    sk.scale = (k, k, k * 0.7); sk.location = (x0, y0, 0.1 + sk.dimensions[2] * 0)
    hw = sk.dimensions[0] * k / 2 if False else None
    # embers deep in the sockets (in skull units: sockets at x +-0.135 h, y +0.033 h of the height)
    hgt = height_px * U
    for ex in (-0.108, 0.108):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=hgt * 0.05, location=(x0 + ex * hgt, y0 + 0.06 * hgt, 0.5))
        e = bpy.context.object; e.name = 'eyeball'; e.scale = (1.2, 0.9, 0.6); bpy.ops.object.shade_smooth(); e.data.materials.append(EYE)
        if HOT == 'eyes':
            l = bpy.data.lights.new('ember', 'POINT'); l.energy = 7.0; l.color = (1.0, 0.25, 0.06); l.shadow_soft_size = 0.05
            lo = bpy.data.objects.new('ember', l); bpy.context.scene.collection.objects.link(lo); lo.location = (x0 + ex * hgt, y0 + 0.06 * hgt, 0.68)
    return sk

def sawblade(px, py, r):
    x, y, _ = at(px, py); R_ = r * U; n = 54
    bm_verts = []
    import bmesh
    me = bpy.data.meshes.new('saw'); bmx = bmesh.new()
    pts = []
    for i in range(n):
        a0 = 2 * math.pi * i / n; a1 = 2 * math.pi * (i + 0.72) / n; a2 = 2 * math.pi * (i + 1) / n
        pts.append((math.cos(a0) * R_ * 0.93, math.sin(a0) * R_ * 0.93))
        pts.append((math.cos(a1) * R_, math.sin(a1) * R_))          # tooth tip, hooked
        pts.append((math.cos(a2 - 0.01) * R_ * 0.935, math.sin(a2 - 0.01) * R_ * 0.935))
    vs = [bmx.verts.new((px_, py_, 0)) for px_, py_ in pts]
    f = bmx.faces.new(vs); bmx.to_mesh(me); bmx.free()
    o = bpy.data.objects.new('saw', me); bpy.context.scene.collection.objects.link(o); bpy.context.view_layer.objects.active = o; o.select_set(True)
    sol = o.modifiers.new('s', 'SOLIDIFY'); sol.thickness = 0.03; bpy.ops.object.modifier_apply(modifier='s')
    o.location = (x, y, 0.04)
    # expansion slots and the gullet holes
    for i in range(6):
        a = 2 * math.pi * i / 6 + 0.2
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x + math.cos(a) * R_ * 0.78, y + math.sin(a) * R_ * 0.78, 0.04)); c = bpy.context.object
        c.scale = (R_ * 0.22, 0.012, 0.2); c.rotation_euler[2] = a; cut(o, c)
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.022, depth=0.2, location=(x + math.cos(a) * R_ * 0.67, y + math.sin(a) * R_ * 0.67, 0.04)); cut(o, bpy.context.object)
    o.data.materials.clear(); o.data.materials.append(SAW)
    # hub
    cyl(x, y, R_ * 0.2, 0.06, 0.07, 96, STEEL_DIRTY, 0.01)
    return o

def porthole(px, py, r):
    x, y, _ = at(px, py); R_ = r * U
    ring = cyl(x, y, R_ * 1.18, 0.12, 0.03, 160, None, 0.0, smooth=False)
    c = cyl(x, y, R_ * 0.98, 0.5, 0.0, 160); cut(ring, c)
    b = ring.modifiers.new('b', 'BEVEL'); b.width = 0.025; b.segments = 5; b.limit_method = 'ANGLE'
    bpy.ops.object.shade_smooth(); ring.data.materials.clear(); ring.data.materials.append(BRASS)
    for i in range(8):
        a = 2 * math.pi * i / 8 + math.pi / 8
        hexbolt(x + math.cos(a) * R_ * 1.08, y + math.sin(a) * R_ * 1.08, 0.045, 0.11)
    # recess in the plate, the printed face, the glass dome
    bpy.ops.mesh.primitive_circle_add(vertices=160, radius=R_ * 0.99, fill_type='NGON', location=(x, y, -0.02)); f = bpy.context.object
    f.data.materials.append(VUFACE)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=R_ * 0.99, location=(x, y, -0.02)); gd = bpy.context.object
    gd.scale = (1, 1, 0.16); bpy.ops.object.shade_smooth(); gd.data.materials.append(DOME)

def vuface():
    m = bpy.data.materials.new('vuface'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); im = nt.nodes.new('ShaderNodeTexImage')
    im.image = bpy.data.images.load(os.path.join(HERE, 'tex', 'skull_vu.png')); nt.links.new(tc.outputs['Generated'], im.inputs['Vector'])
    nt.links.new(im.outputs['Color'], p.inputs['Base Color']); nt.links.new(im.outputs['Color'], p.inputs['Emission Color'])
    p.inputs['Emission Strength'].default_value = 0.22; p.inputs['Roughness'].default_value = 0.7
    return m

def tag(px, py, w, h):
    t = box('tag', px, py, w, h, 0.03, 0.02, 0.06, 0.008, STEEL_TAG)
    rivet(px + 12, py + h / 2, 0.038)

def build():
    global STEEL_DIRTY, COLLAR, BONE, BONE_DK, BLACKBONE, BONE_INSET, TOOTH, EYE, SAW, BRASS, DOME, VUFACE, STEEL_TAG
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    P.SCALE = float(SC); R.P.SCALE = float(SC)
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    STEEL_DIRTY = P.metal('steeld', (0.38, 0.37, 0.35), 0.38, brushed=True, scale=20)
    COLLAR = P.metal('collar', (0.05, 0.048, 0.046), 0.34)
    BONE = bone(); BONE_DK = bone('boned', (0.55, 0.46, 0.32), rough=0.5)
    BLACKBONE = mat_principled('blackbone', (0.02, 0.018, 0.017), 0.38, coat=0.4)
    BONE_INSET = bone('bonei', (0.74, 0.64, 0.46))
    TOOTH = bone('tooth', (0.82, 0.74, 0.58), rough=0.32)
    EYE = emissive('eye', (1.0, 0.12, 0.02), 22.0 if HOT == 'eyes' else 0.0, base=(0.05, 0.005, 0.002), rough=0.15, coat=1.0)
    SAW = P.metal('saw', (0.36, 0.36, 0.37), 0.33, brushed=True, scale=18)
    BRASS = P.metal('brass', (0.42, 0.24, 0.08), 0.36, brushed=True, scale=12)
    DOME = mat_principled('dome', (1, 1, 1), 0.02, transmission=1.0); DOME.node_tree.nodes['Principled BSDF'].inputs['IOR'].default_value = 1.15
    VUFACE = vuface()
    STEEL_TAG = P.metal('tagsteel', (0.16, 0.155, 0.15), 0.4, brushed=True, scale=14)
    # the plate
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.06, 0.016, powdercoat())
    # recess for the porthole
    c = cyl(*at(170, 222)[:2], 1.12, 0.3, 0.0); cut(plate, c)
    for xx in range(40, W - 20, 46):
        for yy in (12, H - 12): rivet(xx, yy)
    for p_ in [(14, 14), (W - 14, 14), (14, H - 14), (W - 14, H - 14)]: R.screw(*p_) if False else rivet(*p_, 0.07)
    porthole(170, 222, 114)
    sawblade(490, 222, 156)
    skull(490, 212, 250)
    fluted_knob(812, 222, 64)
    for x in (82, 194, 306, 676, 788): fluted_knob(x, 456, 25)
    fluted_knob(904, 456, 25, dark=True)
    # PUNISH: a red dome in a steel bezel with a hinged guard frame
    x, y, _ = at(490, 452)
    bz = cyl(x, y, 0.36, 0.06, 0.02, 128, STEEL_DIRTY, 0.012)
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2; hexbolt(x + math.cos(a) * 0.3, y + math.sin(a) * 0.3, 0.03, 0.05)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=0.24, location=(x, y, 0.05)); d = bpy.context.object
    d.scale = (1, 1, 0.62); bpy.ops.object.shade_smooth()
    d.data.materials.append(emissive('punish', (1.0, 0.05, 0.02), 2.6 if HOT == 'cracks' else 0.0, base=(0.5, 0.012, 0.008), rough=0.18, coat=1.0, sss=0.4))
    # model tags (stamped steel), toggle buttons
    for i in range(4): tag(32 + i * 92, 24, 84, 30)
    for i in range(3): box('btnf', 64 + i * 74, 350, 64, 26, 0.04, 0.0, 0.04, 0.006, BONE_DK); box('btn', 68 + i * 74, 353, 56, 20, 0.06, 0.03, 0.035, 0.01, BLACKBONE)
    # title, cast in bone
    bpy.ops.object.text_add(location=at(812, 52, 0.02)); tx = bpy.context.object
    tx.data.body = 'Skull Cutter'; tx.data.font = bpy.data.fonts.load(os.path.join(FONTS, 'MetalMania-Regular.ttf'))
    tx.data.size = 0.52; tx.data.align_x = 'CENTER'; tx.data.align_y = 'CENTER'; tx.data.extrude = 0.025; tx.data.bevel_depth = 0.006; tx.data.bevel_resolution = 3
    tx.data.materials.append(BONE)
    # lights: warm key, cold fill, a blood-red kicker from below
    P.area(s, 'key', (-5, 6, 7), 4, 700, (1.0, 0.93, 0.84))
    P.area(s, 'fill', (6, -4, 5), 6, 140, (0.65, 0.78, 1.0))
    P.area(s, 'rim', (0, 9, 1.4), 9, 260)
    P.area(s, 'red', (2, -8, 0.8), 6, 300, (1.0, 0.12, 0.05))
    P.area(s, 'kick', (6, 3, 2), 2, 220, (1.0, 0.85, 0.7))
    P.area(s, 'soft', (0, 3.2, 9), 7, 80, (0.85, 0.9, 1.0))
    if HOT: s.world.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    name = 'skull' + ('_' + HOT if HOT else '') + '.png'
    s.render.filepath = os.path.join(OUTDIR, name); bpy.ops.render.render(write_still=True); print('rendered', name)

if __name__ == '__main__':
    build()
