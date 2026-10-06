// GOR BRAWL procedural sprites: 32x48 rigs painted at load into one atlas (no image files).
// Same conventions as ship-icons.js: small fixed palettes, 1px black-violet outline pass, ordered-dither shading.
// Pixel idx map -> palette: 0 clear 1 outline 2 main 3 dark 4 light 5 skin 6 skinDark 7 accent 8 glow 9 second 10 white 11 eye 12 accentDark

export const POSES = { idle: 6, run: 8, jump: 4, fall: 4, land: 3, crouch: 1, hit: 2, tumble: 8, shield: 2, roll: 4, grab: 2, ledge: 2,
  swing: 4, swingU: 4, swingD: 4, kick: 4, cast: 4, spin: 4, dive: 2 };
const POSE_ORDER = Object.keys(POSES);
const OUT = 0x1a1226;
const W = 32, Hh = 48, COLS = 16;

const PAL = {
  //        0  1=out  2=main   3=dark   4=light  5=skin   6=skinD  7=accent 8=glow   9=second 10=white 11=eye   12=accD
  pilot:  [0, OUT, 0x5b5bd6, 0x3a3a9a, 0x8a8af0, 0xe8b890, 0xb98860, 0xf29b3a, 0x5ce8ff, 0x2c2c6a, 0xf4f0e8, 0x1a1226, 0xb8651e],
  hopper: [0, OUT, 0x5fbf6a, 0x3c8f4a, 0x9fe08a, 0xcdefa0, 0x9fc878, 0xe8a030, 0xfff0a0, 0x2f6a3a, 0xf4f0e8, 0x1a1226, 0xa8701a],
  builder:[0, OUT, 0x3aa5c8, 0x25708f, 0x7ad0e8, 0xd8a070, 0xa87040, 0xe8c040, 0xffffff, 0x6b3e1e, 0xf4f0e8, 0x1a1226, 0x9a7a20],
  swift:  [0, OUT, 0xa58bd8, 0x7458b0, 0xd0c0f4, 0xf2eaf8, 0xc4b4dc, 0xe8483e, 0xff9ad0, 0x4a3878, 0xf4f0e8, 0x1a1226, 0x9a2420],
  twin:   [0, OUT, 0x2e9a62, 0x1e6a44, 0x6fd8a0, 0xe8b890, 0xb98860, 0xd8f060, 0xfff0a0, 0x16443a, 0xf4f0e8, 0x1a1226, 0x9ab030],
  hauler: [0, OUT, 0xd87a2a, 0x9a4a18, 0xf0b060, 0xc89870, 0x987048, 0xffe060, 0xfff0a0, 0x5a4030, 0xf4f0e8, 0x1a1226, 0xa8901a],
  ranger: [0, OUT, 0x4f8a50, 0x2f5a34, 0x8cc07c, 0xe0b088, 0xb08058, 0xc8a050, 0xff7a5a, 0x6a4a2a, 0xf4f0e8, 0x1a1226, 0x8a6a28],
  bounty: [0, OUT, 0xe07a2a, 0xa04a18, 0xffb860, 0xe8b890, 0xb98860, 0xe8c040, 0x5cffb0, 0x8a2a1a, 0xf4f0e8, 0x1a1226, 0x9a8020],
  jelly:  [0, OUT, 0x7ae0e8, 0x3aa0c0, 0xc8faff, 0xff9ac8, 0xd0709c, 0xff7ab8, 0xffffff, 0x5a70d0, 0xf4f0e8, 0x1a1226, 0xb04a88],
  sparklet:[0, OUT, 0xf4d83a, 0xc4a020, 0xfff090, 0xfff0b0, 0xe0c880, 0xe84a3a, 0xfff8a0, 0x3a2a10, 0xf4f0e8, 0x1a1226, 0x9a2418],
  hum:    [0, OUT, 0xf8a0c8, 0xd070a0, 0xffd0e8, 0xffd8e8, 0xe0a8c4, 0x50c0e0, 0xffffff, 0xa04080, 0xf4f0e8, 0x1a1226, 0x2a8aa8],
  seer:   [0, OUT, 0x3a6ad0, 0x24449a, 0x7aa0f0, 0xf0c8a0, 0xc09870, 0xf0d040, 0xff9aff, 0xc84a3a, 0xf4f0e8, 0x1a1226, 0xb0804a],
  racer:  [0, OUT, 0x2a4aa8, 0x1a2e70, 0x6a8ae0, 0xd8a070, 0xa87848, 0xe84a4a, 0x5ce8ff, 0xe8e8f0, 0xf4f0e8, 0x1a1226, 0xa02a2a],
};

