// ship-hub.js — MOB HUB: one 3D dungeon per planet (docs/mashup-plan.md section 4). Pure sim + one merged mesh set, no DOM.
//
//   createHub(engine, L, { planetNode, seed, parts }) -> hub
//   Units: hub-local METRES (+y up, entrance at the origin, dungeon runs +z and down to y=-12). hub.group is scaled by L (world per metre),
//   positioned at hub.anchor.pos (planet-local) and rotated so +y = hub.anchor.up. Caller parents hub.group into the planet ride group.
//     planetNode (all optional): { id|name, radius|r, biome, hubSpot(seed)->unit dir, heightLocal(dirVec)->radius }.
//   hub.anchor { dir, up, pos }           seeded land spot (planetNode.hubSpot if given, else seeded direction)
//   hub.entrance { pos, radius }          hub-local gate position (arch + green veil + 2 torches, visible from the air)
//   hub.interior { rooms:[{id,kind,box:{min,max},doors,cleared}], walls:[{min,max,door?,active}], floorAt(p)->y|null, roomAt(p), spawn, exit }
//        walls with active===false (open gates) must be ignored by collision. Gates lock while a fight is on (see update()).
//   hub.waves(roomId) -> [{ kinds, count, tier, hpMul }]   arenas: 3 waves each, escalating per arena; 'boss': [{ boss spec }, summon wave]
//   hub.spawns(roomId) -> [[x,y,z]...]    mob spawn points (around the cage)
//   hub.bossSpec  { kind:'boss', name, wave, hp:300, tier:4, scaleH:3, phases, drop:{ blueprint, coin:[100,200] } }   (feed generateBoss(THREE, name, wave, opts))
//   hub.chests [{ id, pos, opened, locked, loot:[{ id|item, n }] }]   hub.open(chestId) -> loot | null
//   hub.update(t, dt, playerHubPos) -> null | { type:'enter'|'lock'|'unlock', room }   torch flicker, gate state, cage pulse
//   hub.state { level, cleared:{roomId:1} }   hub.setCleared(roomId)   hub.serialize() / hub.restore(o)   hub.ambience = 'hub'   hub.stats {tris, draws}
import { mobSpawnRule, MOB_KINDS } from './ship-enemies.js';
import { blueprintItem, BLUEPRINTS } from './ship-craft.js';
import { resourceItem, RESOURCE_BASE } from './ship-items.js';

function mulberry(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }

const DEPTH = 12, TILE = 1.5, GW = 2;          // dungeon floor y = -DEPTH; gate/corridor half width GW; tile edge
// z-chain of rooms (x half-width hw, height h). kind: corridor | arena | loot | boss
const LAYOUT = [
  { id: 'start', kind: 'corridor', z0: 24, z1: 36, hw: GW, h: 4 },
  { id: 'c1', kind: 'corridor', z0: 36, z1: 42, hw: GW, h: 4 },
  { id: 'arena1', kind: 'arena', z0: 42, z1: 62, hw: 10, h: 8 },
  { id: 'c2', kind: 'corridor', z0: 62, z1: 68, hw: GW, h: 4 },
  { id: 'arena2', kind: 'arena', z0: 68, z1: 88, hw: 10, h: 8 },
  { id: 'c3', kind: 'corridor', z0: 88, z1: 94, hw: GW, h: 4 },
  { id: 'loot', kind: 'loot', z0: 94, z1: 106, hw: 6, h: 5 },
  { id: 'c4', kind: 'corridor', z0: 106, z1: 112, hw: GW, h: 4 },
  { id: 'arena3', kind: 'arena', z0: 112, z1: 132, hw: 10, h: 8 },
  { id: 'c5', kind: 'corridor', z0: 132, z1: 138, hw: GW, h: 4 },
  { id: 'boss', kind: 'boss', z0: 138, z1: 172, hw: 16, h: 14 },
];
const GATED = { arena1: 1, arena2: 1, arena3: 1, boss: 1 };
const BOSS_PHASES = [{ at: 1, move: 'slam' }, { at: 0.5, move: 'summon', n: 3 }, { at: 0.25, move: 'charge' }];
const BIOMES = ['rocky', 'lush', 'icy', 'exotic'];

