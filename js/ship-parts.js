/* ship-parts.js - procedural parts kit for NO MANS GOR (hulls, creatures, bosses, weapons, outposts, humans).

   ONE FAMILY: flat-shaded, vertex-coloured, faceted. Every part is a pure function that returns merged geometry
   in its own local frame. Lit geometry and emissive geometry are always separate (two meshes per model).

   API
   ---
   PARTS[name](THREE, params, rng) -> { geo, emissive|null, sockets, tris }
     geo / emissive : non-indexed BufferGeometry with attributes position, normal, color, aFx
                      aFx = (flex weight 0..1, phase, fin-flap flag) - read by the flex shader, ignored otherwise
     sockets        : { name: { pos: Vector3, dir: Vector3, hint?: Vector3 } } attachment points in part space
     tris           : triangle count (lit + emissive)
     params.lod     : 'hi' (<= 400 tris) | 'lo' (<= 120 tris). Missing = 'hi'
     params.pal / base / panel / accent / glow / dark / metal : hex colours (part-level keys override pal)
   Local frame convention: part grows along +Y (outward from its mount), chord/length along Z (nose = -Z),
   thickness along X. A socket's `dir` becomes the child's +Y; the child's Z is the socket `hint`
   (default world Z, X when dir is parallel to Z) projected perpendicular to dir.

   Parts: fin wing spike plate pod antenna engine eye tentacle claw shell dome strut canopy leg
          barrel block tank tower hull spine pad ring slot
          (hull = fuselage 'dart'|'brick'|'disc', length 1, nose -Z; spine = creature body chain;
           pad = outpost platform)

   compose(THREE, recipe, rng) -> { geo, emissive|null, sockets, tris, parts:[{id,part,tris}] }
     recipe node = { part, params?, at?: socketName | Matrix4, offset?:[x,y,z], rot?:[rx,ry,rz], roll?, scale?: n|[x,y,z],
                     mirror?: 'x', id?, children?: [node...] }
     Root node may also carry: palette:{base,panel,accent,glow,dark,metal}, lod:'hi'|'lo',
                               paint:{ lines, stripes, grime }  (post-pass over the merged lit geometry)
     `at` names a socket on the PARENT part. mirror:'x' also emits the whole subtree reflected through world x=0
     (mirror flags inside an already-mirrored pass are ignored). Result is exactly two geometries (lit + emissive).
     Result sockets: root sockets by name, descendants as "<id>.<socket>" (mirrored instance: "<id>M.<socket>").

   makeMaterials(THREE, { flex }) -> { lit, emissive, glass|null, cloneMaterial(m), tick(t) }   shared per options (cached). flex=true is the
     one creature+boss shader (aFx/aPiv/aLm, uSw* limb uniforms); flex=false the static hull/weapon shader. cloneMaterial gives a per-creature
     instance (own uniforms, same GL program). compose() recipe flag flex:false strips aFx/aPiv/aLm for static models.
   paint(geo, { base, accent, panel, rng, grime, lines, stripes }) -> geo   seeded panel lines / accent stripes / grime
     per triangle on any geometry (colours kept if the geometry already has a colour attribute). Use the return value.
   seededRecipe(kind, rng) -> recipe, kind in fighter hauler explorer creature boss-claw outpost gunpod
   seededPalette(rng, kind) -> palette ; mulberry(seed) -> rng ; demo(engine, opts) -> handle (dev preview grid)
*/
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

export function mulberry(a) {
  return function () {
    a |= 0; a = a + 0x6D2B79F5 | 0;
    let t = Math.imul(a ^ a >>> 15, 1 | a);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  };
}
const rn = (r, a, b) => a + (b - a) * r();
const pick = (r, arr) => arr[Math.floor(r() * arr.length) % arr.length];
const clamp = (x, a, b) => x < a ? a : x > b ? b : x;
const hash3 = (x, y, z) => { const s = Math.sin(Math.round(x * 90) * 12.9898 + Math.round(y * 90) * 78.233 + Math.round(z * 90) * 37.719) * 43758.5453; return s - Math.floor(s); };
const V = (T, x, y, z) => new T.Vector3(x, y, z);

// ---------------------------------------------------------------- geometry helpers
function ringPts(T, n, rx, rz, y, cx = 0, cz = 0, rot = 0) {
  const a = [];
  for (let i = 0; i < n; i++) { const t = rot + i / n * Math.PI * 2; a.push(V(T, cx + Math.cos(t) * rx, y, cz + Math.sin(t) * rz)); }
  return a;
}
// loft between equal-length rings. Rings that collapse to a point become fans. caps = centroid fans.
function loft(T, rings, capA, capB) {
  const pos = [], n = rings[0].length;
  const tri = (a, b, c) => pos.push(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
  const degen = r => { for (let k = 1; k < r.length; k++) if (r[k].distanceToSquared(r[0]) > 1e-10) return false; return true; };
  for (let i = 0; i < rings.length - 1; i++) {
    const A = rings[i], B = rings[i + 1], dA = degen(A), dB = degen(B);
    for (let k = 0; k < n; k++) {
      const a = A[k], b = A[(k + 1) % n], c = B[(k + 1) % n], d = B[k];
      if (dA && dB) continue;
      if (dA) tri(a, c, d); else if (dB) tri(a, b, c); else { tri(a, b, c); tri(a, c, d); }
    }
  }
  const cap = ring => {
    if (degen(ring)) return;
    const c = new T.Vector3(); ring.forEach(q => c.add(q)); c.multiplyScalar(1 / ring.length);
    for (let k = 0; k < ring.length; k++) tri(c, ring[k], ring[(k + 1) % ring.length]);
  };
  if (capA) cap(rings[0]);
  if (capB) cap(rings[rings.length - 1]);
  const g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute(pos, 3));
  return g;
}
const cyl = (T, rb, rt, h, seg) => { const g = new T.CylinderGeometry(rt, rb, h, seg); g.translate(0, h / 2, 0); return g; };
const ball = (T, rx, ry, rz, ws, hs) => { const g = new T.SphereGeometry(1, ws, hs); g.scale(rx, ry, rz); return g; };
const boxG = (T, w, h, d) => { const g = new T.BoxGeometry(w, h, d); g.translate(0, h / 2, 0); return g; };
const boxC = (T, w, h, d) => new T.BoxGeometry(w, h, d);
function rodG(T, a, b, r, seg) {
  const d = new T.Vector3().subVectors(b, a), len = Math.max(d.length(), 1e-5);
  const g = cyl(T, r, r, len, seg);
  g.applyQuaternion(new T.Quaternion().setFromUnitVectors(V(T, 0, 1, 0), d.multiplyScalar(1 / len)));
  g.translate(a.x, a.y, a.z); return g;
}
const M4 = (T, pos, rot, scl) => {
  const m = new T.Matrix4();
  m.compose(V(T, pos[0], pos[1], pos[2]), new T.Quaternion().setFromEuler(new T.Euler(rot ? rot[0] : 0, rot ? rot[1] : 0, rot ? rot[2] : 0)),
    scl ? (typeof scl === 'number' ? V(T, scl, scl, scl) : V(T, scl[0], scl[1], scl[2])) : V(T, 1, 1, 1));
  return m;
};

// ---------------------------------------------------------------- builder
function colorsOf(T, p) {
  const pal = p.pal || {};
  const g = (k, d) => new T.Color(p[k] !== undefined ? p[k] : pal[k] !== undefined ? pal[k] : d);
  return { base: g('base', 0x8E3A9C), panel: g('panel', 0x5E2470), accent: g('accent', 0xF05A28), glow: g('glow', 0xE8F6FF), dark: g('dark', 0x2A2430), metal: g('metal', 0x8A8691) };
}
function mkB(T, p, r) {
  const b = { T, p, r, lo: p.lod === 'lo', lit: [], emi: [], sockets: {}, c: colorsOf(T, p), ph: r() * 6.283 };
  const tmp = new T.Color();
  b.add = (geo, col, o = {}) => {
    const g = geo.index ? geo.toNonIndexed() : geo;
    if (o.m) g.applyMatrix4(o.m);
    g.deleteAttribute('uv'); g.deleteAttribute('normal'); g.computeVertexNormals();
    const P = g.attributes.position, n = P.count, C = new Float32Array(n * 3), F = new Float32Array(n * 4), PV = new Float32Array(n * 3), LM = new Float32Array(n * 2);
    const base = col.isColor ? col : tmp.set(col), bc = base.clone(), jit = o.jit === undefined ? 0.14 : o.jit;
    for (let i = 0; i + 2 < n; i += 3) {
      const cx = (P.getX(i) + P.getX(i + 1) + P.getX(i + 2)) / 3, cy = (P.getY(i) + P.getY(i + 1) + P.getY(i + 2)) / 3, cz = (P.getZ(i) + P.getZ(i + 1) + P.getZ(i + 2)) / 3;
      const t = bc.clone(); if (o.cf) o.cf(t, cx, cy, cz);
      const f = o.emi ? 1 : 1 - jit * hash3(cx, cy, cz);
      for (let k = 0; k < 3; k++) {
        const j = (i + k) * 3;
        C[j] = t.r * f; C[j + 1] = t.g * f; C[j + 2] = t.b * f;
        const j4 = (i + k) * 4;
        F[j4] = typeof o.fxw === 'function' ? o.fxw(P.getX(i + k), P.getY(i + k), P.getZ(i + k)) : (o.fxw || 0);
        F[j4 + 1] = b.ph; F[j4 + 2] = o.flap ? 5 : 0;
      }
    }
    g.setAttribute('color', new T.BufferAttribute(C, 3)); g.setAttribute('aFx', new T.BufferAttribute(F, 4)); g.setAttribute('aPiv', new T.BufferAttribute(PV, 3)); g.setAttribute('aLm', new T.BufferAttribute(LM, 2));
    (o.emi ? b.emi : b.lit).push(g);
  };
  b.glow = (geo, o = {}) => b.add(geo, o.col || b.c.glow, Object.assign({ emi: true }, o));
  b.sock = (name, pos, dir, hint) => {
    b.sockets[name] = { pos: V(T, pos[0], pos[1], pos[2]), dir: V(T, dir[0], dir[1], dir[2]).normalize() };
    if (hint) b.sockets[name].hint = V(T, hint[0], hint[1], hint[2]).normalize();
  };
  b.done = () => {
    const geo = mergeGeometries(b.lit, false);
    const emissive = b.emi.length ? mergeGeometries(b.emi, false) : null;
    const tris = (geo.attributes.position.count + (emissive ? emissive.attributes.position.count : 0)) / 3;
    return { geo, emissive, sockets: b.sockets, tris };
  };
  return b;
}

