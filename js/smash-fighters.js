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
};

export const FIGHTER_IDS = ['pilot', 'hopper', 'builder', 'swift'];
