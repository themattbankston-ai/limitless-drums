# PORTCULLIS, the gate plugin's faceplate, rendered in Blender (Cycles).
# A dark castle wall of dressed stone with iron bands, a stone archway over the recessed hit display, an iron
# portcullis (its own layer: the app raises it as the gate opens), two hanging lanterns (green OPEN, red CLOSED),
# forged iron knobs, a carved title lintel.
# Layers (LD_LAYER): 'plate' (gate hidden from the camera), 'gate' (the portcullis alone, closed position).
# LD_HOT: 'open' (green lantern lit), 'closed' (red lantern lit).
# Usage: python3 render_portcullis.py <out dir> [scale] [samples]
import bpy, math, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_skull as SK
from render_skull import mix, mrange, math_, noise, cyl, emissive
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
HOT = os.environ.get('LD_HOT', ''); LAYER = os.environ.get('LD_LAYER', '')
FONTS = os.environ.get('LD_FONTS', '/tmp/claude-0/mk/fonts')
W, H = 980, 544
R.W, R.H = W, H
at = R.at

# layout (panel px); the overlay uses the same numbers
OPEN = dict(x0=340, x1=640, y_spring=170, y_bot=338, ry=84)      # the opening: straight jambs, elliptical arch on top
GATE = dict(x0=344, x1=636, y0=70, y1=338, cols=9, rows=6)
LANTERNS = [(262, 150), (718, 150)]                                # OPEN (green), CLOSED (red)
THRESH = (138, 212, 50)
KEYSRC = (836, 92, 150, 30)
KEYBTN = [(764, 136, 66, 26), (842, 136, 66, 26)]
KEYKNOB = [(790, 246, 22), (884, 246, 22)]
ROW = [100 + i * 130 for i in range(7)]; KY = 452
TITLE = (490, 36)