// ---------------------------------------------------------------- parts
function fin(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.22, w = p.w ?? 0.2, sw = p.sweep ?? 0.5, th = p.thick ?? 0.02, tp = p.taper ?? 0.35;
  const n = L ? 2 : 4;
  const prof = t => {
    const y = t * len, ch = w * (1 - (1 - tp) * t), o = sw * len * t, zf = -ch / 2 + o, zb = ch / 2 + o, zm = zf + 0.38 * (zb - zf);
    const tk = th * (1 - 0.55 * t) * (1 + 0.5 * Math.sin(Math.PI * t));
    return { zf, zb, zm, tk, y, ch };
  };
  const rings = [];
  for (let i = 0; i < n; i++) { const q = prof(i / (n - 1)); rings.push([V(T, 0, q.y, q.zf), V(T, q.tk, q.y, q.zm), V(T, 0, q.y, q.zb), V(T, -q.tk, q.y, q.zm)]); }
  const tipC = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  b.add(loft(T, rings, true, true), c.base, { flap: !!p.flap, cf: (col, x, y) => { const t = y / len; if (t > 0.72) col.lerp(tipC, (t - 0.72) / 0.28 * 0.85); } });
  if (!L) {
    const lr = [];
    for (let i = 0; i < 3; i++) { const q = prof(i / 2 * 0.95); lr.push([V(T, 0, q.y, q.zf - 0.004), V(T, q.tk * 1.15, q.y, q.zf + 0.2 * q.ch), V(T, -q.tk * 1.15, q.y, q.zf + 0.2 * q.ch)]); }
    b.add(loft(T, lr, true, true), c.panel, { flap: !!p.flap, jit: 0.05 });
    b.add(boxG(T, th * 2.6, len * 0.14, w * 0.7), c.panel, { m: M4(T, [0, 0, sw * len * 0.05]), jit: 0.1 });
    if (p.glow) b.glow(boxC(T, th * 2.8, len * 0.05, w * 0.35), { m: M4(T, [0, len * 0.5, sw * len * 0.5]) });
  }
  const q1 = prof(1);
  b.sock('tip', [0, len, sw * len], [0, 1, 0]);
  b.sock('tipFwd', [0, len, q1.zf + sw * 0 ], [0, 0, -1]);
  b.sock('mid', [0, len * 0.5, sw * len * 0.5], [0, 1, 0]);
  return b.done();
}
function wing(T, p, r) {
  return fin(T, Object.assign({ len: 0.4, w: 0.3, sweep: 0.55, thick: 0.022, taper: 0.35, flap: false }, p), r);
}
function spike(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.14, R = p.r ?? 0.03, bend = p.bend ?? 0, seg = L ? 4 : 6;
  const lv = L ? [0, 1] : [0, 0.4, 0.75, 1];
  const rings = lv.map(t => ringPts(T, seg, R * (t >= 1 ? 0 : 1 - t * 0.75), R * (t >= 1 ? 0 : 1 - t * 0.75), t * len, bend * len * t * t, 0));
  const tipC = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  b.add(loft(T, rings, true, false), c.base, { cf: (col, x, y) => { const t = y / len; if (t > 0.45) col.lerp(tipC, (t - 0.45) * 1.4); } });
  if (!L) b.add(cyl(T, R * 1.35, R * 1.15, len * 0.14, seg), c.panel, { jit: 0.08 });
  b.sock('tip', [bend * len, len, 0], [0, 1, 0]);
  return b.done();
}
function plate(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const w = p.w ?? 0.2, d = p.d ?? 0.26, th = p.th ?? 0.025, ins = th * 1.4;
  const rect = (hw, hd, y) => [V(T, -hw, y, -hd), V(T, hw, y, -hd), V(T, hw, y, hd), V(T, -hw, y, hd)];
  b.add(loft(T, [rect(w / 2, d / 2, 0), rect(w / 2 - ins, d / 2 - ins, th)], true, true), c.base, { jit: 0.18 });
  if (!L) {
    b.add(loft(T, [rect(w / 2 - ins * 1.3, d / 2 - ins * 1.3, th), rect(w / 2 - ins * 1.9, d / 2 - ins * 1.9, th * 1.35)], false, true), c.panel, { jit: 0.1 });
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) b.add(boxG(T, th * 0.5, th * 0.55, th * 0.5), c.metal, { m: M4(T, [sx * (w / 2 - ins * 0.7), th, sz * (d / 2 - ins * 0.7)]), jit: 0.1 });
    if (p.glow) b.glow(boxC(T, w * 0.5, th * 0.25, th * 0.35), { m: M4(T, [0, th * 1.4, d * 0.28]) });
  }
  b.sock('top', [0, th * 1.35, 0], [0, 1, 0]);
  b.sock('edge', [w / 2, th * 0.5, 0], [1, 0, 0]);
  return b.done();
}
function pod(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.2, R = p.r ?? 0.05, seg = L ? 5 : 8;
  const prof = L ? [[0, 0.7], [0.5, 1], [1, 0]] : [[0, 0.55], [0.12, 0.92], [0.35, 1], [0.65, 1], [0.88, 0.8], [1, 0]];
  const rings = prof.map(([t, k]) => ringPts(T, seg, R * k, R * k, t * len));
  b.add(loft(T, rings, true, false), c.base, { cf: (col, x, y) => { if (y / len > 0.78) col.lerp(c.panel, 0.5); } });
  if (!L) {
    for (const t of [0.22, 0.7]) b.add(cyl(T, R * 1.08, R * 1.08, len * 0.06, seg), c.panel, { m: M4(T, [0, t * len, 0]), jit: 0.08 });
    b.glow(boxC(T, R * 0.12, len * 0.22, R * 0.5), { m: M4(T, [R * 0.97, len * 0.5, 0]), col: p.glowCol });
    b.add(boxG(T, R * 0.9, len * 0.05, R * 0.9), c.accent, { m: M4(T, [0, len * 0.92, 0], [0, Math.PI / 4, 0]), jit: 0.1 });
  }
  b.sock('tip', [0, len, 0], [0, 1, 0]);
  b.sock('base', [0, 0, 0], [0, -1, 0]);
  b.sock('side', [R, len * 0.5, 0], [1, 0, 0]);
  return b.done();
}
function antenna(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.2, R = p.r ?? 0.006;
  b.add(cyl(T, R * 1.8, R, len, L ? 4 : 5), c.metal, { jit: 0.1 });
  if (!L) {
    for (const [t, k] of [[0.55, 1], [0.78, 0.6]]) b.add(boxC(T, len * 0.28 * k, R * 1.2, R * 1.2), c.metal, { m: M4(T, [0, len * t, 0]), jit: 0.05 });
    if (p.dish) b.add(loft(T, [ringPts(T, 6, 0, 0, len * 0.9), ringPts(T, 6, len * 0.13, len * 0.13, len * 0.96)], false, true), c.base, { m: M4(T, [0, 0, 0], [0.4, 0, 0]) });
    b.add(cyl(T, R * 2.6, R * 2.6, len * 0.04, 5), c.panel, { jit: 0.1 });
  }
  b.glow(ball(T, R * 2.4, R * 2.4, R * 2.4, 4, 3), { m: M4(T, [0, len, 0]), col: p.glowCol || c.accent });
  b.sock('tip', [0, len, 0], [0, 1, 0]);
  return b.done();
}
function engine(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.06, len = p.len ?? 0.16, seg = L ? 5 : 6;
  const ys = L ? [[0, 0.9], [0.6, 1.05], [1, 1.28]] : [[0, 0.92], [0.5, 1.05], [0.8, 0.9], [1, 1.3]];
  const rings = ys.map(([t, k]) => ringPts(T, seg, R * k, R * k, t * len));
  b.add(loft(T, rings, true, false), c.base, { cf: (col, x, y) => { if (y / len > 0.78) col.lerp(c.metal, 0.6); } });
  const gc = p.glowCol || c.glow;
  b.glow(new T.CircleGeometry(R * 1.02, seg).rotateX(-Math.PI / 2).translate(0, len * 0.97, 0), { col: gc });
  if (!L) {
    for (const t of [0.28, 0.62]) b.add(cyl(T, R * 1.12, R * 1.12, len * 0.07, seg), c.panel, { m: M4(T, [0, t * len, 0]), jit: 0.08 });
    for (let i = 0; i < 3; i++) { const a = i / 3 * 6.283 + 0.5; b.add(boxG(T, R * 0.22, len * 0.5, R * 0.5), c.dark, { m: M4(T, [Math.cos(a) * R * 1.0, len * 0.05, Math.sin(a) * R * 1.0], [0, -a, 0]), jit: 0.1 }); }
    b.add(cyl(T, R * 1.28, R * 1.34, len * 0.05, seg), c.accent, { m: M4(T, [0, len * 0.93, 0]), jit: 0.1 });
  }
  b.sock('exhaust', [0, len, 0], [0, 1, 0]);
  b.sock('base', [0, 0, 0], [0, -1, 0]);
  return b.done();
}
function eye(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.04, ws = L ? 5 : 8, hs = L ? 3 : 5;
  b.add(cyl(T, R * 1.25, R * 1.0, R * 0.55, ws), c.panel, { jit: 0.12 });
  const iris = p.glowCol || c.glow;
  b.glow(ball(T, R * 0.95, R * 0.8, R * 0.95, ws, hs), { m: M4(T, [0, R * 0.55, 0]), col: iris });
  b.add(ball(T, R * 0.34, R * 0.2, R * 0.34, L ? 4 : 5, 3), c.dark, { m: M4(T, [0, R * 1.25, 0]), jit: 0 });
  if (!L) b.add(cyl(T, R * 1.5, R * 1.28, R * 0.18, ws), c.accent, { jit: 0.1 });
  b.sock('front', [0, R * 1.3, 0], [0, 1, 0]);
  return b.done();
}
function tentacle(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.4, R = p.r ?? 0.035, n = L ? 4 : 8, seg = L ? 4 : 6;
  const bx = p.bend ?? rn(r, -0.4, 0.4) * len, bz = rn(r, -0.2, 0.2) * len;
  const rings = [], pts = [];
  for (let i = 0; i <= n; i++) {
    const t = i / n, rad = i === n ? 0 : R * (1 - 0.88 * t) * (1 + 0.15 * Math.sin(t * 9));
    const cx = bx * t * t + Math.sin(t * 5) * len * 0.05, cz = bz * t * t;
    pts.push([cx, t * len, cz]);
    rings.push(ringPts(T, seg, rad, rad, t * len, cx, cz));
  }
  const tc = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  b.add(loft(T, rings, true, false), c.base, { fxw: (x, y) => clamp(y / len, 0, 1), cf: (col, x, y) => { const t = y / len; col.lerp(c.panel, 0.4 * (1 - t)); if (t > 0.85) col.lerp(tc, 0.7); } });
  if (!L) for (let i = 1; i < n - 1; i += 2) {
    const q = pts[i], t = i / n, rad = R * (1 - 0.88 * t);
    b.glow(boxC(T, rad * 0.5, rad * 0.5, rad * 0.5), { m: M4(T, [q[0], q[1], q[2] + rad * 0.95]), col: tc, fxw: t });
  }
  const e = pts[n], f = pts[n - 1];
  b.sock('tip', e, [e[0] - f[0], e[1] - f[1], e[2] - f[2]]);
  return b.done();
}
function claw(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.07, open = p.open ?? 0.5, len = p.len ?? R * 3.4;
  b.add(cyl(T, R * 0.55, R * 0.7, R * 0.9, L ? 5 : 6), c.panel, { jit: 0.1 });
  b.add(ball(T, R * 0.95, R * 0.75, R * 0.7, L ? 5 : 6, L ? 3 : 4), c.base, { m: M4(T, [0, R * 1.2, 0]) });
  const steps = L ? 2 : 4;
  for (const s of [-1, 1]) {
    const rings = [];
    for (let i = 0; i <= steps; i++) {
      const t = i / steps, a = open * s * (1 - t * 1.5), w = R * 0.62 * (1 - t) + 0.001;
      const cx = s * R * 0.35 + Math.sin(open * s) * len * t * 0.3 + s * Math.sin(t * Math.PI) * R * 0.5 * (open + 0.3) - s * t * t * R * 0.9;
      const cy = R * 1.7 + t * len;
      const rr = i === steps ? 0 : w;
      rings.push([V(T, cx, cy, -rr), V(T, cx + s * rr * 0.9, cy, 0), V(T, cx, cy, rr), V(T, cx - s * rr * 0.9, cy, 0)]);
    }
    b.add(loft(T, rings, true, false), c.base, { cf: (col, x, y) => { if (y > R * 1.7 + len * 0.65) col.lerp(c.accent, 0.8); } });
  }
  b.sock('tip', [0, R * 1.7 + len * 0.85, 0], [0, 1, 0]);
  b.sock('wrist', [0, 0, 0], [0, -1, 0]);
  return b.done();
}
function shell(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const rx = p.rx ?? 0.2, ry = p.ry ?? 0.1, rz = p.rz ?? 0.28, ws = L ? 6 : 9, hs = L ? 2 : 4;
  const g = new T.SphereGeometry(1, ws, hs, 0, Math.PI * 2, 0, Math.PI / 2); g.scale(rx, ry, rz);
  b.add(g, c.base, { cf: (col, x, y, z) => { const band = Math.floor((z / rz + 1) * 3.2); if (band % 2) col.lerp(c.panel, 0.55); if (y > ry * 0.8) col.lerp(c.accent, 0.35); } });
  if (!L) {
    b.add(cyl(T, 1, 1, ry * 0.18, ws).scale(rx * 1.04, 1, rz * 1.04), c.panel, { m: M4(T, [0, -ry * 0.05, 0]), jit: 0.1 });
    for (let i = 0; i < 3; i++) b.add(boxG(T, rx * 0.12, ry * 0.2, rz * 0.12), c.dark, { m: M4(T, [0, ry * (0.96 - i * 0.06), (i - 1) * rz * 0.45], [0.25 * (i - 1), 0, 0]) });
  }
  b.sock('top', [0, ry, 0], [0, 1, 0]);
  b.sock('rim', [rx, 0, 0], [1, 0, 0]);
  return b.done();
}
function dome(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.1, h = p.h ?? R * 0.8, ws = L ? 6 : 10, hs = L ? 2 : 4;
  const g = new T.SphereGeometry(1, ws, hs, 0, Math.PI * 2, 0, Math.PI / 2); g.scale(R, h, R);
  const glass = new T.Color(p.glass !== undefined ? p.glass : c.dark).lerp(c.base, 0.25);
  b.add(g, glass, { jit: 0.08, cf: (col, x, y) => col.lerp(c.glow, clamp(y / h - 0.3, 0, 1) * 0.35) });
  b.add(cyl(T, R * 1.1, R * 1.04, h * 0.14, ws), c.panel, { jit: 0.1 });
  if (!L) {
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI; b.add(boxC(T, R * 2.02, R * 0.025, R * 0.025), c.metal, { m: M4(T, [0, h * 0.5, 0], [0, a, 0]) }); }
    b.glow(boxC(T, R * 0.3, h * 0.04, R * 0.3), { m: M4(T, [0, h * 0.97, 0]), col: p.glowCol || c.accent });
  }
  b.sock('top', [0, h, 0], [0, 1, 0]);
  return b.done();
}
function strut(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const to = p.to || [0, 0.2, 0], R = p.r ?? 0.012, a = V(T, 0, 0, 0), e = V(T, to[0], to[1], to[2]);
  b.add(rodG(T, a, e, R, L ? 4 : 5), c.metal, { jit: 0.12 });
  if (!L) {
    const d = e.clone().normalize(), off = V(T, 0, 0, R * 2.4);
    b.add(rodG(T, off.clone().addScaledVector(d, e.length() * 0.2), off.clone().addScaledVector(d, e.length() * 0.75), R * 0.55, 4), c.dark, { jit: 0.1 });
    b.add(ball(T, R * 1.9, R * 1.9, R * 1.9, 5, 3), c.panel, { m: M4(T, [0, 0, 0]) });
    b.add(ball(T, R * 1.9, R * 1.9, R * 1.9, 5, 3), c.panel, { m: M4(T, to) });
  }
  b.sock('end', to, to, [0, 0, 1]);
  const mid = [to[0] / 2, to[1] / 2, to[2] / 2];
  b.sock('mid', mid, to, [0, 0, 1]);
  return b.done();
}
function canopy(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.26, hw = p.wid ?? 0.1, h = p.h ?? 0.08;
  const S = L ? [[-0.5, 0.35, 0.3], [-0.15, 1, 1], [0.5, 0.9, 0.3]] : [[-0.5, 0.3, 0.25], [-0.32, 0.72, 0.78], [-0.05, 1, 1], [0.25, 1, 0.8], [0.5, 0.85, 0.2]];
  const rings = S.map(([t, w, k]) => [V(T, -hw * w, 0, t * len), V(T, hw * w, 0, t * len), V(T, hw * w * 0.7, h * k, t * len), V(T, -hw * w * 0.7, h * k, t * len)]);
  const gk = p.tintK ?? 1, glass = new T.Color(p.glass !== undefined ? p.glass : c.dark).lerp(c.glow, 0.12 * gk);
  b.add(loft(T, rings, true, true), glass, { jit: 0.08, cf: (col, x, y) => col.lerp(c.glow, clamp(y / h, 0, 1) * 0.25 * gk) });
  b.add(boxG(T, hw * 2.3, h * 0.12, len * 0.96), c.panel, { m: M4(T, [0, -h * 0.1, 0]), jit: 0.1 });
  if (!L) {
    for (const t of [-0.26, 0.06, 0.28]) b.add(boxC(T, hw * 1.7, h * 0.09, len * 0.03), c.metal, { m: M4(T, [0, h * 0.82, t * len]), jit: 0.05 });
    b.add(boxC(T, hw * 0.12, h * 0.1, len * 0.7), c.metal, { m: M4(T, [0, h * 1.0, 0.05 * len]), jit: 0.05 });
    b.glow(boxC(T, hw * 1.0, h * 0.06, len * 0.04), { m: M4(T, [0, h * 0.1, -0.5 * len]), col: p.glowCol || c.glow });
  }
  b.sock('top', [0, h, 0], [0, 1, 0]);
  return b.done();
}
function leg(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.16, R = p.r ?? 0.014, kx = p.knee ?? 0.05, seg = L ? 4 : 5;
  const k = V(T, kx, len * 0.45, 0), f = V(T, kx * 0.4, len, 0);
  b.add(rodG(T, V(T, 0, 0, 0), k, R, seg), c.metal, { jit: 0.1 });
  b.add(rodG(T, k, f, R * 0.85, seg), c.dark, { jit: 0.1 });
  b.add(cyl(T, R * 3.2, R * 2.5, R * 1.0, L ? 5 : 6), c.panel, { m: M4(T, [kx * 0.4, len, 0]), jit: 0.12 });
  if (!L) {
    b.add(ball(T, R * 1.8, R * 1.8, R * 1.8, 5, 3), c.panel, { m: M4(T, [0, 0, 0]) });
    b.add(ball(T, R * 1.5, R * 1.5, R * 1.5, 5, 3), c.accent, { m: M4(T, [kx, len * 0.45, 0]) });
    b.add(rodG(T, V(T, 0, len * 0.08, R * 2.4), V(T, kx * 0.9, len * 0.8, R * 2.4), R * 0.5, 4), c.metal, { jit: 0.1 });
  }
  b.sock('foot', [kx * 0.4, len + R, 0], [0, 1, 0]);
  return b.done();
}
function barrel(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.2, R = p.r ?? 0.014, seg = L ? 4 : 6;
  b.add(boxG(T, R * 3.2, len * 0.22, R * 3.2), c.panel, { jit: 0.1 });
  b.add(cyl(T, R, R * 0.8, len, seg), c.dark, { m: M4(T, [0, len * 0.1, 0]), jit: 0.1 });
  b.add(cyl(T, R * 1.6, R * 1.35, len * 0.1, seg), c.metal, { m: M4(T, [0, len * 0.92, 0]), jit: 0.1 });
  if (!L) {
    b.add(cyl(T, R * 1.5, R * 1.5, len * 0.05, seg), c.accent, { m: M4(T, [0, len * 0.4, 0]), jit: 0.1 });
    b.add(cyl(T, R * 1.5, R * 1.5, len * 0.05, seg), c.accent, { m: M4(T, [0, len * 0.65, 0]), jit: 0.1 });
  }
  b.glow(new T.CircleGeometry(R * 0.9, seg).rotateX(-Math.PI / 2).translate(0, len * 1.03, 0), { col: p.glowCol || c.glow });
  b.sock('muzzle', [0, len * 1.05, 0], [0, 1, 0]);
  return b.done();
}
function block(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo || !!p.simple;
  const w = p.w ?? 0.16, h = p.h ?? 0.12, d = p.d ?? 0.16, k = 0.92;
  const rect = (hw, hd, y) => [V(T, -hw, y, -hd), V(T, hw, y, -hd), V(T, hw, y, hd), V(T, -hw, y, hd)];
  b.add(loft(T, [rect(w / 2, d / 2, 0), rect(w / 2 * k, d / 2 * k, h)], true, true), p.wall !== undefined ? p.wall : c.base, { jit: 0.16 });
  b.add(boxG(T, w * 0.98, h * 0.07, d * 0.98), c.panel, { m: M4(T, [0, h, 0]), jit: 0.1 });
  if (!L) {
    const rows = p.rows ?? 2;
    for (let i = 0; i < rows; i++) for (const s of [-1, 1]) {
      b.glow(boxC(T, w * 0.02, h * 0.1, d * 0.7), { m: M4(T, [s * (w / 2 * 0.97 + 0.004), h * (0.34 + i * 0.28), 0]), col: p.glowCol || c.glow });
    }
    b.glow(boxC(T, w * 0.5, h * 0.1, d * 0.02), { m: M4(T, [0, h * 0.5, -d / 2 - 0.003]), col: p.glowCol || c.glow });
    b.add(boxG(T, w * 0.28, h * 0.4, d * 0.02), c.dark, { m: M4(T, [0, 0, d / 2 * 0.99]), jit: 0.1 });
    b.add(boxG(T, w * 0.18, h * 0.12, d * 0.25), c.metal, { m: M4(T, [w * 0.2, h * 1.07, 0]), jit: 0.1 });
    b.add(boxG(T, w * 1.0, h * 0.06, d * 0.14), c.accent, { m: M4(T, [0, h * 0.62, d / 2 * 0.9]), jit: 0.1 });
  }
  b.sock('roof', [0, h * 1.07, 0], [0, 1, 0]);
  return b.done();
}
function tank(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.07, h = p.h ?? 0.2, seg = L ? 5 : 8;
  b.add(cyl(T, R, R, h, seg), c.base, { cf: (col, x, y) => { if (y > h * 0.7) col.lerp(c.panel, 0.25); } });
  b.add(loft(T, [ringPts(T, seg, R, R, h), ringPts(T, seg, R * 0.55, R * 0.55, h + R * 0.4), ringPts(T, seg, 0, 0, h + R * 0.55)], false, false), c.panel, { jit: 0.1 });
  if (!L) {
    for (const t of [0.2, 0.6]) b.add(cyl(T, R * 1.06, R * 1.06, h * 0.06, seg), c.accent, { m: M4(T, [0, h * t, 0]), jit: 0.1 });
    b.add(boxG(T, R * 0.12, h * 0.8, R * 0.05), c.metal, { m: M4(T, [R * 1.0, h * 0.05, 0]) });
    b.add(rodG(T, V(T, -R * 0.4, h * 1.2, 0), V(T, -R * 1.4, h * 0.6, 0), R * 0.07, 4), c.metal);
    b.glow(boxC(T, R * 0.16, R * 0.16, R * 0.16), { m: M4(T, [0, h + R * 0.6, 0]), col: p.glowCol || c.accent });
  }
  b.sock('roof', [0, h + R * 0.5, 0], [0, 1, 0]);
  return b.done();
}
function tower(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const h = p.h ?? 0.35, R = p.r ?? 0.045, seg = L ? 5 : 6;
  b.add(cyl(T, R * 1.3, R * 0.8, h * 0.62, seg), c.base, { jit: 0.16 });
  b.add(cyl(T, R * 1.65, R * 1.1, h * 0.09, seg), c.panel, { m: M4(T, [0, h * 0.62, 0]), jit: 0.1 });
  b.add(cyl(T, R * 0.55, R * 0.35, h * 0.28, seg), c.metal, { m: M4(T, [0, h * 0.71, 0]), jit: 0.1 });
  if (!L) {
    for (let i = 0; i < 4; i++) { const a = i / 4 * 6.283 + 0.4; b.glow(boxC(T, R * 0.25, R * 0.2, R * 0.05), { m: M4(T, [Math.cos(a) * R * 1.62, h * 0.655, Math.sin(a) * R * 1.62], [0, -a + Math.PI / 2, 0]), col: p.glowCol || c.glow }); }
    b.add(cyl(T, R * 0.15, R * 0.1, h * 0.25, 4), c.dark, { m: M4(T, [0, h * 0.99, 0]), jit: 0.1 });
    b.add(cyl(T, R * 1.4, R * 1.3, h * 0.04, seg), c.accent, { m: M4(T, [0, h * 0.18, 0]), jit: 0.1 });
  }
  b.glow(ball(T, R * 0.3, R * 0.3, R * 0.3, 4, 3), { m: M4(T, [0, h * (L ? 1.0 : 1.24), 0]), col: p.glowCol || c.accent });
  b.sock('top', [0, h * 1.25, 0], [0, 1, 0]);
  return b.done();
}

