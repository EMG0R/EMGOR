// On-foot traveller (NMS vibe). Height = 1.0 unit, forward = -Z (same as the ship nose), up +Y, feet at y=0.
// SCALE: the hull is 1 unit long and a human should be ~0.09 of that, so the caller does group.scale.setScalar(0.09 * shipScale).
// One SkinnedMesh (lit, flat-shaded ShaderMaterial, vertex colors) + one emissive SkinnedMesh (visor, chest strip, jet glow) sharing a skeleton = 2 draw calls, ~700 tris.
// API: createHuman(THREE, {color, suit:{jetpack,scanner,sprint,storage 0..3}, role:'pilot'|'miner'|'chef'|'scout'|null, pet:{seed,plan}|null})
//   -> { group, update(dt, {moving,running,airborne,speed 0..1,facing rad, jet 0..1, scan 0..1}), setColor(hex), setSuit({suit,role}), pet, dispose }
//   group.userData.jetNozzles = Vector3[] (human-local nozzle exits, for the flame fx) ; group.userData.jetBone = the bone those are relative to (world = jetBone's parent chain, use group-local)
//   Q10 rig: setRig(recipe|null) (recipe per ship-styles.js normRig: headShape headScale torso limbLen limbShape feet tail quills cap eyeStyle palette gait) re-bones the human; also via setSuit({rig}). group.userData.rig = normalised rig (gait, hitbox, top).
//   createPet(THREE, {seed, plan:'walker'|'jelly'|'strider'|0..2}) -> { group, update(dt), follow(target, dt), setMood(0..1), dispose }  (0.4 human height, <=600 tris)
// Suit attachments live in their own skinned mesh pair on the same skeleton, so setSuit rebuilds only those.
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { PARTS, mulberry as pmul } from './ship-parts.js';
import { normRig } from './ship-styles.js';
import { VILLAGER_GEO, VILLAGER_TRI_BUDGET } from './ship-geo-cache.js';

const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV; varying vec3 vN;
#include <common>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  #include <beginnormal_vertex>
  #include <skinbase_vertex>
  #include <skinnormal_vertex>
  vN = normalize(normalMatrix * objectNormal);
  #include <begin_vertex>
  #include <skinning_vertex>
  vec4 mv = modelViewMatrix * vec4(transformed, 1.0);
  vV = mv.xyz;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
