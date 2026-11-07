# The liquid-tube meter: a channel strip (v2: chrome edge, glass name plate, a recessed channel behind the tube)
# and, on its own transparent layer, the glass tube's reflections, so the glowing liquid drawn live sits inside
# the glass. Mono (one fat tube) and stereo (two). Cycles.
# Usage: python3 render_meter.py <out dir> [scale]
import bpy, math, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import render_rack as R
from render_rack import P, U, box, knob, button, mat_principled
os.environ['LD_ONLY'] = 'strip'
import render_mix2 as M2

W, H = 112, 690
TUBES = { 'mono': [(80, 20)], 'stereo': [(71, 15), (90, 15)], 'bus': [(92, 19), (116, 19)] }   # x (left), width; y 306 .. 646

def capsule(name, x, w, y0, y1, mat):
    r = w / 2 * U
    cx, cy0, _ = R.at(x + w / 2, y1 - w / 2); _, cy1, _ = R.at(x + w / 2, y0 + w / 2)
    L = cy1 - cy0
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=r, depth=L, location=(cx, (cy0 + cy1) / 2, 0.03), rotation=(math.pi / 2, 0, 0))
    c = bpy.context.object; bpy.ops.object.shade_smooth(); c.data.materials.append(mat)
    for yy in (cy0, cy1):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=r, location=(cx, yy, 0.03))
        s = bpy.context.object; bpy.ops.object.shade_smooth(); s.data.materials.append(mat)

def scene():
    global W
    R.W, R.H = W, H
    s = P.reset(); s.cycles.samples = 96
    M2.cam(s, W, H); M2.mats()
    return s

def base(variant):
    global W
    W = 144 if variant == 'bus' else 112
    s = scene(); s.render.film_transparent = False
    box('cavity', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    plate = P.metal('anods', (0.07, 0.072, 0.08), 0.36, brushed=True, scale=28)
    box('body', 3, 3, W - 6, H - 6, 0.14, -0.07, 0.03, 0.01, plate)
    chrome = P.metal('chrome', (0.8, 0.81, 0.84), 0.12)
    for x in (3, W - 5): box('edge', x, 6, 2, H - 12, 0.02, 0.003, 0.006, 0.003, chrome)   # chrome edges
    glass = mat_principled('sglass', (0.004, 0.004, 0.006), 0.1, coat=1.0)
    for (x, y, w, h) in ([(8, 8, 128, 24), (14, 652, 66, 22)] if variant == 'bus' else [(8, 8, 96, 24), (14, 652, 50, 22)]):
        box('g', x, y, w, h, 0.012, 0.004, 0.02, 0.004, glass)
    slot = mat_principled('slot', (0.003, 0.003, 0.004), 0.6)
    box('slot', 44 if variant == 'bus' else 30, 306, 8, 340, 0.012, 0.004, 0.02, 0.003, slot)
    # the channel behind the tube(s): a dark, satin trough
    trough = mat_principled('trough', (0.01, 0.01, 0.012), 0.45)
    for (x, w) in TUBES[variant]: box('t', x - 3, 300, w + 6, 352, 0.02, -0.002, 0.03, 0.004, trough)
    if variant == 'bus':
        for i in range(5): button(8, 40 + i * 26, 128, 22)
        knob(40, 198, 12); knob(104, 198, 12); knob(72, 250, 14)
        button(14, 276, 56, 20); button(74, 276, 56, 20)
    else:
        for i in range(5): button(8, 40 + i * 26, 96, 22)
        knob(32, 198, 11); knob(80, 198, 11); knob(56, 250, 13)
        button(12, 276, 42, 20); button(58, 276, 42, 20)
    M2.lights(s)
    P.render(s, 'strip_' + variant + '.png')

def glass(variant):
    # the tube alone, on transparent: clear glass that only shows its reflections and edges
    s = scene(); s.render.film_transparent = True
    s.render.film_transparent = True
    m = bpy.data.materials.new('tube'); m.use_nodes = True; nt = m.node_tree
    for n in list(nt.nodes): nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial'); mix = nt.nodes.new('ShaderNodeMixShader')
    tr = nt.nodes.new('ShaderNodeBsdfTransparent'); gl = nt.nodes.new('ShaderNodeBsdfGlossy'); fr = nt.nodes.new('ShaderNodeLayerWeight')
    gl.inputs['Roughness'].default_value = 0.04; fr.inputs['Blend'].default_value = 0.22
    nt.links.new(fr.outputs['Fresnel'], mix.inputs['Fac']); nt.links.new(tr.outputs[0], mix.inputs[1]); nt.links.new(gl.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    m.blend_method = 'BLEND' if hasattr(m, 'blend_method') else None
    for (x, w) in TUBES[variant]: capsule('tube', x, w, 306, 646, m)
    # a long soft strip light down the side for the glass to catch, plus the rack lights
    M2.lights(s)
    # long thin lights either side of the tube, for the bright streaks a glass cylinder shows
    for (x, w) in TUBES[variant]:
        cx, _, _ = R.at(x + w / 2, 476)
        for side, e in ((-1, 90), (1, 35)):
            ld = bpy.data.lights.new('st', 'AREA'); ld.shape = 'RECTANGLE'; ld.size = 0.08; ld.size_y = 4.0; ld.energy = e
            lo = bpy.data.objects.new('st', ld); s.collection.objects.link(lo)
            lo.location = (cx + side * 0.35, 0, 0.45); lo.rotation_euler = (0, -side * math.radians(38), 0)
    # a soft grey world so the glass has something to reflect
    w = bpy.data.worlds.new('w'); s.world = w; w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.05, 0.055, 0.065, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    P.render(s, 'tube_' + variant + '.png')

def divider():
    # the line between the tracks and the drum bus: a machined rail with a chrome inlay and screws
    global W
    W = 24
    s = scene(); s.render.film_transparent = False
    box('cavity', 0, 0, W, H, 0.05, -0.2, 0.001, 0.001, mat_principled('cav', (0.006, 0.006, 0.008), 0.7))
    box('rail', 3, 3, 18, H - 6, 0.12, -0.06, 0.02, 0.01, P.metal('rail2', (0.09, 0.09, 0.1), 0.35, brushed=True, scale=10))
    box('inlay', 10, 20, 4, H - 40, 0.02, 0.006, 0.01, 0.004, P.metal('chrome', (0.8, 0.81, 0.84), 0.12))
    for y in (10, H - 10, H / 2): R.screw(12, y)
    M2.lights(s)
    P.render(s, 'divider.png')

for v in [x for x in os.environ.get('LD_VARIANTS', 'mono,stereo').split(',') if x]:
    if os.environ.get('LD_GLASS_ONLY') is None: base(v)
    if v != 'bus': glass(v)
if os.environ.get('LD_DIVIDER'): divider()
