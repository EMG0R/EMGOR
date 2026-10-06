// ship-station.js — the 7/11 space station (docs/ship-mode.md rev 22 B).
//
//   createStation(engine, L, opts) -> station
//   Model units: 1 unit = 1 L (the ship's length); the group is scaled by L, so "station-local" coordinates below are in L.
//   Frame: +Z = out of the hangar mouth, -Z = the engines, +Y = up (deck floor at y = 0), spine on the Z axis.
//
//   station.group               THREE.Group in engine.scene (hidden until setVisible(true))
//   station.update(t, dt, shipWorldPos)   orbit (2.2 x coreR around engine.blackHole.pos, xz plane) + yaw sway + ring spin + lights + NPC idle
//   station.pos / .quat         live references (group.position / group.quaternion);   station.vel = world velocity (units/s)
//   station.length (world)  station.lenL (in L)  station.unit = L   station.rescale()  re-read refR (call after boarding) and rebuild if it changed
//   station.mouth  { pos, dir (out of the bay), halfW, halfH (world), trigger(shipPos)->bool }   pos = centre of the opening
//   station.pads   [{ pos (world, ship-origin height), quat (nose into the bay), free:true }] x4
//   station.dockPath(i) / launchPath(i)   [Vector3 world] 48 arc-length-spaced points (dock: 60 L outside the mouth -> pad; launch: pad -> 120 L out)
//   station.deck   { floorAt(worldPos,out?) -> { point, normal, height, inside }, walls:[{min,max,name,field?}] (station-local, L), bounds:{min,max} }
//   station.interior { stores:[storeLike], npcs:[{human,name,lines[3],pos,localPos}], mapPedestal:{pos,radius}, windows:[{pos,w,h}] }
//   station.interior.trade { pos, radius, buys:[stackKey], buyPrices:{key:gorCoin}, sells:[{id,item,n,price}], day }  TRADE terminal on the right wall (market seeded per UTC day; refresh(day?) rebuilds)
//   station.interior.shipyard { pos, radius, ships:[{kind,name,price,stats:{cruise,boost,cargo,guns,hp} (pct deltas vs hauler; guns absolute),blurb}] }  hangar-back alcove, 2 turntable display pads (fighter, explorer)
//   station.interior.questBoard { pos, radius, quests, setQuests([{title,reward,kind}]) }  'BOUNTIES' screen beside the mission board;  .landingFee (0)  .greetingLine(seed?) seeded PA line on dock
//   station.toLocal(v)/toWorld(v)  (local in L units)   station.setVisible(b)   station.stats {tris, meshes}   station.dispose()
import { STORE_NAME, createStandaloneStore, transformBoxes, fillStoreRecord } from './ship-world.js';
import * as lingo from './ship-lingo.js';
import { createHuman } from './ship-human.js';
import { weaponSetFor, generateWeapon } from './ship-weapons.js';
import { makeMarket } from './ship-space.js';
import { buildHull } from './ship-hull.js';
import { fmt } from './ship-craft.js';

const HW = 4.5, HH = 4.8, HD = 26;          // hangar inner half-width, height, depth (L)
const MOUTH_HW = 4.5, MOUTH_HH = 2.4;       // mouth half extents (opening is 9 x 4.8 L; centre y = 2.4)
const HUMAN_H = 0.09;

const C = { hull: 0x3a3352, hull2: 0x2c2640, hull3: 0x201b30, panel: 0x4a4266, violet: 0x8A3CFF, cyan: 0x5CE8FF, mag: 0xFF5CE1, amber: 0xFFC46B, white: 0xF4F8FF, floor: 0x7d7699, wall: 0x544c70, dark: 0x16121f };