// soft 3-band toon ramp (shadow / mid / light, blurred band edges), smooth normals, accent-coloured rim, no specular
const FRAG = /* glsl */`
uniform vec3 uRim; uniform vec3 uShade; uniform vec3 uLight;
varying vec3 vC; varying vec3 vV; varying vec3 vN;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = dot(vN, vN) > 0.01 ? normalize(vN) : normalize(cross(dFdx(vV), dFdy(vV)));      // pets ship no normals: flat fallback
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = dot(n, L) * 0.5 + 0.5;
  float b1 = smoothstep(0.26, 0.5, d), b2 = smoothstep(0.58, 0.82, d);
  vec3 ramp = mix(mix(uShade, vec3(1.0), b1), uLight, b2);
  float fr = 1.0 - max(0.0, dot(n, normalize(-vV)));
  float rim = smoothstep(0.55, 1.0, fr) * (0.25 + 0.75 * b1);
  gl_FragColor = vec4(vC * ramp + rim * uRim * 0.5, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

function mkToon(THREE) {
  return new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, vertexColors: true, uniforms: {
    uRim: { value: new THREE.Color(0xB89CFF) }, uShade: { value: new THREE.Color(0.50, 0.44, 0.70) }, uLight: { value: new THREE.Color(1.1, 1.08, 1.12) } } });
}

// bone name -> world-space joint position at bind pose, parent. Round rig: head joint sits at the head centre (so headScale is exact about it), hands / feet / face / wings / orbs have their own bones
// (hd* / ft* are counter-scaled so mitts and boots stay round when arms / legs are stretched).
const JOINTS = {
  hips: [0, 0.52, 0, null], torso: [0, 0.56, 0, 'hips'], head: [0, 0.93, 0, 'torso'], fc: [0, 0.945, -0.12, 'head'],
  shL: [-0.185, 0.745, 0, 'torso'], elL: [-0.195, 0.585, 0, 'shL'], hdL: [-0.195, 0.43, 0, 'elL'],
  shR: [0.185, 0.745, 0, 'torso'], elR: [0.195, 0.585, 0, 'shR'], hdR: [0.195, 0.43, 0, 'elR'],
  hpL: [-0.075, 0.50, 0, 'hips'], knL: [-0.075, 0.27, 0, 'hpL'], ftL: [-0.075, 0.08, 0, 'knL'],
  hpR: [0.075, 0.50, 0, 'hips'], knR: [0.075, 0.27, 0, 'hpR'], ftR: [0.075, 0.08, 0, 'knR'],
  pack: [0, 0.66, 0.125, 'torso'], jet: [0, 0.44, 0.205, 'pack'],
  wgL: [-0.06, 0.74, 0.17, 'torso'], wgR: [0.06, 0.74, 0.17, 'torso'], orb: [0, 0.7, 0, 'hips'],
};
const BONES = Object.keys(JOINTS);

export function createHuman(THREE, opts = {}) {
  const VIL = opts.lod === 'villager';      // Q52 villager LOD: <= VILLAGER_TRI_BUDGET tris, no pack / belly / ears / blinking face, hats only; geometry shared per colour+role via VILLAGER_GEO
  const theme = new THREE.Color(opts.color != null ? opts.color : 0xB86CFF);
  const DARK = 0x1B1620, GREY = 0x8A8691, WHITE = 0xFFFFFF;
  const bi = n => BONES.indexOf(n);
  const MARK = 0xFF00FF;      // part colour that marks theme-tinted verts (g ~ 0 survives the parts' brightness jitter); setColor repaints them
  const PAL = { base: 0x3A3446, panel: DARK, accent: MARK, glow: 0xF4F8FF, dark: DARK, metal: GREY };
  // palette channels: 'T' theme (torso, arms), 'B' base (head, mitts), 'A' accent (boots, belly, trim), 'D' deep (legs, pelvis). Parts tagged with a channel repaint on setColor / setRig.
  const CH = { T: 1, B: 2, A: 3, D: 4 }, cols = [null, new THREE.Color(), new THREE.Color(), new THREE.Color(), new THREE.Color()];
  let palette = null, rimMat = null, rimHex = null;      // rimHex: planet accent (setAccent) tints the rim light
  // palette: muted violet-grey suit (theme pulled toward a grey-violet), pale lilac base, deep shade, ONE soft accent
  function derive() {
    const p = palette || {};
    cols[1].copy(theme).lerp(_W.set(0x8E87A3), 0.5);
    if (p.base != null) cols[2].set(p.base); else cols[2].copy(theme).lerp(_W.set(0xE8E2F2), 0.78);
    if (p.accent != null) cols[3].set(p.accent); else cols[3].set(0xE0A45C);
    if (p.deep != null) cols[4].set(p.deep); else cols[4].copy(theme).lerp(_W.set(0x2A2438), 0.72);
    if (rimMat) { if (rimHex != null) rimMat.uniforms.uRim.value.set(rimHex); else rimMat.uniforms.uRim.value.copy(cols[3]).lerp(_W.set(0xFFFFFF), 0.25); }
  }
  const _W = new THREE.Color();
  derive();

  // skinned geometry for one LOD: hi = full detail, lo = fewer segments (same bones, same layout)
  function mkBuilder(LO, VL) {
    const parts = [], emiParts = [], glassParts = [], rng = pmul(77);
    const SW = LO ? (VL ? 8 : 6) : 12, SH = LO ? (VL ? 5 : 4) : 8;      // hi / far LOD / villager LOD sphere segments; smooth normals hide the rest
    function put(geo, hex, bone, o = {}) {
      geo = geo.index ? geo.toNonIndexed() : geo;
      geo.deleteAttribute('uv');
      const n = geo.attributes.position.count, code = typeof hex === 'string' ? CH[hex] : 0, c = code ? cols[code] : new THREE.Color(hex);
      const col = new Float32Array(n * 3), si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
      for (let i = 0; i < n; i++) {
        col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
        si[i * 4] = bi(bone); sw[i * 4] = 1;
        if (o.blend) {      // smooth two-bone skin across a joint: blend = [boneBelow, jointY, halfWidth]; the vertex's own bone is the upper one
          const y = geo.attributes.position.getY(i), u = Math.max(0, Math.min(1, (o.blend[1] - y) / (2 * o.blend[2]) + 0.5)), w = u * u * (3 - 2 * u);
          si[i * 4 + 1] = bi(o.blend[0]); sw[i * 4] = 1 - w; sw[i * 4 + 1] = w;
        }
      }
      geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
      geo.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(si, 4));
      geo.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4));
      geo.userData = { code: code || (o.panel ? 1 : 0), kind: o.emis ? 'e' : 'l', tint: !!o.tint };
      (o.glass ? glassParts : o.emis ? emiParts : parts).push(geo);
    }
    // a ship-parts part, rigidly bound to one bone. pos/rot place it in the human's bind pose; keeps the part's own colours.
    function putPart(name, params, pos, rot, bone, o = {}) {
      const res = PARTS[name](THREE, Object.assign({ lod: 'lo', pal: PAL }, params), rng);
      const m = new THREE.Matrix4().compose(new THREE.Vector3(pos[0], pos[1], pos[2]), new THREE.Quaternion().setFromEuler(new THREE.Euler(rot[0], rot[1], rot[2])), new THREE.Vector3(1, 1, 1));
      for (const k of ['geo', 'emissive']) {
        const g = res[k]; if (!g) continue;
        g.applyMatrix4(m);
        for (const a of ['uv', 'aFx', 'aPiv', 'aLm']) g.deleteAttribute(a);
        const n = g.attributes.position.count, si = new Uint16Array(n * 4), sw = new Float32Array(n * 4);
        for (let i = 0; i < n; i++) { si[i * 4] = bi(bone); sw[i * 4] = 1; }
        g.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(si, 4)); g.setAttribute('skinWeight', new THREE.BufferAttribute(sw, 4));
        // theme mask: marker-coloured verts (g ~ 0, r and b high)
        const C = g.attributes.color, mk = [];
        for (let i = 0; i < n; i++) mk.push(k === 'geo' && C.getY(i) < 0.02 && C.getX(i) > 0.15 && C.getZ(i) > 0.15 ? 1 : 0);
        g.userData = { code: 0, mask: mk, kind: k === 'geo' ? 'l' : 'e', tint: false };
        (k === 'geo' ? parts : emiParts).push(g);
      }
    }
    const rot3 = (g, r) => { if (!r) return g; if (r[2]) g.rotateZ(r[2]); if (r[0]) g.rotateX(r[0]); if (r[1]) g.rotateY(r[1]); return g; };
    // rounded primitives (everything on the rig is built from these: no hard boxes)
    // ell: sphere scaled to an ellipsoid; o.half = upper hemisphere; o.sm = fewer segments; o.rot = [x,y,z] radians
    const ell = (rx, ry, rz, x, y, z, hex, bone, o = {}) => {
      const sg = o.seg || (o.sm ? (LO ? [4, 3] : [6, 4]) : [SW, SH]), w = sg[0], h = sg[1];
      const g = o.half ? new THREE.SphereGeometry(1, w, Math.ceil(h / 2), 0, Math.PI * 2, 0, Math.PI / 2) : new THREE.SphereGeometry(1, w, h);
      g.scale(rx, ry, rz); rot3(g, o.rot); g.translate(x, y, z); put(g, hex, bone, o);
    };
    // cap: pill (stretched ellipsoid, 56 tris) whose end-sphere centres are a and b
    const cap = (a, b, r, hex, bone, o) => {
      const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), len = A.distanceTo(B);
      const g = new THREE.SphereGeometry(1, LO ? (VL ? 7 : 5) : 10, LO ? (VL ? 4 : 3) : 7); g.scale(r * 1.06, len / 2 + r * 0.82, r * 1.06);
      g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), B.clone().sub(A).normalize()));
      g.translate((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2); put(g, hex, bone, o);
    };
    // cone from base centre a to tip b
    const cone = (a, b, r, hex, bone, o) => {
      const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), len = A.distanceTo(B);
      const g = new THREE.ConeGeometry(r, len, LO ? 4 : 6, 1);
      g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), B.clone().sub(A).normalize()));
      g.translate((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2); put(g, hex, bone, o);
    };
    // rbox: box with a spherical bevel of radius r (2 segments per axis = 48 tris)
    const rbox = (w, h, d, r, x, y, z, hex, bone, o = {}) => {
      const g = new THREE.BoxGeometry(w, h, d, 2, 2, 2), P = g.attributes.position, N = g.attributes.normal, hw = w / 2 - r, hh = h / 2 - r, hd = d / 2 - r;
      for (let i = 0; i < P.count; i++) {
        const vx = P.getX(i), vy = P.getY(i), vz = P.getZ(i), cx = Math.max(-hw, Math.min(hw, vx)), cy = Math.max(-hh, Math.min(hh, vy)), cz = Math.max(-hd, Math.min(hd, vz));
        const dx = vx - cx, dy = vy - cy, dz = vz - cz, l = Math.hypot(dx, dy, dz) || 1;
        P.setXYZ(i, cx + dx / l * r, cy + dy / l * r, cz + dz / l * r); N.setXYZ(i, dx / l, dy / l, dz / l);
      }
      rot3(g, o.rot); g.translate(x, y, z); put(g, hex, bone, o);
    };
    // limb: one tapered, round-ended lathe from yTop down to yBot (vertical, bind pose), skinned smoothly across a joint: bone = upper bone, o.blend = [lowerBone, jointY, halfWidth]
    const limb = (x, yTop, yBot, rTop, rBot, hex, bone, o = {}) => {
      const pts = [], N = LO ? 4 : 5, cs = [-90, -42], ct = [42, 90], rt = r => Math.max(r, 0.001);
      for (const a of cs) { const q = a * Math.PI / 180; pts.push(new THREE.Vector2(rt(rBot * Math.cos(q)), yBot + rBot + rBot * Math.sin(q))); }
      for (let i = 0; i <= N; i++) { const t = i / N; pts.push(new THREE.Vector2(rBot + (rTop - rBot) * t, yBot + rBot + (yTop - rTop - yBot - rBot) * t)); }
      for (const a of ct) { const q = a * Math.PI / 180; pts.push(new THREE.Vector2(rt(rTop * Math.cos(q)), yTop - rTop + rTop * Math.sin(q))); }
      const g = new THREE.LatheGeometry(pts, LO ? (VL ? 8 : 6) : 10); g.translate(x, 0, 0); put(g, hex, bone, o);
    };
    const box = (w, h, d, x, y, z, hex, bone, o) => { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); put(g, hex, bone, o); };      // only for the (hidden) jet flames
    function finish() {
      if (!parts.length || !emiParts.length) return null;
      for (const g of parts.concat(emiParts, glassParts)) if (!g.attributes.normal) g.computeVertexNormals();
      const geoLit = mergeGeometries(parts), geoEmi = mergeGeometries(emiParts);
      const panelMask = [], emiTint = [];
      parts.forEach(g => { for (let i = 0; i < g.attributes.position.count; i++) panelMask.push(g.userData.mask ? g.userData.mask[i] : g.userData.code); });
      emiParts.forEach(g => { for (let i = 0; i < g.attributes.position.count; i++) emiTint.push(g.userData.tint ? 1 : 0); });
      return { geoLit, geoEmi, geoGlass: glassParts.length ? mergeGeometries(glassParts) : null, panelMask, emiTint };
    }
    return { put, putPart, box, ell, cap, cone, rbox, limb, finish };
  }
  function genGeo(lod) {
    const V = lod === 'vil', LO = lod === 'lo' || V, { putPart, ell, cap, rbox, limb, finish } = mkBuilder(LO, V);
    // few round shapes: pelvis + egg torso + head, one tapered capsule per limb (overlapping the torso so no gap shows) + round boots
    ell(0.17, 0.105, 0.135, 0, 0.52, 0, 'D', 'hips');
    ell(0.19, 0.19, 0.16, 0, 0.665, 0, 'T', 'torso');
    if (!V) ell(0.095, 0.1, 0.05, 0, 0.645, -0.118, 'A', 'torso', { sm: true });
    ell(0.02, 0.02, 0.012, 0, 0.76, -0.138, WHITE, 'torso', { emis: true, sm: true });
    // backpack: soft slab + twin tanks + engine (jet flames hang off the 'jet' bone)
    if (!V) {
      rbox(0.2, 0.26, 0.08, 0.035, 0, 0.66, 0.185, 'D', 'pack');
      for (const sx of [-1, 1]) cap([sx * 0.065, 0.735, 0.235], [sx * 0.065, 0.585, 0.235], 0.036, 'B', 'pack');
      putPart('engine', { r: 0.034, len: 0.08 }, [0, 0.52, 0.205], [Math.PI, 0, 0], 'pack');
    }
    // head: one big soft ellipsoid + ear nubs
    ell(0.145, 0.14, 0.14, 0, 0.93, 0, 'B', 'head', { seg: LO ? (V ? [10, 7] : [8, 5]) : [16, 11] });
    if (!V) for (const sx of [-1, 1]) ell(0.028, 0.04, 0.03, sx * 0.14, 0.925, 0.0, 'B', 'head', { seg: [6, 4] });
    for (const s of ['L', 'R']) {
      const x = s === 'L' ? -1 : 1;
      limb(x * 0.19, 0.775, 0.415, 0.066, 0.06, 'T', 'sh' + s, { blend: ['el' + s, 0.585, 0.05] });      // arm: shoulder -> elbow -> rounded hand
      limb(x * 0.075, 0.545, 0.1, 0.084, 0.064, 'D', 'hp' + s, { blend: ['kn' + s, 0.27, 0.05] });      // leg: hip -> knee -> ankle
      ell(0.088, 0.07, 0.13, x * 0.075, 0.07, -0.035, 'D', 'ft' + s, { seg: LO ? (V ? [8, 5] : [6, 4]) : [12, 8] });
    }
    return finish();
  }
  const freeSet = S => { S.geoLit.dispose(); S.geoEmi.dispose(); if (S.geoGlass) S.geoGlass.dispose(); };
  let hiKey = VIL ? 'b|' + theme.getHex() : null, attKey = null;
  let hi = VIL ? VILLAGER_GEO.acquire(hiKey, () => genGeo('vil')) : genGeo('hi');
  const { geoLit, geoEmi } = hi;

  // skeleton
  const bones = {}, list = [];
  for (const n of BONES) { const b = new THREE.Bone(); b.name = n; bones[n] = b; list.push(b); }
  for (const n of BONES) {
    const [x, y, z, p] = JOINTS[n];
    if (p) { const pj = JOINTS[p]; bones[n].position.set(x - pj[0], y - pj[1], z - pj[2]); bones[p].add(bones[n]); }
    else bones[n].position.set(x, y, z);
  }
  const skeleton = new THREE.Skeleton(list);
  const litMat = mkToon(THREE); rimMat = litMat; derive();
  const emiMat = new THREE.MeshBasicMaterial({ vertexColors: true });
  const lit = new THREE.SkinnedMesh(geoLit, litMat), emi = new THREE.SkinnedMesh(geoEmi, emiMat);
  for (const m of [lit, emi]) { m.frustumCulled = false; m.add(bones.hips); }
  lit.bind(skeleton); emi.bind(skeleton);
  const group = new THREE.Group();
  group.add(lit, emi);
  group.userData.eyeHeight = 0.93;
  group.userData.headPos = new THREE.Vector3(0, 0.93, 0);

  const loSets = [];     // low-LOD geometry sets (built lazily) that setColor must repaint too
  function paintSet(S) {
    const lc = S.geoLit.attributes.color, ec = S.geoEmi.attributes.color;
    for (let i = 0; i < S.panelMask.length; i++) { const v = S.panelMask[i]; if (v) { const c = cols[v]; lc.setXYZ(i, c.r, c.g, c.b); } }
    for (let i = 0; i < S.emiTint.length; i++) if (S.emiTint[i]) ec.setXYZ(i, theme.r * 0.6 + 0.4, theme.g * 0.6 + 0.4, theme.b * 0.6 + 0.4);
    lc.needsUpdate = ec.needsUpdate = true;
  }
  function repaint() { derive(); glassMat.color.copy(cols[2]); paintSet(hi); loSets.forEach(paintSet); if (attSet.cur) paintSet(attSet.cur); }
  function setColor(hex) { if (VIL && new THREE.Color(hex).getHex() !== theme.getHex()) unshare(); theme.set(hex); repaint(); }
  paintSet(hi);

  // villager sets are shared: a recolour / palette rig first takes private copies so it cannot repaint other villagers
  function unshare() {
    const cl = S => ({ geoLit: S.geoLit.clone(), geoEmi: S.geoEmi.clone(), geoGlass: S.geoGlass ? S.geoGlass.clone() : null, panelMask: S.panelMask, emiTint: S.emiTint });
    if (hiKey) { const o = hi; hi = cl(o); VILLAGER_GEO.release(hiKey, freeSet); hiKey = null; lit.geometry = hi.geoLit; emi.geometry = hi.geoEmi; }
    if (attKey) { const o = attSet.cur; attSet.cur = cl(o); VILLAGER_GEO.release(attKey, freeSet); attKey = null; attLit.geometry = attSet.cur.geoLit; attEmi.geometry = attSet.cur.geoEmi; if (attSet.lo) { attSet.lo[0].geometry = attLit.geometry; attSet.lo[1].geometry = attEmi.geometry; } }
  }
  // ── suit attachments: own skinned mesh pair on the same skeleton; setSuit rebuilds only these ──
  const ROLES = {
    pilot: { c: 0x4DA8FF, g: ['..#..', '..#..', '#####', '..#..', '.###.'] },
    miner: { c: 0xFFB020, g: ['.###.', '#.#.#', '..#..', '..#..', '..#..'] },
    chef:  { c: 0xFF5A4D, g: ['.#.#.', '#####', '#####', '.###.', '.###.'] },
    scout: { c: 0x59E08A, g: ['.###.', '#...#', '#.#.#', '#...#', '.###.'] },
  };
  const clampT = v => Math.max(0, Math.min(3, Math.round(+v || 0)));
  const B = bones, HIP_Y = 0.52;
  const rg = { legLen: 1, tx: 1, ty: 1, tz: 1, hipOff: 0, stride: 1, bounce: 1, arm: 1, lean: 1, stance: 0, headY: 0, wings: 0, orbs: 0 };
  const cur = { suit: { jetpack: 0, scanner: 0, sprint: 0, storage: 0 }, role: null, vill: null, extra: [], rig: normRig(null) };
  // villager variants: same base body, hats / apron / glasses / beard per role. opts.role can be 'farmer'|'trader'|'elder'|'kid'; a bare colour from ship-village's ROLE_COL falls back to the same table.
  const VILL = { farmer: ['strawhat', 'apron'], trader: ['beret', 'glasses', 'satchel'], elder: ['beard', 'glasses', 'scarf'], kid: ['beanie', 'pigtails'] };
  const VILL_COLOR = { 0x8FD06A: 'farmer', 0xFFD36A: 'trader', 0xB08CFF: 'elder', 0xFF8AE0: 'kid' };
  function applySuit(o) {
    if (!o) return;
    const su = o.suit || (('jetpack' in o || 'scanner' in o || 'sprint' in o || 'storage' in o) ? o : null);
    if (su) for (const k of ['jetpack', 'scanner', 'sprint', 'storage']) if (k in su) cur.suit[k] = clampT(su[k]);
    if ('role' in o) { cur.role = ROLES[o.role] ? o.role : null; cur.vill = VILL[o.role] ? o.role : null; }
    if ('rig' in o) cur.rig = normRig(o.rig);
    if ('extra' in o) cur.extra = Array.isArray(o.extra) ? o.extra.filter(n => typeof n === 'string') : [];   // re-skin parts (ship-styles.js skinFor): cap cube snout tail quills visor blocky crest
  }
  applySuit(opts);
  if (opts.role == null && opts.rig == null && VILL_COLOR[opts.color]) cur.vill = VILL_COLOR[opts.color];
  const jetNozzles = [];
  function genAtt() {
    const su = cur.suit, role = cur.role, { put, putPart, box, ell, cap, cone, rbox, finish } = mkBuilder(VIL, VIL);
    const J = su.jetpack, S = su.scanner, P = su.sprint, K = su.storage, RG = cur.rig, EX = new Set(VIL ? (cur.vill ? VILL[cur.vill] : []) : cur.extra.concat(RG.extras, cur.vill ? VILL[cur.vill] : []));
    const PI = Math.PI, EYE = DARK;
    jetNozzles.length = 0;
    ell(0.002, 0.002, 0.002, 0, 0.5, 0, DARK, 'hips');      // keeps the lit set non-empty at tier 0
    // jetpack: bigger outer tanks + extra nozzles; every nozzle gets a flame box on the 'jet' bone (scaled by state.jet)
    const noz = [[0, 0.52, 0.205, 0.034]];
    if (J >= 1) for (const sx of [-1, 1]) noz.push([sx * 0.085, 0.52, 0.205, 0.026]);
    if (J >= 2) for (const sx of [-1, 1]) noz.push([sx * 0.125, 0.52, 0.22, 0.021]);
    if (J >= 3) for (const sx of [-1, 1]) noz.push([sx * 0.045, 0.52, 0.27, 0.021]);
    for (const [x, y, z, r] of noz) {
      if (x !== 0 || r !== 0.034) putPart('engine', { r, len: 0.08 }, [x, y, z], [PI, 0, 0], 'pack');
      box(r * 1.5, 0.07, r * 1.5, x, 0.405, z, 0xFFA040, 'jet', { emis: true });
      jetNozzles.push(new THREE.Vector3(x, 0.44, z));
    }
    if (J >= 1) for (const sx of [-1, 1]) cap([sx * (0.105 + 0.006 * J), 0.54 + 0.07 + 0.015 * J, 0.255], [sx * (0.105 + 0.006 * J), 0.54 - 0.04 - 0.015 * J, 0.255], 0.026 + 0.005 * J, 'B', 'pack');
    if (J >= 2) rbox(0.24, 0.04, 0.05, 0.018, 0, 0.77, 0.235, J >= 3 ? WHITE : 'A', 'pack', { emis: J >= 3 });
    // face: big round eyes (helmet), beady dots, or a blank mask; 'visor' is its own extra. The eyes ride the 'fc' bone so they can blink.
    const ES = VIL ? 'vdots' : RG.eyeStyle, cubeH = !VIL && RG.headShape === 'cube';      // villager eyes ride the head bone (no blink)
    if (ES === 'vdots') {
      for (const sx of [-1, 1]) ell(0.018, 0.028, 0.014, sx * 0.05, 0.955, -0.124, EYE, 'head', { sm: true });
    } else if (ES === 'helmet') {
      for (const sx of [-1, 1]) {
        if (cubeH) { rbox(0.075, 0.08, 0.02, 0.01, sx * 0.07, 0.945, -0.147, WHITE, 'fc', { emis: true }); rbox(0.04, 0.05, 0.012, 0.006, sx * 0.062, 0.943, -0.158, EYE, 'fc'); }
        else {
          ell(0.04, 0.05, 0.022, sx * 0.054, 0.945, -0.118, WHITE, 'fc', { emis: true, sm: true }); ell(0.022, 0.032, 0.014, sx * 0.05, 0.943, -0.134, EYE, 'fc', { seg: [5, 3] });
          ell(0.026, 0.016, 0.012, sx * 0.095, 0.905, -0.11, 'A', 'head', { seg: [5, 3] });
        }
      }
    } else if (ES === 'dots') {
      for (const sx of [-1, 1]) ell(0.016, 0.026, 0.014, sx * 0.05, 0.955, -0.124, EYE, 'fc', { sm: true });
    } else if (ES === 'none') {
      ell(0.1, 0.085, 0.04, 0, 0.935, -0.1, 'D', 'head', { sm: true });
    }
    // scanner: side pods + glow antenna
    if (S >= 1) {
      for (const sx of [-1, 1]) ell(0.02 + 0.004 * S, 0.045 + 0.008 * S, 0.045 + 0.008 * S, sx * 0.152, 0.93, -0.02, WHITE, 'head', { emis: true, tint: true, sm: true });
      cap([-0.07, 1.06, 0.02], [-0.09, 1.1 + 0.03 * S, 0.03], 0.007, 'D', 'head', { sm: true }); ell(0.02, 0.02, 0.02, -0.09, 1.12 + 0.03 * S, 0.03, WHITE, 'head', { emis: true, tint: true, sm: true });
    }
    // sprint: leg trim (knee pads, shin rings, ankle glow)
    for (const sx of [-1, 1]) {
      const kn = sx < 0 ? 'knL' : 'knR', x = sx * 0.075;
      if (P >= 1) ell(0.052, 0.05, 0.04, x, 0.3, -0.05, 'A', kn, { sm: true });
      if (P >= 2) ell(0.067, 0.022, 0.067, x, 0.19, 0, 'T', kn, { sm: true });
      if (P >= 3) ell(0.072, 0.014, 0.072, x, 0.125, 0, 0xFFA040, kn, { emis: true, sm: true });
    }
    // storage: side pouches on the belt
    const pz = [[0.03], [0.03, 0.105], [0.03, 0.105, -0.045]][Math.max(0, K - 1)];
    if (K >= 1) for (const sx of [-1, 1]) pz.forEach((z, i) => { const big = K >= 3 && i === 0; rbox(0.05, big ? 0.09 : 0.07, big ? 0.085 : 0.07, 0.02, sx * 0.175, 0.5, z, 'A', 'hips'); });
    // role: chest emblem glyph (pixel quads on the belly patch) + belt strap and a small round tool
    if (role) {
      const R = ROLES[role], cell = 0.0145;
      R.g.forEach((row, ry) => { for (let rx = 0; rx < 5; rx++) if (row[rx] === '#') {
        const g = new THREE.PlaneGeometry(cell * 0.92, cell * 0.92); g.rotateY(Math.PI); g.translate((rx - 2) * cell, 0.645 + (2 - ry) * cell, -0.1695); put(g, R.c, 'torso', { emis: true });
      } });
      const hy = 0.52, hz = -0.13;
      cap([-0.11, hy, hz], [0.07, hy, hz], 0.008, GREY, 'hips');
      if (role === 'pilot') rbox(0.05, 0.04, 0.02, 0.01, -0.125, hy, hz - 0.01, GREY, 'hips');
      else if (role === 'miner') { cap([-0.12, hy - 0.04, hz - 0.01], [-0.12, hy + 0.07, hz - 0.01], 0.011, 0x7A5A3A, 'hips'); ell(0.025, 0.014, 0.012, -0.12, hy + 0.08, hz - 0.01, GREY, 'hips', { sm: true }); }
      else if (role === 'chef') ell(0.03, 0.016, 0.03, -0.125, hy, hz - 0.01, GREY, 'hips', { sm: true });
      else { ell(0.02, 0.02, 0.02, -0.105, hy, hz - 0.01, 0xBFFFE0, 'hips', { emis: true, sm: true }); }
    }

    // ── per-style silhouettes (ship-styles.js rigs): each is its own shape built from capsules / ellipsoids ──
    if (cubeH) { rbox(0.3, 0.27, 0.29, 0.07, 0, 0.93, 0, 'B', 'head'); rbox(0.31, 0.04, 0.3, 0.015, 0, 1.03, 0, 'A', 'head'); }                      // STEEV / HAULER: chamfered head block (+ hair slab)
    if (EX.has('snout')) { ell(0.065, 0.048, 0.075, 0, 0.9, -0.14, 'B', 'head'); ell(0.012, 0.01, 0.01, -0.02, 0.915, -0.208, EYE, 'head', { sm: true }); ell(0.012, 0.01, 0.01, 0.02, 0.915, -0.208, EYE, 'head', { sm: true }); }
    if (EX.has('nose')) ell(0.052, 0.048, 0.05, 0, 0.91, -0.15, 'B', 'head');
    if (EX.has('moustache')) { cap([-0.045, 0.875, -0.14], [0.045, 0.875, -0.14], 0.014, EYE, 'head'); ell(0.02, 0.016, 0.02, -0.052, 0.872, -0.133, EYE, 'head', { sm: true }); ell(0.02, 0.016, 0.02, 0.052, 0.872, -0.133, EYE, 'head', { sm: true }); }
    if (EX.has('cap')) { ell(0.158, 0.125, 0.155, 0, 0.945, 0.005, 'T', 'head', { half: true }); ell(0.1, 0.014, 0.085, 0, 0.958, -0.135, 'T', 'head', { sm: true }); if (EX.has('ridge')) cap([0, 1.065, -0.06], [0, 1.065, 0.07], 0.012, 'A', 'head'); }
    if (EX.has('tail')) {
      const tl = RG.tail ? RG.tail.len : 1, tz = k => 0.1 + (k - 0.1) * tl;
      cap([0, 0.5, 0.1], [0, 0.46, tz(0.22)], 0.04, 'T', 'hips'); cap([0, 0.46, tz(0.22)], [0, 0.5, tz(0.36)], 0.03, 'T', 'hips'); ell(0.024, 0.024, 0.03, 0, 0.56, tz(0.42), 'A', 'hips', { sm: true });
      const sp = RG.tail ? RG.tail.spikes : 0;
      for (let i = 0; i < sp; i++) ell(0.022, 0.032, 0.026, 0, 0.53 + 0.01 * i, tz(0.14 + 0.2 * i / Math.max(1, sp)), 'A', 'hips', { sm: true });
    }
    if (EX.has('quills')) { const nq = RG.quills || 5; for (let i = 0; i < nq; i++) { const a = (i - (nq - 1) / 2) * (1.28 / Math.max(2, nq - 1)); cone([Math.sin(a) * 0.08, 0.97 + 0.03 * Math.cos(a), 0.07], [Math.sin(a) * 0.22, 0.95 + 0.1 * Math.cos(a) - 0.04 * Math.abs(a), 0.29], 0.04, i % 2 ? 'A' : 'D', 'head'); } }
    if (EX.has('backspikes')) for (let i = 0; i < 4; i++) cone([0, 0.8 - 0.07 * i, 0.12], [0, 0.78 - 0.06 * i, 0.3 + 0.02 * (3 - i)], 0.035, 'A', 'torso');
    if (EX.has('visor')) { ell(0.14, 0.036, 0.06, 0, 0.945, -0.1, 0xFFE9A0, 'head', { emis: true, sm: true }); cone([0, 0.945, -0.14], [0, 0.945, -0.27], 0.028, 0xFFE9A0, 'head', { emis: true, rot: null }); }
    if (EX.has('crest')) for (let i = 0; i < 4; i++) ell(0.024, 0.04 - 0.005 * i, 0.034, 0, 1.06 - 0.04 * i, 0.04 + 0.04 * i, 'A', 'head', { sm: true });
    if (EX.has('swept')) { ell(0.1, 0.085, 0.21, 0, 0.97, 0.1, 'A', 'head', { rot: [-0.35, 0, 0] }); }
    if (EX.has('antenna')) { cap([0, 1.06, 0], [0.02, 1.24, 0.01], 0.008, 'D', 'head', { sm: true }); ell(0.03, 0.03, 0.03, 0.02, 1.26, 0.01, 0xFFE36B, 'head', { emis: true, sm: true }); }
    if (EX.has('ears')) for (const sx of [-1, 1]) { cap([sx * 0.07, 1.04, 0.0], [sx * 0.13, 1.3, 0.03], 0.032, 'B', 'head'); cap([sx * 0.075, 1.07, -0.012], [sx * 0.125, 1.26, 0.015], 0.016, 'A', 'head', { sm: true }); }
    if (EX.has('bigfeet')) for (const sx of [-1, 1]) ell(0.095, 0.075, 0.15, sx * 0.075, 0.075, -0.07, 'A', sx < 0 ? 'ftL' : 'ftR');
    if (EX.has('longshoes')) for (const sx of [-1, 1]) ell(0.075, 0.055, 0.21, sx * 0.075, 0.056, -0.11, 'A', sx < 0 ? 'ftL' : 'ftR');
    if (EX.has('blocky')) for (const sx of [-1, 1]) {
      rbox(0.105, 0.2, 0.105, 0.03, sx * 0.195, 0.65, 0, 'T', sx < 0 ? 'shL' : 'shR'); rbox(0.115, 0.2, 0.115, 0.03, sx * 0.075, 0.4, 0, 'D', sx < 0 ? 'hpL' : 'hpR');
    }
    if (EX.has('bigmitts')) for (const sx of [-1, 1]) ell(0.1, 0.1, 0.1, sx * 0.195, 0.4, 0, 'A', sx < 0 ? 'hdL' : 'hdR');
    if (EX.has('dome')) {          // JELLY: soft bell over the head + hanging tentacles
      ell(0.27, 0.24, 0.27, 0, 0.88, 0, WHITE, 'head', { half: true, rot: null, glass: true });      // Q54: translucent bell, own sorted-last mesh (attGlass)
      for (let i = 0; i < 7; i++) { const a = i / 7 * 2 * PI; cap([Math.cos(a) * 0.23, 0.88, Math.sin(a) * 0.23], [Math.cos(a) * 0.24, 0.62, Math.sin(a) * 0.24], 0.022, i % 2 ? 'A' : 'B', 'torso', { sm: true }); }
    }
    if (EX.has('wings')) for (const sx of [-1, 1]) {
      const wb = sx < 0 ? 'wgL' : 'wgR';
      ell(0.14, 0.02, 0.07, sx * 0.2, 0.78, 0.17, 'A', wb, { rot: [0, sx * 0.3, sx * -0.45] }); ell(0.09, 0.016, 0.05, sx * 0.17, 0.7, 0.17, 'B', wb, { rot: [0, sx * 0.3, sx * 0.15] });
    }
    if (EX.has('robe')) {          // SEER: cone robe over the legs + gold hem band
      const y0 = 0.05 - RG.hipOff; const g = new THREE.CylinderGeometry(0.18, 0.32, 0.6 - y0 + 0.02, 10, 1, true); g.translate(0, (y0 + 0.6) / 2, 0); put(g, 'T', 'hips');
      const g2 = new THREE.CylinderGeometry(0.321, 0.331, 0.035, 10, 1, true); g2.translate(0, y0 + 0.02, 0); put(g2, 'A', 'hips');
      const g3 = new THREE.CircleGeometry(0.2, 10); g3.rotateX(PI / 2); g3.translate(0, y0 + 0.03, 0); put(g3, 'D', 'hips');
    }
    if (EX.has('hood')) { ell(0.175, 0.17, 0.17, 0, 0.945, 0.03, 'T', 'head', { half: false }); cone([0, 1.04, 0.1], [0, 1.1, 0.27], 0.07, 'T', 'head'); }
    if (EX.has('orbs')) for (let i = 0; i < 3; i++) { const a = i / 3 * 2 * PI; ell(0.042, 0.042, 0.042, Math.cos(a) * 0.36, 0.5 + i * 0.22, Math.sin(a) * 0.36, WHITE, 'orb', { emis: true, tint: true, sm: true }); ell(0.075, 0.075, 0.075, Math.cos(a) * 0.36, 0.5 + i * 0.22, Math.sin(a) * 0.36, WHITE, 'orb', { sm: true, glass: true }); }      // Q54: bright core + glass halo
    // villagers
    if (EX.has('strawhat')) { ell(0.26, 0.018, 0.26, 0, 1.0, 0, 0xE8C872, 'head', { sm: true }); ell(0.14, 0.09, 0.14, 0, 1.0, 0, 0xE8C872, 'head', { half: true }); ell(0.145, 0.02, 0.145, 0, 1.03, 0, 'A', 'head', { sm: true }); }
    if (EX.has('beret')) ell(0.19, 0.06, 0.17, 0.02, 1.05, 0, 'A', 'head', { rot: [0, 0, -0.2] });
    if (EX.has('beanie')) { ell(0.155, 0.12, 0.15, 0, 0.97, 0, 'A', 'head', { half: true }); ell(0.04, 0.04, 0.04, 0, 1.1, 0, WHITE, 'head', { sm: true }); }
    if (EX.has('pigtails')) for (const sx of [-1, 1]) { cap([sx * 0.15, 0.99, 0.03], [sx * 0.2, 0.86, 0.04], 0.03, 'A', 'head', { sm: true }); ell(0.04, 0.04, 0.04, sx * 0.205, 0.83, 0.04, 'D', 'head', { sm: true }); }
    if (EX.has('glasses')) for (const sx of [-1, 1]) { const g = new THREE.TorusGeometry(0.044, 0.007, VIL ? 3 : 4, VIL ? 8 : 10); g.translate(sx * 0.054, 0.945, -0.13); put(g, EYE, 'fc'); }
    if (EX.has('beard')) { ell(0.1, 0.11, 0.07, 0, 0.85, -0.085, 0xEEEEF2, 'head'); ell(0.07, 0.025, 0.04, 0, 0.9, -0.14, 0xEEEEF2, 'head', { sm: true }); }
    if (EX.has('scarf')) { const g = new THREE.TorusGeometry(0.115, 0.034, VIL ? 4 : 5, VIL ? 8 : 12); g.rotateX(PI / 2); g.translate(0, 0.8, 0); put(g, 'A', 'torso'); cap([0.06, 0.8, -0.1], [0.08, 0.64, -0.14], 0.03, 'A', 'torso', { sm: true }); }
    if (EX.has('apron')) { rbox(0.22, 0.2, 0.03, 0.012, 0, 0.59, -0.15, 0xF2E6D0, 'torso'); cap([-0.07, 0.69, -0.135], [-0.12, 0.8, -0.04], 0.008, 0xF2E6D0, 'torso', { sm: true }); cap([0.07, 0.69, -0.135], [0.12, 0.8, -0.04], 0.008, 0xF2E6D0, 'torso', { sm: true }); }
    if (EX.has('satchel')) { rbox(0.09, 0.08, 0.05, 0.02, 0.19, 0.5, -0.02, 0x8A5A3A, 'hips'); cap([-0.13, 0.78, -0.02], [0.17, 0.52, -0.02], 0.007, 0x8A5A3A, 'torso', { sm: true }); }
    return finish();
  }
  const EMPTY_GEO = new THREE.BufferGeometry();
  const attLit = new THREE.SkinnedMesh(new THREE.BufferGeometry(), litMat), attEmi = new THREE.SkinnedMesh(new THREE.BufferGeometry(), emiMat);
  for (const m of [attLit, attEmi]) { m.frustumCulled = false; m.bind(skeleton, lit.bindMatrix); }
  // Q54 translucent layer (JELLY dome): alpha 0.6, no depth write, drawn after everything opaque; tinted from the theme's B colour in repaint()
  const glassMat = new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.6, depthWrite: false, side: THREE.DoubleSide });
  const attGlass = new THREE.SkinnedMesh(new THREE.BufferGeometry(), glassMat);
  attGlass.frustumCulled = false; attGlass.renderOrder = 10; attGlass.visible = false; attGlass.bind(skeleton, lit.bindMatrix);
  group.add(attLit, attEmi, attGlass);
  // ── jet fx: per nozzle an outer (orange) + inner (pale) cone flame hanging off the pack, plus a pooled ring of puffs. Real meshes (not skinned) so the look is identical in every style.
  const JETFX = { flames: [], puffs: [], t: 0, ph: 0 };
  const jetFx = new THREE.Group(); jetFx.name = 'jetFx'; bones.pack.add(jetFx); jetFx.position.set(0, 0.44 - 0.66, 0.205 - 0.125);      // = the 'jet' bone origin; the pack bone vibrates, the flames ride it
  const flameGeo = new THREE.ConeGeometry(1, 1, 7, 1, true); flameGeo.rotateX(Math.PI); flameGeo.translate(0, -0.5, 0);      // apex down, base at y=0, length 1
  const puffGeo = new THREE.IcosahedronGeometry(1, 0);
  const flameMatO = new THREE.MeshBasicMaterial({ color: 0xFF8A2A, transparent: true, opacity: 0.85, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false });
  const flameMatI = new THREE.MeshBasicMaterial({ color: 0xFFF3C0, transparent: true, opacity: 0.95, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false });
  const PUFFS = 14;
  for (let i = 0; i < PUFFS; i++) {
    const m = new THREE.Mesh(puffGeo, new THREE.MeshBasicMaterial({ color: 0xFFB060, transparent: true, opacity: 1, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
    m.visible = false; m.frustumCulled = false; m.renderOrder = 5; group.add(m); JETFX.puffs.push({ m, life: 0, max: 0.4, vx: 0, vy: 0, vz: 0 });
  }
  function rebuildJetFx() {
    for (const f of JETFX.flames) jetFx.remove(f.o), jetFx.remove(f.i);
    JETFX.flames.length = 0;
    for (const n of jetNozzles) {
      const o = new THREE.Mesh(flameGeo, flameMatO), i = new THREE.Mesh(flameGeo, flameMatI);
      for (const m of [o, i]) { m.position.set(n.x, 0, n.z - 0.205); m.frustumCulled = false; m.visible = false; m.renderOrder = 6; jetFx.add(m); }
      JETFX.flames.push({ o, i, r: n.x === 0 ? 0.034 : 0.026 });
    }
  }
  function updateJetFx(dt, jet, tt) {
    const on = jet > 0.02;
    for (let k = 0; k < JETFX.flames.length; k++) {
      const f = JETFX.flames[k];
      f.o.visible = f.i.visible = on; if (!on) continue;
      const fl = (0.13 + 0.42 * jet) * (0.86 + 0.14 * Math.sin(tt * 43 + k * 1.7) + 0.06 * Math.sin(tt * 97 + k)), w = f.r * (1.1 + 0.5 * jet);
      f.o.scale.set(w, fl, w); f.i.scale.set(w * 0.5, fl * 0.62, w * 0.5);
    }
    JETFX.ph += (on ? jet * 38 : 0) * dt;
    let spawn = JETFX.ph | 0; JETFX.ph -= spawn;
    for (const p of JETFX.puffs) {
      if (p.life <= 0 && spawn > 0 && on && jetNozzles.length) {
        spawn--; const n = jetNozzles[(Math.random() * jetNozzles.length) | 0], fl = 0.13 + 0.42 * jet;
        p.m.position.set(n.x + (Math.random() - 0.5) * 0.03, n.y - fl * 0.85, n.z + (Math.random() - 0.5) * 0.03);
        p.vx = (Math.random() - 0.5) * 0.25; p.vz = (Math.random() - 0.5) * 0.25; p.vy = -(0.5 + 0.9 * jet) - Math.random() * 0.3;
        p.max = p.life = 0.28 + Math.random() * 0.22; p.m.visible = true;
      }
      if (p.life > 0) {
        p.life -= dt; const u = 1 - Math.max(0, p.life) / p.max;
        if (p.life <= 0) { p.m.visible = false; continue; }
        p.m.position.x += p.vx * dt; p.m.position.y += p.vy * dt; p.m.position.z += p.vz * dt;
        p.m.scale.setScalar(0.014 + 0.03 * u + 0.01 * jet);
        p.m.material.color.setRGB(1, 0.62 - 0.35 * u, 0.25 - 0.15 * u).multiplyScalar(1 - u * u);
      }
    }
  }
  const attSet = { cur: null, lo: null };       // lo = the low-LOD twin's mesh pair (created lazily)
  function setSuit(o) {
    applySuit(o);
    let S;
    if (VIL) {
      const k = 'a|' + theme.getHex() + '|' + cur.vill + '|' + JSON.stringify([cur.suit, cur.role, cur.rig]);
      if (attKey && attKey !== k) VILLAGER_GEO.release(attKey, freeSet), attKey = null;
      if (attKey !== k) { S = VILLAGER_GEO.acquire(k, genAtt); if (!S) return; attKey = k; } else S = attSet.cur;
    } else { S = genAtt(); if (!S) return; for (const g of [attLit.geometry, attEmi.geometry, attGlass.geometry]) if (g !== EMPTY_GEO) g.dispose(); }
    attLit.geometry = S.geoLit; attEmi.geometry = S.geoEmi;
    attGlass.geometry = S.geoGlass || EMPTY_GEO; attGlass.visible = !!S.geoGlass;
    if (attSet.lo) { attSet.lo[0].geometry = S.geoLit; attSet.lo[1].geometry = S.geoEmi; }
    if (VIL && !jetNozzles.length) jetNozzles.push(new THREE.Vector3(0, 0.44, 0.205));      // cache hit skipped genAtt
    attSet.cur = S; glassMat.color.copy(cols[2]); paintSet(S); applyRig();
    rebuildJetFx();
    emiBase = 1 + 0.2 * cur.suit.scanner;
    if (VIL) { group.userData.tris = (hi.geoLit.attributes.position.count + hi.geoEmi.attributes.position.count + S.geoLit.attributes.position.count + S.geoEmi.attributes.position.count) / 3; if (group.userData.tris > VILLAGER_TRI_BUDGET) console.warn('villager LOD over budget', group.userData.tris); }
    group.userData.suit = Object.assign({}, cur.suit); group.userData.role = cur.role; group.userData.extra = cur.extra.slice();
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

  // ── Q10 rig: bone scales (torso / head / arms / legs) + genAtt extras; the animator reads rg for hip height, breathing scale and the gait table ──
  function applyRig() {
    const r = cur.rig, ts = r.ts, aw = r.limbShape === 'cube' ? 1.15 : 1, fs = r.feet === 'none' ? 0.6 : 1;
    rg.tx = ts[0]; rg.ty = ts[1]; rg.tz = ts[2]; rg.hipOff = r.hipOff; rg.legLen = r.leg; rg.stride = r.gait.stride; rg.bounce = r.gait.bounce; rg.arm = r.gait.arm; rg.lean = r.gait.lean; rg.stance = r.stance;
    rg.wings = r.extras.indexOf('wings') >= 0 ? 1 : 0; rg.orbs = r.extras.indexOf('orbs') >= 0 ? 1 : 0;
    B.head.scale.set(r.hs[0] / ts[0], r.hs[1] / ts[1], r.hs[2] / ts[2]);                // counter the torso scale so headScale is absolute (head joint = head centre)
    rg.headY = JOINTS.head[1] - JOINTS.torso[1] - r.headDrop / ts[1]; B.head.position.set(0, rg.headY, 0);
    for (const sd of ['L', 'R']) {
      B['sh' + sd].scale.set(aw / ts[0], r.arm / ts[1], aw / ts[2]);
      B['hd' + sd].scale.set(1 / aw, 1 / r.arm, 1 / aw);                                  // mitts / boots stay round when limbs stretch
      B['hp' + sd].scale.set(1, r.leg, 1); B['ft' + sd].scale.set(fs, fs / r.leg, fs);
    }
    B.pack.scale.set(1 / ts[0], 1 / ts[1], 1 / ts[2]);
    group.userData.rig = r; group.userData.eyeHeight = r.hipOff + HIP_Y + 0.41 * ts[1] - r.headDrop; group.userData.headPos.set(0, group.userData.eyeHeight, 0);
    if (VIL && r.palette) unshare();
    palette = r.palette; if (r.palette && r.palette.theme != null) setColor(r.palette.theme); else repaint();
  }
  function setRig(rc) { setSuit({ rig: rc, extra: [] }); return cur.rig; }
  // ── procedural animation (distance-driven gait, 2-bone leg IK, weight / lean / lag) ──
  // st: moving running airborne speed(0..1) facing jet scan | vH (ground speed, human heights / s) dist (heights moved this frame) turn (yaw rate rad/s, + = left)
  //     look {x,z,d} (target in model-local H: x right, z back) | groundAt(lx, lz) -> ground height under a foot relative to the standing plane (H; on group.userData)
  let t = 0, wingPh = 0, psi = 0, vs = 0, vsPrev = 0, accS = 0, run = 0, air = 0, crouch = 0, pit = 0, wasAir = false, sqz = 0, sqzT = 0, takeT = 0, yaw = 0;
  let rollS = 0, lookY = 0, lookG = 0, shiftX = 0, shiftTo = 0, shiftT = 3 + Math.random() * 2, gestT = 4 + Math.random() * 5, gestK = 0, gestSide = 1, dhL = 0, dhR = 0;
  const lerp = (a, b, k) => a + (b - a) * k;
  const damp = (cur, tgt, rate, dt) => cur + (tgt - cur) * (1 - Math.exp(-rate * dt));
  const sstep = (a, b, x) => { const u = Math.max(0, Math.min(1, (x - a) / (b - a))); return u * u * (3 - 2 * u); };
  const ik = { a: 0, k: 0 };
  // 2-bone leg in the sagittal plane: foot at (f forward, d below the hip joint); thigh length L1, shin L2. Knee bends forward (shin goes back).
  function legIK(f, d, L1, L2) {
    let D = Math.hypot(f, d); const lam = Math.atan2(f, d), Dm = (L1 + L2) * 0.998, Dn = Math.abs(L1 - L2) + 0.02;
    D = Math.max(Dn, Math.min(Dm, D));
    ik.k = -(Math.PI - Math.acos(Math.max(-1, Math.min(1, (L1 * L1 + L2 * L2 - D * D) / (2 * L1 * L2)))));
    ik.a = lam + Math.acos(Math.max(-1, Math.min(1, (L1 * L1 + D * D - L2 * L2) / (2 * L1 * D))));
  }
  // one leg's gait target: p in [0,1) cycle phase, duty = stance fraction, E = half stance excursion, lift = swing height. returns f (forward), y (lift), toe pitch
  const gt = { f: 0, y: 0, pt: 0 };
  function gaitFoot(p, duty, E, lift) {
    if (p < duty) { const u = p / duty; gt.f = E * (1 - 2 * u); gt.y = 0; gt.pt = lerp(0.3, -0.4, sstep(0.55, 1, u)) * (u < 0.55 ? 1 - 0.7 * (u / 0.55) : 1); }
    else { const u = (p - duty) / (1 - duty), e = sstep(0, 1, u); gt.f = -E + 2 * E * e; gt.y = lift * Math.sin(Math.PI * Math.pow(u, 0.8)); gt.pt = lerp(-0.4, 0.3, u); }
  }
  group.userData.bones = bones;

  function update(dt, st = {}) {
    dt = Math.min(dt, 0.1); t += dt;
    const sp = Math.max(0, Math.min(1, st.speed == null ? 1 : st.speed));
    const vWant = !st.moving ? 0 : st.vH != null ? st.vH : 2.6 + Math.max(0, sp - 0.5) * 8.8;
    vs = damp(vs, vWant, 14, dt);
    const acc = (vs - vsPrev) / Math.max(dt, 1e-4); vsPrev = vs; accS = damp(accS, Math.max(-60, Math.min(60, acc)), 10, dt);
    run = damp(run, st.running && st.moving ? 1 : 0, 8, dt);
    air = damp(air, st.airborne ? 1 : 0, 14, dt);
    if (wasAir && !st.airborne) { sqz = 1; sqzT = 0.13; }                       // landing squash: y 0.9 held 6 frames, then relaxes
    if (!wasAir && st.airborne) takeT = 0.12;                                     // takeoff stretch
    wasAir = !!st.airborne;
    sqzT = Math.max(0, sqzT - dt); takeT = Math.max(0, takeT - dt);
    if (sqzT <= 0) sqz = damp(sqz, 0, 11, dt);
    crouch = damp(crouch, st.crouch > 0.5 ? 1 : 0, st.crouch > 0.5 ? 30 : 9, dt);      // MARO styles drive crouch (long-jump / backflip wind-up, slide)
    pit = damp(pit, st.pitch > 0.5 ? 1 : 0, 14, dt);                              // dive / belly slide: body face-down
    if (st.facing != null) group.rotation.y = st.facing;

    const g = 1 - air, gw = sstep(0.12, 0.9, vs) * g;                             // ground weight, gait weight
    // gait (all distance driven): step rate R grows with speed, step length Ls = v / R, stance duty shrinks (walk > 0.5 > run), foot excursion = Ls * duty
    const R = Math.max(2.4, Math.min(7.5, 2.6 + 0.55 * vs)) * rg.stride, Ls = Math.max(0.12, vs / R), duty = Math.max(0.28, Math.min(0.62, 0.62 - 0.07 * vs));
    const dist = st.dist != null ? st.dist : vs * dt;
    psi = (psi + (vs > 0.05 ? dist / (2 * Ls) : 0)) % 1;
    const E = Math.min(0.21, Ls * duty), lift = 0.05 + 0.04 * Math.min(1, vs / 8);
    const c2 = Math.cos(2 * Math.PI * psi), s2 = Math.sin(2 * Math.PI * psi);
    const turn = st.turn || 0, br = Math.sin(t * Math.PI * 0.8);

    // idle: weight shift every 3-5 s (hips slide over one foot, legs counter so the feet stay put)
    shiftT -= dt; if (shiftT <= 0) { shiftT = 3 + Math.random() * 2; shiftTo = (Math.random() < 0.5 ? -1 : 1) * 0.018; }
    shiftX = damp(shiftX, shiftTo * (1 - gw) * g, 2.2, dt);

    // hips: bob at twice the step frequency (high at mid-stance when walking, low at mid-stance when running), crouch, landing squash, airborne stretch
    const sgn = Math.max(-1, Math.min(1, (duty - 0.5) * 10)), bobA = lerp(0.014, 0.035, sstep(1, 8, vs)) * rg.bounce * gw;
    const bob = bobA * sgn * Math.cos(4 * Math.PI * (psi - duty / 2));
    const reach = 0.40 * rg.legLen, sink = Math.max(0, 0.42 * rg.legLen - Math.sqrt(Math.max(0.01, reach * reach - E * E))) * gw + 0.015 * (1 - gw) * g;
    const sy = 1 - 0.1 * sqz + 0.07 * (takeT / 0.12) + 0.05 * air * (1 - sqz) - 0.1 * crouch * 0.6, sxz = 1 + 0.09 * sqz - 0.03 * air + 0.06 * crouch * 0.6;
    let hipY = HIP_Y + rg.hipOff + bob - sink - 0.09 * sqz - 0.17 * crouch - 0.05 * air + (1 - gw) * g * br * 0.003;
    B.hips.scale.set(sxz, sy, sxz);
    const gAt = group.userData.groundAt, sz = 0.02 - rg.stance;

    // legs: foot targets -> IK -> bone angles; blended with the jump tuck while airborne
    const legs = [['L', 0, -1], ['R', 0.5, 1]], hYaw = -0.1 * c2 * gw, hRoll = shiftX * 2.2 + 0.03 * s2 * gw;      // pelvis yaw / roll move the hip joints: the IK target compensates so a planted foot stays planted
    let minDh = 0;
    const dhs = [0, 0];      // slopes: each foot's target rides the ground under it, the pelvis drops toward the lower one
    for (let li = 0; li < 2; li++) {
      const [sd, off, sx] = legs[li], p = (psi + off) % 1;
      gaitFoot(p, duty, E, lift);
      const f = gt.f * gw;
      let dh = 0;
      if (gAt && g > 0.4) { try { dh = Math.max(-0.25, Math.min(0.3, +gAt(sx * 0.075, -(f + 0.035)) || 0)); } catch (e) { dh = 0; } }
      if (li === 0) dhL = damp(dhL, dh, 24, dt); else dhR = damp(dhR, dh, 24, dt);
      dhs[li] = li === 0 ? dhL : dhR; if (dhs[li] < minDh) minDh = dhs[li];
    }
    hipY += 0.5 * minDh * g;
    B.hips.position.y = hipY; B.hips.position.x = shiftX;
    const ankle = HIP_Y + rg.hipOff - 0.44 * rg.legLen;                            // ankle height above the standing plane in bind pose
    for (let li = 0; li < 2; li++) {
      const [sd, off, sx] = legs[li], p = (psi + off) % 1;
      gaitFoot(p, duty, E, lift);
      const f = gt.f * gw, ly = gt.y * gw, pt = gt.pt * gw;
      const d = (hipY - 0.02 * sy - (ankle + ly + dhs[li])) / sy, fl2 = (f - sx * 0.075 * Math.sin(hYaw)) / sxz;
      legIK(fl2, Math.max(0.05, d), 0.23 * rg.legLen, 0.19 * rg.legLen);
      let a = ik.a, k = ik.k;
      // airborne tuck (ground weight g blends the planted leg back in on landing)
      const tuckA = li === 0 ? 0.8 : 0.5, tuckK = li === 0 ? -1.5 : -1.1;
      a = lerp(tuckA, a, g); k = lerp(tuckK, k, g);
      B['hp' + sd].rotation.set(a, 0, (sd === 'L' ? sz : -sz) - shiftX / Math.max(0.2, d) - hRoll * g);
      B['kn' + sd].rotation.x = k;
      B['ft' + sd].rotation.x = -(a + k) + pt * (1 - air) + 0.5 * air;
      B['ft' + sd].rotation.z = sd === 'L' ? -sz : sz;
    }
    B.hips.rotation.y = hYaw;
    B.hips.rotation.x = (st.flip > 0 && st.flipKind > 0 && st.flipKind < 4 ? (st.flipKind === 1 ? -1 : 1) * Math.PI * 2 * st.flip : 0) - 1.35 * pit;
    B.hips.rotation.z = hRoll;

    // torso: lean into acceleration and speed, roll into turns, counter-rotated shoulders, breathing
    rollS = damp(rollS, Math.max(-0.26, Math.min(0.26, turn * Math.min(vs, 6) * 0.03)), 9, dt);
    const lean = lerp(0.04, 0.2, sstep(0.5, 6, vs)) * g * rg.lean + Math.max(-0.12, Math.min(0.22, accS * 0.012)) * g + 0.12 * crouch + 0.1 * air + 0.05 * (1 - gw) * g;
    // look-at: head turns toward a nearby target (within 6 H), body follows a quarter of it
    const lk = st.look; let tgtY = 0, has = false;
    if (lk && lk.d < 6 && g > 0.5) { tgtY = Math.max(-1.1, Math.min(1.1, Math.atan2(-lk.x, -lk.z))); has = true; }
    lookG = damp(lookG, has ? 1 : 0, 4, dt); lookY = damp(lookY, tgtY, 7, dt);
    const idleLook = (Math.sin(t * 0.37) * 0.35 + Math.sin(t * 0.91) * 0.12) * (1 - gw) * g * (1 - lookG);
    const tw = 0.18 * c2 * gw + 0.25 * lookY * lookG + turn * 0.04;
    B.torso.rotation.set(-lean + (1 - gw) * br * 0.012, tw, rollS);
    B.torso.scale.set(rg.tx * (1 + (1 - gw) * br * 0.008), rg.ty * (1 + (1 - gw) * br * 0.015), rg.tz * (1 + (1 - gw) * br * 0.02));
    // head: stabilised (counter-lean, counter-roll, counter-bob), leads turns, looks at targets
    B.head.rotation.set(lean * 0.85 - 0.05 * air + 0.012 * gw * Math.sin(4 * Math.PI * psi - 0.7), idleLook + 0.75 * lookY * lookG + Math.max(-0.5, Math.min(0.5, turn * 0.1)) - tw * 0.8, -rollS * 0.7 - shiftX * 1.5);
    B.head.position.y = rg.headY - bob * 0.6 - 0.006 * (1 - gw) * br;
    const bl = (t % 4.3) < 0.14 ? Math.abs(Math.sin((t % 4.3) / 0.14 * Math.PI)) : 0; B.fc.scale.set(1, 1 - 0.9 * bl, 1);      // blink

    // arms: opposite to the legs, trailing them by a beat; elbow bends with speed; jump = arms up; villagers add an idle gesture
    const aAmp = lerp(0.45, 0.95, sstep(1, 8, vs)) * rg.arm, ph2 = 2 * Math.PI * (psi - 0.07), ea = Math.cos(ph2) * (1.1 - 0.1 * Math.cos(ph2) * Math.cos(ph2));
    let alS = ea * aAmp * gw, arS = -ea * aAmp * gw;
    let alE = 0.22 + (0.12 + 0.75 * sstep(2, 9, vs)) * gw, arE = alE;
    alS = lerp(-2.5, alS, g); arS = lerp(-2.5, arS, g);
    alE = lerp(0.5, alE, g); arE = lerp(0.5, arE, g);
    alS += 0.3 * crouch + 0.25 * sqz; arS += 0.3 * crouch + 0.25 * sqz;
    if (VIL) {
      gestT -= dt; if (gestT <= 0 && gestK <= 0 && gw < 0.1 && g > 0.5) { gestT = 5 + Math.random() * 5; gestK = 1.4; gestSide = Math.random() < 0.5 ? -1 : 1; }
      if (gestK > 0) {
        gestK -= dt; const u = Math.sin(Math.max(0, Math.min(1, gestK / 1.4)) * Math.PI), wv = Math.sin(t * 9) * 0.25 * u;
        if (gestSide < 0) { alS = lerp(alS, -1.4, u); alE = lerp(alE, 1.1 + wv, u); } else { arS = lerp(arS, -1.4, u); arE = lerp(arE, 1.1 + wv, u); }
      }
    }
    const sway = br * 0.03 * (1 - gw) * g;
    B.shL.rotation.set(alS + sway, 0, 0.08 + 0.55 * air - rollS * 0.5); B.shR.rotation.set(arS - sway, 0, -0.08 - 0.55 * air - rollS * 0.5);
    B.elL.rotation.x = alE; B.elR.rotation.x = arE;
    B.elL.rotation.z = B.elR.rotation.z = 0;

    if (rg.wings) { const mv = Math.min(1, vs / 3), cd = st.cad == null ? mv : Math.min(1, st.cad), fa = 0.18 + 0.5 * air + 0.12 * mv; wingPh += dt * (4 + 12 * air + 5 * cd * (1 - air)); const fl2 = Math.sin(wingPh) * fa; B.wgL.rotation.z = -fl2; B.wgR.rotation.z = fl2; }      // Q54: beat rate follows cadence on the ground; phase integrated so rate changes do not jump
    if (rg.orbs) B.orb.rotation.y = t * 0.9;
    // jetpack: backpack vibration + nozzle flame scale; scanner: visor pulse (emissive brightness)
    const jet = Math.max(0, Math.min(1, st.jet || 0)), scan = Math.max(0, Math.min(1, st.scan || 0));
    const vib = jet * (0.5 + 0.5 * Math.sin(t * 61));
    B.pack.rotation.set(jet * 0.018 * Math.sin(t * 47), 0, jet * 0.018 * Math.sin(t * 53 + 1));
    B.pack.position.set(0, 0.10 + jet * 0.003 * Math.sin(t * 71), 0.125 + jet * 0.002 * Math.sin(t * 59));
    const fl = jet > 0.02 ? (0.25 + 1.5 * jet) * (0.88 + 0.12 * Math.sin(t * 43)) : 0.0001;
    B.jet.scale.setScalar(0.0001);      // the baked flame boxes stay hidden; updateJetFx drives the visible jet
    updateJetFx(dt, jet, t);
    emiMat.color.setScalar(emiBase * (1 + scan * 0.55 * (0.5 + 0.5 * Math.sin(t * 9))));
  }

  function dispose() {
    if (hiKey) VILLAGER_GEO.release(hiKey, freeSet); else freeSet(hi); if (loGroup) loGroup.userData.geoms.forEach(g => g.dispose()); if (attKey) VILLAGER_GEO.release(attKey, freeSet); else { attLit.geometry.dispose(); attEmi.geometry.dispose(); if (attGlass.geometry !== EMPTY_GEO) attGlass.geometry.dispose(); } if (pet) pet.dispose(); flameGeo.dispose(); puffGeo.dispose(); glassMat.dispose(); EMPTY_GEO.dispose(); flameMatO.dispose(); flameMatI.dispose(); for (const p of JETFX.puffs) p.m.material.dispose(); litMat.dispose(); emiMat.dispose(); skeleton.dispose();
    if (group.parent) group.parent.remove(group);
  }
  return { group, update, setColor, setSuit, setRig, pet, dispose, setAccent(hex) { rimHex = hex; derive(); } };
}

// ── companion critter: 3 silhouettes, ~0.4 human height (feet at y=0, forward -Z), <= 600 tris, no skeleton ──
export function createPet(THREE, o = {}) {
  const seed = o.seed != null ? (o.seed | 0) : 1, rng = pmul(seed * 131 + 7), PLANS = ['walker', 'jelly', 'strider'];
  let plan = o.plan; if (typeof plan === 'number') plan = PLANS[((plan % 3) + 3) % 3]; if (!PLANS.includes(plan)) plan = PLANS[Math.abs(seed) % 3];
  const hue = rng(), body = new THREE.Color().setHSL(hue, 0.5, 0.52), belly = new THREE.Color().setHSL(hue, 0.4, 0.72), dark = new THREE.Color().setHSL(hue, 0.35, 0.26), glowC = new THREE.Color().setHSL((hue + 0.45) % 1, 0.9, 0.65);
  const litMat = mkToon(THREE), emiMat = new THREE.MeshBasicMaterial({ vertexColors: true });
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