// ---- big circular engine ring (hauler main engine): grows along +Y, exhaust face at +Y
function ring(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.12, len = p.len ?? 0.1, seg = L ? 10 : 16, nt = L ? 8 : 16;
  b.add(cyl(T, R, R, len, seg), c.dark, { jit: 0.1 });
  b.add(cyl(T, R * 1.07, R * 1.07, len * 0.18, seg), c.metal, { jit: 0.1 });
  b.add(cyl(T, R * 0.9, R * 0.9, len * 0.1, seg), c.panel, { m: M4(T, [0, len * 0.98, 0]), jit: 0.1 });
  const tA = new T.Color(p.tileA !== undefined ? p.tileA : 0x4A7CFF), tB = new T.Color(p.tileB !== undefined ? p.tileB : 0x3A66E0);
  for (let i = 0; i < nt; i++) {
    const a = i / nt * Math.PI * 2;
    b.glow(new T.PlaneGeometry(R * 0.27 * (16 / nt), R * 0.18).rotateX(-Math.PI / 2), { m: M4(T, [Math.cos(a) * R * 0.72, len * 1.1, Math.sin(a) * R * 0.72], [0, -a - Math.PI / 2, 0]), col: i % 2 ? tB : tA });
  }
  if (!L) for (let i = 0; i < 24; i++) {
    const a = i / 24 * Math.PI * 2;
    b.glow(new T.PlaneGeometry(R * 0.06, R * 0.06).rotateX(-Math.PI / 2), { m: M4(T, [Math.cos(a) * R * 0.5, len * 1.09, Math.sin(a) * R * 0.5]), col: 0xB8C8FF });
  }
  b.glow(new T.CircleGeometry(R * 0.38, seg).rotateX(-Math.PI / 2).translate(0, len * 1.14, 0), { col: 0xB070FF });
  b.glow(new T.CircleGeometry(R * 0.27, seg).rotateX(-Math.PI / 2).translate(0, len * 1.2, 0), { col: p.coreCol !== undefined ? p.coreCol : 0xF2E6FF });
  b.sock('exhaust', [0, len * 1.2, 0], [0, 1, 0]);
  b.sock('base', [0, 0, 0], [0, -1, 0]);
  return b.done();
}
// ---- thruster slot: dark frame with a glowing pane; face normal +Y
function slot(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const w = p.w ?? 0.08, h = p.h ?? 0.06, th = p.th ?? 0.008;
  b.add(boxG(T, w, th, h), c.dark, { jit: 0.1 });
  b.glow(boxC(T, w * 0.85, th * 0.8, h * 0.8), { m: M4(T, [0, th * 1.1, 0]), col: p.glowCol || c.glow });
  if (!L) {
    b.add(boxG(T, w * 1.06, th * 0.6, h * 0.12), c.metal, { m: M4(T, [0, 0, h * 0.5]), jit: 0.1 });
    b.add(boxG(T, w * 1.06, th * 0.6, h * 0.12), c.metal, { m: M4(T, [0, 0, -h * 0.5]), jit: 0.1 });
  }
  b.sock('exhaust', [0, th * 1.5, 0], [0, 1, 0]);
  return b.done();
}