function mul(seed) { let a = seed | 0; return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

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
const FRAG_LIT = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  vec3 n = normalize(cross(dFdx(vV), dFdy(vV)));
  vec3 Ld = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = abs(dot(n, Ld)) * 0.55 + 0.45;
  float rim = pow(1.0 - abs(dot(n, normalize(-vV))), 3.0) * 0.3;
  gl_FragColor = vec4(vC * d + rim * vec3(0.5, 0.3, 0.85), 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const FRAG_EMI = /* glsl */`
uniform float uTime;
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  gl_FragColor = vec4(vC * (0.95 + 0.06 * sin(uTime * 2.5 + vC.g * 9.0)), 1.0);
  #include <colorspace_fragment>
}`;
const VERT_LIGHT = /* glsl */`
uniform float uPx; uniform float uH;
varying vec3 vC; varying vec2 vUv;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = vec3(1.0);
  #ifdef USE_INSTANCING_COLOR
    vC = instanceColor;
  #endif
  vec4 c = instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0);
  float s = length(instanceMatrix[0].xyz) * length(modelViewMatrix[0].xyz);
  vec4 mv = modelViewMatrix * c;
  float minS = uPx * 2.0 * max(-mv.z, 0.0001) / (projectionMatrix[1][1] * uH);
  float size = max(s, minS);
  mv.xy += position.xy * size;
  vUv = position.xy * 2.0;
  gl_Position = projectionMatrix * mv;
  #include <logdepthbuf_vertex>
}`;
const FRAG_LIGHT = /* glsl */`
varying vec3 vC; varying vec2 vUv;
#include <common>
#include <logdepthbuf_pars_fragment>
void main(){
  #include <logdepthbuf_fragment>
  float r = length(vUv);
  if (r > 1.0) discard;
  float g = pow(1.0 - r, 2.0) * 1.1 + smoothstep(0.55, 0.0, r) * 1.4;
  gl_FragColor = vec4(vC * g * 2.2, min(1.0, g));
}`;

// ── merged vertex-coloured soup ──
function makeSoup(T) {
  const pos = [], col = [], c = new T.Color(), v = new T.Vector3(), m = new T.Matrix4(), q = new T.Quaternion(), e = new T.Euler(), s = new T.Vector3(), p = new T.Vector3();
  const unit = new T.BoxGeometry(1, 1, 1).toNonIndexed().attributes.position.array;
  const soup = {
    geo(arr, hex, mat, br = 1) {                  // arr = non-indexed position array
      c.set(hex); const r = c.r * br, g = c.g * br, b = c.b * br;
      for (let i = 0; i < arr.length; i += 3) {
        v.set(arr[i], arr[i + 1], arr[i + 2]); if (mat) v.applyMatrix4(mat);
        pos.push(v.x, v.y, v.z); col.push(r, g, b);
      }
    },
    // centre box with optional Euler rotation
    cbox(w, h, d, cx, cy, cz, hex, br = 1, rx = 0, ry = 0, rz = 0) {
      e.set(rx, ry, rz); q.setFromEuler(e); s.set(w, h, d); p.set(cx, cy, cz); m.compose(p, q, s);
      soup.geo(unit, hex, m, br);
    },
    // min/max box
    bx(x0, x1, y0, y1, z0, z1, hex, br = 1) { soup.cbox(x1 - x0, y1 - y0, z1 - z0, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2, hex, br); },
    shape(geometry, hex, mat, br = 1) { const g = geometry.index ? geometry.toNonIndexed() : geometry; soup.geo(g.attributes.position.array, hex, mat, br); g.dispose(); },
    geometry() {
      const g = new T.BufferGeometry();
      g.setAttribute('position', new T.Float32BufferAttribute(pos, 3));
      g.setAttribute('color', new T.Float32BufferAttribute(col, 3));
      g.computeBoundingSphere();
      return g;
    },
    tris() { return pos.length / 9; },
  };
  return soup;
}

function signCanvas() {
  const c = document.createElement('canvas'); c.width = 1024; c.height = 512;
  const x = c.getContext('2d');
  // top half: the 7/11 sign
  x.fillStyle = '#120c1c'; x.fillRect(0, 0, 1024, 256);
  x.font = '900 200px "Arial Black", Impact, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle';
  x.shadowColor = '#ff5ce1'; x.shadowBlur = 40; x.fillStyle = '#fff2fb'; x.fillText(STORE_NAME, 512, 140);
  x.shadowBlur = 0; x.strokeStyle = '#8a3cff'; x.lineWidth = 12; x.strokeRect(8, 8, 1008, 240);
  x.strokeStyle = '#5ce8ff'; x.lineWidth = 3; x.strokeRect(22, 22, 980, 212);
  // bottom half: the black hole seen from a window
  const g = x.createLinearGradient(0, 256, 0, 512); g.addColorStop(0, '#05030a'); g.addColorStop(1, '#0c0618');
  x.fillStyle = g; x.fillRect(0, 256, 1024, 256);
  const rng = mul(7);
  for (let i = 0; i < 160; i++) { x.fillStyle = 'rgba(255,255,255,' + (0.2 + rng() * 0.6) + ')'; x.fillRect(rng() * 1024, 256 + rng() * 256, 1.5, 1.5); }
  x.save(); x.translate(512, 384);
  for (let k = 0; k < 6; k++) {
    x.beginPath(); x.ellipse(0, 0, 330 - k * 22, 38 - k * 3, -0.08, 0, Math.PI * 2);
    x.strokeStyle = ['#ff9a4d', '#ffb86b', '#ff5ce1', '#b36cff', '#8a3cff', '#5ce8ff'][k]; x.globalAlpha = 0.55; x.lineWidth = 14 - k; x.shadowColor = x.strokeStyle; x.shadowBlur = 24; x.stroke();
  }
  x.globalAlpha = 1; x.shadowBlur = 0; x.fillStyle = '#000'; x.beginPath(); x.arc(0, 0, 92, 0, Math.PI * 2); x.fill();
  x.strokeStyle = '#d7a2ff'; x.lineWidth = 4; x.beginPath(); x.arc(0, 0, 94, 0, Math.PI * 2); x.stroke();
  x.restore();
  return c;
}

const CLERK_LINES = ['Welcome aboard. Prices are higher up here; so is the view.', 'Orbit is a lifestyle. Also a delivery surcharge.', 'Come back in one piece. The hangar floor is hard to clean.'];
const NPC_DEF = [
  { name: 'Vess', color: 0x5CE8FF, lines: ['Mind the field at the mouth. It is thinner than it looks.', 'I came here to fix one thing. That was a year ago.', 'The hole does not care about you. Weirdly comforting.'] },
  { name: 'Pim', color: 0xFF5CE1, lines: ['Everything pulls toward it. I just try to pull back.', 'The map table shows the whole galaxy. Do not breathe on it.', 'Tip: dock slowly. Or do not. I enjoy the show.'] },
  { name: 'Roan', color: 0xFFC46B, lines: ['This is the best job in the dark. Free coffee.', 'Four pads, one wrench. We make it work.', 'You flew in like you meant it. Nice.'] },
];

const SHIPS = [
  { kind: 'fighter', name: 'Wasp-class Fighter', price: 1800, stats: { cruise: 25, boost: 40, cargo: -50, guns: 2, hp: -20 }, blurb: 'Fast, twitchy, armed. Bring less luggage.' },
  { kind: 'explorer', name: 'Drifter Explorer', price: 2600, stats: { cruise: 10, boost: 20, cargo: -25, guns: 0, hp: 15 }, blurb: 'Long legs and a big window. Scanners included.' },
];
const PA_LINES = ['Docking complete. Please keep limbs inside the hangar.', 'Welcome to the 7/11. We never close; we only orbit.', 'Reminder: the black hole is not a shortcut.', 'Landing fee waived. Tipping the clerk is not.', 'Pad cleared. Mind the field on your way out.', 'Attention: someone left a wrench on pad three.'];
const QCOL = { mine: '#ffc46b', scout: '#5ce8ff', trade: '#8aff9c', bounty: '#ff5ce1' };
function textCanvas(w, h) { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; }
function yardSignCanvas() {
  const c = textCanvas(1024, 160), x = c.getContext('2d');
  x.fillStyle = '#120c1c'; x.fillRect(0, 0, 1024, 160); x.strokeStyle = '#5ce8ff'; x.lineWidth = 6; x.strokeRect(6, 6, 1012, 148);
  x.font = '900 104px "Arial Black", Impact, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.shadowColor = '#5ce8ff'; x.shadowBlur = 26; x.fillStyle = '#eafcff'; x.fillText('SHIPYARD', 512, 84);
  return c;
}
function questCanvas(quests) {
  const c = textCanvas(512, 576), x = c.getContext('2d');
  x.fillStyle = '#0d0818'; x.fillRect(0, 0, 512, 576); x.strokeStyle = '#ff5ce1'; x.lineWidth = 6; x.strokeRect(5, 5, 502, 566);
  x.font = '900 56px "Arial Black", Impact, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.shadowColor = '#ff5ce1'; x.shadowBlur = 18; x.fillStyle = '#ffe9fa'; x.fillText('BOUNTIES', 256, 48); x.shadowBlur = 0;
  x.textAlign = 'left'; const q = quests || [];
  if (!q.length) { x.font = '28px sans-serif'; x.fillStyle = '#8a7aa8'; x.fillText('No bounties posted.', 36, 140); }
  q.slice(0, 5).forEach((o, i) => {
    const y = 92 + i * 94, col = QCOL[o.kind] || '#d7a2ff';
    x.fillStyle = 'rgba(138,60,255,0.16)'; x.fillRect(20, y, 472, 84); x.fillStyle = col; x.fillRect(20, y, 6, 84);
    x.font = 'bold 26px sans-serif'; x.fillStyle = '#f4f8ff'; let t = String(o.title || '?'); while (t.length > 3 && x.measureText(t).width > 440) t = t.slice(0, -2);
    x.fillText(t, 38, y + 28); x.font = '22px sans-serif'; x.fillStyle = col; x.fillText(String(o.kind || '').toUpperCase(), 38, y + 62);
    x.textAlign = 'right'; x.fillStyle = '#ffc46b'; x.fillText(typeof o.reward === 'number' ? fmt(o.reward) : String(o.reward == null ? '' : o.reward), 480, y + 62); x.textAlign = 'left';
  });
  return c;
}

export function createStation(engine, L, opts) {
  opts = opts || {};
  const T = engine.THREE, scene = engine.scene, root = engine.root;
  L = L > 0 ? L : 0.03;
  const group = new T.Group(); group.name = 'ship-station'; group.visible = false; group.scale.setScalar(L);
  scene.add(group);

  const st = {
    group, unit: L, lenL: 0, length: 0, pos: group.position, quat: group.quaternion, vel: new T.Vector3(),
    mouth: { pos: new T.Vector3(), dir: new T.Vector3(0, 0, 1), halfW: MOUTH_HW * L, halfH: MOUTH_HH * L, trigger: null },
    pads: [], deck: { walls: [], bounds: { min: new T.Vector3(), max: new T.Vector3() }, floorAt: null },
    interior: { stores: [], npcs: [], mapPedestal: { pos: new T.Vector3(), radius: 0.45 * L }, windows: [], board: { pos: new T.Vector3(), radius: 0.6 * L }, missions: null, landingFee: 0, greetingLine: null, shipyard: { pos: new T.Vector3(), radius: 3 * L, ships: SHIPS }, questBoard: { pos: new T.Vector3(), radius: 0.6 * L, quests: [], setQuests: null }, trade: { pos: new T.Vector3(), radius: 0.6 * L, buys: [], buyPrices: {}, sells: [], day: 0, refresh: null } },
    stats: { tris: 0, meshes: 0 },
  };
  const uTime = { value: 0 }, uPx = { value: 6.5 }, uH = { value: 900 };
  const matLit = new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_LIT, vertexColors: true, side: T.DoubleSide, uniforms: {} });
  const matEmi = new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_EMI, vertexColors: true, side: T.DoubleSide, uniforms: { uTime } });
  const matLight = new T.ShaderMaterial({ vertexShader: VERT_LIGHT, fragmentShader: FRAG_LIGHT, uniforms: { uPx, uH }, transparent: true, depthWrite: false, blending: T.AdditiveBlending });
  const tex = new T.CanvasTexture(signCanvas()); tex.colorSpace = T.SRGBColorSpace; tex.anisotropy = 4;
  const matSign = new T.MeshBasicMaterial({ map: tex, toneMapped: false, side: T.DoubleSide });
  const matField = new T.MeshBasicMaterial({ color: C.cyan, transparent: true, opacity: 0.07, depthWrite: false, side: T.DoubleSide });
  const quadGeo = new T.PlaneGeometry(1, 1);
  // shipyard sign + bounties screen (2 draws) + 2 display hulls
  const yardTex = new T.CanvasTexture(yardSignCanvas()); yardTex.colorSpace = T.SRGBColorSpace;
  const yardSign = new T.Mesh(quadGeo, new T.MeshBasicMaterial({ map: yardTex, toneMapped: false })); yardSign.name = 'station-yard-sign'; yardSign.frustumCulled = false;
  let qTex = new T.CanvasTexture(questCanvas([])); qTex.colorSpace = T.SRGBColorSpace;
  const qMat = new T.MeshBasicMaterial({ map: qTex, toneMapped: false });
  const questMesh = new T.Mesh(quadGeo, qMat); questMesh.name = 'station-bounties'; questMesh.frustumCulled = false; questMesh.rotation.y = -Math.PI / 2;
  const yardHulls = SHIPS.map((sh) => { const g = new T.Group(); g.name = 'yard-' + sh.kind; let h = null; try { h = buildHull(T, { kind: sh.kind, lod: 'low' }); } catch (e) { h = null; } if (h) { const b = new T.Box3().setFromObject(h), sz = b.getSize(new T.Vector3()), c = b.getCenter(new T.Vector3()); const k = 2.6 / Math.max(sz.x, sz.y, sz.z, 1e-6); h.scale.multiplyScalar(k); h.position.set(-c.x * k, -c.y * k, -c.z * k); g.add(h); } return g; });
  group.add(yardSign, questMesh, ...yardHulls);

  // meshes (rebuilt geometry on rescale)
  const hullMesh = new T.Mesh(new T.BufferGeometry(), matLit); hullMesh.name = 'station-hull';
  const emiMesh = new T.Mesh(new T.BufferGeometry(), matEmi); emiMesh.name = 'station-emissive';
  const ring = new T.Group(); ring.name = 'station-ring';
  const ringMesh = new T.Mesh(new T.BufferGeometry(), matLit); ringMesh.name = 'station-ring-hull';
  const ringEmi = new T.Mesh(new T.BufferGeometry(), matEmi); ringEmi.name = 'station-ring-emissive';
  ring.add(ringMesh, ringEmi);
  const signMesh = new T.Mesh(new T.BufferGeometry(), matSign); signMesh.name = 'station-signs';
  const fieldMesh = new T.Mesh(quadGeo, matField); fieldMesh.name = 'station-field';
  let lightMesh = null, lightList = [], lightCol = null;
  [hullMesh, emiMesh, ringMesh, ringEmi, signMesh, fieldMesh].forEach((m) => { m.frustumCulled = false; });
  group.add(hullMesh, emiMesh, ring, signMesh, fieldMesh);

  // local frame helpers
  const _m4 = new T.Matrix4(), _inv = new T.Matrix4(), _v = new T.Vector3(), _v2 = new T.Vector3(), _q = new T.Quaternion(), _up = new T.Vector3(0, 1, 0);
  st.toWorld = (v, out) => (out || new T.Vector3()).copy(v).multiplyScalar(1).applyMatrix4(group.matrixWorld);
  st.toLocal = (v, out) => (out || new T.Vector3()).copy(v).applyMatrix4(_inv.copy(group.matrixWorld).invert());
  const toW = (x, y, z, out) => (out || new T.Vector3()).set(x, y, z).applyMatrix4(group.matrixWorld);

  // ── layout (set by build) ──
  let zF = 0, zR = 0, lenL = 0, padLocal = [], storeWalls = [];
  let ss = null;                                  // the standalone INTERGALACTIC 7/11 (x1.5 scale, rotated so its glass front faces +x into the hangar)
  const STORE_S = 1.5 * HUMAN_H / 1.75;           // L per metre
  const lay = { counter: new T.Vector3(), clerk: new T.Vector3(), ped: new T.Vector3(), board: new T.Vector3(), yard: [new T.Vector3(), new T.Vector3()], quest: new T.Vector3(), trade: new T.Vector3(), npc: [] };

  function build(newLenL) {
    lenL = newLenL; st.lenL = lenL; st.length = lenL * L;
    zF = lenL / 2; zR = zF - HD;
    const B = makeSoup(T), E = makeSoup(T), RB = makeSoup(T), RE = makeSoup(T), rng = mul(711);
    const lights = []; lightList = lights;
    const lt = (x, y, z, size, hex, mode, phase, speed) => lights.push({ x, y, z, size, c: new T.Color(hex), mode: mode || 0, ph: phase || 0, sp: speed || 1 });
    const k = Math.max(1, lenL / 300), kw = Math.pow(k, 0.95);
    const cy = 2.4, WO = HW + 1.0;                 // spine centre height, hangar outer half width
    // ── hangar shell ──
    B.bx(-WO, WO, -1.0, 0, zR - 1, zF, C.floor, 0.8);                           // floor slab (walkable top)
    B.bx(-WO, -HW, 0, HH + 1, zR - 1, zF, C.wall); B.bx(HW, WO, 0, HH + 1, zR - 1, zF, C.wall);
    B.bx(-WO, WO, HH, HH + 1, zR - 1, zF, C.hull2);                              // ceiling
    B.bx(-HW, HW, 0, HH, zR - 1, zR, C.wall, 0.9);                               // back wall
    // mouth bezel: heavy frame
    B.bx(-WO - 0.6, -HW, -1.4, HH + 1.4, zF - 1.2, zF + 0.4, C.hull3); B.bx(HW, WO + 0.6, -1.4, HH + 1.4, zF - 1.2, zF + 0.4, C.hull3);
    B.bx(-WO - 0.6, WO + 0.6, HH, HH + 1.4, zF - 1.2, zF + 0.4, C.hull3);
    B.bx(-WO - 0.6, WO + 0.6, -1.4, -1.0, zF - 1.2, zF + 0.4, C.hull3);
    E.bx(-HW - 0.12, -HW + 0.02, 0, HH, zF + 0.41, zF + 0.5, C.violet); E.bx(HW - 0.02, HW + 0.12, 0, HH, zF + 0.41, zF + 0.5, C.violet);
    E.bx(-HW, HW, HH - 0.14, HH, zF + 0.41, zF + 0.5, C.violet); E.bx(-HW, HW, 0, 0.14, zF + 0.41, zF + 0.5, C.cyan);
    // apron lip + landing guide beams
    B.bx(-HW - 0.4, HW + 0.4, -0.5, 0, zF + 0.4, zF + 8, C.hull2);
    E.bx(-HW, -HW + 0.18, 0.0, 0.05, zF + 0.4, zF + 8, C.cyan); E.bx(HW - 0.18, HW, 0.0, 0.05, zF + 0.4, zF + 8, C.cyan);
    E.bx(-0.08, 0.08, 0.0, 0.05, zF + 0.4, zF + 8, C.mag);
    // ceiling light panels, floor lane lines, pad rings
    for (let z = zR + 2; z < zF - 1; z += 4.2) { E.bx(-1.1, 1.1, HH - 0.06, HH, z, z + 1.6, C.white, 0.9); E.bx(-3.5, -2.4, HH - 0.06, HH, z, z + 1.0, C.cyan, 0.5); E.bx(2.4, 3.5, HH - 0.06, HH, z, z + 1.0, C.cyan, 0.5); }
    E.bx(-HW + 0.3, -HW + 0.38, 0.0, 0.04, zR + 0.5, zF - 0.3, C.violet, 0.8); E.bx(HW - 0.38, HW - 0.3, 0.0, 0.04, zR + 0.5, zF - 0.3, C.violet, 0.8);
    E.bx(-0.06, 0.06, 0.0, 0.04, zR + 0.5, zF - 0.3, C.mag, 0.6);
    for (let z = zR + 1; z < zF; z += 2.2) { E.bx(-HW + 0.05, -HW + 0.16, 0.4, 0.5, z, z + 0.9, C.cyan, 0.5); E.bx(HW - 0.16, HW - 0.05, 0.4, 0.5, z, z + 0.9, C.cyan, 0.5); }
    // pads
    padLocal = [new T.Vector3(-1.7, 0.25, zF - 8), new T.Vector3(1.7, 0.25, zF - 8), new T.Vector3(-1.7, 0.25, zF - 16), new T.Vector3(1.7, 0.25, zF - 16)];
    padLocal.forEach((p) => {
      B.shape(new T.CylinderGeometry(1.15, 1.25, 0.12, 18), C.hull2, new T.Matrix4().makeTranslation(p.x, 0.06, p.z));
      const tm = new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(p.x, 0.14, p.z));
      E.shape(new T.TorusGeometry(1.0, 0.05, 5, 24), C.cyan, tm);
      E.bx(p.x - 0.5, p.x + 0.5, 0.12, 0.15, p.z - 0.03, p.z + 0.03, C.mag, 0.8); E.bx(p.x - 0.03, p.x + 0.03, 0.12, 0.15, p.z - 0.5, p.z + 0.5, C.mag, 0.8);
      for (let k = 0; k < 8; k++) { const a = k / 8 * 6.2832 + 0.39; lt(p.x + Math.cos(a) * 1.1, 0.22, p.z + Math.sin(a) * 1.1, 0.07, k % 2 ? C.violet : C.cyan, 4, k / 8, 0.5); }
    });
    // ── store zone (left wall), map pedestal (right wall), clerk ──
    const cz = zF - 11;
    // the INTERGALACTIC 7/11 (ship-world.js standalone store) fills the left wall here; see layoutNpcs()
    lay.ped.set(3.7, 0, cz); lay.board.set(HW - 0.12, 1.5, cz - 4);
    // TRADE terminal: amber screen on the right wall, 7 L aft of the mission board's neighbour
    lay.trade.set(HW - 0.12, 1.3, cz + 3.6);
    B.bx(HW - 0.1, HW - 0.04, 0.7, 1.9, cz + 3.6 - 0.7, cz + 3.6 + 0.7, C.hull3);
    E.bx(HW - 0.14, HW - 0.1, 0.8, 1.8, cz + 3.6 - 0.6, cz + 3.6 + 0.6, C.amber, 0.9);
    E.bx(HW - 0.15, HW - 0.1, 1.85, 1.9, cz + 3.6 - 0.7, cz + 3.6 + 0.7, C.cyan, 0.9);
    lt(HW - 0.3, 1.3, cz + 3.6, 0.1, C.amber, 4, 0.3, 0.6);
    // mission board: glowing screen on the right wall (data in st.interior.board / missions())
    B.bx(HW - 0.1, HW - 0.04, 0.9, 2.1, cz - 4 - 0.95, cz - 4 + 0.95, C.hull3);
    E.bx(HW - 0.14, HW - 0.1, 1.0, 2.0, cz - 4 - 0.85, cz - 4 + 0.85, C.cyan, 0.9);
    E.bx(HW - 0.15, HW - 0.1, 2.05, 2.1, cz - 4 - 0.95, cz - 4 + 0.95, C.mag, 0.9);
    B.shape(new T.CylinderGeometry(0.1, 0.13, 0.05, 10), C.hull2, new T.Matrix4().makeTranslation(3.7, 0.025, cz));
    B.shape(new T.CylinderGeometry(0.05, 0.08, 0.04, 10), C.violet, new T.Matrix4().makeTranslation(3.7, 0.07, cz), 0.8);
    E.shape(new T.TorusGeometry(0.08, 0.006, 4, 16), C.cyan, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(3.7, 0.12, cz)));
    E.shape(new T.OctahedronGeometry(0.035), C.mag, new T.Matrix4().makeTranslation(3.7, 0.17, cz));
    lt(3.7, 0.17, cz, 0.09, C.mag, 4, 0, 0.6);
    // ── shipyard alcove (back wall): two display pads + sign; bounties screen beside the mission board ──
    const yx = [-2.8, 2.8], yz = zR + 3.2;
    yx.forEach((px, i) => {
      B.shape(new T.CylinderGeometry(1.5, 1.6, 0.18, 20), C.hull2, new T.Matrix4().makeTranslation(px, 0.09, yz));
      E.shape(new T.TorusGeometry(1.35, 0.05, 5, 28), i ? C.violet : C.cyan, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(px, 0.2, yz)));
      B.bx(px - 1.5, px + 1.5, 0, 3.2, zR + 0.01, zR + 0.06, C.hull3);
      E.bx(px - 1.5, px + 1.5, 3.2, 3.26, zR + 0.06, zR + 0.1, C.cyan, 0.8);
      lt(px, 0.24, yz + 1.4, 0.09, i ? C.violet : C.cyan, 4, i * 0.5, 0.5);
      lay.yard[i].set(px, 1.6, yz); yardHulls[i].position.copy(lay.yard[i]);
    });
    yardSign.scale.set(5, 0.78, 1); yardSign.position.set(0, 4.4, zR + 0.12);
    const qz = cz - 8;
    B.bx(HW - 0.1, HW - 0.04, 0.5, 2.5, qz - 0.95, qz + 0.95, C.hull3); E.bx(HW - 0.14, HW - 0.1, 2.5, 2.56, qz - 0.95, qz + 0.95, C.mag, 0.9);
    questMesh.scale.set(1.8, 2.0, 1); questMesh.position.set(HW - 0.16, 1.5, qz); lay.quest.set(HW - 0.16, 1.5, qz);
    // back wall: bulkhead door + banners
    B.bx(-1.0, 1.0, 0, 1.8, zR + 0.01, zR + 0.1, C.hull3); E.bx(-0.9, -0.85, 0.02, 1.7, zR + 0.1, zR + 0.14, C.violet); E.bx(0.85, 0.9, 0.02, 1.7, zR + 0.1, zR + 0.14, C.violet); E.bx(-0.9, 0.9, 1.66, 1.7, zR + 0.1, zR + 0.14, C.violet);
    // ── exterior spine: sections, collars ──
    let z = zR - 1; const zE = -lenL / 2 + 16 * kw;
    const sections = [];
    while (z > zE + 6) {
      const l = Math.min(z - zE, (14 + rng() * 26) * k), w = (4.6 + rng() * 1.6) * kw, h = (6.0 + rng() * 2.2) * kw;
      sections.push({ z0: z - l, z1: z, w, h }); z -= l;
    }
    sections.forEach((s, i) => {
      const yc = cy + (i % 2 ? 0.1 : -0.1);
      B.bx(-s.w, s.w, yc - s.h / 2, yc + s.h / 2, s.z0, s.z1, i % 2 ? C.hull : C.hull2);
      B.bx(-s.w - 0.4 * kw, s.w + 0.4 * kw, yc - s.h / 2 - 0.4 * kw, yc + s.h / 2 + 0.4 * kw, s.z1 - 1.6 * kw, s.z1, C.hull3);        // collar
      E.bx(-s.w - 0.41 * kw, s.w + 0.41 * kw, yc + s.h / 2 - 0.25 * kw, yc + s.h / 2 - 0.12 * kw, s.z1 - 1.5 * kw, s.z1 - 0.1, C.violet, 0.8);
      // top hull plates
      B.bx(-s.w * 0.7, s.w * 0.7, yc + s.h / 2, yc + s.h / 2 + 0.35 * kw, s.z0 + 1, s.z1 - 2.5 * kw, C.panel);
      // side window rows
      for (let zz = s.z0 + 1.5 * kw; zz < s.z1 - 2.5 * kw; zz += 2.4 * kw) {
        if (rng() < 0.2) continue;
        const col = rng() < 0.8 ? C.amber : C.cyan;
        E.bx(s.w, s.w + 0.07 * kw, yc - 0.2 * kw, yc + 0.2 * kw, zz, zz + 1.1 * kw, col, 0.8); E.bx(-s.w - 0.07 * kw, -s.w, yc - 0.2 * kw, yc + 0.2 * kw, zz, zz + 1.1 * kw, col, 0.8);
      }
      // accent edge strips
      E.bx(s.w - 0.05, s.w + 0.05 * kw, yc - s.h / 2 + 0.3 * kw, yc - s.h / 2 + 0.42 * kw, s.z0 + 0.5, s.z1 - 2 * kw, C.violet, 0.7); E.bx(-s.w - 0.05 * kw, -s.w + 0.05, yc - s.h / 2 + 0.3 * kw, yc - s.h / 2 + 0.42 * kw, s.z0 + 0.5, s.z1 - 2 * kw, C.violet, 0.7);
    });
    // hab pods stuck to the sides + sensor domes
    for (let i = 0; i < 12; i++) {
      const sd = i % 2 ? 1 : -1, zz = zR - 12 - rng() * (zR - zE - 30), w = (2 + rng() * 2.5) * kw, h = (1.6 + rng() * 2.2) * kw, d = (4 + rng() * 6) * kw, xo = 5.4 * kw + 1.2 * kw + w / 2;
      B.cbox(w, h, d, sd * xo, cy + (rng() - 0.5) * 3 * kw, zz, C.panel); E.bx(sd * (xo + w / 2) - 0.02, sd * (xo + w / 2) + 0.04 * sd * kw, cy - 0.2 * kw, cy + 0.2 * kw, zz - d / 2 + 0.7 * kw, zz + d / 2 - 0.7 * kw, C.amber, 0.7);
    }
    // keel truss
    B.bx(-1.2 * kw, 1.2 * kw, cy - 5 * kw, cy - 3 * kw, zE, zR - 2, C.hull2);
    for (let zz = zE + 4; zz < zR - 4; zz += 12 * k) { B.bx(-3.5 * kw, 3.5 * kw, cy - 5 * kw, cy - 4.5 * kw, zz, zz + 0.8 * kw, C.hull3); B.cbox(0.35 * kw, 2 * kw, 0.35 * kw, 0, cy - 4 * kw, zz + 0.4, C.hull3, 1, 0, 0, 0); }
    // dorsal masts, dishes, antennas with red beacons
    for (let i = 0; i < 8; i++) {
      const zz = zR - 8 - i * (zR - zE - 24) / 7, hgt = (7 + rng() * 14) * k, xx = (rng() - 0.5) * 5 * kw, th = 0.16 * kw, my = cy + 3.5 * kw;
      B.bx(xx - th, xx + th, my, my + hgt, zz - th, zz + th, C.hull);
      B.bx(xx - 1.1 * kw, xx + 1.1 * kw, my + hgt * 0.6, my + hgt * 0.6 + 0.12 * kw, zz - 0.1 * kw, zz + 0.1 * kw, C.hull);
      if (i % 3 === 0) B.shape(new T.ConeGeometry(2.0 * kw, 0.9 * kw, 12, 1, true), C.panel, new T.Matrix4().makeRotationX(Math.PI * 0.7).premultiply(new T.Matrix4().makeTranslation(xx, my + hgt + 0.3, zz)));
      lt(xx, my + hgt + 0.2, zz, 0.45 * k, 0xff3d3d, 1, rng(), 0.6 + rng() * 0.3);
    }
    // solar wings
    const swz = -lenL * 0.3, ks = Math.max(1, k * 0.8);
    for (const sd of [-1, 1]) {
      B.bx(sd * 5.5 * kw - 0.3 * kw, sd * 5.5 * kw + 0.3 * kw + sd * 3 * ks, cy - 0.3 * kw, cy + 0.3 * kw, swz - 0.3 * kw, swz + 0.3 * kw, C.hull3);
      for (let j = 0; j < 3; j++) {
        const x0 = sd * ks * (9 + j * 11), x1 = sd * ks * (9 + j * 11 + 10.2), xa = Math.min(x0, x1), xb = Math.max(x0, x1), pz = 7.5 * ks, th = 0.14 * kw;
        B.bx(xa, xb, cy - th, cy + th, swz - pz, swz + pz, 0x1d2a55);
        for (let g = 1; g < 4; g++) E.bx(xa, xb, cy + th, cy + th * 1.2, swz - pz + g * pz / 2 - 0.06 * kw, swz - pz + g * pz / 2 + 0.06 * kw, C.cyan, 0.5);
        B.bx(xa - 0.1 * kw, xa + 0.1 * kw, cy - 0.18 * kw, cy + 0.18 * kw, swz - 0.15 * kw, swz + 0.15 * kw, C.hull3);
      }
      lt(sd * ks * 42, cy, swz, 0.5 * k, sd < 0 ? 0xff3d3d : 0x3dff7a, 3, 0, 1);
    }
    // docking booms by the hangar with beacons
    for (const sd of [-1, 1]) {
      B.bx(sd * (WO + 0.5) - 0.3, sd * (WO + 0.5) + 0.3 + sd * 5, 2.0, 2.5, zF - 6, zF - 2, C.hull3);
      B.cbox(2.4, 2.2, 6, sd * (WO + 6.5), 2.3, zF - 4, C.hull2); E.bx(sd * (WO + 6.5) - 1.2 * sd - 0.02, sd * (WO + 6.5) - 1.2 * sd + 0.02, 1.8, 2.2, zF - 6.5, zF - 1.5, C.cyan, 0.8);
      lt(sd * (WO + 6.5), 3.6, zF - 1, 0.5, C.mag, 1, sd > 0 ? 0 : 0.5, 1.1);
    }
    // engine block + nozzles
    const ez0 = -lenL / 2, ez1 = ez0 + 16 * kw;
    B.bx(-7 * kw, 7 * kw, cy - 5.6 * kw, cy + 5.6 * kw, ez0 + 3, ez1, C.hull2); B.bx(-7.4 * kw, 7.4 * kw, cy - 6 * kw, cy + 6 * kw, ez1 - 2, ez1, C.hull3);
    for (let i = 0; i < 4; i++) {
      const nx = (i % 2 ? 1 : -1) * 3.2 * kw, ny = cy + (i < 2 ? 3.2 : -3.2) * kw;
      const rm = new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(nx, ny, ez0 + 1.5));
      B.shape(new T.CylinderGeometry(2.2 * kw, 1.6 * kw, 3.4 * kw, 14, 1, true), C.hull3, rm);
      E.shape(new T.CircleGeometry(1.5 * kw, 14), 0xB07CFF, new T.Matrix4().makeTranslation(nx, ny, ez0 - 0.1));
      lt(nx, ny, ez0 - 0.6, 3.4 * kw, 0x9a6cff, 4, i * 0.25, 0.35);
    }
    // top-edge nav strobes along the hangar front, mouth corner beacons
    [[-WO - 0.6, HH + 1.4], [WO + 0.6, HH + 1.4], [-WO - 0.6, -1.4], [WO + 0.6, -1.4]].forEach((p, i) => lt(p[0], p[1], zF + 0.5, 0.5, i < 2 ? C.cyan : C.mag, 1, i * 0.23, 0.9));
    // mouth frame lights + runway chase lights (4 rails to ~64 L out)
    const rails = [[-HW + 0.3, 0.2], [HW - 0.3, 0.2], [-HW + 0.3, HH - 0.2], [HW - 0.3, HH - 0.2]], N = 22;
    rails.forEach((rl, j) => { for (let i = 0; i < N; i++) lt(rl[0], rl[1], zF + 9 + i * 2.6, 0.2 + 0.12 * (i === 0), j < 2 ? C.cyan : C.violet, 2, i / N, 0.9); });
    for (let i = 0; i < 18; i++) { const u = i / 17; lt(-HW + u * 2 * HW, 0.1, zF + 0.6, 0.12, C.white, 0, 0, 0); lt(-HW + u * 2 * HW, HH - 0.1, zF + 0.6, 0.12, C.violet, 0, 0, 0); }
    for (let i = 0; i < 8; i++) { lt(-HW + 0.1, 0.6 + i * 0.5, zF + 0.6, 0.1, C.violet, 0, 0, 0); lt(HW - 0.1, 0.6 + i * 0.5, zF + 0.6, 0.1, C.violet, 0, 0, 0); }
    // ── ring habitat (spins) ──
    const Rr = Math.max(24, lenL * 0.1), rz = -lenL * 0.08, rr = Math.max(2.7, Rr * 0.09), rk = rr / 2.7;
    RB.shape(new T.TorusGeometry(Rr, rr, 10, 56), C.hull, new T.Matrix4().makeTranslation(0, 0, 0));
    RB.shape(new T.TorusGeometry(Rr, rr + 0.5, 4, 56), C.hull3, new T.Matrix4().makeScale(1, 1, 0.35));
    for (let i = 0; i < 6; i++) { const a = i / 6 * 6.2832; RB.cbox(1.1 * rk, Rr - 4.5 * rk, 1.1 * rk, Math.cos(a) * (Rr / 2 + 2) , Math.sin(a) * (Rr / 2 + 2), 0, C.hull3, 1, 0, 0, a - Math.PI / 2); }
    for (let i = 0; i < 64; i++) {
      if (rng() < 0.15) continue; const a = i / 64 * 6.2832, col = rng() < 0.8 ? C.amber : C.cyan;
      RE.cbox(1.5 * rk, 0.28 * rk, 0.5 * rk, Math.cos(a) * (Rr + rr * 0.92), Math.sin(a) * (Rr + rr * 0.92), 0, col, 0.85, 0, 0, a + Math.PI / 2);
    }
    for (let i = 0; i < 4; i++) { const a = i / 4 * 6.2832 + 0.4; RE.cbox(1.2 * rk, 0.2 * rk, 0.9 * rk, Math.cos(a) * (Rr - rr - 0.05), Math.sin(a) * (Rr - rr - 0.05), 0, C.violet, 0.9, 0, 0, a + Math.PI / 2); }
    ring.position.set(0, cy, rz);
    // spine collar where the ring spokes meet the hull
    B.bx(-6.5 * kw, 6.5 * kw, cy - 4.2 * kw, cy + 4.2 * kw, rz - 2 * rk, rz + 2 * rk, C.hull3);
    // ── signs: the 7/11 mast over the mouth, interior windows ──
    const Ws = Math.max(24, Math.min(420, lenL * 0.12)), Hs = Ws / 4, sk = Math.max(1, Ws / 60), sy = HH + 1.4 + 3 + Hs / 2, sz = zF - 2.0;
    B.bx(-Ws / 2 - 1, Ws / 2 + 1, HH + 1.0, HH + 1.0 + 0.8 * sk, sz - 3 * sk, sz + 1, C.hull3);                         // crossbar deck
    for (const sd of [-1, 1]) { B.bx(sd * (Ws / 2 - 2 * sk) - 0.5 * sk, sd * (Ws / 2 - 2 * sk) + 0.5 * sk, HH + 1.0, sy + Hs / 2 + 0.6 * sk, sz - 1.2 * sk, sz - 0.2, C.hull3); B.bx(sd * 6 * sk - 0.4 * sk, sd * 6 * sk + 0.4 * sk, HH + 1.0, sy, sz - 1.2 * sk, sz - 0.2, C.hull3); }
    const fr = 0.8 * sk;
    B.bx(-Ws / 2 - fr, Ws / 2 + fr, sy - Hs / 2 - fr, sy - Hs / 2 - 0.1, sz - 1.4 * sk, sz - 0.1, C.hull3); B.bx(-Ws / 2 - fr, Ws / 2 + fr, sy + Hs / 2 + 0.1, sy + Hs / 2 + fr, sz - 1.4 * sk, sz - 0.1, C.hull3);
    B.bx(-Ws / 2 - fr, -Ws / 2 - 0.1, sy - Hs / 2, sy + Hs / 2, sz - 1.4 * sk, sz - 0.1, C.hull3); B.bx(Ws / 2 + 0.1, Ws / 2 + fr, sy - Hs / 2, sy + Hs / 2, sz - 1.4 * sk, sz - 0.1, C.hull3);
    E.bx(-Ws / 2 - fr, Ws / 2 + fr, sy + Hs / 2 + fr, sy + Hs / 2 + fr + 0.25 * sk, sz - 0.2, sz, C.mag); E.bx(-Ws / 2 - fr, Ws / 2 + fr, sy - Hs / 2 - fr - 0.25 * sk, sy - Hs / 2 - fr, sz - 0.2, sz, C.mag);
    // sign + windows share one geometry (canvas atlas: v>0.5 sign, v<0.5 window)
    const sp = [], su = [];
    const quad = (cx, cy2, cz2, rx, ry, rz2, ux, uy, uz, w, h, u0, u1, v0, v1) => {
      const P = (a, b) => [cx + rx * w * a + ux * h * b, cy2 + ry * w * a + uy * h * b, cz2 + rz2 * w * a + uz * h * b];
      const a = P(-.5, -.5), b = P(.5, -.5), c = P(.5, .5), d = P(-.5, .5);
      sp.push(...a, ...b, ...c, ...a, ...c, ...d); su.push(u0, v0, u1, v0, u1, v1, u0, v0, u1, v1, u0, v1);
    };
    quad(0, sy, sz, 1, 0, 0, 0, 1, 0, Ws, Hs, 0, 1, 0.5, 1);
    st.interior.windows.length = 0;
    const win = (x, y, z, nx, w, h, u0, u1) => {                     // window on a side wall; nx = +1 faces +x (left wall)
      if (nx !== 0) quad(x, y, z, 0, 0, -nx, 0, 1, 0, w, h, u0, u1, 0, 0.5); else quad(x, y, z, 1, 0, 0, 0, 1, 0, w, h, u0, u1, 0, 0.5);
      st.interior.windows.push({ pos: new T.Vector3(x, y, z), w, h, nx });
    };
    win(-HW + 0.01, 3.0, zF - 21, 1, 5, 2.2, 0.05, 0.6); win(-HW + 0.01, 3.0, zF - 3.5, 1, 4, 2.2, 0.4, 0.95);
    win(HW - 0.01, 3.0, zF - 21, -1, 5, 2.2, 0.4, 0.95); win(HW - 0.01, 3.0, zF - 13, -1, 6, 2.2, 0.05, 0.6); win(HW - 0.01, 3.0, zF - 3.5, -1, 4, 2.2, 0.2, 0.75);
    win(0, 3.3, zR + 0.02, 0, 7, 1.6, 0.1, 0.9);
    // window frames (emissive)
    st.interior.windows.forEach((w) => {
      const t = 0.07;
      if (w.nx !== 0) { const x0 = w.pos.x - (w.nx > 0 ? 0 : 0.08), x1 = x0 + 0.08; E.bx(x0, x1, w.pos.y + w.h / 2, w.pos.y + w.h / 2 + t, w.pos.z - w.w / 2 - t, w.pos.z + w.w / 2 + t, C.violet, 0.8); E.bx(x0, x1, w.pos.y - w.h / 2 - t, w.pos.y - w.h / 2, w.pos.z - w.w / 2 - t, w.pos.z + w.w / 2 + t, C.violet, 0.8); }
      else { E.bx(-w.w / 2 - t, w.w / 2 + t, w.pos.y + w.h / 2, w.pos.y + w.h / 2 + t, zR + 0.02, zR + 0.1, C.violet, 0.8); E.bx(-w.w / 2 - t, w.w / 2 + t, w.pos.y - w.h / 2 - t, w.pos.y - w.h / 2, zR + 0.02, zR + 0.1, C.violet, 0.8); }
    });
    const sg = new T.BufferGeometry(); sg.setAttribute('position', new T.Float32BufferAttribute(sp, 3)); sg.setAttribute('uv', new T.Float32BufferAttribute(su, 2)); sg.computeBoundingSphere();
    // sign backlight halo lights
    for (let i = 0; i < 9; i++) lt(-Ws / 2 + i * Ws / 8, sy + Hs / 2 + fr + 0.5, sz + 0.2, 0.55 * sk, C.mag, 0, 0, 0);

    // ── assign geometry ──
    const set = (mesh, soup) => { if (mesh.geometry) mesh.geometry.dispose(); mesh.geometry = soup.geometry(); };
    set(hullMesh, B); set(emiMesh, E); set(ringMesh, RB); set(ringEmi, RE);
    if (signMesh.geometry) signMesh.geometry.dispose(); signMesh.geometry = sg;
    fieldMesh.scale.set(2 * MOUTH_HW, 2 * MOUTH_HH, 1); fieldMesh.position.set(0, MOUTH_HH, zF + 0.2);
    // instanced lights
    if (lightMesh) { group.remove(lightMesh); lightMesh.geometry.dispose(); lightMesh.dispose(); }
    lightMesh = new T.InstancedMesh(quadGeo.clone(), matLight, lights.length); lightMesh.name = 'station-lights'; lightMesh.frustumCulled = false;
    const m4 = new T.Matrix4(), qq = new T.Quaternion(), sc = new T.Vector3(), pp = new T.Vector3();
    lights.forEach((l, i) => { pp.set(l.x, l.y, l.z); sc.set(l.size, l.size, l.size); m4.compose(pp, qq, sc); lightMesh.setMatrixAt(i, m4); lightMesh.setColorAt(i, l.c); });
    lightMesh.instanceMatrix.needsUpdate = true; lightCol = lightMesh.instanceColor.array; group.add(lightMesh);
    st.stats.tris = Math.round((B.tris() + E.tris() + RB.tris() + RE.tris()) + sp.length / 9 + 2 + lights.length * 2);
    st.stats.meshes = 7;

    // ── data layout ──
    st.mouth.halfW = MOUTH_HW * L; st.mouth.halfH = MOUTH_HH * L;
    st.deck.walls.length = 0;
    const W = (name, x0, x1, y0, y1, z0, z1, field) => st.deck.walls.push({ name, min: new T.Vector3(x0, y0, z0), max: new T.Vector3(x1, y1, z1), field: !!field });
    W('wallL', -WO, -HW, -1, HH + 1, zR - 1, zF); W('wallR', HW, WO, -1, HH + 1, zR - 1, zF); W('back', -HW, HW, -1, HH + 1, zR - 1.5, zR);
    W('pedestal', 3.58, 3.82, 0, 0.1, cz - 0.12, cz + 0.12);
    W('mouthField', -HW, HW, 0, HH + 1, zF - 0.05, zF + 0.3, true);
    storeWalls = [];
    st.deck.bounds.min.set(-HW, 0, zR); st.deck.bounds.max.set(HW, HH, zF);
    st.deck.walls.forEach((w) => { /* stored in L units */ });
    st.pads.length = 0; padLocal.forEach((p) => st.pads.push({ pos: new T.Vector3(), quat: new T.Quaternion(), free: true, local: p.clone() }));
    st.interior.mapPedestal.radius = 0.45 * L;
    layoutNpcs();
    group.updateMatrixWorld(true); syncWorld();
  }

  // ── NPCs / clerk ──
  let humans = [];
  function makeHuman(color) {
    try { const h = createHuman(T, { color }); h.group.traverse((o) => { o.frustumCulled = false; }); h.group.scale.setScalar(HUMAN_H); group.add(h.group); return h; } catch (e) { return null; }
  }
  function placeHuman(h, x, z, faceX, faceZ) { if (!h) return; h.group.position.set(x, 0.02, z); h.group.rotation.y = Math.atan2(-faceX, -faceZ); }
  function layoutNpcs() {
    const cz = zF - 11;
    if (!ss) {
      ss = createStandaloneStore(T, { id: 'station-711', lex: 0x7117, shoppers: 6, dealers: 2, pads: false });
      group.add(ss.group);
    }
    // place the store against the left wall, glass front facing +x
    ss.group.scale.setScalar(STORE_S); ss.group.rotation.set(0, Math.PI / 2, 0);
    ss.group.position.set(-HW + 13.8 * STORE_S, 0, cz); ss.group.updateMatrix(); ss.group.updateMatrixWorld(true);
    ss.interior.setMatrix(ss.group.matrix, STORE_S);
    ss.crowd.frame = ss.group.matrixWorld;                       // NPC .pos come out in WORLD space
    // push the store's walls / shelves into the deck colliders (station-local L, axis aligned: the yaw is 90 degrees)
    storeWalls = transformBoxes(T, ss.interior.walls.concat(ss.interior.aisles), ss.group.matrix);
    storeWalls.forEach((w) => st.deck.walls.push({ name: 'store-' + w.name, min: w.min, max: w.max, field: false }));
    if (!st.interior.stores.length) {
      const id = 'station-711';
      const base = weaponSetFor(id, 4), CL = ['C', 'B', 'A', 'S'];
      const wlist = []; const names = new Set();
      base.forEach((w, k) => {
        let tier = Math.min(3, Math.max(0, CL.indexOf(w.cls)) + 1), g = null, guard = 0;
        do { g = generateWeapon(id + '#' + k + '#' + guard, tier); guard++; } while (names.has(g.name) && guard < 12);
        names.add(g.name); wlist.push(g);
      });
      const shield = [1, 2, 3].map((tr) => ({ id: 'shield' + tr, tier: tr, name: 'Shield Cell ' + ['I', 'II', 'III'][tr - 1], maxShield: 20, price: [150, 350, 700][tr - 1] }));
      const engineU = [1, 2, 3].map((tr) => ({ id: 'engine' + tr, tier: tr, name: 'Drive Tune ' + ['I', 'II', 'III'][tr - 1], speedMul: 0.1, price: [200, 450, 900][tr - 1] }));
      const store = {
        id, name: STORE_NAME + ' Orbital', pos: new T.Vector3(), npcSpot: { pos: new T.Vector3(), facing: new T.Vector3(1, 0, 0) }, counter: { pos: new T.Vector3(), radius: 4.5 * STORE_S * L },
        inventory: { weapons: wlist, upgrades: { shield, engine: engineU }, snack: { id: 'snack', name: 'Event Horizon Dog', price: 50, heal: 'full' } }, lineIdx: 0, interior: ss.interior,
      };
      fillStoreRecord(store, ss.crowd, id);
      store.localPos = new T.Vector3(); store.localCounter = new T.Vector3();
      st.interior.stores.push(store);
      const lexN = { seed: 0x7117, role: 'shopper', known: ss.crowd.known };
      NPC_DEF.forEach((d) => {
        const h = makeHuman(d.color); if (h) humans.push(h);
        const npc = { seed: 0x7117, role: 'shopper', known: ss.crowd.known, name: d.name };
        st.interior.npcs.push({ human: h, name: d.name, lines: [lingo.line(npc, 'shopper'), lingo.line(npc, 'gossip'), lingo.line(npc, 'warning')], say: (ctx) => lingo.line(npc, ctx || 'shopper'), npc, pos: new T.Vector3(), localPos: new T.Vector3() });
      });
    }
    const s = st.interior.stores[0];
    ss.interior.toParent(_v.set(6.5, 0, 6.4), s.localPos); s.localPos.y = 0;
    ss.interior.toParent(_v.set(6.5, 0, 10.9), s.localCounter); s.localCounter.y = 0;
    const spots = [[3.9, zF - 4.5, -1, 0.3], [-1.2, zF - 18, 1, 0.2], [0.9, zR + 3.5, 0, 1]];
    st.interior.npcs.forEach((n, i) => { n.localPos.set(spots[i][0], 0, spots[i][1]); placeHuman(n.human, spots[i][0], spots[i][1], spots[i][2], spots[i][3]); });
    lay.ped.set(3.2, 0, cz);
  }

  // push local layout to world-space live objects
  function syncWorld() {
    const gw = group.matrixWorld;
    toW(0, MOUTH_HH, zF, st.mouth.pos); st.mouth.dir.set(0, 0, 1).applyQuaternion(group.quaternion).normalize();
    st.pads.forEach((p) => { p.pos.copy(p.local).applyMatrix4(gw); p.quat.copy(group.quaternion); });
    const s = st.interior.stores[0];
    if (s) {
      toW(s.localCounter.x, 0, s.localCounter.z, s.counter.pos); toW(s.localPos.x, 0, s.localPos.z, s.npcSpot.pos); s.pos.copy(s.counter.pos);
      s.npcSpot.facing.set(1, 0, 0).applyQuaternion(group.quaternion); s.counter.radius = 4.5 * STORE_S * L;
    }
    st.interior.npcs.forEach((n) => toW(n.localPos.x, 0, n.localPos.z, n.pos));
    toW(lay.ped.x, 0, lay.ped.z, st.interior.mapPedestal.pos); st.interior.mapPedestal.radius = 0.45 * L;
    toW(lay.board.x, lay.board.y, lay.board.z, st.interior.board.pos); st.interior.board.radius = 0.6 * L;
    toW(lay.trade.x, lay.trade.y, lay.trade.z, st.interior.trade.pos); st.interior.trade.radius = 0.8 * L;
    toW(0, 0, lay.yard[0].z, st.interior.shipyard.pos); st.interior.shipyard.radius = 6 * L;
    toW(lay.quest.x, lay.quest.y, lay.quest.z, st.interior.questBoard.pos); st.interior.questBoard.radius = 0.8 * L;
  }
  st.interior.trade.refresh = (day) => {
    const tr = st.interior.trade; tr.day = day == null ? Math.floor(Date.now() / 864e5) : day;
    const m = makeMarket('station:' + tr.day, { buyMul: 1.0, sellMul: 1.15, nSell: 6, nBuy: 5 });
    tr.buys = m.buys; tr.buyPrices = m.buyPrices; tr.sells = m.sells;
  };
  st.interior.trade.refresh();
  st.interior.questBoard.setQuests = (list) => {
    const qb = st.interior.questBoard; qb.quests = (list || []).slice(0, 5);
    const old = qTex; qTex = new T.CanvasTexture(questCanvas(qb.quests)); qTex.colorSpace = T.SRGBColorSpace; qMat.map = qTex; qMat.needsUpdate = true; old.dispose();
  };
  st.interior.greetingLine = (seed) => {
    const r = mul(typeof seed === 'number' ? seed : Array.from(String(seed == null ? Math.floor(Date.now() / 864e5) : seed)).reduce((h, c) => (Math.imul(h, 31) + c.charCodeAt(0)) | 0, 11));
    return PA_LINES[Math.floor(r() * PA_LINES.length)];
  };

  // ── missions (rev 25 E): seeded, deterministic per seed; planetId is a real node id from engine.drawOrder ──
  st.interior.missions = (seed, n) => {
    n = n == null ? 3 : n; const r = mul(typeof seed === 'number' ? seed : (Array.from(String(seed)).reduce((h, c) => (Math.imul(h, 31) + c.charCodeAt(0)) | 0, 7)));
    const nodes = (engine.drawOrder || []).filter((q) => q && q.id != null), kinds = ['mine', 'scout', 'trade', 'bounty'];
    const ITEMS = ['fuel-cell', 'plasma-coil', 'hull-plate', 'flux-capacitor', 'logic-chip'], out = [];
    for (let i = 0; i < n; i++) {
      const kind = kinds[Math.floor(r() * 4)], node = nodes.length ? nodes[Math.floor(r() * nodes.length)] : null;
      const pid = node ? node.id : 'unknown', pname = node ? (node.name || node.title || node.id) : 'the void', minutes = 3 + Math.floor(r() * 18);
      const T2 = { mine: ['Mine ore on ', 'ore'], scout: ['Scout the belt of ', 'belt survey'], trade: ['Trade run to ', 'cargo'], bounty: ['Bounty hunt over ', 'wanted ship'] }[kind];
      out.push({ id: 'm' + i + '-' + Math.floor(r() * 1e6).toString(36), kind, title: T2[0] + pname, planetId: pid, target: T2[1], minutes,
        reward: { gor: Math.round((40 + minutes * (8 + r() * 10)) / 5) * 5, stacks: [{ item: ITEMS[Math.floor(r() * ITEMS.length)], n: 1 + Math.floor(r() * 4) }], itemChance: Math.round((0.1 + r() * 0.35) * 100) / 100 } });
    }
    return out;
  };

  // ── API ──
  st.setVisible = (b) => { group.visible = !!b; };
  st.deck.floorAt = (wp, out) => {
    const o = out || { point: new T.Vector3(), normal: new T.Vector3(), height: 0, inside: false };
    const l = st.toLocal(wp, _v);
    o.inside = Math.abs(l.x) <= HW && l.z >= zR && l.z <= zF;
    o.height = l.y * L;
    toW(l.x, 0, l.z, o.point); o.normal.set(0, 1, 0).applyQuaternion(group.quaternion);
    return o;
  };
  st.mouth.trigger = (sp) => {
    if (!sp) return false;
    const l = st.toLocal(sp, _v2);
    return Math.abs(l.x) < MOUTH_HW && Math.abs(l.y - MOUTH_HH) < MOUTH_HH && l.z > zF - 4 && l.z < zF + 16;
  };
  const padPts = (i, dock) => {
    const p = padLocal[Math.max(0, Math.min(3, i | 0))] || padLocal[0], px = p.x, pz = p.z, c = MOUTH_HH;
    const pts = dock
      ? [[0, c, zF + 60], [0, c, zF + 36], [0, c, zF + 16], [0, c, zF + 4], [0, c - 0.1, zF - 3], [px * 0.5, c - 0.4, pz + 7], [px, 1.5, pz + 3], [px, 0.7, pz + 0.3], [px, p.y, pz]]
      : [[px, p.y, pz], [px, 0.7, pz + 0.1], [px, 1.5, pz + 2.5], [px * 0.5, c - 0.4, pz + 7], [0, c - 0.1, zF - 3], [0, c, zF + 4], [0, c, zF + 24], [0, c, zF + 60], [0, c, zF + 120]];
    const curve = new T.CatmullRomCurve3(pts.map((q) => new T.Vector3(q[0], q[1], q[2])), false, 'centripetal');
    return curve.getSpacedPoints(dock ? 56 : 64).map((q) => q.applyMatrix4(group.matrixWorld));
  };
  st.dockPath = (i) => { group.updateMatrixWorld(true); return padPts(i, true); };
  st.launchPath = (i) => { group.updateMatrixWorld(true); return padPts(i, false); };

  // refR (pilot-scaled, read fresh)
  function readRefR() {
    const k = root && root.kids && root.kids[0]; let r = 0;
    try { r = k ? engine.renderedRadius(k) : 0; } catch (e) { r = 0; }
    return r > 0 && isFinite(r) ? r : 320 * L;
  }
  let refR = 0;
  function targetLenL() { refR = readRefR(); return Math.max(150, 0.8 * refR / L); }
  st.rescale = () => {
    const n = targetLenL();
    if (!lenL || Math.abs(n - lenL) / lenL > 0.02) { build(n); }
    return st.length;
  };

  // orbit
  let theta = (opts.phase != null ? opts.phase : 0.9), lastT = null, yaw0 = 0;
  const _c = new T.Vector3(), _prev = new T.Vector3(), _qa = new T.Quaternion(), _tan = new T.Vector3(), _col = new T.Color();
  const IDLE = { moving: false, running: false, airborne: false, speed: 0, facing: 0 };
  const PERIOD = 1500;
  function bh() {
    const b = engine.blackHole;
    if (b && b.pos && b.coreR > 0) return { c: _c.copy(b.pos), core: b.coreR };
    return { c: _c.set(0, 0, 0), core: 0.25 * (refR || readRefR()) };
  }
  st.update = (t, dt, shipPos) => {
    uTime.value = t;
    if (!lenL) st.rescale();
    const b = bh(), r = 2.2 * b.core, w = Math.PI * 2 / PERIOD;
    if (lastT !== null && t < lastT) lastT = t;
    theta = (opts.phase != null ? opts.phase : 0.9) + t * w; lastT = t;
    _prev.copy(group.position);
    const s = Math.sin(theta), c = Math.cos(theta);
    group.position.set(b.c.x + c * r, b.c.y, b.c.z + s * r);
    // spine along the prograde tangent; +Z (the mouth) leads, with a slow sway
    _tan.set(-s, 0, c);
    const yaw = Math.atan2(_tan.x, _tan.z) + 0.3 * Math.sin(t * 0.04);
    group.quaternion.setFromAxisAngle(_up, yaw);
    ring.rotation.z = t * 0.12;
    group.updateMatrixWorld(true);
    if (dt > 0) st.vel.copy(group.position).sub(_prev).divideScalar(dt);
    if (st.vel.length() > 1e6) st.vel.set(0, 0, 0);
    syncWorld();
    if (!group.visible) return;
    // pixel scale for the billboard lights
    try { const sz = engine.renderer.getSize(_v); uH.value = Math.max(200, sz.y * (engine.renderer.getPixelRatio ? engine.renderer.getPixelRatio() : 1)); uPx.value = 6.5 * (engine.renderer.getPixelRatio ? engine.renderer.getPixelRatio() : 1); } catch (e) { /* defaults */ }
    // lights
    const a = lightCol;
    for (let i = 0; i < lightList.length; i++) {
      const l = lightList[i]; let k = 1;
      if (l.mode === 1) { const f = (t * l.sp + l.ph) % 1; k = f < 0.1 ? 1 : 0.06; }
      else if (l.mode === 2) { const pp = (t * l.sp) % 1, d = ((pp - (1 - l.ph)) % 1 + 1) % 1; k = 0.12 + 0.88 * Math.exp(-d * 9); }
      else if (l.mode === 3) { const f = (t * l.sp + l.ph) % 1.4; k = (f < 0.06 || (f > 0.16 && f < 0.22)) ? 1 : 0.05; }
      else if (l.mode === 4) { k = 0.55 + 0.45 * Math.sin(t * l.sp * 6.28 + l.ph * 6.28); }
      a[i * 3] = l.c.r * k; a[i * 3 + 1] = l.c.g * k; a[i * 3 + 2] = l.c.b * k;
    }
    lightMesh.instanceColor.needsUpdate = true;
    // humans idle when the ship is anywhere near
    const nearShip = !shipPos || shipPos.distanceTo(group.position) < st.length * 0.5 + 150 * L;
    if (nearShip) humans.forEach((h) => { try { h.update(dt || 0.016, IDLE); } catch (e) { /* ignore */ } });
    if (ss) ss.update(t, dt || 0.016, nearShip);
    yardHulls.forEach((g, i) => { g.rotation.y = t * 0.35 + i * 2; });
  };
  st.dispose = () => {
    if (group.parent) group.parent.remove(group);
    [hullMesh, emiMesh, ringMesh, ringEmi, signMesh].forEach((m) => m.geometry && m.geometry.dispose());
    if (lightMesh) { lightMesh.geometry.dispose(); lightMesh.dispose(); }
    quadGeo.dispose(); matLit.dispose(); matEmi.dispose(); matLight.dispose(); matSign.dispose(); matField.dispose(); tex.dispose();
    yardTex.dispose(); qTex.dispose(); qMat.dispose(); yardSign.material.dispose(); yardHulls.forEach((g) => g.traverse((o) => { if (o.geometry) o.geometry.dispose(); }));
    humans.forEach((h) => { try { h.dispose(); } catch (e) { /* ignore */ } }); humans = [];
    if (ss) { ss.dispose(); ss = null; }
  };

  build(targetLenL());
  st.update(0, 0, null);
  return st;
}
