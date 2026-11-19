"""Track crew models for MX Dirt Mod Alt (Blender, headless). GPL-3.0, part of the mod.

Writes a model the render plugin draws (src/render/mxdirt_crew_model.h): <out>/model.mxm plus one .tga per texture.
Game axes: metres, +Y up, +Z forward, +X right; origin at the middle of the footprint, on the ground.

  Convert a model you downloaded (FBX):
    blender -b --factory-startup --python tools/crew_export.py -- fbx <file.fbx> <out dir> <forward axis> <body objects> [wheel objects]
      forward axis: +X -X +Y -Y (Blender axes, the way the machine drives); objects: comma-separated names
  Build the mod's own low-poly models (no third-party content):
    blender -b --factory-startup --python tools/crew_export.py -- own <out root>
"""
import bpy, bmesh, sys, os, struct, math
from mathutils import Vector

MAX_TEX = 1024

def args():
    a = sys.argv[sys.argv.index('--') + 1:]
    return a

def axes(fwd):
    f = {'+X': Vector((1, 0, 0)), '-X': Vector((-1, 0, 0)), '+Y': Vector((0, 1, 0)), '-Y': Vector((0, -1, 0))}[fwd]
    up = Vector((0, 0, 1))
    right = up.cross(f)  # game X = Y x Z
    return right, up, f

