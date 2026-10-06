// GOR BRAWL stages: geometry (sim, no DOM) + procedural layered pixel scenes (render, DOM canvas at load).
// PLATEAU: ringed planet in the sky, slow moon crossing, rock platforms. All art is original and generated.

export const STAGES = {
  plateau: {
    id: 'plateau', name: 'PLATEAU',
    solids: [{ x0: -180, x1: 180, y0: 0, y1: 70 }],        // main platform, top at y=0
    thin: [{ x0: -132, x1: -68, y: -56 }, { x0: 68, x1: 132, y: -56 }], // pass-through rock platforms
    blast: { l: -330, r: 330, t: -270, b: 170 },
    spawns: [{ x: -110, y: -110 }, { x: -40, y: -110 }, { x: 40, y: -110 }, { x: 110, y: -110 }],
    start: [{ x: -90, y: 0 }, { x: 90, y: 0 }, { x: -30, y: 0 }, { x: 30, y: 0 }],
  },
  hangar: {
    id: 'hangar', name: 'HANGAR',
    solids: [{ x0: -200, x1: 200, y0: 0, y1: 70 }],
    thin: [{ x0: -150, x1: -86, y: -52 }, { x0: 86, x1: 150, y: -52 }, { x0: -32, x1: 32, y: -104 }],
    pads: [{ i: 0, base: -52, amp: 24, per: 480, ph: 0 }, { i: 1, base: -52, amp: 24, per: 480, ph: 180 }],
    blast: { l: -350, r: 350, t: -280, b: 170 },
    spawns: [{ x: -120, y: -120 }, { x: -40, y: -120 }, { x: 40, y: -120 }, { x: 120, y: -120 }],
    start: [{ x: -100, y: 0 }, { x: 100, y: 0 }, { x: -40, y: 0 }, { x: 40, y: 0 }],
  },
  roof: {
    id: 'roof', name: 'ROOF',
    solids: [{ x0: -150, x1: 150, y0: 0, y1: 60 }],
    thin: [{ x0: -124, x1: -84, y: -36 }, { x0: 84, x1: 124, y: -36 }, { x0: -34, x1: 34, y: -88 }],
    blast: { l: -310, r: 310, t: -270, b: 160 },
    spawns: [{ x: -90, y: -110 }, { x: -30, y: -110 }, { x: 30, y: -110 }, { x: 90, y: -110 }],
    start: [{ x: -80, y: 0 }, { x: 80, y: 0 }, { x: -30, y: 0 }, { x: 30, y: 0 }],
  },
};
export const STAGE_IDS = ['plateau', 'hangar', 'roof'];

// Stateless hazard schedule (function of the sim frame only: host, guest and replays agree).
// hangar: docking beam every 20 s on alternating sides (60 f telegraph, 24 f active). roof: sign flicker, then a zap every 15 s.
export function hazardState(stage, frame) {
  if (!stage) return null;
  if (stage.id === 'hangar') {
    const per = 1200, k = (frame / per) | 0, ph = frame % per, side = (n) => (n & 1 ? 1 : -1) * 150;
    const act = k >= 1 && ph < 24, tele = ph >= 1140 ? (ph - 1140) / 60 : 0;
    return { kind: 'beam', act, tele, x: act ? side(k) : side(k + 1), w: 44, y0: -300, y1: 130, ys: -120, dmg: 4, bkb: 22, gr: 0, start: act && ph === 0 };
  }
  if (stage.id === 'roof') {
    const per = 900, k = (frame / per) | 0, ph = frame % per;
    const act = k >= 1 && ph < 22, tele = ph >= 780 ? (ph - 780) / 120 : 0;
    return { kind: 'zap', act, tele, x: 0, w: 100, y0: -150, y1: 12, ys: -70, dmg: 6, bkb: 30, gr: 40, start: act && ph === 0 };
  }
  return null;
}

function rng(a) { return function () { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function mk(w, h) { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; }
function hex(c) { return '#' + c.toString(16).padStart(6, '0'); }

function pixelFill(ctx, w, h, fn) { // per-pixel painter, fn(x,y)->0xRRGGBB or -1
  const id = ctx.createImageData(w, h), d = id.data;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const c = fn(x, y); if (c < 0) continue; const i = (y * w + x) * 4;
    d[i] = (c >> 16) & 255; d[i + 1] = (c >> 8) & 255; d[i + 2] = c & 255; d[i + 3] = 255;
  }
  ctx.putImageData(id, 0, 0);
}
const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];

function skyCanvas() {
  const c = mk(480, 270), x = c.getContext('2d');
  const stops = [[0x07030f, 0], [0x120826, .35], [0x2a1050, .62], [0x5a2a8a, .85], [0x8a4aa0, 1]];
  pixelFill(x, 480, 270, (px, py) => {
    const t = py / 269 + (BAYER[(py & 3) * 4 + (px & 3)] / 16 - 0.5) * 0.06;
    let i = 0; while (i < stops.length - 2 && t > stops[i + 1][1]) i++;
    return t < stops[i + 1][1] - (stops[i + 1][1] - stops[i][1]) * 0.5 ? stops[i][0] : stops[i + 1][0];
  });
  return c;
}

