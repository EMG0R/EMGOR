// ship-village.js — VILLAGES + GOLUM guardians (docs/HANDOFF.md "Villages"). Pure sim + two merged meshes, no DOM.
//
//   createVillage(engine, u, { planetNode, L }) -> village | null      (null on gas / icy / exotic worlds, or when no flat site exists)
//   u = world units per metre (ship.js passes GM * L). Units inside: village METRES, +y up, origin = the well; village.group is scaled by u, positioned at
//   village.anchor.pos (planet-local) and rotated so +y = up. Caller parents village.group into the planet ride group (same as ship-hub.js).
//     planetNode: { id|name, radius, biome ('lush'|'rocky' only), floor(x,y,z)->radius (analytic), live(x,y,z)->radius (rendered mesh; optional),
//                   land(x,y,z)->bool, avoid:[unit Vector3 dirs] (first outpost: the site is >= 0.3 rad away) }.  L = ship length in world units (40 L humans radius).
//   Seeded by the planet id -> every client finds the same site, huts, villagers, trades. Villager routines come from the shared wall clock (ctx.t), so no netcode.
//   Golum vs mob fights are local to each client (golums act on the mobs THIS client sees); the player's own hits on villagers are local too.
//
//   village.anchor { dir, up, pos }      village.radius  (m, outer fence)       village.name
//   village.huts [{ id, tmpl, center:[x,z], box:{min,max}, door:{pos:[x,y,z], front:[x,z], side}, walls:[box] }]
//   box = { name, min:Vector3, max:Vector3, solid:true, indestructible:true }  in VILLAGE metres. village.walls = every solid box (hut walls + roof slabs + well ring).
//   village.solid { m, inv, K, hw, hd, hh, a, b }   exactly the entry ship.js' objBegin() hands to objPush (m = village metres -> planet-local).
//   village.well { pos:[x,y,z], box:[...] }   village.bell { pos:[x,y,z] }   village.overlaps(planetLocalPos, worldRadius) -> bool (wall within r: STEEV placement refuses)
//   village.villagers [{ id, name, role: farmer|trader|elder|kid, seed, home (hut index), lines[3], trades?, pos (live, village m), human, inside }]
//   village.golums [{ id, hp, hpMax, pos (live), target (attacker / mob id string | null), say }]   village.threat { id, t } | null
//   village.update(dt, ctx)   ctx = { t (shared clock s), night (0..1), player: Vector3 planet-local feet | null, known: Set, mobs: [{ id, ref, pos (planet-local) }],
//                              hurtMob(ref, dmg), hurtPlayer(dmg) }  -> village.events [{ type:'greet', v, text } | { type:'golum', text }]  (ship.js drains with village.drain())
//   village.onHit(villagerId, attackerId)   attackerId 'me' | 'mob:<id>' : the villager flees home, every golum targets the attacker for 30 s.
//   village.punchHit(pPlanetLocal, fwdPlanetLocal, reachWorld, dmg) -> { type:'villager'|'golum', id } | null   (the player's fist; villager hits call onHit('me'))
//   village.nearVillager(pPlanetLocal, r) -> villager | null      village.market(trader) -> { buys, buyPrices, sells } (ship-space makeMarket shape, live stock)
//   village.scanTargets() -> [{ type:'village', id, kind:'village', name, pos (planet-local), color }]    village.poi { kind:'village', name, pos }
//   village.serialize() / restore(o)   (trade stock per UTC day)       village.stats { tris, draws }       village.toParent(v, out) / toLocal(p, out)
//   village.setNight(n)  dims the day mesh / brightens windows + torches.  Draw calls: 2 merged meshes (+2 per live human within 40 L, +5 per golum).
import { createHuman } from './ship-human.js';
import { compose, makeMaterials, mergeLit, mulberry as pmul } from './ship-parts.js';
import { storeMenu, resourceItem, RESOURCE_BASE } from './ship-items.js';
import * as lingo from './ship-lingo.js';

function mulberry(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }

const RP = 7;                     // plaza ring radius (villagers walk it)
const TW = 0.5, DOOR_W = 1.7, DOOR_H = 2.5, FOUND = 1.6;     // wall thickness, door gap, foundation depth
const TMPL = [{ id: 'cottage', w: 6, d: 5, h: 3.1, roof: 'gable' }, { id: 'longhouse', w: 9, d: 5, h: 3.1, roof: 'gable' }, { id: 'tower', w: 5, d: 5, h: 4.6, roof: 'pyramid' }];
const ROLE_COL = { farmer: 0x8FD06A, trader: 0xFFD36A, elder: 0xB08CFF, kid: 0xFF8AE0 };
const WALK = 2.6, GOLUM_V = 3.1, GOLUM_HP = 600, GOLUM_DMG_MOB = 45, GOLUM_DMG_ME = 12, VILL_ACT_L = 40;