def tri_data(obj, right, up, fwd):
    """World-space triangles of obj in game axes: list of (pos, nrm, uv) and the material image per triangle."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    mw = obj.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    uvl = me.uv_layers.active.data if me.uv_layers.active else None
    out = []
    for t in me.loop_triangles:
        img = None
        if obj.material_slots and t.material_index < len(obj.material_slots):
            mat = obj.material_slots[t.material_index].material
            if mat and mat.node_tree:
                for n in mat.node_tree.nodes:
                    if n.type == 'TEX_IMAGE' and n.image:
                        img = n.image
                        break
        vs = []
        for li in t.loops:
            lo = me.loops[li]
            p = mw @ me.vertices[lo.vertex_index].co
            n = (nm @ (lo.normal if t.use_smooth else t.normal)).normalized()
            uv = uvl[li].uv if uvl else (0.0, 0.0)
            vs.append(((p.dot(right), p.dot(up), p.dot(fwd)), (n.dot(right), n.dot(up), n.dot(fwd)), (uv[0], uv[1])))
        out.append((vs, img))
    ev.to_mesh_clear()
    return out

def wheel_groups(obj, right, up, fwd):
    """Split a wheels object into wheels: loose parts whose centres lie within 40 cm (tyre + rim) form one wheel (40 cm: dual tyres and hubs too)."""
    tris = tri_data(obj, right, up, fwd)
    # union-find over triangles sharing a position
    key = lambda p: (round(p[0], 4), round(p[1], 4), round(p[2], 4))
    parent = list(range(len(tris)))
    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    seen = {}
    for i, (vs, _) in enumerate(tris):
        for v in vs:
            k = key(v[0])
            if k in seen:
                a, b = find(i), find(seen[k])
                if a != b:
                    parent[a] = b
            else:
                seen[k] = i
    parts = {}
    for i in range(len(tris)):
        parts.setdefault(find(i), []).append(tris[i])
    groups = []  # [centre, tris]
    for ts in parts.values():
        pts = [v[0] for vs, _ in ts for v in vs]
        c = Vector([sum(p[j] for p in pts) / len(pts) for j in range(3)])
        for g in groups:
            if (Vector((g[0].x, g[0].y, g[0].z)) - c).length < .4:
                g[1].extend(ts)
                break
        else:
            groups.append([c, list(ts)])
    out = []
    for c, ts in groups:
        pts = [v[0] for vs, _ in ts for v in vs]
        ys = [p[1] for p in pts]
        zs = [p[2] for p in pts]
        mid = Vector(((min(p[0] for p in pts) + max(p[0] for p in pts)) / 2, (min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2))
        out.append((mid, (max(ys) - min(ys)) / 2, ts))
    return out

def save_tex(img, out_dir, used):
    if img is None:
        return ''
    name = ''.join(ch if ch.isalnum() else '_' for ch in os.path.splitext(img.name)[0])[:30] + '.tga'
    if name in used:
        return name
    im = img.copy()
    w, h = im.size
    s = min(1.0, MAX_TEX / max(w, h, 1))
    if s < 1.0:
        im.scale(max(1, int(w * s)), max(1, int(h * s)))
    im.filepath_raw = os.path.join(out_dir, name)
    im.file_format = 'TARGA_RAW'
    im.save()
    bpy.data.images.remove(im)
    used.add(name)
    return name

def write(out_dir, parts):
    """parts: list of dict(tex, flags, pivot, radius, tris) in game axes; recentred so the footprint middle is 0 and
    the lowest point is on the ground."""
    allp = [v[0] for p in parts for vs, _ in p['tris'] for v in vs]
    cx = (min(p[0] for p in allp) + max(p[0] for p in allp)) / 2
    cz = (min(p[2] for p in allp) + max(p[2] for p in allp)) / 2
    y0 = min(p[1] for p in allp)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'model.mxm'), 'wb') as f:
        f.write(b'MXM1')
        f.write(struct.pack('<I', len(parts)))
        for p in parts:
            tex = p['tex'].encode('ascii')[:39]
            piv = p['pivot']
            f.write(tex + b'\0' * (40 - len(tex)))
            f.write(struct.pack('<I4fI', p['flags'], piv[0] - cx, piv[1] - y0, piv[2] - cz, p['radius'], 3 * len(p['tris'])))
            for vs, _ in p['tris']:
                for pos, n, uv in vs:
                    f.write(struct.pack('<8f', pos[0] - cx, pos[1] - y0, pos[2] - cz, n[0], n[1], n[2], uv[0], uv[1]))
    tris = sum(len(p['tris']) for p in parts)
    print('MXM', out_dir, 'parts', len(parts), 'tris', tris,
          'size', round(max(p[0] for p in allp) - min(p[0] for p in allp), 2), round(max(p[1] for p in allp) - y0, 2),
          round(max(p[2] for p in allp) - min(p[2] for p in allp), 2))

def split_by_image(tris, out_dir, used, flags=0, pivot=(0, 0, 0), radius=0.0):
    by = {}
    for t in tris:
        by.setdefault(t[1].name if t[1] else '', []).append(t)
    parts = []
    for ts in by.values():
        parts.append({'tex': save_tex(ts[0][1], out_dir, used), 'flags': flags, 'pivot': pivot, 'radius': radius, 'tris': ts})
    return parts

def convert(fbx, out_dir, fwd, body, wheels):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    right, up, f = axes(fwd)
    used = set()
    parts = []
    for name in body:
        parts += split_by_image(tri_data(bpy.data.objects[name], right, up, f), out_dir, used)
    for name in wheels:
        for mid, r, ts in wheel_groups(bpy.data.objects[name], right, up, f):
            parts += split_by_image(ts, out_dir, used, 1, tuple(mid), r)
    write(out_dir, parts)

# ---------------- the mod's own models ----------------
PALETTE = {'yellow': (.86, .64, .10), 'dark': (.10, .10, .11), 'steel': (.42, .43, .45), 'glass': (.16, .22, .26),
           'tyre': (.07, .07, .07), 'rim': (.55, .55, .52), 'white': (.85, .85, .82), 'tank': (.62, .64, .66),
           'red': (.62, .12, .10), 'blade': (.30, .30, .31), 'blue': (.18, .30, .55), 'light': (.95, .85, .45)}

def palette_image(out_dir):
    names = list(PALETTE)
    n = 4
    img = bpy.data.images.new('crew_palette', 16 * n, 16 * n, alpha=False)
    px = [0.0] * (16 * n * 16 * n * 4)
    uv = {}
    for i, k in enumerate(names):
        cx, cy = i % n, i // n
        c = PALETTE[k]
        for y in range(16):
            for x in range(16):
                j = ((cy * 16 + y) * 16 * n + cx * 16 + x) * 4
                g = .92 + .08 * ((x * 7 + y * 3) % 5) / 4  # a little grain
                px[j:j + 4] = [c[0] * g, c[1] * g, c[2] * g, 1.0]
        uv[k] = ((cx + .5) / n, (cy + .5) / n)
    img.pixels = px
    img.filepath_raw = os.path.join(out_dir, 'palette.tga')
    img.file_format = 'TARGA_RAW'
    img.save()
    return uv

class Builder:
    """Boxes and cylinders in game axes (x right, y up, z forward), each face on a palette swatch."""
    def __init__(self, uv):
        self.uv = uv
        self.body = []
        self.wheels = []

    def quad(self, out, a, b, c, d, col):
        n = (Vector(b) - Vector(a)).cross(Vector(c) - Vector(a)).normalized()
        u = self.uv[col]
        for tri in ((a, b, c), (a, c, d)):
            out.append(([(tuple(p), tuple(n), u) for p in tri], None))

    def box(self, x0, x1, y0, y1, z0, z1, col, out=None, top=None):
        out = self.body if out is None else out
        P = lambda x, y, z: (x, y, z)
        q = self.quad
        q(out, P(x0, y0, z1), P(x1, y0, z1), P(x1, y1, z1), P(x0, y1, z1), col)  # front (+z)
        q(out, P(x1, y0, z0), P(x0, y0, z0), P(x0, y1, z0), P(x1, y1, z0), col)  # back
        q(out, P(x1, y0, z1), P(x1, y0, z0), P(x1, y1, z0), P(x1, y1, z1), col)  # right
        q(out, P(x0, y0, z0), P(x0, y0, z1), P(x0, y1, z1), P(x0, y1, z0), col)  # left
        q(out, P(x0, y1, z1), P(x1, y1, z1), P(x1, y1, z0), P(x0, y1, z0), top or col)  # top
        q(out, P(x0, y0, z0), P(x1, y0, z0), P(x1, y0, z1), P(x0, y0, z1), col)  # bottom

    def cyl_z(self, x, y, z0, z1, r, col, seg=14):
        """a tank lying along z"""
        out = self.body
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            p = lambda a, z: (x + r * math.cos(a), y + r * math.sin(a), z)
            self.quad(out, p(a0, z0), p(a1, z0), p(a1, z1), p(a0, z1), col)
            self.quad(out, (x, y, z1), p(a0, z1), p(a1, z1), (x, y, z1), col)
            self.quad(out, (x, y, z0), p(a1, z0), p(a0, z0), (x, y, z0), col)

    def wheel(self, x, z, r, w, seg=16):
        """a wheel on the x axis: tyre, rim disc, both sides"""
        out = []
        y = r
        side = 1 if x > 0 else -1
        xi, xo = x - side * w / 2, x + side * w / 2
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            p = lambda xx, a, rr: (xx, y + rr * math.sin(a), z + rr * math.cos(a))
            self.quad(out, p(xi, a0, r), p(xi, a1, r), p(xo, a1, r), p(xo, a0, r), 'tyre')
            for xx, s in ((xo, 1), (xi, -1)):
                rim = 'rim' if xx == xo else 'tyre'
                if (s > 0) == (side > 0):
                    self.quad(out, (xx, y, z), p(xx, a0, r * .6), p(xx, a1, r * .6), (xx, y, z), rim)
                    self.quad(out, p(xx, a0, r * .6), p(xx, a0, r), p(xx, a1, r), p(xx, a1, r * .6), 'tyre')
                else:
                    self.quad(out, (xx, y, z), p(xx, a1, r), p(xx, a0, r), (xx, y, z), 'tyre')
        self.wheels.append(((x, y, z), r, out))

def build_own(root):
    os.makedirs(root, exist_ok=True)
    # grader: a motor grader - long frame, rear engine and cab, blade under the middle, 6 wheels
    for name in ('grader', 'truck'):
        out_dir = os.path.join(root, name)
        os.makedirs(out_dir, exist_ok=True)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        uv = palette_image(out_dir)
        b = Builder(uv)
        if name == 'grader':
            b.box(-.35, .35, .75, 1.05, -.2, 4.4, 'yellow')          # main beam to the front axle
            b.box(-1.05, 1.05, .55, 1.45, -3.1, -.6, 'yellow', top='dark')  # engine
            b.box(-.85, .85, 1.45, 2.95, -1.55, -.35, 'yellow')    # cab
            b.box(-.80, .80, 1.75, 2.75, -.36, -.30, 'glass')      # windscreen
            b.box(-.86, -.80, 1.75, 2.75, -1.45, -.45, 'glass')
            b.box(.80, .86, 1.75, 2.75, -1.45, -.45, 'glass')
            b.box(-.95, .95, 2.95, 3.05, -1.65, -.25, 'dark')      # roof
            b.box(-.15, .15, 1.45, 2.3, -2.9, -2.75, 'dark')       # exhaust
            b.box(-1.9, 1.9, .05, .65, 1.15, 1.35, 'blade')        # moldboard
            b.box(-.25, .25, .65, .80, .2, 2.4, 'yellow')          # drawbar
            b.box(-.55, .55, .35, .75, 4.2, 4.6, 'yellow')         # front axle
            b.box(-.2, .2, 2.95, 3.12, -1.0, -.8, 'light')         # beacon
            for x in (-1.15, 1.15):
                b.wheel(x, 4.4, .62, .45)
                b.wheel(x, -1.95, .62, .45)
                b.wheel(x, -3.35, .62, .45)
        else:
            b.box(-1.05, 1.05, .55, .85, -3.6, 2.6, 'dark')         # chassis
            b.box(-1.15, 1.15, .85, 2.6, 1.7, 3.3, 'white')         # cab
            b.box(-1.0, 1.0, 1.75, 2.4, 3.29, 3.33, 'glass')        # windscreen
            b.box(-1.16, -1.14, 1.75, 2.35, 2.1, 3.1, 'glass')
            b.box(1.14, 1.16, 1.75, 2.35, 2.1, 3.1, 'glass')
            b.box(-1.15, 1.15, .7, .85, 3.3, 3.45, 'dark')          # bumper
            b.cyl_z(0, 1.75, -3.5, 1.45, .95, 'tank')               # tank
            b.box(-.25, .25, 2.7, 2.85, -1.6, -.6, 'steel')         # hatch
            b.box(-1.2, 1.2, .55, .7, -3.75, -3.6, 'steel')         # spray bar
            b.box(-.15, .15, 2.6, 2.75, 2.2, 2.4, 'light')
            for x in (-1.05, 1.05):
                b.wheel(x, 2.5, .5, .35)
                b.wheel(x, -1.6, .5, .35)
                b.wheel(x, -2.75, .5, .35)
        parts = [{'tex': 'palette.tga', 'flags': 0, 'pivot': (0, 0, 0), 'radius': 0, 'tris': b.body}]
        for c, r, ts in b.wheels:
            parts.append({'tex': 'palette.tga', 'flags': 1, 'pivot': c, 'radius': r, 'tris': ts})
        write(out_dir, parts)

a = args()
if a[0] == 'fbx':
    convert(a[1], a[2], a[3], a[4].split(','), a[5].split(',') if len(a) > 5 and a[5] else [])
elif a[0] == 'own':
    build_own(a[1])