function planetCanvas() { // stars + huge ringed planet
  const W = 1100, H = 300, c = mk(W, H), x = c.getContext('2d'), R = rng(777);
  for (let i = 0; i < 260; i++) { const a = R(), px = (R() * W) | 0, py = (R() * H * 0.9) | 0; x.fillStyle = a > .9 ? '#fff6d8' : a > .5 ? '#b8a8ff' : '#6a5aa8'; x.fillRect(px, py, a > .96 ? 2 : 1, 1); }
  const cx = 640, cy = 130, Rr = 64, tilt = -0.28, ct = Math.cos(tilt), st = Math.sin(tilt);
  const bands = [0xd8a0e8, 0xc080d8, 0xa862c8, 0xe0b0f0, 0x9050b8, 0xb878d0];
  const rx = 150, ry = 30, inner = 0.55;
  const ring = (px, py) => { const dx = px - cx, dy = py - cy, u = dx * ct + dy * st, v = -dx * st + dy * ct; const e = (u / rx) ** 2 + (v / ry) ** 2; return e < 1 && e > inner * inner ? { e, v } : null; };
  pixelFill(x, W, H, (px, py) => {
    const dx = px - cx, dy = py - cy, d2 = dx * dx + dy * dy, rg = ring(px, py);
    if (d2 <= Rr * Rr) {
      if (rg && rg.v > 0) return ((px + py) & 1) ? 0xf0d8a0 : 0xd8b878;
      const sh = (dx + dy * 0.4) / Rr; // shade from top-left
      let b = bands[((py - cy + 200) / 7 | 0) % bands.length];
      const th = sh + (BAYER[(py & 3) * 4 + (px & 3)] / 16 - 0.5) * 0.35;
      if (th > 0.35) b = ((b >> 1) & 0x7f7f7f); if (th > 0.7) b = ((b >> 1) & 0x7f7f7f);
      return b;
    }
    if (rg && rg.v <= 0 && d2 > Rr * Rr) return ((px + py) & 1) ? 0xb89ae0 : 0x8a70b8;
    if (rg && rg.v > 0) return ((px + py) & 1) ? 0xf0d8a0 : 0xd8b878;
    return -1;
  });
  // pixelFill overwrote stars: repaint stars on empty pixels
  const R2 = rng(777); const id = x.getImageData(0, 0, W, H);
  void id; // (stars were painted before pixelFill; redo after)
  for (let i = 0; i < 260; i++) { const a = R2(), px = (R2() * W) | 0, py = (R2() * H * 0.9) | 0; const p = x.getImageData(px, py, 1, 1).data; if (p[3] === 0) { x.fillStyle = a > .9 ? '#fff6d8' : a > .5 ? '#b8a8ff' : '#6a5aa8'; x.fillRect(px, py, a > .96 ? 2 : 1, 1); } }
  return c;
}

function ridgeCanvas(W, H, seed, base, amp, col, lit, towers) {
  const c = mk(W, H), x = c.getContext('2d'), R = rng(seed), ph = [R() * 6, R() * 6, R() * 6];
  const top = (px) => base + Math.sin(px * 0.011 + ph[0]) * amp + Math.sin(px * 0.027 + ph[1]) * amp * 0.5 + Math.sin(px * 0.061 + ph[2]) * amp * 0.2;
  pixelFill(x, W, H, (px, py) => {
    const t = top(px); if (py < t) return -1;
    if (py < t + 2 && ((px + py) & 1)) return lit; return py < t + 1 ? lit : col;
  });
  if (towers) for (let i = 0; i < towers; i++) {
    const tx = (R() * (W - 40) + 10) | 0, th = 14 + (R() * 26 | 0), tw = 5 + (R() * 6 | 0), ty = (top(tx) | 0) - th + 2;
    x.fillStyle = hex(col); x.fillRect(tx, ty, tw, th + 4);
    x.fillStyle = hex(lit); x.fillRect(tx, ty, tw, 1);
    for (let k = 0; k < 4; k++) if (R() > .4) { x.fillStyle = R() > .5 ? '#ffd870' : '#8af0ff'; x.fillRect(tx + 1 + (R() * (tw - 2) | 0), ty + 3 + k * 4, 1, 1); }
    if (i % 4 === 0) { x.fillStyle = '#ff4a6a'; x.fillRect(tx + (tw >> 1), ty - 3, 1, 3); }
  }
  if (towers) { // lit sign block (store-sign glow)
    const sx = (W * 0.62) | 0, sy = (top(sx) | 0) - 18; x.fillStyle = '#3a1f60'; x.fillRect(sx - 1, sy - 1, 26, 12);
    x.fillStyle = '#ff7a2a'; x.fillRect(sx, sy, 24, 3); x.fillStyle = '#18c070'; x.fillRect(sx, sy + 3, 24, 3); x.fillStyle = '#e03a4a'; x.fillRect(sx, sy + 6, 24, 3);
    x.fillStyle = '#ffffff22'; x.fillRect(sx - 3, sy - 3, 30, 16);
  }
  return c;
}

