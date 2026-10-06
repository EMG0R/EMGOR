// GOR BRAWL fighters: data-only stat + move tables (4 fighters, pass 1).
// Coordinates: pivot = feet centre, +x forward (mirrored by facing), -y up. Units px / frames.
// Hit cell: seg = capsule segment [x0,y0,x1,y1], r = radius, ang in degrees (0 fwd, 90 up, 361 = AUTO).
// All original names and art. Sim-only: no DOM.

export const AUTO = 361;
const H = (s, e, seg, r, dmg, bkb, gr, ang, x) => Object.assign({ s, e, seg, r, dmg, bkb, gr, ang }, x || {});
const rise = (f, px) => { f.vy = -Math.sqrt(2 * f.def.grav * px); };
const dirX = (f) => (f.inp & 2 ? 1 : 0) - (f.inp & 1 ? 1 : 0);

// generic moves every fighter shares (utilt / dtilt / aerials); k scales damage
function generic(k, o) {
  o = o || {};
  const g = {
    utilt: { name: 'utilt', dur: 24, pose: 'swingU', hits: [H(5, 10, [-8, -46, 14, -46], 9, 6 * k, 15, 70, 90)] },
    dtilt: { name: 'dtilt', dur: 20, pose: 'swingD', hits: [H(4, 9, [10, -5, 30, -5], 7, 5 * k, 8, 30, 70)] },
    nair: { name: 'nair', dur: 28, air: true, pose: 'spin', hits: [H(3, 14, [-14, -22, 14, -22], 12, 7 * k, 10, 40, AUTO)] },
    fair: { name: 'fair', dur: 36, air: true, pose: 'swing', hits: [H(9, 13, [12, -26, 30, -16], 10, 11 * k, 18, 80, 40)] },
    bair: { name: 'bair', dur: 32, air: true, pose: 'kick', back: true, hits: [H(5, 10, [-30, -24, -12, -22], 10, 10 * k, 16, 85, 145)] },
    uair: { name: 'uair', dur: 30, air: true, pose: 'swingU', hits: [H(5, 12, [-6, -48, 10, -54], 11, 8 * k, 12, 70, 90)] },
    dair: { name: 'dair', dur: 38, air: true, pose: 'dive', hits: [H(9, 18, [-3, 3, 3, 9], 10, 9 * k, 12, 50, 290)] },
  };
  return Object.assign(g, o);
}

function chain(arr) { // jab chain: each next when A buffered after hit window
  arr.forEach((m, i) => { if (arr[i + 1]) { m.next = 'jab' + (i + 2); m.cancelFrom = m.hits[0].s + 1; } });
  return arr;
}


const multi = (s0, n, gap, mk) => Array.from({ length: n }, (_, i) => mk(s0 + i * gap, i));