export function createHub(engine, L, opts) {
  opts = opts || {};
  const T = engine.THREE;
  L = L > 0 ? L : 1;
  const pn = opts.planetNode || {};
  const seedKey = (opts.seed != null ? opts.seed : pn.id || pn.name || 'hub');
  const seed = typeof seedKey === 'number' ? seedKey | 0 : hashStr(seedKey);
  const rng = mulberry(seed ^ 0x48554221);
  const biome = BIOMES.includes(pn.biome) ? pn.biome : BIOMES[seed % 4];
  const planetName = String(pn.name || pn.id || seedKey);

  // ── anchor: seeded land spot (planet-local) ──
  let dir;
  if (typeof pn.hubSpot === 'function') { const d = pn.hubSpot(seed); dir = new T.Vector3(d.x, d.y, d.z).normalize(); }
  else { const a = rng() * Math.PI * 2, y = rng() * 1.2 - 0.6; dir = new T.Vector3(Math.cos(a) * Math.sqrt(1 - y * y), y, Math.sin(a) * Math.sqrt(1 - y * y)); }
  let rad = pn.radius || pn.r || 1;
  if (typeof pn.heightLocal === 'function') { const h = pn.heightLocal(dir); if (h > 0) rad = h; }
  const anchor = { dir, up: dir.clone(), pos: dir.clone().multiplyScalar(rad) };

  const group = new T.Group(); group.name = 'ship-hub';
  group.position.copy(anchor.pos); group.scale.setScalar(L);
  group.quaternion.setFromUnitVectors(new T.Vector3(0, 1, 0), anchor.up);

  // ── lights (baked into vertex colours; no real lights) ──
  const lights = [], WARM = [1.0, 0.58, 0.26], GREEN = [0.25, 1.0, 0.45];
  const addLight = (x, y, z, c, r, k) => lights.push({ x, y, z, c, r, k });
  function lightAt(x, y, z) {
    let R = 0.10, G = 0.09, B = 0.14;
    for (let i = 0; i < lights.length; i++) {
      const l = lights[i], dx = x - l.x, dy = y - l.y, dz = z - l.z, q = (dx * dx + dy * dy + dz * dz) / (l.r * l.r), f = l.k / (1 + q * 1.6) / (1 + q * q);
      R += l.c[0] * f; G += l.c[1] * f; B += l.c[2] * f;
    }
    return [Math.min(R, 1.6), Math.min(G, 1.6), Math.min(B, 1.6)];
  }

  // ── room table ──
  const rooms = LAYOUT.map((r) => {
    const o = { id: r.id, kind: r.kind, box: { min: [-r.hw, -DEPTH, r.z0], max: [r.hw, -DEPTH + r.h, r.z1] }, doors: [], cleared: false, _r: r };
    if (GATED[r.id]) o.doors = ['d:' + r.id + ':in'].concat(r.id === 'boss' ? [] : ['d:' + r.id + ':out']);
    return o;
  });
  const roomById = {}; rooms.forEach((r) => { roomById[r.id] = r; });
  const mid = (r) => (r._r.z0 + r._r.z1) / 2;

  // ── torches ──
  const torches = [];
  const torch = (x, y, z, side) => { torches.push({ x, y, z, side, ph: rng() * 6.28 }); addLight(x - side * 0.5, y, z, WARM, 5.5, 1.15); };
  // entrance: two torches in front of the pillars
  torch(-3.4, 3.2, -0.2, 1); torch(3.4, 3.2, -0.2, -1);
  for (const r of rooms) {
    const q = r._r, y = -DEPTH + 2.3;
    if (r.kind === 'corridor') { torch(-q.hw + 0.25, y, (q.z0 + q.z1) / 2, 1); if (q.z1 - q.z0 > 8) torch(q.hw - 0.25, y, q.z0 + 3, -1); }
    else {
      const n = r.kind === 'boss' ? 4 : r.kind === 'loot' ? 2 : 2, span = q.z1 - q.z0;
      for (let i = 0; i < n; i++) { const z = q.z0 + span * (i + 0.5) / n; torch(-q.hw + 0.25, y, z, 1); torch(q.hw - 0.25, y, z, -1); }
    }
  }
  // stair torches
  torch(-GW + 0.25, -4.5, 12, 1); torch(GW - 0.25, -9.5, 20, -1);

  // ── cages / gates / chests / spawn points ──
  const cages = [], gates = [], spawnPts = {};
  for (const r of rooms) {
    if (r.kind !== 'arena' && r.kind !== 'boss') continue;
    const q = r._r, cz = r.kind === 'boss' ? q.z1 - 6 : mid(r);
    cages.push({ room: r.id, x: 0, y: -DEPTH + 1.5, z: cz, s: r.kind === 'boss' ? 2.4 : 1.7 });
    addLight(0, -DEPTH + 1.5, cz, GREEN, 8, 0.85);
    const pts = [], n = r.kind === 'boss' ? 4 : 5, R = r.kind === 'boss' ? 6 : 4.2;
    for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2 + 0.4; pts.push([+(Math.cos(a) * R).toFixed(2), -DEPTH, +(cz + Math.sin(a) * R).toFixed(2)]); }
    spawnPts[r.id] = pts;
    gates.push({ id: 'd:' + r.id + ':in', z: q.z0, open: 1, target: 1 });
    if (r.id !== 'boss') gates.push({ id: 'd:' + r.id + ':out', z: q.z1, open: 1, target: 1 });
  }
  addLight(0, 3, 1, GREEN, 9, 0.9);                                   // entrance veil spill

  const chests = [];
  { const lr = roomById.loot._r, y = -DEPTH;
    [[-4.6, lr.z0 + 3], [4.6, lr.z0 + 6], [-4.6, lr.z0 + 9]].forEach((p, i) => chests.push({ id: 'loot' + i, pos: [p[0], y, p[1]], face: p[0] < 0 ? 1 : -1, room: 'loot' }));
    const bz = roomById.boss._r; chests.push({ id: 'boss0', pos: [0, y, bz.z1 - 2.5], face: 0, room: 'boss', locked: true, boss: true }); }
  const lootRng = mulberry(seed ^ 0xC4E57);
  const resKinds = Object.keys(RESOURCE_BASE);
  for (const c of chests) {
    const loot = [{ id: 'gor coin', n: c.boss ? 100 + Math.floor(lootRng() * 101) : 20 + Math.floor(lootRng() * 61) }];
    const rk = resKinds[Math.floor(lootRng() * resKinds.length)];
    loot.push({ item: resourceItem(rk, planetName), n: 1 + Math.floor(lootRng() * (c.boss ? 4 : 3)) });
    if (c.boss || lootRng() < 0.3) { const bp = blueprintItem(BLUEPRINTS[Math.floor(lootRng() * BLUEPRINTS.length)].id); if (bp) loot.push({ item: bp, n: 1 }); }
    c.loot = loot; c.opened = false; c.locked = !!c.locked;
  }

  // ── geometry: one vertex-coloured stone mesh ──
  const P = [], C = [];
  const col = new T.Color();
  const stoneCol = (moss, r) => moss ? col.setHSL(0.28 + r() * 0.06, 0.38, 0.17 + r() * 0.07) : col.setHSL(0.73 + r() * 0.04, 0.08, 0.2 + r() * 0.08);
  const vtx = (x, y, z, c) => { const l = lightAt(x, y, z); P.push(x, y, z); C.push(c.r * l[0], c.g * l[1], c.b * l[2]); };
  const quad = (a, b, c, d, cc) => { vtx(a[0], a[1], a[2], cc); vtx(b[0], b[1], b[2], cc); vtx(c[0], c[1], c[2], cc); vtx(a[0], a[1], a[2], cc); vtx(c[0], c[1], c[2], cc); vtx(d[0], d[1], d[2], cc); };
  const geoRng = mulberry(seed ^ 0x57A1E);
  // tiled plane: origin o, unit axes u,v, lengths; hole(x,y,z)->bool skips; mossy(y)->prob
  function plane(o, u, v, lu, lv, hole, mossP, tile) {
    tile = tile || TILE; const nu = Math.max(1, Math.round(lu / tile)), nv = Math.max(1, Math.round(lv / tile)), du = lu / nu, dv = lv / nv;
    for (let i = 0; i < nu; i++) for (let j = 0; j < nv; j++) {
      const p = (a, b) => [o[0] + u[0] * a + v[0] * b, o[1] + u[1] * a + v[1] * b, o[2] + u[2] * a + v[2] * b];
      const m = p((i + 0.5) * du, (j + 0.5) * dv); if (hole && hole(m[0], m[1], m[2])) continue;
      const moss = geoRng() < (mossP ? mossP(m[0], m[1], m[2]) : 0.1);
      const cc = stoneCol(moss, geoRng).clone(); if (((i + j) & 1) === 0) cc.multiplyScalar(0.9);
      quad(p(i * du, j * dv), p((i + 1) * du, j * dv), p((i + 1) * du, (j + 1) * dv), p(i * du, (j + 1) * dv), cc);
    }
  }
  const X = [1, 0, 0], Y = [0, 1, 0], Z = [0, 0, 1];
  function box(x0, y0, z0, x1, y1, z1, mossP, tile) {            // all 4 side faces + top (bottom skipped)
    const dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    plane([x0, y0, z0], X, Y, dx, dy, null, mossP, tile); plane([x0, y0, z1], X, Y, dx, dy, null, mossP, tile);
    plane([x0, y0, z0], Z, Y, dz, dy, null, mossP, tile); plane([x1, y0, z0], Z, Y, dz, dy, null, mossP, tile);
    plane([x0, y1, z0], X, Z, dx, dz, null, mossP, tile);
  }
  const mossLow = (base) => (x, y) => (y - base < 1.3 ? 0.5 : 0.1);
  const isGap = (x, y, base) => Math.abs(x) < GW && y - base < 4;

  for (const r of rooms) {
    const q = r._r, y0 = -DEPTH, y1 = y0 + q.h, hw = q.hw, len = q.z1 - q.z0;
    const solidEnd0 = r.kind !== 'corridor', solidEnd1 = r.kind !== 'corridor' && r.id !== 'boss' ? true : r.id === 'boss';
    plane([-hw, y0, q.z0], X, Z, 2 * hw, len, null, (x, y, z) => 0.16 + (Math.abs(x) > hw - 1.6 ? 0.35 : 0));        // floor
    plane([-hw, y1, q.z0], X, Z, 2 * hw, len, null, () => 0.04, 2.2);                                                  // ceiling
    plane([-hw, y0, q.z0], Z, Y, len, q.h, null, mossLow(y0)); plane([hw, y0, q.z0], Z, Y, len, q.h, null, mossLow(y0));
    if (solidEnd0) plane([-hw, y0, q.z0], X, Y, 2 * hw, q.h, (x, y) => isGap(x, y, y0), mossLow(y0));
    if (solidEnd1) plane([-hw, y0, q.z1], X, Y, 2 * hw, q.h, r.id === 'boss' ? null : (x, y) => isGap(x, y, y0), mossLow(y0));
  }
  // boss pillars
  const pillars = [];
  { const q = roomById.boss._r; for (let k = 0; k < 4; k++) for (const s of [-1, 1]) {
    const x = s * 9, z = q.z0 + 6 + k * 8; pillars.push({ min: [x - 0.8, -DEPTH, z - 0.8], max: [x + 0.8, -DEPTH + q.h, z + 0.8], door: null, active: true });
    box(x - 0.8, -DEPTH, z - 0.8, x + 0.8, -DEPTH + q.h, z + 0.8, mossLow(-DEPTH), 2.2); } }
  // stairs trench (open to the sky): steps + two side walls + parapet rim
  for (let i = 0; i < 24; i++) {
    const yt = -i * 0.5, yb = -(i + 1) * 0.5;
    plane([-GW, yt, i], X, Z, 2 * GW, 1, null, () => 0.08);
    plane([-GW, yb, i + 1], X, Y, 2 * GW, 0.5, null, () => 0.08, 0.5);
  }
  plane([-GW, -DEPTH, 0], Z, Y, 24, DEPTH + 0.7, null, (x, y) => (y < -9 ? 0.4 : 0.15), 1.5);
  plane([GW, -DEPTH, 0], Z, Y, 24, DEPTH + 0.7, null, (x, y) => (y < -9 ? 0.4 : 0.15), 1.5);
  box(-GW - 1.2, 0, 0, -GW, 0.7, 24, () => 0.2, 2); box(GW, 0, 0, GW + 1.2, 0.7, 24, () => 0.2, 2);
  // entrance arch: two pillars + lintel + capstones
  box(-3.5, 0, -0.7, -2.1, 7, 0.7, mossLow(0), 1.4); box(2.1, 0, -0.7, 3.5, 7, 0.7, mossLow(0), 1.4);
  box(-3.9, 6.9, -0.9, 3.9, 8.1, 0.9, () => 0.05, 1.3); box(-1.2, 8.1, -0.5, 1.2, 8.7, 0.5, () => 0.0, 1.2);
  // torch brackets + bones + skulls
  const BONE = new T.Color(0.62, 0.6, 0.5), WOOD = new T.Color(0.2, 0.12, 0.07);
  function addBox(cx, cy, cz, sx, sy, sz, ry, c) {
    const co = Math.cos(ry), si = Math.sin(ry), h = [sx / 2, sy / 2, sz / 2];
    const V = (a, b, d) => [cx + co * a * h[0] + si * d * h[2], cy + b * h[1], cz - si * a * h[0] + co * d * h[2]];
    const f = (i0, i1, i2, i3, k) => quad(i0, i1, i2, i3, c.clone().multiplyScalar(k));
    f(V(-1, -1, -1), V(1, -1, -1), V(1, 1, -1), V(-1, 1, -1), 0.8); f(V(-1, -1, 1), V(1, -1, 1), V(1, 1, 1), V(-1, 1, 1), 0.9);
    f(V(-1, -1, -1), V(-1, -1, 1), V(-1, 1, 1), V(-1, 1, -1), 0.7); f(V(1, -1, -1), V(1, -1, 1), V(1, 1, 1), V(1, 1, -1), 1.0);
    f(V(-1, 1, -1), V(1, 1, -1), V(1, 1, 1), V(-1, 1, 1), 1.1);
  }
  for (const t of torches) addBox(t.x, t.y - 0.15, t.z, 0.18, 0.5, 0.18, 0, WOOD);
  for (const r of rooms) if (r.kind === 'arena' || r.kind === 'boss' || r.kind === 'loot') {
    const q = r._r, n = r.kind === 'boss' ? 22 : 14;
    for (let i = 0; i < n; i++) {
      const x = (geoRng() * 2 - 1) * (q.hw - 1), z = q.z0 + 1 + geoRng() * (q.z1 - q.z0 - 2);
      if (geoRng() < 0.2) addBox(x, -DEPTH + 0.16, z, 0.3, 0.3, 0.3, geoRng() * 3, BONE);                            // skull
      else addBox(x, -DEPTH + 0.05, z, 0.7 + geoRng() * 0.4, 0.09, 0.1, geoRng() * 6.28, BONE);                      // femur
    }
  }
  const stoneGeo = new T.BufferGeometry();
  stoneGeo.setAttribute('position', new T.Float32BufferAttribute(P, 3)); stoneGeo.setAttribute('color', new T.Float32BufferAttribute(C, 3));
  const stoneMesh = new T.Mesh(stoneGeo, new T.MeshBasicMaterial({ vertexColors: true, side: T.DoubleSide }));
  stoneMesh.frustumCulled = false; stoneMesh.name = 'hub-stone'; group.add(stoneMesh);
  let tris = P.length / 9;

  // cobwebs (translucent corner quads)
  const W = [];
  const web = (cx, cy, cz, sx, sz, k) => { const a = 1.4 + geoRng() * 1.2; const A = [cx, cy, cz], B = [cx + sx * a, cy, cz], D = [cx, cy, cz + sz * a], Cc = [cx + sx * a * 0.55, cy - 0.9 * k, cz + sz * a * 0.55];
    W.push(...A, ...B, ...Cc, ...A, ...Cc, ...D); };
  for (const r of rooms) if (r.kind !== 'corridor') { const q = r._r, y = -DEPTH + q.h - 0.02, hw = q.hw - 0.02;
    web(-hw, y, q.z0 + 0.02, 1, 1, 1); web(hw, y, q.z0 + 0.02, -1, 1, 1); web(-hw, y, q.z1 - 0.02, 1, -1, 1); web(hw, y, q.z1 - 0.02, -1, -1, 1); }
  const webGeo = new T.BufferGeometry(); webGeo.setAttribute('position', new T.Float32BufferAttribute(W, 3));
  const webMesh = new T.Mesh(webGeo, new T.MeshBasicMaterial({ color: 0xc8cadc, transparent: true, opacity: 0.3, side: T.DoubleSide, depthWrite: false }));
  webMesh.frustumCulled = false; group.add(webMesh); tris += W.length / 9;

  // entrance glow: veil in the arch + ground halo (additive)
  const GP = [], GC = [];
  { const gv = (x, y, z, k) => { GP.push(x, y, z); GC.push(0.1 * k, 0.9 * k, 0.35 * k); };
    for (let i = 0; i < 6; i++) { const y0 = i * 1.15, y1 = (i + 1) * 1.15, k0 = 0.55 + 0.45 * Math.sin(Math.PI * Math.min(1, y0 / 7)), k1 = 0.55 + 0.45 * Math.sin(Math.PI * Math.min(1, y1 / 7));
      gv(-2.1, y0, 0, k0 * 0.5); gv(2.1, y0, 0, k0 * 0.5); gv(2.1, y1, 0, k1 * 0.5); gv(-2.1, y0, 0, k0 * 0.5); gv(2.1, y1, 0, k1 * 0.5); gv(-2.1, y1, 0, k1 * 0.5); }
    const N = 20, R = 9; for (let i = 0; i < N; i++) { const a0 = i / N * 6.2832, a1 = (i + 1) / N * 6.2832;
      GP.push(0, 0.12, 6, Math.cos(a0) * R, 0.12, 6 + Math.sin(a0) * R, Math.cos(a1) * R, 0.12, 6 + Math.sin(a1) * R);
      GC.push(0.12, 0.8, 0.35, 0, 0, 0, 0, 0, 0); } }
  const glowGeo = new T.BufferGeometry(); glowGeo.setAttribute('position', new T.Float32BufferAttribute(GP, 3)); glowGeo.setAttribute('color', new T.Float32BufferAttribute(GC, 3));
  const glowMat = new T.MeshBasicMaterial({ vertexColors: true, transparent: true, blending: T.AdditiveBlending, depthWrite: false, side: T.DoubleSide });
  const glowMesh = new T.Mesh(glowGeo, glowMat); glowMesh.frustumCulled = false; group.add(glowMesh); tris += GP.length / 9;

  // instanced: flames, cages, gates, chest bodies, chest lids
  const unit = new T.BoxGeometry(1, 1, 1);
  const flames = new T.InstancedMesh(unit, new T.MeshBasicMaterial({ color: 0xffa040 }), torches.length); flames.frustumCulled = false; group.add(flames);
  const cageGeo = (() => { const g = [], bar = (cx, cy, cz, sx, sy, sz) => { const b = new T.BoxGeometry(sx, sy, sz); b.translate(cx, cy, cz); g.push(b); };
    for (const a of [-1, 1]) for (const b of [-1, 1]) { bar(a * 0.5, b * 0.5, 0, 0.09, 0.09, 1.09); bar(a * 0.5, 0, b * 0.5, 0.09, 1.09, 0.09); bar(0, a * 0.5, b * 0.5, 1.09, 0.09, 0.09); }
    for (let i = -1; i <= 1; i += 1) { bar(0, i * 0.5, 0.5, 0.05, 0.05, 0.05); }
    bar(0, 0, 0, 0.3, 0.3, 0.3);
    const pos = [], col2 = []; let tcount = 0;
    for (const b of g) { const nb = b.toNonIndexed(), p = nb.attributes.position; for (let i = 0; i < p.count; i++) { pos.push(p.getX(i), p.getY(i), p.getZ(i)); const core = Math.abs(p.getX(i)) < 0.16 && Math.abs(p.getY(i)) < 0.16 && Math.abs(p.getZ(i)) < 0.16; col2.push(core ? 1 : 0.35, core ? 1 : 1, core ? 1 : 0.55); } tcount += p.count / 3; b.dispose(); nb.dispose(); }
    const geo = new T.BufferGeometry(); geo.setAttribute('position', new T.Float32BufferAttribute(pos, 3)); geo.setAttribute('color', new T.Float32BufferAttribute(col2, 3)); geo.userData.tris = tcount; return geo; })();
  const cageMat = new T.MeshBasicMaterial({ vertexColors: true, color: 0x66ff99 });
  const cageMesh = new T.InstancedMesh(cageGeo, cageMat, cages.length); cageMesh.frustumCulled = false; group.add(cageMesh);
  const gateMesh = new T.InstancedMesh(unit, new T.MeshBasicMaterial({ color: 0x3a2a2e }), gates.length); gateMesh.frustumCulled = false; group.add(gateMesh);
  const chestBody = new T.InstancedMesh(unit, new T.MeshBasicMaterial({ color: 0xffffff }), chests.length), chestLid = new T.InstancedMesh(unit, new T.MeshBasicMaterial({ color: 0xffffff }), chests.length);
  chestBody.frustumCulled = chestLid.frustumCulled = false; group.add(chestBody); group.add(chestLid);
  tris += 12 * (torches.length + gates.length + chests.length * 2) + cages.length * cageGeo.userData.tris;

  const M = new T.Matrix4(), Q = new T.Quaternion(), V1 = new T.Vector3(), V2 = new T.Vector3(), EU = new T.Euler(), CC = new T.Color();
  const cageState = cages.map(() => 1);
  function setCageMatrices() { cages.forEach((c, i) => { const s = cageState[i] * c.s; M.compose(V1.set(c.x, c.y, c.z), Q.identity(), V2.set(s, s, s)); cageMesh.setMatrixAt(i, M); }); cageMesh.instanceMatrix.needsUpdate = true; }
  function setChest(i) { const c = chests[i], lid = c.opened ? 1 : (c.lid || 0), x = c.pos[0], y = c.pos[1], z = c.pos[2];
    M.compose(V1.set(x, y + 0.35, z), Q.setFromAxisAngle(V2.set(0, 1, 0), c.face > 0 ? Math.PI / 2 : c.face < 0 ? -Math.PI / 2 : Math.PI), V2.set(1.1, 0.7, 0.75)); chestBody.setMatrixAt(i, M);
    // lid hinges at the back edge: rotate about local x at (y+0.7, back)
    const base = new T.Quaternion().setFromAxisAngle(new T.Vector3(0, 1, 0), c.face > 0 ? Math.PI / 2 : c.face < 0 ? -Math.PI / 2 : Math.PI);
    const hinge = new T.Vector3(0, 0.7, -0.375).applyQuaternion(base).add(new T.Vector3(x, y, z));
    const rot = base.clone().multiply(new T.Quaternion().setFromAxisAngle(new T.Vector3(1, 0, 0), -1.2 * lid));
    const center = new T.Vector3(0, 0.12, 0.375).applyQuaternion(rot).add(hinge);
    M.compose(center, rot, V2.set(1.1, 0.24, 0.75)); chestLid.setMatrixAt(i, M);
    chestBody.setColorAt(i, CC.set(c.boss ? 0x8a3cff : c.opened ? 0x3a2a1a : 0x7a4a22)); chestLid.setColorAt(i, CC.set(c.boss ? 0xb870ff : c.opened ? 0x4a3a2a : 0xa86a30));
    chestBody.instanceMatrix.needsUpdate = chestLid.instanceMatrix.needsUpdate = true; chestBody.instanceColor.needsUpdate = chestLid.instanceColor.needsUpdate = true; }
  const setGate = (i) => { const g = gates[i], k = g.open; M.compose(V1.set(0, -DEPTH + 2 - k * 2.05, g.z), Q.identity(), V2.set(2 * GW + 0.1, 4, 0.5)); gateMesh.setMatrixAt(i, M); gateMesh.instanceMatrix.needsUpdate = true; };
  const flameBase = torches.map((t) => ({ x: t.x + (t.side || 0) * 0.02, y: t.y + 0.28, z: t.z }));
  function flickerFlames(t) { torches.forEach((o, i) => { const f = 0.8 + 0.25 * Math.sin(t * 9 + o.ph) + 0.12 * Math.sin(t * 23 + o.ph * 2), b = flameBase[i];
    M.compose(V1.set(b.x, b.y + f * 0.05, b.z), Q.identity(), V2.set(0.2, 0.3 * f, 0.2)); flames.setMatrixAt(i, M); }); flames.instanceMatrix.needsUpdate = true; }
  setCageMatrices(); flickerFlames(0); gates.forEach((g, i) => { g.open = 1; setGate(i); }); chests.forEach((c, i) => setChest(i));

  // ── colliders ──
  const walls = [];
  const slab = (x0, y0, z0, x1, y1, z1, door) => { const w = { min: [x0, y0, z0], max: [x1, y1, z1], door: door || null, active: true }; walls.push(w); return w; };
  const T_ = 0.6;
  for (const r of rooms) {
    const q = r._r, y0 = -DEPTH, y1 = y0 + q.h;
    slab(-q.hw - T_, y0, q.z0, -q.hw, y1, q.z1); slab(q.hw, y0, q.z0, q.hw + T_, y1, q.z1);
    if (r.kind !== 'corridor') {
      const face = (z, gap) => { if (!gap) { slab(-q.hw - T_, y0, z - T_ / 2, q.hw + T_, y1, z + T_ / 2); return; }
        slab(-q.hw - T_, y0, z - T_ / 2, -GW, y1, z + T_ / 2); slab(GW, y0, z - T_ / 2, q.hw + T_, y1, z + T_ / 2); slab(-GW, y0 + 4, z - T_ / 2, GW, y1, z + T_ / 2); };
      face(q.z0, true); face(q.z1, r.id !== 'boss');
    }
  }
  slab(-GW - T_, -DEPTH, 0, -GW, 0.7, 24); slab(GW, -DEPTH, 0, GW + T_, 0.7, 24);
  for (const p of pillars) walls.push(p);
  const gateWall = {}; gates.forEach((g) => { gateWall[g.id] = slab(-GW, -DEPTH, g.z - 0.25, GW, -DEPTH + 4, g.z + 0.25, g.id); gateWall[g.id].active = false; });
  // entrance arch pillars as colliders
  slab(-3.5, 0, -0.7, -2.1, 7, 0.7); slab(2.1, 0, -0.7, 3.5, 7, 0.7);

  function floorAt(p) {
    const x = p.x != null ? p.x : p[0], z = p.z != null ? p.z : p[2];
    if (z >= 0 && z < 24 && Math.abs(x) <= GW) return -DEPTH * z / 24;
    for (let i = 0; i < rooms.length; i++) { const b = rooms[i].box; if (x >= b.min[0] && x <= b.max[0] && z >= b.min[2] && z < b.max[2]) return -DEPTH; }
    return null;
  }
  function roomAt(p) {
    const x = p.x != null ? p.x : p[0], y = p.y != null ? p.y : p[1], z = p.z != null ? p.z : p[2];
    for (let i = 0; i < rooms.length; i++) { const b = rooms[i].box; if (x >= b.min[0] && x <= b.max[0] && z >= b.min[2] && z < b.max[2] && y >= b.min[1] - 0.5 && y <= b.max[1]) return rooms[i]; }
    return null;
  }

  // ── waves / boss ──
  const kindsFor = (night) => { const k = MOB_KINDS.filter((m) => mobSpawnRule(m, { biome, lightLevel: 0, night: night || 1 }) > 0); return k.length ? k : ['zomby']; };
  const waveRng = mulberry(seed ^ 0xA7E4A);
  const waveTable = {};
  ['arena1', 'arena2', 'arena3'].forEach((id, a) => {
    const base = kindsFor(1), out = [];
    for (let w = 0; w < 3; w++) {
      const count = Math.min(8, 4 + a + w + (w === 2 ? 1 : 0)), kinds = [];
      for (let i = 0; i < count; i++) kinds.push(base[Math.floor(waveRng() * base.length)]);
      if (a === 2 && w === 2 && base.includes('ender')) kinds[0] = 'ender';
      out.push({ kinds, count, tier: a + 1, hpMul: +[1, 1.3, 1.6][w].toFixed(2) * (1 + a * 0.25) });
    }
    waveTable[id] = out;
  });
  const bossSpec = { kind: 'boss', name: 'WARDEN', plan: 'monolith', wave: 3 + (seed % 3), hp: 300, tier: 4, scaleH: 3, phases: BOSS_PHASES, telegraph: 0.8,
    spawn: [0, -DEPTH, roomById.boss._r.z1 - 6], drop: { blueprint: BLUEPRINTS[seed % BLUEPRINTS.length].id, coin: [100, 200], rare: 'mob-drop' } };
  waveTable.boss = [{ kinds: ['warden'], count: 1, tier: 4, hpMul: 1, boss: bossSpec }, { kinds: kindsFor(1).slice(0, 2).concat(kindsFor(1)[0]), count: 3, tier: 4, hpMul: 1.6, trigger: 'phase:summon' }];

  // ── state / persistence ──
  const state = { level: opts.level | 0, cleared: {} };
  function applyGate(id, openFlag) { const i = gates.findIndex((g) => g.id === id); if (i < 0) return; gates[i].target = openFlag ? 1 : 0; gateWall[id].active = !openFlag; }
  function setCleared(id) {
    const r = roomById[id]; if (!r) return; state.cleared[id] = 1; r.cleared = true; r.doors.forEach((d) => applyGate(d, true));
    if (id === 'boss') chests.forEach((c, i) => { if (c.boss) { c.locked = false; } });
    const ci = cages.findIndex((c) => c.room === id); if (ci >= 0) cageState[ci] = 0;
    setCageMatrices(); locked = locked === id ? null : locked;
  }
  let locked = null, current = null;
  function open(id) {
    const i = chests.findIndex((c) => c.id === id), c = chests[i]; if (!c || c.opened || c.locked) return null;
    c.opened = true; c.lid = 0; c.t = 0; setChest(i); return c.loot.map((s) => Object.assign({}, s));
  }
  function serialize() { return { v: 1, seed, level: state.level, cleared: Object.keys(state.cleared), opened: chests.filter((c) => c.opened).map((c) => c.id) }; }
  function restore(o) {
    if (!o || o.v !== 1 || o.seed !== seed) return false;
    state.level = o.level | 0; (o.cleared || []).forEach((id) => setCleared(id));
    chests.forEach((c, i) => { c.opened = (o.opened || []).includes(c.id); setChest(i); }); return true;
  }
  const events = [];
  function update(t, dt, pp) {
    const px = pp ? (pp.x != null ? pp.x : pp[0]) : 0, py = pp ? (pp.y != null ? pp.y : pp[1]) : 0, pz = pp ? (pp.z != null ? pp.z : pp[2]) : 0;
    if (pp && (Math.abs(px) > 200 || pz < -200 || pz > 400)) return null;
    flickerFlames(t); cageMat.color.setRGB(0.6 + 0.4 * Math.sin(t * 3), 1, 0.7 + 0.3 * Math.sin(t * 3 + 1)); glowMat.opacity = 0.75 + 0.25 * Math.sin(t * 1.7);
    let ev = null;
    const rm = pp ? roomAt({ x: px, y: py, z: pz }) : null, id = rm ? rm.id : null;
    if (id !== current) { current = id; if (id) ev = { type: 'enter', room: id }; }
    if (rm && GATED[rm.id] && !rm.cleared && !locked && pz > rm._r.z0 + 2.2 && pz < rm._r.z1 - 1.5) {
      locked = rm.id; rm.doors.forEach((d) => applyGate(d, false)); ev = { type: 'lock', room: rm.id };
    }
    for (let i = 0; i < gates.length; i++) { const g = gates[i], d = g.target - g.open; if (Math.abs(d) > 1e-3) { g.open += Math.sign(d) * Math.min(Math.abs(d), dt * 1.5); setGate(i); } }
    for (let i = 0; i < chests.length; i++) { const c = chests[i]; if (c.opened && c.lid < 1) { c.lid = Math.min(1, (c.lid || 0) + dt * 2.5); setChest(i); } }
    return ev;
  }
  function dispose() { [stoneGeo, webGeo, glowGeo, cageGeo, unit].forEach((g) => g.dispose()); group.traverse((o) => { if (o.material) o.material.dispose(); }); if (group.parent) group.parent.remove(group); }

  const interior = { rooms: rooms.map((r) => { delete r._rr; return r; }), walls, floorAt, roomAt, spawn: [0, -DEPTH, 27], exit: [0, 0.2, -3] };
  const hub = {
    group, anchor, biome, entrance: { pos: [0, 0, 0], radius: 3.5 }, interior, bossSpec, chests, lights: { torches, count: torches.length }, ambience: 'hub',
    state, waves: (id) => (waveTable[id] || []).map((w) => Object.assign({}, w)), spawns: (id) => (spawnPts[id] || []).map((p) => p.slice()),
    open, setCleared, serialize, restore, update, dispose, floorAt, roomAt, stats: { tris: Math.round(tris), draws: 9 },
    toHub(v, out) { out = out || new T.Vector3(); return group.worldToLocal(out.copy(v)); }, onRoom: null,
  };
  return hub;
}