function rockCanvas(w, h, taper, seed) { // solid rock slab; top surface at row 0
  const c = mk(w, h), x = c.getContext('2d'), R = rng(seed);
  pixelFill(x, w, h, (px, py) => {
    const half = w / 2 - py * taper, dx = Math.abs(px + 0.5 - w / 2);
    if (dx > half + (R() * 1.2)) return -1;
    if (py < 2) return 0xc79cff; // lit top edge
    if (py < 5) return ((px + py) & 1) ? 0x7d4fd0 : 0x6a3fb8; // violet moss
    const sh = (dx / half) * 0.5 + py / h * 0.5 + (BAYER[(py & 3) * 4 + (px & 3)] / 16 - 0.5) * 0.3;
    return sh > .75 ? 0x1f1236 : sh > .45 ? 0x2d1c4c : 0x3d2866;
  });
  // outline pass
  const id = x.getImageData(0, 0, w, h), d = id.data, o = new Uint8Array(w * h);
  for (let py = 0; py < h; py++) for (let px = 0; px < w; px++) {
    if (d[(py * w + px) * 4 + 3]) continue;
    for (const [ax, ay] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const nx = px + ax, ny = py + ay; if (nx >= 0 && ny >= 0 && nx < w && ny < h && d[(ny * w + nx) * 4 + 3]) { o[py * w + px] = 1; break; } }
  }
  for (let i = 0; i < w * h; i++) if (o[i]) { d[i * 4] = 0x12; d[i * 4 + 1] = 0x08; d[i * 4 + 2] = 0x20; d[i * 4 + 3] = 255; }
  x.putImageData(id, 0, 0);
  return c;
}

function moonCanvas() {
  const c = mk(22, 22), x = c.getContext('2d');
  pixelFill(x, 22, 22, (px, py) => { const dx = px - 10.5, dy = py - 10.5, d = Math.sqrt(dx * dx + dy * dy); if (d > 10.5) return -1; const sh = (dx + dy) / 15 + (BAYER[(py & 3) * 4 + (px & 3)] / 16 - .5) * .3; return sh > .45 ? 0x8a80a8 : 0xd8d0ee; });
  x.fillStyle = '#9890b8'; x.fillRect(6, 7, 3, 2); x.fillRect(13, 13, 3, 3); x.fillRect(11, 5, 2, 1);
  return c;
}

function fgCanvas() {
  const W = 1600, H = 120, c = mk(W, H), x = c.getContext('2d'), R = rng(4242);
  for (let i = 0; i < 26; i++) { // crystal clumps + grass spikes silhouette
    const bx = (R() * W) | 0, h = 18 + (R() * 70 | 0), w = 3 + (R() * 5 | 0);
    x.fillStyle = '#0c0618'; for (let k = 0; k < h; k++) { const ww = Math.max(1, w * (1 - k / h)); x.fillRect(bx - ww / 2 | 0, H - k, Math.ceil(ww), 1); }
    if (R() > .6) { x.fillStyle = '#5a3ab0'; x.fillRect(bx - 1, H - h, 1, 3); }
  }
  return c;
}

function hexc(c) { return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255]; }

