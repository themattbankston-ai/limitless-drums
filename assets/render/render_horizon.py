# HORIZON, the width plugin's faceplate, rendered in Blender (Cycles).
# A cockpit panel: bare brushed aluminium skins with seams and rivet lines, quarter-turn fasteners, black avionics
# bezels (round radar scope, wide horizon display, slip-ball correlation tube), a throttle quadrant for Width (the
# lever is its own layer: the app slides it), an avionics knob for Bass mono, a lit annunciator for Keep loudness.
# Layers (LD_LAYER): 'plate' (lever hidden from the camera), 'lever' (the lever alone, at 100 %).
# Usage: python3 render_horizon.py <out dir> [scale] [samples]
import bpy, math, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
OUTDIR = sys.argv[1] if len(sys.argv) > 1 else 'out'; SC = sys.argv[2] if len(sys.argv) > 2 else '2'; SAMP = int(sys.argv[3]) if len(sys.argv) > 3 else 96
import render_skull as SK
from render_skull import cyl, mix, mrange, math_, noise
import render_rack as R
from render_rack import P, U, box, cut, mat_principled
LAYER = os.environ.get('LD_LAYER', '')
W, H = 980, 544
R.W, R.H = W, H
at = R.at

# layout (panel px), shared with the overlay
# panel sections (between the skin seams): left 0-300 (centre 150), middle 303-790 (centre 546), right 793-980 (centre 890)
RADAR = (150, 238, 94)
HDISP = (330, 70, 430, 220)              # x, y, w, h
SLIP = (545, 338, 300, 34)               # centre x, centre y, w, h of the tube window
SLOT = (882, 104, 16, 330)                # x, y, w, h of the throttle slot
LEVER_Y = SLOT[1] + SLOT[3] / 2          # lever rests at 100 % (the middle)
BASS = (150, 462, 28)
ANN = (364, 440, 150, 44)                # annunciator x, y, w, h
WREAD = (654, 462, 150, 46)              # width read-out window
TITLE = (150, 54, 190, 36)

def alu():
    m = bpy.data.materials.new('alu'); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (60, 1.2, 1)
    nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 30; nz.inputs['Detail'].default_value = 10
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], nz.inputs['Vector'])
    smudge = mrange(nt, noise(nt, 3, 8, 0.6), 0.45, 0.7)
    col = mix(nt, mrange(nt, nz.outputs['Fac'], 0.3, 0.7), (0.3, 0.31, 0.33), (0.4, 0.41, 0.44))
    col = mix(nt, math_(nt, 'MULTIPLY', smudge, 0.35), col, (0.3, 0.3, 0.31))
    nt.links.new(col, p.inputs['Base Color']); p.inputs['Metallic'].default_value = 1.0
    nt.links.new(mrange(nt, nz.outputs['Fac'], 0, 1, 0.3, 0.42), p.inputs['Roughness'])
    if 'Anisotropic' in p.inputs: p.inputs['Anisotropic'].default_value = 0.6
    return m

def bezel_rect(x, y, w, h, t, mat, z=0.0, r=0.08):
    o = P.rounded_box('bz', (w + 2 * t) * U, (h + 2 * t) * U, 0.08, r + t * U, 0.012); o.location = at(x + w / 2, y + h / 2, z + 0.04)
    c = P.rounded_box('bzc', w * U, h * U, 0.6, r, 0.01); c.location = at(x + w / 2, y + h / 2, 0.0); cut(o, c)
    o.data.materials.clear(); o.data.materials.append(mat)
    for (sx_, sy_) in ((x - t / 2, y - t / 2), (x + w + t / 2, y - t / 2), (x - t / 2, y + h + t / 2), (x + w + t / 2, y + h + t / 2)):
        X, Y, _ = at(sx_, sy_); s = cyl(X, Y, 0.035, 0.03, 0.09, 32, SCREW, 0.008)
    return o

def window(plate, x, y, w, h, depth, mat):
    c = P.rounded_box('win', w * U, h * U, 0.6, 0.06, 0.01); c.location = at(x + w / 2, y + h / 2, 0.0); cut(plate, c)
    bpy.ops.mesh.primitive_plane_add(size=1); f = bpy.context.object; f.scale = (w * U, h * U, 1); f.location = at(x + w / 2, y + h / 2, -depth); f.data.materials.append(mat)