// ── dev view: ?hub on any page that imports this module ──
try {
  if (typeof location !== 'undefined' && /[?&]hub(&|$)/.test(location.search) && typeof document !== 'undefined') {
    import('three').then((THREE) => {
      const W = innerWidth, H = innerHeight, cv = document.createElement('canvas');
      cv.style.cssText = 'position:fixed;inset:0;width:100%;height:100%;z-index:99999;background:#0b0710';
      document.body.appendChild(cv);
      const r = new THREE.WebGLRenderer({ canvas: cv, antialias: true }); r.setSize(W, H); r.setClearColor(0x0b0710);
      const sc = new THREE.Scene(), cam = new THREE.PerspectiveCamera(60, W / H, 0.05, 600);
      const ground = new THREE.Mesh(new THREE.PlaneGeometry(400, 400).rotateX(-Math.PI / 2), new THREE.MeshBasicMaterial({ color: 0x1a1426 })); ground.position.y = 0.02; sc.add(ground);
      const hub = createHub({ THREE }, 1, { planetNode: { name: 'Mockia', radius: 0, biome: 'icy' }, seed: 'mock-1' });
      hub.group.position.set(0, 0, 0); hub.group.quaternion.identity(); sc.add(hub.group);
      const views = { entrance: [[0, 16, -22], [0, 3, 4]], arena: [[-8, -DEPTH + 3.5, 44], [0, -DEPTH + 1.5, 62]], boss: [[0, -DEPTH + 6, 141], [0, -DEPTH + 3, 168]], loot: [[0, -DEPTH + 2.5, 95.5], [-4, -DEPTH + 0.5, 100]] };
      let t = 0;
      const view = (n) => { const v = views[n]; cam.position.set(...v[0]); cam.lookAt(...v[1]); hub.update(t += 0.016, 0.016, null); r.render(sc, cam); };
      window.__hub = { hub, view, THREE, tris: hub.stats.tris, calls: () => r.info.render.calls };
      view('entrance');
    });
  }
} catch (e) { /* dev view only */ }