// HANGAR: station deck, rail lights, window onto a ringed planet, two moving pads, docking beam hazard.
function hangarVisual(def) {
  const R0 = rng(91), stars = []; for (let i = 0; i < 90; i++) stars.push({ x: R0() * 300, y: R0() * 110, a: R0() });
  const sky = [0x0a0620, 0x0f0a30, 0x15103e, 0x1c1650, 0x241a5c];
  return {
    drawBack(r, cam, t, S) {
      const z = cam.zoom; r.setTint(1, 1, 1, 1);
      for (let k = 0; k < 14; k++) { const c = hexc(sky[Math.min(4, (k / 14 * 5) | 0)]); r.rect(0, k * 20, 480, 20, c[0], c[1], c[2], 1); }
      // window onto space (parallax .08)
      const wx = 120 - cam.x * 0.08 * z, wy = 24 - cam.y * 0.05 * z, ww = 240 * z, wh = 110 * z;
      r.rect(wx - 3, wy - 3, ww + 6, wh + 6, 0.12, 0.1, 0.22, 1); r.rect(wx, wy, ww, wh, 0.02, 0.01, 0.07, 1);
      for (const s of stars) r.rect(wx + (s.x % 240) * z, wy + s.y * z, s.a > 0.9 ? 2 : 1, 1, 0.8, 0.8, 1, 0.4 + 0.5 * s.a);
      circleS(r, wx + 170 * z, wy + 52 * z, 30 * z, 0.62, 0.35, 0.82, 1); circleS(r, wx + 162 * z, wy + 46 * z, 22 * z, 0.78, 0.5, 0.92, 0.7); r.rect(wx + 120 * z, wy + 50 * z, 100 * z, 3 * z, 0.9, 0.8, 0.6, 0.8);
      for (let k = 1; k < 4; k++) r.rect(wx + k * 60 * z, wy, 3 * z, wh, 0.14, 0.12, 0.26, 1);
      // girders (parallax .3) + rail lights
      for (let k = -8; k < 12; k++) { const gx = ((k * 90 - cam.x * 0.3) * z) + 240; if (gx < -20 || gx > 500) continue; r.rect(gx, 0, 9 * z, 270, 0.1, 0.09, 0.22, 1); r.rect(gx, 0, 2 * z, 270, 0.2, 0.18, 0.4, 1); for (let y = 30; y < 250; y += 38) r.rect(gx + 3 * z, y - cam.y * 0.3 * z * 0.5, 3 * z, 3 * z, ((t >> 4) + k + (y / 38 | 0)) & 3 ? 0.3 : 1, 0.9, 0.5, 1); }
      r.rect(0, 0, 480, 10 * z, 0.08, 0.07, 0.18, 1);
    },
    drawStage(r, cam, S) {
      const z = cam.zoom, X = (wx) => Math.round((wx - cam.x) * z + 240), Y = (wy) => Math.round((wy - cam.y) * z + 135), at = r.atlas;
      const m = def.solids[0], x = X(m.x0), w = Math.round((m.x1 - m.x0) * z), y = Y(0), h = Math.round(70 * z);
      r.rect(x, y, w, h, 0.17, 0.18, 0.3, 1); r.rect(x, y + h * 0.4, w, h * 0.6, 0.1, 0.1, 0.2, 1); r.rect(x, y, w, Math.max(2, 3 * z), 0.62, 0.66, 0.85, 1); r.rect(x, y + 3 * z, w, 2 * z, 0.35, 0.4, 0.62, 1);
      r.rect(x - 1, y, 1, h, 0.05, 0.03, 0.1, 1); r.rect(x + w, y, 1, h, 0.05, 0.03, 0.1, 1); r.rect(x, y + h, w, 1, 0.05, 0.03, 0.1, 1);
      for (let gx = m.x0 + 8; gx < m.x1 - 8; gx += 20) { const on = ((((S ? S.frame : 0) >> 4) + (gx / 20 | 0)) & 3) !== 0; r.rect(X(gx), y + 6 * z, 4 * z, 2 * z, on ? 0.4 : 0.9, on ? 0.95 : 0.5, on ? 1 : 0.3, 1); }
      for (let gx = m.x0; gx < m.x1; gx += 16) { r.rect(X(gx), y + 14 * z, 8 * z, 4 * z, 0.95, 0.8, 0.2, 1); r.rect(X(gx + 8), y + 14 * z, 8 * z, 4 * z, 0.1, 0.08, 0.14, 1); }
      for (let gx = m.x0 + 30; gx < m.x1; gx += 80) r.rect(X(gx), y + 22 * z, 6 * z, 44 * z, 0.12, 0.12, 0.25, 1);
      const th = S ? S.thin : def.thin;
      th.forEach((t, i) => { const px = X(t.x0), pw = Math.round((t.x1 - t.x0) * z), py = Y(t.y), moving = i < 2;
        r.img(at.canvas, at.fx.pad.x, at.fx.pad.y, 44, 10, px, py, pw, Math.round(10 * z));
        if (moving) { r.rect(px + pw / 2 - 1, py + 10 * z, 2 * z, 150 * z, 0.2, 0.22, 0.4, 0.5); r.rect(px + 4 * z, py + 8 * z, pw - 8 * z, 2 * z, 0.4, 0.95, 1, 0.9); } });
      const hz = hazardState(def, S ? S.frame : 0);
      if (hz) {
        const bx = X(hz.x - hz.w / 2), bw = Math.round(hz.w * z), f = S ? S.frame : 0;
        r.rect(bx, Y(-280), bw, 14 * z, 0.3, 0.3, 0.5, 1); r.rect(bx + 4 * z, Y(-266), bw - 8 * z, 4 * z, hz.tele > 0 || hz.act ? 1 : 0.3, 0.3, 0.3, 1);
        if (hz.tele > 0 && !hz.act) for (let yy = -266; yy < 40; yy += 10) if (((f >> 2) + (yy / 10 | 0)) & 1) r.rect(bx + 2 * z, Y(yy), bw - 4 * z, 5 * z, 1, 0.25, 0.25, 0.25 + 0.4 * hz.tele);
        if (hz.act) { r.rect(bx - 4 * z, Y(-266), bw + 8 * z, 300 * z, 0.4, 0.8, 1, 0.3); r.rect(bx, Y(-266), bw, 300 * z, 0.7, 0.95, 1, 0.85); r.rect(bx + bw * 0.3, Y(-266), bw * 0.4, 300 * z, 1, 1, 1, 1); }
      }
    },
    drawFront() {},
  };
}
function circleS(r, cx, cy, rad, R, G, Bc, a) { for (let y = -rad; y <= rad; y += 2) { const w = Math.sqrt(Math.max(0, rad * rad - y * y)); r.rect(cx - w, cy + y, w * 2, 2, R, G, Bc, a); } }