// ---- pose rig ---------------------------------------------------------------------------------
function pose(name, i) {
  const P = { bob: 0, lean: 0, fF: [3, 0], fB: [-3, 0], hF: [4, 9], hB: [-4, 9], head: [0, 0], fist: 0, tool: 0, rot: 0 };
  const s = Math.sin, c = Math.cos, PI2 = Math.PI * 2;
  switch (name) {
    case 'idle': { const b = [0, 0, 1, 1, 0, 0][i]; P.bob = b; P.hF = [4, 9 - b]; P.hB = [-4, 9 - b]; P.head = [0, b ? 0 : 0]; break; }
    case 'run': { const ph = i / 8 * PI2; P.fF = [s(ph) * 9, -Math.max(0, -c(ph)) * 6]; P.fB = [-s(ph) * 9, -Math.max(0, c(ph)) * 6]; P.hF = [-s(ph) * 6 + 2, 7]; P.hB = [s(ph) * 6 + 2, 7]; P.lean = 2; P.bob = -Math.abs(s(ph * 2)) * 1.2 + 1; break; }
    case 'jump': P.fF = [5, -6]; P.fB = [-2, -3]; P.hF = [6, -4]; P.hB = [-6, -2]; P.bob = -2 + (i > 1 ? 1 : 0); break;
    case 'fall': P.fF = [3, -3]; P.fB = [-3, -5]; P.hF = [8, 1 + (i & 1)]; P.hB = [-8, 1 + ((i + 1) & 1)]; P.bob = -2; break;
    case 'land': P.bob = [4, 6, 3][i]; P.lean = 1; P.fF = [5, 0]; P.fB = [-5, 0]; P.hF = [6, 6]; P.hB = [-5, 6]; break;
    case 'crouch': P.bob = 7; P.lean = 2; P.fF = [6, 0]; P.fB = [-5, 0]; P.hF = [7, 5]; P.hB = [-3, 7]; P.head = [1, 1]; break;
    case 'hit': P.lean = -4; P.head = [-2, 1 + i]; P.hF = [-8, -4]; P.hB = [-6, 3]; P.fF = [-1, -3]; P.fB = [-6, -4]; P.bob = -1; break;
    case 'shield': P.bob = 4 + i; P.lean = 1; P.fF = [5, 0]; P.fB = [-4, 0]; P.hF = [9, 1]; P.hB = [7, 5]; break;
    case 'roll': P.bob = 11; P.lean = 5; P.head = [3, 4]; P.fF = [2, -2]; P.fB = [-3, -1]; P.hF = [5, 4]; P.hB = [3, 6]; break;
    case 'grab': P.lean = 3; P.hF = [13, -2 + i]; P.hB = [11, 1]; P.fF = [5, 0]; P.fB = [-4, 0]; P.bob = 1; break;
    case 'ledge': P.hF = [3, -15]; P.hB = [-2, -15]; P.fF = [3, -1]; P.fB = [-2, -3]; P.bob = 6; P.head = [1, 3]; break;
    case 'swing': { const k = [[-8, -1], [9, -3], [15, -2], [7, 5]][i]; P.hF = k; P.hB = [-5, 8]; P.lean = [-2, 3, 3, 1][i]; P.fF = [6, 0]; P.fB = [-5, 0]; P.fist = i === 2 || i === 1 ? 1 : 0; P.tool = i; break; }
    case 'swingU': { const k = [[-3, -2], [2, -13], [4, -17], [3, -6]][i]; P.hF = k; P.hB = [-5, 8]; P.lean = [-1, 1, 2, 0][i]; P.fF = [4, 0]; P.fB = [-4, 0]; P.fist = i > 0 && i < 3 ? 1 : 0; P.tool = i; break; }
    case 'swingD': { const k = [[-3, 3], [9, 9], [13, 11], [7, 7]][i]; P.hF = k; P.hB = [-5, 7]; P.bob = 3; P.lean = 2; P.fF = [7, 0]; P.fB = [-5, 0]; P.fist = i === 1 || i === 2 ? 1 : 0; P.tool = i; break; }
    case 'kick': { const k = [[-4, 0], [10, -9], [17, -11], [6, -2]][i]; P.fF = k; P.lean = [-2, -3, -3, 0][i]; P.hF = [-6, 4]; P.hB = [5, -1]; P.fB = [-4, 0]; P.bob = 1; break; }
    case 'cast': { const k = [[2, 0], [6, -2], [11, -3], [8, -1]][i]; P.hF = k; P.hB = [k[0] - 2, k[1] + 2]; P.lean = [-1, 1, 3, 2][i]; P.fF = [6, 0]; P.fB = [-5, 0]; P.fist = i === 2 ? 1 : 0; P.bob = 1; break; }
    case 'spin': { const sg = i & 1 ? -1 : 1; P.hF = [12 * sg, -2]; P.hB = [-12 * sg, -2]; P.fF = [6 * sg, -3]; P.fB = [-6 * sg, -3]; P.bob = -1; P.fist = 1; break; }
    case 'dive': P.hF = [2, 12]; P.hB = [-2, 12]; P.fF = [2, -4]; P.fB = [-1, -3]; P.bob = -2 + i; P.fist = 1; break;
    case 'tumble': { const b = pose('hit', 0); b.rot = i * 45; return b; }
  }
  return P;
}

// ---- idx-map painting -------------------------------------------------------------------------
function Canvas() { return new Uint8Array(W * Hh); }
function put(m, x, y, c) { x |= 0; y |= 0; if (x >= 0 && y >= 0 && x < W && y < Hh) m[y * W + x] = c; }
function rect(m, x, y, w, h, c) { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) put(m, x + i, y + j, c); }
function disc(m, cx, cy, r, c) { for (let y = -r; y <= r; y++) for (let x = -r; x <= r; x++) if (x * x + y * y <= r * r + r * 0.6) put(m, cx + x, cy + y, c); }
function line(m, x0, y0, x1, y1, t, c) {
  const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
  for (let k = 0; k <= n; k++) { const x = x0 + (x1 - x0) * k / n, y = y0 + (y1 - y0) * k / n; rect(m, Math.round(x - (t - 1) / 2), Math.round(y - (t - 1) / 2), t, t, c); }
}
function ik(ax, ay, bx, by, l1, l2, sgn) {
  let dx = bx - ax, dy = by - ay, d = Math.hypot(dx, dy) || 0.01; const mx = l1 + l2 - 0.05;
  if (d > mx) { dx *= mx / d; dy *= mx / d; d = mx; bx = ax + dx; by = ay + dy; }
  const a = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = Math.sqrt(Math.max(0, l1 * l1 - a * a));
  return [ax + dx * a / d + (-dy / d) * h * sgn, ay + dy * a / d + (dx / d) * h * sgn, bx, by];
}
function limb(m, a, b, hip, t, col, sgn, l1, l2) {
  const k = ik(a[0], a[1], b[0], b[1], l1, l2, sgn);
  line(m, a[0], a[1], k[0], k[1], t, col); line(m, k[0], k[1], k[2], k[3], t, col);
  return [k[2], k[3]];
}