// ---- hull (length 1, nose -Z, tail +Z)
const HK = {
  dart:  [[0, .012, -.02, .02], [.08, .05, -.05, .06], [.25, .11, -.08, .10], [.5, .15, -.09, .12], [.75, .14, -.09, .11], [.92, .11, -.08, .09], [1, .10, -.07, .08]],
  brick: [[0, .10, -.06, .04], [.07, .20, -.14, .13], [.3, .24, -.16, .17], [.8, .245, -.16, .18], [.95, .23, -.15, .17], [1, .21, -.13, .15]],
  disc:  [[0, .03, -.02, .02], [.12, .24, -.05, .05], [.35, .40, -.08, .09], [.55, .43, -.09, .10], [.82, .30, -.06, .07], [.95, .16, -.04, .05], [1, .10, -.03, .04]],
};
// profile lookup for decals: half-width / bottom / top of a hull kind at u (0 nose .. 1 tail), same scaling as the hull part
export function hullSection(kind, u, wid = 1, hgt = 1) {
  const K = HK[kind] || HK.dart;
  let i = 0; while (i < K.length - 2 && u > K[i + 1][0]) i++;
  const a = K[i], q = K[i + 1], t = clamp((u - a[0]) / (q[0] - a[0] || 1), 0, 1);
  return { w: (a[1] + (q[1] - a[1]) * t) * wid, yb: (a[2] + (q[2] - a[2]) * t) * hgt, yt: (a[3] + (q[3] - a[3]) * t) * hgt };
}
function hull(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo, kind = p.kind || 'dart', wid = p.wid ?? 1, hgt = p.hgt ?? 1;
  const K = HK[kind] || HK.dart;
  const samp = u => {
    let i = 0; while (i < K.length - 2 && u > K[i + 1][0]) i++;
    const a = K[i], q = K[i + 1], t = clamp((u - a[0]) / (q[0] - a[0] || 1), 0, 1);
    return { w: (a[1] + (q[1] - a[1]) * t) * wid, yb: (a[2] + (q[2] - a[2]) * t) * hgt, yt: (a[3] + (q[3] - a[3]) * t) * hgt };
  };
  const ring8 = (u) => {
    const s = samp(u), z = -0.5 + u, ch = Math.min(s.w, (s.yt - s.yb) / 2) * (p.chamfer ?? 0.5), w = s.w, yb = s.yb, yt = s.yt;
    return [V(T, -w + ch, yt, z), V(T, w - ch, yt, z), V(T, w, yt - ch, z), V(T, w, yb + ch, z), V(T, w - ch, yb, z), V(T, -w + ch, yb, z), V(T, -w, yb + ch, z), V(T, -w, yt - ch, z)];
  };
  const us = L ? [0, 0.12, 0.3, 0.55, 0.8, 1] : [0, 0.06, 0.14, 0.24, 0.36, 0.5, 0.62, 0.75, 0.87, 0.95, 1];
  const stripeC = new T.Color(c.accent);
  b.add(loft(T, us.map(ring8), true, true), c.base, {
    jit: 0.16, cf: (col, x, y, z) => {
      const s = samp(z + 0.5);
      if (y < s.yb + (s.yt - s.yb) * 0.28) col.lerp(c.panel, 0.55);
      if (p.stripe && Math.abs(x) > s.w * 0.5 && y > s.yb + (s.yt - s.yb) * 0.4 && y < s.yb + (s.yt - s.yb) * 0.62) col.lerp(stripeC, 0.8);
    },
  });
  if (!L) {
    const ridge = (u0, u1, k, h, col, bottom) => {
      const rr = [];
      for (let i = 0; i < 5; i++) {
        const u = u0 + (u1 - u0) * i / 4, s = samp(u), z = -0.5 + u, ww = s.w * k, y0 = bottom ? s.yb + 0.004 : s.yt - 0.004, dir = bottom ? -1 : 1;
        rr.push([V(T, -ww, y0, z), V(T, -ww * 0.7, y0 + dir * h, z), V(T, ww * 0.7, y0 + dir * h, z), V(T, ww, y0, z)]);
      }
      b.add(loft(T, rr, true, true), col, { jit: 0.1 });
    };
    ridge(0.22, 0.82, 0.42, 0.016, c.panel, false);
    ridge(0.3, 0.9, 0.3, 0.02, c.dark, true);
    for (const s of [-1, 1]) {
      const u = 0.5, sm = samp(u), yc = (sm.yb + sm.yt) / 2;
      b.glow(boxC(T, 0.004, (sm.yt - sm.yb) * 0.06, 0.34), { m: M4(T, [s * (sm.w + 0.003), yc, -0.5 + u]), col: p.glowCol || c.glow });
      b.add(boxC(T, 0.01, (sm.yt - sm.yb) * 0.34, 0.1), c.dark, { m: M4(T, [s * (sm.w + 0.002), yc, -0.5 + 0.38]), jit: 0.1 });
      b.add(boxC(T, 0.008, (sm.yt - sm.yb) * 0.22, 0.12), c.accent, { m: M4(T, [s * (sm.w + 0.002), yc - (sm.yt - sm.yb) * 0.05, -0.5 + 0.7]), jit: 0.1 });
    }
    b.glow(boxC(T, 0.03, 0.012, 0.012), { m: M4(T, [0, samp(0.03).yt * 0.3, -0.5]), col: p.glowCol || c.glow });
  }
  const at = (u) => { const s = samp(u); return { z: -0.5 + u, ...s, yc: (s.yb + s.yt) / 2 }; };
  const n = at(0), tl = at(1), tf = at(0.3), tm = at(0.55), tr = at(0.8), s0 = at(0.5), s1 = at(0.72), lf = at(0.3), lr = at(0.78);
  b.sock('nose', [0, n.yc, n.z], [0, 0, -1]);
  b.sock('tail', [0, tl.yc, tl.z], [0, 0, 1]);
  b.sock('tail1', [tl.w * 0.55, tl.yc, tl.z], [0, 0, 1]);
  b.sock('top_front', [0, tf.yt, tf.z + 0.06], [0, 1, 0]);
  b.sock('top_mid', [0, tm.yt, tm.z], [0, 1, 0]);
  b.sock('top_rear', [0, tr.yt, tr.z], [0, 1, 0]);
  b.sock('top_rearX', [tr.w * 0.6, tr.yt - 0.01, tr.z], [0, 1, 0]);
  b.sock('side0', [s0.w, s0.yc, s0.z], [1, 0, 0], [0, 0, 1]);
  b.sock('side1', [s1.w, s1.yc, s1.z], [1, 0, 0], [0, 0, 1]);
  b.sock('belly0', [0, tf.yb, tf.z], [0, -1, 0]);
  b.sock('legF', [lf.w * 0.7, lf.yb, lf.z], [0, -1, 0]);
  b.sock('legR', [lr.w * 0.7, lr.yb, lr.z], [0, -1, 0]);
  b.sock('cargoA', [0, tm.yt, tm.z - 0.06], [0, 1, 0]);
  b.sock('cargoB', [0, tr.yt, tr.z + 0.02], [0, 1, 0]);
  return b.done();
}

