// On-foot traveller (NMS vibe). Height = 1.0 unit, forward = -Z (same as the ship nose), up +Y, feet at y=0.
// SCALE: the hull is 1 unit long and a human should be ~0.09 of that, so the caller does group.scale.setScalar(0.09 * shipScale).
// One SkinnedMesh (lit, flat-shaded ShaderMaterial, vertex colors) + one emissive SkinnedMesh (visor, chest strip, jet glow) sharing a skeleton = 2 draw calls, ~700 tris.
// API: createHuman(THREE, {color, suit:{jetpack,scanner,sprint,storage 0..3}, role:'pilot'|'miner'|'chef'|'scout'|null, pet:{seed,plan}|null})
//   -> { group, update(dt, {moving,running,airborne,speed 0..1,facing rad, jet 0..1, scan 0..1}), setColor(hex), setSuit({suit,role}), pet, dispose }
//   group.userData.jetNozzles = Vector3[] (human-local nozzle exits, for the flame fx) ; group.userData.jetBone = the bone those are relative to (world = jetBone's parent chain, use group-local)
//   createPet(THREE, {seed, plan:'walker'|'jelly'|'strider'|0..2}) -> { group, update(dt), follow(target, dt), setMood(0..1), dispose }  (0.4 human height, <=600 tris)
// Suit attachments live in their own skinned mesh pair on the same skeleton, so setSuit rebuilds only those.
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { PARTS, mulberry as pmul } from './ship-parts.js';

