// ship-interior.js — the inside of your ship (docs/ship-mode.md rev 25 C/D/E): a freighter-scale base.
//
//   createInterior(engine, L, opts) -> interior         (opts.scale = extra size multiplier, opts.pocket = Vector3)
//   Model units: 1 unit = 1 L (the group is scaled by U = L * opts.scale). Hall: x +-10, y 0..8, z -30 (aft, hatch) .. +30 (cockpit).
//   Frame: +Z = forward (cockpit window), +Y up, floor y = 0 (cockpit platform y = 0.8 via a ramp at z 21..23.5).
//   The group lives in a pocket at (0, 60000, 0) (floating origin does the rest: only transforms are used); interior.pocket = group.position.
//
//   interior.group / setVisible(b)           in engine.scene, hidden until shown
//   interior.pocket                          Vector3 (live, = group.position)
//   interior.unit                            world units per model unit (U)
//   interior.rooms                           [{name,min,max}]  local L units: aft (hatch/table/kitchen), cargo, quarters (pens+pods), cockpit
//   interior.deck.floorAt(worldPos,out?)     -> { point(world), normal(world), height(world), inside }
//   interior.deck.walls                      [{name,min,max}] AABBs, LOCAL L units (same as station.deck.walls); cargo crates move with setStacks
//   interior.hatch / spawn / table / kitchen / board     { pos(world), radius(world) }; spawn also { dir(world, looks down the hall), localPos }
//   interior.pens[4] {pos,radius,pet,setPet(item),clear()}    item = { seed, name, tier? }  (generateEnemy tier 1 model, scaled)
//   interior.pods[4] {pos,radius,crew,setCrew(member),clear()} member = { seed, name, color }
//   interior.board.setMissions([{title,status,progress?|have+target?}])   redraws the glowing screen; active/partial missions get a progress bar
//   interior.trophies {pos,radius,setDiscoveries([{kind,name,by}])}      OBSERVATION room (left wall alcove, wide window): <=24 framed plaques, one atlas texture
//   interior.kitchen.setMenu(items)          <=6 item boxes on the counter, item.color or hashed colour
//   pens[i].setPet(item): item = {seed,name,plan?} -> createPet (walker|jelly|strider); idles in the pen, hops to the player within 2 L
//   pods[i].setCrew(member): member = {seed,name,color,role?} -> createHuman with role; idles, looks at the player when near
//   interior.cargo.setStacks(stacks)         stacks = [{id|name,count,color?}] -> up to 24 instanced crates sized by count
//   interior.windows                         [{pos,w,h}] world; the cockpit opening is a real hole, the live star sky shows through
//   interior.update(t, dt, playerWorldPos)   lights, pets, crew idle, screen pulse
//   interior.toLocal(v,out?) / toWorld(v,out?)   local = L units
//   interior.stats {tris, draws}   interior.dispose()
import { createHuman, createPet } from './ship-human.js';