// ---- fighters ---------------------------------------------------------------------------------
const STYLE = {
  pilot:   { legT: 3, armT: 3, legL: 7, torsoW: 9, leg: 3, arm: 2 },
  hopper:  { legT: 3, armT: 3, legL: 6.5, torsoW: 9, leg: 3, arm: 5 },
  builder: { legT: 4, armT: 4, legL: 7, torsoW: 10, leg: 3, arm: 5 },
  swift:   { legT: 2, armT: 2, legL: 7, torsoW: 7, leg: 2, arm: 7 },
  twin:    { legT: 3, armT: 3, legL: 7, torsoW: 9 },
  hauler:  { legT: 5, armT: 5, legL: 7, torsoW: 14, handR: 2, handC: 9 },
  ranger:  { legT: 3, armT: 3, legL: 7, torsoW: 9 },
  bounty:  { legT: 4, armT: 4, legL: 7, torsoW: 11 },
  jelly:   { legT: 2, armT: 2, legL: 3.5, torsoW: 6, torsoH: 6, oy: 5, armL: 4 },
  sparklet:{ legT: 2, armT: 2, legL: 3.5, torsoW: 7, torsoH: 8, oy: 9, armL: 4 },
  hum:     { legT: 2, armT: 2, legL: 3.5, torsoW: 8, torsoH: 7, oy: 6, armL: 4 },
  seer:    { legT: 3, armT: 3, legL: 7, torsoW: 9 },
  racer:   { legT: 3, armT: 3, legL: 7, torsoW: 9, handC: 10 },
};