// ---- creature spine: head -Z, tail +Z, gentle seeded S-curve
function spine(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const n = L ? Math.min(5, p.segs ?? 5) : Math.min(9, p.segs ?? 7), len = p.len ?? 1, R = p.r ?? 0.1, sx = p.wid ?? 1, sy = p.hgt ?? 1, bodyFx = p.bodyFx ?? 0.9, curve = p.curve ?? rn(r, -0.08, 0.08), ph = r() * 6;
  const ws = L ? 5 : 7, hs = L ? 3 : 4;
  const taper = t => R * (0.55 + 0.55 * Math.sin(Math.PI * clamp(t * 1.25 + 0.05, 0, 1))) * (1 - 0.65 * t * t);
  const info = [];
  for (let i = 0; i < n; i++) {
    const t = (i + 0.5) / n, z = -len / 2 + t * len, rad = taper(t), dz = len / n;
    const x = Math.sin(t * 4.2 + ph) * curve * len, y = Math.sin(t * 3 + ph * 0.7) * curve * len * 0.4;
    info.push({ t, z, x, y, rad, dz, rx: rad * sx, ry: rad * 0.82 * sy });
    b.add(ball(T, rad * sx, rad * 0.82 * sy, dz * (p.stretch ?? 0.78), ws, hs), i % 2 ? c.panel : c.base, {
      m: M4(T, [x, y, z]), fxw: (px, py, pz) => clamp(((pz + len / 2) / len), 0, 1) * bodyFx,
      cf: (col, px, py) => { if (py < y - rad * 0.2) col.lerp(c.dark, 0.5); if (py > y + rad * 0.45) col.lerp(c.accent, 0.12); },
    });
    if (!L && i > 0 && i < n - 1 && i % 2 === 1) for (const s of [-1, 1]) b.glow(boxC(T, rad * 0.06, rad * 0.14, dz * 0.25), { m: M4(T, [x + s * rad * sx * 0.97, y + rad * 0.1, z]), col: p.glowCol || c.glow, fxw: t });
  }
  const h = info[0], tl = info[n - 1];
  b.sock('head', [h.x, h.y, h.z - h.dz * 0.7], [0, 0, -1]);
  b.sock('tail', [tl.x, tl.y, tl.z + tl.dz * 0.7], [0, 0, 1]);
  b.sock('eyeA', [h.x + h.rx * 0.45, h.y + h.ry * 0.45, h.z - h.dz * 0.5], [0.55, 0.35, -0.75]);
  b.sock('eyeC', [h.x, h.y + h.ry * 0.8, h.z - h.dz * 0.25], [0, 0.8, -0.6]);
  b.sock('jawA', [h.x + h.rx * 0.35, h.y - h.ry * 0.4, h.z - h.dz * 0.6], [0.35, -0.3, -0.9]);
  info.forEach((q, i) => {
    b.sock('dorsal' + i, [q.x, q.y + q.ry * 0.92, q.z], [0, 1, 0]);
    b.sock('flank' + i, [q.x + q.rx * 0.9, q.y, q.z], [1, 0, 0], [0, 0, 1]);
    b.sock('belly' + i, [q.x + q.rx * 0.15, q.y - q.ry * 0.92, q.z], [0, -1, 0]);
  });
  b.info = info;
  b.nseg = n;
  return b.done();
}

// ---- outpost platform
function pad(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const R = p.r ?? 0.5, h = p.h ?? 0.035, seg = 6;
  b.add(cyl(T, R, R * 0.93, h, seg), c.panel, { jit: 0.14 });
  b.add(cyl(T, R * 0.97, R * 0.9, h * 0.35, seg), c.base, { m: M4(T, [0, h, 0]), jit: 0.14 });
  if (!L) {
    b.add(cyl(T, R * 1.04, R * 1.02, h * 0.35, seg), c.dark, { m: M4(T, [0, 0, 0]), jit: 0.1 });
    b.add(cyl(T, R * 0.3, R * 0.27, h * 0.3, seg), c.accent, { m: M4(T, [0, h * 1.35, 0]), jit: 0.1 });
    for (let i = 0; i < 6; i++) {
      const a = i / 6 * 6.283 + 0.52; b.glow(boxC(T, R * 0.05, h * 0.3, R * 0.05), { m: M4(T, [Math.cos(a) * R * 0.97, h * 1.1, Math.sin(a) * R * 0.97]), col: p.glowCol || c.accent });
    }
    for (let i = 0; i < 6; i++) { const a = i / 6 * 6.283; b.add(boxC(T, R * 0.35, h * 0.2, R * 0.04), c.metal, { m: M4(T, [Math.cos(a) * R * 0.45, h * 1.2, Math.sin(a) * R * 0.45], [0, -a - 1.57, 0]), jit: 0.1 }); }
  }
  for (let i = 0; i < 6; i++) { const q = padPos(i, R, h); b.sock('b' + i, q, [0, 1, 0], [0, 0, 1]); }
  b.sock('c', [0, h * 1.5, 0], [0, 1, 0], [0, 0, 1]);
  return b.done();
}
const padPos = (i, R, h) => { const a = i / 6 * 6.283 + 0.26; return [Math.cos(a) * R * 0.62, h * 1.3, Math.sin(a) * R * 0.62]; };

// ---- rev 19 abstract-boss parts. All grow along +Y; chord along Z, thickness along X.
// shard: elongated obsidian bipyramid sliver (obsidian body, accent-tinted tip, optional thin emissive glint along the spine)
function shard(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.2, w = p.w ?? len * 0.22, th = p.th ?? w * 0.35, n = L ? 3 : 4, mid = p.mid ?? 0.36;
  const tipC = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  const ring = (y, k) => { const o = []; for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2 + 0.5; o.push(V(T, Math.cos(a) * th * k, y, Math.sin(a) * w * k)); } return o; };
  const pt = (y) => { const o = []; for (let i = 0; i < n; i++) o.push(V(T, 0, y, 0)); return o; };
  b.add(loft(T, [pt(0), ring(len * mid, 1), pt(len)], false, false), c.base,
    { jit: 0.2, cf: (col, x, y) => { const t = y / len; if (t > 0.62) col.lerp(tipC, (t - 0.62) / 0.38 * (p.tipK ?? 0.55)); } });
  if (p.glint && !L) b.glow(boxC(T, th * 0.5, len * 0.7, 0.003), { m: M4(T, [0, len * 0.45, 0]), col: tipC });
  b.sock('tip', [0, len, 0], [0, 1, 0]);
  return b.done();
}
// orb: faceted dark sphere (icosahedron) with an emissive pore on +Y
function orb(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo, R = p.r ?? 0.08;
  const g = new T.IcosahedronGeometry(R, L ? 0 : 1);
  b.add(g, c.base, { jit: 0.25, cf: (col, x, y, z) => { const k = hash3(x, y, z); if (k > 0.8) col.lerp(c.panel, 0.7); } });
  const pc = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  b.glow(new T.IcosahedronGeometry(R * (p.pore ?? 0.26), 0), { m: M4(T, [0, R * 0.92, 0], null, [1, 0.5, 1]), col: pc });
  b.sock('top', [0, R, 0], [0, 1, 0]);
  return b.done();
}
// filament: thin curved tapered thread along +Y with an emissive bead at the tip. emi:true makes the whole thread self-lit.
function filament(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.4, R = p.r ?? 0.006, n = L ? 3 : 6, seg = L ? 3 : 4, bx = p.bend ?? 0, bz = p.bendZ ?? 0;
  const rings = [];
  for (let i = 0; i <= n; i++) {
    const t = i / n, rr = R * (1 - 0.8 * t), cx = bx * t * t, cz = bz * Math.sin(t * Math.PI), pts = [];
    for (let k = 0; k < seg; k++) { const a = k / seg * Math.PI * 2; pts.push(V(T, cx + Math.cos(a) * rr, t * len, cz + Math.sin(a) * rr)); }
    rings.push(pts);
  }
  const col = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  b.add(loft(T, rings, false, false), p.emi ? col : c.dark, { emi: !!p.emi, jit: 0.1 });
  if (p.bead !== false) b.glow(new T.IcosahedronGeometry(R * 2.2, 0), { m: M4(T, [bx, len, 0]), col });
  b.sock('tip', [bx, len, 0], [0, 1, 0]);
  return b.done();
}
// tooth: hooked fang, curves toward +Z, accent-tinted point
function tooth(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.15, R = p.r ?? 0.025, hook = p.hook ?? 0.3, n = L ? 2 : 4, seg = L ? 3 : 4;
  const tipC = new T.Color(p.tip !== undefined ? p.tip : c.accent);
  const rings = [];
  for (let i = 0; i <= n; i++) {
    const t = i / n, rr = R * Math.pow(1 - t, 0.8) + 0.0001, cz = hook * len * t * t, pts = [];
    for (let k = 0; k < seg; k++) { const a = k / seg * Math.PI * 2 + 0.78; pts.push(V(T, Math.cos(a) * rr * 0.8, t * len, cz + Math.sin(a) * rr)); }
    rings.push(pts);
  }
  b.add(loft(T, rings, true, false), c.base, { jit: 0.2, cf: (col, x, y) => { const t = y / len; if (t > 0.55) col.lerp(tipC, (t - 0.55) / 0.45 * 0.7); } });
  b.sock('tip', [0, len, hook * len], [0, 1, 0]);
  return b.done();
}
// finwing: flat kite blade (thin in X, chord along Z), dark with an emissive edge strip along the leading edge
function finwing(T, p, r) {
  const b = mkB(T, p, r), c = b.c, L = b.lo;
  const len = p.len ?? 0.3, w = p.w ?? 0.12, sw = p.sweep ?? 0.4, th = p.th ?? 0.008;
  const prof = [[0, 0.45, 0], [0.5, 1, sw * 0.5], [1, 0.0, sw]];
  const rings = prof.map(([t, k, o]) => { const ch = w * k, y = t * len, tk = th * (1 - 0.6 * t); return [V(T, 0, y, -ch / 2 + o * len * 0.5), V(T, tk, y, o * len * 0.5), V(T, 0, y, ch / 2 + o * len * 0.5), V(T, -tk, y, o * len * 0.5)]; });
  b.add(loft(T, rings, true, false), c.base, { jit: 0.2 });
  if (!L) {
    const edge = p.edge !== undefined ? new T.Color(p.edge) : c.accent;
    b.glow(boxC(T, th * 1.1, len * 0.95, 0.004), { m: M4(T, [0, len * 0.5, -w * 0.18 + sw * len * 0.12], [0, 0, 0]), col: edge });
  }
  b.sock('tip', [0, len, sw * len * 0.5], [0, 1, 0]);
  return b.done();
}

export const PARTS = { fin, wing, spike, plate, pod, antenna, engine, eye, tentacle, claw, shell, dome, strut, canopy, leg, barrel, block, tank, tower, hull, spine, pad, ring, slot, shard, orb, filament, tooth, finwing };

// ---------------------------------------------------------------- compose
function socketMatrix(T, s, extraRoll) {
  const Y = s.dir.clone().normalize();
  let Z = (s.hint || V(T, 0, 0, 1)).clone();
  Z.addScaledVector(Y, -Z.dot(Y));
  if (Z.lengthSq() < 1e-4) { Z = V(T, 1, 0, 0); Z.addScaledVector(Y, -Z.dot(Y)); }
  Z.normalize();
  const X = new T.Vector3().crossVectors(Y, Z);
  const m = new T.Matrix4().makeBasis(X, Y, Z); m.setPosition(s.pos);
  if (extraRoll) m.multiply(new T.Matrix4().makeRotationY(extraRoll));
  return m;
}
export function compose(T, recipe, rng) {
  const lit = [], emi = [], sockets = {}, parts = [], cnt = {};
  const lod = recipe.lod || 'hi', pal = recipe.palette || {};
  const MX = new T.Matrix4().makeScale(-1, 1, 1);
  let idc = 0;
  function emit(node, parentW, parentSock, mirroredPass, path, isRoot) {
    const def = PARTS[node.part];
    if (!def) throw new Error('ship-parts: unknown part ' + node.part);
    const params = Object.assign({ lod, pal }, node.params || {});
    const res = def(T, params, rng);
    let A = new T.Matrix4();
    if (node.at && node.at.isMatrix4) A.copy(node.at);
    else if (typeof node.at === 'string') {
      const s = parentSock && parentSock[node.at];
      if (s) A = socketMatrix(T, s, node.roll);
      else if (typeof console !== 'undefined' && !emit._warned) { emit._warned = 1; console.warn('ship-parts: missing socket', node.at, 'for', node.part); }
    }
    if (node.offset) A.multiply(new T.Matrix4().makeTranslation(node.offset[0], node.offset[1], node.offset[2]));
    if (node.rot) A.multiply(M4(T, [0, 0, 0], node.rot));
    if (node.scale) A.multiply(M4(T, [0, 0, 0], null, node.scale));
    const W = parentW.clone().multiply(A);
    const id = node.id || (node.part + (cnt[node.part] = (cnt[node.part] || 0) + 1));
    const passes = (node.mirror && !mirroredPass) ? [false, true] : [false];
    for (const mp of passes) {
      if (mp && !res.geo) continue;
      const Wp = mp ? MX.clone().multiply(W) : W;
      const g = res.geo.clone().applyMatrix4(Wp); lit.push(g);
      if (res.emissive) emi.push(res.emissive.clone().applyMatrix4(Wp));
      const pid = id + (mp ? 'M' : '');
      parts.push({ id: pid, part: node.part, tris: res.tris });
      const worldSock = {};
      for (const k in res.sockets) {
        const s = res.sockets[k];
        const ws = { pos: s.pos.clone().applyMatrix4(Wp), dir: s.dir.clone().transformDirection(Wp), hint: s.hint ? s.hint.clone().transformDirection(Wp) : undefined };
        if (isRoot) sockets[k] = ws;
        else sockets[pid + '.' + k] = ws;
      }
      if (node.children) for (const ch of node.children) emit(ch, Wp, res.sockets, mp || mirroredPass, path + '/' + pid, false);
    }
  }
  emit(recipe, new T.Matrix4(), null, false, '', true);
  let geo = mergeGeometries(lit, false);
  if (recipe.paint) geo = paint(geo, Object.assign({ rng }, recipe.paint));
  let emissive = emi.length ? mergeGeometries(emi, false) : null;
  for (const g of [geo, emissive]) if (g) g.deleteAttribute('normal');
  if (recipe.flex === false) for (const g of [geo, emissive]) if (g) { g.deleteAttribute('aFx'); g.deleteAttribute('aPiv'); g.deleteAttribute('aLm'); }
  const tris = (geo.attributes.position.count + (emissive ? emissive.attributes.position.count : 0)) / 3;
  return { geo, emissive, sockets, tris, parts };
}
// fold the emissive geometry into the lit one (colours pushed past 1.15 so the lit shader draws them self-lit and pulsing): ONE mesh, ONE material.
export function mergeLit(T, res, k = 2.0) {
  if (!res.emissive) return res.geo;
  const e = res.emissive, C = e.attributes.color.array;
  for (let i = 0; i < C.length; i++) C[i] *= k;
  const g = mergeGeometries([res.geo, e], false);
  e.dispose(); res.geo.dispose();
  return g;
}