const HW = 10, HL = 30, HH = 8, PLAT = 0.8, RAMP0 = 21, RAMP1 = 23.5;
const C = { hull: 0x4a4366, hull2: 0x38324f, hull3: 0x262138, panel: 0x5b5380, violet: 0x8A3CFF, cyan: 0x5CE8FF, mag: 0xFF5CE1, amber: 0xFFC46B, white: 0xF4F8FF, floor: 0x625b80, wall: 0x6a6190, dark: 0x1a1626, wood: 0x8a5a2b, wood2: 0xb98a4f, steel: 0x9a96b0 };
const CREW_S = 2.0, PET_S = 2.2, N_PEN = 4, N_POD = 4, N_CRATE = 24, N_PLAQUE = 24, N_MENU = 6;
const BZ = 26.0;                                   // mission board centre z
const OPZ0 = -26, OPZ1 = -16, OZ0 = -27, OZ1 = -15, OX = -22;   // observation alcove: opening in the left wall, room z range, back-wall x
const KIND_COL = { planet: 0x5CE8FF, moon: 0x9ad8ff, creature: 0xFF5CE1, fauna: 0xFF5CE1, flora: 0x7CFF8A, plant: 0x7CFF8A, mineral: 0xFFC46B, resource: 0xFFC46B, ship: 0x8A3CFF, station: 0xF4F8FF, anomaly: 0xFF6B6B, star: 0xFFE98A };

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
const VERT_INST = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = vec3(0.6);
  #ifdef USE_INSTANCING_COLOR
    vC = instanceColor;
  #endif
  vec4 mv = modelViewMatrix * instanceMatrix * vec4(position, 1.0);
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
  vec3 Ld = normalize(mat3(viewMatrix) * normalize(vec3(-0.35, 0.8, 0.3)));
  float d = abs(dot(n, Ld)) * 0.5 + 0.62;
  float rim = pow(1.0 - abs(dot(n, normalize(-vV))), 3.0) * 0.25;
  gl_FragColor = vec4(vC * d * 0.5 + rim * vec3(0.3, 0.2, 0.6), 1.0);
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
  gl_FragColor = vec4(vC * g * 1.6, min(1.0, g * 0.8));
}`;

function mul(seed) { let a = seed | 0; return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }

function makeSoup(T) {
  const pos = [], col = [], c = new T.Color(), v = new T.Vector3(), m = new T.Matrix4(), q = new T.Quaternion(), e = new T.Euler(), s = new T.Vector3(), p = new T.Vector3();
  const unit = new T.BoxGeometry(1, 1, 1).toNonIndexed().attributes.position.array;
  const soup = {
    geo(arr, hex, mat, br = 1) {
      c.set(hex); const r = c.r * br, g = c.g * br, b = c.b * br;
      for (let i = 0; i < arr.length; i += 3) { v.set(arr[i], arr[i + 1], arr[i + 2]); if (mat) v.applyMatrix4(mat); pos.push(v.x, v.y, v.z); col.push(r, g, b); }
    },
    cbox(w, h, d, cx, cy, cz, hex, br = 1, rx = 0, ry = 0, rz = 0) { e.set(rx, ry, rz); q.setFromEuler(e); s.set(w, h, d); p.set(cx, cy, cz); m.compose(p, q, s); soup.geo(unit, hex, m, br); },
    bx(x0, x1, y0, y1, z0, z1, hex, br = 1) { soup.cbox(x1 - x0, y1 - y0, z1 - z0, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2, hex, br); },
    shape(geometry, hex, mat, br = 1) { const g = geometry.index ? geometry.toNonIndexed() : geometry; soup.geo(g.attributes.position.array, hex, mat, br); g.dispose(); },
    geometry() { const g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute(pos, 3)); g.setAttribute('color', new T.Float32BufferAttribute(col, 3)); g.computeBoundingSphere(); return g; },
    tris() { return pos.length / 9; },
  };
  return soup;
}

function heightAt(x, z) {
  if (z <= RAMP0) return 0;
  if (z >= RAMP1) return PLAT;
  return (z - RAMP0) / (RAMP1 - RAMP0) * PLAT;
}

export function createInterior(engine, L, opts) {
  opts = opts || {};
  const T = engine.THREE, scene = engine.scene;
  L = L > 0 ? L : 0.03;
  const U = L * (opts.scale > 0 ? opts.scale : 1);
  const group = new T.Group(); group.name = 'ship-interior'; group.visible = false; group.scale.setScalar(U);
  const pocket = opts.pocket ? opts.pocket.clone() : new T.Vector3(0, 60000, 0);
  group.position.copy(pocket);
  scene.add(group);

  const uTime = { value: 0 }, uPx = { value: 6.5 }, uH = { value: 900 };
  const matLit = new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_LIT, vertexColors: true, side: T.DoubleSide, uniforms: {} });
  const matEmi = new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_EMI, vertexColors: true, side: T.DoubleSide, uniforms: { uTime } });
  const matCrate = new T.ShaderMaterial({ vertexShader: VERT_INST, fragmentShader: FRAG_LIT, side: T.DoubleSide, uniforms: {} });
  const matLight = new T.ShaderMaterial({ vertexShader: VERT_LIGHT, fragmentShader: FRAG_LIGHT, uniforms: { uPx, uH }, transparent: true, depthWrite: false, blending: T.AdditiveBlending });
  const matGlass = new T.MeshBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.17, depthWrite: false, side: T.DoubleSide });
  const quadGeo = new T.PlaneGeometry(1, 1);

  // ── board canvas ──
  const bc = document.createElement('canvas'); bc.width = 512; bc.height = 272;
  const btex = new T.CanvasTexture(bc); btex.colorSpace = T.SRGBColorSpace; btex.anisotropy = 4;
  const matBoard = new T.MeshBasicMaterial({ map: btex, toneMapped: false, side: T.DoubleSide });
  function missionProgress(r) {
    let p = r.progress;
    if (p == null && r.target > 0) p = (r.have != null ? r.have : r.cur != null ? r.cur : r.count) / r.target;
    if (p == null && /^(active|open|prog|run|doing|ongoing)/i.test(String(r.status || ''))) p = 0;
    if (p == null || !isFinite(p)) return null;
    return /^(done|complete|paid)/i.test(String(r.status || '')) ? 1 : Math.max(0, Math.min(1, +p));
  }
  function drawBoard(list) {
    const x = bc.getContext('2d');
    x.fillStyle = '#0b0716'; x.fillRect(0, 0, 512, 272);
    x.strokeStyle = '#8a3cff'; x.lineWidth = 6; x.strokeRect(4, 4, 504, 264);
    x.strokeStyle = '#5ce8ff'; x.lineWidth = 2; x.strokeRect(14, 14, 484, 244);
    x.font = '700 30px monospace'; x.textAlign = 'left'; x.fillStyle = '#ff5ce1'; x.shadowColor = '#ff5ce1'; x.shadowBlur = 12; x.fillText('MISSION BOARD', 30, 56); x.shadowBlur = 0;
    const rows = (list && list.length) ? list.slice(0, 3) : [{ title: 'no contracts posted', status: '' }];
    rows.forEach((r, i) => {
      const y = 108 + i * 52, p = missionProgress(r);
      x.fillStyle = 'rgba(138,60,255,0.22)'; x.fillRect(28, y - 30, 456, 46);
      x.fillStyle = '#e8f6ff'; x.font = '600 22px monospace'; x.textAlign = 'left'; x.fillText(String(r.title || '').slice(0, 28), 40, y - 8);
      x.fillStyle = '#5ce8ff'; x.textAlign = 'right'; x.fillText(String(p == null ? (r.status || '') : p >= 1 ? 'DONE' : Math.round(p * 100) + '%').slice(0, 10), 474, y - 8); x.textAlign = 'left';
      if (p != null) {
        x.fillStyle = '#1b1230'; x.fillRect(40, y + 1, 434, 9);
        if (p > 0) { const g = x.createLinearGradient(40, 0, 474, 0); g.addColorStop(0, '#5ce8ff'); g.addColorStop(1, '#ff5ce1'); x.fillStyle = g; x.fillRect(40, y + 1, Math.max(4, 434 * p), 9); }
        x.strokeStyle = 'rgba(92,232,255,0.5)'; x.lineWidth = 1; x.strokeRect(40.5, y + 1.5, 433, 8);
      }
    });
    btex.needsUpdate = true;
  }
  drawBoard([]);

  // ── trophy plaque atlas: 8x3 cells, one texture, one draw ──
  const CW = 128, CH = 112, pc = document.createElement('canvas'); pc.width = CW * 8; pc.height = CH * 3;
  const ptex = new T.CanvasTexture(pc); ptex.colorSpace = T.SRGBColorSpace; ptex.anisotropy = 4;
  const matPlaque = new T.MeshBasicMaterial({ map: ptex, toneMapped: false, side: T.DoubleSide });
  const kindHex = (k) => { k = String(k || '').toLowerCase(); if (KIND_COL[k] != null) return KIND_COL[k]; return new T.Color().setHSL((hashStr(k) % 360) / 360, 0.65, 0.58).getHex(); };
  function drawPlaques(list) {
    const x = pc.getContext('2d'); x.clearRect(0, 0, pc.width, pc.height);
    for (let i = 0; i < N_PLAQUE; i++) {
      const cx = (i % 8) * CW, cy = Math.floor(i / 8) * CH, d = list[i];
      x.fillStyle = d ? '#140d24' : '#0d0916'; x.fillRect(cx, cy, CW, CH);
      x.strokeStyle = d ? '#8a3cff' : '#2a2142'; x.lineWidth = 3; x.strokeRect(cx + 2, cy + 2, CW - 4, CH - 4);
      if (!d) continue;
      const col = '#' + new T.Color(kindHex(d.kind)).getHexString(), r = mul(hashStr(String(d.kind) + String(d.name)));
      x.fillStyle = col; x.globalAlpha = 0.16; x.fillRect(cx + 8, cy + 8, CW - 16, 64); x.globalAlpha = 1;
      for (let py = 0; py < 5; py++) for (let px = 0; px < 3; px++) if (r() < 0.55 || (px === 2 && py === 2)) { x.fillStyle = col; x.fillRect(cx + 34 + px * 12, cy + 12 + py * 11, 11, 10); x.fillRect(cx + 34 + (4 - px) * 12, cy + 12 + py * 11, 11, 10); }
      x.textAlign = 'center'; x.fillStyle = '#f4f8ff'; x.font = '700 15px monospace'; x.fillText(String(d.name || '?').slice(0, 13), cx + CW / 2, cy + 90);
      x.fillStyle = col; x.font = '600 11px monospace'; x.fillText((String(d.kind || '') + (d.by ? ' / ' + d.by : '')).slice(0, 20), cx + CW / 2, cy + 105);
    }
    ptex.needsUpdate = true;
  }
  drawPlaques([]);

  // ── layout data ──
  const rooms = [
    { name: 'aft', min: new T.Vector3(-HW, 0, -HL), max: new T.Vector3(HW, HH, -12) },
    { name: 'cargo', min: new T.Vector3(-HW, 0, -12), max: new T.Vector3(HW, HH, 7.5) },
    { name: 'quarters', min: new T.Vector3(-HW, 0, 7.5), max: new T.Vector3(HW, HH, RAMP0) },
    { name: 'cockpit', min: new T.Vector3(-HW, 0, RAMP0), max: new T.Vector3(HW, HH, HL) },
  ];
  const walls = [];
  const W = (name, x0, x1, y0, y1, z0, z1) => { const w = { name, min: new T.Vector3(x0, y0, z0), max: new T.Vector3(x1, y1, z1) }; walls.push(w); return w; };
  const spots = [];      // {obj, local:Vector3, r}
  const spot = (x, y, z, r, extra) => { const o = Object.assign({ pos: new T.Vector3(), radius: 0 }, extra || {}); spots.push({ obj: o, local: new T.Vector3(x, y, z), r }); return o; };

  const B = makeSoup(T), E = makeSoup(T), G = makeSoup(T), rng = mul(2501);
  const lights = [];
  const lt = (x, y, z, size, hex, mode, phase, speed) => lights.push({ x, y, z, size, c: new T.Color(hex), mode: mode || 0, ph: phase || 0, sp: speed || 1 });

  // shell
  B.bx(-HW, HW, -0.6, 0, -HL, HL, C.floor, 0.85);
  B.bx(-HW - 0.5, -HW, -0.6, HH + 0.5, -HL - 0.5, OPZ0, C.wall); B.bx(-HW - 0.5, -HW, -0.6, HH + 0.5, OPZ1, HL + 0.5, C.wall); B.bx(-HW - 0.5, -HW, 6, HH + 0.5, OPZ0, OPZ1, C.wall);
  B.bx(HW, HW + 0.5, -0.6, HH + 0.5, -HL - 0.5, HL + 0.5, C.wall);
  B.bx(-HW, HW, HH, HH + 0.5, -HL, HL, C.hull2);
  W('wallL', -HW - 0.5, -HW, -1, HH + 1, -HL - 0.5, OPZ0); W('wallL2', -HW - 0.5, -HW, -1, HH + 1, OPZ1, HL + 0.5); W('wallR', HW, HW + 0.5, -1, HH + 1, -HL - 0.5, HL + 0.5);
  W('aftWall', -HW, HW, -1, HH + 1, -HL - 0.5, -HL); W('frontWall', -HW, HW, -1, HH + 1, HL, HL + 0.5);
  // aft wall + hatch
  B.bx(-HW, HW, 0, HH, -HL - 0.5, -HL, C.wall, 0.9);
  B.bx(-2.6, 2.6, 0, 4.6, -HL + 0.0, -HL + 0.35, C.hull3); B.bx(-2.2, 2.2, 0, 4.2, -HL + 0.35, -HL + 0.45, C.dark);
  E.bx(-2.6, -2.35, 0, 4.8, -HL + 0.35, -HL + 0.5, C.violet); E.bx(2.35, 2.6, 0, 4.8, -HL + 0.35, -HL + 0.5, C.violet); E.bx(-2.6, 2.6, 4.55, 4.8, -HL + 0.35, -HL + 0.5, C.violet);
  for (let i = 0; i < 5; i++) E.bx(-1.8 + i * 0.9, -1.5 + i * 0.9, 0.4, 0.6, -HL + 0.46, -HL + 0.5, C.amber, 0.8);
  E.shape(new T.TorusGeometry(2.0, 0.07, 4, 24), C.cyan, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(0, 0.02, -HL + 2.4)));
  lt(0, 5.2, -HL + 0.8, 1.2, C.cyan, 4, 0, 0.4);
  // floor seams, guide strips, skirting, ribs, light strips
  for (let z = -HL + 1; z < HL - 1; z += 4) {
    E.bx(-0.5, 0.5, 0.0, 0.03, z, z + 1.5, C.cyan, 0.45);
    E.bx(-HW + 0.6, -HW + 0.7, 0.0, 0.03, z, z + 2.2, C.violet, 0.6); E.bx(HW - 0.7, HW - 0.6, 0.0, 0.03, z, z + 2.2, C.violet, 0.6);
    E.bx(-0.7, 0.7, HH - 0.05, HH, z, z + 2.6, C.white, 0.9);
    E.bx(-6.2, -5.0, HH - 0.05, HH, z + 0.5, z + 1.9, C.cyan, 0.55); E.bx(5.0, 6.2, HH - 0.05, HH, z + 0.5, z + 1.9, C.cyan, 0.55);
  }
  for (let z = -HL + 2; z <= HL - 2; z += 5) {
    for (const sd of [-1, 1]) { if (sd < 0 && ((z > OPZ0 - 0.4 && z < OPZ1 + 0.4) || Math.abs(z - BZ) < 3.7)) continue; B.bx(sd > 0 ? HW - 0.6 : -HW, sd > 0 ? HW : -HW + 0.6, 0, HH, z - 0.25, z + 0.25, C.hull3); E.bx(sd > 0 ? HW - 0.62 : -HW + 0.58, sd > 0 ? HW - 0.58 : -HW + 0.62, 1.2, 6.8, z - 0.05, z + 0.05, C.violet, 0.6); }
  }
  for (const sd of [-1, 1]) {
    for (const [za, zb] of (sd < 0 ? [[-HL, OPZ0], [OPZ1, HL]] : [[-HL, HL]])) {
      B.bx(sd > 0 ? HW - 0.3 : -HW, sd > 0 ? HW : -HW + 0.3, 0, 0.9, za, zb, C.hull3); E.bx(sd > 0 ? HW - 0.32 : -HW + 0.3, sd > 0 ? HW - 0.3 : -HW + 0.32, 0.88, 0.95, za, zb, C.cyan, 0.5);
    }
    // pipes along the ceiling
    for (const [py, pr, col] of [[7.2, 0.22, C.hull3], [6.5, 0.15, C.panel]]) {
      B.shape(new T.CylinderGeometry(pr, pr, 2 * HL, 8, 1), col, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(sd * (HW - 0.9), py, 0)));
    }
    for (let z = -HL + 3; z < HL; z += 6) { B.cbox(0.55, 1.2, 0.35, sd * (HW - 0.9), 6.85, z, C.hull3); E.bx(sd * (HW - 0.9) - 0.03, sd * (HW - 0.9) + 0.03, 7.0, 7.4, z - 0.19, z + 0.19, C.amber, 0.7); }
  }
  // bulkheads with doorways at z = -12 and 7.5
  for (const bz of [-12, 7.5]) {
    B.bx(-HW, -3.5, 0, HH, bz - 0.25, bz + 0.25, C.wall); B.bx(3.5, HW, 0, HH, bz - 0.25, bz + 0.25, C.wall); B.bx(-3.5, 3.5, 5.2, HH, bz - 0.25, bz + 0.25, C.wall);
    E.bx(-3.55, -3.35, 0, 5.3, bz - 0.28, bz + 0.28, C.violet); E.bx(3.35, 3.55, 0, 5.3, bz - 0.28, bz + 0.28, C.violet); E.bx(-3.55, 3.55, 5.2, 5.4, bz - 0.28, bz + 0.28, C.violet);
    W('bulkheadL' + bz, -HW, -3.5, -1, HH + 1, bz - 0.25, bz + 0.25); W('bulkheadR' + bz, 3.5, HW, -1, HH + 1, bz - 0.25, bz + 0.25);
    lt(0, 5.0, bz, 0.8, C.violet, 4, 0.3, 0.3);
  }
  // OBSERVATION room: alcove off the left wall (z -26..-16 opening), wide window in the back wall, trophy plaques on the north wall
  B.bx(OX, -HW, -0.6, 0, OZ0, OZ1, C.floor, 0.85); B.bx(OX - 0.5, -HW, 6, 6.5, OZ0 - 0.5, OZ1 + 0.5, C.hull2);
  B.bx(OX - 0.5, -HW, -0.6, 6.5, OZ0 - 0.5, OZ0, C.wall); B.bx(OX - 0.5, -HW, -0.6, 6.5, OZ1, OZ1 + 0.5, C.wall);
  B.bx(OX - 0.5, OX, -0.6, 1.0, OZ0, OZ1, C.hull3); B.bx(OX - 0.5, OX, 5.0, 6.5, OZ0, OZ1, C.hull3);
  B.bx(OX - 0.5, OX, -0.6, 6.5, OZ0, OPZ0, C.hull3); B.bx(OX - 0.5, OX, -0.6, 6.5, OPZ1, OZ1, C.hull3);
  E.bx(OX, OX + 0.1, 0.95, 1.05, OPZ0, OPZ1, C.violet); E.bx(OX, OX + 0.1, 4.95, 5.05, OPZ0, OPZ1, C.violet);
  E.bx(OX, OX + 0.1, 1.0, 5.0, OPZ0, OPZ0 + 0.1, C.violet); E.bx(OX, OX + 0.1, 1.0, 5.0, OPZ1 - 0.1, OPZ1, C.violet);
  G.bx(OX - 0.1, OX - 0.05, 1.0, 5.0, OPZ0, OPZ1, 0x7ff0ff);
  B.bx(OX + 0.2, OX + 0.5, 0, 1.15, OPZ0 + 0.3, OPZ1 - 0.3, C.hull); E.bx(OX + 0.2, OX + 0.5, 1.15, 1.2, OPZ0 + 0.3, OPZ1 - 0.3, C.cyan, 0.6);
  B.bx(-19.5, -13, 0, 0.6, OZ1 - 1.1, OZ1, C.hull3); B.bx(-19.5, -13, 0.6, 0.7, OZ1 - 1.1, OZ1, C.violet, 0.7);
  E.bx(OX + 0.5, -HW, 0.0, 0.03, -21.2, -20.8, C.cyan, 0.45);
  W('obsBackWall', OX - 0.5, OX + 0.5, -1, HH + 1, OZ0, OZ1); W('obsNorth', OX - 0.5, -HW, -1, HH + 1, OZ0 - 0.5, OZ0); W('obsSouth', OX - 0.5, -HW, -1, HH + 1, OZ1, OZ1 + 0.5);
  W('obsBench', -19.5, -13, 0, 0.7, OZ1 - 1.1, OZ1); W('obsRail', OX + 0.2, OX + 0.5, 0, 1.2, OPZ0 + 0.3, OPZ1 - 0.3);
  rooms.push({ name: 'observation', min: new T.Vector3(OX, 0, OZ0), max: new T.Vector3(-HW, 6, OZ1) });
  const PLQ_COLS = 8, PLQ_X0 = -16 - 3.5 * 1.2, PLQ_Z = OZ0 + 0.1, plqPos = [];
  for (let r = 0; r < 3; r++) for (let c = 0; c < PLQ_COLS; c++) {
    const x = PLQ_X0 + c * 1.2, y = 3.95 - r * 1.05; plqPos.push([x, y]);
    B.bx(x - 0.56, x + 0.56, y - 0.5, y + 0.5, OZ0, OZ0 + 0.06, C.hull3);
  }
  for (let r = 0; r < 3; r++) E.bx(PLQ_X0 - 0.6, PLQ_X0 + 7 * 1.2 + 0.6, 3.95 - r * 1.05 - 0.54, 3.95 - r * 1.05 - 0.51, OZ0, OZ0 + 0.08, C.violet, 0.6);
  lt(-16, 5.4, OZ0 + 0.8, 0.9, C.amber, 0, 0, 0); lt(-14, 5.6, -21, 1.0, C.cyan, 4, 0.2, 0.3); lt(OX + 1, 3.0, -21, 0.7, C.violet, 4, 0.6, 0.3);
  // cockpit: ramp, platform, front wall with the star window, consoles, seats
  B.bx(-HW, HW, 0, PLAT, RAMP1, HL, C.floor, 0.95);
  const ang = Math.atan2(PLAT, RAMP1 - RAMP0), rl = Math.hypot(RAMP1 - RAMP0, PLAT);
  B.cbox(2 * HW, 0.12, rl, 0, PLAT / 2, (RAMP0 + RAMP1) / 2, C.floor, 1, -ang, 0, 0);
  E.bx(-HW, HW, PLAT, PLAT + 0.04, RAMP1 - 0.05, RAMP1 + 0.1, C.amber, 0.8);
  for (let i = 0; i < 6; i++) E.cbox(1.2, 0.03, 0.15, -3 + i * 1.2, PLAT * (0.3 + 0.1 * i) + 0.06, RAMP0 + 0.4 + i * 0.35, C.amber, 0.6, -ang, 0, 0);
  const WX = 7, WY0 = 1.9, WY1 = 7.0, FZ = HL;
  B.bx(-HW, HW, 0, WY0, FZ - 0.3, FZ + 0.5, C.hull3); B.bx(-HW, HW, WY1, HH + 0.5, FZ - 0.3, FZ + 0.5, C.hull3);
  B.bx(-HW, -WX, 0, HH + 0.5, FZ - 0.3, FZ + 0.5, C.hull3); B.bx(WX, HW, 0, HH + 0.5, FZ - 0.3, FZ + 0.5, C.hull3);
  E.bx(-WX, WX, WY0 - 0.12, WY0, FZ - 0.35, FZ - 0.2, C.violet); E.bx(-WX, WX, WY1, WY1 + 0.12, FZ - 0.35, FZ - 0.2, C.violet);
  E.bx(-WX - 0.12, -WX, WY0, WY1, FZ - 0.35, FZ - 0.2, C.violet); E.bx(WX, WX + 0.12, WY0, WY1, FZ - 0.35, FZ - 0.2, C.violet);
  E.bx(-0.05, 0.05, WY0, WY1, FZ - 0.3, FZ - 0.22, C.cyan, 0.4);
  const win = spot(0, (WY0 + WY1) / 2, FZ - 0.3, 0, { w: 2 * WX * U, h: (WY1 - WY0) * U });
  // console desk + seats
  B.bx(-6.5, 6.5, PLAT, PLAT + 1.2, 27.4, 29.2, C.hull3); B.cbox(13, 0.2, 1.7, 0, PLAT + 1.35, 28.1, C.panel, 1, 0.5, 0, 0);
  for (let i = 0; i < 9; i++) E.cbox(1.1, 0.03, 0.7, -5.2 + i * 1.3, PLAT + 1.47, 28.0, [C.cyan, C.mag, C.amber][i % 3], 0.8, 0.5, 0, 0);
  W('cockpitConsole', -6.5, 6.5, PLAT, PLAT + 1.6, 27.3, 29.3);
  for (const sx of [-2.6, 2.6]) { B.bx(sx - 0.7, sx + 0.7, PLAT, PLAT + 0.8, 24.6, 25.6, C.hull3); B.bx(sx - 0.7, sx + 0.7, PLAT + 0.8, PLAT + 2.4, 25.4, 25.8, C.hull); W('seat' + sx, sx - 0.7, sx + 0.7, PLAT, PLAT + 2.4, 24.6, 25.8); }
  lt(0, 4.8, 27, 1.0, C.cyan, 4, 0.1, 0.3);

  // crafting table (Minecraft style cube)
  const TX = -6.5, TZ = -20;
  B.bx(TX - 1.2, TX + 1.2, 0, 1.0, TZ - 1.2, TZ + 1.2, C.wood, 0.85);
  B.bx(TX - 1.28, TX + 1.28, 1.0, 1.2, TZ - 1.28, TZ + 1.28, C.wood2);
  for (const o of [-0.4, 0.4]) { B.bx(TX - 1.1, TX + 1.1, 1.2, 1.23, TZ + o - 0.03, TZ + o + 0.03, C.dark); B.bx(TX + o - 0.03, TX + o + 0.03, 1.2, 1.23, TZ - 1.1, TZ + 1.1, C.dark); }
  B.bx(TX - 1.1, TX + 1.1, 1.2, 1.22, TZ - 1.1, TZ - 1.06, C.dark); B.bx(TX - 1.1, TX + 1.1, 1.2, 1.22, TZ + 1.06, TZ + 1.1, C.dark);
  B.bx(TX - 1.1, TX - 1.06, 1.2, 1.22, TZ - 1.1, TZ + 1.1, C.dark); B.bx(TX + 1.06, TX + 1.1, 1.2, 1.22, TZ - 1.1, TZ + 1.1, C.dark);
  for (const sd of [-1, 1]) {   // tools on the side faces
    B.bx(TX + sd * 1.21, TX + sd * 1.26, 0.55, 0.85, TZ - 0.6, TZ + 0.6, C.hull3); B.bx(TX + sd * 1.26, TX + sd * 1.3, 0.62, 0.78, TZ - 0.5, TZ - 0.1, C.steel); B.bx(TX + sd * 1.26, TX + sd * 1.3, 0.6, 0.8, TZ + 0.15, TZ + 0.5, C.panel);
  }
  B.bx(TX - 1.2, TX + 1.2, 0.35, 0.45, TZ - 1.22, TZ - 1.2, C.wood2, 0.8);
  E.bx(TX - 0.15, TX + 0.15, 1.23, 1.26, TZ - 0.15, TZ + 0.15, C.cyan, 0.5);
  W('table', TX - 1.3, TX + 1.3, 0, 1.25, TZ - 1.3, TZ + 1.3);
  lt(TX, 1.5, TZ, 0.6, C.violet, 4, 0, 0.5);
  const table = spot(TX, 0, TZ, 3.6);
  // kitchen counter against the right wall
  const KX = 8.4, KZ = -20;
  B.bx(KX - 1.5, HW - 0.6, 0, 1.15, KZ - 3.2, KZ + 3.2, C.hull);
  B.bx(KX - 1.62, HW - 0.6, 1.15, 1.3, KZ - 3.3, KZ + 3.3, C.steel, 0.8);
  for (const [bx2, bz2] of [[KX - 0.1, KZ - 1.6], [KX - 0.1, KZ - 0.2], [KX + 0.7, KZ - 0.9]]) E.shape(new T.TorusGeometry(0.4, 0.05, 4, 14), C.amber, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(bx2, 1.33, bz2)));
  B.bx(KX - 1.0, HW - 0.6, 1.3, 1.9, KZ + 1.2, KZ + 2.8, C.hull3); B.bx(KX - 1.0, HW - 0.6, 1.9, 2.0, KZ + 1.2, KZ + 2.8, C.steel);
  B.bx(KX - 0.8, HW - 0.6, 3.5, 4.3, KZ - 2.2, KZ + 0.2, C.hull3); B.bx(HW - 1.5, HW - 1.1, 4.3, HH, KZ - 1.4, KZ - 0.6, C.hull3);
  E.bx(KX - 0.83, KX - 0.8, 3.7, 3.85, KZ - 2.0, KZ, C.amber, 0.9);
  for (let i = 0; i < 4; i++) B.cbox(0.34, 0.3, 0.34, KX + 0.1, 2.2, KZ - 3.1 + i * 0.1 * 3 + 1.2 * 0, [C.mag, C.cyan, C.amber, C.violet][i], 0.8);
  W('kitchen', KX - 1.65, HW, 0, 1.35, KZ - 3.3, KZ + 3.3);
  lt(KX, 3.6, KZ - 1, 0.6, C.amber, 0, 0, 0);
  const kitchen = spot(KX - 2.0, 0, KZ, 4.0);
  // cargo hold floor markers + rails
  for (let s = 0; s < 2; s++) for (let c = 0; c < 3; c++) for (let r = 0; r < 4; r++) {
    const cx = (s ? 1 : -1) * (5.0 + c * 1.8), cz = -9 + r * 3.6; E.bx(cx - 0.95, cx + 0.95, 0.0, 0.02, cz - 0.95, cz - 0.9, C.cyan, 0.35); E.bx(cx - 0.95, cx + 0.95, 0.0, 0.02, cz + 0.9, cz + 0.95, C.cyan, 0.35);
  }
  E.bx(-0.07, 0.07, 0.0, 0.03, -12, 7.5, C.mag, 0.5);
  // pens (left) and pods (right)
  const penZ = [10.5, 13.5, 16.5, 19.5];
  const penLocal = penZ.map((z) => new T.Vector3(-7.4, 0, z)), podLocal = penZ.map((z) => new T.Vector3(7.4, 0, z));
  penLocal.concat(podLocal).forEach((p, i) => {
    const pod = i >= N_PEN, sd = pod ? 1 : -1;
    B.shape(new T.CylinderGeometry(1.45, 1.6, 0.35, 14), C.hull3, new T.Matrix4().makeTranslation(p.x, 0.18, p.z));
    B.shape(new T.CylinderGeometry(1.45, 1.45, 0.25, 14), C.hull2, new T.Matrix4().makeTranslation(p.x, 3.55, p.z));
    B.bx(sd > 0 ? HW - 0.5 : -HW, sd > 0 ? HW : -HW + 0.5, 0.2, 3.6, p.z - 0.5, p.z + 0.5, C.hull3);
    E.shape(new T.TorusGeometry(1.35, 0.05, 4, 20), pod ? C.mag : C.cyan, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(p.x, 0.4, p.z)));
    E.shape(new T.TorusGeometry(1.35, 0.05, 4, 20), pod ? C.mag : C.cyan, new T.Matrix4().makeRotationX(Math.PI / 2).premultiply(new T.Matrix4().makeTranslation(p.x, 3.4, p.z)));
    G.shape(new T.CylinderGeometry(1.35, 1.35, 3.0, 16, 1, true), pod ? 0xff9ae8 : 0x7ff0ff, new T.Matrix4().makeTranslation(p.x, 1.9, p.z));
    if (pod) { B.bx(p.x - 0.5, p.x + 0.5, 0.2, 0.34, p.z - 0.5, p.z + 0.5, C.hull); E.bx(HW - 0.55, HW - 0.5, 1.6, 2.4, p.z - 0.3, p.z + 0.3, C.amber, 0.8); }
    lt(p.x, 3.7, p.z, 0.55, pod ? C.mag : C.cyan, 4, i * 0.13, 0.35);
    W((pod ? 'pod' : 'pen') + (i % 4), p.x - 1.6, p.x + 1.6, 0, 3.7, p.z - 1.6, p.z + 1.6);
  });
  // board: left wall in the cockpit
  const BXs = -HW + 0.05;
  B.bx(-HW, -HW + 0.25, PLAT + 1.0, PLAT + 4.8, BZ - 3.5, BZ + 3.5, C.hull3);
  E.bx(-HW + 0.25, -HW + 0.32, PLAT + 0.95, PLAT + 1.02, BZ - 3.5, BZ + 3.5, C.violet); E.bx(-HW + 0.25, -HW + 0.32, PLAT + 4.78, PLAT + 4.85, BZ - 3.5, BZ + 3.5, C.violet);
  B.bx(-HW + 0.3, -HW + 1.5, PLAT, PLAT + 1.1, BZ - 3.0, BZ + 3.0, C.hull); E.bx(-HW + 0.3, -HW + 1.5, PLAT + 1.1, PLAT + 1.14, BZ - 2.9, BZ + 2.9, C.cyan, 0.45);
  W('boardDesk', -HW, -HW + 1.5, PLAT, PLAT + 1.2, BZ - 3.0, BZ + 3.0);
  lt(-HW + 1.2, PLAT + 5.6, BZ, 0.8, C.mag, 4, 0.5, 0.25);   // above the frame, not a blob in front of the screen
  const board = spot(-HW + 2.2, PLAT, BZ, 4.0, {});
  const boardMesh = new T.Mesh(new T.PlaneGeometry(6, 3.18), matBoard);
  boardMesh.position.set(-HW + 0.36, PLAT + 2.9, BZ); boardMesh.rotation.y = Math.PI / 2; boardMesh.name = 'interior-board';
  // spawn + hatch
  const trophies = spot(-16, 0, -21, 6.5);
  const hatch = spot(0, 0, -HL + 1.6, 2.8);
  const spawn = spot(0, 0, -24, 0, { dir: new T.Vector3(0, 0, 1), localPos: new T.Vector3(0, 0, -24) });

  // ── meshes ──
  const hullMesh = new T.Mesh(B.geometry(), matLit); hullMesh.name = 'interior-hull';
  const emiMesh = new T.Mesh(E.geometry(), matEmi); emiMesh.name = 'interior-emissive';
  const glassMesh = new T.Mesh(G.geometry(), matGlass); glassMesh.name = 'interior-glass'; glassMesh.renderOrder = 2;
  const lightMesh = new T.InstancedMesh(quadGeo.clone(), matLight, lights.length); lightMesh.name = 'interior-lights';
  const m4 = new T.Matrix4(), qq = new T.Quaternion(), sc = new T.Vector3(), pp = new T.Vector3();
  lights.forEach((l, i) => { pp.set(l.x, l.y, l.z); sc.set(l.size, l.size, l.size); m4.compose(pp, qq, sc); lightMesh.setMatrixAt(i, m4); lightMesh.setColorAt(i, l.c); });
  lightMesh.instanceMatrix.needsUpdate = true; const lightCol = lightMesh.instanceColor.array;
  const pg = new T.BufferGeometry(), ppos = [], puv = [];
  plqPos.forEach(([x, y], i) => {
    const c = i % 8, r = Math.floor(i / 8), u0 = c / 8 + 0.004, u1 = (c + 1) / 8 - 0.004, v1 = 1 - r / 3 - 0.006, v0 = 1 - (r + 1) / 3 + 0.006, w = 0.5, h = 0.43, z = PLQ_Z - 0.03;
    ppos.push(x - w, y - h, z, x + w, y - h, z, x + w, y + h, z, x - w, y - h, z, x + w, y + h, z, x - w, y + h, z);
    puv.push(u0, v0, u1, v0, u1, v1, u0, v0, u1, v1, u0, v1);
  });
  pg.setAttribute('position', new T.Float32BufferAttribute(ppos, 3)); pg.setAttribute('uv', new T.Float32BufferAttribute(puv, 2));
  const plaqueMesh = new T.Mesh(pg, matPlaque); plaqueMesh.name = 'interior-plaques';
  const menuMesh = new T.InstancedMesh(new T.BoxGeometry(1, 1, 1), matCrate, N_MENU); menuMesh.name = 'interior-menu';
  for (let i = 0; i < N_MENU; i++) { menuMesh.setColorAt(i, new T.Color(0x333333)); }
  const crateMesh = new T.InstancedMesh(new T.BoxGeometry(1, 1, 1), matCrate, N_CRATE); crateMesh.name = 'interior-crates';
  const crateWalls = [];
  for (let i = 0; i < N_CRATE; i++) { crateMesh.setColorAt(i, new T.Color(0x333333)); crateWalls.push(W('crate' + i, 0, 0, -100, -100, 0, 0)); }
  crateMesh.count = N_CRATE;
  [hullMesh, emiMesh, glassMesh, lightMesh, crateMesh, boardMesh, plaqueMesh, menuMesh].forEach((m) => { m.frustumCulled = false; group.add(m); });
  const hullTris = Math.round(B.tris() + E.tris() + G.tris() + lights.length * 2 + 12 * N_CRATE + 2 + 2 * N_PLAQUE + 12 * N_MENU);

  // ── pens / pods ──
  const IDLE = { moving: false, running: false, airborne: false, speed: 0, facing: 0 };
  const pens = [], pods = [];
  const petModels = new Array(N_PEN).fill(null), crewModels = new Array(N_POD).fill(null);
  const prng = mul(77), PEN_Y = 0.36, CREW_BASE = Math.PI / 2;
  penLocal.forEach((p, i) => {
    const pen = spot(p.x, 0, p.z, 3.2, { pet: null, index: i });
    pen.setPet = (item) => {
      if (petModels[i]) { try { petModels[i].dispose(); } catch (e) { /* ignore */ } if (petModels[i].group.parent) petModels[i].group.parent.remove(petModels[i].group); petModels[i] = null; }
      pen.pet = item || null;
      if (!item) return;
      try {
        const m = createPet(T, { seed: (item.seed | 0) || hashStr(item.name || 'pet'), plan: item.plan });
        m.group.traverse((o) => { o.frustumCulled = false; });
        m.group.scale.setScalar(PET_S); m.group.position.set(p.x, PEN_Y, p.z); m.group.rotation.y = prng() * 6.28;
        m.setMood(item.mood != null ? item.mood : 0.5); group.add(m.group);
        m.st = { wx: p.x, wz: p.z, wt: 0.5 + prng() * 2, near: false }; petModels[i] = m;
      } catch (e) { console.info('[interior] pet failed', e); petModels[i] = null; }
    };
    pen.clear = () => pen.setPet(null);
    pens.push(pen);
  });
  podLocal.forEach((p, i) => {
    const pod = spot(p.x, 0, p.z, 3.2, { crew: null, index: i });
    pod.setCrew = (member) => {
      if (crewModels[i]) { try { crewModels[i].dispose(); } catch (e) { /* ignore */ } if (crewModels[i].group.parent) crewModels[i].group.parent.remove(crewModels[i].group); crewModels[i] = null; }
      pod.crew = member || null;
      if (!member) return;
      try {
        const h = createHuman(T, { color: member.color != null ? member.color : 0xB86CFF, role: member.role || null });
        h.group.traverse((o) => { o.frustumCulled = false; });
        h.group.scale.setScalar(CREW_S); h.group.position.set(p.x - 0.35, PEN_Y, p.z + 0.4); h.group.rotation.y = CREW_BASE;
        group.add(h.group); h.sway = prng() * 6.28; crewModels[i] = h;
      } catch (e) { console.info('[interior] crew failed', e); crewModels[i] = null; }
    };
    pod.clear = () => pod.setCrew(null);
    pods.push(pod);
  });

  // ── cargo ──
  const cargo = {
    stacks: [],
    setStacks(stacks) {
      stacks = (stacks || []).filter((s) => s && s.count > 0).slice(0, N_CRATE); cargo.stacks = stacks;
      const col = new T.Color();
      for (let i = 0; i < N_CRATE; i++) {
        const s = stacks[i], w = crateWalls[i];
        if (!s) { sc.set(0, 0, 0); pp.set(0, -100, 0); m4.compose(pp, qq, sc); crateMesh.setMatrixAt(i, m4); w.min.set(0, -100, 0); w.max.set(0, -100, 0); continue; }
        const side = i % 2, c = Math.floor(i / 2) % 3, r = Math.floor(i / 6);
        const sz = 0.6 + Math.min(1.1, Math.log2(1 + s.count) * 0.14), cx = (side ? 1 : -1) * (5.0 + c * 1.8), cz = -9 + r * 3.6;
        const hh = hashStr(s.id || s.name);
        if (s.color != null) col.set(s.color); else col.setHSL((hh % 360) / 360, 0.5, 0.45);
        crateMesh.setColorAt(i, col);
        pp.set(cx, sz / 2, cz); sc.set(sz, sz, sz); m4.compose(pp, qq, sc); crateMesh.setMatrixAt(i, m4);
        w.min.set(cx - sz / 2, 0, cz - sz / 2); w.max.set(cx + sz / 2, sz, cz + sz / 2);
      }
      crateMesh.instanceMatrix.needsUpdate = true; if (crateMesh.instanceColor) crateMesh.instanceColor.needsUpdate = true;
    },
  };
  cargo.setStacks([]);
  board.setMissions = (list) => { board.missions = list || []; drawBoard(board.missions); };
  kitchen.menu = [];
  kitchen.setMenu = (items) => {
    items = (items || []).filter(Boolean).slice(0, N_MENU); kitchen.menu = items;
    const col = new T.Color();
    for (let i = 0; i < N_MENU; i++) {
      const it = items[i];
      if (!it) { sc.set(0, 0, 0); pp.set(0, -100, 0); m4.compose(pp, qq, sc); menuMesh.setMatrixAt(i, m4); continue; }
      const sz = 0.42 + Math.min(0.2, Math.log2(1 + (it.count || 1)) * 0.04);
      if (it.color != null) col.set(it.color); else col.setHSL((hashStr(it.id || it.name) % 360) / 360, 0.5, 0.45);
      menuMesh.setColorAt(i, col);
      pp.set(KX - 1.0, 1.3 + sz / 2, KZ - 3.0 + i * 1.2); sc.set(sz, sz, sz); m4.compose(pp, qq, sc); menuMesh.setMatrixAt(i, m4);
    }
    menuMesh.instanceMatrix.needsUpdate = true; if (menuMesh.instanceColor) menuMesh.instanceColor.needsUpdate = true;
  };
  kitchen.setMenu([]);
  trophies.discoveries = [];
  trophies.setDiscoveries = (list) => { trophies.discoveries = (list || []).filter(Boolean).slice(0, N_PLAQUE); drawPlaques(trophies.discoveries); };

  // ── world sync ──
  let lx = NaN, ly = NaN, lz = NaN;
  function sync() {
    group.updateMatrixWorld(true);
    spots.forEach((s) => { s.obj.pos.copy(s.local).applyMatrix4(group.matrixWorld); s.obj.radius = s.r * U; });
    spawn.dir.set(0, 0, 1).applyQuaternion(group.quaternion);
    lx = group.position.x; ly = group.position.y; lz = group.position.z;
  }
  sync();
  hatch.radius = 2.8 * U;
  const windows = [win];
  const _tg = new T.Vector3(), _v = new T.Vector3(), _inv = new T.Matrix4(), _n = new T.Vector3();
  const interior = {
    group, pocket: group.position, unit: U, rooms, windows, hatch, spawn, table, kitchen, board, pens, pods, cargo, trophies, observation: trophies,
    deck: { walls, floorAt(wp, out) {
      const o = out || { point: new T.Vector3(), normal: new T.Vector3(), height: 0, inside: false };
      const l = _v.copy(wp).applyMatrix4(_inv.copy(group.matrixWorld).invert());
      o.inside = Math.abs(l.x) <= HW && l.z >= -HL && l.z <= HL;
      const h = heightAt(l.x, l.z); o.height = h * U;
      o.point.set(l.x, h, l.z).applyMatrix4(group.matrixWorld);
      if (l.z > RAMP0 && l.z < RAMP1) _n.set(0, RAMP1 - RAMP0, -PLAT).normalize(); else _n.set(0, 1, 0);
      o.normal.copy(_n).applyQuaternion(group.quaternion);
      return o;
    } },
    stats: { tris: hullTris, draws: 8 },
    setVisible(b) { group.visible = !!b; },
    toLocal(v, out) { return (out || new T.Vector3()).copy(v).applyMatrix4(_inv.copy(group.matrixWorld).invert()); },
    toWorld(v, out) { return (out || new T.Vector3()).copy(v).applyMatrix4(group.matrixWorld); },
    update(t, dt, playerPos) {
      dt = dt || 0.016; uTime.value = t;
      if (group.position.x !== lx || group.position.y !== ly || group.position.z !== lz) sync();
      if (!group.visible) return;
      try { const sz = engine.renderer.getSize(_v); const pr = engine.renderer.getPixelRatio ? engine.renderer.getPixelRatio() : 1; uH.value = Math.max(200, sz.y * pr); uPx.value = 6.5 * pr; } catch (e) { /* defaults */ }
      for (let i = 0; i < lights.length; i++) {
        const l = lights[i]; const k = l.mode === 4 ? 0.6 + 0.4 * Math.sin(t * l.sp * 6.28 + l.ph * 6.28) : 1;
        lightCol[i * 3] = l.c.r * k; lightCol[i * 3 + 1] = l.c.g * k; lightCol[i * 3 + 2] = l.c.b * k;
      }
      lightMesh.instanceColor.needsUpdate = true;
      matBoard.color.setScalar(0.9 + 0.1 * Math.sin(t * 2.2));
      let pl = null; if (playerPos) pl = interior.toLocal(playerPos, new T.Vector3());
      for (let i = 0; i < N_PEN; i++) {
        const m = petModels[i]; if (!m) continue;
        const c = penLocal[i], st = m.st, g = m.group;
        let near = false, dx = 0, dz = 0;
        if (pl) { dx = pl.x - c.x; dz = pl.z - c.z; near = Math.hypot(dx, dz) < 1.35 + 2 && pl.y < 6; }
        if (near !== st.near) { st.near = near; m.setMood(near ? 0.95 : 0.5); }
        if (near) _tg.set(pl.x, PEN_Y, pl.z);
        else {
          st.wt -= dt; if (st.wt <= 0) { const a = prng() * 6.283, r = 0.15 + prng() * 0.65; st.wx = c.x + Math.cos(a) * r; st.wz = c.z + Math.sin(a) * r; st.wt = 2.5 + prng() * 4; }
          _tg.set(st.wx, PEN_Y, st.wz);
        }
        m.follow(_tg, dt, near ? { dist: 0.5, rate: 4 } : { dist: 0.02, rate: 1.2 });
        const ox = g.position.x - c.x, oz = g.position.z - c.z, od = Math.hypot(ox, oz);
        if (od > 0.95) { g.position.x = c.x + ox / od * 0.95; g.position.z = c.z + oz / od * 0.95; }
      }
      for (let i = 0; i < N_POD; i++) {
        const h = crewModels[i]; if (!h) continue;
        try { h.update(dt, IDLE); } catch (e) { /* ignore */ }
        const g = h.group, c = podLocal[i];
        let want = CREW_BASE + Math.sin(t * 0.5 + h.sway) * 0.14;
        if (pl && Math.hypot(pl.x - c.x, pl.z - c.z) < 1.35 + 2.5) want = Math.atan2(-(pl.x - g.position.x), -(pl.z - g.position.z));
        let df = want - g.rotation.y; df = Math.atan2(Math.sin(df), Math.cos(df));
        g.rotation.y += df * Math.min(1, dt * 4);
      }
    },
    dispose() {
      if (group.parent) group.parent.remove(group);
      petModels.forEach((m) => { try { m && m.dispose(); } catch (e) { /* ignore */ } });
      plaqueMesh.geometry.dispose(); menuMesh.geometry.dispose(); menuMesh.dispose(); matPlaque.dispose(); ptex.dispose();
      crewModels.forEach((h) => { try { h && h.dispose(); } catch (e) { /* ignore */ } });
      [hullMesh, emiMesh, glassMesh, boardMesh].forEach((m) => m.geometry.dispose());
      lightMesh.geometry.dispose(); lightMesh.dispose(); crateMesh.geometry.dispose(); crateMesh.dispose();
      quadGeo.dispose(); matLit.dispose(); matEmi.dispose(); matCrate.dispose(); matLight.dispose(); matGlass.dispose(); matBoard.dispose(); btex.dispose();
    },
  };
  return interior;
}

// ── dev view: only when this module is imported on a page loaded with ?interior2 (window.__int2.view('pen'|'pod'|'trophy'|'board'|'kitchen')) ──
try {
  if (typeof location !== 'undefined' && /[?&]interior2(&|$)/.test(location.search) && typeof document !== 'undefined') {
    import('three').then((THREE) => {
      const W = innerWidth, H = innerHeight, cv = document.createElement('canvas');
      cv.style.cssText = 'position:fixed;inset:0;width:100%;height:100%;z-index:99999;background:#0b0710';
      document.body.appendChild(cv);
      const r = new THREE.WebGLRenderer({ canvas: cv, antialias: true, logarithmicDepthBuffer: true }); r.setSize(W, H); r.setClearColor(0x0b0710);
      const sc = new THREE.Scene(), cam = new THREE.PerspectiveCamera(55, W / H, 0.05, 200);
      const it = createInterior({ THREE, scene: sc, renderer: r }, 1, { scale: 1, pocket: new THREE.Vector3(0, 0, 0) }); it.setVisible(true);
      ['walker', 'jelly', 'strider', 0].forEach((pl, i) => it.pens[i].setPet({ seed: 11 + i * 7, plan: pl, name: 'p' + i }));
      ['pilot', 'miner', 'chef', 'scout'].forEach((ro, i) => it.pods[i].setCrew({ seed: i, name: ro, role: ro, color: [0xB86CFF, 0x5CE8FF, 0xFFC46B, 0xFF5CE1][i] }));
      it.trophies.setDiscoveries([{ kind: 'planet', name: 'Zorbulon', by: 'EMGOR' }, { kind: 'creature', name: 'Glim Hopper', by: 'Dana' }, { kind: 'flora', name: 'Violet Reed', by: 'Alex' }, { kind: 'mineral', name: 'Lydian Ore', by: 'kowl' }, { kind: 'star', name: 'Old Faithful', by: 'EMGOR' }, { kind: 'anomaly', name: 'The Hum', by: 'neptr' }]);
      it.kitchen.setMenu([{ name: 'stew', color: 0xFF5CE1 }, { name: 'ore', color: 0xFFC46B }, { name: 'reed', color: 0x7CFF8A }, { name: 'ice', color: 0x5CE8FF }, { name: 'goo', color: 0x8A3CFF }, { name: 'bun', color: 0xF4F8FF }]);
      it.board.setMissions([{ title: 'Haul 12 ore', status: 'active', have: 5, target: 12 }, { title: 'Scan Zorbulon', status: 'active', progress: 0.8 }, { title: 'Feed the pets', status: 'done' }]);
      const player = new THREE.Vector3(0, 1.5, 0), look = new THREE.Vector3(), views = {
        pen: [[-3.6, 2.4, 8.6], [-7.4, 1.3, 11.2], [-5.2, 1.5, 10.6]], pod: [[3.6, 2.6, 13.0], [7.2, 1.4, 15.0], [5.6, 1.5, 14.2]],
        trophy: [[-15, 3.2, -19.5], [-16, 2.9, -27], [-15, 1.5, -22]], board: [[-3.5, 3.6, 24.5], [-9.6, 3.6, 26], [0, 1.5, 26]], kitchen: [[2.5, 3, -16], [8.5, 1.8, -20], [2, 1.5, -16]],
      };
      window.__int2 = { it, cam, player, view(n) { const v = views[n]; cam.position.set(...v[0]); look.set(...v[1]); cam.lookAt(look); player.set(...v[2]); } };
      window.__int2.view('pen');
      let last = performance.now();
      const loop = (now) => { const dt = Math.min(0.05, (now - last) / 1000); last = now; it.update(now / 1000, dt, player); r.render(sc, cam); requestAnimationFrame(loop); };
      requestAnimationFrame(loop);
    });
  }
} catch (e) { /* dev view only */ }