function drawFighter(id, P) {
  const m = Canvas(), S = STYLE[id], oy = S.oy || 0, AL = S.armL || 5.5, TH = S.torsoH || 13;
  const hx = 16 + P.lean * 0.4, hy = 31 + oy + P.bob;     // hip
  const nx = 16 + P.lean, ny = 19 + oy + P.bob;            // neck
  const hdx = nx + P.head[0], hdy = 13 + oy + P.bob + P.head[1];
  const shF = [nx + 1, ny + 2], shB = [nx - 1, ny + 2];
  const footF = [16 + P.fF[0], 45 + P.fF[1]], footB = [16 + P.fB[0], 45 + P.fB[1]];
  const handF = [shF[0] + P.hF[0], shF[1] + P.hF[1]], handB = [shB[0] + P.hB[0], shB[1] + P.hB[1]];
  const legC = id === 'builder' ? 9 : 3, footBC = id === 'pilot' ? 9 : 3, footC = id === 'pilot' ? 9 : (id === 'swift' || id === 'hopper') ? 7 : id === 'racer' ? 10 : 3;
  const tw = S.torsoW, tx0 = Math.round(hx - tw / 2 + P.lean * 0.3), R_ = Math.round;

  // back layer: tails / cape / pack
  if (id === 'hopper') { let tx = hx - 3, ty = hy; for (let k = 0; k < 4; k++) { rect(m, Math.round(tx - 3), Math.round(ty), 3, 3 - (k > 1 ? 1 : 0), 2); tx -= 3; ty += k < 2 ? 2 : -1; } rect(m, Math.round(tx - 1), Math.round(ty - 1), 2, 2, 7); }
  if (id === 'swift') { let tx = hx - 3, ty = hy - 2; for (let k = 0; k < 6; k++) { rect(m, Math.round(tx - 2), Math.round(ty), 3, 3, k > 4 ? 7 : 2); tx -= 2.4; ty -= k < 3 ? 1.6 : -0.3; } }
  if (id === 'pilot') { rect(m, nx - 8, ny + 1, 4, 11, 3); rect(m, nx - 7, ny + 11, 2, 3, 8); rect(m, nx - 10, ny + 1, 3, 7 + (P.bob < 0 ? 1 : 0), 7); }
  if (id === 'twin' || id === 'racer') { rect(m, nx - 10, ny + 1, 6, 2, 7); rect(m, nx - 13, ny + 2 + (P.bob < 0 ? 1 : 0), 4, 2, 7); }
  if (id === 'hauler') { rect(m, nx - 10, ny, 5, 11, 9); rect(m, nx - 10, ny, 5, 2, 3); rect(m, nx - 6, ny + 2, 2, 3, 3); }
  if (id === 'ranger') rect(m, nx - 8, ny + 1, 5, 12 + (P.bob < 0 ? 1 : 0), 3);
  if (id === 'bounty') { rect(m, nx - 8, ny + 1, 4, 9, 3); rect(m, nx - 8, ny + 9, 4, 2, 8); }
  if (id === 'sparklet') { let tx = hx - 3, ty = hy - 1; for (let k = 0; k < 5; k++) { rect(m, R_(tx), R_(ty), 3, 3, k === 4 ? 7 : 2); tx -= 2; ty += (k & 1) ? 3 : -5; } }
  if (id === 'hum') { rect(m, hx - 8, hy - 2, 3, 3, 2); rect(m, hx - 9, hy - 4, 2, 2, 4); }
  if (id === 'seer') { rect(m, nx - 9, ny + 1, 6, 10, 9); rect(m, nx - 9, ny + 1, 6, 2, 7); }
  // back arm + back leg
  limb(m, [shB[0], shB[1]], handB, 0, S.armT, 3, 1, AL, AL);
  disc(m, Math.round(handB[0]), Math.round(handB[1]), 1, 5);
  limb(m, [hx - 1, hy], footB, 0, S.legT, legC, -1, S.legL, S.legL);
  rect(m, Math.round(footB[0]) - 1, Math.round(footB[1]) - 1, 4, 2, footBC);
  // torso
  for (let y = 0; y < TH; y++) {
    const yy = Math.round(ny + y), lean = P.lean * (1 - y / TH) * 0.6;
    const ww = tw + (y < 3 ? 1 : 0) - (y > TH - 4 ? 1 : 0);
    rect(m, Math.round(nx - ww / 2 + lean), yy, ww, 1, 2);
  }
  if (id === 'pilot') { rect(m, tx0 + 1, Math.round(hy - 2), tw - 1, 2, 7); rect(m, tx0 + 3, Math.round(ny + 3), 4, 2, 4); rect(m, tx0 + 4, Math.round(ny + 4), 2, 1, 8); }
  if (id === 'hopper') { rect(m, tx0 + 3, Math.round(ny + 2), tw - 3, 9, 5); for (let y = 0; y < 8; y += 2) rect(m, tx0 + 4, Math.round(ny + 3 + y), tw - 4, 1, 6); }
  if (id === 'builder') { rect(m, tx0, Math.round(hy - 3), tw, 4, 9); rect(m, tx0 + 4, Math.round(hy - 3), 2, 2, 7); rect(m, tx0 + 2, Math.round(ny + 2), 2, 2, 4); }
  if (id === 'swift') { rect(m, Math.round(nx - 1), Math.round(ny + 2), 4, 8, 10); rect(m, tx0, Math.round(hy - 2), tw, 1, 7); }
  if (id === 'twin') { rect(m, tx0 + 1, R_(hy - 2), tw - 1, 2, 9); rect(m, R_(nx - 3), R_(ny), 7, 2, 7); rect(m, tx0 + 3, R_(ny + 3), 2, 2, 8); }
  if (id === 'hauler') { rect(m, tx0 + 2, R_(ny + 1), 2, 11, 9); rect(m, tx0 + tw - 4, R_(ny + 1), 2, 11, 9); rect(m, tx0, R_(hy - 3), tw, 3, 9); rect(m, tx0 + tw / 2 - 2, R_(hy - 3), 4, 3, 7); rect(m, tx0 + tw / 2 - 1, R_(ny + 3), 3, 3, 8); }
  if (id === 'ranger') { rect(m, tx0, R_(hy - 2), tw, 2, 7); disc(m, R_(hx + 4), R_(hy + 1), 3, 9); rect(m, R_(hx + 3), R_(hy - 2), 2, 1, 7); rect(m, tx0 + tw / 2 - 1, R_(ny + 1), 2, 9, 4); }
  if (id === 'bounty') { rect(m, tx0 + 2, R_(ny + 2), tw - 3, 6, 3); rect(m, tx0 + 4, R_(ny + 3), 3, 3, 7); put(m, tx0 + 5, R_(ny + 4), 8); rect(m, tx0, R_(hy - 2), tw, 2, 7); disc(m, R_(shF[0]), R_(shF[1]), 3, 7); }
  if (id === 'jelly') { for (let k = 0; k < 4; k++) rect(m, R_(hx - 5 + k * 3.3), R_(hy + (k & 1)), 2, 3 + ((k + (P.bob > 0 ? 1 : 0)) & 1), 4); }
  if (id === 'sparklet') { rect(m, tx0 + 1, R_(ny + 1), tw - 2, 5, 5); rect(m, tx0, R_(ny + 1), 2, 1, 3); rect(m, tx0, R_(ny + 4), 2, 1, 3); }
  if (id === 'hum') { rect(m, tx0 + 1, R_(ny + 1), tw - 2, 4, 5); }
  if (id === 'seer') { rect(m, tx0, R_(ny + 4), tw, 2, 7); rect(m, tx0 + tw - 3, R_(ny + 1), 1, 8, 12); }
  if (id === 'racer') { rect(m, tx0 + 1, R_(ny + 2), tw - 2, 2, 7); rect(m, tx0, R_(hy - 2), tw, 2, 7); rect(m, tx0 + tw / 2, R_(ny + 5), 3, 3, 10); }
  // front leg
  limb(m, [hx + 1, hy], footF, 0, S.legT, legC, -1, S.legL, S.legL);
  rect(m, Math.round(footF[0]) - 1, Math.round(footF[1]) - 1, 5, 2, footC);
  // head
  if (id === 'pilot') {
    disc(m, hdx, hdy, 6, 5); for (let y = -6; y <= 0; y++) for (let x = -6; x <= 6; x++) if (x * x + y * y <= 36 + 3) put(m, hdx + x, hdy + y, 2);
    rect(m, hdx - 1, hdy - 1, 8, 3, 8); rect(m, hdx + 1, hdy - 1, 3, 1, 10); rect(m, hdx - 6, hdy - 1, 4, 5, 3); rect(m, hdx - 1, hdy - 7, 3, 1, 4);
  } else if (id === 'hopper') {
    rect(m, hdx - 5, hdy - 4, 10, 8, 2); rect(m, hdx + 4, hdy - 2, 6, 5, 4); rect(m, hdx + 4, hdy + 2, 6, 1, 5);
    put(m, hdx + 9, hdy - 1, 3); rect(m, hdx + 1, hdy - 3, 3, 3, 10); rect(m, hdx + 2, hdy - 2, 2, 2, 11);
    for (let k = 0; k < 3; k++) rect(m, hdx - 4 + k * 3, hdy - 6 + (k & 1), 2, 2, 7);
  } else if (id === 'builder') {
    rect(m, hdx - 5, hdy - 5, 11, 11, 5); rect(m, hdx - 5, hdy - 5, 11, 4, 9); rect(m, hdx - 5, hdy - 5, 3, 7, 9); rect(m, hdx + 2, hdy - 1, 2, 2, 11); rect(m, hdx + 5, hdy, 1, 1, 6); rect(m, hdx - 1, hdy + 3, 5, 1, 6);
  } else if (id === 'swift') {
    disc(m, hdx, hdy, 5, 2); rect(m, hdx + 3, hdy - 1, 5, 3, 4); put(m, hdx + 7, hdy, 11);
    rect(m, hdx - 5, hdy - 1, 11, 2, 7); rect(m, hdx + 1, hdy - 2, 3, 2, 8); put(m, hdx + 2, hdy - 1, 11);
    for (const ex of [-4, 1]) for (let k = 0; k < 6; k++) rect(m, hdx + ex + (k > 3 ? 1 : 0), hdy - 5 - k, Math.max(1, 3 - (k >> 1)), 1, 2);
    rect(m, hdx - 3, hdy - 9, 1, 1, 7); put(m, hdx + 2, hdy - 8, 7);
  } else if (id === 'twin') {
    disc(m, hdx, hdy, 5, 5); rect(m, hdx - 5, hdy - 6, 11, 4, 3); rect(m, hdx - 6, hdy - 3, 3, 5, 3); rect(m, hdx + 2, hdy - 7, 4, 2, 3);
    rect(m, hdx + 1, hdy - 2, 6, 3, 10); rect(m, hdx + 2, hdy - 1, 4, 2, 8); put(m, hdx + 4, hdy + 3, 6);
  } else if (id === 'hauler') {
    rect(m, hdx - 4, hdy - 3, 9, 8, 5); rect(m, hdx - 5, hdy - 6, 11, 4, 2); rect(m, hdx - 5, hdy - 3, 2, 3, 2);
    rect(m, hdx + 2, hdy - 8, 4, 3, 7); rect(m, hdx + 5, hdy - 7, 3, 1, 8); rect(m, hdx - 3, hdy + 2, 8, 3, 9); rect(m, hdx + 2, hdy - 1, 2, 2, 10); put(m, hdx + 3, hdy, 11);
  } else if (id === 'ranger') {
    disc(m, hdx, hdy, 4, 5); rect(m, hdx - 6, hdy - 5, 12, 6, 2); rect(m, hdx - 6, hdy, 3, 5, 2); rect(m, hdx - 8, hdy - 9, 4, 4, 2); rect(m, hdx - 4, hdy - 8, 4, 3, 2);
    rect(m, hdx - 3, hdy - 2, 8, 2, 3); rect(m, hdx + 2, hdy, 2, 2, 10); put(m, hdx + 3, hdy + 1, 11);
  } else if (id === 'bounty') {
    disc(m, hdx, hdy, 6, 2); rect(m, hdx, hdy - 2, 7, 5, 8); rect(m, hdx + 1, hdy - 1, 3, 1, 10); rect(m, hdx - 4, hdy - 9, 1, 4, 3); put(m, hdx - 4, hdy - 10, 7); rect(m, hdx - 4, hdy + 3, 8, 2, 3);
  } else if (id === 'jelly') {
    disc(m, hdx, hdy + 1, 8, 2); rect(m, hdx - 5, hdy - 5, 4, 2, 4); put(m, hdx - 6, hdy - 3, 10);
    rect(m, hdx - 3, hdy, 2, 4, 11); rect(m, hdx + 2, hdy, 2, 4, 11); put(m, hdx - 3, hdy, 10); put(m, hdx + 2, hdy, 10);
    rect(m, hdx - 5, hdy + 4, 2, 1, 7); rect(m, hdx + 5, hdy + 4, 2, 1, 7); put(m, hdx, hdy + 5, 11); rect(m, hdx - 6, hdy + 7, 13, 1, 3);
  } else if (id === 'sparklet') {
    disc(m, hdx, hdy, 4, 2); rect(m, hdx - 4, hdy - 10, 2, 6, 2); rect(m, hdx - 4, hdy - 11, 2, 3, 9); rect(m, hdx + 1, hdy - 10, 2, 6, 2); rect(m, hdx + 1, hdy - 11, 2, 3, 9);
    put(m, hdx + 1, hdy - 1, 11); put(m, hdx + 3, hdy - 1, 11); rect(m, hdx + 3, hdy + 1, 2, 2, 7); put(m, hdx + 2, hdy + 2, 3);
  } else if (id === 'hum') {
    disc(m, hdx, hdy, 8, 2); rect(m, hdx - 1, hdy - 10, 3, 2, 2); rect(m, hdx + 1, hdy - 12, 3, 3, 2); put(m, hdx + 3, hdy - 12, 4);
    rect(m, hdx - 7, hdy - 8, 3, 4, 2); rect(m, hdx + 4, hdy - 8, 3, 4, 2);
    rect(m, hdx - 1, hdy - 1, 5, 5, 10); rect(m, hdx, hdy, 3, 4, 7); put(m, hdx + 1, hdy + 1, 11); rect(m, hdx - 6, hdy - 1, 4, 5, 10); rect(m, hdx - 5, hdy, 3, 4, 7); put(m, hdx - 4, hdy + 1, 11); put(m, hdx + 1, hdy + 5, 11);
  } else if (id === 'seer') {
    disc(m, hdx, hdy, 5, 5); rect(m, hdx - 5, hdy - 6, 11, 4, 2); rect(m, hdx - 6, hdy - 3, 12, 2, 2); rect(m, hdx + 1, hdy - 3, 8, 2, 2); rect(m, hdx - 1, hdy - 5, 3, 2, 7);
    rect(m, hdx - 5, hdy - 1, 2, 3, 3); rect(m, hdx + 2, hdy, 2, 2, 10); put(m, hdx + 3, hdy + 1, 11);
  } else if (id === 'racer') {
    disc(m, hdx, hdy, 6, 10); rect(m, hdx, hdy - 1, 7, 4, 8); rect(m, hdx + 1, hdy, 3, 1, 10); rect(m, hdx - 6, hdy - 3, 12, 2, 7); rect(m, hdx - 3, hdy + 4, 6, 2, 3); rect(m, hdx - 7, hdy - 1, 3, 4, 7);
  }
  // front arm
  limb(m, [shF[0], shF[1]], handF, 0, S.armT, id === 'builder' ? 5 : 2, 1, AL, AL);
  const gx0 = Math.round(handF[0]), gy0 = Math.round(handF[1]);
  if (id === 'builder') rect(m, gx0 - 1, gy0 - 1, 3, 3, 5); else disc(m, gx0, gy0, S.handR || 1, S.handC || 5);
  if (id === 'hopper') { put(m, gx0, gy0, 4); }
  if (id === 'bounty') { rect(m, gx0 - 2, gy0 - 2, 6, 5, 3); rect(m, gx0 + 3, gy0 - 1, 3, 3, 8); }
  if (P.fist) { disc(m, gx0, gy0, 3, 8); put(m, gx0, gy0, 10); }
  if (id === 'builder' && P.tool > 0 && (P.hF[0] !== 4)) { // crafting pick: handle + head, not a branded shape
    line(m, gx0, gy0 + 3, gx0 + 1, gy0 - 8, 2, 9); rect(m, gx0 - 5, gy0 - 10, 12, 3, 3); rect(m, gx0 - 5, gy0 - 10, 12, 1, 4);
  }
  if (id === 'ranger' && P.tool > 0 && (P.hF[0] !== 4)) { line(m, gx0, gy0 + 1, gx0 + 9, gy0 - 9, 2, 10); rect(m, gx0 - 2, gy0 - 1, 5, 2, 7); }
  if (id === 'seer' && P.tool > 0 && (P.hF[0] !== 4)) { line(m, gx0, gy0 + 3, gx0 + 9, gy0 - 9, 3, 12); rect(m, gx0 + 8, gy0 - 11, 3, 3, 12); }
  return m;
}