const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  #include <skinbase_vertex>
  #include <begin_vertex>
  #include <skinning_vertex>
  vec4 mv = modelViewMatrix * vec4(transformed, 1.0);
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
  gl_FragColor = vec4(vC * d + rim * vec3(0.5, 0.3, 0.85), 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// bone name -> world-space joint position at bind pose, parent
const JOINTS = {
  hips: [0, 0.52, 0, null], torso: [0, 0.56, 0, 'hips'], head: [0, 0.80, 0, 'torso'],
  shL: [-0.15, 0.76, 0, 'torso'], elL: [-0.16, 0.58, 0, 'shL'],
  shR: [0.15, 0.76, 0, 'torso'], elR: [0.16, 0.58, 0, 'shR'],
  hpL: [-0.065, 0.50, 0, 'hips'], knL: [-0.065, 0.27, 0, 'hpL'],
  hpR: [0.065, 0.50, 0, 'hips'], knR: [0.065, 0.27, 0, 'hpR'],
  pack: [0, 0.66, 0.125, 'torso'], jet: [0, 0.44, 0.15, 'pack'],
};
const BONES = Object.keys(JOINTS);

export function createHuman(THREE, opts = {}) {
  const theme = new THREE.Color(opts.color != null ? opts.color : 0xB86CFF);
  const DARK = 0x2A2430, SUIT = 0x3A3446, GREY = 0x8A8691;
  const bi = n => BONES.indexOf(n);
  const MARK = 0xFF00FF;      // part colour that marks theme-tinted verts (g ~ 0 survives the parts' brightness jitter); setColor repaints them
  const PAL = { base: SUIT, panel: DARK, accent: MARK, glow: 0xF4F8FF, dark: DARK, metal: GREY };

  // skinned geometry for one LOD: hi = full detail, lo = parts' lo variants (same bones, same layout)
  function mkBuilder(LO) {
    const parts = [], emiParts = [], rng = pmul(77);
    function put(geo, hex, bone, o = {}) {
      geo = geo.index ? geo.toNonIndexed() : geo;
      geo.deleteAttribute('normal'); geo.deleteAttribute('uv');
      const n = geo.attributes.position.count, c = new THREE.Color(hex);
      const col = new Float32Array(n * 3), si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
      const shade = o.emis ? 1 : 0.9 + 0.1 * Math.sin(geo.attributes.position.getY(0) * 40);
      for (let i = 0; i < n; i++) {
        col[i * 3] = c.r * shade; col[i * 3 + 1] = c.g * shade; col[i * 3 + 2] = c.b * shade;
        si[i * 4] = bi(bone); sw[i * 4] = 1;
      }
      geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
      geo.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(si, 4));
      geo.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4));
      geo.userData = { panel: !!o.panel, kind: o.emis ? 'e' : 'l', tint: !!o.tint };
      (o.emis ? emiParts : parts).push(geo);
    }
    // a ship-parts part, rigidly bound to one bone. pos/rot place it in the human's bind pose; keeps the part's own colours.
    function putPart(name, params, pos, rot, bone, o = {}) {
      const res = PARTS[name](THREE, Object.assign({ lod: 'lo', pal: PAL }, params), rng);
      const m = new THREE.Matrix4().compose(new THREE.Vector3(pos[0], pos[1], pos[2]), new THREE.Quaternion().setFromEuler(new THREE.Euler(rot[0], rot[1], rot[2])), new THREE.Vector3(1, 1, 1));
      for (const k of ['geo', 'emissive']) {
        const g = res[k]; if (!g) continue;
        g.applyMatrix4(m);
        for (const a of ['normal', 'uv', 'aFx', 'aPiv', 'aLm']) g.deleteAttribute(a);
        const n = g.attributes.position.count, si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
        for (let i = 0; i < n; i++) { si[i * 4] = bi(bone); sw[i * 4] = 1; }
        g.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(si, 4)); g.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4));
        // theme mask: marker-coloured verts (g ~ 0, r and b high)
        const C = g.attributes.color, mk = [];
        for (let i = 0; i < n; i++) mk.push(k === 'geo' && C.getY(i) < 0.02 && C.getX(i) > 0.15 && C.getZ(i) > 0.15 ? 1 : 0);
        g.userData = { panel: false, mask: mk, kind: k === 'geo' ? 'l' : 'e', tint: false };
        (k === 'geo' ? parts : emiParts).push(g);
      }
    }
    const box = (w, h, d, x, y, z, hex, bone, o) => { const g = new THREE.BoxGeometry(w, h, d); if (o && o.ry) g.rotateY(o.ry); g.translate(x, y, z); put(g, hex, bone, o); };
    // tapered limb segment between two world points, 6-sided
    const seg = (a, b, r0, r1, hex, bone, o) => {
      const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), len = A.distanceTo(B);
      const g = new THREE.CylinderGeometry(r0, r1, len, LO ? 4 : 6, 1);
      g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), B.clone().sub(A).normalize()));
      g.translate((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2); put(g, hex, bone, o);
    };
    function finish() {
      if (!parts.length || !emiParts.length) return null;
      const geoLit = mergeGeometries(parts), geoEmi = mergeGeometries(emiParts);
      const panelMask = [], emiTint = [];
      parts.forEach(g => { for (let i = 0; i < g.attributes.position.count; i++) panelMask.push(g.userData.mask ? g.userData.mask[i] : (g.userData.panel ? 1 : 0)); });
      emiParts.forEach(g => { for (let i = 0; i < g.attributes.position.count; i++) emiTint.push(g.userData.tint ? 1 : 0); });
      return { geoLit, geoEmi, panelMask, emiTint };
    }
    return { put, putPart, box, seg, finish };
  }
  function genGeo(lod) {
    const LO = lod === 'lo', { put, putPart, box, seg, finish } = mkBuilder(LO);
    // torso (+ pelvis on hips)
    box(0.30, 0.10, 0.17, 0, 0.50, 0, DARK, 'hips');
    box(0.31, 0.24, 0.19, 0, 0.67, 0, SUIT, 'torso');
    box(0.20, 0.13, 0.02, 0, 0.68, -0.105, theme, 'torso', { panel: true });         // chest panel
    box(0.09, 0.025, 0.012, 0, 0.72, -0.118, 0xFFFFFF, 'torso', { emis: true });       // chest strip
    box(0.33, 0.05, 0.20, 0, 0.79, 0, DARK, 'torso');                                  // collar/shoulders
    // backpack: frame plate + twin air tanks + jet engine (all ship-parts)
    box(0.22, 0.30, 0.07, 0, 0.66, 0.125, DARK, 'pack');
    putPart('tank', { r: 0.042, h: 0.2 }, [-0.058, 0.55, 0.2], [0, 0, 0], 'pack');
    putPart('tank', { r: 0.042, h: 0.2 }, [0.058, 0.55, 0.2], [0, 0, 0], 'pack');
    putPart('plate', { w: 0.16, d: 0.05, th: 0.012 }, [0, 0.8, 0.19], [Math.PI / 2, 0, 0], 'pack');
    putPart('engine', { r: 0.034, len: 0.08 }, [0, 0.52, 0.15], [Math.PI, 0, 0], 'pack');
    // head: helmet + visor + neck
    box(0.07, 0.04, 0.07, 0, 0.82, 0, DARK, 'head');
    { const g = new THREE.SphereGeometry(0.115, LO ? 6 : 10, LO ? 4 : 7); g.translate(0, 0.925, 0); put(g, SUIT, 'head'); }
    box(0.17, 0.065, 0.06, 0, 0.93, -0.085, 0xFFFFFF, 'head', { emis: true, tint: true });   // visor (tinted by theme)
    putPart('antenna', { len: 0.1, dish: false }, [0.09, 1.0, 0.04], [0, 0, -0.15], 'head');
    putPart('plate', { w: 0.05, d: 0.06, th: 0.02 }, [-0.115, 0.92, 0], [0, 0, Math.PI / 2], 'head');     // ear pods
    putPart('plate', { w: 0.05, d: 0.06, th: 0.02 }, [0.115, 0.92, 0], [0, 0, -Math.PI / 2], 'head');
    // arms
    for (const s of ['L', 'R']) {
      const x = s === 'L' ? -1 : 1, sh = 'sh' + s, el = 'el' + s;
      { const g = new THREE.SphereGeometry(0.06, LO ? 4 : 6, LO ? 3 : 4); g.translate(x * 0.15, 0.76, 0); put(g, theme, sh, { panel: true }); }
      seg([x * 0.15, 0.76, 0], [x * 0.16, 0.58, 0], 0.045, 0.04, SUIT, sh);
      seg([x * 0.16, 0.58, 0], [x * 0.16, 0.43, 0], 0.04, 0.035, SUIT, el);
      box(0.07, 0.07, 0.08, x * 0.16, 0.40, 0, DARK, el);                                // glove
    }
    // legs
    for (const s of ['L', 'R']) {
      const x = s === 'L' ? -1 : 1, hp = 'hp' + s, kn = 'kn' + s;
      seg([x * 0.065, 0.50, 0], [x * 0.065, 0.27, 0], 0.065, 0.05, SUIT, hp);
      box(0.07, 0.07, 0.07, x * 0.065, 0.34, -0.03, theme, hp, { panel: true });         // thigh panel
      seg([x * 0.065, 0.27, 0], [x * 0.065, 0.07, 0], 0.05, 0.045, DARK, kn);
      box(0.095, 0.07, 0.17, x * 0.065, 0.035, -0.03, GREY, kn);                         // boot
    }
    return finish();
  }
  const hi = genGeo('hi');
  const { geoLit, geoEmi, panelMask, emiTint } = hi;

  // skeleton
  const bones = {}, list = [];
  for (const n of BONES) { const b = new THREE.Bone(); b.name = n; bones[n] = b; list.push(b); }
  for (const n of BONES) {
    const [x, y, z, p] = JOINTS[n];
    if (p) { const pj = JOINTS[p]; bones[n].position.set(x - pj[0], y - pj[1], z - pj[2]); bones[p].add(bones[n]); }
    else bones[n].position.set(x, y, z);
  }
  const skeleton = new THREE.Skeleton(list);
  const litMat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, vertexColors: true });
  const emiMat = new THREE.MeshBasicMaterial({ vertexColors: true });
  const lit = new THREE.SkinnedMesh(geoLit, litMat), emi = new THREE.SkinnedMesh(geoEmi, emiMat);
  for (const m of [lit, emi]) { m.frustumCulled = false; m.add(bones.hips); }
  lit.bind(skeleton); emi.bind(skeleton);
  const group = new THREE.Group();
  group.add(lit, emi);
  group.userData.eyeHeight = 0.93;
  group.userData.headPos = new THREE.Vector3(0, 0.925, 0);

  const loSets = [];     // low-LOD geometry sets (built lazily) that setColor must repaint too
  function paintSet(S) {
    const lc = S.geoLit.attributes.color, ec = S.geoEmi.attributes.color;
    for (let i = 0; i < S.panelMask.length; i++) if (S.panelMask[i]) lc.setXYZ(i, theme.r, theme.g, theme.b);
    for (let i = 0; i < S.emiTint.length; i++) if (S.emiTint[i]) ec.setXYZ(i, theme.r * 0.6 + 0.4, theme.g * 0.6 + 0.4, theme.b * 0.6 + 0.4);
    lc.needsUpdate = ec.needsUpdate = true;
  }
  function setColor(hex) { theme.set(hex); paintSet(hi); loSets.forEach(paintSet); if (attSet.cur) paintSet(attSet.cur); }
  paintSet(hi);

  // ── suit attachments: own skinned mesh pair on the same skeleton; setSuit rebuilds only these ──
  const ROLES = {
    pilot: { c: 0x4DA8FF, g: ['..#..', '..#..', '#####', '..#..', '.###.'] },
    miner: { c: 0xFFB020, g: ['.###.', '#.#.#', '..#..', '..#..', '..#..'] },
    chef:  { c: 0xFF5A4D, g: ['.#.#.', '#####', '#####', '.###.', '.###.'] },
    scout: { c: 0x59E08A, g: ['.###.', '#...#', '#.#.#', '#...#', '.###.'] },
  };
  const clampT = v => Math.max(0, Math.min(3, Math.round(+v || 0)));
  const cur = { suit: { jetpack: 0, scanner: 0, sprint: 0, storage: 0 }, role: null };
  function applySuit(o) {
    if (!o) return;
    const su = o.suit || (('jetpack' in o || 'scanner' in o || 'sprint' in o || 'storage' in o) ? o : null);
    if (su) for (const k of ['jetpack', 'scanner', 'sprint', 'storage']) if (k in su) cur.suit[k] = clampT(su[k]);
    if ('role' in o) cur.role = ROLES[o.role] ? o.role : null;
  }
  applySuit(opts);
  const jetNozzles = [];
  function genAtt() {
    const su = cur.suit, role = cur.role, { put, putPart, box, seg, finish } = mkBuilder(true);
    const J = su.jetpack, S = su.scanner, P = su.sprint, K = su.storage;
    jetNozzles.length = 0;
    box(0.002, 0.002, 0.002, 0, 0.5, 0, DARK, 'hips');      // keeps the lit set non-empty at tier 0
    // jetpack: bigger outer tanks + extra nozzles; every nozzle gets a flame box on the 'jet' bone (scaled by state.jet)
    const noz = [[0, 0.52, 0.15, 0.034]];
    if (J >= 1) for (const sx of [-1, 1]) noz.push([sx * 0.085, 0.52, 0.15, 0.026]);
    if (J >= 2) for (const sx of [-1, 1]) noz.push([sx * 0.125, 0.52, 0.165, 0.021]);
    if (J >= 3) for (const sx of [-1, 1]) noz.push([sx * 0.045, 0.52, 0.215, 0.021]);
    for (const [x, y, z, r] of noz) {
      if (x !== 0 || r !== 0.034) putPart('engine', { r, len: 0.08 }, [x, y, z], [Math.PI, 0, 0], 'pack');
      box(r * 1.5, 0.07, r * 1.5, x, 0.405, z, 0xFFA040, 'jet', { emis: true });
      jetNozzles.push(new THREE.Vector3(x, 0.44, z));
    }
    if (J >= 1) for (const sx of [-1, 1]) putPart('tank', { r: 0.03 + 0.006 * J, h: 0.14 + 0.03 * J }, [sx * (0.1 + 0.006 * J), 0.54, 0.2], [0, 0, 0], 'pack');
    if (J >= 2) putPart('plate', { w: 0.24, d: 0.04, th: 0.012, glow: J >= 3 }, [0, 0.745, 0.238], [Math.PI / 2, 0, 0], 'pack');
    // scanner: visor wings + glow antenna
    if (S >= 1) {
      for (const sx of [-1, 1]) box(0.05 + 0.01 * S, 0.055 + 0.01 * S, 0.035, sx * 0.108, 0.93, -0.06, 0xFFFFFF, 'head', { emis: true, tint: true, ry: -sx * 0.9 });
      putPart('antenna', { len: 0.07 + 0.03 * S, dish: S >= 2 }, [-0.07, 1.0, 0.04], [0, 0, 0.15], 'head');
    }
    // sprint: leg armor
    for (const sx of [-1, 1]) {
      const kn = sx < 0 ? 'knL' : 'knR', hp = sx < 0 ? 'hpL' : 'hpR', x = sx * 0.065;
      if (P >= 1) { putPart('plate', { w: 0.055, d: 0.12, th: 0.014, glow: P >= 3 }, [x, 0.17, -0.046], [-Math.PI / 2, 0, 0], kn); box(0.018, 0.09, 0.006, x, 0.17, -0.062, theme, kn, { panel: true }); }
      if (P >= 2) putPart('plate', { w: 0.07, d: 0.09, th: 0.014 }, [x, 0.44, -0.058], [-Math.PI / 2, 0, 0], hp);
      if (P >= 3) { box(0.075, 0.05, 0.04, x, 0.27, -0.05, DARK, kn); box(0.07, 0.03, 0.05, x, 0.08, 0.06, theme, kn, { panel: true }); }
    }
    // storage: side pouches on the belt
    const pz = [[0.03], [0.03, 0.105], [0.03, 0.105, -0.045]][Math.max(0, K - 1)];
    if (K >= 1) for (const sx of [-1, 1]) pz.forEach((z, i) => {
      const big = K >= 3 && i === 0;
      box(0.045, big ? 0.09 : 0.07, big ? 0.085 : 0.07, sx * 0.17, 0.5, z, DARK, 'hips');
      box(0.05, 0.02, big ? 0.09 : 0.075, sx * 0.17, 0.535 + (big ? 0.01 : 0), z, theme, 'hips', { panel: true });
    });
    // role: chest emblem glyph (pixel quads) + belt tool
    if (role) {
      const R = ROLES[role], cell = 0.0145;
      R.g.forEach((row, ry) => { for (let rx = 0; rx < 5; rx++) if (row[rx] === '#') {
        const g = new THREE.PlaneGeometry(cell * 0.92, cell * 0.92); g.rotateY(Math.PI); g.translate((rx - 2) * cell, 0.66 + (2 - ry) * cell, -0.1195); put(g, R.c, 'torso', { emis: true });
      } });
      const hy = 0.52, hz = -0.105, A = [-0.11, hy, hz], B = [0.06, hy, hz];
      if (role === 'pilot') { seg(A, B, 0.008, 0.008, GREY, 'hips'); box(0.05, 0.04, 0.014, -0.125, hy, hz, GREY, 'hips'); box(0.02, 0.02, 0.016, -0.14, hy, hz, DARK, 'hips'); }
      else if (role === 'miner') { seg(A, B, 0.009, 0.009, 0x7A5A3A, 'hips'); box(0.016, 0.1, 0.016, -0.12, hy, hz, GREY, 'hips'); box(0.016, 0.016, 0.04, -0.12, hy + 0.05, hz, GREY, 'hips'); }
      else if (role === 'chef') { seg(A, B, 0.006, 0.006, 0x9A8A70, 'hips'); const g = new THREE.SphereGeometry(0.03, 5, 3); g.scale(1, 0.5, 1); g.translate(-0.125, hy, hz); put(g, GREY, 'hips'); }
      else { seg([-0.1, hy, hz], B, 0.017, 0.017, DARK, 'hips'); box(0.014, 0.034, 0.034, -0.105, hy, hz, 0xBFFFE0, 'hips', { emis: true }); }
    }
    return finish();
  }
  const attLit = new THREE.SkinnedMesh(new THREE.BufferGeometry(), litMat), attEmi = new THREE.SkinnedMesh(new THREE.BufferGeometry(), emiMat);
  for (const m of [attLit, attEmi]) { m.frustumCulled = false; m.bind(skeleton, lit.bindMatrix); }
  group.add(attLit, attEmi);
  const attSet = { cur: null, lo: null };       // lo = the low-LOD twin's mesh pair (created lazily)
  function setSuit(o) {
    applySuit(o);
    const S = genAtt(); if (!S) return;
    for (const g of [attLit.geometry, attEmi.geometry]) g.dispose();
    attLit.geometry = S.geoLit; attEmi.geometry = S.geoEmi;
    if (attSet.lo) { attSet.lo[0].geometry = S.geoLit; attSet.lo[1].geometry = S.geoEmi; }
    attSet.cur = S; paintSet(S);
    emiBase = 1 + 0.2 * cur.suit.scanner;
    group.userData.suit = Object.assign({}, cur.suit); group.userData.role = cur.role;
  }
  group.userData.jetNozzles = jetNozzles; group.userData.jetBone = bones.jet;
  let emiBase = 1;
  const pet = opts.pet ? createPet(THREE, opts.pet) : null;
  setSuit();
  // rev 18 LOD: `group.lo` is a cheaper skinned twin (parts' lo variants) on the SAME skeleton + materials; the caller shows one or the other
  let loGroup = null;
  Object.defineProperty(group, 'lo', {
    enumerable: false, configurable: true,
    get() {
      if (!loGroup) {
        const S = genGeo('lo'); loSets.push(S); paintSet(S);
        const a = new THREE.SkinnedMesh(S.geoLit, litMat), b2 = new THREE.SkinnedMesh(S.geoEmi, emiMat);
        for (const m of [a, b2]) { m.frustumCulled = false; m.bind(skeleton); }
        const a3 = new THREE.SkinnedMesh(attLit.geometry, litMat), b3 = new THREE.SkinnedMesh(attEmi.geometry, emiMat);
        for (const m of [a3, b3]) { m.frustumCulled = false; m.bind(skeleton, lit.bindMatrix); }
        attSet.lo = [a3, b3];
        loGroup = new THREE.Group(); loGroup.add(a, b2, a3, b3); loGroup.visible = false;
        loGroup.userData.tris = (S.geoLit.attributes.position.count + S.geoEmi.attributes.position.count) / 3;
        loGroup.userData.geoms = [S.geoLit, S.geoEmi];
      }
      return loGroup;
    },
  });

  // ── procedural animation ──
  const B = bones, HIP_Y = 0.52;
  let t = 0, phase = 0, mv = 0, run = 0, air = 0, crouch = 0, wasAir = false, landT = 0, yaw = 0;
  const lerp = (a, b, k) => a + (b - a) * k;
  const damp = (cur, tgt, rate, dt) => cur + (tgt - cur) * (1 - Math.exp(-rate * dt));

  function update(dt, st = {}) {
    dt = Math.min(dt, 0.1); t += dt;
    const sp = Math.max(0, Math.min(1, st.speed == null ? 1 : st.speed));
    mv = damp(mv, st.moving ? Math.max(0.35, sp) : 0, 10, dt);
    run = damp(run, st.running && st.moving ? 1 : 0, 8, dt);
    air = damp(air, st.airborne ? 1 : 0, 14, dt);
    if (wasAir && !st.airborne) landT = 0.28;
    wasAir = !!st.airborne;
    landT = Math.max(0, landT - dt);
    crouch = damp(crouch, landT > 0 ? 1 : 0, landT > 0 ? 30 : 9, dt);
    const freq = lerp(1.8, 2.6, run);
    phase += dt * freq * Math.PI * 2 * (st.moving ? 1 : 0.0);
    if (st.facing != null) group.rotation.y = st.facing;

    const g = 1 - air;                         // ground weight
    const w = mv * g;                          // locomotion weight
    const s = Math.sin(phase), c = Math.cos(phase);
    const amp = lerp(0.55, 0.95, run), arm = lerp(0.5, 1.0, run);
    const br = Math.sin(t * 1.6), look = Math.sin(t * 0.37) * 0.35 + Math.sin(t * 0.91) * 0.12;

    // legs: +x swings forward (-Z)
    let lh = s * amp * w, rh = -s * amp * w;
    let lk = -Math.max(0, -c * 1.0 + 0.2) * 0.9 * w * (0.7 + 0.6 * run), rk = -Math.max(0, c * 1.0 + 0.2) * 0.9 * w * (0.7 + 0.6 * run);
    // jump tuck
    lh = lerp(lh, 0.8, air); rh = lerp(rh, 0.5, air);
    lk = lerp(lk, -1.5, air); rk = lerp(rk, -1.1, air);
    // landing crouch
    lh += 0.75 * crouch; rh += 0.75 * crouch; lk -= 1.3 * crouch; rk -= 1.3 * crouch;
    B.hpL.rotation.set(lh, 0, 0.02); B.hpR.rotation.set(rh, 0, -0.02);
    B.knL.rotation.x = lk; B.knR.rotation.x = rk;

    // hips: bob + crouch lowering
    const bob = Math.abs(Math.cos(phase)) * lerp(0.018, 0.04, run) * w;
    B.hips.position.y = HIP_Y + bob - 0.17 * crouch - 0.05 * air + (1 - mv) * g * br * 0.003;
    B.hips.rotation.y = -s * 0.1 * w;
    // torso: lean, counter-twist, breathing
    const lean = lerp(0.05, 0.38, run) * mv * g + 0.18 * crouch + 0.1 * air;
    B.torso.rotation.set(-lean + (1 - mv) * br * 0.012, s * 0.12 * w, 0);
    B.torso.scale.set(1, 1 + (1 - mv) * br * 0.012, 1 + (1 - mv) * br * 0.02);
    // head: counter the lean, idle look-around
    B.head.rotation.set(lean * 0.8 - 0.05 * air, look * (1 - mv) * g + (st.moving ? 0 : 0), 0);

    // arms: opposite phase to legs; jump = arms up; idle = slight sway
    let alS = -s * arm * w, arS = s * arm * w;
    let alE = 0.25 + (0.15 + 0.9 * run) * w, arE = alE;
    alS = lerp(alS, -2.5, air); arS = lerp(arS, -2.5, air);
    alE = lerp(alE, 0.5, air); arE = lerp(arE, 0.5, air);
    alS += 0.4 * crouch; arS += 0.4 * crouch;
    const sway = br * 0.03 * (1 - mv) * g;
    B.shL.rotation.set(alS + sway, 0, 0.08 + 0.55 * air); B.shR.rotation.set(arS - sway, 0, -0.08 - 0.55 * air);
    B.elL.rotation.x = alE; B.elR.rotation.x = arE;
    B.elL.rotation.z = B.elR.rotation.z = 0;

    // jetpack: backpack vibration + nozzle flame scale; scanner: visor pulse (emissive brightness)
    const jet = Math.max(0, Math.min(1, st.jet || 0)), scan = Math.max(0, Math.min(1, st.scan || 0));
    const vib = jet * (0.5 + 0.5 * Math.sin(t * 61));
    B.pack.rotation.set(jet * 0.018 * Math.sin(t * 47), 0, jet * 0.018 * Math.sin(t * 53 + 1));
    B.pack.position.set(0, 0.10 + jet * 0.003 * Math.sin(t * 71), 0.125 + jet * 0.002 * Math.sin(t * 59));
    const fl = jet > 0.02 ? (0.25 + 1.5 * jet) * (0.88 + 0.12 * Math.sin(t * 43)) : 0.0001;
    B.jet.scale.set(0.5 + 0.6 * jet + vib * 0.1, fl, 0.5 + 0.6 * jet + vib * 0.1);
    emiMat.color.setScalar(emiBase * (1 + scan * 0.55 * (0.5 + 0.5 * Math.sin(t * 9))));
  }

  function dispose() {
    geoLit.dispose(); geoEmi.dispose(); if (loGroup) loGroup.userData.geoms.forEach(g => g.dispose()); attLit.geometry.dispose(); attEmi.geometry.dispose(); if (pet) pet.dispose(); litMat.dispose(); emiMat.dispose(); skeleton.dispose();
    if (group.parent) group.parent.remove(group);
  }
  return { group, update, setColor, setSuit, pet, dispose };
}