export function createVillage(engine, u, opts) {
  opts = opts || {};
  const T = engine.THREE, pn = opts.planetNode || {};
  if (!(u > 0)) return null;
  const biome = pn.biome;
  if (biome !== 'lush' && biome !== 'rocky') return null;
  const pid = String(pn.id != null ? pn.id : (pn.name || 'planet'));
  const seed = hashStr('village:' + pid), rng = mulberry(seed ^ 0x51AC3);
  const Lw = opts.L > 0 ? opts.L : u / 0.05;
  const floorFn = typeof pn.floor === 'function' ? pn.floor : null, liveFn = typeof pn.live === 'function' ? pn.live : floorFn, landFn = typeof pn.land === 'function' ? pn.land : () => true;
  if (!floorFn) return null;
  const vec = (x, y, z) => new T.Vector3(x, y, z);

  // ── site: flattest seeded land direction >= 0.3 rad from the first outpost; a tangent frame (X, Y = up, Z) at it ──
  const R0 = pn.radius || 1, avoid = pn.avoid || [];
  let best = null;
  const sr = mulberry(seed ^ 0x517E);
  const ringPts = [[14, 8], [24, 10], [34, 12]];
  for (let k = 0; k < 220; k++) {
    const z = (sr() * 2 - 1) * 0.85, ph = sr() * 6.2831853, q = Math.sqrt(1 - z * z), d = vec(q * Math.cos(ph), z, q * Math.sin(ph));
    let ok = true; for (let i = 0; i < avoid.length; i++) if (Math.acos(Math.max(-1, Math.min(1, d.dot(avoid[i])))) < 0.3) { ok = false; break; }
    if (!ok || !landFn(d.x, d.y, d.z)) continue;
    const r0 = floorFn(d.x, d.y, d.z); if (!(r0 > 0)) continue;
    const ref = Math.abs(d.y) < 0.9 ? vec(0, 1, 0) : vec(1, 0, 0), ex = new T.Vector3().crossVectors(d, ref).normalize(), ez = new T.Vector3().crossVectors(ex, d);
    let lo = 0, hi = 0, wet = false;
    for (const [rr, nn] of ringPts) {
      for (let j = 0; j < nn && !wet; j++) {
        const a = j / nn * 6.2831853, px = Math.cos(a) * rr, pz = Math.sin(a) * rr;
        const p = d.clone().multiplyScalar(r0).addScaledVector(ex, px * u).addScaledVector(ez, pz * u), l = p.length(); p.divideScalar(l);
        if (!landFn(p.x, p.y, p.z)) { wet = true; break; }
        const y = (floorFn(p.x, p.y, p.z) * p.dot(d) - r0) / u; if (y < lo) lo = y; if (y > hi) hi = y;
      }
    }
    if (wet) continue;
    const dev = hi - lo; if (!best || dev < best.dev) { best = { d, r0, ex, ez, dev }; if (dev < 2.2) break; }
  }
  if (!best || best.dev > 7) return null;
  const up = best.d.clone(), X = best.ex.clone(), Z = best.ez.clone();
  const anchor = { dir: up, up: up.clone(), pos: up.clone().multiplyScalar(best.r0) };
  const group = new T.Group(); group.name = 'ship-village';
  group.position.copy(anchor.pos); group.scale.setScalar(u); group.quaternion.setFromRotationMatrix(new T.Matrix4().makeBasis(X, up, Z));
  group.updateMatrix();
  const MAT = group.matrix.clone(), INV = MAT.clone().invert();
  const tA = vec(0, 0, 0), tB = vec(0, 0, 0);
  const toParent = (v, out) => out.copy(v).applyMatrix4(MAT);
  const toLocal = (p, out) => out.copy(p).applyMatrix4(INV);
  function groundAt(x, z, fn) {      // village-metre height of the surface under (x, z)
    tA.copy(anchor.pos).addScaledVector(X, x * u).addScaledVector(Z, z * u); const l = tA.length(); tA.divideScalar(l);
    const r = (fn || floorFn)(tA.x, tA.y, tA.z); tB.copy(tA).multiplyScalar(r).sub(anchor.pos);
    return tB.dot(up) / u;
  }

  // ── geometry buffers (vertex-coloured, flat shaded by baked normals) ──
  const LP = [], LC = [], EP = [], EC = [];
  const LDIR = vec(-0.45, 0.8, 0.35).normalize(), col = new T.Color();
  const lush = biome === 'lush';
  const WALL = lush ? [0.62, 0.5, 0.34] : [0.43, 0.41, 0.48], ROOF = lush ? [0.36, 0.2, 0.55] : [0.3, 0.17, 0.26], WOOD = [0.22, 0.14, 0.09], PATH = lush ? [0.56, 0.47, 0.33] : [0.36, 0.34, 0.4];
  const SOIL = [0.25, 0.16, 0.1], CROP = lush ? [0.3, 0.7, 0.28] : [0.5, 0.35, 0.7], STONE = [0.5, 0.5, 0.56];
  const sh = (nx, ny, nz) => 0.5 + 0.5 * Math.max(0, nx * LDIR.x + ny * LDIR.y + nz * LDIR.z);
  function tri(P, C, a, b, c, cc, nx, ny, nz, k) {
    const s = sh(nx, ny, nz) * (k || 1);
    for (const v of [a, b, c]) { P.push(v[0], v[1], v[2]); C.push(cc[0] * s, cc[1] * s, cc[2] * s); }
  }
  function quad(P, C, a, b, c, d, cc, n, k) { tri(P, C, a, b, c, cc, n[0], n[1], n[2], k); tri(P, C, a, c, d, cc, n[0], n[1], n[2], k); }
  function boxG(P, C, x0, y0, z0, x1, y1, z1, cc, bottom) {
    const k = 0.92 + rng() * 0.16;
    quad(P, C, [x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1], cc, [0, 0, 1], k); quad(P, C, [x1, y0, z0], [x0, y0, z0], [x0, y1, z0], [x1, y1, z0], cc, [0, 0, -1], k);
    quad(P, C, [x1, y0, z1], [x1, y0, z0], [x1, y1, z0], [x1, y1, z1], cc, [1, 0, 0], k); quad(P, C, [x0, y0, z0], [x0, y0, z1], [x0, y1, z1], [x0, y1, z0], cc, [-1, 0, 0], k);
    quad(P, C, [x0, y1, z1], [x1, y1, z1], [x1, y1, z0], [x0, y1, z0], cc, [0, 1, 0], k);
    if (bottom) quad(P, C, [x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1], cc, [0, -1, 0], k);
  }
  const mkBox = (name, x0, y0, z0, x1, y1, z1) => ({ name, min: vec(x0, y0, z0), max: vec(x1, y1, z1), solid: true, indestructible: true });
  const tint = (c, k) => [c[0] * k, c[1] * k, c[2] * k];
  const walls = [];

  // ── layout ──
  const wellY = Math.max(groundAt(-1.5, -1.5), groundAt(1.5, -1.5), groundAt(-1.5, 1.5), groundAt(1.5, 1.5), groundAt(0, 0));
  const nHuts = 5 + Math.floor(rng() * 5);
  const huts = [], occupied = [{ x0: -RP - 1, x1: RP + 1, z0: -RP - 1, z1: RP + 1 }];
  const hit = (a, b, pad) => a.x0 - pad < b.x1 && a.x1 + pad > b.x0 && a.z0 - pad < b.z1 && a.z1 + pad > b.z0;
  for (let i = 0; i < nHuts; i++) {
    const inner = i % 2 === 0, base = (i / nHuts) * 6.2831853;
    let placed = null, fallback = null;
    for (let tries = 0; tries < 18 && !placed; tries++) {
      const a = base + (rng() - 0.5) * 0.5, rr = (inner ? 17 : 27) + (rng() - 0.5) * 3, cx = Math.cos(a) * rr, cz = Math.sin(a) * rr, tm = TMPL[Math.floor(rng() * 3)];
      const side = Math.abs(cx) > Math.abs(cz) ? (cx > 0 ? 'nx' : 'px') : (cz > 0 ? 'nz' : 'pz');      // door faces the well (axis aligned: AABB walls)
      const swap = side === 'px' || side === 'nx';
      const hx = (swap ? tm.d : tm.w) / 2, hz = (swap ? tm.w : tm.d) / 2, rect = { x0: cx - hx, x1: cx + hx, z0: cz - hz, z1: cz + hz };
      if (occupied.some((o) => hit(rect, o, 2.5))) continue;
      let lo = 1e9, hi = -1e9;
      for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1], [0, 0]]) { const y = groundAt(cx + sx * hx, cz + sz * hz); if (y < lo) lo = y; if (y > hi) hi = y; }
      const cand = { cx, cz, hx, hz, side, tm, dev: hi - lo, rect, hi };
      if (hi - lo <= 1.3) { placed = cand; break; }
      if (!fallback || cand.dev < fallback.dev) fallback = cand;
    }
    placed = placed || fallback; if (!placed) continue;
    occupied.push(placed.rect); placed.idx = huts.length; huts.push(placed);
  }
  // well + plots + stall
  const plots = [];
  for (let k = 0, tries = 0; k < 3 && tries < 60; tries++) {
    const a = rng() * 6.2831853, rr = 9.5 + rng() * 2, cx = Math.cos(a) * rr, cz = Math.sin(a) * rr, rect = { x0: cx - 3, x1: cx + 3, z0: cz - 2.2, z1: cz + 2.2 };
    if (occupied.some((o) => hit(rect, o, 1.2))) continue;
    occupied.push(rect); plots.push({ cx, cz, a, rect }); k++;
  }
  let stall = null;
  for (let tries = 0; tries < 60 && !stall; tries++) {
    const a = rng() * 6.2831853, rr = 10.5, cx = Math.cos(a) * rr, cz = Math.sin(a) * rr, rect = { x0: cx - 2, x1: cx + 2, z0: cz - 1.4, z1: cz + 1.4 };
    if (occupied.some((o) => hit(rect, o, 0.8))) continue;
    occupied.push(rect); stall = { cx, cz, a, rect };
  }
  if (!stall) { stall = { cx: 0, cz: 10.5, a: Math.PI / 2, rect: { x0: -2, x1: 2, z0: 9.1, z1: 11.9 } }; }

  // ── build: huts ──
  const torches = [];
  const door = (h) => {      // door centre on the wall + the point just outside it (toward the well)
    const d = h.side === 'px' ? [1, 0] : h.side === 'nx' ? [-1, 0] : h.side === 'pz' ? [0, 1] : [0, -1];
    const dx = h.cx + d[0] * h.hx, dz = h.cz + d[1] * h.hz;
    return { d, c: [dx, dz], front: [dx + d[0] * 1.6, dz + d[1] * 1.6] };
  };
  for (const h of huts) {
    const tm = h.tm, dr = door(h), hx = h.hx, hz = h.hz, cx = h.cx, cz = h.cz;
    const yb = Math.max(h.hi, groundAt(dr.front[0], dr.front[1]), groundAt(cx, cz)), y0 = yb - FOUND, y1 = yb + tm.h, wc = tint(WALL, 0.9 + rng() * 0.2);
    h.yb = yb; h.door = dr;
    const hw = [];
    const wb = (nm, x0, z0, x1, z1, ya, yc) => { const b = mkBox('hut' + h.idx + '-' + nm, x0, ya == null ? y0 : ya, z0, x1, yc == null ? y1 : yc, z1); hw.push(b); walls.push(b); boxG(LP, LC, x0, b.min.y, z0, x1, b.max.y, z1, wc); return b; };
    const doorOn = (s) => h.side === s;
    // z walls (full width) and x walls (between them); the door side is split around the gap + a lintel
    const zWall = (nm, zA, zB, isDoor) => {
      if (!isDoor) { wb(nm, cx - hx, zA, cx + hx, zB); return; }
      wb(nm + 'a', cx - hx, zA, cx - DOOR_W / 2, zB); wb(nm + 'b', cx + DOOR_W / 2, zA, cx + hx, zB); wb(nm + 'l', cx - DOOR_W / 2, zA, cx + DOOR_W / 2, zB, yb + DOOR_H, y1);
    };
    const xWall = (nm, xA, xB, isDoor) => {
      if (!isDoor) { wb(nm, xA, cz - hz + TW, xB, cz + hz - TW); return; }
      wb(nm + 'a', xA, cz - hz + TW, xB, cz - DOOR_W / 2); wb(nm + 'b', xA, cz + DOOR_W / 2, xB, cz + hz - TW); wb(nm + 'l', xA, cz - DOOR_W / 2, xB, cz + DOOR_W / 2, yb + DOOR_H, y1);
    };
    zWall('n', cz - hz, cz - hz + TW, doorOn('nz')); zWall('s', cz + hz - TW, cz + hz, doorOn('pz'));
    xWall('w', cx - hx, cx - hx + TW, doorOn('nx')); xWall('e', cx + hx - TW, cx + hx, doorOn('px'));
    const slab = mkBox('hut' + h.idx + '-roof', cx - hx - 0.1, y1, cz - hz - 0.1, cx + hx + 0.1, y1 + 0.35, cz + hz + 0.1); hw.push(slab); walls.push(slab);
    // corner posts + door frame (visual)
    const post = tint(WOOD, 1);
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) boxG(LP, LC, sx > 0 ? cx + hx - 0.3 : cx - hx - 0.06, y0, sz > 0 ? cz + hz - 0.3 : cz - hz - 0.06, sx > 0 ? cx + hx + 0.06 : cx - hx + 0.3, y1 + 0.05, sz > 0 ? cz + hz + 0.06 : cz - hz + 0.3, post);
    const fr = 0.18;
    if (h.side === 'pz' || h.side === 'nz') { const zz = h.side === 'pz' ? cz + hz : cz - hz; boxG(LP, LC, cx - DOOR_W / 2 - fr, yb, zz - 0.35, cx - DOOR_W / 2, yb + DOOR_H, zz + 0.35, post); boxG(LP, LC, cx + DOOR_W / 2, yb, zz - 0.35, cx + DOOR_W / 2 + fr, yb + DOOR_H, zz + 0.35, post); }
    else { const xx = h.side === 'px' ? cx + hx : cx - hx; boxG(LP, LC, xx - 0.35, yb, cz - DOOR_W / 2 - fr, xx + 0.35, yb + DOOR_H, cz - DOOR_W / 2, post); boxG(LP, LC, xx - 0.35, yb, cz + DOOR_W / 2, xx + 0.35, yb + DOOR_H, cz + DOOR_W / 2 + fr, post); }
    // windows: glowing squares on the two side walls (not the door wall)
    const WC = [1.5, 1.1, 0.55];
    const wq = (x, y, z, nx, nz) => { const a = nx ? [0, 0.45] : [0.45, 0], b = [0, 0.45]; const P = EP, C = EC; const q = [[x - a[0], y - 0.4, z - a[1]], [x + a[0], y - 0.4, z + a[1]], [x + a[0], y + 0.4, z + a[1]], [x - a[0], y + 0.4, z - a[1]]]; quad(P, C, q[0], q[1], q[2], q[3], WC, [nx, 0, nz], 1.0); };
    const eps = 0.03;
    if (h.side === 'px' || h.side === 'nx') { wq(cx, yb + 1.7, cz - hz - eps, 0, -1); wq(cx, yb + 1.7, cz + hz + eps, 0, 1); }
    else { wq(cx - hx - eps, yb + 1.7, cz, -1, 0); wq(cx + hx + eps, yb + 1.7, cz, 1, 0); }
    // roof
    const ov = 0.7, rc = tint(ROOF, 0.9 + rng() * 0.2), ax = hx >= hz, ha = (ax ? hx : hz) + ov, hb = (ax ? hz : hx) + ov, yR = y1 + 0.35, rh = tm.roof === 'pyramid' ? hb * 0.95 : hb * 0.72;
    const pt = (a, b, y) => (ax ? [cx + a, y, cz + b] : [cx + b, y, cz + a]);
    const rn = (p0, p1, p2) => { const ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2], vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2]; let nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx; const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l; const mx = (p0[0] + p1[0] + p2[0]) / 3 - cx, mz = (p0[2] + p1[2] + p2[2]) / 3 - cz; if (nx * mx + nz * mz + ny * 0.2 < 0) { nx = -nx; ny = -ny; nz = -nz; } return [nx, ny, nz]; };
    const rq = (p0, p1, p2, p3, k) => { quad(LP, LC, p0, p1, p2, p3, rc, rn(p0, p1, p2), k); };
    if (tm.roof === 'pyramid') {
      const apex = [cx, yR + rh, cz], c4 = [[cx - hx - ov, yR, cz - hz - ov], [cx + hx + ov, yR, cz - hz - ov], [cx + hx + ov, yR, cz + hz + ov], [cx - hx - ov, yR, cz + hz + ov]];
      for (let i = 0; i < 4; i++) { const a = c4[i], b = c4[(i + 1) % 4]; tri(LP, LC, a, b, apex, rc, ...rn(a, b, apex), 0.9 + (i % 2) * 0.12); }
    } else {
      const r0 = pt(-ha, -hb, yR), r1 = pt(ha, -hb, yR), r2 = pt(ha, hb, yR), r3 = pt(-ha, hb, yR), t0 = pt(-ha, 0, yR + rh), t1 = pt(ha, 0, yR + rh);
      rq(r0, r1, t1, t0, 1.0); rq(r2, r3, t0, t1, 0.88);
      const g0 = [r3, r0, t0], g1 = [r1, r2, t1];
      tri(LP, LC, g0[0], g0[1], g0[2], tint(wc, 0.9), ...rn(g0[0], g0[1], g0[2]), 1); tri(LP, LC, g1[0], g1[1], g1[2], tint(wc, 0.9), ...rn(g1[0], g1[1], g1[2]), 1);
    }
    // chimney on the cottage / longhouse
    if (tm.roof === 'gable') boxG(LP, LC, cx + (ax ? ha * 0.5 : hb * 0.4) - 0.3, yR + 0.2, cz + (ax ? -hb * 0.35 : ha * 0.5) - 0.3, cx + (ax ? ha * 0.5 : hb * 0.4) + 0.3, yR + rh + 1.1, cz + (ax ? -hb * 0.35 : ha * 0.5) + 0.3, tint(STONE, 0.8));
    // door torches (a post + flame each side of the door front)
    const pdx = -dr.d[1], pdz = dr.d[0];
    for (const s of [-1, 1]) torches.push({ x: dr.front[0] + pdx * 1.5 * s + dr.d[0] * 0.2, z: dr.front[1] + pdz * 1.5 * s + dr.d[1] * 0.2 });
    h.walls = hw;
    h.boxOut = { min: [cx - hx, y0, cz - hz], max: [cx + hx, y1 + 0.35, cz + hz] };
  }

  // well: ring walls (solid), posts, roof, water + bell
  { const wy0 = wellY - 0.4, wy1 = wellY + 1.1, sc = tint(STONE, 1);
    const wb = (nm, x0, z0, x1, z1) => { const b = mkBox('well-' + nm, x0, wy0, z0, x1, wy1, z1); walls.push(b); boxG(LP, LC, x0, wy0, z0, x1, wy1, z1, sc); return b; };
    const w1 = wb('n', -1.5, -1.5, 1.5, -0.8), w2 = wb('s', -1.5, 0.8, 1.5, 1.5), w3 = wb('w', -1.5, -0.8, -0.8, 0.8), w4 = wb('e', 0.8, -0.8, 1.5, 0.8);
    var wellBoxes = [w1, w2, w3, w4];
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) boxG(LP, LC, sx * 1.35 - 0.12, wy1, sz * 1.35 - 0.12, sx * 1.35 + 0.12, wellY + 3.2, sz * 1.35 + 0.12, tint(WOOD, 1));
    boxG(LP, LC, -1.5, wellY + 3.0, -0.12, 1.5, wellY + 3.25, 0.12, tint(WOOD, 1.15));
    const apex = [0, wellY + 4.5, 0], c4 = [[-2.0, wellY + 3.2, -2.0], [2.0, wellY + 3.2, -2.0], [2.0, wellY + 3.2, 2.0], [-2.0, wellY + 3.2, 2.0]], rc = tint(ROOF, 1);
    for (let i = 0; i < 4; i++) { const a = c4[i], b = c4[(i + 1) % 4]; const nx = (a[0] + b[0]) / 2, nz = (a[2] + b[2]) / 2, l = Math.hypot(nx, nz) || 1; tri(LP, LC, a, b, apex, rc, nx / l * 0.6, 0.8, nz / l * 0.6, 0.9 + (i % 2) * 0.12); }
    quad(LP, LC, [-0.8, wellY + 0.55, -0.8], [0.8, wellY + 0.55, -0.8], [0.8, wellY + 0.55, 0.8], [-0.8, wellY + 0.55, 0.8], [0.3, 0.75, 1.0], [0, 1, 0], 1.25);
    boxG(LP, LC, -0.16, wellY + 2.15, -0.14, 0.16, wellY + 2.75, 0.14, [1.0, 0.82, 0.3]);       // the bell
  }
  const bell = { pos: [0, wellY + 2.4, 0] };

  // market stall (solid counter + awning)
  { const sx = stall.cx, sz = stall.cz, sy = Math.max(groundAt(sx - 2, sz), groundAt(sx + 2, sz), groundAt(sx, sz)), wc = tint(WOOD, 1.3);
    const b = mkBox('stall', sx - 1.6, sy - 0.8, sz - 0.5, sx + 1.6, sy + 1.0, sz + 0.5); walls.push(b); boxG(LP, LC, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, wc);
    for (const k of [-1, 1]) boxG(LP, LC, sx + k * 1.7 - 0.1, sy, sz - 0.1 + (stall.cz > 0 ? -0.9 : 0.9), sx + k * 1.7 + 0.1, sy + 2.5, sz + 0.1 + (stall.cz > 0 ? -0.9 : 0.9), tint(WOOD, 1));
    const zA = sz + (stall.cz > 0 ? -1.6 : 0.4), zB = zA + 1.2; boxG(LP, LC, sx - 2.0, sy + 2.5, zA, sx + 2.0, sy + 2.75, zB + 0.6, tint(ROOF, 1.2));
    stall.y = sy;
    stall.stand = [stall.cx - Math.cos(stall.a) * 1.8, stall.cz - Math.sin(stall.a) * 1.8];       // the trader stands on the well side of the counter
    for (const k of [-1, 1]) boxG(EP, EC, sx + k * 1.1 - 0.12, sy + 1.0, sz - 0.12, sx + k * 1.1 + 0.12, sy + 1.25, sz + 0.12, [1.4, 0.95, 0.4]);
  }
  // farm plots + fences
  for (const p of plots) {
    const y = Math.max(groundAt(p.cx - 2.8, p.cz - 2), groundAt(p.cx + 2.8, p.cz + 2), groundAt(p.cx, p.cz)) + 0.12; p.y = y;
    boxG(LP, LC, p.cx - 2.8, y - 0.5, p.cz - 2, p.cx + 2.8, y, p.cz + 2, SOIL);
    for (let r = 0; r < 4; r++) for (let c = 0; c < 8; c++) { const x = p.cx - 2.4 + c * 0.68, z = p.cz - 1.5 + r * 0.9, hh = 0.35 + rng() * 0.35; boxG(LP, LC, x - 0.14, y, z - 0.14, x + 0.14, y + hh, z + 0.14, tint(CROP, 0.8 + rng() * 0.4)); }
    const fx0 = p.cx - 3.1, fx1 = p.cx + 3.1, fz0 = p.cz - 2.4, fz1 = p.cz + 2.4, wd = tint(WOOD, 1.1);
    const fp = (x, z) => { const g = groundAt(x, z); boxG(LP, LC, x - 0.08, g - 0.2, z - 0.08, x + 0.08, g + 1.05, z + 0.08, wd); return g; };
    for (let x = fx0; x <= fx1 + 0.01; x += 1.55) { const g0 = fp(x, fz0), g1 = fp(x, fz1); void g0; void g1; }
    for (let z = fz0 + 1.6; z < fz1 - 0.1; z += 1.6) { fp(fx0, z); fp(fx1, z); }
    for (const yy of [0.45, 0.85]) {
      boxG(LP, LC, fx0, y + yy - 0.04, fz0 - 0.03, fx1, y + yy + 0.04, fz0 + 0.03, wd); boxG(LP, LC, fx0, y + yy - 0.04, fz1 - 0.03, fx1, y + yy + 0.04, fz1 + 0.03, wd);
      boxG(LP, LC, fx0 - 0.03, y + yy - 0.04, fz0, fx0 + 0.03, y + yy + 0.04, fz1, wd); boxG(LP, LC, fx1 - 0.03, y + yy - 0.04, fz0, fx1 + 0.03, y + yy + 0.04, fz1, wd);
    }
  }
  // plaza torches
  for (let k = 0; k < 4; k++) { const a = Math.PI / 4 + k * Math.PI / 2; torches.push({ x: Math.cos(a) * 5.8, z: Math.sin(a) * 5.8 }); }
  for (const t of torches) {
    const g = groundAt(t.x, t.z); t.y = g;
    boxG(LP, LC, t.x - 0.08, g - 0.2, t.z - 0.08, t.x + 0.08, g + 1.5, t.z + 0.08, tint(WOOD, 1));
    boxG(EP, EC, t.x - 0.13, g + 1.5, t.z - 0.13, t.x + 0.13, g + 1.85, t.z + 0.13, [1.7, 1.0, 0.3]);
  }
  // path decals: plaza ring + a ribbon from the plaza to every door
  function ribbon(x0, z0, x1, z1, w, cc) {
    const dx = x1 - x0, dz = z1 - z0, len = Math.hypot(dx, dz); if (len < 0.1) return;
    const nx = -dz / len * w / 2, nz = dx / len * w / 2, n = Math.max(1, Math.round(len / 1.6));
    for (let i = 0; i < n; i++) {
      const a = i / n, b = (i + 1) / n, ax = x0 + dx * a, az = z0 + dz * a, bx = x0 + dx * b, bz = z0 + dz * b, k = 0.88 + rng() * 0.2, o = 0.14;
      quad(LP, LC, [ax + nx, groundAt(ax + nx, az + nz) + o, az + nz], [ax - nx, groundAt(ax - nx, az - nz) + o, az - nz], [bx - nx, groundAt(bx - nx, bz - nz) + o, bz - nz], [bx + nx, groundAt(bx + nx, bz + nz) + o, bz + nz], cc, [0, 1, 0], k);
    }
  }
  for (let i = 0; i < 24; i++) { const a0 = i / 24 * 6.2831853, a1 = (i + 1) / 24 * 6.2831853; ribbon(Math.cos(a0) * RP, Math.sin(a0) * RP, Math.cos(a1) * RP, Math.sin(a1) * RP, 2.0, PATH); }
  for (const h of huts) { const f = h.door.front, l = Math.hypot(f[0], f[1]) || 1; ribbon(f[0] / l * RP, f[1] / l * RP, f[0], f[1], 1.9, PATH); }
  ribbon(0, 0, 0, 0, 1, PATH); ribbon(Math.cos(stall.a) * RP, Math.sin(stall.a) * RP, stall.cx - Math.cos(stall.a) * 2.5, stall.cz - Math.sin(stall.a) * 2.5, 1.6, PATH);

  // ── meshes: ONE lit (day) + ONE emissive (windows / torches / stall lamps, night-bright) ──
  function mkGeo(P, C) { const g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute(P, 3)); g.setAttribute('color', new T.Float32BufferAttribute(C, 3)); g.computeBoundingSphere(); return g; }
  const litMat = new T.MeshBasicMaterial({ vertexColors: true, side: T.DoubleSide }), emiMat = new T.MeshBasicMaterial({ vertexColors: true, side: T.DoubleSide, toneMapped: false });
  const litMesh = new T.Mesh(mkGeo(LP, LC), litMat), emiMesh = new T.Mesh(mkGeo(EP, EC), emiMat);
  litMesh.name = 'village-day'; emiMesh.name = 'village-glow'; litMesh.frustumCulled = false; emiMesh.frustumCulled = false; emiMesh.renderOrder = 1;
  group.add(litMesh); group.add(emiMesh);
  const tris = (LP.length + EP.length) / 9;
  let nightV = -1;
  function setNight(n) {
    n = Math.max(0, Math.min(1, n)); if (Math.abs(n - nightV) < 0.01) return; nightV = n;
    const d = 1 - 0.62 * n; litMat.color.setRGB(d * (1 - 0.1 * n), d, d * (1 + 0.15 * n)); const e = 0.28 + 0.72 * n; emiMat.color.setRGB(e, e, e);
  }
  setNight(0);

  // ── villagers ──
  const vname = lingo.name(seed ^ 0x5151);
  if (huts.length < 4) { litMat.dispose(); emiMat.dispose(); litMesh.geometry.dispose(); emiMesh.geometry.dispose(); return null; }
  const nVill = Math.min(huts.length, 4 + Math.floor(rng() * 3));
  const roleOrder = ['elder', 'trader', 'farmer', 'farmer', 'kid', 'kid', 'farmer'];
  const villagers = [];
  const stopOf = {
    plaza: (a) => ({ x: Math.cos(a) * RP, z: Math.sin(a) * RP, kind: 'plaza' }),
    door: (h) => ({ x: h.door.front[0], z: h.door.front[1], kind: 'door', hut: h }),
  };
  const kidR = mulberry(seed ^ 0x6C1D);
  for (let i = 0; i < nVill; i++) {
    const role = roleOrder[i % roleOrder.length], home = huts[i % huts.length], vid = pid + ':v' + i;
    const vseed = hashStr(vid), vr = mulberry(vseed ^ 0x17), stops = [];
    if (role === 'trader') { stops.push({ x: stall.stand[0], z: stall.stand[1], kind: 'stall', dwell: 70 }, { x: Math.cos(stall.a + 0.5) * RP, z: Math.sin(stall.a + 0.5) * RP, kind: 'plaza', dwell: 10 }); }
    else if (role === 'farmer') {
      for (const p of plots.slice(0, 2)) { const l = Math.hypot(p.cx, p.cz) || 1; stops.push({ x: p.cx / l * RP, z: p.cz / l * RP, kind: 'plaza', dwell: 18 + vr() * 14 }); }
      stops.push({ x: Math.cos(0.4 + i) * RP, z: Math.sin(0.4 + i) * RP, kind: 'plaza', dwell: 8 }); stops.push(Object.assign(stopOf.door(home), { dwell: 12 }));
    } else if (role === 'elder') { stops.push({ x: Math.cos(1.1) * 3.4, z: Math.sin(1.1) * 3.4, kind: 'plaza', dwell: 45 }, Object.assign(stopOf.door(home), { dwell: 20 })); }
    else { for (let k = 0; k < 4; k++) stops.push(k % 2 ? Object.assign(stopOf.door(huts[Math.floor(kidR() * huts.length)]), { dwell: 5 + vr() * 5 }) : Object.assign(stopOf.plaza(vr() * 6.28), { dwell: 5 + vr() * 6 })); }
    const total = stops.reduce((s, q) => s + q.dwell, 0);
    const v = {
      id: vid, name: lingo.name(vseed ^ 0x77), role, seed: vseed, home: home.idx, homeHut: home, stops, total, off: vr() * total,
      pos: vec(home.cx, 0, home.cz), x: home.cx, z: home.cz, yaw: 0, inside: true, moving: false, human: null, path: null, pi: 0, tgt: '', panic: 0, greetCd: 0, mood: 0, flinch: 0,
      known: null, _n: 0, trades: null,
      say(ctx) { return lingo.line(v, ctx || 'greeting'); },
      get lines() { return [lingo.line({ seed: vseed, role, name: v.name, known: v.known }, 'greeting'), lingo.line({ seed: vseed, role, name: v.name, known: v.known }, 'shopper'), lingo.line({ seed: vseed, role, name: v.name, known: v.known }, 'gossip')]; },
    };
    v.x = home.cx; v.z = home.cz;
    villagers.push(v);
  }
  // trader's six seeded trades: they WANT a resource (SELL tab pays above market) and SELL seeded goods (BUY tab, village discount)
  const trader = villagers.find((v) => v.role === 'trader') || null;
  if (trader) {
    const menu = storeMenu('village:' + pid, 24, 1), tr = mulberry(seed ^ 0x7E4D), kinds = Object.keys(RESOURCE_BASE), used = new Set();
    trader.trades = [];
    for (let i = 0; i < 6; i++) {
      let gi = Math.floor(tr() * menu.length), g = 0; while (used.has(gi) && g++ < 30) gi = (gi + 1) % menu.length; used.add(gi);
      const getIt = menu[gi], giveIt = resourceItem(kinds[Math.floor(tr() * kinds.length)], pn.name || pid);
      trader.trades.push({ id: trader.id + ':t' + i, give: { item: giveIt, pay: Math.max(1, Math.round(giveIt.price * (1.25 + tr() * 0.25))) }, get: { item: getIt, n: 1 + Math.floor(tr() * 3), price: Math.max(1, Math.round((getIt.price || 20) * 0.85)) } });
    }
  }
  let stockDay = '', stock = {};
  const dayKey = () => { const d = new Date(); return d.getUTCFullYear() + '-' + (d.getUTCMonth() + 1) + '-' + d.getUTCDate(); };
  function market(v) {
    const t = v && v.trades; if (!t) return null;
    const dk = dayKey(); if (stockDay !== dk) { stockDay = dk; stock = {}; }
    const m = { buys: [], buyPrices: {}, sells: [], day: 0 };
    for (const tr of t) {
      const k = tr.give.item.stackKey; if (m.buys.indexOf(k) < 0) { m.buys.push(k); m.buyPrices[k] = tr.give.pay; }
      const left = stock[tr.id] != null ? stock[tr.id] : tr.get.n; if (left > 0) m.sells.push({ id: tr.id, item: tr.get.item, n: left, price: tr.get.price });
    }
    m._live = true; village.lastMarket = m; return m;
  }
  function syncStock() { const m = village.lastMarket; if (!m || !trader) return; for (const tr of trader.trades) { const s = m.sells.find((q) => q.id === tr.id); stock[tr.id] = s ? s.n : 0; } }

  // ── routes: stops are plaza / door-front points; legs go radially to the plaza ring, round it, radially out ──
  function route(ax, az, bx, bz) {
    const pts = [[ax, az]], ra = Math.hypot(ax, az), rb = Math.hypot(bx, bz), aa = Math.atan2(az, ax), ab = Math.atan2(bz, bx);
    if (Math.abs(ra - RP) > 0.4) pts.push([Math.cos(aa) * RP, Math.sin(aa) * RP]);
    let da = ab - aa; while (da > Math.PI) da -= 6.2831853; while (da < -Math.PI) da += 6.2831853;
    const n = Math.floor(Math.abs(da) / 0.4); for (let i = 1; i <= n; i++) { const a = aa + da * i / (n + 1); pts.push([Math.cos(a) * RP, Math.sin(a) * RP]); }
    if (Math.abs(rb - RP) > 0.4) { pts.push([Math.cos(ab) * RP, Math.sin(ab) * RP]); pts.push([bx, bz]); } else pts.push([bx, bz]);
    return pts;
  }
  function targetFor(v, tm, night) {
    if (night > 0.55 || v.panic > 0) return { home: true };
    let tt = (tm + v.off) % v.total; if (tt < 0) tt += v.total;
    for (let i = 0; i < v.stops.length; i++) { tt -= v.stops[i].dwell; if (tt <= 0) return { stop: i }; }
    return { stop: 0 };
  }
  function setPath(v, tg) {
    const key = tg.home ? 'home' : 's' + tg.stop; if (v.tgt === key) return; v.tgt = key;
    let pts = []; const h = v.homeHut;
    if (v.inside) { pts.push([h.cx, h.cz], [h.door.c[0], h.door.c[1]], [h.door.front[0], h.door.front[1]]); v.inside = false; v.hidden = false; }
    const from = pts.length ? pts[pts.length - 1] : [v.x, v.z];
    if (tg.home) { const r = route(from[0], from[1], h.door.front[0], h.door.front[1]); pts = pts.concat(pts.length ? r.slice(1) : r); pts.push([h.door.c[0], h.door.c[1]], [h.cx, h.cz]); }
    else { const s = v.stops[tg.stop], r = route(from[0], from[1], s.x, s.z); pts = pts.concat(pts.length ? r.slice(1) : r); }
    v.path = pts; v.pi = 1;
  }

  // ── humans (lazy: only inside VILLAGE_ACT_L of the player) ──
  function ensureHuman(v) {
    if (v.human || v.humanFail) return;
    try {
      const hm = createHuman(T, { color: ROLE_COL[v.role] || 0xffffff }); hm.group.traverse((q) => { q.frustumCulled = false; });
      const s = v.role === 'kid' ? 1.15 : v.role === 'elder' ? 1.7 : 1.75; hm.group.scale.setScalar(s); hm.group.visible = false; group.add(hm.group); v.human = hm;
    } catch (e) {
      v.humanFail = true;
      const m = new T.Mesh(new T.CapsuleGeometry(0.35, 1.1, 3, 6), new T.MeshBasicMaterial({ color: ROLE_COL[v.role] || 0xffffff })); m.position.y = 0.9; const g = new T.Group(); g.add(m); g.visible = false; group.add(g);
      v.human = { group: g, update() {}, dispose() { m.geometry.dispose(); m.material.dispose(); } };
    }
  }

  // ── golums ──
  const mats = (() => { try { return makeMaterials(T, { flex: false }); } catch (e) { return null; } })();
  function golumMesh(rg, r, spec) {
    const rec = { part: spec.part, params: spec.params, offset: spec.offset, rot: spec.rot, children: spec.children, flex: false, lod: 'hi', palette: { base: 0x6C6A7A, panel: 0x4A4658, accent: 0x7A5CD0, glow: 0xB070FF, dark: 0x25212F, metal: 0x8A8798 } };
    const res = compose(T, rec, rg); const g = mergeLit(T, res, 2.2); const m = new T.Mesh(g, mats.lit); m.frustumCulled = false; r.add(m); return m;
  }
  function buildGolum(i) {      // ~4.5 m = 2.5 human heights; forward = -Z like the human rig
    if (!mats) return null;
    const rg = pmul(seed ^ (0xA110 + i)), root = new T.Group(), G = { root, legL: null, legR: null, armL: null, armR: null };
    const body = new T.Group(); body.position.y = 1.7; root.add(body);
    const eye = (x) => ({ part: 'eye', params: { r: 0.2 }, offset: [x, 0.62, -0.66], rot: [-Math.PI / 2, 0, 0] });
    golumMesh(rg, body, { part: 'block', params: { w: 3.0, h: 1.9, d: 1.9, rows: 2 }, children: [
      { part: 'block', params: { w: 1.5, h: 0.95, d: 1.4, simple: true }, offset: [0, 1.85, 0], children: [eye(-0.38), eye(0.38)] },
      { part: 'block', params: { w: 1.1, h: 0.8, d: 1.1, simple: true }, offset: [-1.8, 1.05, 0] }, { part: 'block', params: { w: 1.1, h: 0.8, d: 1.1, simple: true }, offset: [1.8, 1.05, 0] }] });
    const limb = (x, y, spec) => { const piv = new T.Group(); piv.position.set(x, y, 0); root.add(piv); golumMesh(rg, piv, spec); return piv; };
    G.legL = limb(-0.8, 1.7, { part: 'block', params: { w: 1.1, h: 1.7, d: 1.2, simple: true }, rot: [Math.PI, 0, 0] });
    G.legR = limb(0.8, 1.7, { part: 'block', params: { w: 1.1, h: 1.7, d: 1.2, simple: true }, rot: [Math.PI, 0, 0] });
    G.armL = limb(-1.85, 3.5, { part: 'block', params: { w: 0.95, h: 2.7, d: 0.95, simple: true }, rot: [Math.PI, 0, 0] });
    G.armR = limb(1.85, 3.5, { part: 'block', params: { w: 0.95, h: 2.7, d: 0.95, simple: true }, rot: [Math.PI, 0, 0] });
    root.visible = false; group.add(root); return G;
  }
  const golums = [];
  { const n = rng() < 0.5 ? 2 : 1; for (let i = 0; i < n; i++) golums.push({ id: pid + ':g' + i, hp: GOLUM_HP, hpMax: GOLUM_HP, x: Math.cos(i * 3.14 + 0.5) * 8.4, z: Math.sin(i * 3.14 + 0.5) * 8.4, yaw: 0, y: wellY, target: null, tRef: null, tKind: '', cd: 1, wind: 0, ph: 0, pa: i * 3.14 + 0.5, pause: 0, anger: 0, M: null, dead: false, flash: 0, speedNow: 0, say(c) { return lingo.line({ seed: seed ^ (0x60C + i), role: 'guard', name: 'GOLUM', known: c }, 'warning'); } }); }

  // ── ground-truth helpers ──
  const pLocal = vec(0, 0, 0), vLocal = vec(0, 0, 0);
  function pushOut(g, r) {          // circle (x, z) vs every wall box (village metres): leave by the shortest axis
    for (let i = 0; i < walls.length; i++) {
      const b = walls[i]; if (g.y > b.max.y + 1 || g.y + 3 < b.min.y) continue;
      const cx = Math.max(b.min.x, Math.min(g.x, b.max.x)), cz = Math.max(b.min.z, Math.min(g.z, b.max.z)), dx = g.x - cx, dz = g.z - cz, d2 = dx * dx + dz * dz;
      if (d2 >= r * r) continue;
      if (d2 > 1e-8) { const d = Math.sqrt(d2), k = (r - d) / d; g.x += dx * k; g.z += dz * k; }
      else { const l = g.x - b.min.x, rr = b.max.x - g.x, t = g.z - b.min.z, bb = b.max.z - g.z, m = Math.min(l, rr, t, bb); if (m === l) g.x = b.min.x - r; else if (m === rr) g.x = b.max.x + r; else if (m === t) g.z = b.min.z - r; else g.z = b.max.z + r; }
    }
  }
  const events = [];
  const village = {
    id: pid, name: vname, group, anchor, radius: 44, huts: null, walls, well: { pos: [0, wellY, 0], box: wellBoxes }, bell, plots, stall, torches, villagers, golums, threat: null, events,
    scanColor: 0xFFC060, lastMarket: null, stats: { tris, draws: 2 }, night: 0, near: null,
    toParent, toLocal, groundAt, market, setNight, stockOf: () => stock,
    solid: null, poi: null,
  };
  village.huts = huts.map((h) => ({ id: pid + ':h' + h.idx, tmpl: h.tm.id, center: [h.cx, h.cz], box: h.boxOut, door: { pos: [h.door.c[0], h.yb, h.door.c[1]], front: h.door.front, side: h.side }, walls: h.walls }));
  village.solid = { m: MAT, inv: INV, K: u, hw: 62, hd: 62, hh: 14, a: walls, b: null };
  village.poi = { kind: 'village', name: vname, pos: anchor.pos.clone() };
  village.drain = () => events.splice(0, events.length);
  village.scanTargets = () => [{ type: 'village', id: pid, kind: 'village', name: 'VILLAGE ' + vname, pos: anchor.pos.clone(), color: village.scanColor }];
  village.overlaps = (p, rw) => {
    toLocal(p, vLocal); const r = rw / u + 0.05;
    for (let i = 0; i < walls.length; i++) { const b = walls[i]; if (vLocal.x > b.min.x - r && vLocal.x < b.max.x + r && vLocal.y > b.min.y - r && vLocal.y < b.max.y + r && vLocal.z > b.min.z - r && vLocal.z < b.max.z + r) return true; }
    return false;
  };
  village.nearVillager = (p, r) => {
    toLocal(p, vLocal); let bd = (r / u) * (r / u), b = null;
    for (const v of villagers) { if (v.inside) continue; const dx = v.x - vLocal.x, dz = v.z - vLocal.z, dy = (v.y || 0) - vLocal.y, d = dx * dx + dz * dz + dy * dy * 0.1; if (d < bd) { bd = d; b = v; } }
    return b;
  };
  village.onHit = (vid, aid) => {
    const v = villagers.find((q) => q.id === vid); if (!v) return false;
    const fresh = !village.threat || village.threat.id !== String(aid);
    v.panic = 10; v.flinch = 0.5; village.threat = { id: String(aid), t: 30 };
    for (const g of golums) if (!g.dead) { g.target = String(aid); g.anger = 30; g.tRef = null; g.tKind = aid === 'me' ? 'me' : 'mob'; }
    if (fresh) events.push({ type: 'golum', text: 'GOLUM · ' + (aid === 'me' ? 'YOU HIT ' + v.name.toUpperCase() : 'DEFENDING ' + v.name.toUpperCase()) });
    return true;
  };
  village.punchHit = (p, fwd, reach, dmg) => {
    toLocal(p, vLocal); tA.copy(fwd).transformDirection(INV); const rM = reach / u; let best = null, bd = 1e9;
    for (const v of villagers) { if (v.inside) continue; const dx = v.x - vLocal.x, dz = v.z - vLocal.z, d = Math.hypot(dx, dz); if (d < rM + 0.5 && (dx * tA.x + dz * tA.z) / (d || 1) > 0.25 && d < bd) { bd = d; best = { type: 'villager', id: v.id, o: v }; } }
    for (const g of golums) { if (g.dead || !g.M) continue; const dx = g.x - vLocal.x, dz = g.z - vLocal.z, d = Math.hypot(dx, dz); if (d < rM + 2.0 && (dx * tA.x + dz * tA.z) / (d || 1) > 0.1 && d < bd) { bd = d; best = { type: 'golum', id: g.id, o: g }; } }
    if (!best) return null;
    if (best.type === 'villager') village.onHit(best.id, 'me');
    else { const g = best.o; g.hp -= dmg || 6; g.flash = 0.25; g.target = 'me'; g.tKind = 'me'; g.anger = 25; if (g.hp <= 0) { g.dead = true; if (g.M) g.M.root.visible = false; } }
    return { type: best.type, id: best.id };
  };

  // ── per-frame ──
  let greetT = 0;
  function groundLive(x, z) { return groundAt(x, z, liveFn); }
  function stepGolum(g, dt, ctx, px, pz, haveP, mobsV) {
    if (g.dead) return;
    if (!g.M) g.M = buildGolum(golums.indexOf(g)); if (!g.M) return;
    g.anger = Math.max(0, g.anger - dt); if (g.anger <= 0 && g.tKind === 'me') { g.target = null; g.tKind = ''; }
    let tx = 0, tz = 0, has = false, ref = null;
    if (g.tKind === 'me' && haveP) { tx = px; tz = pz; has = true; }
    else {
      if (g.tKind === 'mob' || !g.target) {      // nearest night mob inside the village radius (or the mob that hit a villager)
        let bd = 1e9; g.target = null; g.tKind = '';
        for (const m of mobsV) { const dd = Math.hypot(m.x, m.z); if (dd > village.radius + 8) continue; const d = Math.hypot(m.x - g.x, m.z - g.z); if (d < bd) { bd = d; ref = m; } }
        if (ref) { g.target = 'mob:' + ref.id; g.tKind = 'mob'; g.tRef = ref; tx = ref.x; tz = ref.z; has = true; }
      }
    }
    let spd = 0;
    if (has) {
      const dx = tx - g.x, dz = tz - g.z, d = Math.hypot(dx, dz) || 1;
      let dyaw = Math.atan2(-dx, -dz) - g.yaw; while (dyaw > Math.PI) dyaw -= 6.2831853; while (dyaw < -Math.PI) dyaw += 6.2831853; g.yaw += dyaw * Math.min(1, 6 * dt);
      if (d > 2.9) { spd = GOLUM_V; g.x += dx / d * spd * dt; g.z += dz / d * spd * dt; }
      g.cd -= dt;
      if (d < 3.6 && g.cd <= 0 && g.wind <= 0) g.wind = 0.45;
      if (g.wind > 0) { g.wind -= dt; if (g.wind <= 0) { g.cd = 1.5; g.swing = 0.35; if (d < 4.2) { if (g.tKind === 'me') ctx.hurtPlayer && ctx.hurtPlayer(GOLUM_DMG_ME); else if (g.tRef) ctx.hurtMob && ctx.hurtMob(g.tRef.ref, GOLUM_DMG_MOB); } } }
    } else {
      g.pause -= dt;
      if (g.pause <= 0) {      // patrol the plaza ring slowly
        g.pa += 0.9 * dt / 8.4 * 1.0; const tx2 = Math.cos(g.pa) * 8.4, tz2 = Math.sin(g.pa) * 8.4, dx = tx2 - g.x, dz = tz2 - g.z, d = Math.hypot(dx, dz) || 1;
        spd = Math.min(1.5, d * 3); g.x += dx / d * spd * dt; g.z += dz / d * spd * dt;
        let dyaw = Math.atan2(-dx, -dz) - g.yaw; while (dyaw > Math.PI) dyaw -= 6.2831853; while (dyaw < -Math.PI) dyaw += 6.2831853; g.yaw += dyaw * Math.min(1, 3 * dt);
        if (Math.random() < dt * 0.04) g.pause = 4 + Math.random() * 6;
      }
    }
    const rr = Math.hypot(g.x, g.z); if (rr > village.radius + 14) { g.x *= (village.radius + 14) / rr; g.z *= (village.radius + 14) / rr; }
    g.y = groundLive(g.x, g.z); pushOut(g, 1.7); g.y = groundLive(g.x, g.z);
    g.speedNow += (spd - g.speedNow) * Math.min(1, 6 * dt); g.ph += g.speedNow * dt * 1.15;
    const M = g.M, sw = Math.sin(g.ph) * Math.min(1, g.speedNow / 2) * 0.7;
    M.root.visible = true; M.root.position.set(g.x, g.y, g.z); M.root.rotation.y = g.yaw;
    M.legL.rotation.x = sw; M.legR.rotation.x = -sw; M.armL.rotation.x = -sw * 0.8;
    if (g.swing > 0) { g.swing -= dt; M.armR.rotation.x = -2.2 * (g.swing / 0.35) + 0.2; } else if (g.wind > 0) M.armR.rotation.x = -2.2 * (1 - g.wind / 0.45); else M.armR.rotation.x = sw * 0.8;
    g.pos = M.root.position;
  }

  village.update = (dt, ctx) => {
    ctx = ctx || {}; const night = ctx.night || 0; village.night = night; setNight(night);
    if (village.threat) { village.threat.t -= dt; if (village.threat.t <= 0) village.threat = null; }
    const tm = ctx.t != null ? ctx.t : Date.now() / 1000, haveP = !!ctx.player;
    let px = 0, pz = 0, pd = 1e12;
    if (haveP) { toLocal(ctx.player, vLocal); px = vLocal.x; pz = vLocal.z; pd = anchor.pos.distanceTo(ctx.player); }
    const act = pd < VILL_ACT_L * Lw + 60 * u;
    if (!act) { for (const v of villagers) { v.first = null; if (v.human) v.human.group.visible = false; } for (const g of golums) if (g.M) g.M.root.visible = false; village.near = null; return; }
    // mobs -> village metres
    const mobsV = [];
    if (ctx.mobs && ctx.mobs.length) for (const m of ctx.mobs) { toLocal(m.pos, tA); const x = tA.x, z = tA.z; if (Math.hypot(x, z) < village.radius + 25) mobsV.push({ id: m.id, ref: m.ref, x, z }); }
    // villagers
    let near = null, nd = 1e9;
    for (const v of villagers) {
      ensureHuman(v);
      if (v.known !== ctx.known) v.known = ctx.known || null;
      v.panic = Math.max(0, v.panic - dt); v.flinch = Math.max(0, v.flinch - dt); v.greetCd = Math.max(0, v.greetCd - dt);
      if (v.first == null) {      // first sim frame: snap to the routine so every client agrees
        v.first = 1; const tg = targetFor(v, tm, night);
        if (tg.home) { v.x = v.homeHut.cx; v.z = v.homeHut.cz; v.inside = true; }
        else { const s = v.stops[tg.stop]; v.x = s.x; v.z = s.z; v.inside = false; }
        v.tgt = tg.home ? 'home' : 's' + tg.stop; v.path = null;
      }
      const tg = targetFor(v, tm, night);
      if (!(v.inside && tg.home)) setPath(v, tg);
      let moving = false;
      if (v.path && v.pi < v.path.length) {
        const q = v.path[v.pi], dx = q[0] - v.x, dz = q[1] - v.z, d = Math.hypot(dx, dz), sp = (v.panic > 0 ? WALK * 1.8 : WALK * (v.role === 'kid' ? 1.3 : v.role === 'elder' ? 0.7 : 1)) * dt;
        if (d <= sp) { v.x = q[0]; v.z = q[1]; v.pi++; } else { v.x += dx / d * sp; v.z += dz / d * sp; v.yaw = Math.atan2(-dx, -dz); moving = true; }
        if (v.pi >= v.path.length && v.tgt === 'home') { v.inside = true; v.path = null; }
      }
      v.moving = moving; v.y = v.inside ? v.homeHut.yb : groundLive(v.x, v.z); v.pos.set(v.x, v.y, v.z);
      const hm = v.human;
      if (hm) {
        const vis = !v.inside; if (hm.group.visible !== vis) hm.group.visible = vis;
        if (vis) { hm.group.position.set(v.x, v.y, v.z); hm.group.rotation.y = v.yaw; try { hm.update(dt, { moving, running: v.panic > 0, airborne: false, speed: moving ? (v.panic > 0 ? 1 : 0.5) : 0, facing: 0, jet: 0, scan: 0 }); } catch (e) { /* ignore */ } }
      }
      if (haveP && !v.inside) { const d = Math.hypot(v.x - px, v.z - pz); if (d < nd) { nd = d; near = v; } }
      // a mob within 2.5 m of a villager = a hit on a villager
      if (!v.inside) for (const m of mobsV) if (Math.hypot(m.x - v.x, m.z - v.z) < 2.5 && (v.mobCd = (v.mobCd || 0)) <= 0) { v.mobCd = 3; village.onHit(v.id, 'mob:' + m.id); }
      if (v.mobCd > 0) v.mobCd -= dt;
    }
    village.near = near;
    // greet within 3 H (5.25 m)
    greetT -= dt;
    if (near && nd < 5.25 && near.greetCd <= 0 && greetT <= 0) { near.greetCd = 40; greetT = 4; events.push({ type: 'greet', v: near, text: lingo.line({ seed: near.seed, role: near.role, name: near.name, known: near.known, _n: near._n++ }, near._n % 3 === 0 ? 'shopper' : 'greeting') }); }
    // golums
    for (const g of golums) stepGolum(g, dt, ctx, px, pz, haveP, mobsV);
  };

  village.serialize = () => { syncStock(); return { day: stockDay, stock: Object.assign({}, stock) }; };
  village.restore = (o) => { if (!o || typeof o !== 'object') return; if (o.day === dayKey() && o.stock && typeof o.stock === 'object') { stockDay = o.day; stock = {}; for (const k in o.stock) if (typeof o.stock[k] === 'number') stock[k] = o.stock[k] | 0; } };
  village.syncStock = syncStock;
  village.dispose = () => {
    for (const v of villagers) if (v.human) { try { v.human.dispose(); } catch (e) { /* ignore */ } }
    for (const g of golums) if (g.M) g.M.root.traverse((o) => { if (o.geometry) o.geometry.dispose(); });
    litMesh.geometry.dispose(); emiMesh.geometry.dispose(); litMat.dispose(); emiMat.dispose();
    if (group.parent) group.parent.remove(group);
  };
  return village;
}
