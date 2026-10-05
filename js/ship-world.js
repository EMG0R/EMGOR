// ship-world.js — INTERGALACTIC 7/11 stores, Burger House, NPCs and shards for ship mode (docs/ship-mode.md rev 21 C + rev 23 D).
//
//   createWorld(THREE, ps, L, { parts, weapons }) -> world
//     world.group             THREE.Group in PLANET-LOCAL space (the planet module parents it into its 'ride' group). Empty until attach().
//     world.attach(node)      build this planet's stores + shards + Burger House (one store per outpost; if the planet has none, one at the flattest land spot found by sampling)
//     world.detach()          remove them and free their per-planet GPU data (the world can attach again)
//     world.stores            [{ id, name, pos, npcSpot:{pos,facing}, counter:{pos,radius}, clerk:{ name, seed, role, known, lines[3], human },
//                               inventory:{ weapons[4], upgrades:{ shield:[3 tiers], engine:[3 tiers] }, snack },
//                               menu:[item x24]  (js/ship-items.js, sorted by price),
//                               dealers:[{ id, name, seed, role:'dealer', known, human, pos (live, parent frame), radius, menu:[item x6], say(ctx, item?) }] x2,
//                               shoppers:[{ id, name, seed, role: shopper|conspiracy|pilot|kid|cop, known, human, pos (live), radius, say(ctx) }] x3..6,
//                               characters:[...] (the shoppers with a special role),
//                               interior:{ floorY, walls:[box], aisles:[box], size:{w,d,h}, unit, matrix (store-local metres -> planet-local), inverse,
//                                          toParent(v,out), toStore(v,out), inside(p parent-frame) },
//                               burgerHouse: world.burgerHouse on the first store, else null }]      all positions planet-local (Vector3)
//        box = { name, min:Vector3, max:Vector3 } in STORE-LOCAL metres (x across, z toward the glass front (+z), y up from the slab; floorY = standing height).
//        Convert with store.interior.toParent / matrix; 1 m = interior.unit world units (= 0.09 L / 1.75 = a human's height is 1.75 m).
//     world.burgerHouse       { id, pos, windowPos, radius, clerk:{ name, seed, role:'fries', known, human, say(ctx) }, menu:[FRIES], walls:[box], interior-like matrix } or null before attach
//     world.known             Set of learned concepts (ship-lingo). world.setKnown(set) rebinds every NPC to the player's saved Set.
//     world.shards            [{ id, pos, taken }]    20 per planet; pos is the resting spot (planet-local), the crystal floats ~0.9 L above it
//     world.takeShard(id)     -> the shard (and hides it) or null if unknown / already taken.  world.markTaken([ids]) restores saved state silently.
//     world.onShard           optional callback(shard): update() auto-takes a shard when the player is within 1.6 L and calls this
//     world.npcLine(id)       id of a store (or its clerk, any NPC id, 'burger'): next lingo line (string with alien-word markers, see ship-lingo render()).
//     world.npcSay(id, ctx)   same with an explicit context ('greeting'|'pitch'|'gossip'|'warning'|'lore'|'dealer'|'cashier'|'fries'|'shopper')
//     world.update(t, dt, playerLocalPos)   animation + proximity; sets world.nearStore (counter within reach), world.nearBurger (window within reach), world.nearNpc (nearest shopper/dealer within ~3.5 m)
//     world.dispose()
//   STORE_NAME is the one constant to rename the shop (the lit sign, the UI and the clerk lines read it).
//   Also exported for the station: createStandaloneStore(THREE, opts), transformBoxes(boxes, matrix).
// Scale: the store is modelled in metres (40 wide, 26 deep, 16 tall); a human is 1.75 m = 0.09 L.
// Draw calls: stores = 4 instanced draws (bodies, emissives, signs, glass) for all outposts + 2 per human (visible only within ~30 L); shards = 1; Burger House = 3.
// Store shell + shelves + item boxes is ONE shared geometry (see world.storeTris, <= 6000) instanced per outpost.
import { createHuman } from './ship-human.js';
import { storeMenu, dealerMenu, FRIES } from './ship-items.js';
import * as lingo from './ship-lingo.js';

export const STORE_NAME = 'INTERGALACTIC 7/11';
export const STORE_COLOR = 0x8A3CFF;
export const SHARDS_PER_PLANET = 20;
export const SHARD_VALUE = 25;
const HUMAN_H = 0.09;                   // human height in L
const W = 40, D = 26, H = 16;           // store footprint in metres
const HW = W / 2, HD = D / 2, WT = 0.5, FY = 0.22;   // half sizes, wall thickness, floor standing height

