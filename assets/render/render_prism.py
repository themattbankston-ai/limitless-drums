# PRISM, the EQ's faceplate, rendered in Blender (Cycles).
# Black anodised, brushed aluminium with bright machined chamfers; a real glass prism in a cradle at the left (the app
# draws the white beam going in and the spectrum fanning out into the display); a deep display under a thick glass
# slab; machined keys with frosted light rings; the rack's own knobs (knurled skirt, silver cap, acrylic ring).
# Usage: python3 render_prism.py <out dir> [scale] [samples]
import bpy, math, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_skull as SK
from render_skull import cyl
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
FONTS = os.environ.get('LD_FONTS', '/tmp/claude-0/mk/fonts')
W, H = 980, 544
R.W, R.H = W, H
at = R.at

# layout (panel px), shared with the overlay
DISPLAY = (130, 28, 670, 272)                          # x, y, w, h
PRISM = (66, 160, 82)
BAY = (14, 40, 104, 214)                                  # centre, side
METER = (840, 40, 56, 206)                             # one glass window, two segmented bars inside
KEY = (40, 36)                                         # every key is the same machined square
BANDKEYS = [(130 + i * 52, 324) for i in range(6)]     # top-left corners
SHAPEKEYS = [(470 + i * 52, 324) for i in range(6)]
LEFTKEYS = [(46, 390), (46, 438), (46, 486)]           # band on / solo / delete
DYNKEY = (480, 448)
PHASEKEY = (822, 316, 92, 36)
KNOBS = [(180, 466), (290, 466), (400, 466), (600, 466), (710, 466)]; KR = 22
OUTPUT = (868, 466, 24)
BANDLCD = (28, 326, 80, 50)

