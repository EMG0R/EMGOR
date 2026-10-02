// On-foot traveller (NMS vibe). Height = 1.0 unit, forward = -Z (same as the ship nose), up +Y, feet at y=0.
// SCALE: the hull is 1 unit long and a human should be ~0.09 of that, so the caller does group.scale.setScalar(0.09 * shipScale).
// One SkinnedMesh (lit, flat-shaded ShaderMaterial, vertex colors) + one emissive SkinnedMesh (visor, chest strip, jet glow) sharing a skeleton = 2 draw calls, ~700 tris.
// API: createHuman(THREE, {color}) -> { group, update(dt, {moving,running,airborne,speed 0..1,facing rad}), setColor(hex), dispose }
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

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
};
const BONES = Object.keys(JOINTS);

export function createHuman(THREE, opts = {}) {
  const theme = new THREE.Color(opts.color != null ? opts.color : 0xB86CFF);
  const DARK = 0x2A2430, SUIT = 0x3A3446, GREY = 0x8A8691;
  const parts = [], emiParts = [];
  const bi = n => BONES.indexOf(n);

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
    geo.userData = { panel: !!o.panel, kind: o.emis ? 'e' : 'l' };
    (o.emis ? emiParts : parts).push(geo);
  }
  const box = (w, h, d, x, y, z, hex, bone, o) => { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); put(g, hex, bone, o); };
  // tapered limb segment between two world points, 6-sided
  const seg = (a, b, r0, r1, hex, bone, o) => {
    const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), len = A.distanceTo(B);
    const g = new THREE.CylinderGeometry(r0, r1, len, 6, 1);
    g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), B.clone().sub(A).normalize()));
    g.translate((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2); put(g, hex, bone, o);
  };

  // torso (+ pelvis on hips)
  box(0.30, 0.10, 0.17, 0, 0.50, 0, DARK, 'hips');
  box(0.31, 0.24, 0.19, 0, 0.67, 0, SUIT, 'torso');
  box(0.20, 0.13, 0.02, 0, 0.68, -0.105, theme, 'torso', { panel: true });         // chest panel
  box(0.09, 0.025, 0.012, 0, 0.72, -0.118, 0xFFFFFF, 'torso', { emis: true });       // chest strip
  box(0.33, 0.05, 0.20, 0, 0.79, 0, DARK, 'torso');                                  // collar/shoulders
  // backpack + jet nozzle
  box(0.22, 0.30, 0.11, 0, 0.66, 0.15, DARK, 'torso');
  box(0.16, 0.06, 0.02, 0, 0.74, 0.21, theme, 'torso', { panel: true });
  { const g = new THREE.CylinderGeometry(0.035, 0.05, 0.08, 6); g.translate(0, 0.46, 0.15); put(g, GREY, 'torso'); }
  { const g = new THREE.CylinderGeometry(0.03, 0.03, 0.012, 6); g.translate(0, 0.40, 0.15); put(g, 0xF4F8FF, 'torso', { emis: true }); }
  // head: helmet + visor + neck
  box(0.07, 0.04, 0.07, 0, 0.82, 0, DARK, 'head');
  { const g = new THREE.SphereGeometry(0.115, 8, 6); g.translate(0, 0.925, 0); put(g, SUIT, 'head'); }
  box(0.17, 0.065, 0.06, 0, 0.93, -0.085, 0xFFFFFF, 'head', { emis: true });         // visor (tinted by theme)
  box(0.05, 0.05, 0.05, -0.12, 0.92, 0, theme, 'head', { panel: true });             // ear pods
  box(0.05, 0.05, 0.05, 0.12, 0.92, 0, theme, 'head', { panel: true });
  // arms
  for (const s of ['L', 'R']) {
    const x = s === 'L' ? -1 : 1, sh = 'sh' + s, el = 'el' + s;
    { const g = new THREE.SphereGeometry(0.06, 6, 4); g.translate(x * 0.15, 0.76, 0); put(g, theme, sh, { panel: true }); }
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

  const geoLit = mergeGeometries(parts), geoEmi = mergeGeometries(emiParts);
  // remember which lit vertices are theme panels, and which emissive verts are theme-tinted (all but chest strip / nozzle)
  const panelMask = [];
  parts.forEach(g => { for (let i = 0; i < g.attributes.position.count; i++) panelMask.push(g.userData.panel ? 1 : 0); });
  // emissive put order: chest strip(0), nozzle glow(1), visor(2); only the visor is theme-tinted
  const emiTint = [];
  emiParts.forEach((g, k) => { for (let i = 0; i < g.attributes.position.count; i++) emiTint.push(k === 2 ? 1 : 0); });

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

  function setColor(hex) {
    theme.set(hex);
    const lc = geoLit.attributes.color, ec = geoEmi.attributes.color;
    for (let i = 0; i < panelMask.length; i++) if (panelMask[i]) lc.setXYZ(i, theme.r, theme.g, theme.b);
    for (let i = 0; i < emiTint.length; i++) if (emiTint[i]) ec.setXYZ(i, theme.r * 0.6 + 0.4, theme.g * 0.6 + 0.4, theme.b * 0.6 + 0.4);
    lc.needsUpdate = ec.needsUpdate = true;
  }
  setColor(theme.getHex());

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
  }

  function dispose() {
    geoLit.dispose(); geoEmi.dispose(); litMat.dispose(); emiMat.dispose(); skeleton.dispose();
    if (group.parent) group.parent.remove(group);
  }
  return { group, update, setColor, dispose };
}