// ROOF: night, 7/11 rooftop, sign tower mid platform that flickers then zaps, AC-unit side platforms.
function roofVisual(def) {
  const R0 = rng(5150), stars = [], blds = [];
  for (let i = 0; i < 110; i++) stars.push({ x: R0() * 900, y: R0() * 150, a: R0() });
  for (let i = 0; i < 26; i++) blds.push({ x: i * 56 + R0() * 20, w: 30 + R0() * 36, h: 40 + R0() * 90, lit: R0() });
  return {
    drawBack(r, cam, t, S) {
      const z = cam.zoom; r.setTint(1, 1, 1, 1);
      for (let k = 0; k < 14; k++) { const u = k / 13, c = [0.03 + 0.16 * u * u, 0.02 + 0.07 * u, 0.1 + 0.2 * u]; r.rect(0, k * 20, 480, 20, c[0], c[1], c[2], 1); }
      for (const s of stars) r.rect(((s.x - cam.x * 0.04) * z % 480 + 480) % 480, s.y * 0.9, s.a > 0.93 ? 2 : 1, 1, 0.85, 0.85, 1, 0.3 + 0.6 * ((s.a * 7 + t * 0.02) % 1));
      circleS(r, 380 - cam.x * 0.06 * z, 54 - cam.y * 0.03 * z, 16 * z, 0.92, 0.9, 0.98, 1); circleS(r, 385 - cam.x * 0.06 * z, 50 - cam.y * 0.03 * z, 13 * z, 0.78, 0.74, 0.9, 0.6);
      for (const layer of [0.25, 0.5]) for (const b of blds) {
        const bx = ((b.x - cam.x * layer) * z) % (26 * 56 * z) , x0 = bx < -80 ? bx + 26 * 56 * z : bx; const hh = b.h * z * (layer > 0.3 ? 1 : 0.7), by = 135 + (70 - cam.y * layer * 0.4) * z - hh;
        const c = layer > 0.3 ? [0.06, 0.04, 0.14] : [0.1, 0.07, 0.2]; r.rect(x0, by, b.w * z, hh + 80, c[0], c[1], c[2], 1);
        for (let wy = 5; wy < b.h - 8; wy += 9) for (let wx = 4; wx < b.w - 4; wx += 8) if (((wx * 7 + wy * 3 + (b.x | 0)) % 5) < 2 + (b.lit * 3 | 0)) r.rect(x0 + wx * z, by + wy * z * (layer > 0.3 ? 1 : 0.7), 3 * z, 3 * z, 1, 0.82, 0.4, 0.8);
        if (b.lit > 0.7) r.rect(x0 + b.w * z / 2, by - 4 * z, 2 * z, 4 * z, ((t >> 5) & 1) ? 1 : 0.3, 0.2, 0.25, 1);
      }
    },
    drawStage(r, cam, S) {
      const z = cam.zoom, X = (wx) => Math.round((wx - cam.x) * z + 240), Y = (wy) => Math.round((wy - cam.y) * z + 135), f = S ? S.frame : 0;
      const m = def.solids[0], x = X(m.x0), w = Math.round((m.x1 - m.x0) * z), y = Y(0), h = Math.round(60 * z);
      r.rect(x, y, w, h, 0.2, 0.18, 0.3, 1); r.rect(x, y + 8 * z, w, h - 8 * z, 0.13, 0.11, 0.22, 1); r.rect(x - 2 * z, y - 3 * z, w + 4 * z, 4 * z, 0.5, 0.46, 0.66, 1); r.rect(x, y, w, 2 * z, 0.72, 0.68, 0.9, 1);
      r.rect(x - 1, y, 1, h, 0.05, 0.03, 0.1, 1); r.rect(x + w, y, 1, h, 0.05, 0.03, 0.1, 1); r.rect(x, y + h, w, 1, 0.05, 0.03, 0.1, 1);
      for (let gx = m.x0 + 10; gx < m.x1 - 6; gx += 24) r.rect(X(gx), y + 16 * z, 14 * z, 2 * z, 0.3, 0.27, 0.45, 1);
      for (const sd of [-1, 1]) { const ax = sd < 0 ? def.thin[0] : def.thin[1], bx = X(ax.x0), bw = Math.round((ax.x1 - ax.x0) * z), by = Y(ax.y);
        r.rect(bx, by, bw, 24 * z, 0.5, 0.52, 0.6, 1); r.rect(bx, by, bw, 2 * z, 0.85, 0.88, 0.95, 1); r.rect(bx, by + 22 * z, bw, 2 * z, 0.3, 0.32, 0.4, 1); circleS(r, bx + bw / 2, by + 11 * z, 8 * z, 0.3, 0.32, 0.4, 1); for (let k = -1; k <= 1; k++) r.rect(bx + bw / 2 - 7 * z, by + 11 * z + k * 3 * z, 14 * z, 1 * z, 0.14, 0.16, 0.22, 1); }
      const hz = hazardState(def, f), sg = def.thin[2], sx = X(sg.x0), sw = Math.round((sg.x1 - sg.x0) * z), sy = Y(sg.y);
      r.rect(X(-3), sy + 26 * z, 6 * z, (88 - 26) * z, 0.3, 0.28, 0.42, 1); r.rect(X(-3), sy + 26 * z, 2 * z, (88 - 26) * z, 0.55, 0.52, 0.72, 1);
      const flick = hz && hz.tele > 0 && !hz.act && (((f >> 1) + (f >> 3)) & 1), zap = hz && hz.act, dim = flick ? 0.25 : 1;
      r.rect(sx - 2 * z, sy - 1, sw + 4 * z, 30 * z, 0.08, 0.05, 0.16, 1);
      r.rect(sx, sy + 1 * z, sw, 8 * z, 1 * dim, 0.48 * dim, 0.16 * dim, 1); r.rect(sx, sy + 9 * z, sw, 8 * z, 0.09 * dim, 0.75 * dim, 0.44 * dim, 1); r.rect(sx, sy + 17 * z, sw, 8 * z, 0.9 * dim, 0.22 * dim, 0.28 * dim, 1);
      for (let k = 0; k < 7; k++) r.rect(sx + 5 * z + k * 9 * z, sy + 10 * z, 5 * z, 5 * z, 1, 1, 1, 0.7 * dim);
      r.rect(sx, sy, sw, 2 * z, 0.8, 0.7, 1, 1);
      if (!flick) { r.rect(sx - 6 * z, sy - 4 * z, sw + 12 * z, 38 * z, 1, 0.6, 0.3, zap ? 0.4 : 0.12); }
      if (zap) { r.rect(X(-50), Y(-150), 100 * z, 160 * z, 0.7, 0.8, 1, 0.25); for (let k = -2; k <= 2; k++) { let px = k * 22; for (let yy = -82; yy < 8; yy += 10) { const nx = k * 22 + (((yy * 11 + f * 19 + k * 7) % 11) - 5); r.rect(X(Math.min(px, nx)), Y(yy), (Math.abs(nx - px) + 3) * z, 10 * z, 0.8, 0.9, 1, 1); px = nx; } } }
      else if (hz && hz.tele > 0) { r.rect(X(-50), Y(-6), 100 * z, 3 * z, 1, 0.8, 0.3, 0.3 + 0.5 * hz.tele * (((f >> 2) & 1) ? 1 : 0.5)); }
    },
    drawFront() {},
  };
}