def build():
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    R.ANOD = P.metal('anod', (0.035, 0.036, 0.04), 0.42, brushed=True, scale=28)
    R.STEEL = P.metal('steel', (0.6, 0.61, 0.64), 0.26)
    R.GLASS = mat_principled('glass', (0.003, 0.003, 0.005), 0.06, coat=1.0)
    R.COLLAR = mat_principled('collar', (0.008, 0.008, 0.01), 0.5)
    R.ACRYLIC = mat_principled('acrylic', (0.55, 0.57, 0.62), 0.35, sss=0.6, coat=0.6)
    R.BLACKMETAL = P.metal('blackm', (0.03, 0.03, 0.034), 0.32)
    R.SILVER = P.radial_brushed('silver', (0.75, 0.76, 0.8), 0.22)
    KEYM = P.metal('keym', (0.5, 0.51, 0.54), 0.3, brushed=True, scale=20)
    CHAMFER = P.metal('chamf', (0.85, 0.86, 0.9), 0.12)
    SK.STEEL_DIRTY = R.STEEL
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.05, 0.012, R.ANOD)          # a wide bevel: the bright machined chamfer
    # display: a pocket with a dark floor and a thick bevelled glass slab on top
    dx, dy, dw, dh = DISPLAY
    cpt = P.rounded_box('cut', dw * U, dh * U, 0.6, 0.08, 0.01); cpt.location = at(dx + dw / 2, dy + dh / 2, 0.0); cut(plate, cpt)
    bpy.ops.mesh.primitive_plane_add(size=1); fl = bpy.context.object; fl.scale = (dw * U, dh * U, 1); fl.location = at(dx + dw / 2, dy + dh / 2, -0.14)
    fl.data.materials.append(mat_principled('floor', (0.004, 0.004, 0.006), 0.5))
    fl.data.materials[0] = mat_principled('floor', (0.004, 0.004, 0.006), 0.35, coat=1.0)   # glass-coated floor; the code adds the sheen
    # a chamfered bright frame round the display
    fr = P.rounded_box('frame', (dw + 14) * U, (dh + 14) * U, 0.04, 0.11, 0.015); fr.location = at(dx + dw / 2, dy + dh / 2, 0.01)
    fc = P.rounded_box('fc', dw * U, dh * U, 0.5, 0.08, 0.01); fc.location = at(dx + dw / 2, dy + dh / 2, 0.0); cut(fr, fc); fr.data.materials.clear(); fr.data.materials.append(CHAMFER)
    # the meter: one glass window
    mx, my, mw, mh = METER; R.window(plate, mx, my, mw, mh, depth=0.06)
    # the prism: an equilateral glass prism lying in a two-arm cradle
    # the prism sits in a satin silver bay so you can see it bend the light
    bx_, by_, bw_, bh_ = BAY
    cb = P.rounded_box('bay', bw_ * U, bh_ * U, 0.6, 0.08, 0.01); cb.location = at(bx_ + bw_ / 2, by_ + bh_ / 2, 0.0); cut(plate, cb)
    bpy.ops.mesh.primitive_plane_add(size=1); bf = bpy.context.object; bf.scale = (bw_ * U, bh_ * U, 1); bf.location = at(bx_ + bw_ / 2, by_ + bh_ / 2, -0.1)
    bf.data.materials.append(mat_principled('velvet', (0.006, 0.006, 0.008), 0.9))
    px, py, side = PRISM; hgt = side * math.sqrt(3) / 2
    import bmesh
    me = bpy.data.meshes.new('prism'); bm = bmesh.new()
    tri = [(px, py - hgt * 2 / 3), (px - side / 2, py + hgt / 3), (px + side / 2, py + hgt / 3)]
    vs = [bm.verts.new((*at(x, y)[:2], -0.08)) for x, y in tri]; f = bm.faces.new(vs)
    r = bmesh.ops.extrude_face_region(bm, geom=[f]); top = [e for e in r['geom'] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=top, vec=(0, 0, 0.5)); bm.to_mesh(me); bm.free()
    pr = bpy.data.objects.new('prism', me); s.collection.objects.link(pr)
    bv = pr.modifiers.new('b', 'BEVEL'); bv.width = 0.015; bv.segments = 3
    pm = mat_principled('prismg', (0.97, 0.98, 1.0), 0.0, transmission=1.0); pb = pm.node_tree.nodes['Principled BSDF']; pb.inputs['IOR'].default_value = 1.62
    if 'Dispersion' in pb.inputs: pb.inputs['Dispersion'].default_value = 0.6
    # a crystal that holds the spectrum: glass plus a faint rainbow glowing inside it
    nt = pm.node_tree; pb = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Generated'], sep.inputs[0])
    ramp = nt.nodes.new('ShaderNodeValToRGB'); el = ramp.color_ramp.elements
    el[0].position = 0.0; el[0].color = (1, 0.15, 0.15, 1); el[1].position = 1.0; el[1].color = (0.6, 0.25, 1, 1)
    for pos, col in ((0.2, (1, 0.55, 0.1, 1)), (0.4, (1, 0.95, 0.2, 1)), (0.6, (0.2, 1, 0.5, 1)), (0.8, (0.2, 0.7, 1, 1))):
        e = el.new(pos); e.color = col
    nt.links.new(sep.outputs[0], ramp.inputs[0]); nt.links.new(ramp.outputs[0], pb.inputs['Emission Color']); pb.inputs['Emission Strength'].default_value = 0.9
    pr.data.materials.append(pm)
    pr.rotation_euler = (0.0, math.radians(28), 0.0)   # turned a little so its faces read as a solid crystal
    import mathutils
    piv = mathutils.Vector((*at(px, py)[:2], 0.17)); pr.location = piv - mathutils.Matrix.Rotation(math.radians(28), 4, 'Y') @ piv + mathutils.Vector((0, 0, 0))
    for dxx in (-side * 0.36, side * 0.36):
        box('cradle', px + dxx - 4, py + hgt / 3 - 4, 8, 22, 0.3, 0.04, 0.01, 0.006, CHAMFER)
    box('base', px - side / 2 - 4, py + hgt / 3 + 14, side + 8, 10, 0.06, -0.05, 0.01, 0.006, CHAMFER)
    # keys: machined caps over a frosted ring (the code lights the ring in the band's colour)
    for (kx, ky, kw, kh) in [(x, y, *KEY) for (x, y) in BANDKEYS + SHAPEKEYS + LEFTKEYS + [DYNKEY]] + [PHASEKEY]:
        box('ring', kx - 3, ky - 3, kw + 6, kh + 6, 0.03, 0.01, 0.03, 0.008, R.ACRYLIC)
        box('key', kx, ky, kw, kh, 0.08, 0.05, 0.03, 0.012, KEYM)
    # band number LCD
    bx, by, bw, bh = BANDLCD; R.window(plate, bx + bw / 2 - bw / 2, by, bw, bh, depth=0.05) if False else R.window(plate, bx, by, bw, bh, depth=0.05)
    # knobs
    for (x, y) in KNOBS: R.knob(x, y, KR)
    R.knob(OUTPUT[0], OUTPUT[1], OUTPUT[2])
    # screws at the corners
    for (x, y) in ((14, 14), (W - 14, 14), (14, H - 14), (W - 14, H - 14)): R.screw(x, y)
    # studio light: soft key, cool fill, a strong rim for the chamfers
    P.area(s, 'key', (-4, 6, 6), 5, 1100)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.7, 0.82, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 460)
    P.area(s, 'kick', (5, 3, 2), 2, 260, (1.0, 0.9, 0.8))
    P.area(s, 'under', (0, -9, 1.5), 8, 140, (0.6, 0.75, 1.0))
    s.render.filepath = os.path.join(OUTDIR, 'prism.png'); bpy.ops.render.render(write_still=True); print('rendered prism.png')

if __name__ == '__main__':
    build()