// ── companion critter: 3 silhouettes, ~0.4 human height (feet at y=0, forward -Z), <= 600 tris, no skeleton ──
export function createPet(THREE, o = {}) {
  const seed = o.seed != null ? (o.seed | 0) : 1, rng = pmul(seed * 131 + 7), PLANS = ['walker', 'jelly', 'strider'];
  let plan = o.plan; if (typeof plan === 'number') plan = PLANS[((plan % 3) + 3) % 3]; if (!PLANS.includes(plan)) plan = PLANS[Math.abs(seed) % 3];
  const hue = rng(), body = new THREE.Color().setHSL(hue, 0.5, 0.52), belly = new THREE.Color().setHSL(hue, 0.4, 0.72), dark = new THREE.Color().setHSL(hue, 0.35, 0.26), glowC = new THREE.Color().setHSL((hue + 0.45) % 1, 0.9, 0.65);
  const litMat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, vertexColors: true }), emiMat = new THREE.MeshBasicMaterial({ vertexColors: true });
  const geoms = [], root = new THREE.Group(), bodyG = new THREE.Group();
  root.add(bodyG);
  const paint = (g, c) => {
    g = g.index ? g.toNonIndexed() : g; g.deleteAttribute('normal'); g.deleteAttribute('uv');
    const n = g.attributes.position.count, a = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { const sh = 0.92 + 0.08 * Math.sin(i * 1.7); a[i * 3] = c.r * sh; a[i * 3 + 1] = c.g * sh; a[i * 3 + 2] = c.b * sh; }
    g.setAttribute('color', new THREE.BufferAttribute(a, 3)); return g;
  };
  const sph = (r, w, h, sx, sy, sz, x, y, z, c) => { const g = new THREE.SphereGeometry(r, w, h); g.scale(sx, sy, sz); g.translate(x, y, z); return paint(g, c); };
  const cyl = (r0, r1, h, seg, x, y, z, c) => { const g = new THREE.CylinderGeometry(r0, r1, h, seg, 1); g.translate(x, y, z); return paint(g, c); };
  const mesh = (list, mat, parent) => { const g = mergeGeometries(list); geoms.push(g); const m = new THREE.Mesh(g, mat); m.frustumCulled = false; parent.add(m); return m; };
  const legs = [], hang = [];   // pivots animated in update
  const pivot = (x, y, z, geo) => { const p = new THREE.Group(); p.position.set(x, y, z); const g = geo; geoms.push(g); const m = new THREE.Mesh(g, litMat); m.frustumCulled = false; p.add(m); bodyG.add(p); return p; };
  const lit = [], emi = [];
  let lift = 0, bobAmp = 0.008;
  if (plan === 'walker') {        // squat 4-legged critter, tail, ear nubs
    lit.push(sph(0.5, 7, 5, 0.13, 0.085, 0.17, 0, 0.19, 0.02, body), sph(0.5, 6, 4, 0.1, 0.05, 0.12, 0, 0.17, 0.02, belly));
    lit.push(sph(0.5, 6, 4, 0.15, 0.13, 0.13, 0, 0.275, -0.15, body));
    lit.push(cyl(0.012, 0.03, 0.06, 4, -0.045, 0.37, -0.15, dark), cyl(0.012, 0.03, 0.06, 4, 0.045, 0.37, -0.15, dark));
    lit.push(cyl(0.004, 0.025, 0.12, 4, 0, 0.23, 0.2, dark));
    emi.push(sph(0.5, 4, 3, 0.04, 0.04, 0.02, -0.04, 0.285, -0.215, new THREE.Color(1, 1, 1)), sph(0.5, 4, 3, 0.04, 0.04, 0.02, 0.04, 0.285, -0.215, new THREE.Color(1, 1, 1)), sph(0.5, 4, 3, 0.05, 0.03, 0.05, 0, 0.255, 0.22, glowC));
    [[-1, -1], [1, -1], [-1, 1], [1, 1]].forEach(([sx, sz], i) => legs.push(pivot(sx * 0.075, 0.14, sz * 0.095, paint(new THREE.CylinderGeometry(0.02, 0.016, 0.14, 4).translate(0, -0.07, 0), dark))));
  } else if (plan === 'jelly') {  // floating bell + hanging tentacles + glow core
    lift = 0.08; bobAmp = 0.03;
    { const g = new THREE.SphereGeometry(0.5, 8, 4, 0, Math.PI * 2, 0, Math.PI / 2); g.scale(0.3, 0.26, 0.3); g.translate(0, 0.17, 0); lit.push(paint(g, body)); }
    lit.push(sph(0.5, 8, 2, 0.3, 0.02, 0.3, 0, 0.17, 0, belly));
    emi.push(sph(0.5, 5, 4, 0.12, 0.12, 0.12, 0, 0.23, 0, glowC), sph(0.5, 4, 3, 0.04, 0.04, 0.02, -0.06, 0.2, -0.14, new THREE.Color(1, 1, 1)), sph(0.5, 4, 3, 0.04, 0.04, 0.02, 0.06, 0.2, -0.14, new THREE.Color(1, 1, 1)));
    for (let i = 0; i < 5; i++) { const a = i / 5 * 6.283; hang.push(pivot(Math.cos(a) * 0.1, 0.17, Math.sin(a) * 0.1, paint(new THREE.CylinderGeometry(0.012, 0.004, 0.22, 3).translate(0, -0.11, 0), i % 2 ? belly : dark))); }
  } else {                        // strider: tall thin legs, small pod body, neck + eye
    lit.push(sph(0.5, 6, 4, 0.14, 0.09, 0.18, 0, 0.27, 0.03, body), sph(0.5, 5, 4, 0.1, 0.05, 0.12, 0, 0.245, 0.03, belly));
    lit.push(cyl(0.012, 0.02, 0.1, 4, 0, 0.35, -0.1, dark), sph(0.5, 6, 4, 0.1, 0.085, 0.1, 0, 0.375, -0.14, body));
    emi.push(sph(0.5, 5, 4, 0.055, 0.055, 0.03, 0, 0.38, -0.19, new THREE.Color(1, 1, 1)), sph(0.5, 4, 3, 0.05, 0.03, 0.05, 0, 0.3, 0.19, glowC));
    [[-1, -1], [1, -1], [-1, 1], [1, 1]].forEach(([sx, sz]) => legs.push(pivot(sx * 0.08, 0.26, sz * 0.09, paint(new THREE.CylinderGeometry(0.012, 0.007, 0.26, 3).translate(0, -0.13, 0), dark))));
  }
  mesh(lit, litMat, bodyG); const emiMesh = mesh(emi, emiMat, bodyG);
  let tris = 0; geoms.forEach(g => { tris += g.attributes.position.count / 3; });
  root.userData.tris = tris; root.userData.plan = plan; root.userData.height = 0.4;
  let t = rng() * 20, mood = 0.5, spd = 0, hop = 0, yawT = 0, followed = false;
  const lerp = (a, b, k) => a + (b - a) * k;
  function setMood(m) { mood = Math.max(0, Math.min(1, +m || 0)); emiMat.color.setScalar(0.35 + 1.25 * mood); }
  setMood(0.5);
  function anim(dt) {
    dt = Math.min(dt, 0.1); t += dt;
    const tm = 1 + mood * 0.6, mv = Math.min(1, spd / 0.6);
    hop += dt * (8 + 4 * mv) * (mv > 0.05 ? 1 : 0);
    const air = Math.abs(Math.sin(hop)) * mv * (plan === 'jelly' ? 0.03 : 0.055);
    bodyG.position.y = lift + Math.sin(t * 2.1 * tm) * bobAmp * (1 - mv * 0.6) + air;
    bodyG.rotation.z = Math.sin(t * 1.3 * tm) * 0.03; bodyG.rotation.x = -0.12 * mv;
    legs.forEach((l, i) => { l.rotation.x = Math.sin(hop * 1 + (i % 2 ? Math.PI : 0) + (i > 1 ? 0.8 : 0)) * 0.7 * mv; });
    hang.forEach((l, i) => { l.rotation.x = Math.sin(t * 2.6 * tm + i * 1.3) * (0.18 + 0.2 * mv); l.rotation.z = Math.cos(t * 2.2 * tm + i * 0.9) * 0.18; });
    root.rotation.y += (yawT - root.rotation.y) * Math.min(1, dt * 8);
  }
  function update(dt) { if (followed) { followed = false; return; } spd = lerp(spd, 0, Math.min(1, dt * 6)); anim(dt); }
  // follow(target Vector3 | Object3D in the pet's parent space, dt, {dist, rate}): lerp toward a spot `dist` short of the target, face it, bounce while moving. Also animates (update() then skips that frame).
  function follow(target, dt, f = {}) {
    const tp = target && target.isObject3D ? target.position : target; if (!tp) return;
    const dist = f.dist != null ? f.dist : 0.28, rate = f.rate != null ? f.rate : 3.5;
    const dx = tp.x - root.position.x, dz = tp.z - root.position.z, d = Math.hypot(dx, dz) || 1e-6;
    const k = 1 - Math.exp(-rate * Math.min(dt, 0.1)), want = Math.max(0, d - dist), mx = dx / d * want * k, mz = dz / d * want * k;
    root.position.x += mx; root.position.z += mz; root.position.y = lerp(root.position.y, tp.y, k);
    spd = lerp(spd, Math.hypot(mx, mz) / Math.max(dt, 1e-4), Math.min(1, dt * 10));
    if (d > 0.02) yawT = Math.atan2(-dx, -dz);
    followed = true; anim(dt);
  }
  function dispose() { geoms.forEach(g => g.dispose()); litMat.dispose(); emiMat.dispose(); if (root.parent) root.parent.remove(root); }
  return { group: root, update, follow, setMood, dispose };
}

