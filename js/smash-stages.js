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
};

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

export function createStageVisual(def) {
  if (typeof document === 'undefined') return null;
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