// shading + outline -> RGBA
function toRGBA(m, pal) {
  const g = (x, y) => (x < 0 || y < 0 || x >= W || y >= Hh) ? 0 : m[y * W + x];
  const out = new Uint8Array(m.length);
  const darker = { 2: 3, 5: 6, 7: 12, 9: 9 }, lighter = { 2: 4, 5: 5, 9: 9, 3: 3 };
  for (let y = 0; y < Hh; y++) for (let x = 0; x < W; x++) {
    let c = g(x, y);
    if (c && darker[c]) {
      const br = g(x + 1, y + 1), b = g(x, y + 1), r = g(x + 1, y), tl = g(x - 1, y - 1), t = g(x, y - 1), l = g(x - 1, y);
      if (c === 2 || c === 5) {
        if (!br || (!b && !r)) c = darker[c];
        else if ((!b || !r) && ((x + y) & 1)) c = darker[c];
        else if (!tl || (!t && !l)) c = lighter[c] || c;
        else if ((!t || !l) && ((x + y) & 1)) c = lighter[c] || c;
      }
    }
    out[y * W + x] = c;
  }
  for (let y = 0; y < Hh; y++) for (let x = 0; x < W; x++) if (!out[y * W + x] && (g(x - 1, y) || g(x + 1, y) || g(x, y - 1) || g(x, y + 1))) out[y * W + x] = 1;
  const px = new Uint32Array(W * Hh);
  for (let i = 0; i < out.length; i++) { const c = out[i]; if (c) { const v = pal[c]; px[i] = (255 << 24) | ((v & 255) << 16) | (v & 0xff00) | ((v >> 16) & 255); } }
  return px; // ABGR little-endian for ImageData
}
function rotate(px, deg) {
  const out = new Uint32Array(px.length), a = deg * Math.PI / 180, cs = Math.cos(a), sn = Math.sin(a), cx = 15.5, cy = 26;
  for (let y = 0; y < Hh; y++) for (let x = 0; x < W; x++) {
    const dx = x - cx, dy = y - cy, sx = Math.round(cx + dx * cs + dy * sn), sy = Math.round(cy - dx * sn + dy * cs);
    if (sx >= 0 && sy >= 0 && sx < W && sy < Hh) out[y * W + x] = px[sy * W + sx];
  }
  return out;
}