// ---------------------------------------------------------------- materials
// ONE flex vertex shader serves creatures AND bosses (superset), ONE static vertex shader serves hulls / weapons / outposts.
//   aFx = (sway weight, phase, mode, param). mode 0 = parts sway (weight) + body wave (uWave), 5 = fin flap (+ sway + wave),
//   1 = jaw hinge about Y, 2 = boss sway (uAmp/uSp/uK), 3 = fin flap about Z, 4 = bell pulse.  aPiv = hinge pivot. aLm = (limb id+1, weight):
//   boss limbs swing about uSwP[i] by uSwA[i] (axis xyz, angle w) + uSwT[i].xyz translate, uSwT[i].w = telegraph glow.
const VERT_FLEX = /* glsl */`
uniform float uTime; uniform float uPh; uniform float uWave;
uniform vec3 uAmp; uniform float uSp; uniform float uK; uniform float uBreath; uniform float uHsp; uniform float uPulse;
uniform vec4 uSwA[8]; uniform vec4 uSwT[8]; uniform vec3 uSwP[8]; uniform float uGl;
attribute vec4 aFx; attribute vec3 aPiv; attribute vec2 aLm;
varying vec3 vC; varying vec3 vV; varying float vGl;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec3 p = position;
  float md = aFx.z;
  if (md < 0.5 || abs(md - 5.0) < 0.5) {
    float tail = smoothstep(-0.25, 0.45, p.z) * uWave;
    p.x += sin(p.z * 9.0 - uTime * 3.2 + uPh) * 0.03 * tail;
    p.y += sin(p.z * 7.0 - uTime * 2.4 + uPh) * 0.022 * tail;
    float w = aFx.x;
    p.x += sin(uTime * 2.3 + aFx.y + w * 4.5) * 0.11 * w * w;
    p.y += cos(uTime * 1.8 + aFx.y * 1.3 + w * 3.5) * 0.08 * w * w;
    p.z += sin(uTime * 2.0 + aFx.y + w * 3.0) * 0.05 * w * w;
    if (md > 4.5) { float fin = abs(p.x); p.y += sin(uTime * 3.0 + uPh + fin * 6.0) * fin * 0.22 * smoothstep(0.05, 0.25, fin); }
    p.xy *= 1.0 + 0.03 * uWave * sin(uTime * 2.2 + uPh + p.z * 6.0);
  } else if (abs(md - 1.0) < 0.5) {
    float a = aFx.w * (0.5 + 0.5 * sin(uTime * uHsp + aFx.y));
    vec3 q = p - aPiv; float c = cos(a), s = sin(a);
    p = aPiv + vec3(q.x * c + q.z * s, q.y, -q.x * s + q.z * c);
  } else if (abs(md - 2.0) < 0.5) {
    vec3 off = vec3(sin(uTime * uSp + aFx.y + aFx.w * uK) * uAmp.x,
                    cos(uTime * uSp * 0.8 + aFx.y * 1.3 + aFx.w * uK) * uAmp.y,
                    sin(uTime * uSp * 0.6 + aFx.y) * uAmp.z);
    p += off * aFx.x;
    p.xy *= 1.0 + uPulse * sin(uTime * 1.7) * (1.0 - aFx.x);
  } else if (abs(md - 3.0) < 0.5) {
    float a = aFx.w * sin(uTime * uHsp + aFx.y);
    vec3 q = p - aPiv; float c = cos(a), s = sin(a);
    p = aPiv + vec3(q.x * c - q.y * s, q.x * s + q.y * c, q.z);
  } else if (abs(md - 4.0) < 0.5) {
    p.xy *= 1.0 + uPulse * sin(uTime * 1.7);
  } else if (md > 5.5 && md < 8.5) {
    // rev 19 abstract bosses. 6 = orbit about Y through aPiv.xz (+ bob), 7 = rotate about Z through aPiv.xy, 8 = scale pulse about aPiv.
    // aFx = (amp, phase, mode, speed). A rare sudden twitch (pow of a slow sine) is added to the angle: slow, then a snap.
    float tw = pow(abs(sin(uTime * 0.31 + aFx.y)), 26.0) * 0.5 * sign(aFx.w);
    if (md < 6.5) {
      float a = uTime * aFx.w + tw; vec3 q = p - vec3(aPiv.x, 0.0, aPiv.z); float c = cos(a), s = sin(a);
      p = vec3(aPiv.x, 0.0, aPiv.z) + vec3(q.x * c + q.z * s, q.y + sin(uTime * 0.7 + aFx.y) * aFx.x, -q.x * s + q.z * c);
    } else if (md < 7.5) {
      float a = uTime * aFx.w + tw; vec2 q = p.xy - aPiv.xy; float c = cos(a), s = sin(a);
      p.xy = aPiv.xy + vec2(q.x * c - q.y * s, q.x * s + q.y * c);
      p.z += sin(uTime * 0.6 + aFx.y) * aFx.x;
    } else {
      float k = 1.0 + aFx.x * (sin(uTime * aFx.w + aFx.y) + 0.6 * tw * 4.0);
      p = aPiv + (p - aPiv) * k;
    }
  }
  if (uGl > 0.0) { float gs = floor(uTime * 11.0 + aFx.y); float gh = fract(sin(gs * 12.9898 + uPh * 78.233) * 43758.5453); p.x += uGl * 0.012 * step(0.955, gh) * sin(p.y * 61.0 + gs); }
  vGl = 0.0;
  if (aLm.x > 0.5) {
    int li = int(aLm.x + 0.5) - 1;
    vec4 sa = uSwA[li]; vec4 sw = uSwT[li]; vec3 pv = uSwP[li];
    if (abs(md - 7.0) < 0.5) p.xy *= 1.0 + aPiv.z * sw.w;       // iris: the ring opens with the telegraph glow
    float ang = sa.w * aLm.y; vec3 q2 = p - pv; float cc = cos(ang), ss = sin(ang);
    p = pv + q2 * cc + cross(sa.xyz, q2) * ss + sa.xyz * dot(sa.xyz, q2) * (1.0 - cc) + sw.xyz * aLm.y;
    vGl = sw.w;
  }
  p *= 1.0 + uBreath * sin(uTime * 0.9);
  vec4 mv = modelViewMatrix * vec4(p, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const VERT_STATIC = /* glsl */`
uniform float uTime; uniform float uPh;
varying vec3 vC; varying vec3 vV; varying float vGl;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color; vGl = 0.0;
  vec4 mv = modelViewMatrix * vec4(position, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG_LIT = /* glsl */`
uniform float uTime; uniform float uPh; uniform float uHit; uniform vec3 uRim; uniform float uGl; uniform vec3 uGlC;
varying vec3 vC; varying vec3 vV; varying float vGl;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = max(0.0, dot(n, L)) * 0.7 + 0.3;
  float rim = pow(1.0 - max(0.0, dot(n, normalize(-vV))), 3.0);
  vec3 col = vC * d + rim * uRim;
  float em = step(1.15, max(vC.r, max(vC.g, vC.b)));
  float fl = 1.0;
  if (uGl > 0.0) {   // rev 19: subtle emissive flicker + rare dropout slices
    float gs = floor(uTime * 14.0), gh = fract(sin(gs * 12.9898 + uPh * 78.233) * 43758.5453);
    fl = 1.0 - uGl * (0.62 * step(0.93, gh) + 0.1 * sin(uTime * 31.0 + uPh * 5.0 + vC.g * 9.0));
  }
  col = mix(col, vC * (0.85 + 0.3 * sin(uTime * 5.0 + uPh)) * fl, em);
  col += uHit * vec3(1.0, 0.65, 0.5);
  col += vGl * uGlC * (0.65 + 0.35 * sin(uTime * 26.0));
  gl_FragColor = vec4(col, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const FRAG_EMI = /* glsl */`
uniform float uTime; uniform float uPh;
varying vec3 vC; varying vec3 vV; varying float vGl;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  gl_FragColor = vec4(vC * (0.92 + 0.12 * sin(uTime * 4.0 + uPh)), 1.0);
  #include <colorspace_fragment>
}`;
const FRAG_GLASS = /* glsl */`
uniform float uTime; uniform float uPh; uniform float uHit;
varying vec3 vC; varying vec3 vV; varying float vGl;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  float rim = pow(1.0 - abs(dot(n, normalize(-vV))), 2.0);
  vec3 col = vC * (0.7 + 0.3 * sin(uTime * 1.7)) + rim * vec3(0.5, 0.95, 1.0) * 0.9 + uHit * vec3(1.0, 0.6, 0.5);
  gl_FragColor = vec4(col, 0.2 + 0.5 * rim);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const _mats = new Map();