export const FIGHTERS = {
  pilot: {
    id: 'pilot', name: 'PILOT', color: '#7d7df0', weight: 100, jump: 60, fall: 1.7, run: 1.5, grav: 0.095, air: 0.10, jumps: 1, shortHop: 0.55,
    blurb: 'ALL-ROUNDER. PLASMA BALL, BOOST PUNCH.', hint: { range: 28, style: 'zoner', ranged: true },
    moves: (() => {
      const m = generic(1);
      const [j1, j2, j3] = chain([
        { name: 'jab1', dur: 14, pose: 'swing', hits: [H(3, 5, [8, -24, 26, -24], 8, 3, 5, 10, AUTO)] },
        { name: 'jab2', dur: 14, pose: 'swing', hits: [H(3, 5, [8, -22, 26, -22], 8, 3, 5, 10, AUTO)] },
        { name: 'jab3', dur: 22, pose: 'kick', hits: [H(4, 7, [8, -22, 30, -26], 9, 5, 12, 60, 45)] },
      ]);
      m.jab1 = j1; m.jab2 = j2; m.jab3 = j3;
      m.ftilt = { name: 'ftilt', dur: 33, pose: 'kick', hits: [H(8, 11, [6, -26, 30, -24], 11, 10, 20, 80, 40)],
        tick(sim, f, t) { if (t >= 6 && t <= 10) f.vx = f.face * 1.9; } };
      m.bN = { name: 'plasma', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 0 && (f.cd.ball > 0 || sim.countProj(f, 'plasma') >= 2)) return true;
          if (t === 9) { f.cd.ball = 20; sim.proj(f, { kind: 'plasma', ox: 16, oy: -22, vx: 3.2, vy: -0.4, g: 0.07, bounce: 0.85, life: 120, r: 6, dmg: 5, bkb: 6, gr: 8, ang: -5 }); sim.sfx('shot'); }
        } };
      m.bU = { name: 'boost', dur: 40, pose: 'swingU', noGrav: [5, 14],
        hits: [H(5, 8, [8, -30, 16, -44], 10, 14, 45, 90, 80, { id: 'sweet' }), H(8, 13, [8, -30, 16, -44], 10, 9, 25, 70, 80)],
        tick(sim, f, t) {
          if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); }
          if (t >= 5 && t <= 14) { f.vy = -6.2; f.vx = dirX(f) * 1.5 + f.face * 0.5; }
          if (t === 15) { f.vy = -1; f.vx *= 0.5; }
        } };
      m.bD = { name: 'flip', dur: 26, pose: 'spin', hits: [H(4, 9, [-12, -24, 12, -24], 14, 5, 10, 40, AUTO)],
        tick(sim, f, t) { if (t === 3) f.reflectT = 8; } };
      return m;
    })(),
  },

  hopper: {
    id: 'hopper', name: 'HOPPER', color: '#5fbf6a', weight: 98, jump: 55, fall: 1.5, run: 1.3, grav: 0.09, air: 0.11, jumps: 1, flutter: true, shortHop: 0.55,
    blurb: 'EGG TRAPS, FLUTTER JUMP, GROUND POUND.', hint: { range: 34, style: 'allround', ranged: true },
    moves: (() => {
      const m = generic(1);
      m.jab1 = { name: 'tongue', dur: 22, pose: 'swing', hits: [H(5, 8, [8, -20, 36, -20], 5, 3, 5, 10, AUTO)] };
      m.ftilt = { name: 'headbutt', dur: 30, pose: 'kick', armor: [6, 10], hits: [H(10, 13, [10, -28, 22, -26], 10, 12, 22, 85, 40)],
        tick(sim, f, t) { if (t >= 7 && t <= 12) f.vx = f.face * 1.2; } };
      m.bN = { name: 'egglay', dur: 40, pose: 'cast',
        hits: [H(10, 14, [8, -20, 40, -20], 6, 0, 0, 0, 0, { egg: true, noKb: true })],
        tick(sim, f, t) { if (t === 0) { if (f.cd.egg > 0) return true; f.cd.egg = 40; sim.sfx('shot'); } } };
      m.bU = { name: 'eggthrow', dur: 34, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 0) { if (!f.grounded) { if (f.upUsed) return true; f.upUsed = true; rise(f, 40); } }
          if (t === 12) { sim.proj(f, { kind: 'egg', ox: 14, oy: -26, vx: 2.8, vy: -2.2, g: 0.09, bounce: 0.6, life: 150, r: 6, dmg: 12, bkb: 25, gr: 80, ang: AUTO, hits: 2 }); sim.sfx('shot'); }
        } };
      m.bDAir = { name: 'pound', dur: 70, pose: 'dive', air: true, noGrav: [0, 69], landLag: 10,
        hits: [H(14, 69, [-8, -2, 8, 4], 12, 12, 30, 90, 290)],
        tick(sim, f, t) {
          if (t < 14) { f.vx = 0; f.vy = 0; }
          else { f.vy = 7; f.vx = 0; if (f.grounded) { f.mf = 60; } }
        } };
      return m;
    })(),
  },

  builder: {
    id: 'builder', name: 'BUILDER', color: '#3ab0d0', weight: 100, jump: 58, fall: 1.75, run: 1.35, grav: 0.098, air: 0.09, jumps: 1, shortHop: 0.55,
    blurb: 'BLOCKS, MINING, TNT, ANVIL, MINECART.', hint: { range: 30, style: 'allround', ranged: false }, res0: 6,
    moves: (() => {
      const m = generic(1);
      m.jab1 = { name: 'pick', dur: 24, pose: 'swing', hits: [H(6, 8, [8, -24, 28, -20], 8, 4, 8, 20, AUTO)] };
      m.ftilt = { name: 'hack', dur: 37, pose: 'swing', hits: [H(10, 13, [8, -30, 34, -14], 9, 10, 20, 85, 40)] };
      m.bN = { name: 'place', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 10) sim.placeBlock(f, 'block'); } };
      m.bF = { name: 'mine', dur: 28, pose: 'swing', hits: [H(8, 10, [8, -20, 30, -16], 9, 5, 10, 30, AUTO)],
        tick(sim, f, t) { if (t === 8) sim.mineBlock(f); } };
      m.bU = { name: 'cart', dur: 60, pose: 'kick', cart: true,
        hits: [H(6, 50, [-10, -18, 16, -14], 12, 8, 25, 80, 45)],
        tick(sim, f, t) {
          if (t === 0) { if (f.res < 4 || (f.upUsed && !f.grounded)) return true; f.res -= 4; if (!f.grounded) f.upUsed = true; }
          if (t >= 6 && t <= 50) { f.vx = f.face * 3.2; }
          if (t === 8) f.vy = -3.0;
        } };
      m.bD = { name: 'tnt', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && f.res < 2) return true; if (t === 10) { f.res -= 2; sim.placeBlock(f, 'tnt'); } } };
      m.bDAir = { name: 'anvil', dur: 90, pose: 'dive', air: true, noGrav: [0, 89], landLag: 12,
        hits: [H(60, 89, [-6, -6, 6, -2], 12, 16, 40, 130, 290)],
        tick(sim, f, t) {
          if (t === 0 && f.res < 3) return true;
          if (t === 0) f.res -= 3;
          if (t < 60) { f.vx = 0; f.vy = 0; } else { f.vy = 8; f.vx = 0; if (f.grounded) f.mf = 82; }
        } };
      return m;
    })(),
  },

  swift: {
    id: 'swift', name: 'SWIFT', color: '#b08cf0', weight: 90, jump: 62, fall: 2.3, run: 1.7, grav: 0.12, air: 0.10, jumps: 1, shortHop: 42 / 62,
    blurb: 'FAST. BLASTER, REFLECTOR, ILLUSION DASH.', hint: { range: 24, style: 'rush', ranged: true },
    moves: (() => {
      const m = generic(0.9);
      m.jab1 = { name: 'jab', dur: 9, pose: 'swing', next: 'jab1', cancelFrom: 3, hits: [H(2, 3, [8, -22, 24, -22], 6, 2, 3, 10, AUTO)] };
      m.ftilt = { name: 'dashkick', dur: 25, pose: 'kick', hits: [H(6, 9, [8, -14, 32, -12], 8, 11, 24, 90, 40)],
        tick(sim, f, t) { if (t >= 4 && t <= 9) f.vx = f.face * 2.6; } };
      m.bN = { name: 'blaster', dur: 18, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 6) { sim.proj(f, { kind: 'bolt', ox: 16, oy: -22, vx: 6, vy: 0, g: 0, life: 70, r: 3, dmg: 3, bkb: 0, gr: 0, ang: 0, flinch: false }); sim.sfx('shot'); } } };
      m.bF = { name: 'reflector', dur: 24, pose: 'spin', hits: [H(2, 5, [-10, -22, 22, -22], 14, 1, 8, 20, 80)],
        tick(sim, f, t) { if (t === 2) { f.reflectT = 6; f.inv = Math.max(f.inv, 6); sim.sfx('reflect'); } } };
      m.bU = { name: 'illusion', dur: 30, pose: 'dive', noGrav: [6, 30], dash: true,
        hits: [H(6, 14, [-8, -24, 12, -24], 14, 5, 16, 60, AUTO)],
        tick(sim, f, t) {
          if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; }
          if (t === 6) {
            let dx = dirX(f), dy = (f.inp & 4 ? -1 : 0) + (f.inp & 8 ? 1 : 0), L = 1;
            if (!dx && !dy) { dx = f.grounded ? f.face : sim.towardCenter(f); dy = f.grounded ? 0 : -0.5; L = Math.sqrt(dx * dx + dy * dy); }
            else if (dx && dy) L = 1.4142;
            f.dashx = dx / L; f.dashy = dy / L; if (dx) f.face = dx > 0 ? 1 : -1;
            sim.sfx('boost');
          }
          if (t >= 6 && t < 14) { f.vx = f.dashx * 16.25; f.vy = f.dashy * 16.25; }
          if (t === 14) { f.vx = f.dashx * 1.5; f.vy = f.dashy * 1.5; }
        } };
      return m;
    })(),
  },

  twin: {
    id: 'twin', name: 'TWIN', color: '#4ad08a', weight: 95, jump: 72, fall: 1.45, run: 1.3, grav: 0.08, air: 0.12, jumps: 1, shortHop: 0.55,
    blurb: 'FLOATY. MISFIRE ROCKET, CYCLONE.', hint: { range: 28, style: 'allround', ranged: true },
    moves: (() => {
      const m = generic(0.95);
      m.jab1 = { name: 'jab', dur: 14, pose: 'swing', hits: [H(4, 6, [8, -24, 26, -24], 8, 3, 5, 10, AUTO)] };
      m.ftilt = { name: 'dashhit', dur: 30, pose: 'kick', hits: [H(11, 14, [6, -26, 32, -24], 11, 12, 22, 85, 40)], tick(sim, f, t) { if (t >= 8 && t <= 12) f.vx = f.face * 2.2; } };
      m.bN = { name: 'plasma', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 0 && (f.cd.ball > 0 || sim.countProj(f, 'plasma') >= 2)) return true;
          if (t === 9) { f.cd.ball = 24; sim.proj(f, { kind: 'plasma', ox: 16, oy: -22, vx: 2.4, vy: -0.2, g: 0.12, bounce: 0.6, life: 110, r: 6, dmg: 4, bkb: 5, gr: 6, ang: -5 }); sim.sfx('shot'); }
        } };
      m.bF = { name: 'slide', dur: 30, pose: 'kick', hits: [H(5, 16, [6, -8, 30, -6], 8, 8, 18, 70, 40)], tick(sim, f, t) { if (t >= 4 && t <= 16) f.vx = f.face * 2.6; } };
      m.bU = { name: 'rockethop', dur: 62, pose: 'swingU', noGrav: [4, 52],
        hits: [H(6, 12, [4, -30, 14, -44], 10, 7, 20, 60, 90, { cond: (f) => f.fx !== 1 }), H(46, 52, [4, -30, 16, -52], 14, 23, 60, 110, 90, { cond: (f) => f.fx === 1 })],
        tick(sim, f, t) {
          if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; f.fx = sim.rnd(f.i * 7 + 1) < 0.125 ? 1 : 0; sim.sfx('boost'); }
          if (f.fx !== 1) { if (t >= 5 && t <= 12) { f.vy = -5.4; f.vx = dirX(f) * 1.2; } if (t === 13) f.vy = -0.8; if (t >= 26) return true; }
          else { if (t < 45) { f.vx *= 0.9; f.vy = -0.2; if (t === 20) sim.sfx('charge'); } if (t >= 45 && t <= 50) f.vy = -7.6; if (t === 51) f.vy = -1; }
        } };
      m.bD = { name: 'cyclone', dur: 52, pose: 'spin',
        hits: [...multi(4, 4, 6, (s) => H(s, s + 3, [-14, -24, 14, -24], 15, 2, 6, 10, 90)), H(28, 34, [-14, -24, 14, -30], 16, 6, 25, 60, 80)],
        tick(sim, f, t) { if (t >= 4 && t <= 34) { f.vx = dirX(f) * 0.9; if (!f.grounded) f.vy = Math.min(f.vy, -0.5); } } };
      return m;
    })(),
  },

  hauler: {
    id: 'hauler', name: 'HAULER', color: '#e08a30', weight: 135, jump: 48, fall: 1.9, run: 1.1, grav: 0.115, air: 0.07, jumps: 1, shortHop: 0.55, hurtR: 1.2, carry: true,
    blurb: 'HEAVY. GIANT PUNCH, CARGO THROW.', hint: { range: 34, style: 'grappler', ranged: false, kclose: true },
    throws: { f: { dmg: 11, bkb: 30, gr: 90, ang: 38 }, b: { dmg: 12, bkb: 28, gr: 95, ang: 145 }, u: { dmg: 9, bkb: 30, gr: 80, ang: 88 }, d: { dmg: 8, bkb: 22, gr: 60, ang: 80 } },
    moves: (() => {
      const m = generic(1.15);
      m.jab1 = { name: 'jab', dur: 14, pose: 'swing', hits: [H(4, 6, [10, -24, 30, -24], 10, 4, 6, 12, AUTO)] };
      m.ftilt = { name: 'haymaker', dur: 40, pose: 'swing', armor: [10, 20], hits: [H(20, 24, [10, -28, 38, -24], 13, 15, 35, 100, 38)] };
      m.bN = { name: 'giantpunch', dur: 30, pose: 'swing',
        hits: [H(10, 14, [10, -28, 42, -24], 15, 8, 20, 80, 38, { dyn: (a) => { const c = a.fx / 120; return { dmg: 8 + 20 * c, bkb: 20 + 40 * c, gr: 80 + 50 * c }; } })],
        tick(sim, f, t) {
          if (t === 0) { f.fx = 0; f.fy = 0; }
          if (t === 6 && !f.fy) { if ((f.inp & 32) && f.fx < 120) { f.fx++; f.mf--; f.armorNow = 1; if (f.fx === 60) sim.sfx('charge'); return; } f.fy = 1; }
          if (t === 6) sim.sfx('boost');
        } };
      m.bF = { name: 'ram', dur: 34, pose: 'kick', hits: [H(8, 16, [6, -24, 28, -20], 14, 10, 25, 80, 40)], tick(sim, f, t) { if (t >= 6 && t <= 16) f.vx = f.face * 2.2; } };
      m.bU = { name: 'spinup', dur: 52, pose: 'spin', noGrav: [4, 34],
        hits: [...multi(4, 5, 4, (s) => H(s, s + 2, [-14, -26, 14, -26], 15, 1.5, 6, 10, 90)), H(26, 30, [-6, -34, 10, -50], 14, 6, 30, 80, 80)],
        tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; } if (t >= 4 && t <= 20) { f.vy = -2.3; f.vx = dirX(f) * 0.9; } if (t === 21) f.vy = -0.6; } };
      m.bD = { name: 'stomp', dur: 40, pose: 'swingD', hits: [H(8, 11, [-32, -6, 32, -6], 9, 5, 10, 30, 70), H(20, 23, [-32, -6, 32, -6], 9, 5, 10, 30, 70)] };
      return m;
    })(),
  },

  ranger: {
    id: 'ranger', name: 'RANGER', color: '#7aa05a', weight: 104, jump: 58, fall: 1.8, run: 1.3, grav: 0.1, air: 0.09, jumps: 1, shortHop: 0.55,
    blurb: 'BOMBS, BOOMERANG, GRAPPLE RECOVERY.', hint: { range: 30, style: 'zoner', ranged: true },
    moves: (() => {
      const m = generic(1);
      const [j1, j2, j3] = chain([
        { name: 'jab1', dur: 14, pose: 'swing', hits: [H(4, 6, [8, -24, 30, -24], 8, 3, 5, 10, AUTO)] },
        { name: 'jab2', dur: 14, pose: 'swing', hits: [H(4, 6, [8, -22, 30, -22], 8, 3, 5, 12, AUTO)] },
        { name: 'jab3', dur: 22, pose: 'swing', hits: [H(5, 8, [8, -22, 34, -26], 9, 5, 16, 40, 45)] },
      ]);
      m.jab1 = j1; m.jab2 = j2; m.jab3 = j3;
      m.ftilt = { name: 'overhead', dur: 36, pose: 'swingU', hits: [H(15, 18, [8, -52, 28, -10], 10, 11, 22, 85, 45)] };
      m.bN = { name: 'bomb', dur: 28, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 0 && (f.cd.bomb > 0 || sim.countProj(f, 'bomb') >= 2)) return true;
          if (t === 10) { f.cd.bomb = 24; sim.proj(f, { kind: 'bomb', ox: 10, oy: -24, vx: 2.2, vy: -1.9, g: 0.1, bounce: 0.35, life: 220, r: 5, dmg: 0, bkb: 0, gr: 0, ang: AUTO, noHit: true, fuse0: 12, fuse: -1, explode: { r: 34, dmg: 8, bkb: 22, gr: 80, ang: AUTO } }); sim.sfx('shot'); }
        } };
      m.bF = { name: 'boomerang', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 0 && sim.countProj(f, 'rang') > 0) return true;
          if (t === 8) { sim.proj(f, { kind: 'rang', ox: 14, oy: -24, vx: 3.4, vy: 0, g: 0, life: 300, r: 6, dmg: 5, bkb: 8, gr: 30, ang: 35, hits: 99, ghost: true, mode: 0, dist: 0 }); sim.sfx('shot'); }
        } };
      m.bU = { name: 'spinblade', dur: 40, pose: 'spin', noGrav: [4, 24],
        hits: [...multi(4, 3, 4, (s) => H(s, s + 2, [-16, -26, 16, -26], 14, 3, 6, 60, 90)), H(16, 21, [-8, -34, 12, -52], 14, 9, 25, 90, 90)],
        tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); } if (t >= 4 && t <= 18) { f.vy = -3.1; f.vx = dirX(f) * 1.2; } if (t === 19) f.vy = -0.8; } };
      m.bD = { name: 'grapple', dur: 34, pose: 'cast', hook: true, hits: [H(7, 12, [10, -26, 44, -26], 4, 2, 0, 0, AUTO, { flinch: false })],
        tick(sim, f, t) {
          if (t === 0) { f.fz = 0; }
          if (t === 5) { const a = sim.hook(f); if (a) { f.fx = a.x; f.fy = a.y; f.fz = 1; f.face = a.x >= f.x ? 1 : -1; sim.sfx('shot'); } }
          if (t >= 5 && t < 5) return false;
          if (t >= 5 && !f.fz && t >= 18) return true;
          if (f.fz && t >= 8) {
            const dx = f.fx - f.x, dy = (f.fy - f.y), d = Math.sqrt(dx * dx + dy * dy);
            if (d < 7 || t > 28) { f.vx = dx * 0.1; f.vy = Math.min(0, dy * 0.1); f.fz = 0; return true; }
            f.vx = dx / d * 5.6; f.vy = dy / d * 5.6;
          }
        } };
      return m;
    })(),
  },

  bounty: {
    id: 'bounty', name: 'BOUNTY', color: '#e07a2a', weight: 110, jump: 54, fall: 1.6, run: 1.0, grav: 0.09, air: 0.09, jumps: 1, shortHop: 0.55,
    blurb: 'ARMORED. CHARGE SHOT, MORPH BOMBS.', hint: { range: 30, style: 'zoner', ranged: true },
    moves: (() => {
      const m = generic(1.05);
      m.jab1 = { name: 'swing', dur: 16, pose: 'swing', hits: [H(5, 7, [8, -24, 28, -22], 9, 4, 6, 12, AUTO)] };
      m.ftilt = { name: 'bash', dur: 34, pose: 'swing', hits: [H(16, 19, [8, -26, 34, -22], 11, 12, 22, 90, 38)] };
      m.bN = { name: 'chargeshot', dur: 34, pose: 'cast', hits: [],
        tick(sim, f, t) {
          if (t === 5) {
            if ((f.inp & 64)) return true; // shield while charging keeps the charge
            if ((f.inp & 32) && f.fx < 90) { f.fx++; f.mf--; if (f.fx === 90) sim.sfx('charge'); return; }
            const c = f.fx / 90; f.fx = 0;
            sim.proj(f, { kind: 'charge', ox: 16, oy: -24, vx: 5, vy: 0, g: 0, life: 90, r: 4 + 6 * c, dmg: 4 + 21 * c, bkb: 45 * c, gr: 110 * c, ang: c > 0.2 ? 45 : 0, flinch: c > 0.2, c }); sim.sfx('shot');
          }
        } };
      m.bF = { name: 'missile', dur: 24, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && (f.cd.msl > 0 || sim.countProj(f, 'missile') >= 2)) return true; if (t === 8) { f.cd.msl = 22; sim.proj(f, { kind: 'missile', ox: 16, oy: -22, vx: 4.2, vy: 0, g: 0, life: 80, r: 4, dmg: 5, bkb: 8, gr: 20, ang: AUTO }); sim.sfx('shot'); } } };
      m.bU = { name: 'screwjump', dur: 44, pose: 'spin', noGrav: [4, 30],
        hits: [...multi(4, 3, 5, (s) => H(s, s + 3, [-14, -26, 14, -26], 14, 1.5, 5, 10, 90)), H(20, 26, [-8, -34, 12, -56], 14, 5, 30, 80, 80)],
        tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); } if (t >= 4 && t <= 26) { f.vy = -3.2; f.vx = dirX(f) * 1.1; } if (t === 27) f.vy = -0.5; } };
      m.bD = { name: 'morph', dur: 56, pose: 'crouch', hits: [],
        tick(sim, f, t) {
          if (t === 0) { if (sim.countProj(f, 'mbomb') >= 3) return true; f.hurtMul = 0.62; }
          if (t === 10 || t === 30) { sim.proj(f, { kind: 'mbomb', ox: 0, oy: -6, vx: 0, vy: -0.8, g: 0.09, bounce: 0.3, life: 100, r: 4, dmg: 0, bkb: 0, gr: 0, ang: 80, noHit: true, fuse: 40, explode: { r: 26, dmg: 7, bkb: 15, gr: 70, ang: 80 } }); sim.sfx('place'); }
        } };
      return m;
    })(),
  },

  jelly: {
    id: 'jelly', name: 'JELLY', color: '#7ae0e8', weight: 80, jump: 58, jump2: 36, fall: 1.2, run: 1.1, grav: 0.07, air: 0.13, jumps: 5, shortHop: 0.55, glide: true, hurtR: 0.9,
    blurb: 'FIVE JUMPS. INHALE COPIES A SPECIAL.', hint: { range: 26, style: 'allround', ranged: false, kclose: true },
    moves: (() => {
      const m = generic(0.9);
      m.jab1 = { name: 'wobble', dur: 12, pose: 'swing', hits: [H(3, 5, [8, -22, 24, -22], 9, 3, 5, 10, AUTO)] };
      m.ftilt = { name: 'hammer', dur: 46, pose: 'swingU', hits: [H(32, 35, [8, -44, 36, -4], 12, 16, 40, 110, 45)] };
      m.bN = { name: 'inhale', dur: 66, pose: 'cast', swallow: true, hits: [],
        tick(sim, f, t) {
          if (t === 0) { f.fx = 0; f.fy = 0; }
          if (t >= 8 && t < 18 && !f.fx) { const v = sim.suck(f, 40); if (v) { f.fx = 1; f.fy = v.i; sim.sfx('grab'); } }
          if (f.fx) {
            const v = sim.fighters[f.fy];
            if (v && v.state === 'held' && v.heldBy === f.i) { v.x = f.x + f.face * 3; v.y = f.y; v.vx = 0; v.vy = 0; if (t === 46) sim.spit(f, v); }
          } else if (t >= 26) return true;
        } };
      m.bF = { name: 'dashslice', dur: 32, pose: 'kick', hits: [H(8, 18, [4, -22, 30, -20], 10, 8, 18, 70, 40)], tick(sim, f, t) { if (t >= 6 && t <= 16) f.vx = f.face * 3; } };
      m.bU = { name: 'ascend', dur: 42, pose: 'swingU', noGrav: [4, 20], hits: [H(5, 12, [6, -30, 12, -52], 10, 5, 8, 20, 90)],
        tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); } if (t >= 4 && t <= 18) { f.vy = -3.6; f.vx = dirX(f) * 1.2; } if (t === 19) f.vy = -0.4; } };
      m.bD = { name: 'stone', dur: 44, pose: 'crouch', hits: [H(2, 40, [-9, -18, 9, -4], 12, 3, 8, 20, 70)],
        tick(sim, f, t) { if (t === 0) { if (f.cd.stone > 0) return true; f.cd.stone = 180; f.pct += 10; } if (t >= 3 && t <= 38) { f.inv = Math.max(f.inv, 2); f.vx = 0; if (!f.grounded) f.vy = Math.max(f.vy, 3.4); } } };
      return m;
    })(),
  },

  sparklet: {
    id: 'sparklet', name: 'SPARKLET', color: '#f4d83a', weight: 76, jump: 58, fall: 1.7, run: 1.6, grav: 0.095, air: 0.11, jumps: 1, shortHop: 0.55, hurtR: 0.7, hurtMul: 0.82,
    blurb: 'TINY AND QUICK. DOUBLE ZIP, THUNDER.', hint: { range: 24, style: 'rush', ranged: true },
    moves: (() => {
      const m = generic(0.85);
      m.jab1 = { name: 'zapjab', dur: 10, pose: 'swing', next: 'jab1', cancelFrom: 3, hits: [H(3, 5, [8, -20, 24, -20], 7, 3, 4, 10, AUTO)] };
      m.ftilt = { name: 'tail', dur: 26, pose: 'kick', hits: [H(8, 11, [4, -18, 30, -26], 9, 9, 18, 80, 45)] };
      m.bN = { name: 'sparks', dur: 22, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && sim.countProj(f, 'zap') >= 3) return true; if (t === 7) { sim.proj(f, { kind: 'zap', ox: 12, oy: -18, vx: 4, vy: 0, g: 0, life: 90, r: 4, dmg: 4, bkb: 0, gr: 0, ang: 0, flinch: false, ghost: true }); sim.sfx('shot'); } } };
      m.bF = { name: 'tackle', dur: 42, pose: 'kick', hits: [H(10, 24, [0, -16, 22, -14], 9, 7, 20, 70, 40)], tick(sim, f, t) { if (t >= 8 && t <= 22) f.vx = f.face * 3.2; } };
      m.bU = { name: 'quickdash', dur: 46, pose: 'dive', noGrav: [4, 34], dash: true,
        hits: [H(5, 10, [-6, -24, 10, -24], 12, 3, 0, 0, AUTO, { flinch: false }), H(22, 28, [-6, -24, 10, -24], 12, 3, 0, 0, AUTO, { flinch: false })],
        tick(sim, f, t) {
          const aim = () => { let dx = dirX(f), dy = (f.inp & 4 ? -1 : 0) + (f.inp & 8 ? 1 : 0); if (!dx && !dy) dy = -1; const L = Math.sqrt(dx * dx + dy * dy); f.dashx = dx / L; f.dashy = dy / L; if (dx) f.face = dx > 0 ? 1 : -1; };
          if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; }
          if (t === 4 || t === 22) { aim(); sim.sfx('boost'); }
          if ((t >= 4 && t < 10) || (t >= 22 && t < 28)) { f.vx = f.dashx * 21.7; f.vy = f.dashy * 21.7; }
          if (t === 10 || t === 28) { f.vx = f.dashx * 1.2; f.vy = f.dashy * 1.2; }
        } };
      m.bD = { name: 'thunder', dur: 70, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && f.cd.thunder > 0) return true; if (t === 8) { f.cd.thunder = 60; sim.proj(f, { kind: 'cloud', ox: 0, oy: -80, vx: 0, vy: 0, g: 0, life: 40, r: 10, dmg: 0, bkb: 0, gr: 0, ang: 0, noHit: true, onEnd: 'strike' }); sim.sfx('charge'); } } };
      return m;
    })(),
  },

  hum: {
    id: 'hum', name: 'HUM', color: '#f8a0c8', weight: 62, jump: 58, jump2: 38, fall: 1.1, run: 1.0, grav: 0.065, air: 0.14, jumps: 5, shortHop: 0.55, glide: true, hurtR: 0.95,
    blurb: 'LIGHTEST. SING PUTS FOES TO SLEEP, REST.', hint: { range: 26, style: 'allround', ranged: false, kclose: true, noUp: true },
    moves: (() => {
      const m = generic(0.85);
      m.jab1 = { name: 'pound', dur: 14, pose: 'swing', hits: [H(4, 6, [8, -22, 24, -22], 8, 3, 5, 10, AUTO)] };
      m.ftilt = { name: 'doubleslap', dur: 28, pose: 'swing', hits: [H(8, 10, [8, -24, 28, -22], 9, 4, 8, 20, AUTO), H(14, 17, [8, -24, 30, -22], 10, 9, 18, 80, 45)] };
      m.bN = { name: 'sing', dur: 92, pose: 'cast', hits: [H(14, 18, [0, -24, 0, -24], 80, 0, 0, 0, AUTO, { sleep: true })],
        tick(sim, f, t) { if (t === 0 && f.cd.sing > 0) return true; if (t === 0) f.cd.sing = 120; if (t === 12) { sim.events.push({ t: 'ring', x: f.x, y: f.y - 24 }); sim.sfx('sing'); } if (f.grounded) f.vx = 0; } };
      m.bF = { name: 'rollout', dur: 44, pose: 'kick', hits: [H(6, 32, [0, -16, 16, -14], 10, 6, 15, 60, 40)], tick(sim, f, t) { if (t >= 6 && t <= 32) f.vx = f.face * 3.1; } };
      m.bU = { name: 'rest', dur: 170, pose: 'crouch', hits: [H(1, 4, [6, -22, 22, -24], 14, 24, 75, 165, 80)],
        tick(sim, f, t) { if (t === 0) sim.sfx('sing'); if (t >= 4) { f.vx *= 0.5; } } };
      m.bD = { name: 'balloon', dur: 38, pose: 'spin', hits: [H(8, 16, [0, -22, 0, -22], 26, 8, 30, 80, AUTO)], tick(sim, f, t) { if (t === 8) sim.events.push({ t: 'ring', x: f.x, y: f.y - 22 }); } };
      return m;
    })(),
  },

  seer: {
    id: 'seer', name: 'SEER', color: '#4a7ae0', weight: 94, jump: 58, fall: 1.65, run: 1.2, grav: 0.092, air: 0.10, jumps: 1, shortHop: 0.55,
    blurb: 'PSY SPARK, PSY BOLT RECOVERY, ABSORB.', hint: { range: 30, style: 'zoner', ranged: true },
    moves: (() => {
      const m = generic(0.95);
      const [j1, j2, j3] = chain([
        { name: 'jab1', dur: 18, pose: 'swing', hits: [H(5, 7, [8, -24, 32, -22], 8, 2, 4, 10, AUTO)] },
        { name: 'jab2', dur: 18, pose: 'swing', hits: [H(5, 7, [8, -22, 32, -22], 8, 3, 5, 12, AUTO)] },
        { name: 'jab3', dur: 26, pose: 'swing', hits: [H(6, 9, [8, -22, 36, -26], 9, 7, 18, 55, AUTO)] },
      ]);
      m.jab1 = j1; m.jab2 = j2; m.jab3 = j3;
      m.ftilt = { name: 'batswing', dur: 34, pose: 'swing', hits: [H(13, 17, [10, -30, 40, -20], 11, 14, 30, 100, 40)] };
      m.bN = { name: 'psyspark', dur: 26, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && (f.cd.psy > 0 || sim.countProj(f, 'psy') >= 2)) return true; if (t === 9) { f.cd.psy = 26; sim.proj(f, { kind: 'psy', ox: 14, oy: -24, vx: 2.2, vy: 0, g: 0, life: 120, r: 5, dmg: 5, bkb: 5, gr: 20, ang: AUTO, hy: 0 }); sim.sfx('shot'); } } };
      m.bF = { name: 'psyfire', dur: 30, pose: 'cast', hits: [],
        tick(sim, f, t) { if (t === 0 && (f.cd.fire > 0 || sim.countProj(f, 'flame') >= 1)) return true; if (t === 10) { f.cd.fire = 40; sim.proj(f, { kind: 'flame', ox: 14, oy: -22, vx: 4, vy: 0.4, g: 0.02, life: 50, r: 6, dmg: 7, bkb: 12, gr: 50, ang: AUTO }); sim.sfx('shot'); } } };
      m.bU = { name: 'psybolt', dur: 74, pose: 'cast', pkMove: true, hits: [],
        tick(sim, f, t) {
          if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; f.vx *= 0.3; }
          if (t === 8) { sim.proj(f, { kind: 'pk', ox: 10, oy: -34, vx: 0.2, vy: -2.4, g: 0, life: 150, r: 6, dmg: 0, bkb: 0, gr: 0, ang: 90, noHit: true, ghost: true, dx: 0, dy: -1 }); sim.sfx('shot'); }
          if (t >= 8 && t < 60 && !f.grounded) { f.vy = Math.min(f.vy, 0.1); }
          if (t >= 70) return true;
        } };
      m.bD = { name: 'absorb', dur: 34, pose: 'cast', hits: [], tick(sim, f, t) { if (t === 2) { f.absorbT = 12; sim.sfx('reflect'); } } };
      return m;
    })(),
  },

  racer: {
    id: 'racer', name: 'RACER', color: '#3a5ad8', weight: 104, jump: 58, fall: 2.1, run: 2.0, grav: 0.105, air: 0.09, jumps: 1, shortHop: 0.55,
    blurb: 'FASTEST. KNEE STRIKE, FLAME FIST.', hint: { range: 28, style: 'rush', ranged: false },
    moves: (() => {
      const m = generic(1.05);
      m.jab1 = { name: 'jab', dur: 10, pose: 'swing', next: 'jab1', cancelFrom: 3, hits: [H(3, 5, [8, -24, 28, -24], 8, 3, 5, 10, AUTO)] };
      m.ftilt = { name: 'knee', dur: 28, pose: 'kick', hits: [H(8, 11, [16, -24, 26, -22], 6, 24, 60, 140, 40), H(11, 14, [6, -22, 28, -20], 11, 15, 50, 130, 40)], tick(sim, f, t) { if (t >= 4 && t <= 10) f.vx = f.face * 2.4; } };
      m.bN = { name: 'flamefist', dur: 92, pose: 'swing', hits: [H(62, 67, [8, -26, 34, -24], 12, 25, 60, 130, 40)],
        tick(sim, f, t) { if (t === 0) sim.sfx('charge'); if (t >= 58 && t <= 66) f.vx = f.face * 5.5; if (t === 62) sim.events.push({ t: 'puff', x: f.x + f.face * 20, y: f.y - 24 }); } };
      m.bF = { name: 'lunge', dur: 40, pose: 'dive', dash: true, hits: [...multi(6, 3, 4, (s) => H(s, s + 4, [8, -24, 24, -22], 11, 3, 10, 30, 45)), H(18, 22, [8, -24, 28, -22], 11, 9, 25, 80, 45)],
        tick(sim, f, t) { if (t === 4) { f.dashx = f.face; f.dashy = 0; } if (t >= 6 && t <= 18) f.vx = f.face * 10.8; if (t === 19) f.vx = f.face * 1.5; } };
      m.bU = { name: 'talonclimb', dur: 40, pose: 'swingU', noGrav: [4, 22], hits: [H(5, 9, [4, -30, 14, -48], 11, 5, 18, 60, 80), H(12, 17, [4, -36, 14, -56], 11, 3, 10, 20, 85)],
        tick(sim, f, t) { if (t === 0) { if (f.upUsed && !f.grounded) return true; if (!f.grounded) f.upUsed = true; sim.sfx('boost'); } if (t >= 4 && t <= 18) { f.vy = -3.9; f.vx = dirX(f) * 1.0; } if (t === 19) f.vy = -0.8; } };
      m.bD = { name: 'divekick', dur: 36, pose: 'kick', hits: [H(6, 26, [4, -14, 24, -8], 10, 12, 30, 80, 40)],
        bDAir: { name: 'raptor', dur: 40, pose: 'dive', air: true, noGrav: [4, 30], landLag: 10, hits: [H(6, 34, [0, -6, 14, 4], 11, 12, 28, 70, 290)], tick(sim, f, t) { if (t >= 4 && t <= 30) { f.vx = f.face * 3.2; f.vy = 5.6; } } },
        tick(sim, f, t) { if (t >= 6 && t <= 20) f.vx = f.face * 3.6; } };
      m.bDAir = m.bD.bDAir;
      return m;
    })(),
  },
};