// ---- fx sprites -------------------------------------------------------------------------------
function paint(w, h, fn) {
  const id = new ImageData(w, h), d = new Uint32Array(id.data.buffer);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) { const c = fn(x, y); if (c != null && c >= 0) d[y * w + x] = c >= 0x1000000 ? (c >>> 0) : ((255 << 24) | ((c & 255) << 16) | (c & 0xff00) | ((c >> 16) & 255)); }
  return id;
}
const argb = (c, a) => ((a << 24) | ((c & 255) << 16) | (c & 0xff00) | ((c >> 16) & 255)) >>> 0;
const B4 = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
function orb(w, h, cOut, c1, c2, c3) {
  return paint(w, h, (x, y) => { const dx = (x - (w - 1) / 2) / (w / 2), dy = (y - (h - 1) / 2) / (h / 2), d = dx * dx + dy * dy; if (d > 1) return -1; if (d > 0.7) return cOut; const t = d + (B4[(y & 3) * 4 + (x & 3)] / 16 - .5) * .3 + (dx + dy) * .15; return t > .5 ? c1 : t > .2 ? c2 : c3; });
}
function fxSprites() {
  const f = {};
  f.plasma = orb(12, 12, 0x1a1226, 0x6a3ad0, 0x5ce8ff, 0xffffff);
  f.egg = paint(12, 15, (x, y) => { const dx = (x - 5.5) / 6, dy = (y - 7.5) / 7.5 * (y < 7 ? 1 : 0.92), d = dx * dx + dy * dy; if (d > 1) return -1; if (d > .72) return 0x1a1226; const spot = ((x - 3) * (x - 3) + (y - 9) * (y - 9) < 5) || ((x - 8) * (x - 8) + (y - 5) * (y - 5) < 3); return spot ? 0x5fbf6a : ((x + y) & 1 && dx > 0.1 ? 0xd8d0b0 : 0xf4f0e0); });
  f.bolt = paint(12, 5, (x, y) => { if (y === 0 || y === 4) return x > 1 && x < 11 ? 0x1a1226 : -1; if (x === 11 && y === 2) return 0x1a1226; return x < 3 ? (y === 2 ? 0xff9ad0 : -1) : (y === 2 ? 0xffffff : 0xff6ab8); });
  f.block = paint(32, 32, (x, y) => { if (x === 0 || y === 0 || x === 31 || y === 31) return 0x1a1226; if (x === 1 || y === 1) return 0xa88868; if (x === 30 || y === 30) return 0x6a4a30; const n = ((x >> 2) * 7 + (y >> 2) * 13) % 5, b = (B4[(y & 3) * 4 + (x & 3)] > 8); return [0x8a6a48, 0x7a5a3c, 0x9a7a54, 0x826242, 0x6e5034][n] + (b ? 0x0a0a0a : 0); });
  f.tnt = paint(32, 32, (x, y) => { if (x === 0 || y === 0 || x === 31 || y === 31) return 0x1a1226; if (y >= 11 && y <= 20) return (x + y) % 8 < 4 ? 0xf4f0e8 : 0xdadada; return (x >> 1) % 2 ? 0xd8402e : 0xc03626; });
  f.cart = paint(40, 22, (x, y) => { const wheel = (y >= 16 && ((x >= 6 && x < 13) || (x >= 27 && x < 34))); if (wheel) return 0x1a1226; if (y < 3 || y > 15) return -1; if (y === 3 || y === 15 || x === 0 || x === 39) return 0x1a1226; return (y < 6) ? 0xb8bec8 : (x + y) & 1 ? 0x7a808c : 0x8a909c; });
  f.shield = paint(48, 48, (x, y) => { const dx = (x - 23.5) / 24, dy = (y - 23.5) / 24, d = dx * dx + dy * dy; if (d > 1) return -1; if (d > .82) return argb(0x9ad8ff, 200); return (B4[(y & 3) * 4 + (x & 3)] / 16 < 0.35 + d * 0.3) ? argb(0x5ce8ff, 70) : argb(0x5ce8ff, 30); });
  f.spark = paint(9, 9, (x, y) => (x === 4 || y === 4) ? 0xffffff : (Math.abs(x - 4) === Math.abs(y - 4) && Math.abs(x - 4) < 3 ? 0xffe888 : -1));
  f.pad = paint(44, 10, (x, y) => { const e = Math.abs(x - 21.5) / 22; if (y < 3) return e < .9 ? (y === 0 ? 0xc79cff : 0x8a5cd8) : -1; if (y < 7) return e < 1 - (y - 3) * .1 ? ((x & 1) ? 0x3a2866 : 0x2a1c4c) : -1; return e < .5 ? 0x5ce8ff : -1; });
  const OUTC = 0x1a1226;
  const pO = (w, h, fn) => { // paint with a 1px dark outline around opaque pixels
    const id = paint(w, h, fn), d = new Uint32Array(id.data.buffer), o = new Uint32Array(d.length);
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (!d[y * w + x]) { if ((x > 0 && d[y * w + x - 1]) || (x < w - 1 && d[y * w + x + 1]) || (y > 0 && d[(y - 1) * w + x]) || (y < h - 1 && d[(y + 1) * w + x])) o[y * w + x] = (255 << 24) | ((OUTC & 255) << 16) | (OUTC & 0xff00) | ((OUTC >> 16) & 255); }
    for (let i = 0; i < d.length; i++) if (!d[i] && o[i]) d[i] = o[i]; return id;
  };
  const ln = (x, y, x0, y0, x1, y1, t) => { const dx = x1 - x0, dy = y1 - y0, L = dx * dx + dy * dy, u = Math.max(0, Math.min(1, ((x - x0) * dx + (y - y0) * dy) / L)), px = x0 + dx * u - x, py = y0 + dy * u - y; return px * px + py * py <= t * t; };
  f.it_bomb = pO(10, 10, (x, y) => { const dx = x - 4.5, dy = y - 5.5, d = dx * dx + dy * dy; if (x === 7 && y <= 1) return 0xffd040; if (d > 12) return -1; return (x < 4 && y < 5) ? 0x6a5a88 : 0x2a2038; });
  f.rang = pO(12, 12, (x, y) => { if (ln(x, y, 1.5, 9, 1.5, 3, 1.4) || ln(x, y, 1.5, 3, 9, 3, 1.4)) return (x + y) & 1 ? 0xf0e8ff : 0xb8a8e0; return -1; });
  f.cloud = pO(40, 18, (x, y) => { const cs = [[10, 11, 7], [20, 8, 8], [30, 11, 7], [20, 12, 9]]; for (const c of cs) { const dx = x - c[0], dy = y - c[1]; if (dx * dx + dy * dy <= c[2] * c[2]) return y < 8 ? 0x7a78a8 : 0x3a3858; } return -1; });
  f.limb_claw = paint(128, 64, (x, y) => {
    for (let k = 0; k < 3; k++) { const yc = 6 + k * 14 + 36 * Math.pow(x / 128, 1.3), th = 7 * Math.pow(Math.sin(Math.PI * x / 127), 0.8), d = Math.abs(y - yc); if (d < th / 2 + 1) { if (d > th / 2) return OUTC; if (d < th * 0.2) return 0xf4eaff; return d < th * 0.38 ? 0x9a5ae8 : 0x4a2a90; } } return -1;
  });
  f.limb_slab = paint(56, 96, (x, y) => {
    const top = Math.abs(x - 27.5) * 0.5; if (y < top || x < 3 || x > 52 || y > 94) return -1;
    if (y === Math.ceil(top) || x === 3 || x === 52 || y === 94) return OUTC;
    const rune = (y % 14 === 6 && x > 12 && x < 43 && (x * 3 + y) % 7 < 5) || (x === 27 && y > 20 && y < 80 && y % 14 > 2);
    if (rune) return 0x5ce8ff; return x < 12 ? 0x3d2866 : x > 44 ? 0x1f1236 : ((x * 5 + y * 3) % 11 === 0 ? 0x2a1c4c : 0x32215a);
  });
  const jaw = (up) => (x, y) => { const yy = up ? y : 47 - y, hw = 46 - yy * 0.25; if (Math.abs(x - 47.5) > hw || yy < 0) return -1; if (yy < 24) return (x > 4 && x < 92 && yy === 0) ? OUTC : (yy < 22 ? ((x + yy) % 9 === 0 ? 0x6a1f58 : 0x4a1840) : 0xe8486a); const tooth = ((x + 3) % 12) < 7, tl = 47 - 24, tw = (yy - 24); if (tooth && ((x + 3) % 12) >= 3 - 0 && tw < 22 - Math.abs(((x + 3) % 12) - 3.5) * 5) return tw > 15 ? 0xd8d0e0 : 0xf4f0ff; return -1; };
  f.limb_mawt = paint(96, 48, jaw(true)); f.limb_mawb = paint(96, 48, jaw(false));
  f.star = paint(5, 5, (x, y) => (x === 2 || y === 2) ? 0xffffff : -1);
  f.dot = paint(2, 2, () => 0xffffff);
  return f;
}