function hashStr(s) { let h = 2166136261; s = String(s); for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
function mulberry(a) { a |= 0; return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

const VERT = /* glsl */`
varying vec3 vC; varying vec3 vV;
#include <common>
#include <logdepthbuf_pars_vertex>
void main(){
  vC = color;
  vec4 lp = vec4(position, 1.0);
  #ifdef USE_INSTANCING
    lp = instanceMatrix * lp;
  #endif
  #ifdef USE_INSTANCING_COLOR
    vC *= instanceColor;
  #endif
  vec4 mv = modelViewMatrix * lp;
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
  vec3 L = normalize(mat3(viewMatrix) * normalize(vec3(-0.62, 0.52, 0.4)));
  float d = max(0.0, dot(n, L)) * 0.7 + 0.3;
  float rim = pow(1.0 - max(0.0, dot(n, normalize(-vV))), 3.0) * 0.28;
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
  gl_FragColor = vec4(vC * (0.94 + 0.08 * sin(uTime * 3.0 + vC.g * 9.0 + vV.x * 0.02)), 1.0);
  #include <colorspace_fragment>
}`;

// ── non-indexed box soup with vertex colours (position + color only) ──
function makeSoup(T) {
  const pos = [], col = [], c = new T.Color(), q = new T.Vector3();
  const soup = {
    // box: size (w,h,d), centre x, bottom y, centre z, colour, optional rotation about X (rad) around its own bottom-front... centre of the box
    box(w, h, d, cx, y0, cz, hex, rx = 0, brightness = 1) {
      c.set(hex); const r = c.r * brightness, g = c.g * brightness, b = c.b * brightness;
      const corners = [];
      for (let i = 0; i < 8; i++) {
        q.set((i & 1 ? 0.5 : -0.5) * w, (i & 2 ? 0.5 : -0.5) * h, (i & 4 ? 0.5 : -0.5) * d);
        if (rx) { const cs = Math.cos(rx), sn = Math.sin(rx), y = q.y * cs - q.z * sn, z = q.y * sn + q.z * cs; q.y = y; q.z = z; }
        corners.push([q.x + cx, q.y + y0 + h / 2, q.z + cz]);
      }
      // faces by corner index: bit0=x, bit1=y, bit2=z.  Two triangles per face, CCW seen from outside (DoubleSide anyway).
      const quads = [[1, 3, 7, 5], [0, 4, 6, 2], [2, 6, 7, 3], [0, 1, 5, 4], [4, 5, 7, 6], [0, 2, 3, 1]];
      for (const f of quads) for (const t of [[0, 1, 2], [0, 2, 3]]) for (const k of t) { const p = corners[f[k]]; pos.push(p[0], p[1], p[2]); col.push(r, g, b); }
    },
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

// ── the INTERGALACTIC 7/11, store-local metres. Origin = footprint centre on the slab, front (glass, door, awning) faces +Z. ──
const LANES_X = [-16.85, -11, -5, 1.6, 8.5, 14.5];          // walkable lanes (gondolas sit between them)
const ZN = -9.9, ZS = 4.8;                                    // north / south cross aisles
const ITEM_COLORS = (() => { const c = storeMenu('baked-shelf', 32).map((i) => i.color); return c.length ? c : [0xFF5CE1, 0x5CE8FF, 0xFFD25C, 0x8A6CFF, 0x7CFF8A, 0xFF8A5C]; })();

function buildStoreGeoms(T, opts) {
  opts = opts || {};
  const B = makeSoup(T), E = makeSoup(T), walls = [], aisles = [];
  const WALL = 0xEEEAF8, TRIM = 0x5E32B8, TEAL = 0x2FD9C8, MAG = 0xFF5CE1, DARK = 0x2A2430, FLOOR = 0xB4AEC8, SHELF = 0x4A4256;
  const rec = (list, name, w, h, d, cx, y0, cz) => list.push({ name, min: [cx - w / 2, y0, cz - d / 2], max: [cx + w / 2, y0 + h, cz + d / 2] });
  const wall = (name, w, h, d, cx, y0, cz, hex) => { B.box(w, h, d, cx, y0, cz, hex); rec(walls, name, w, h, d, cx, y0, cz); };
  const aisle = (name, w, h, d, cx, y0, cz) => rec(aisles, name, w, h, d, cx, y0, cz);
  let ci = 0; const nextCol = () => ITEM_COLORS[(ci++ * 7 + 3) % ITEM_COLORS.length];

  // slab, foundation, tile floor
  B.box(W + 1.6, 3.2, D + 1.6, 0, -3.0, 0, 0x6A6480); B.box(W + 1.6, 0.5, D + 1.6, 0, -0.3, 0, FLOOR); B.box(W - 1, 0.02, D - 1, 0, 0.2, 0, 0xD5CFE6);
  for (let k = -3; k <= 3; k++) E.box(0.18, 0.02, D - 3, k * 5.2, 0.22, 0, 0xB8A8F0, 0, 0.8);   // lit floor strips down the aisles
  // shell
  wall('back', W, H, WT, 0, 0.2, -HD + WT / 2, WALL);
  wall('wallL', WT, H, D, -HW + WT / 2, 0.2, 0, WALL); wall('wallR', WT, H, D, HW - WT / 2, 0.2, 0, WALL);
  wall('roof', W + 1.2, 0.8, D + 1.2, 0, H + 0.2, 0, TRIM);
  B.box(W + 1.2, 1.4, 0.7, 0, H + 1.0, HD + 0.15, TRIM);                                    // parapet
  B.box(W + 0.4, 0.6, 0.5, 0, H + 0.45, -HD - 0.3, TRIM);                                    // back cornice
  // front wall: pier | window (sill, mullions, lintel) | pier | door | pier
  const fz = HD - WT / 2;
  wall('pierL', 1.4, H, WT, -19.3, 0.2, fz, WALL);
  wall('sill', 28.6, 1.0, WT, -4.3, 0.2, fz, TRIM);
  wall('lintelW', 28.6, H + 0.2 - 7.5, WT, -4.3, 7.5, fz, WALL);
  for (let i = 1; i < 6; i++) B.box(0.3, 6.3, 0.5, -18.6 + 28.6 * i / 6, 1.2, HD - 0.25, TRIM);
  rec(walls, 'glass', 28.6, 6.3, 0.2, -4.3, 1.2, HD - 0.2);
  wall('pier2', 2.0, H, WT, 11, 0.2, fz, WALL);
  wall('lintelD', 5.0, H + 0.2 - 5.8, WT, 14.5, 5.8, fz, WALL);
  wall('pierR', 3.0, H, WT, 18.5, 0.2, fz, WALL);
  B.box(0.4, 5.6, 0.7, 12.0, 0.2, HD - 0.1, TRIM); B.box(0.4, 5.6, 0.7, 17.0, 0.2, HD - 0.1, TRIM); B.box(5.4, 0.4, 0.7, 14.5, 5.6, HD - 0.1, TRIM);
  E.box(0.14, 5.6, 0.14, 11.78, 0.2, HD + 0.28, MAG); E.box(0.14, 5.6, 0.14, 17.22, 0.2, HD + 0.28, MAG); E.box(5.6, 0.14, 0.14, 14.5, 6.0, HD + 0.28, MAG);
  E.box(4.6, 0.04, 2.0, 14.5, 0.22, HD - 1.3, TEAL, 0, 0.6);                                  // lit door mat
  // awning: striped slats over the whole front
  for (let i = 0; i < 14; i++) { const w = 43 / 14; B.box(w, 0.16, 4.4, -21.5 + w * (i + 0.5), 8.4, HD + 2.0, i % 2 ? 0xF2ECFF : STORE_COLOR, 0.2); }
  B.box(0.35, 8.2, 0.35, -21.2, 0.2, HD + 4.0, DARK); B.box(0.35, 8.2, 0.35, 21.2, 0.2, HD + 4.0, DARK); B.box(0.35, 8.2, 0.35, 0, 0.2, HD + 4.0, DARK);
  // sign: backing on the parapet, neon frame, little satellite dishes on the roof
  B.box(32, 6.4, 0.7, 0, H + 2.4, HD + 0.2, DARK);
  E.box(32.6, 0.24, 0.22, 0, H + 2.3, HD + 0.58, MAG); E.box(32.6, 0.24, 0.22, 0, H + 8.7, HD + 0.58, MAG);
  E.box(0.24, 6.6, 0.22, -16.2, H + 2.3, HD + 0.58, TEAL); E.box(0.24, 6.6, 0.22, 16.2, H + 2.3, HD + 0.58, TEAL);
  B.box(0.3, 1.8, 0.3, -9, H + 1.0, 4, DARK); B.box(2.4, 0.2, 2.4, -9, H + 2.7, 4, 0xDDD8EE, 0.7); E.box(0.3, 0.3, 0.3, -9, H + 3.0, 4.2, 0xFF3A6A);
  // ceiling: beams + lit panels
  for (let k = -3; k <= 3; k++) B.box(0.5, 0.7, D - 1, k * 5.4, H - 0.6 + 0.2, 0, SHELF);
  for (let r = 0; r < 3; r++) for (let c = 0; c < 5; c++) E.box(3.4, 0.1, 2.6, -16 + c * 8, H + 0.2 - 0.12, -8 + r * 8, 0xFFF4D8, 0, 0.95);
  // back wall: wash + fridge wall (9 doors, 6 visible items each)
  E.box(W - 3, 7.2, 0.08, 0, 6.3, -HD + WT + 0.05, 0xFFE6B0, 0, 0.35);
  for (let f = 0; f < 9; f++) {
    const fx = -16 + f * 4.0;
    B.box(3.85, 6.0, 1.6, fx, 0.2, -HD + WT + 0.8, 0x3A3446);
    E.box(3.4, 5.2, 0.08, fx, 0.55, -HD + WT + 1.62, 0x5CE8FF, 0, 0.42);
    for (let r = 0; r < 2; r++) for (let c = 0; c < 3; c++) E.box(0.8, 1.2, 0.12, fx - 1.1 + c * 1.1, 1.2 + r * 2.2, -HD + WT + 1.7, nextCol(), 0, 0.85);
    E.box(3.4, 0.16, 0.1, fx, 5.9, -HD + WT + 1.66, 0xFFF4D8);
  }
  aisle('fridges', 36.3, 6.0, 1.6, 0, 0.2, -HD + WT + 0.8);
  // wall shelves along the left wall (4 levels, 12 boxes each)
  for (let l = 0; l < 4; l++) {
    B.box(1.1, 0.12, 22, -HW + WT + 0.55, 0.9 + l * 1.5, 0, SHELF);
    for (let k = 0; k < 12; k++) E.box(0.8, 0.9, 1.3, -HW + WT + 0.6, 1.02 + l * 1.5, -10.0 + k * 1.82, nextCol(), 0, 0.82);
  }
  B.box(0.2, 6.2, 22, -HW + WT + 0.1, 0.2, 0, SHELF);
  aisle('shelvesL', 1.2, 6.2, 22.2, -HW + WT + 0.6, 0.2, 0);
  // gondolas: 3 double-sided units along z, 4 levels, 6 boxes per side per level
  [-14, -8, -2].forEach((gx, gi) => {
    const gz = -3, len = 11.2;
    B.box(0.25, 5.4, len, gx, 0.2, gz, SHELF);
    for (let l = 0; l < 4; l++) {
      B.box(2.4, 0.12, len, gx, 0.55 + l * 1.25, gz, SHELF);
      for (const sd of [-1, 1]) for (let k = 0; k < 6; k++) E.box(0.95, 0.85, 1.3, gx + sd * 0.7, 0.67 + l * 1.25, gz - len / 2 + 0.95 + k * 1.86, nextCol(), 0, 0.82);
    }
    for (const sz of [-1, 1]) {
      B.box(2.5, 0.9, 0.25, gx, 5.4, gz + sz * (len / 2), DARK);
      E.box(2.2, 0.7, 0.1, gx, 5.5, gz + sz * (len / 2 + 0.16), [TEAL, MAG, 0xFFC46B][gi], 0, 0.9);
    }
    E.box(2.0, 1.0, 0.1, gx, 8.4, gz, [TEAL, MAG, 0xFFC46B][gi], 0, 0.9); B.box(0.08, 2.2, 0.08, gx, 6.4, gz, DARK);      // hanging aisle sign
    aisle('gondola' + gi, 2.5, 5.4, len + 0.3, gx, 0.2, gz);
  });
  // cashier counter (customer side z+), till, candy rack, hot-dog roller
  B.box(6.0, 1.5, 1.8, 6.5, 0.2, 8.8, STORE_COLOR, 0, 0.8); B.box(6.6, 0.15, 2.2, 6.5, 1.7, 8.8, DARK);
  B.box(1.0, 0.45, 0.8, 4.4, 1.85, 8.6, 0x3A3446); E.box(0.8, 0.04, 0.55, 4.4, 2.3, 8.55, 0x6CFFB0); B.box(1.6, 0.5, 0.9, 6.6, 1.85, 8.8, 0x555064);
  for (let k = 0; k < 4; k++) E.box(0.7, 0.9, 0.5, 7.9 + k * 0.4, 1.85, 8.5, nextCol(), 0, 0.85);
  for (let k = 0; k < 3; k++) E.box(1.2, 0.2, 0.2, 5.6, 2.4 + k * 0.25, 9.3, 0xE0603A, 0, 0.8);
  aisle('counter', 6.6, 1.7, 2.2, 6.5, 0.2, 8.8);
  B.box(6.0, 3.2, 0.4, 6.5, 0.2, 4.7, SHELF); for (let k = 0; k < 6; k++) E.box(0.8, 0.6, 0.1, 4.0 + k * 0.95, 1.3 + (k % 2) * 0.9, 4.95, nextCol(), 0, 0.85);     // smokes-and-lottery wall behind the clerk
  // slurpee machine (swirling tanks) and coffee station on the right wall
  B.box(2.4, 3.2, 4.2, 18.2, 0.2, 0, 0x3A3446);
  [0x5CE8FF, 0xFF5CE1, 0x7CFF8A, 0xFFD25C].forEach((c, k) => { E.box(0.9, 2.6, 0.8, 18.2, 3.4, -1.5 + k * 1.0, c, 0, 0.9); B.box(0.35, 0.5, 0.3, 16.9, 1.9, -1.5 + k * 1.0, DARK); });
  E.box(0.1, 0.4, 3.6, 16.98, 2.7, 0, MAG);
  B.box(2.4, 2.2, 6.0, 18.2, 0.2, -8, 0x3A3446); B.box(2.6, 0.15, 6.2, 18.2, 2.4, -8, DARK);
  [0xFFB25C, 0xFFB25C, 0xC98B4A].forEach((c, k) => E.box(0.6, 0.9, 0.6, 18.2, 2.55, -9.5 + k * 1.5, c, 0, 0.9));
  for (let k = 0; k < 3; k++) E.box(0.5, 0.4, 0.5, 17.6, 2.55, -6.9 + k * 0.6, 0xF4F8FF, 0, 0.9);
  aisle('slurpee', 2.4, 6.0, 4.2, 18.2, 0.2, 0); aisle('coffee', 2.6, 3.4, 6.2, 18.2, 0.2, -8);
  // hanging mascots (a giant slurpee and a hot dog)
  for (const [dx, dz] of [[-1.2, -1.2], [1.2, -1.2], [-1.2, 1.2], [1.2, 1.2]]) B.box(0.08, 5.4, 0.08, 10 + dx, H - 5.6, -4 + dz, DARK);
  E.box(3.2, 3.6, 3.2, 10, H - 9.4, -4, 0x5CE8FF, 0, 0.85); E.box(3.4, 0.5, 3.4, 10, H - 5.8, -4, 0xF4F8FF, 0, 0.9); E.box(0.35, 2.8, 0.35, 10.6, H - 5.8, -4, MAG);
  // parking pads / apron outside
  B.box(W + 4, 0.3, 7, 0, -0.2, HD + 3.5, 0x6A6480);
  if (opts.pads !== false) for (const px of [-10, 2, 14.5]) {
    B.box(8, 0.3, 10, px, -0.2, HD + 11, 0x5A5470);
    E.box(8, 0.02, 0.2, px, 0.1, HD + 6.2, TEAL); E.box(0.2, 0.02, 10, px - 3.9, 0.1, HD + 11, TEAL); E.box(0.2, 0.02, 10, px + 3.9, 0.1, HD + 11, TEAL); E.box(2.4, 0.02, 0.4, px, 0.1, HD + 11, MAG);
  }
  // dealer spots: crates + a trash can by the posts
  B.box(1.2, 0.9, 1.2, -6.8, 0.1, HD + 3.2, 0x6A5030); B.box(0.8, 1.0, 0.8, 21.0, 0.1, HD + 2.4, 0x555064); E.box(0.82, 0.12, 0.82, 21.0, 1.1, HD + 2.4, 0xFF8A5C);
  const body = B.geometry(), emi = E.geometry();
  return { body, emi, tris: B.tris() + E.tris(), walls, aisles };
}

// glass front (one translucent plane) + sign plane geometry (store-local)
function glassGeometry(T) { const g = new T.PlaneGeometry(28.6, 6.3); g.translate(-4.3, 1.2 + 3.15, HD - 0.12); return g; }
function signTexture(T) {
  const c = document.createElement('canvas'); c.width = 1024; c.height = 224;
  const x = c.getContext('2d');
  const bg = x.createLinearGradient(0, 0, 1024, 0); bg.addColorStop(0, '#14082a'); bg.addColorStop(0.5, '#1e0c3a'); bg.addColorStop(1, '#0a1230');
  x.fillStyle = bg; x.fillRect(0, 0, 1024, 224);
  const rng = mulberry(711);
  for (let i = 0; i < 90; i++) { x.fillStyle = 'rgba(255,255,255,' + (0.25 + rng() * 0.6) + ')'; x.fillRect(rng() * 1024, rng() * 224, 2, 2); }
  x.save(); x.translate(512, 112); x.rotate(-0.08); x.strokeStyle = 'rgba(92,232,255,0.35)'; x.lineWidth = 6; x.beginPath(); x.ellipse(0, 4, 470, 56, 0, 0, Math.PI * 2); x.stroke(); x.restore();
  let fs = 112; x.textBaseline = 'middle'; x.textAlign = 'center';
  do { x.font = '900 ' + fs + 'px "Arial Black", Impact, sans-serif'; fs -= 4; } while (x.measureText('INTERGALACTIC 7/11').width > 940 && fs > 30);
  x.shadowColor = '#ff5ce1'; x.shadowBlur = 30; x.fillStyle = '#fff2fb'; x.fillText('INTERGALACTIC 7/11', 512, 116);
  x.shadowColor = '#5ce8ff'; x.shadowBlur = 16; x.strokeStyle = '#5ce8ff'; x.lineWidth = 3; x.strokeText('INTERGALACTIC 7/11', 512, 116);
  x.shadowBlur = 0; x.strokeStyle = '#8a3cff'; x.lineWidth = 8; x.strokeRect(5, 5, 1014, 214);
  const tex = new T.CanvasTexture(c); tex.colorSpace = T.SRGBColorSpace; tex.anisotropy = 4;
  return tex;
}

// ── Burger House: a tiny red-and-white 1950s walk-up burger stand (low-poly, no logos). Local metres, front window faces +Z. ──
function buildBurgerGeoms(T) {
  const B = makeSoup(T), E = makeSoup(T), walls = [];
  const WH = 0xF6F2EC, RED = 0xD8312F, DARK = 0x2A2024, CONC = 0xBDB8C8, CREAM = 0xFFF1C8;
  const wall = (name, w, h, d, cx, y0, cz, hex) => { B.box(w, h, d, cx, y0, cz, hex); walls.push({ name, min: [cx - w / 2, y0, cz - d / 2], max: [cx + w / 2, y0 + h, cz + d / 2] }); };
  B.box(15, 2.4, 13, 0, -2.2, 1.6, 0x6A6480); B.box(14, 0.3, 12, 0, -0.1, 1.6, CONC);                  // foundation + concrete lot
  E.box(13.6, 0.02, 0.25, 0, 0.2, 7.2, RED, 0, 0.7);                                                       // painted kerb line
  // walls: body 8 x 5.2, front wall z=+2.45 with the walk-up window
  wall('back', 8, 3.6, 0.3, 0, 0.2, -2.45, WH); wall('sideL', 0.3, 3.6, 5.2, -3.85, 0.2, 0, WH); wall('sideR', 0.3, 3.6, 5.2, 3.85, 0.2, 0, WH);
  wall('frontL', 2.1, 3.6, 0.3, -2.95, 0.2, 2.45, WH); wall('frontR', 2.1, 3.6, 0.3, 2.95, 0.2, 2.45, WH);
  wall('frontLow', 3.8, 1.3, 0.3, 0, 0.2, 2.45, WH); wall('frontHigh', 3.8, 0.8, 0.3, 0, 3.0, 2.45, WH);
  for (const sx of [-1, 1]) B.box(2.2, 1.0, 0.32, sx * 2.95, 0.2, 2.47, RED);                              // red kick band
  B.box(3.8, 0.7, 0.34, 0, 0.2, 2.47, RED);
  B.box(8.3, 0.25, 5.5, 0, 3.8 - 0.05, 0, WH); B.box(9.2, 0.3, 6.3, 0, 4.0, 0, RED); B.box(9.4, 0.14, 6.5, 0, 4.3, 0, WH);   // roof + stripe + cap
  B.box(3.8, 1.5, 0.1, 0, 1.5, -2.3, DARK); E.box(3.7, 1.3, 0.06, 0, 1.6, -2.2, 0xFFC878, 0, 0.7);        // lit interior glow through the window
  B.box(5.0, 0.14, 1.0, 0, 1.5, 3.0, RED); B.box(5.2, 0.05, 1.1, 0, 1.64, 3.0, WH);                         // service ledge
  wall('floorInside', 7.4, 0.1, 4.6, 0, 0.2, 0, DARK);
  for (let i = 0; i < 8; i++) { const w = 5.6 / 8; B.box(w, 0.12, 2.3, -2.8 + w * (i + 0.5), 3.1, 3.5, i % 2 ? WH : RED, 0.32); }       // striped awning
  B.box(0.12, 1.4, 0.12, -2.8, 1.7, 4.6, DARK); B.box(0.12, 1.4, 0.12, 2.8, 1.7, 4.6, DARK);
  E.box(4.0, 0.1, 0.1, 0, 3.0, 2.62, 0xFF4A4A); E.box(4.0, 0.1, 0.1, 0, 1.4, 2.62, 0xFF4A4A); E.box(0.1, 1.7, 0.1, -2.0, 1.4, 2.62, 0xFF4A4A); E.box(0.1, 1.7, 0.1, 2.0, 1.4, 2.62, 0xFF4A4A);     // red neon window outline
  E.box(1.5, 1.0, 0.06, -3.0, 1.6, 2.62, CREAM, 0, 0.9); E.box(1.3, 0.12, 0.08, -3.0, 2.3, 2.64, RED); E.box(1.3, 0.12, 0.08, -3.0, 1.9, 2.64, RED);   // menu board (one line: fries)
  for (let k = 0; k < 13; k++) E.box(0.16, 0.16, 0.16, -3.9 + k * 0.65, 3.25, 2.7, k % 2 ? 0xFFE08A : 0xFFB04A);       // string lights on the eave
  // roof sign backing (+ canvas plane added separately), rooftop fry box
  B.box(7.6, 2.3, 0.35, 0, 4.3, 0, RED); B.box(7.8, 0.15, 0.4, 0, 6.6, 0, WH); B.box(7.8, 0.15, 0.4, 0, 4.2, 0, WH);
  B.box(2.0, 1.4, 1.4, -3.3, 4.4, -1.4, RED, 0.12); for (let k = 0; k < 8; k++) E.box(0.22, 1.2 + (k % 3) * 0.3, 0.22, -3.9 + k * 0.17, 5.6, -1.4 + ((k % 2) - 0.5) * 0.4, 0xFFD23A, 0, 0.95);
  // pylon sign, picnic tables
  B.box(0.3, 6.2, 0.3, 6.4, 0.2, 5.0, RED); B.box(3.0, 1.7, 0.3, 6.4, 5.95, 5.0, RED); B.box(2.8, 1.5, 0.12, 6.4, 6.05, 5.1, WH); E.box(3.0, 0.14, 0.2, 6.4, 7.75, 5.0, 0xFF4A4A);
  for (const [tx, tz, ry] of [[-5.0, 4.8], [-2.2, 6.4]]) { B.box(2.2, 0.12, 0.9, tx, 1.0, tz, RED); B.box(2.2, 0.1, 0.35, tx, 0.62, tz - 0.8, WH); B.box(2.2, 0.1, 0.35, tx, 0.62, tz + 0.8, WH); B.box(0.12, 0.8, 1.8, tx - 0.9, 0.2, tz, DARK); B.box(0.12, 0.8, 1.8, tx + 0.9, 0.2, tz, DARK); }
  return { body: B.geometry(), emi: E.geometry(), tris: B.tris() + E.tris(), walls };
}
function burgerSignTexture(T) {
  const c = document.createElement('canvas'); c.width = 1024; c.height = 512;
  const x = c.getContext('2d');
  x.fillStyle = '#f6f2ec'; x.fillRect(0, 0, 1024, 512);
  x.fillStyle = '#d8312f'; x.fillRect(0, 0, 1024, 220);                    // top: roof sign
  x.textAlign = 'center'; x.textBaseline = 'middle';
  let fs = 120; do { x.font = '900 ' + fs + 'px "Arial Black", Impact, sans-serif'; fs -= 4; } while (x.measureText('BURGER HOUSE').width > 930 && fs > 30);
  x.fillStyle = '#fff'; x.fillText('BURGER HOUSE', 512, 112);
  x.fillStyle = '#d8312f'; fs = 190; do { x.font = '900 ' + fs + 'px "Arial Black", Impact, sans-serif'; fs -= 4; } while (x.measureText('FRIES').width > 440 && fs > 30); x.fillText('FRIES', 256, 380);
  x.font = '900 56px "Arial Black", Impact, sans-serif'; x.fillText('SEASONED', 768, 330); x.fillStyle = '#2a2024'; x.font = '700 40px Arial, sans-serif'; x.fillText('WALK-UP WINDOW', 768, 400); x.fillText('HOT * SALTY * BLUE', 768, 450);
  x.strokeStyle = '#d8312f'; x.lineWidth = 10; x.strokeRect(8, 8, 1008, 496);
  const tex = new T.CanvasTexture(c); tex.colorSpace = T.SRGBColorSpace; tex.anisotropy = 4;
  return tex;
}
function burgerSignGeometry(T) {
  const p = [], u = [];
  const quad = (cx, cy, cz, w, h, u0, u1, v0, v1) => { p.push(cx - w / 2, cy - h / 2, cz, cx + w / 2, cy - h / 2, cz, cx + w / 2, cy + h / 2, cz, cx - w / 2, cy - h / 2, cz, cx + w / 2, cy + h / 2, cz, cx - w / 2, cy + h / 2, cz); u.push(u0, v0, u1, v0, u1, v1, u0, v0, u1, v1, u0, v1); };
  quad(0, 5.45, 0.2, 7.4, 2.0, 0.02, 0.98, 0.56, 0.98);        // roof sign: BURGER HOUSE strip (top ~43% of the canvas)
  quad(6.4, 6.8, 5.19, 2.7, 1.45, 0.02, 0.48, 0.03, 0.48);     // pylon: FRIES
  const g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute(p, 3)); g.setAttribute('uv', new T.Float32BufferAttribute(u, 2)); g.computeBoundingSphere();
  return g;
}

// transform {min,max} arrays by a matrix -> axis-aligned boxes of Vector3 (works for 90 degree yaws and any translate/scale)
export function transformBoxes(T, list, m) {
  const v = new T.Vector3();
  return list.map((b) => {
    const mn = new T.Vector3(Infinity, Infinity, Infinity), mx = new T.Vector3(-Infinity, -Infinity, -Infinity);
    const g = (p, k) => (Array.isArray(p) ? p[k] : k === 0 ? p.x : k === 1 ? p.y : p.z);
    for (let i = 0; i < 8; i++) { v.set(i & 1 ? g(b.max, 0) : g(b.min, 0), i & 2 ? g(b.max, 1) : g(b.min, 1), i & 4 ? g(b.max, 2) : g(b.min, 2)).applyMatrix4(m); mn.min(v); mx.max(v); }
    return { name: b.name, min: mn, max: mx };
  });
}

// bipyramid crystal, height 1 (y -0.5..0.5), tint comes from the instance colour
function shardGeometry(T) {
  const n = 6, r = 0.2, wy = -0.1, pos = [], col = [];
  const top = [0, 0.5, 0], bot = [0, -0.5, 0], ring = [];
  for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2; ring.push([Math.cos(a) * r, wy, Math.sin(a) * r]); }
  const push = (p, k) => { pos.push(p[0], p[1], p[2]); col.push(k, k, k); };
  for (let i = 0; i < n; i++) {
    const a = ring[i], b = ring[(i + 1) % n], sh = 0.75 + 0.25 * (i % 2);
    push(top, 1.0); push(a, sh); push(b, sh);
    push(bot, 0.35); push(b, 0.6 * sh); push(a, 0.6 * sh);
  }
  const g = new T.BufferGeometry();
  g.setAttribute('position', new T.Float32BufferAttribute(pos, 3));
  g.setAttribute('color', new T.Float32BufferAttribute(col, 3));
  g.computeBoundingSphere();
  return g;
}

const SNACKS = ['Nebula Noodles', 'Meteor Dog', 'Void Slush', 'Comet Cake', 'Plasma Pretzel', 'Quasar Crisps'];
const PRICE = { C: 120, B: 300, A: 700, S: 1500 };

// ── shared assets (one set per world / standalone store) ──
function makeAssets(T) {
  const uniformsT = { value: 0 };
  const base = { vertexShader: VERT, vertexColors: true, side: T.DoubleSide };
  const A = {
    uniformsT, geoms: null, burger: null,
    matBody: new T.ShaderMaterial(Object.assign({ fragmentShader: FRAG_LIT, uniforms: {} }, base)),
    matEmi: new T.ShaderMaterial(Object.assign({ fragmentShader: FRAG_EMI, uniforms: { uTime: uniformsT } }, base)),
    matGlass: new T.MeshBasicMaterial({ color: 0xA8ECFF, transparent: true, opacity: 0.16, depthWrite: false, side: T.DoubleSide }),
    signTex: null, matSign: null, signGeo: null, glassGeo: null, bTex: null, bMat: null, bGeo: null,
  };
  A.geoms = buildStoreGeoms(T, {});
  A.signTex = signTexture(T); A.matSign = new T.MeshBasicMaterial({ map: A.signTex, toneMapped: false });
  A.signGeo = new T.PlaneGeometry(31, 6.2); A.signGeo.translate(0, H + 5.6, HD + 0.58);
  A.glassGeo = glassGeometry(T);
  A.burgerGeoms = () => {
    if (A.burger) return A.burger;
    A.burger = buildBurgerGeoms(T); A.bTex = burgerSignTexture(T); A.bMat = new T.MeshBasicMaterial({ map: A.bTex, toneMapped: false, side: T.DoubleSide }); A.bGeo = burgerSignGeometry(T);
    return A.burger;
  };
  A.dispose = () => {
    A.geoms.body.dispose(); A.geoms.emi.dispose(); A.matBody.dispose(); A.matEmi.dispose(); A.matGlass.dispose(); A.matSign.dispose(); A.signTex.dispose(); A.signGeo.dispose(); A.glassGeo.dispose();
    if (A.burger) { A.burger.body.dispose(); A.burger.emi.dispose(); A.bTex.dispose(); A.bMat.dispose(); A.bGeo.dispose(); A.burger = null; }
  };
  return A;
}
function boxV(T, list) { return list.map((b) => ({ name: b.name, min: new T.Vector3(...b.min), max: new T.Vector3(...b.max) })); }

// ── the crowd: clerk, 3..6 shoppers wandering the aisles, 2 dealers outside. All logic in STORE-LOCAL metres; humans live in `parent` (a group in store-local metres). ──
const ROLES = ['shopper', 'conspiracy', 'pilot', 'kid', 'cop', 'shopper'];
const ROLE_COLOR = { shopper: 0x8FA0FF, conspiracy: 0x66FF99, pilot: 0xFFB05C, kid: 0xFF5CE1, cop: 0x5C7AFF, dealer: 0xFFE24A, cashier: STORE_COLOR, fries: 0xD8312F };
function makeNpc(T, parent, o) {
  let human = null;
  try { human = createHuman(T, { color: o.color }); human.group.traverse((q) => { q.frustumCulled = false; }); parent.add(human.group); } catch (e) { human = null; }
  const npc = {
    id: o.id, name: o.name || lingo.name(hashStr(o.id)), seed: o.lex, role: o.role, known: o.known, human, scale: o.scale || 1, radius: 3.5,
    lx: o.x, lz: o.z, yaw: o.yaw || 0, speed: o.speed || 1.4, pos: new T.Vector3(), local: new T.Vector3(o.x, FY, o.z),
    state: { route: [], wait: 1 + Math.random() * 3, moving: false, face: o.yaw || 0 },
    say(ctx, item) { return lingo.line(npc, item ? { ctx: ctx, item: item, price: item.price } : ctx); },
  };
  return npc;
}
function createCrowd(T, parent, cfg) {
  const r = mulberry(hashStr(cfg.id) ^ 0x7777), K = cfg.unit || 1, known = cfg.known || new Set();
  const crowd = { frame: cfg.frame || new T.Matrix4(), npcs: [], shoppers: [], dealers: [], characters: [], clerk: null, known, node: parent };
  const lex = cfg.lex;
  // cashier behind the counter, facing the customers (+z)
  crowd.clerk = makeNpc(T, parent, { id: cfg.id + '-clerk', color: ROLE_COLOR.cashier, role: 'cashier', lex, known, x: 6.5, z: 6.4, yaw: Math.PI, name: lingo.name(hashStr(cfg.id) + 5) });
  crowd.npcs.push(crowd.clerk);
  // shoppers (seeded roles; at least two special ones)
  const nShop = Math.max(0, cfg.shoppers == null ? 3 + Math.floor(r() * 4) : cfg.shoppers);
  const roles = ROLES.slice().sort(() => r() - 0.5);
  for (let i = 0; i < nShop; i++) {
    const role = i < 2 ? ['conspiracy', 'pilot', 'kid', 'cop'][(i + Math.floor(r() * 4)) % 4] : roles[i % roles.length];
    const lane = LANES_X[Math.floor(r() * LANES_X.length)], z = -8 + r() * 10;
    const npc = makeNpc(T, parent, { id: cfg.id + '-sh' + i, color: role === 'shopper' ? [0x8FA0FF, 0xFF8AA0, 0x8AFFD0, 0xE0A0FF, 0xFFD88A][Math.floor(r() * 5)] : ROLE_COLOR[role], role, lex, known, x: lane, z, yaw: r() * 6.28, scale: role === 'kid' ? 0.62 : 1, speed: role === 'kid' ? 2.1 : 1.3 });
    npc.state.wait = r() * 3; crowd.shoppers.push(npc); crowd.npcs.push(npc); if (role !== 'shopper') crowd.characters.push(npc);
  }
  // dealers loitering outside under the awning, pacing a short stretch
  const dealerCols = [0xFFE24A, 0x3AFF9A, 0xFF7A3A];
  for (let i = 0; i < (cfg.dealers == null ? 2 : cfg.dealers); i++) {
    const hx = [4.0, -9.5, 8][i % 3], hz = HD + 2.6 + (i % 2) * 0.4;
    const npc = makeNpc(T, parent, { id: cfg.id + '-d' + i, color: dealerCols[i % 3], role: 'dealer', lex, known, x: hx, z: hz, yaw: 0, speed: 0.6 });
    npc.home = { x: hx, z: hz }; npc.menu = dealerMenu(cfg.id + '-d' + i, 6); npc.state.wait = r() * 4;
    crowd.dealers.push(npc); crowd.npcs.push(npc);
  }
  crowd.setKnown = (set) => { crowd.known = set; crowd.npcs.forEach((n) => { n.known = set; }); };
  const pickTarget = (n) => {
    const rr = Math.random;
    if (rr() < 0.14) return { x: 6.5 + (rr() - 0.5) * 3, z: 10.7, face: Math.PI, counter: true };       // queue at the counter
    const lane = LANES_X[Math.floor(rr() * LANES_X.length)], z = -8 + rr() * 10.2;
    let face = lane === LANES_X[0] ? -Math.PI / 2 : lane === LANES_X[5] ? Math.PI / 2 : lane === LANES_X[3] ? -Math.PI / 2 : (rr() < 0.5 ? Math.PI / 2 : -Math.PI / 2);
    if (lane === LANES_X[5] && rr() < 0.4) face = Math.PI / 2;
    return { x: lane, z, face };
  };
  const planRoute = (cx, cz, t) => {
    const pts = [];
    if (cz > ZS + 0.6) { pts.push({ x: 13.5, z: cz }, { x: 13.5, z: ZS }); }
    else if (cz < ZS - 0.6) pts.push({ x: cx, z: ZS });
    if (t.counter) pts.push({ x: 13.5, z: ZS }, { x: 13.5, z: t.z }, { x: t.x, z: t.z });
    else pts.push({ x: t.x, z: ZS }, { x: t.x, z: t.z });
    return pts;
  };
  const _q = new T.Quaternion(), _y = new T.Vector3(0, 1, 0);
  crowd.update = (dt, active) => {
    parent.visible = active !== false;
    if (active === false) return;
    dt = Math.min(dt, 0.1);
    for (const n of crowd.npcs) {
      const st = n.state; let moving = false;
      if (n.role === 'dealer') {
        // slow pacing: walk to a point within 1.8 m of home, wait, face the street (+z) mostly
        if (st.route.length) {
          const p = st.route[0], dx = p.x - n.lx, dz = p.z - n.lz, d = Math.hypot(dx, dz);
          if (d < 0.1) { st.route.shift(); if (!st.route.length) st.wait = 2 + Math.random() * 5; }
          else { const s = Math.min(d, n.speed * dt); n.lx += dx / d * s; n.lz += dz / d * s; st.face = Math.atan2(-dx, -dz); moving = true; }
        } else { st.wait -= dt; st.face = Math.PI * (0.5 + 0.5 * Math.sin(n.lx)); if (st.wait <= 0) st.route.push({ x: n.home.x + (Math.random() - 0.5) * 3.6, z: n.home.z }); }
      } else if (n.role === 'cashier') {
        st.face = Math.PI + Math.sin(performance.now() * 0.0003 + n.lx) * 0.15;
      } else if (n.role === 'fries') {
        st.face = Math.PI;
      } else {
        if (st.route.length) {
          const p = st.route[0], dx = p.x - n.lx, dz = p.z - n.lz, d = Math.hypot(dx, dz);
          if (d < 0.12) { st.route.shift(); if (!st.route.length) { st.wait = 2 + Math.random() * 5; st.face = st.target ? st.target.face : st.face; } }
          else { const s = Math.min(d, n.speed * dt); n.lx += dx / d * s; n.lz += dz / d * s; const want = Math.atan2(-dx, -dz); let df = want - st.face; df = Math.atan2(Math.sin(df), Math.cos(df)); st.face += df * Math.min(1, dt * 8); moving = true; }
        } else {
          st.wait -= dt;
          if (st.wait <= 0) { st.target = pickTarget(n); st.route = planRoute(n.lx, n.lz, st.target); }
        }
      }
      // face smoothly toward st.face when idle
      let df = st.face - n.yaw; df = Math.atan2(Math.sin(df), Math.cos(df)); n.yaw += df * Math.min(1, dt * (moving ? 12 : 4));
      if (n.human) {
        const g = n.human.group; g.position.set(n.lx, FY, n.lz); g.rotation.y = n.yaw; g.scale.setScalar(1.75 * n.scale);
        try { n.human.update(dt, { moving, running: false, airborne: false, speed: 0.55 }); } catch (e) { /* ignore */ }
      }
      n.local.set(n.lx, FY, n.lz); n.pos.copy(n.local).applyMatrix4(crowd.frame);
    }
  };
  crowd.dispose = () => { crowd.npcs.forEach((n) => { if (n.human) { try { n.human.dispose(); } catch (e) { /* ignore */ } } }); crowd.npcs = []; };
  return crowd;
}

// interior descriptor (store-local boxes + frame helpers). `matrix` is store-local metres -> the owner's frame, kept current by the owner via interior.setMatrix.
function makeInterior(T, geoms) {
  const inv = new T.Matrix4(), tmp = new T.Vector3();
  const it = {
    floorY: FY, walls: boxV(T, geoms.walls), aisles: boxV(T, geoms.aisles), size: { w: W, d: D, h: H }, unit: 1, matrix: new T.Matrix4(), inverse: inv,
    setMatrix(m, unit) { it.matrix.copy(m); inv.copy(m).invert(); if (unit) it.unit = unit; },
    toParent(v, out) { return (out || new T.Vector3()).copy(v).applyMatrix4(it.matrix); },
    toStore(v, out) { return (out || new T.Vector3()).copy(v).applyMatrix4(inv); },
    inside(p) { tmp.copy(p).applyMatrix4(inv); return Math.abs(tmp.x) < HW && Math.abs(tmp.z) < HD && tmp.y > -1 && tmp.y < H + 1; },
  };
  return it;
}

// store record fields shared by the planet world and the station (menu, clerk, shoppers, dealers, characters)
function fillStoreRecord(store, crowd, id) {
  store.menu = storeMenu(id, 24);
  const c = crowd.clerk;
  store.clerk = { name: c.name, seed: c.seed, role: 'cashier', known: crowd.known, lines: [c.say('greeting'), c.say('cashier'), c.say('pitch', store.menu[Math.floor(store.menu.length / 2)])], human: c.human, npc: c, say: c.say };
  store.dealers = crowd.dealers; store.shoppers = crowd.shoppers; store.characters = crowd.characters; store.crowd = crowd;
  store.burgerHouse = null;
}

// ── a self-contained store (own meshes) for the space station: opts { id, scale (parent units per metre), lex, shoppers, dealers, pads:false } ──
export function createStandaloneStore(THREE, opts) {
  const T = THREE; opts = opts || {};
  const A = makeAssets(T);
  if (opts.pads === false) { A.geoms.body.dispose(); A.geoms.emi.dispose(); A.geoms = buildStoreGeoms(T, { pads: false }); }
  const group = new T.Group(); group.name = 'standalone-store';
  const body = new T.Mesh(A.geoms.body, A.matBody), emi = new T.Mesh(A.geoms.emi, A.matEmi), sign = new T.Mesh(A.signGeo, A.matSign), glass = new T.Mesh(A.glassGeo, A.matGlass);
  [body, emi, sign, glass].forEach((m) => { m.frustumCulled = false; group.add(m); });
  glass.renderOrder = 2;
  const humans = new T.Group(); humans.name = 'store-humans'; group.add(humans);
  const crowd = createCrowd(T, humans, { id: opts.id || 'store', lex: opts.lex != null ? opts.lex : hashStr(opts.id || 'store'), shoppers: opts.shoppers, dealers: opts.dealers, known: opts.known });
  const interior = makeInterior(T, A.geoms);
  const out = {
    group, crowd, interior, tris: A.geoms.tris, assets: A,
    update(t, dt, active) { A.uniformsT.value = t; crowd.update(dt, active); },
    dispose() { crowd.dispose(); A.dispose(); if (group.parent) group.parent.remove(group); },
  };
  return out;
}
export const STORE_DIMS = { W, D, H, floorY: FY, halfW: HW, halfD: HD };
export { fillStoreRecord };

export function createWorld(THREE, ps, L, mods) {
  const T = THREE, weapons = (mods && mods.weapons) || null, parts = (mods && mods.parts) || null;
  const rngOf = (seed) => (parts && parts.mulberry) ? parts.mulberry(seed) : mulberry(seed);
  const K = HUMAN_H * L / 1.75;                            // store unit = 1 m in world units (a 1.75 m human = 0.09 L)
  const group = new T.Group(); group.name = 'ship-world';
  const world = { group, stores: [], shards: [], nearStore: null, nearBurger: null, nearNpc: null, burgerHouse: null, onShard: null, node: null, storeTris: 0, burgerTris: 0, known: new Set() };

  let A = null, matShard = null, shardGeo = null;
  let bodies = null, emis = null, signs = null, glasses = null, shardMesh = null;
  let burger = null;                                        // { node, meshes[], matrix, ... }
  let sites = [], taken = new Set();
  const _m = new T.Matrix4(), _v = new T.Vector3(), _q = new T.Quaternion(), _q2 = new T.Quaternion(), _s = new T.Vector3(), _up = new T.Vector3(), _yAxis = new T.Vector3(0, 1, 0);

  function ensureShared() {
    if (A) return;
    A = makeAssets(T); world.storeTris = A.geoms.tris;
    matShard = new T.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG_EMI, vertexColors: true, side: T.DoubleSide, uniforms: { uTime: A.uniformsT } });
    shardGeo = shardGeometry(T);
  }

  // ── sites: one per outpost, or one on the flattest land found by sampling ──
  function padFrames() {
    const out = [], R = ps.radius || 1;
    const outs = ps.outposts || [];
    for (let i = 0; i < outs.length; i++) out.push({ id: outs[i].id, name: outs[i].name, m: outs[i].pad.matrix.clone(), key: outs[i].pad.position.length() });
    if (out.length) return out;
    // fallback: sample 400 directions, score by land + flatness of a ring at 14 L, take the flattest
    const node = world.node, sd = hashStr(String(node && node.id)) + 777, r = rngOf(sd);
    let best = null;
    for (let k = 0; k < 400; k++) {
      const z = r() * 2 - 1, ph = r() * 6.2831853, rr = Math.sqrt(1 - z * z), dx = rr * Math.cos(ph), dy = z, dz = rr * Math.sin(ph);
      if (ps.landLocal && !ps.landLocal(dx * R, dy * R, dz * R)) continue;
      const hc = ps.heightLocal(dx, dy, dz);
      const e1 = (Math.abs(dy) < 0.9 ? new T.Vector3(dz, 0, -dx) : new T.Vector3(0, -dz, dy)).normalize();
      const e2 = new T.Vector3(dx, dy, dz).cross(e1).normalize(), ang = (L * 14) / R;
      let worst = 0, wet = false;
      for (let q = 0; q < 6; q++) {
        const a = q / 6 * 6.2832, px = dx + (e1.x * Math.cos(a) + e2.x * Math.sin(a)) * ang, py = dy + (e1.y * Math.cos(a) + e2.y * Math.sin(a)) * ang, pz = dz + (e1.z * Math.cos(a) + e2.z * Math.sin(a)) * ang;
        const pl = Math.hypot(px, py, pz); px / pl;
        const h = ps.heightLocal(px / pl, py / pl, pz / pl);
        if (ps.landLocal && !ps.landLocal(px / pl * R, py / pl * R, pz / pl * R)) wet = true;
        worst = Math.max(worst, Math.abs(h - hc));
      }
      if (wet) continue;
      if (!best || worst < best.s) best = { s: worst, d: new T.Vector3(dx, dy, dz), e1, e2, h: hc };
    }
    if (!best) return out;
    // basis X = e1 (pad "toward building"), Y = up, Z = X x Y
    const up = best.d, x = best.e1, z = new T.Vector3().crossVectors(x, up).normalize();
    const m = new T.Matrix4().makeBasis(x, up, z);
    m.setPosition(up.x * R * (1 + best.h), up.y * R * (1 + best.h), up.z * R * (1 + best.h));
    out.push({ id: (node ? node.id : 'planet') + '-op0', name: 'Roadside Stop', m, key: R * (1 + best.h) });
    return out;
  }

  // planet-local matrix of a store from its pad frame (store centre 9.6 L behind the pad so the glass front sits ~8.9 L away, as before)
  const _t = new T.Matrix4(), _sc = new T.Matrix4();
  function storeMatrix(padM, out) {
    out.copy(padM).multiply(_t.makeTranslation(-1.5 * L, 0.05 * L, -9.6 * L)).multiply(_sc.makeScale(K, K, K));
    return out;
  }

  function layoutStore(site) {
    storeMatrix(site.pad, site.M);
    const M = site.M, st = site.store;
    st.pos.set(0, 0, 0).applyMatrix4(M);
    const toW = (x, y, z, o) => o.set(x, y, z).applyMatrix4(M);
    toW(6.5, FY, 6.4, st.npcSpot.pos);
    st.npcSpot.facing.set(0, 0, 1).transformDirection(M);
    toW(6.5, FY, 10.9, st.counter.pos);
    st.counter.radius = 4.5 * K;
    site.node.matrix.copy(M); site.node.matrixWorldNeedsUpdate = true;
    st.interior.setMatrix(M, K);
  }

  // Burger House: first outpost, on the nearest flat-ish land, window facing the pad
  function layoutBurger(site0) {
    if (!burger || !site0) return;
    const R = ps.radius || 1, pm = site0.pad;
    const cands = [[-6, -4], [-8, -9], [5, -8], [-10, 0], [7, -3], [0, -14], [-14, -6], [12, -10]];
    let best = null;
    for (const c of cands) {
      const p = new T.Vector3(c[0] * L, 0, c[1] * L).applyMatrix4(pm), dir = p.clone().normalize();
      if (ps.landLocal && !ps.landLocal(dir.x * R, dir.y * R, dir.z * R)) continue;
      const fl = ps.floorLocal(dir.x, dir.y, dir.z); if (!isFinite(fl)) continue;
      const e1 = (Math.abs(dir.y) < 0.9 ? new T.Vector3(dir.z, 0, -dir.x) : new T.Vector3(0, -dir.z, dir.y)).normalize(), e2 = new T.Vector3().crossVectors(dir, e1).normalize();
      let worst = 0;
      for (let k = 0; k < 4; k++) {
        const a = k * 1.5708 + 0.4, q = dir.clone().addScaledVector(e1, Math.cos(a) * 2.5 * L / R).addScaledVector(e2, Math.sin(a) * 2.5 * L / R).normalize();
        if (ps.landLocal && !ps.landLocal(q.x * R, q.y * R, q.z * R)) { worst = 9; break; }
        worst = Math.max(worst, Math.abs(ps.floorLocal(q.x, q.y, q.z) - fl));
      }
      if (!best || worst < best.worst) best = { worst, dir, fl };
      if (worst < 0.03 * L) break;
    }
    if (!best) { best = { dir: new T.Vector3(0, 0, 0).setFromMatrixPosition(pm).normalize(), fl: new T.Vector3().setFromMatrixPosition(pm).length() }; best.dir.multiplyScalar(1); }
    const ground = best.dir.clone().multiplyScalar(best.fl), up = best.dir.clone();
    const padPos = new T.Vector3().setFromMatrixPosition(pm);
    const fwd = padPos.clone().sub(ground); fwd.addScaledVector(up, -fwd.dot(up)); if (fwd.lengthSq() < 1e-12) fwd.set(1, 0, 0).addScaledVector(up, -up.x); fwd.normalize();
    const xA = new T.Vector3().crossVectors(up, fwd).normalize();
    burger.matrix.makeBasis(xA, up, fwd).setPosition(ground); burger.matrix.multiply(_sc.makeScale(K, K, K));
    burger.node.matrix.copy(burger.matrix); burger.node.matrixWorldNeedsUpdate = true;
    const bh = world.burgerHouse;
    bh.pos.set(0, 0, 0).applyMatrix4(burger.matrix); bh.windowPos.set(0, FY, 4.6).applyMatrix4(burger.matrix); bh.radius = 4.2 * K;
    bh.matrix.copy(burger.matrix); bh.unit = K;
    burger.clerk.pos.set(0, FY, 0.6).applyMatrix4(burger.matrix);
  }

  function layoutShards(frames) {
    const R = ps.radius || 1, n = SHARDS_PER_PLANET, sd = hashStr(String(world.node && world.node.id)) + 4242, r = rngOf(sd);
    const info = world.shards;
    for (let i = 0; i < n; i++) {
      const f = frames[i % frames.length] || frames[0]; if (!f) break;
      const pm = f.m.elements, cx = pm[12], cy = pm[13], cz = pm[14], ux = pm[0], uy = pm[1], uz = pm[2], vx = pm[8], vy = pm[9], vz = pm[10];
      let placed = null;
      for (let tr = 0; tr < 8 && !placed; tr++) {
        const a = r() * 6.2832, rad = (9 + r() * 24) * L, c = Math.cos(a) * rad, s = Math.sin(a) * rad;
        const px = cx + ux * c + vx * s, py = cy + uy * c + vy * s, pz = cz + uz * c + vz * s, pl = Math.hypot(px, py, pz);
        const dx = px / pl, dy = py / pl, dz = pz / pl;
        if (ps.landLocal && !ps.landLocal(dx * R, dy * R, dz * R)) continue;
        const fl = ps.floorLocal(dx, dy, dz);
        placed = new T.Vector3(dx * fl, dy * fl, dz * fl);
      }
      if (!placed) { placed = new T.Vector3(cx, cy, cz); const q = placed.length() || 1; placed.multiplyScalar((ps.floorLocal(cx / q, cy / q, cz / q)) / q); }
      const sh = info[i];
      if (sh) sh.pos.copy(placed);
    }
  }

  let lastKey = '';
  function frameKey(frames) { return frames.map((f) => f.key.toFixed(3)).join('|') + '@' + (ps.radius || 0).toFixed(3); }

  function buildBurger(site0) {
    const bg = A.burgerGeoms();
    world.burgerTris = bg.tris + 6;
    const node = new T.Group(); node.name = 'burger-house'; node.matrixAutoUpdate = false;
    const body = new T.Mesh(bg.body, A.matBody), emi = new T.Mesh(bg.emi, A.matEmi), sign = new T.Mesh(A.bGeo, A.bMat);
    [body, emi, sign].forEach((m) => { m.frustumCulled = false; node.add(m); });
    const lex = hashStr((world.node ? world.node.id : 'p') + '-lex');
    const humans = new T.Group(); node.add(humans);
    const clerk = makeNpc(T, humans, { id: (world.node ? world.node.id : 'p') + '-burger-clerk', color: ROLE_COLOR.fries, role: 'fries', lex, known: world.known, x: 0, z: 0.6, yaw: 0, name: lingo.name(hashStr('fries' + lex)) });
    clerk.say = (ctx, item) => lingo.line(clerk, ctx || 'fries');
    clerk.state.face = 0; clerk.yaw = 0;
    group.add(node);
    burger = { node, meshes: [body, emi, sign], matrix: new T.Matrix4(), clerk, humans };
    world.burgerHouse = {
      id: (world.node ? world.node.id : 'p') + '-burger', name: 'BURGER HOUSE', pos: new T.Vector3(), windowPos: new T.Vector3(), radius: 4.2 * K, matrix: new T.Matrix4(), unit: K,
      clerk: { name: clerk.name, seed: lex, role: 'fries', known: world.known, human: clerk.human, npc: clerk, say: clerk.say, lines: [clerk.say('fries'), clerk.say('greeting'), clerk.say('fries')] },
      menu: [FRIES], walls: boxV(T, bg.walls), lex,
    };
  }

  function build() {
    ensureShared();
    const frames = padFrames();
    lastKey = frameKey(frames);
    // stores (data persists across relayouts; only matrices / positions refresh)
    if (!sites.length || sites.length !== frames.length) {
      disposeSites();
      const wpn = weapons;
      frames.forEach((f, i) => {
        const seed = hashStr(f.id), r = rngOf(seed), lex = hashStr((world.node ? world.node.id : 'p') + '-lex');
        const node = new T.Group(); node.name = 'store-site'; node.matrixAutoUpdate = false; group.add(node);
        const crowd = createCrowd(T, node, { id: f.id, lex, known: world.known, unit: K, frame: new T.Matrix4() });
        const wlist = wpn && wpn.weaponSetFor ? wpn.weaponSetFor(f.id, 4) : [];
        wlist.forEach((w) => { if (w.price == null) w.price = PRICE[w.cls] || 200; });
        const shield = [1, 2, 3].map((tr) => ({ id: 'shield' + tr, tier: tr, name: 'Shield Cell ' + ['I', 'II', 'III'][tr - 1], maxShield: 20, price: [150, 350, 700][tr - 1] }));
        const engine = [1, 2, 3].map((tr) => ({ id: 'engine' + tr, tier: tr, name: 'Drive Tune ' + ['I', 'II', 'III'][tr - 1], speedMul: 0.1, price: [200, 450, 900][tr - 1] }));
        const snack = { id: 'snack', name: SNACKS[Math.floor(r() * SNACKS.length)], price: 50, heal: 'full' };
        const store = {
          id: f.id, name: STORE_NAME + ' ' + (f.name || ''), pos: new T.Vector3(), npcSpot: { pos: new T.Vector3(), facing: new T.Vector3(0, 0, 1) },
          counter: { pos: new T.Vector3(), radius: 4.5 * K },
          inventory: { weapons: wlist, upgrades: { shield, engine }, snack },
          interior: makeInterior(T, A.geoms), lineIdx: 0,
        };
        fillStoreRecord(store, crowd, f.id);
        crowd.frame = new T.Matrix4();           // planet-local frame of this store (layoutStore keeps it equal to site.M)
        sites.push({ pad: f.m.clone(), M: crowd.frame, store, node, crowd });
        world.stores.push(store);
      });
      // Burger House (first outpost only)
      buildBurger(sites[0]);
      if (world.stores[0]) world.stores[0].burgerHouse = world.burgerHouse;
      // shard records
      world.shards = [];
      for (let i = 0; i < SHARDS_PER_PLANET; i++) { const id = (world.node ? world.node.id : 'p') + '-sh' + i; world.shards.push({ id, pos: new T.Vector3(), taken: taken.has(id) }); }
      // instanced meshes
      const n = Math.max(1, sites.length);
      bodies = new T.InstancedMesh(A.geoms.body, A.matBody, n); emis = new T.InstancedMesh(A.geoms.emi, A.matEmi, n); signs = new T.InstancedMesh(A.signGeo, A.matSign, n); glasses = new T.InstancedMesh(A.glassGeo, A.matGlass, n);
      glasses.renderOrder = 2;
      shardMesh = new T.InstancedMesh(shardGeo, matShard, SHARDS_PER_PLANET);
      const palette = [0x5CE8FF, 0xFF5CE1, 0x8A6CFF];
      world.shards.forEach((s, i) => shardMesh.setColorAt(i, new T.Color(palette[i % 3]).multiplyScalar(1.35)));
      shardMesh.instanceColor.needsUpdate = true;
      [bodies, emis, signs, glasses, shardMesh].forEach((m) => { m.frustumCulled = false; group.add(m); });
      bodies.name = 'store-bodies'; emis.name = 'store-emissive'; signs.name = 'store-signs'; glasses.name = 'store-glass'; shardMesh.name = 'shards';
      bodies.count = emis.count = signs.count = glasses.count = sites.length;
    }
    frames.forEach((f, i) => { sites[i].pad.copy(f.m); layoutStore(sites[i]); });
    layoutBurger(sites[0]);
    layoutShards(frames);
    for (let i = 0; i < sites.length; i++) {
      bodies.setMatrixAt(i, sites[i].M); emis.setMatrixAt(i, sites[i].M); signs.setMatrixAt(i, sites[i].M); glasses.setMatrixAt(i, sites[i].M);
    }
    bodies.instanceMatrix.needsUpdate = emis.instanceMatrix.needsUpdate = signs.instanceMatrix.needsUpdate = glasses.instanceMatrix.needsUpdate = true;
    group.updateMatrixWorld(true);
  }

  function disposeSites() {
    sites.forEach((s) => { s.crowd.dispose(); group.remove(s.node); });
    if (burger) { if (burger.clerk.human) { try { burger.clerk.human.dispose(); } catch (e) { /* ignore */ } } group.remove(burger.node); burger = null; }
    world.burgerHouse = null;
    sites = []; world.stores = [];
    [bodies, emis, signs, glasses, shardMesh].forEach((m) => { if (m) { group.remove(m); m.dispose(); } });
    bodies = emis = signs = glasses = shardMesh = null;
  }

  world.attach = function (node) {
    world.detach(); world.node = node; taken = new Set(); build();
    return world;
  };
  world.detach = function () { disposeSites(); world.shards = []; world.node = null; world.nearStore = world.nearBurger = world.nearNpc = null; lastKey = ''; };
  world.markTaken = function (ids) { (ids || []).forEach((id) => taken.add(id)); world.shards.forEach((s) => { if (taken.has(s.id)) s.taken = true; }); };
  world.takeShard = function (id) {
    const s = world.shards.find((q) => q.id === id);
    if (!s || s.taken) return null;
    s.taken = true; taken.add(id); return s;
  };
  world.setKnown = function (set) {
    world.known = set || new Set();
    sites.forEach((s) => { s.crowd.setKnown(world.known); s.store.clerk.known = world.known; });
    if (burger) { burger.clerk.known = world.known; world.burgerHouse.clerk.known = world.known; }
  };
  function findNpc(id) {
    for (const s of sites) {
      if (s.store.id === id) return { npc: s.crowd.clerk, store: s.store };
      const n = s.crowd.npcs.find((q) => q.id === id || q.name === id); if (n) return { npc: n, store: s.store };
    }
    if (world.burgerHouse && (id === 'burger' || id === world.burgerHouse.id || id === burger.clerk.id || id === burger.clerk.name)) return { npc: burger.clerk, store: null };
    return null;
  }
  world.npcSay = function (id, ctx) { const f = findNpc(id); if (!f) return ''; f.npc.known = world.known; return f.npc.say(ctx); };
  const CYCLE = ['greeting', 'pitch', 'cashier', 'gossip'];
  world.npcLine = function (id) {
    const f = findNpc(id); if (!f) return '';
    const n = f.npc; n.known = world.known;
    let ctx = n.role === 'fries' ? 'fries' : n.role === 'dealer' ? 'dealer' : n.role === 'cashier' ? CYCLE[(f.store ? f.store.lineIdx++ : 0) % CYCLE.length] : ['shopper', 'gossip', 'warning', 'lore'][(n._cyc = (n._cyc | 0) + 1) % 4];
    if (ctx === 'pitch' && f.store) return n.say('pitch', f.store.menu[Math.floor(Math.random() * f.store.menu.length)]);
    if (ctx === 'dealer' && n.menu) return n.say('dealer', n.menu[Math.floor(Math.random() * n.menu.length)]);
    return n.say(ctx);
  };

  world.update = function (t, dt, pl) {
    if (!world.node || !bodies) return;
    A.uniformsT.value = t;
    // planet rescaled (pilot blend) or outposts re-laid: re-seat everything
    const outs = ps.outposts || [];
    let key = '';
    if (outs.length) key = outs.map((o) => o.pad.position.length().toFixed(3)).join('|') + '@' + (ps.radius || 0).toFixed(3);
    else key = lastKey;
    if (outs.length && key !== lastKey) { build(); }
    else if (!outs.length && (ps.radius || 0).toFixed(3) !== lastKey.split('@')[1]) { build(); }
    // crowds: only animate stores near the player
    const near2 = (30 * L) * (30 * L);
    for (const s of sites) { const act = !pl || pl.distanceToSquared(s.store.pos) < near2; s.crowd.update(dt, act); }
    if (burger) {
      const act = !pl || pl.distanceToSquared(world.burgerHouse.pos) < near2;
      burger.humans.visible = act;
      if (act) { const c = burger.clerk, g = c.human && c.human.group; if (g) { g.position.set(0, FY, 0.6); g.rotation.y = 0; g.scale.setScalar(1.75); try { c.human.update(dt, { moving: false }); } catch (e) { /* ignore */ } } }
    }
    // shards: float, spin, shrink when taken
    const sc = 0.5 * L;
    for (let i = 0; i < world.shards.length; i++) {
      const sh = world.shards[i];
      if (sh.taken) { _s.set(0, 0, 0); } else _s.set(sc * 0.55, sc, sc * 0.55);
      _up.copy(sh.pos).normalize();
      _q.setFromUnitVectors(_yAxis, _up); _q2.setFromAxisAngle(_yAxis, t * 0.9 + i * 1.3); _q.multiply(_q2);
      const lift = (0.85 + 0.12 * Math.sin(t * 1.6 + i * 2.1)) * L;
      _v.copy(sh.pos).addScaledVector(_up, lift);
      _m.compose(_v, _q, _s); shardMesh.setMatrixAt(i, _m);
    }
    shardMesh.instanceMatrix.needsUpdate = true;
    // proximity
    world.nearStore = null; world.nearBurger = null; world.nearNpc = null;
    if (pl) {
      for (const st of world.stores) if (pl.distanceTo(st.counter.pos) < st.counter.radius) { world.nearStore = st; break; }
      if (world.burgerHouse && pl.distanceTo(world.burgerHouse.windowPos) < world.burgerHouse.radius) world.nearBurger = world.burgerHouse;
      let bd = 3.5 * K * 3.5 * K;
      for (const s of sites) for (const n of s.crowd.npcs) { if (n.role === 'cashier') continue; const d2 = pl.distanceToSquared(n.pos); if (d2 < bd) { bd = d2; world.nearNpc = n; } }
      const pr = 1.6 * L;
      for (const sh of world.shards) if (!sh.taken && pl.distanceToSquared(_v.copy(sh.pos).addScaledVector(_up.copy(sh.pos).normalize(), 0.85 * L)) < pr * pr) { world.takeShard(sh.id); if (world.onShard) world.onShard(sh); }
    }
  };
  world.dispose = function () {
    world.detach();
    if (A) { A.dispose(); matShard.dispose(); shardGeo.dispose(); A = null; }
    if (group.parent) group.parent.remove(group);
  };
  return world;
}