// rim light colours (premultiplied): creatures warm orange, hulls cool violet
export const RIM_WARM = [0.315, 0.105, 0.0525], RIM_COOL = [0.14, 0.084, 0.238];
function uniformSet(T, flex, rim) {
  const u = { uTime: { value: 0 }, uPh: { value: 0 }, uHit: { value: 0 }, uRim: { value: new T.Vector3(rim[0], rim[1], rim[2]) }, uGl: { value: 0 }, uGlC: { value: new T.Vector3(1.15, 0.4, 0.12) } };
  if (flex) {
    u.uWave = { value: 1 }; u.uAmp = { value: new T.Vector3() }; u.uSp = { value: 1 }; u.uK = { value: 0 }; u.uBreath = { value: 0 }; u.uHsp = { value: 1 }; u.uPulse = { value: 0 };
    u.uSwA = { value: [] }; u.uSwT = { value: [] }; u.uSwP = { value: [] };
    for (let i = 0; i < 8; i++) { u.uSwA.value.push(new T.Vector4(0, 1, 0, 0)); u.uSwT.value.push(new T.Vector4()); u.uSwP.value.push(new T.Vector3()); }
  }
  return u;
}
// ShaderMaterial.clone() copies uniform arrays shallowly; this deep-copies the Vector arrays so per-creature limb state never leaks. Same shader text = same GL program.
export function cloneMaterial(T, mat) {
  const m = mat.clone();
  for (const k in m.uniforms) { const v = m.uniforms[k].value; if (Array.isArray(v)) m.uniforms[k].value = v.map(q => (q && q.clone) ? q.clone() : q); }
  return m;
}
// shared: mats.lit / mats.emissive (+ mats.glass for flex). opts.flex picks the creature program; the static program is for hulls / weapons / outposts.
export function makeMaterials(T, opts) {
  const flex = !!(opts && opts.flex), key = T.REVISION + (flex ? ':f' : ':s');
  if (_mats.has(key)) return _mats.get(key);
  const rim = flex ? RIM_WARM : RIM_COOL;
  const mk = (frag, extra) => new T.ShaderMaterial(Object.assign({ vertexShader: flex ? VERT_FLEX : VERT_STATIC, fragmentShader: frag, vertexColors: true, side: T.DoubleSide, uniforms: uniformSet(T, flex, rim) }, extra || {}));
  const lit = mk(FRAG_LIT), emissive = mk(FRAG_EMI);
  const out = { lit, emissive, flex, glass: flex ? mk(FRAG_GLASS, { transparent: true, depthWrite: false }) : null, cloneMaterial: m => cloneMaterial(T, m || lit),
    tick(t, ph) { for (const m of [lit, emissive]) { m.uniforms.uTime.value = t; if (ph !== undefined) m.uniforms.uPh.value = ph; } } };
  _mats.set(key, out);
  return out;
}

// ---------------------------------------------------------------- paint
const s2l = c => { c /= 255; return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4); };
const toRGB = c => (c && typeof c === 'object') ? [c.r, c.g, c.b] : [s2l((c >> 16) & 255), s2l((c >> 8) & 255), s2l(c & 255)];
export function paint(geo, o = {}) {
  if (geo.index) geo = geo.toNonIndexed();
  const rng = o.rng || mulberry(7), P = geo.attributes.position, n = P.count;
  geo.computeBoundingBox();
  const bb = geo.boundingBox, sz = [bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z];
  const long = sz[2] >= sz[0] * 0.7 ? 2 : 0;       // length axis: z unless the model is clearly wider than long
  const mn = [bb.min.x, bb.min.y, bb.min.z];
  let C = geo.attributes.color;
  const base = toRGB(o.base ?? 0x8E3A9C), accent = toRGB(o.accent ?? 0xF05A28), panel = toRGB(o.panel ?? 0x2A1535);
  if (!C) { C = new Float32Array(n * 3); for (let i = 0; i < n; i++) { C[i * 3] = base[0]; C[i * 3 + 1] = base[1]; C[i * 3 + 2] = base[2]; } geo.setAttribute('color', new P.constructor(C, 3)); C = geo.attributes.color; }
  const arr = C.array, grime = o.grime ?? 0.35;
  const nl = o.lines ?? 4, ns = o.stripes ?? 1;
  const lines = [], stripes = [];
  for (let i = 0; i < nl; i++) lines.push({ t: 0.08 + rng() * 0.84, w: 0.004 + rng() * 0.008 });
  for (let i = 0; i < ns; i++) stripes.push({ t: 0.15 + rng() * 0.6, w: 0.03 + rng() * 0.05, yLo: 0.25 + rng() * 0.2 });
  const cellSeed = rng() * 100;
  const e1 = new Float32Array(3), e2 = new Float32Array(3);
  for (let i = 0; i + 2 < n; i += 3) {
    const cx = (P.getX(i) + P.getX(i + 1) + P.getX(i + 2)) / 3, cy = (P.getY(i) + P.getY(i + 1) + P.getY(i + 2)) / 3, cz = (P.getZ(i) + P.getZ(i + 1) + P.getZ(i + 2)) / 3;
    const u = [(cx - mn[0]) / (sz[0] || 1), (cy - mn[1]) / (sz[1] || 1), (cz - mn[2]) / (sz[2] || 1)];
    const t = u[long];
    // face normal y/x for decal placement
    e1[0] = P.getX(i + 1) - P.getX(i); e1[1] = P.getY(i + 1) - P.getY(i); e1[2] = P.getZ(i + 1) - P.getZ(i);
    e2[0] = P.getX(i + 2) - P.getX(i); e2[1] = P.getY(i + 2) - P.getY(i); e2[2] = P.getZ(i + 2) - P.getZ(i);
    let nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0];
    const nlen = Math.hypot(nx, ny, nz) || 1; nx /= nlen; ny /= nlen; nz /= nlen;
    let mix = [0, 0, 0], k = 0, tgt = null;
    for (const L of lines) if (Math.abs(t - L.t) < L.w) { tgt = panel; k = 0.7; }
    if (long === 2 && Math.abs(nx) < 0.9) { const q = (u[0] - 0.5) * 2; if (Math.abs(q) < 0.02) { tgt = panel; k = 0.5; } }
    for (const S of stripes) if (Math.abs(t - S.t) < S.w && Math.abs(ny) < 0.6 && u[1] > S.yLo && u[1] < S.yLo + 0.3) { tgt = accent; k = 0.85; }
    // per-cell panel tone variation
    const cell = hash3(Math.floor(cx * 9 / (sz[0] || 1) + cellSeed), Math.floor(cy * 9 / (sz[1] || 1)), Math.floor(cz * 9 / (sz[2] || 1)));
    let f = 0.93 + 0.14 * cell;
    // grime: noise + undersides + rear soot
    const nz2 = hash3(Math.floor(cx * 5 / (sz[0] || 1) + cellSeed * 2), Math.floor(cy * 5 / (sz[1] || 1)), Math.floor(cz * 5 / (sz[2] || 1)));
    f *= 1 - grime * (nz2 * 0.35 + (1 - u[1]) * 0.18 + (long === 2 ? Math.max(0, u[2] - 0.75) * 0.6 : 0));
    for (let v = 0; v < 3; v++) {
      const j = (i + v) * 3;
      let rr = arr[j], gg = arr[j + 1], bb2 = arr[j + 2];
      if (tgt) { rr += (tgt[0] - rr) * k; gg += (tgt[1] - gg) * k; bb2 += (tgt[2] - bb2) * k; }
      const em = Math.max(rr, gg, bb2) > 1.15;      // never dim emissive-in-lit colours
      arr[j] = em ? rr : rr * f; arr[j + 1] = em ? gg : gg * f; arr[j + 2] = em ? bb2 : bb2 * f;
    }
  }
  C.needsUpdate = true;
  return geo;
}

// ---------------------------------------------------------------- palettes + seeded recipes
function hsl(h, s, l) {
  h = ((h % 1) + 1) % 1;
  const a = s * Math.min(l, 1 - l), f = n => { const k = (n + h * 12) % 12; return l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)); };
  return (Math.round(f(0) * 255) << 16) | (Math.round(f(8) * 255) << 8) | Math.round(f(4) * 255);
}
export function seededPalette(rng, kind) {
  const R = rng;
  if (kind === 'creature' || kind === 'boss-claw') {
    const h = pick(R, [0.97, 0.02, 0.9, 0.5, 0.08]) + rn(R, -0.02, 0.02);
    return { base: hsl(h, 0.65, 0.36), panel: hsl(h + 0.02, 0.7, 0.22), accent: hsl(h + 0.07, 1, 0.58), glow: hsl(pick(R, [0.5, 0.15, 0.78]), 1, 0.72), dark: hsl(h, 0.6, 0.07), metal: hsl(h, 0.2, 0.45) };
  }
  if (kind === 'outpost') {
    const h = rn(R, 0.72, 0.8);
    return { base: hsl(h, 0.2, 0.45), panel: hsl(h, 0.25, 0.27), accent: pick(R, [hsl(0.07, 0.9, 0.55), hsl(0.13, 0.85, 0.56), hsl(0.5, 0.8, 0.5)]), glow: hsl(pick(R, [0.12, 0.5, 0.58]), 1, 0.78), dark: hsl(h, 0.25, 0.1), metal: hsl(h, 0.08, 0.55) };
  }
  const h = pick(R, [0.77, 0.74, 0.82, 0.68, 0.06, 0.12]);
  const warm = h < 0.2;
  return { base: warm ? hsl(h, 0.55, 0.45) : hsl(h, 0.45, 0.4), panel: hsl(h + 0.01, 0.5, 0.24), accent: pick(R, [hsl(0.06, 0.9, 0.55), hsl(0.13, 0.85, 0.58), hsl(0.5, 0.75, 0.55), hsl(0.9, 0.7, 0.6)]), glow: hsl(pick(R, [0.55, 0.5, 0.12]), 0.9, 0.82), dark: hsl(h, 0.3, 0.1), metal: hsl(h, 0.07, 0.55) };
}
const N = (part, params, at, extra) => Object.assign({ part, params, at }, extra);