// ---- atlas ------------------------------------------------------------------------------------
export function buildAtlas(ids) {
  const per = POSE_ORDER.reduce((a, k) => a + POSES[k], 0);
  const rows = Math.ceil(per * ids.length / COLS);
  const AW = COLS * W, fxY = rows * Hh, AH = fxY + 360;
  const canvas = document.createElement('canvas'); canvas.width = AW; canvas.height = AH;
  const ctx = canvas.getContext('2d'), index = {}, fx = {};
  let n = 0;
  for (const id of ids) {
    const pal = PAL[id], cache = {};
    for (const pn of POSE_ORDER) for (let i = 0; i < POSES[pn]; i++) {
      const P = pose(pn, i);
      let px;
      if (pn === 'tumble') px = rotate(cache.hit0, P.rot);
      else if (pn === 'roll') px = rotate(toRGBA(drawFighter(id, P), pal), i * 90);
      else px = toRGBA(drawFighter(id, P), pal);
      if (pn === 'hit' && i === 0) cache.hit0 = px;
      const cell = n++, x = (cell % COLS) * W, y = ((cell / COLS) | 0) * Hh;
      const img = new ImageData(new Uint8ClampedArray(px.buffer), W, Hh); ctx.putImageData(img, x, y);
      index[id + ':' + pn + ':' + i] = [x, y];
    }
  }
  const sp = fxSprites(); let cx = 0, cy = fxY, rowH = 0;
  for (const k of Object.keys(sp)) {
    const im = sp[k]; if (cx + im.width > AW) { cx = 0; cy += rowH + 1; rowH = 0; }
    ctx.putImageData(im, cx, cy); fx[k] = { x: cx, y: cy, w: im.width, h: im.height }; cx += im.width + 1; rowH = Math.max(rowH, im.height);
  }
  return {
    canvas, fx, W, H: Hh,
    frame(id, pn, i) { return index[id + ':' + pn + ':' + (i % POSES[pn])] || index[id + ':idle:0']; },
    count: n,
  };
}
export { PAL };