// FINALS: GOR ORB unlocks one scripted boss-limb move per fighter (shield + special). Hits are unblockable.
function mkFinal(o) {
  const start = o.start || 48, gap = o.gap || 14, n = o.n || 3, hits = [], r = o.limb === 'slab' ? 36 : o.limb === 'beam' ? 22 : o.limb === 'claw' ? 30 : 36;
  for (let i = 0; i < n; i++) {
    const last = i === n - 1, s = start + i * gap;
    let seg;
    switch (o.limb) {
      case 'beam': seg = [10, -26, o.reach || 150, -26]; break;
      case 'claw': seg = [14, -36, o.reach || 110, -8]; break;
      case 'slab': seg = [30, -90, o.reach || 70, -4]; break;
      case 'maw': seg = [-90, -22, 90, -22]; break;
      case 'storm': seg = [-120, -24, 120, -24]; break;
      default: seg = [0, -24, o.reach || 130, -24];
    }
    hits.push(H(s, s + 4, seg, r, o.dmg * (last ? 1.6 : 1), last ? (o.bkb || 70) : 14, last ? (o.gr || 140) : 0, last ? (o.ang || 45) : AUTO,
      { unblock: true, sleep: !!(o.sleep && i === 0), flinch: true }));
  }
  return { name: 'final', dur: start + n * gap + 36, final: true, limb: o.limb, pose: 'cast', noGrav: [0, 999], hits, fstart: start, fgap: gap,
    tick(sim, f, t) { f.inv = Math.max(f.inv, 3); f.vy = 0; f.vx = (o.dash && t >= start - 6 && t < start + n * gap) ? f.face * o.dash : 0; } };
}
const FINAL_SPECS = {
  pilot: { limb: 'beam', n: 5, gap: 10, dmg: 5, reach: 170 }, twin: { limb: 'storm', n: 4, gap: 12, dmg: 6 }, hauler: { limb: 'slab', n: 3, gap: 16, dmg: 10, reach: 80 },
  ranger: { limb: 'claw', n: 3, gap: 14, dmg: 8, reach: 120 }, bounty: { limb: 'beam', n: 4, gap: 14, dmg: 7, reach: 190 }, hopper: { limb: 'maw', n: 3, gap: 16, dmg: 8 },
  jelly: { limb: 'maw', n: 2, gap: 20, dmg: 9 }, swift: { limb: 'beam', n: 4, gap: 9, dmg: 6, reach: 50, dash: 5 }, sparklet: { limb: 'storm', n: 5, gap: 10, dmg: 5 },
  hum: { limb: 'wave', n: 2, gap: 22, dmg: 3, sleep: true, reach: 150 }, seer: { limb: 'wave', n: 3, gap: 14, dmg: 7, reach: 140 }, racer: { limb: 'claw', n: 2, gap: 18, dmg: 14, reach: 90, bkb: 80 },
  builder: { limb: 'slab', n: 2, gap: 18, dmg: 12, reach: 60 },
};
for (const id in FINAL_SPECS) FIGHTERS[id].moves.fin = mkFinal(FINAL_SPECS[id]);

export const FIGHTER_IDS = ['pilot', 'twin', 'hauler', 'ranger', 'bounty', 'hopper', 'jelly', 'swift', 'sparklet', 'hum', 'seer', 'racer', 'builder'];