export function seededRecipe(kind, rng) {
  const r = rng, pal = seededPalette(r, kind), lod = 'hi';
  if (kind === 'fighter') {
    const wid = rn(r, 0.85, 1.3), nEng = pick(r, [1, 2, 2, 3]), kids = [];
    kids.push(N('canopy', { len: rn(r, 0.2, 0.3), wid: 0.09 * wid, h: rn(r, 0.06, 0.09) }, 'top_front'));
    const wingKids = [];
    const gun = r() < 0.7;
    if (gun) wingKids.push(N('barrel', { len: rn(r, 0.14, 0.24), r: 0.011 }, 'tipFwd'));
    else wingKids.push(N('pod', { len: 0.16, r: 0.025 }, 'tip', { rot: [-Math.PI / 2, 0, 0] }));
    kids.push(N('wing', { len: rn(r, 0.26, 0.48), w: rn(r, 0.26, 0.4), sweep: rn(r, 0.25, 1.0), taper: rn(r, 0.2, 0.5), glow: true }, 'side1', { mirror: 'x', children: wingKids }));
    if (r() < 0.6) kids.push(N('wing', { len: rn(r, 0.1, 0.18), w: 0.14, sweep: 0.3 }, 'side0', { mirror: 'x', offset: [0, 0, -0.04] }));
    if (r() < 0.55) kids.push(N('fin', { len: rn(r, 0.12, 0.2), w: 0.2, sweep: 0.7, glow: true }, 'top_rear'));
    else kids.push(N('fin', { len: rn(r, 0.1, 0.16), w: 0.17, sweep: 0.6 }, 'top_rearX', { mirror: 'x', rot: [0, 0, -0.4] }));
    if (nEng === 1) kids.push(N('engine', { r: 0.055, len: 0.14 }, 'tail'));
    else { kids.push(N('engine', { r: 0.042, len: 0.13 }, 'tail1', { mirror: 'x' })); if (nEng === 3) kids.push(N('engine', { r: 0.035, len: 0.12 }, 'tail')); }
    if (r() < 0.7) kids.push(N('plate', { w: 0.14 * wid, d: rn(r, 0.16, 0.26), glow: true }, 'top_mid'));
    if (r() < 0.5) kids.push(N('antenna', { len: rn(r, 0.12, 0.2) }, 'top_rearX', { mirror: 'x', offset: [0.02, 0, 0] }));
    if (r() < 0.45) kids.push(N('barrel', { len: 0.16, r: 0.014 }, 'nose'));
    return N('hull', { kind: 'dart', wid, hgt: rn(r, 0.9, 1.2), stripe: r() < 0.6 }, null, { palette: pal, lod, paint: { lines: 4, stripes: 1, grime: 0.3, base: pal.base, accent: pal.accent, panel: pal.panel }, children: kids });
  }
  if (kind === 'hauler') {
    const kids = [];
    kids.push(N('canopy', { len: rn(r, 0.22, 0.32), wid: 0.14, h: rn(r, 0.08, 0.12) }, 'top_front'));
    const nb = pick(r, [1, 2, 2]);
    const cc = () => ({ rows: 2, wall: [pal.accent, pal.base, pal.metal, pal.panel][Math.floor(r() * 4)] });
    kids.push(N('block', Object.assign({ w: 0.3, h: rn(r, 0.08, 0.16), d: rn(r, 0.2, 0.28) }, cc()), 'cargoA'));
    if (nb > 1) kids.push(N('block', Object.assign({ w: 0.26, h: rn(r, 0.06, 0.14), d: 0.2 }, cc()), 'cargoB'));
    if (r() < 0.8) kids.push(N('pod', { len: rn(r, 0.3, 0.45), r: rn(r, 0.045, 0.07) }, 'side0', { mirror: 'x', rot: [Math.PI / 2, 0, 0], offset: [0, 0.0, -0.16] }));
    kids.push(N('leg', { len: 0.14, knee: 0.04 }, 'legF', { mirror: 'x' }));
    kids.push(N('leg', { len: 0.14, knee: 0.04 }, 'legR', { mirror: 'x' }));
    const nE = pick(r, [2, 2, 3]);
    kids.push(N('engine', { r: rn(r, 0.055, 0.075), len: 0.18 }, 'tail1', { mirror: 'x' }));
    if (nE === 3) kids.push(N('engine', { r: 0.06, len: 0.16 }, 'tail'));
    if (r() < 0.7) kids.push(N('antenna', { len: rn(r, 0.14, 0.24), dish: r() < 0.5 }, 'top_rearX', { mirror: 'x', offset: [0.05, 0, 0] }));
    kids.push(N('plate', { w: 0.2, d: 0.14 }, 'top_front', { offset: [0, 0.0, 0.22] }));
    if (r() < 0.6) kids.push(N('fin', { len: 0.1, w: 0.2, sweep: 0.5 }, 'top_rear', { offset: [0, 0, 0.1] }));
    return N('hull', { kind: 'brick', wid: rn(r, 0.9, 1.15), hgt: rn(r, 0.9, 1.15), stripe: r() < 0.7 }, null, { palette: pal, lod, paint: { lines: 6, stripes: 2, grime: 0.5, base: pal.base, accent: pal.accent, panel: pal.panel }, children: kids });
  }
  if (kind === 'explorer') {
    const kids = [];
    kids.push(N('dome', { r: rn(r, 0.16, 0.22), h: rn(r, 0.1, 0.16) }, 'top_mid', { id: 'dome', children: [N('antenna', { len: 0.2, dish: true }, 'top')] }));
    kids.push(N('pod', { len: rn(r, 0.2, 0.3), r: 0.04 }, 'side0', { mirror: 'x', rot: [Math.PI / 2, 0, 0], offset: [0.02, 0, -0.12] }));
    kids.push(N('leg', { len: 0.12, knee: 0.05 }, 'legF', { mirror: 'x' }));
    kids.push(N('leg', { len: 0.12, knee: 0.05 }, 'legR', { mirror: 'x' }));
    kids.push(N('engine', { r: 0.05, len: 0.12 }, 'tail1', { mirror: 'x' }));
    kids.push(N('fin', { len: 0.1, w: 0.18, sweep: 0.7 }, 'top_rear'));
    return N('hull', { kind: 'disc', wid: 0.9, hgt: 1.2, stripe: true }, null, { palette: pal, lod, paint: { lines: 4, stripes: 2, grime: 0.3, base: pal.base, accent: pal.accent, panel: pal.panel }, children: kids });
  }
  if (kind === 'creature' || kind === 'boss-claw') {
    const boss = kind === 'boss-claw', segs = Math.round(rn(r, 5, 8.4)), kids = [], k = boss ? 1.5 : 1;
    kids.push(N('eye', { r: rn(r, 0.03, 0.045) * k }, 'eyeA', { mirror: 'x' }));
    if (r() < 0.55 || boss) kids.push(N('eye', { r: rn(r, 0.045, 0.07) * k }, 'eyeC'));
    kids.push(N('spike', { len: rn(r, 0.08, 0.16), r: 0.02 * k, bend: 0.3 }, 'jawA', { mirror: 'x' }));
    const nf = pick(r, [1, 2, 2]);
    for (let i = 1; i <= nf; i++) kids.push(N('fin', { len: rn(r, 0.15, 0.3) * k, w: rn(r, 0.12, 0.22), sweep: rn(r, 0.3, 0.9), flap: true, glow: true }, 'flank' + (i + 1 > segs - 1 ? segs - 2 : i + 1), { mirror: 'x', rot: [0, 0, 0.3] }));
    const spikeEvery = pick(r, [1, 2, 2]);
    for (let i = 1; i < segs - 1; i += spikeEvery) kids.push(N('spike', { len: rn(r, 0.06, 0.15) * k, r: 0.016 * k, bend: 0.25 }, 'dorsal' + i));
    const nT = boss ? pick(r, [3, 4]) : pick(r, [1, 2, 3]);
    for (let i = 0; i < nT; i++) kids.push(N('tentacle', { len: rn(r, 0.3, 0.55) * k, r: 0.03 * k }, i === 0 ? 'tail' : 'belly' + Math.min(segs - 1, 1 + i * 2), { rot: i === 0 ? [0, 0, 0] : [0, i * 1.7, 0] }));
    if (r() < 0.6 || boss) kids.push(N('shell', { rx: 0.14 * k, ry: 0.08 * k, rz: rn(r, 0.2, 0.3) * k }, 'dorsal' + Math.floor(segs / 2), { offset: [0, -0.03, 0] }));
    if (boss) {
      for (const idx of [1]) kids.push(N('strut', { to: [0.22, 0.02, -0.2], r: 0.03 }, 'flank' + idx, { mirror: 'x', rot: [0, 0, -1.2], children: [N('claw', { r: 0.09, open: rn(r, 0.3, 0.7) }, 'end')] }));
    } else if (r() < 0.45) {
      kids.push(N('claw', { r: 0.045, open: 0.5 }, 'flank0', { mirror: 'x' }));
    }
    return N('spine', { segs, r: (boss ? 0.15 : 0.1) * rn(r, 0.9, 1.2), len: 1 }, null, { palette: pal, lod, paint: { lines: 0, stripes: 0, grime: 0.25, base: pal.base, accent: pal.accent, panel: pal.panel }, children: kids });
  }
  if (kind === 'outpost') {
    const kids = [], R = 0.5, h = 0.035, order = [];
    for (let i = 0; i < 6; i++) order.push(i);
    for (let i = 5; i > 0; i--) { const j = Math.floor(r() * (i + 1)); const t = order[i]; order[i] = order[j]; order[j] = t; }
    const nb = Math.round(rn(r, 3, 5.4));
    for (let q = 0; q < nb; q++) {
      const i = order[q], t = r();
      if (t < 0.28) kids.push(N('block', { w: rn(r, 0.16, 0.24), h: rn(r, 0.1, 0.2), d: rn(r, 0.14, 0.22), rows: 2 }, 'b' + i, { rot: [0, rn(r, 0, 3), 0] }));
      else if (t < 0.5) kids.push(N('tank', { r: rn(r, 0.05, 0.08), h: rn(r, 0.14, 0.26) }, 'b' + i));
      else if (t < 0.7) kids.push(N('tower', { h: rn(r, 0.28, 0.5), r: 0.04 }, 'b' + i));
      else if (t < 0.88) kids.push(N('dome', { r: rn(r, 0.08, 0.13), h: rn(r, 0.06, 0.1) }, 'b' + i));
      else kids.push(N('pod', { len: rn(r, 0.16, 0.26), r: 0.045 }, 'b' + i, { rot: [0.1, 0, 0.1] }));
    }
    // pipes between consecutive buildings
    for (let q = 0; q + 1 < nb; q++) {
      if (r() < 0.75) {
        const a = padPos(order[q], R, h), bq = padPos(order[q + 1], R, h);
        kids.push(N('strut', { to: [bq[0] - a[0], 0, bq[2] - a[2]], r: 0.01 }, 'b' + order[q], { offset: [0, 0.035, 0] }));
      }
    }
    kids.push(N(pick(r, ['tower', 'dome']), { h: rn(r, 0.4, 0.6), r: 0.05, rr: 0 }, 'c'));
    if (r() < 0.8) kids.push(N('antenna', { len: rn(r, 0.25, 0.4), dish: r() < 0.5 }, 'b' + order[nb % 6], { offset: [0, 0, 0] }));
    return N('pad', { r: R, h }, null, { palette: pal, lod, paint: { lines: 0, stripes: 0, grime: 0.45, base: pal.base, accent: pal.accent, panel: pal.panel }, children: kids });
  }
  if (kind === 'gunpod') {
    const nb = pick(r, [1, 2, 2, 3]), kids = [];
    for (let i = 0; i < nb; i++) kids.push(N('barrel', { len: rn(r, 0.16, 0.26), r: 0.012 }, 'tip', { offset: [(i - (nb - 1) / 2) * 0.03, 0, 0], id: 'b' + i }));
    kids.push(N('fin', { len: 0.08, w: 0.12, sweep: 0.6 }, 'side'));
    kids.push(N('strut', { to: [0, -0.1, 0], r: 0.012 }, 'base'));
    return N('pod', { len: 0.2, r: rn(r, 0.05, 0.07) }, null, { palette: pal, lod, children: kids });
  }
  throw new Error('ship-parts: unknown recipe kind ' + kind);
}

// ---------------------------------------------------------------- dev preview
// demo(engine, {seed, scale, kinds}) -> { group, items, tris, dispose }
export function demo(engine, opts = {}) {
  const T = engine.THREE, cam = engine.camera, scene = engine.scene;
  const seed = opts.seed ?? 11;
  const list = opts.kinds || ['fighter', 'fighter', 'fighter', 'hauler', 'hauler', 'hauler', 'creature', 'creature', 'creature', 'outpost', 'outpost', 'outpost'];
  const lodMode = opts.lod || 'hi';
  const group = new T.Group(), mats = makeMaterials(T, { flex: true });
  const S = opts.scale ?? 1, cols = opts.cols || 3, sp = 1.5 * S, items = [];
  const origin = opts.origin || [0, 0, 0];
  const rows = Math.ceil(list.length / cols);
  list.forEach((kind, i) => {
    const rec = seededRecipe(kind, mulberry(seed * 977 + i * 131)); rec.lod = lodMode;
    const res = compose(T, rec, mulberry(seed + i));
    const m = new T.Group(); m.frustumCulled = false;
    const a = new T.Mesh(res.geo, mats.lit); a.frustumCulled = false; m.add(a);
    if (res.emissive) { const e = new T.Mesh(res.emissive, mats.emissive); e.frustumCulled = false; m.add(e); }
    const cx = (i % cols - (cols - 1) / 2) * sp, cy = ((rows - 1) / 2 - Math.floor(i / cols)) * sp * 0.78;
    m.position.set(cx, cy, 0); m.scale.setScalar(S); m.rotation.set(0.45, -0.7, 0);
    if (kind === 'outpost') { m.scale.setScalar(S * 0.9); m.rotation.set(0.5, 0.4, 0); }
    if (kind === 'creature') m.rotation.set(0.35, 0.8, 0);
    group.add(m); items.push({ kind, tris: res.tris, parts: res.parts.length, res });
  });
  const back = new T.Mesh(new T.SphereGeometry(40 * S, 16, 12), new T.MeshBasicMaterial({ color: 0x0b0714, side: T.BackSide, depthWrite: true }));
  back.frustumCulled = false; back.renderOrder = -10;
  group.add(back);
  group.position.set(origin[0], origin[1], origin[2]);
  scene.add(group);
  const dist = opts.dist ?? (cols * sp * 0.95 + 3 * S);
  const t0 = performance.now();
  engine.setPilot(() => {
    cam.position.set(origin[0], origin[1], origin[2] + dist);
    cam.up.set(0, 1, 0); cam.lookAt(origin[0], origin[1], origin[2]);
    cam.updateMatrixWorld(true);
    mats.tick((performance.now() - t0) / 1000, 0);
    group.children.forEach((m, i) => { if (m !== back) m.rotation.y += 0.0; });
  });
  return { group, items, tris: items.map(i => i.tris), dispose() { engine.setPilot(null); scene.remove(group); } };
}