export function createStageVisual(def) {
  if (typeof document === 'undefined') return null;
  if (def.id === 'hangar') return hangarVisual(def);
  if (def.id === 'roof') return roofVisual(def);
  const sky = skyCanvas(), planet = planetCanvas(), moon = moonCanvas();
  const far = ridgeCanvas(1400, 200, 11, 120, 26, 0x2a1850, 0x5a3a90, 0);
  const mid = ridgeCanvas(1400, 200, 29, 130, 18, 0x170a2e, 0x3a2268, 14);
  const fg = fgCanvas();
  const main = rockCanvas(360, 74, 1.15, 3), plats = [rockCanvas(64, 16, 1.5, 5), rockCanvas(64, 16, 1.5, 6)];
  function tile(r, img, W, H, factor, oy, cam, yf, alpha) { // repeat horizontally with parallax
    const z = cam.zoom, w = W * z, h = H * z;
    const sy = (oy - cam.y * factor * yf) * z + 135;
    let sx = ((-cam.x * factor - W / 2) * z + 240) % w; if (sx > 0) sx -= w;
    r.setTint(1, 1, 1, alpha == null ? 1 : alpha);
    for (let x0 = sx; x0 < 480; x0 += w) r.img(img, 0, 0, W, H, x0, sy, w, h);
    r.setTint(1, 1, 1, 1);
  }
  return {
    drawBack(r, cam, t) {
      r.img(sky, 0, 0, 480, 270, 0, 0, 480, 270);
      tile(r, planet, 1100, 300, 0.07, -240, cam, 0.5);
      // slow moon crossing (parallax 0.12)
      const mx = ((t * 0.04) % 1000) - 380, z = cam.zoom;
      r.img(moon, 0, 0, 22, 22, ((mx - cam.x * 0.12) * z) + 240, ((-150 - cam.y * 0.06 + Math.sin(mx * 0.004) * 12) * z) + 135, 22 * z, 22 * z);
      tile(r, far, 1400, 200, 0.25, -90, cam, 0.5);
      tile(r, mid, 1400, 200, 0.5, -105, cam, 0.5);
    },
    drawStage(r, cam) {
      const z = cam.zoom, X = (wx) => Math.round((wx - cam.x) * z + 240), Y = (wy) => Math.round((wy - cam.y) * z + 135);
      r.img(main, 0, 0, 360, 74, X(-180), Y(0), Math.round(360 * z), Math.round(74 * z));
      for (let i = 0; i < def.thin.length; i++) { const p = def.thin[i]; r.img(plats[i], 0, 0, 64, 16, X(p.x0), Y(p.y), Math.round(64 * z), Math.round(16 * z)); }
    },
    drawFront(r, cam) { tile(r, fg, 1600, 120, 1.25, 60, cam, 1.0, 0.95); },
  };
}