// ── dev lineup: only runs when this module is imported on a page loaded with ?suit (and not ?suit2) ──
try {
  if (typeof location !== 'undefined' && /[?&]suit(&|$)/.test(location.search) && typeof document !== 'undefined') {
    import('three').then(THREE => {
      const W = innerWidth, H = innerHeight, cv = document.createElement('canvas');
      cv.style.cssText = 'position:fixed;inset:0;width:100%;height:100%;z-index:99999;background:#0b0710';
      document.body.appendChild(cv);
      const r = new THREE.WebGLRenderer({ canvas: cv, antialias: true }); r.setSize(W, H); r.setClearColor(0x0b0710);
      const sc = new THREE.Scene(), cam = new THREE.PerspectiveCamera(40, W / H, 0.1, 50);
      cam.position.set(0, 2.3, -5.4); cam.lookAt(0, 0.5, -0.5);
      const all3 = { jetpack: 3, scanner: 3, sprint: 3, storage: 3 }, items = [];
      const far = [['base', {}], ['all3', { suit: all3, role: 'pilot' }], ['pilot', { role: 'pilot' }], ['miner', { role: 'miner' }], ['chef', { role: 'chef' }], ['scout', { role: 'scout' }]];
      far.forEach(([n, o], i) => { const h = createHuman(THREE, Object.assign({ color: 0xB86CFF }, o)); h.group.position.set(1.9 - i * 0.76, 0, 0.6); sc.add(h.group); items.push({ n, h }); });
      [['base back', {}], ['all3 back', { suit: all3 }]].forEach(([n, o], i) => { const h = createHuman(THREE, Object.assign({ color: 0xB86CFF }, o)); h.group.position.set(1.52 - i * 0.76, 0, -1.6); h.group.rotation.y = Math.PI * 0.8; sc.add(h.group); items.push({ n, h, back: true }); });
      const pets = [0, 1, 2].map(i => { const p = createPet(THREE, { seed: 11 + i * 7, plan: i }); p.group.position.set(-0.4 - i * 0.8, 0, -1.6); p.setMood(0.3 + 0.35 * i); sc.add(p.group); return p; });
      let last = performance.now();
      const loop = now => {
        const dt = (now - last) / 1000; last = now;
        items.forEach(({ n, h }) => h.update(dt, n.startsWith('all3') ? { jet: 0.8, scan: 0.7 } : {}));
        pets.forEach(p => p.update(dt)); r.render(sc, cam); requestAnimationFrame(loop);
      };
      requestAnimationFrame(loop);
      const tri = h => { let n = 0; h.group.traverse(o => { if (o.isSkinnedMesh) n += o.geometry.attributes.position.count / 3; }); return Math.round(n); };
      window.__suitDemo = { ready: true, humanTris: tri(items[0].h), all3Tris: tri(items[1].h), petTris: pets.map(p => Math.round(p.group.userData.tris)), nozzles: items[1].h.group.userData.jetNozzles.length };
    }).catch(e => console.error('suit lineup', e));
  }
} catch (e) { }