def dzus(px, py):
    X, Y, _ = at(px, py); cyl(X, Y, 0.09, 0.03, 0.015, 64, SCREW, 0.01)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(X, Y, 0.04)); sl = bpy.context.object; sl.scale = (0.12, 0.018, 0.05); sl.rotation_euler[2] = 0.4
    sl.data.materials.append(mat_principled('slot', (0.02, 0.02, 0.02), 0.6))

def build():
    global SCREW
    s = P.reset(); s.render.film_transparent = False; s.cycles.samples = SAMP
    c = P.camera(s, W, H, max(W, H) * U * 1.01); t = math.radians(11); c.location = (0, -10 * math.sin(t), 10 * math.cos(t)); c.rotation_euler = (t, 0, 0)
    SCREW = P.metal('screw', (0.55, 0.55, 0.57), 0.3); SK.STEEL_DIRTY = SCREW
    BLACK = mat_principled('avionics', (0.018, 0.018, 0.02), 0.55, coat=0.2)
    GLASS = mat_principled('glass', (0.003, 0.004, 0.006), 0.06, coat=1.0)
    plate = box('plate', 0, 0, W, H, 0.16, -0.08, 0.04, 0.012, alu())
    # skin seams (grooves) with rivet lines along them
    for (x0, y0, w_, h_) in ((0, 108, W, 3), (300, 0, 3, H), (790, 0, 3, H)):
        g_ = P.rounded_box('seam', w_ * U, h_ * U, 0.04, 0.002, 0.001); g_.location = at(x0 + w_ / 2, y0 + h_ / 2, 0.0); cut(plate, g_)
    for x in range(20, W, 26):
        if not (300 < x < 790): SK.rivet(x, 100, 0.028); SK.rivet(x, 116, 0.028)
    for y in range(20, H, 26): SK.rivet(292, y, 0.028); SK.rivet(310, y, 0.028); SK.rivet(782, y, 0.028); SK.rivet(800, y, 0.028)
    for (x, y) in ((22, 22), (W - 22, 22), (22, H - 22), (W - 22, H - 22), (545, 22), (545, H - 22)): dzus(x, y)
    # radar: round bezel, recessed scope glass
    rx, ry, rr = RADAR; X, Y, _ = at(rx, ry)
    hole = cyl(X, Y, rr * U, 0.6, 0.0, 128); cut(plate, hole)
    bz = cyl(X, Y, rr * U * 1.16, 0.1, 0.04, 128, None, 0.0, smooth=False); hc = cyl(X, Y, rr * U * 0.99, 0.5, 0.0, 128); cut(bz, hc)
    bb = bz.modifiers.new('b', 'BEVEL'); bb.width = 0.025; bb.segments = 4; bpy.ops.object.shade_smooth(); bz.data.materials.clear(); bz.data.materials.append(BLACK)
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2; cyl(X + math.cos(a) * rr * U * 1.08, Y + math.sin(a) * rr * U * 1.08, 0.035, 0.03, 0.1, 32, SCREW, 0.008)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=rr * U, location=(X, Y, -0.08)); scr = bpy.context.object
    scr.scale = (1, 1, 0.12); bpy.ops.object.shade_smooth(); scr.data.materials.append(mat_principled('scope', (0.004, 0.012, 0.012), 0.08, coat=1.0))
    # horizon display
    hx, hy, hw, hh = HDISP
    window(plate, hx, hy, hw, hh, 0.12, GLASS); bezel_rect(hx, hy, hw, hh, 12, BLACK)
    # slip-ball tube: window, then a curved glass tube with a black ball (the ball is drawn by the app)
    sx_, sy_, sw, sh = SLIP; window(plate, sx_ - sw / 2, sy_ - sh / 2, sw, sh, 0.1, GLASS); bezel_rect(sx_ - sw / 2, sy_ - sh / 2, sw, sh, 8, BLACK)
    # throttle quadrant: a black housing with the slot; the lever
    tx, ty, tw, th = SLOT
    box('quad', tx - 34, ty - 24, tw + 68, th + 48, 0.2, 0.1, 0.03, 0.012, BLACK)
    sl = P.rounded_box('slotcut', tw * U, th * U, 0.8, 0.06, 0.01); sl.location = at(tx + tw / 2, ty + th / 2, 0.0)
    q = [o for o in s.objects if o.name.startswith('quad')][-1]; cut(q, sl); cut(plate, sl)
    lever = []
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.05, depth=0.5, location=(*at(tx + tw / 2, LEVER_Y)[:2], 0.3)); sh_ = bpy.context.object
    sh_.data.materials.append(P.metal('chrome', (0.8, 0.8, 0.82), 0.1)); lever.append(sh_)
    hd = box('lv_handle', tx + tw / 2 - 46, LEVER_Y - 15, 92, 30, 0.22, 0.56, 0.08, 0.02, mat_principled('handle', (0.02, 0.02, 0.022), 0.4, coat=0.5)); lever.append(hd)
    for i in range(5): lever.append(box('lv_rib', tx + tw / 2 - 30 + i * 15 - 2, LEVER_Y - 12, 4, 24, 0.24, 0.6, 0.01, 0.004, mat_principled('rib', (0.05, 0.05, 0.055), 0.5)))
    for o in lever: o.name = 'lever_' + o.name
    # bass mono: avionics knob (knurled black, concentric)
    bx_, by_, br = BASS; X, Y, _ = at(bx_, by_)
    k = cyl(X, Y, br * U, 0.22, 0.12, 120, None, 0.0)
    for v in k.data.vertices:
        a = math.atan2(v.co.y, v.co.x); n = int(round((a + math.pi) / (2 * math.pi) * 120)) % 2
        if n and abs(v.co.z) > -1: v.co.x *= 0.96; v.co.y *= 0.96
    k.data.materials.append(BLACK); cyl(X, Y, br * U * 0.6, 0.06, 0.25, 96, P.radial_brushed('kc', (0.5, 0.5, 0.52), 0.3), 0.01)
    cyl(X, Y, br * U * 1.4, 0.03, 0.0, 120, BLACK, 0.008)
    # annunciator: a square lit push button in a black housing; width read-out window
    ax, ay, aw, ah = ANN; box('annh', ax - 6, ay - 6, aw + 12, ah + 12, 0.08, 0.04, 0.03, 0.01, BLACK)
    box('annb', ax, ay, aw, ah, 0.06, 0.09, 0.02, 0.01, mat_principled('annglass', (0.06, 0.07, 0.065), 0.2, sss=0.4, coat=1.0))
    wx, wy, ww, wh = WREAD; window(plate, wx - ww / 2, wy - wh / 2, ww, wh, 0.1, GLASS); bezel_rect(wx - ww / 2, wy - wh / 2, ww, wh, 8, BLACK)
    # title placard
    px_, py_, pw, ph = TITLE; box('placard', px_ - pw / 2, py_ - ph / 2, pw, ph, 0.02, 0.01, 0.02, 0.004, BLACK)
    for (a_, b_) in ((px_ - pw / 2 + 8, py_), (px_ + pw / 2 - 8, py_)): SK.rivet(a_, b_, 0.025)
    # light: bright hangar key, cool fill, rim
    P.area(s, 'key', (-4, 6, 6), 5, 760)
    P.area(s, 'fill', (6, -5, 5), 6, 160, (0.75, 0.85, 1.0))
    P.area(s, 'rim', (0, 9, 1.2), 9, 380)
    P.area(s, 'kick', (5, 3, 2), 2, 220, (1.0, 0.92, 0.82))
    if LAYER == 'plate':
        for o in s.objects:
            if o.name.startswith('lever_'): o.visible_camera = False; o.visible_shadow = False
    elif LAYER == 'lever':
        s.render.film_transparent = True
        for o in s.objects:
            if o.type in ('MESH', 'CURVE', 'FONT') and not o.name.startswith('lever_'): o.visible_camera = False
    name = 'horizon' + ('_' + LAYER if LAYER else '') + '.png'
    s.render.filepath = os.path.join(OUTDIR, name); bpy.ops.render.render(write_still=True); print('rendered', name)

if __name__ == '__main__':
    build()