// ---- terrain stages (proximity fights): geometry from 128 terrain height samples ---------------
// samples = heights along a line in human heights (up = positive). Output has the same shape as STAGES.* and is
// handed to createMatch via cfg.stageDef. Sim-only part first, visual second.
const clampN = (v, a, b) => (v < a ? a : v > b ? b : v);
export function terrainStage(samples, id) {
  const n = samples.length | 0 || 2, SPAN = 420, dxp = SPAN / (n - 1), PXH = 18, Q = 8;
  const raw = []; for (let i = 0; i < n; i++) { const v = +samples[i]; raw.push(Number.isFinite(v) ? v : 0); }
  const sm = raw.map((v, i) => (raw[Math.max(0, i - 1)] + v + raw[Math.min(n - 1, i + 1)]) / 3);
  const q = sm.map((v) => Math.round(clampN(v, -12, 12) * PXH / Q) * Q); // quantised terrain height px (up +)
  let a0 = 0, b0 = 0;
  for (let a = 0; a < n; a++) { let b = a; while (b + 1 < n && Math.abs(q[b + 1] - q[a]) <= Q) b++; if (b - a > b0 - a0) { a0 = a; b0 = b; } }
  const run = q.slice(a0, b0 + 1).sort((x, y) => x - y), med = run[run.length >> 1];
  const cx = ((a0 + b0) / 2) * dxp - SPAN / 2, half = clampN(((b0 - a0) * dxp) / 2, 110, 180);
  const wx = (i) => i * dxp - SPAN / 2 - cx, wy = (i) => -(q[i] - med);
  const prof = []; for (let i = 0; i < n; i++) prof.push({ x: wx(i), y: wy(i) });
  // shelves = merged equal-height runs outside the main platform (pass-through, clamped to a playable band)
  const segs = []; let s0 = 0;
  for (let i = 1; i <= n; i++) if (i === n || q[i] !== q[s0]) { segs.push({ xa: wx(s0) - dxp / 2, xb: wx(i - 1) + dxp / 2, y: clampN(wy(s0), -70, 60) }); s0 = i; }
  let thin = [];
  for (const g of segs) {
    for (const [xa, xb] of [[g.xa, Math.min(g.xb, -half)], [Math.max(g.xa, half), g.xb]]) if (xb - xa >= 14) thin.push({ x0: Math.round(xa), x1: Math.round(xb), y: g.y });
  }
  thin.sort((p, r) => (r.x1 - r.x0) - (p.x1 - p.x0)); thin = thin.slice(0, 6);
  const fw = 64, fx = half * 0.55; // 2 floating platforms like PLATEAU
  const floats = [{ x0: -fx - fw / 2, x1: -fx + fw / 2, y: -56 }, { x0: fx - fw / 2, x1: fx + fw / 2, y: -56 }];
  const k = half / 180, low = thin.reduce((m, t) => Math.max(m, t.y), 0);
  return {
    id: id || 'terrain', name: 'TERRAIN', terrain: true, prof, half, span: SPAN,
    solids: [{ x0: -half, x1: half, y0: 0, y1: 70 }],
    thin: floats.concat(thin), floats: floats.length,
    blast: { l: -(half + 150), r: half + 150, t: -270, b: 170 + Math.max(0, low) },
    spawns: [-110, -40, 40, 110].map((x) => ({ x: x * k, y: -110 })),
    start: [-90, 90, -30, 30].map((x) => ({ x: x * k, y: 0 })),
  };
}

function hexRGB(c, d) {
  if (c == null) return d;
  if (typeof c === 'number') return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255];
  const m = /^#?([0-9a-f]{6})$/i.exec(String(c).trim()); if (!m) return d; const v = parseInt(m[1], 16); return [((v >> 16) & 255) / 255, ((v >> 8) & 255) / 255, (v & 255) / 255];
}
const mixC = (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];

