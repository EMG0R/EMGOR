// Procedural No Man's Sky "hauler" replica. Nose -Z, tail +Z, length 1.0, centered, up +Y.
// Two meshes: flat-shaded lit (ShaderMaterial, vertex colors) + emissive (MeshBasicMaterial).
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const C = {
  hull: 0x8E3A9C, panel: 0x5E2470, orange: 0xF05A28, tan: 0xC9A96A, glass: 0x14101E,
  light: 0xE8F6FF, ring: 0x4A7CFF, core: 0xF2E6FF, thr: 0xF4F8FF, grey: 0x8A8691, dark: 0x2A2430,
  yellow: 0xF2C94C, pink: 0xE060B0, tanDark: 0x4A3A22,
};

const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec4 mv = modelViewMatrix * vec4(position, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = max(0.0, dot(n, L)) * 0.7 + 0.3;
  float rim = pow(1.0 - max(0.0, dot(n, normalize(-vV))), 3.0) * 0.28;
  vec3 col = vC * d + rim * vec3(0.5, 0.3, 0.85);
  gl_FragColor = vec4(col, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// measured from the full hull so the low-LOD hull lands on exactly the same centre + scale
const NORM_FULL = { x: 0, y: 1.0750000476837158, z: 0.16249990463256836, s: 0.09341429305249369 };

export function buildHull(THREE, opts) {
  const low = !!(opts && opts.lod === 'low');   // rev 12: dock LOD = fuselage + wings + fin + engine ring only
  const lit = [], emi = [];
  const V3 = (x, y, z) => new THREE.Vector3(x, y, z);
  const hash = (x, y, z) => {
    const s = Math.sin(Math.round(x * 50) * 12.9898 + Math.round(y * 50) * 78.233 + Math.round(z * 50) * 37.719) * 43758.5453;
    return s - Math.floor(s);
  };
  function put(geo, hex, emis = false, jit = 0.14) {
    if (geo.index) geo = geo.toNonIndexed();
    geo.deleteAttribute('normal'); geo.deleteAttribute('uv');
    const col = new THREE.Color(hex), p = geo.attributes.position, n = p.count;
    const arr = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const f = emis ? 1 : (1 - jit * hash(p.getX(i), p.getY(i), p.getZ(i))) * (0.86 + 0.14 * Math.min(1, Math.max(0, (p.getY(i) + 1.5) / 3)));
      arr[i * 3] = col.r * f; arr[i * 3 + 1] = col.g * f; arr[i * 3 + 2] = col.b * f;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(arr, 3));
    (emis ? emi : lit).push(geo);
  }
  const box = (w, h, d, x, y, z, hex, o = {}) => {
    const g = new THREE.BoxGeometry(w, h, d);
    if (o.rx || o.ry || o.rz) g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(o.rx || 0, o.ry || 0, o.rz || 0)));
    g.translate(x, y, z); put(g, hex, o.emis, o.jit);
  };
  const cylZ = (rt, rb, len, seg, x, y, z, hex, o = {}) => {
    const g = new THREE.CylinderGeometry(rt, rb, len, seg); g.rotateX(Math.PI / 2);
    g.translate(x, y, z); put(g, hex, o.emis, o.jit);
  };
  const rod = (a, b, r, hex, seg = 5) => {
    const d = new THREE.Vector3().subVectors(b, a), len = d.length();
    const g = new THREE.CylinderGeometry(r, r, len, seg);
    g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(V3(0, 1, 0), d.normalize()));
    g.translate((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2); put(g, hex);
  };
  // loft of rectangular sections [z, halfW, yBottom, yTop, cx]
  const loft = (secs, hex, jit) => {
    const pos = [];
    const cs = secs.map(([z, w, yb, yt, cx = 0]) => [V3(cx - w, yb, z), V3(cx + w, yb, z), V3(cx + w, yt, z), V3(cx - w, yt, z)]);
    const tri = (a, b, c) => pos.push(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
    for (let i = 0; i < cs.length - 1; i++) for (let k = 0; k < 4; k++) {
      const a = cs[i][k], b = cs[i][(k + 1) % 4], c = cs[i + 1][(k + 1) % 4], d = cs[i + 1][k];
      tri(a, b, c); tri(a, c, d);
    }
    for (const s of [cs[0], cs[cs.length - 1]]) { tri(s[0], s[1], s[2]); tri(s[0], s[2], s[3]); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    put(g, hex, false, jit);
  };
  // flat shape decal on a flank (shape x -> along Z, y -> up), proud of face by 0.002
  const sideShape = (pts, side, zc, yc, hex, face = 1.5) => {
    const sh = new THREE.Shape(pts.map(([x, y]) => new THREE.Vector2(x, y)));
    const g = new THREE.ExtrudeGeometry(sh, { depth: 0.01, bevelEnabled: false });
    if (side > 0) { g.rotateY(Math.PI / 2); g.translate(face + 0.002, yc, zc); }
    else { g.rotateY(-Math.PI / 2); g.translate(-face - 0.002, yc, zc); }
    put(g, hex, false, 0.06);
  };
  const flank = (z0, z1, y0, y1, hex, side, face = 1.5, jit = 0.06) =>
    box(0.01, y1 - y0, z1 - z0, side * (face + 0.007), (y0 + y1) / 2, (z0 + z1) / 2, hex, { jit });

  // ---- fuselage ----
  loft([
    [-4.7, 0.5, -0.15, 0.5], [-3.9, 0.95, -0.55, 0.75], [-3.0, 1.35, -0.85, 0.9],
    [-2.4, 1.5, -0.9, 0.9], [0.4, 1.5, -0.9, 0.9], [3.2, 1.5, -0.9, 0.9],
    [4.2, 1.32, -0.82, 0.86], [4.7, 1.25, -0.78, 0.8],
  ], C.hull, 0.2);
  // lower dark belly + side skirt panels
  loft([[-3.2, 1.2, -1.15, -0.8], [-1.0, 1.55, -1.05, -0.8], [3.0, 1.55, -1.05, -0.8], [4.4, 1.2, -0.95, -0.78]], C.panel, 0.2);
  // upper spine plate
  loft([[-2.4, 1.1, 0.9, 1.0], [3.0, 1.2, 0.9, 1.0]], C.panel, 0.2);
  // grey mid-band panel on flanks (as in the photo)
  if (!low) for (const s of [-1, 1]) {
    flank(-1.9, 1.3, -0.5, 0.35, C.grey, s, 1.5, 0.2);
    flank(-3.0, -1.9, -0.55, 0.2, C.panel, s, 1.35 + 0.0, 0.1);
  }

  // ---- nose ----
  box(0.9, 0.5, 0.5, 0, 0.1, -4.8, C.panel);
  for (const s of low ? [] : [-1, 1]) {
    cylZ(0.14, 0.14, 1.3, 8, s * 0.9, -0.4, -4.5, C.dark);
    cylZ(0.2, 0.2, 0.5, 8, s * 0.9, -0.4, -3.9, C.grey);
    cylZ(0.1, 0.1, 0.06, 8, s * 0.9, -0.4, -5.16, C.light, { emis: true });
  }

  // ---- cockpit canopy ----
  loft([[-3.3, 0.8, 0.7, 0.8], [-2.4, 1.05, 0.8, 1.55], [-1.3, 1.05, 0.8, 1.55], [-0.5, 1.2, 0.8, 0.95]], C.glass, 0.05);
  // frame bars
  if (!low) for (const s of [-1, 1]) {
    box(0.07, 0.07, 1.15, s * 1.05, 1.56, -1.85, C.grey, { jit: 0.05 });
    rod(V3(s * 1.05, 1.55, -2.4), V3(s * 0.8, 0.78, -3.3), 0.04, C.grey);
    rod(V3(s * 1.05, 1.55, -1.3), V3(s * 1.2, 0.95, -0.5), 0.04, C.grey);
    box(0.08, 0.08, 0.08, s * 1.06, 1.58, -2.4, C.light, { emis: true });
    box(0.08, 0.08, 0.08, s * 1.06, 1.58, -1.3, C.light, { emis: true });
  }
  if (!low) {
  box(2.1, 0.06, 0.07, 0, 1.56, -2.4, C.grey, { jit: 0.05 });
  box(2.1, 0.06, 0.07, 0, 1.56, -1.3, C.grey, { jit: 0.05 });
  box(0.07, 0.07, 1.1, 0, 1.57, -1.85, C.grey, { jit: 0.05 });
  box(0.9, 0.04, 0.04, 0, 0.82, -3.3, C.light, { emis: true });
  }

  // ---- three stacked ribbed boosters (top rear) ----
  const boosters = [[-0.55, 1.28], [0.55, 1.28], [0, 1.95]];
  for (const [bx, by] of low ? [] : boosters) {
    for (let i = 0; i < 6; i++) {
      const z = 1.1 + i * 0.36;
      cylZ(0.46, 0.46, 0.3, 8, bx, by, z, i % 2 ? C.dark : C.grey);
      cylZ(0.52, 0.52, 0.08, 8, bx, by, z + 0.2, C.dark);
    }
    cylZ(0.3, 0.3, 0.05, 8, bx, by, 3.3, C.orange, { emis: true });
  }
  if (!low) box(1.8, 0.4, 2.2, 0, 1.0, 2.2, C.panel); // saddle under tanks

  // ---- tail fin (extruded swept profile) ----
  {
    const sh = new THREE.Shape();
    sh.moveTo(1.6, 0.0); sh.lineTo(4.5, 0.0); sh.lineTo(5.0, 3.2); sh.lineTo(4.55, 3.4); sh.lineTo(3.1, 1.2); sh.lineTo(1.6, 0.7); sh.closePath();
    const g = new THREE.ExtrudeGeometry(sh, { depth: 0.14, bevelEnabled: false });
    g.rotateY(-Math.PI / 2); // shape x -> +Z, depth -> -X
    g.translate(-1.07, 0.85, 0); // x spans -1.21..-1.07 (left of boosters)
    put(g, C.panel, false, 0.2);
    const g2 = new THREE.BoxGeometry(0.16, 0.12, 1.2); g2.rotateX(-0.52); g2.translate(-1.14, 1.9, 4.05); put(g2, C.orange, false, 0.05);
  }
  // small secondary fin on opposite side
  if (!low) box(0.1, 0.9, 1.0, 1.2, 1.35, 3.9, C.panel, { rx: -0.3 });

  // ---- side cargo pods hanging off the flanks ----
  for (const s of [-1, 1]) {
    if (!low) {
    box(0.85, 1.3, 1.9, s * 1.9, -0.05, -1.7, C.panel);
    box(0.9, 0.12, 1.95, s * 1.9, 0.62, -1.7, C.hull);
    for (let i = 0; i < 6; i++) box(0.04, 1.0, 0.1, s * (2.34), -0.05, -2.4 + i * 0.28, C.dark, { jit: 0.05 });
    box(0.5, 0.5, 0.8, s * 1.85, -0.2, -0.4, C.grey);
    }
    // rear cargo/engine pods with thrusters (wings)
    box(1.1, 1.7, 2.3, s * 1.85, 0.0, 3.85, C.hull);
    box(1.15, 0.15, 2.35, s * 1.85, 0.9, 3.85, C.panel);
    box(1.15, 0.15, 2.35, s * 1.85, -0.9, 3.85, C.panel);
    // wing arms with hooked ends
    box(1.7, 0.22, 0.4, s * 3.0, 0.75, 3.3, C.dark);
    box(0.25, 0.9, 0.35, s * 3.8, 0.35, 3.3, C.grey);
    if (!low) {
    box(0.35, 0.3, 0.5, s * 3.8, -0.2, 3.35, C.grey);
    // underslung bits
    cylZ(0.16, 0.16, 0.5, 6, s * 1.95, -1.15, 3.6, C.grey);
    cylZ(0.1, 0.1, 0.1, 6, s * 1.95, -1.15, 3.9, C.yellow, { emis: true });
    // antenna rods
    rod(V3(s * 3.8, 0.35, 3.3), V3(s * 6.4, 0.55, 2.4), 0.025, C.dark, 4);
    rod(V3(s * 3.8, -0.1, 3.4), V3(s * 6.0, -0.3, 4.4), 0.025, C.dark, 4);
    }
    // thruster slots (rear faces), two stacked
    for (const y of low ? [] : [0.42, -0.42]) {
      box(0.78, 0.6, 0.06, s * 1.85, y, 5.02, C.dark);
      box(0.66, 0.48, 0.05, s * 1.85, y, 5.06, C.thr, { emis: true });
    }
  }

  // ---- main engine ----
  cylZ(1.12, 1.12, 1.0, 20, 0, 0, 4.85, C.dark);
  cylZ(1.2, 1.2, 0.18, 20, 0, 0, 4.5, C.grey);
  cylZ(1.0, 1.0, 0.1, 20, 0, 0, 5.38, C.panel);
  for (let i = 0; i < 16; i++) {
    const a = (i / 16) * Math.PI * 2, r = 0.8;
    const g = new THREE.BoxGeometry(0.3, 0.2, 0.06);
    g.rotateZ(a + Math.PI / 2); g.translate(Math.cos(a) * r, Math.sin(a) * r, 5.45);
    put(g, i % 2 ? 0x3A66E0 : C.ring, true);
  }
  for (let i = 0; i < (low ? 0 : 24); i++) {
    const a = (i / 24) * Math.PI * 2, r = 0.55;
    const g = new THREE.BoxGeometry(0.07, 0.07, 0.05);
    g.translate(Math.cos(a) * r, Math.sin(a) * r, 5.44); put(g, 0xB8C8FF, true);
  }
  cylZ(0.42, 0.42, 0.05, 16, 0, 0, 5.46, 0xB070FF, { emis: true });
  cylZ(0.3, 0.3, 0.05, 16, 0, 0, 5.49, C.core, { emis: true });

  // ---- decals on both flanks ----
  const V = [[-0.65, 0.45], [-0.35, 0.45], [0, -0.1], [0.35, 0.45], [0.65, 0.45], [0, -0.6]];
  for (const s of low ? [] : [-1, 1]) {
    flank(0.3, 1.1, -0.35, 0.5, C.orange, s);            // orange block
    sideShape(V, s, 1.95, 0.0, C.orange);                // V chevron
    sideShape([[-0.2, 0], [0.2, 0], [0, 0.35]], s, 1.95, -0.62, C.yellow); // yellow triangle
    flank(-2.5, -2.1, 0.1, 0.45, C.pink, s, 1.4);        // small magenta marks
    flank(-2.0, -1.8, -0.4, -0.2, C.orange, s);
    flank(2.9, 3.1, 0.1, 0.25, C.yellow, s);
    // vents
    for (let i = 0; i < 4; i++) flank(-0.5, -0.1, -0.1 - i * 0.07, -0.07 - i * 0.07, C.dark, s);
    // tan cargo band with dark stripes, rear flank
    flank(3.0, 4.2, -0.7, 0.7, C.tan, s, 1.5 - 0.0, 0.12);
    for (let i = 0; i < 7; i++) flank(3.05 + i * 0.16, 3.13 + i * 0.16, -0.7, 0.7, C.tanDark, s, 1.5 + 0.003, 0.05);
  }

  // ---- landing legs ----
  for (const s of low ? [] : [-1, 1]) for (const z of [-1.9, 2.6]) {
    rod(V3(s * 1.2, -0.95, z), V3(s * 1.45, -2.0, z + (z < 0 ? -0.1 : 0.1)), 0.09, C.grey);
    rod(V3(s * 1.2, -1.0, z + 0.25), V3(s * 1.45, -1.95, z + 0.05), 0.05, C.dark);
    box(0.7, 0.1, 0.95, s * 1.5, -2.05, z, C.grey, { jit: 0.2 });
  }

  // ---- merge, normalize ----
  const thrustersRaw = [V3(0, 0, 5.52), V3(-1.85, 0.42, 5.1), V3(-1.85, -0.42, 5.1), V3(1.85, 0.42, 5.1), V3(1.85, -0.42, 5.1)];
  const muzzlesRaw = [V3(-0.9, -0.4, -5.2), V3(0.9, -0.4, -5.2)];
  const litG = mergeGeometries(lit), emiG = mergeGeometries(emi);
  const bb = new THREE.Box3().setFromBufferAttribute(litG.attributes.position);
  bb.union(new THREE.Box3().setFromBufferAttribute(emiG.attributes.position));
  let ctr = bb.getCenter(new THREE.Vector3()), s = 1 / (bb.max.z - bb.min.z);
  const normUsed = { x: ctr.x, y: ctr.y, z: ctr.z, s };
  if (low && NORM_FULL) { ctr = new THREE.Vector3(NORM_FULL.x, NORM_FULL.y, NORM_FULL.z); s = NORM_FULL.s; }   // same normalization as the full hull
  for (const g of [litG, emiG]) { g.translate(-ctr.x, -ctr.y, -ctr.z); g.scale(s, s, s); }
  const fix = v => v.sub(ctr).multiplyScalar(s);

  const litMat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, vertexColors: true, side: THREE.DoubleSide });
  const emiMat = new THREE.MeshBasicMaterial({ vertexColors: true, toneMapped: false, side: THREE.DoubleSide });
  const group = new THREE.Group();
  const m1 = new THREE.Mesh(litG, litMat), m2 = new THREE.Mesh(emiG, emiMat);
  m1.frustumCulled = m2.frustumCulled = false;
  group.add(m1, m2);
  group.userData.thrusters = thrustersRaw.map(fix);
  group.userData.gunMuzzles = muzzlesRaw.map(fix);
  group.userData.norm = normUsed;
  group.userData.triangles = (litG.attributes.position.count + emiG.attributes.position.count) / 3;
  return group;
}