def stone():
    m = bpy.data.materials.new('stone'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); br = nt.nodes.new('ShaderNodeTexBrick')
    br.inputs['Scale'].default_value = 0.9; br.inputs['Mortar Size'].default_value = 0.022; br.inputs['Brick Width'].default_value = 0.7; br.inputs['Row Height'].default_value = 0.3; br.inputs['Bias'].default_value = 0.0
    br.inputs['Mortar Smooth'].default_value = 0.3; br.offset = 0.5
    br.inputs['Color1'].default_value = (0.15, 0.135, 0.115, 1); br.inputs['Color2'].default_value = (0.08, 0.075, 0.07, 1)
    nt.links.new(tc.outputs['Object'], br.inputs['Vector'])
    mortar = br.outputs['Fac']
    grain = mrange(nt, noise(nt, 18, 10, 0.7), 0.3, 0.7)
    col = mix(nt, math_(nt, 'MULTIPLY', grain, 0.6), br.outputs['Color'], (0.16, 0.15, 0.14))
    moss = math_(nt, 'MULTIPLY', mrange(nt, noise(nt, 4, 6), 0.55, 0.68), mortar)
    col = mix(nt, math_(nt, 'MULTIPLY', mrange(nt, noise(nt, 1.4, 4), 0.45, 0.75), 0.65), col, (0.02, 0.019, 0.018))
    col = mix(nt, mortar, col, (0.025, 0.024, 0.022))
    col = mix(nt, math_(nt, 'MULTIPLY', moss, 0.8), col, (0.03, 0.06, 0.025))
    nt.links.new(col, p.inputs['Base Color']); p.inputs['Roughness'].default_value = 0.85
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 1.0; bump.inputs['Distance'].default_value = 0.06
    h = math_(nt, 'SUBTRACT', math_(nt, 'MULTIPLY', noise(nt, 26, 12, 0.75), 0.5), math_(nt, 'MULTIPLY', mortar, 1.2))
    nt.links.new(h, bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def dressed(name='dressed', base=(0.3, 0.28, 0.25)):
    """Cut stone for the arch, jambs and lintel: lighter, chisel-marked."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    g = mrange(nt, noise(nt, 14, 10, 0.7), 0.3, 0.7)
    col = mix(nt, g, base, tuple(c * 0.62 for c in base))
    ao = nt.nodes.new('ShaderNodeAmbientOcclusion'); ao.inputs['Distance'].default_value = 0.12
    col = mix(nt, mrange(nt, ao.outputs['AO'], 0.4, 1.0, 1, 0), col, (0.02, 0.02, 0.018))
    nt.links.new(col, p.inputs['Base Color']); p.inputs['Roughness'].default_value = 0.8
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.6; bump.inputs['Distance'].default_value = 0.02
    nt.links.new(noise(nt, 40, 12, 0.8), bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def iron(name='iron'):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    rust = mrange(nt, noise(nt, 9, 8, 0.7), 0.6, 0.72)
    col = mix(nt, rust, (0.05, 0.05, 0.052), (0.2, 0.07, 0.025))
    nt.links.new(col, p.inputs['Base Color']); nt.links.new(math_(nt, 'SUBTRACT', 1.0, rust), p.inputs['Metallic'])
    nt.links.new(mrange(nt, rust, 0, 1, 0.42, 0.8), p.inputs['Roughness'])
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.5   # hammered
    vor = nt.nodes.new('ShaderNodeTexVoronoi'); vor.inputs['Scale'].default_value = 60; tc = nt.nodes.new('ShaderNodeTexCoord'); nt.links.new(tc.outputs['Object'], vor.inputs['Vector'])
    nt.links.new(math_(nt, 'ADD', vor.outputs['Distance'], math_(nt, 'MULTIPLY', rust, 0.5)), bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def arch_points(n=40, grow=0.0):
    """Outline of the opening (panel px): up the left jamb, over the arch, down the right jamb."""
    o = OPEN; cx = (o['x0'] + o['x1']) / 2; rx = (o['x1'] - o['x0']) / 2 + grow; ry = o['ry'] + grow
    pts = [(o['x0'] - grow, o['y_bot'] + grow)]
    for i in range(n + 1):
        a = math.pi + math.pi * i / n; pts.append((cx + math.cos(a) * rx, o['y_spring'] + math.sin(a) * ry))
    pts.append((o['x1'] + grow, o['y_bot'] + grow))
    return pts

def prism(name, pts, z0, z1, mat):
    import bmesh
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [bm.verts.new((*at(x, y)[:2], z0)) for x, y in pts]; f = bm.faces.new(vs)
    r = bmesh.ops.extrude_face_region(bm, geom=[f]); top = [e for e in r['geom'] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=top, vec=(0, 0, z1 - z0)); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); o.data.materials.append(mat)
    return o

def voussoirs(mat):
    """The arch stones, each its own block with a gap, plus the keystone; and the jamb blocks."""
    o = OPEN; cx = (o['x0'] + o['x1']) / 2; rx = (o['x1'] - o['x0']) / 2; ry = o['ry']; t = 30; n = 11
    for i in range(n):
        a0 = math.pi + math.pi * i / n + 0.012; a1 = math.pi + math.pi * (i + 1) / n - 0.012
        key = i == n // 2; tt = t + (10 if key else 0)
        pts = [(cx + math.cos(a0) * rx, o['y_spring'] + math.sin(a0) * ry), (cx + math.cos(a0) * (rx + tt), o['y_spring'] + math.sin(a0) * (ry + tt)),
               (cx + math.cos(a1) * (rx + tt), o['y_spring'] + math.sin(a1) * (ry + tt)), (cx + math.cos(a1) * rx, o['y_spring'] + math.sin(a1) * ry)]
        b = prism('vous', pts, 0.0, 0.16 + (0.05 if key else 0), mat)
        bv = b.modifiers.new('bv', 'BEVEL'); bv.width = 0.02; bv.segments = 3
    for side in (0, 1):
        x = o['x0'] - t if side == 0 else o['x1']
        y = o['y_spring']
        while y < o['y_bot'] - 4:
            h = min(42, o['y_bot'] - y)
            b = box('jamb', x + (2 if side == 0 else 0), y + 1, t - 2, h - 2, 0.16, 0.08, 0.015, 0.012, mat); y += h
    # the sill under the opening
    box('sill', o['x0'] - t - 6, o['y_bot'], o['x1'] - o['x0'] + 2 * t + 12, 14, 0.14, 0.07, 0.01, 0.01, mat)

def portcullis(mat):
    gg = GATE; objs = []
    w = gg['x1'] - gg['x0']; h = gg['y1'] - gg['y0']
    for i in range(gg['cols']):
        x = gg['x0'] + 8 + (w - 16) * i / (gg['cols'] - 1); X, Y, _ = at(x, (gg['y0'] + gg['y1'] - 18) / 2)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(X, Y, 0.06)); b = bpy.context.object; b.scale = (0.09, (h - 18) * U, 0.09)
        bv = b.modifiers.new('b', 'BEVEL'); bv.width = 0.012; bv.segments = 2; b.data.materials.append(mat); objs.append(b)
        X2, Y2, _ = at(x, gg['y1'] - 9)
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.07, depth=0.22, location=(X2, Y2, 0.06)); sp = bpy.context.object
        sp.rotation_euler = (math.pi / 2, math.pi / 4, 0); sp.data.materials.append(mat); objs.append(sp)
    for j in range(gg['rows']):
        y = gg['y0'] + 10 + (h - 50) * j / (gg['rows'] - 1); X, Y, _ = at((gg['x0'] + gg['x1']) / 2, y)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(X, Y, 0.11)); b = bpy.context.object; b.scale = (w * U, 0.07, 0.06)
        bv = b.modifiers.new('b', 'BEVEL'); bv.width = 0.01; bv.segments = 2; b.data.materials.append(mat); objs.append(b)
        for i in range(gg['cols']):
            x = gg['x0'] + 8 + (w - 16) * i / (gg['cols'] - 1); X, Y, _ = at(x, y)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.028, location=(X, Y, 0.145)); r = bpy.context.object
            r.scale = (1, 1, 0.6); r.data.materials.append(mat); objs.append(r)
    for o in objs: o.name = 'gate_' + o.name
    return objs

def lantern(px, py, glass):
    x, y, _ = at(px, py); k = 1.5
    box('brk', px - 12, py - 100, 24, 30, 0.05, 0.025, 0.02, 0.006, IRON)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(*at(px, py - 80)[:2], 0.16)); a = bpy.context.object; a.scale = (0.035, 0.24, 0.035); a.data.materials.append(IRON)
    box('lbot', px - 30, py + 34, 60, 10, 0.16, 0.26, 0.01, 0.004, IRON)
    roof = prism('roof', [(px - 34, py - 38), (px + 34, py - 38), (px + 4, py - 60), (px - 4, py - 60)], 0.1, 0.5, IRON)
    rb = roof.modifiers.new('b', 'BEVEL'); rb.width = 0.02; rb.segments = 2
    bpy.ops.mesh.primitive_torus_add(major_radius=0.07, minor_radius=0.014, location=(*at(px, py - 66)[:2], 0.3)); ring = bpy.context.object
    ring.rotation_euler = (math.pi / 2, 0, 0); ring.data.materials.append(IRON)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(*at(px, py - 4)[:2], 0.3)); gl = bpy.context.object; gl.scale = (0.5, 0.7, 0.42)
    bv = gl.modifiers.new('b', 'BEVEL'); bv.width = 0.03; gl.data.materials.append(glass)
    for dx in (-26, 26):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(*at(px + dx, py - 4)[:2], 0.3)); pst = bpy.context.object; pst.scale = (0.05, 0.74, 0.46); pst.data.materials.append(IRON)
    for dy in (-22, -4, 14):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(*at(px, py + dy)[:2], 0.52)); bar = bpy.context.object; bar.scale = (0.5, 0.018, 0.018); bar.data.materials.append(IRON)

def iron_knob(px, py, r):
    """Forged octagonal iron knob with a bronze cap."""
    x, y, _ = at(px, py); R_ = r * U
    cyl(x, y, R_ * 1.4, 0.03, 0.0, 8, IRON, R_ * 0.06, smooth=False).rotation_euler[2] = math.pi / 8
    b = cyl(x, y, R_, 0.24, 0.14, 8, IRON, R_ * 0.1, smooth=False); b.rotation_euler[2] = math.pi / 8
    cyl(x, y, R_ * 0.6, 0.04, 0.27, 96, BRONZE, R_ * 0.06)

def build():
    global IRON, BRONZE
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    IRON = iron(); BRONZE = P.metal('bronze', (0.42, 0.26, 0.11), 0.34); SK.STEEL_DIRTY = IRON
    DRESSED = dressed()
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.04, 0.012, stone())
    # the opening: cut through the wall into a dark recess with the display glass at the back
    import bmesh
    op = prism('opening_cut', arch_points(), -0.4, 0.4, IRON); cut(plate, op)
    fl = prism('screenglass', arch_points(grow=4), -0.13, -0.12, mat_principled('glass', (0.004, 0.006, 0.005), 0.08, coat=1.0))
    voussoirs(DRESSED)
    # iron bands, top and bottom, with big rivets
    for y0 in (0, H - 22):
        box('band', 0, y0, W, 22, 0.05, 0.025, 0.004, 0.006, IRON)
        for xx in range(24, W, 48): SK.rivet(xx, y0 + 11, 0.05)
    # title lintel: a dressed stone plaque with raised iron letters
    box('lintel', TITLE[0] - 170, TITLE[1] - 26, 340, 52, 0.14, 0.07, 0.02, 0.012, dressed('lintelm', (0.42, 0.39, 0.34)))
    bpy.ops.object.text_add(location=at(TITLE[0], TITLE[1] + 2, 0.15)); tx = bpy.context.object
    tx.data.body = 'Portcullis'; tx.data.font = bpy.data.fonts.load(os.path.join(FONTS, 'PirataOne-Regular.ttf'))
    tx.data.size = 0.56; tx.data.align_x = 'CENTER'; tx.data.align_y = 'CENTER'; tx.data.extrude = 0.02; tx.data.bevel_depth = 0.004; tx.data.bevel_resolution = 2
    tx.data.materials.append(IRON)
    # lanterns: green (open), red (closed)
    gON = HOT == 'open'; rON = HOT == 'closed'
    lantern(*LANTERNS[0], emissive('lg_g', (0.25, 1.0, 0.55), 12.0 if gON else 0.0, base=(0.04, 0.12, 0.07), rough=0.15, sss=0.6, coat=1.0))
    lantern(*LANTERNS[1], emissive('lg_r', (1.0, 0.12, 0.05), 12.0 if rON else 0.0, base=(0.14, 0.03, 0.02), rough=0.15, sss=0.6, coat=1.0))
    for on, (px, py), col in ((gON, LANTERNS[0], (0.3, 1.0, 0.6)), (rON, LANTERNS[1], (1.0, 0.18, 0.06))):
        if on:
            l = bpy.data.lights.new('lamp', 'POINT'); l.energy = 40; l.color = col; lo = bpy.data.objects.new('lamp', l); s.collection.objects.link(lo); lo.location = (*at(px, py)[:2], 0.3)
    # controls
    iron_knob(THRESH[0], THRESH[1], THRESH[2])
    for x in ROW: iron_knob(x, KY, 21)
    for (x, y, r) in KEYKNOB: iron_knob(x, y, r)
    kx, ky, kw, kh = KEYSRC; box('keysrc', kx - kw / 2 - 6, ky - kh / 2 - 6, kw + 12, kh + 12, 0.05, 0.02, 0.02, 0.008, IRON)
    box('keywin', kx - kw / 2, ky - kh / 2, kw, kh, 0.02, 0.05, 0.012, 0.004, mat_principled('kglass', (0.004, 0.006, 0.005), 0.1, coat=1.0))
    for (bx_, by_, bw_, bh_) in KEYBTN:
        box('kbf', bx_ - 4, by_ - 4, bw_ + 8, bh_ + 8, 0.04, 0.02, 0.02, 0.006, IRON); box('kb', bx_, by_, bw_, bh_, 0.06, 0.05, 0.02, 0.01, DRESSED)
    for (a_, b_) in [(KEYSRC[0] - 80, KEYSRC[1]), (KEYSRC[0] + 80, KEYSRC[1])]: SK.rivet(a_, b_, 0.03)
    # the portcullis
    gate = portcullis(IRON)
    # light: cold moonlight key, a warm torch fill from below, rim
    P.area(s, 'key', (-5, 6, 7), 3, 520, (0.78, 0.86, 1.0))
    for (lx, ly) in LANTERNS:   # warm torch light on the wall around each lantern
        l = bpy.data.lights.new('torch', 'POINT'); l.energy = 25; l.color = (1.0, 0.55, 0.25); l.shadow_soft_size = 0.3
        lo = bpy.data.objects.new('torch', l); s.collection.objects.link(lo); lo.location = (*at(lx, ly + 50)[:2], 0.9)
    P.area(s, 'fill', (6, -4, 4), 6, 140, (1.0, 0.7, 0.4))
    P.area(s, 'rim', (0, 9, 1.4), 9, 240, (0.8, 0.9, 1.0))
    P.area(s, 'kick', (6, 3, 2), 2, 150, (0.85, 0.95, 1.0))
    P.area(s, 'soft', (0, 3.2, 9), 7, 60, (0.85, 0.9, 1.0))
    if LAYER == 'plate':
        for o in gate: o.visible_camera = False; o.visible_shadow = False
    elif LAYER == 'gate':
        s.render.film_transparent = True
        for o in s.objects:
            if o.type in ('MESH', 'CURVE', 'FONT') and o not in gate: o.visible_camera = False
    name = 'port' + ('_' + LAYER if LAYER else '') + ('_' + HOT if HOT else '') + '.png'
    s.render.filepath = os.path.join(OUTDIR, name); bpy.ops.render.render(write_still=True); print('rendered', name)

if __name__ == '__main__':
    build()