// Visual: rect-only (no textures) so the sky can be translucent and the 3D scene shows through (skyAlpha).
export function createTerrainVisual(def, pal, seed, skyAlpha) {
  pal = pal || {}; const top = hexRGB(pal.top, [0.05, 0.02, 0.12]), hor = hexRGB(pal.horizon, [0.45, 0.2, 0.62]), ring = hexRGB(pal.ring, null);
  const sa = skyAlpha == null ? 0.5 : skyAlpha, R = rng((seed | 0) || 7);
  const stars = []; for (let i = 0; i < 70; i++) stars.push({ x: R() * 960, y: R() * 200, a: R() });
  const lit = mixC(hor, [1, 1, 1], 0.45), body = mixC(top, hor, 0.35), deep = mixC(top, [0, 0, 0], 0.4);
  const P = def.prof, X0 = P[0].x, X1 = P[P.length - 1].x;
  const profAt = (wx) => { const t = (wx - X0) / (X1 - X0), u = ((t % 2) + 2) % 2, v = u > 1 ? 2 - u : u, f = v * (P.length - 1), i = Math.min(P.length - 2, f | 0); return P[i].y + (P[i + 1].y - P[i].y) * (f - i); };
  return {
    drawBack(r, cam, t) {
      const z = cam.zoom; r.setTint(1, 1, 1, 1);
      for (let k = 0; k < 15; k++) { const c = mixC(top, hor, Math.pow(k / 14, 1.4)); r.rect(0, k * 18, 480, 18, c[0], c[1], c[2], sa); }
      for (const s of stars) { const sx = ((s.x - cam.x * 0.05 * z) % 480 + 480) % 480; r.rect(sx, s.y * 0.9, 1, 1, 0.85, 0.85, 1, 0.35 + 0.4 * (0.5 + 0.5 * Math.sin(t * 0.05 + s.a * 20))); }
      if (ring) { const px = 340 - cam.x * 0.07, py = 62 - cam.y * 0.04, rad = 26;
        for (let y = -rad; y <= rad; y += 2) { const w = Math.sqrt(Math.max(0, rad * rad - y * y)); const sh = 0.55 + 0.3 * (-y / rad); r.rect(px - w, py + y, w * 2, 2, ring[0] * sh, ring[1] * sh, ring[2] * sh, Math.min(1, sa + 0.4)); }
        for (let k = 0; k < 64; k++) { const an = k / 64 * 6.2832; r.rect(px + Math.cos(an) * 46, py + Math.sin(an) * 9 - 2, 2, 1, lit[0], lit[1], lit[2], 0.7); } }
      for (let sx = 0; sx < 480; sx += 6) { // far ridge = the sampled terrain profile, parallax 0.3
        const wx = (sx - 240) / z + cam.x * 0.3, ty = (70 - profAt(wx) * 0.8 - cam.y * 0.3) * z + 135;
        r.rect(sx, ty, 6, 270 - ty, deep[0], deep[1], deep[2], Math.min(1, sa + 0.15)); r.rect(sx, ty, 6, 1, body[0], body[1], body[2], 0.8);
      }
    },
    drawStage(r, cam) {
      const z = cam.zoom, X = (wx) => Math.round((wx - cam.x) * z + 240), Y = (wy) => Math.round((wy - cam.y) * z + 135);
      for (const s of def.solids) {
        const x = X(s.x0), w = Math.round((s.x1 - s.x0) * z), y = Y(s.y0), h = Math.round((s.y1 - s.y0) * z);
        r.rect(x, y, w, h, body[0], body[1], body[2], 1);
        r.rect(x, y + h * 0.35, w, h * 0.65, deep[0] * 1.6, deep[1] * 1.6, deep[2] * 1.6, 1);
        r.rect(x, y, w, Math.max(2, 3 * z), lit[0], lit[1], lit[2], 1); r.rect(x, y + 3 * z, w, 2 * z, hor[0], hor[1], hor[2], 1);
        r.rect(x - 1, y, 1, h, 0.07, 0.03, 0.12, 1); r.rect(x + w, y, 1, h, 0.07, 0.03, 0.12, 1); r.rect(x, y + h, w, 1, 0.07, 0.03, 0.12, 1);
        for (let gx = s.x0 + 6; gx < s.x1 - 4; gx += 14) r.rect(X(gx), y - 2 * z, Math.max(1, z), 2 * z, lit[0], lit[1], lit[2], 1);
      }
      def.thin.forEach((t, i) => {
        const x = X(t.x0), w = Math.round((t.x1 - t.x0) * z), y = Y(t.y), fl = i < def.floats;
        r.rect(x, y, w, Math.max(1, 2 * z), lit[0], lit[1], lit[2], 1);
        r.rect(x + 2 * z, y + 2 * z, w - 4 * z, (fl ? 8 : 5) * z, body[0], body[1], body[2], 1);
        r.rect(x + 4 * z, y + (fl ? 10 : 7) * z, Math.max(1, w - 8 * z), 2 * z, deep[0], deep[1], deep[2], 1);
      });
    },
    drawFront() {},
  };
}
