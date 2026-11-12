// The encoder, rendered.
//
// Everything on the field is drawn WHERE IT PHYSICALLY IS -- the 8x8 pad as an
// 8x8 field, the ribbons along the edges they run along, the lidar point in the
// volume above. Nothing is labelled, because the position is the label.
//
// Sigma is drawn as bloom radius. A confident reading is a sharp point; an
// uncertain one is a haze. That is the whole visual grammar: brightness is how
// much, sharpness is how sure.

const POLL_MS = 40;
// How fast the wake fades. With additive blending the steady state is
// roughly (added per frame) / TRAIL, so anything drawn EVERY frame saturates
// to white unless it is drawn normally instead of added. Only transient,
// input-driven light is additive; the resting field is not.
const TRAIL = 0.28;
const IDLE_DRIFT = 0.55;     // px of slow wander on resting pads

// Hue by what the channel MEANS, not by channel order.
const HUE = {
  position: [92, 225, 255],   // cyan
  force:    [255, 171, 61],   // amber, heat
  distance: [255, 61, 120],   // magenta, the IR the rangefinders actually emit
  ribbon:   [167, 123, 255],  // violet
};
const GRID = 8;

const cv = document.getElementById("field");
const ctx = cv.getContext("2d", { alpha: false });
const mon = document.getElementById("monitor");
const raws = document.getElementById("raws");

let W = 0, H = 0, DPR = 1;
let frame = null;
let lastState = {};
const held = new Map();       // pid -> { v, brightness } for smooth decay

function resize() {
  DPR = Math.min(window.devicePixelRatio || 1, 2);
  W = Math.max(1, cv.clientWidth);
  H = Math.max(1, cv.clientHeight);
  cv.width = Math.floor(W * DPR);
  cv.height = Math.floor(H * DPR);
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  ctx.fillStyle = "#06080c";
  ctx.fillRect(0, 0, W, H);
}
window.addEventListener("resize", resize);

// The raw strip is empty until the first poll lands, so the layout SHIFTS
// after the canvas has already been sized -- leaving the buffer wrong and
// everything drawing off-screen. Watch the element, not the window.
if (window.ResizeObserver) {
  new ResizeObserver(() => {
    if (Math.abs(cv.clientWidth - W) > 1 || Math.abs(cv.clientHeight - H) > 1) resize();
  }).observe(cv);
}

// Tap the field to turn the grid a quarter. Which way it is mounted has been
// wrong twice and it is one number -- better under a thumb than in a guess.


async function post(path, body) {
  try {
    await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body || {}),
    });
  } catch { /* the field keeps running */ }
}

document.getElementById("setup").addEventListener("click", (ev) => {
  const b = ev.target.closest("button");
  if (!b) return;
  if (b.dataset.ribbon) return post("/api/ribbon", { position: b.dataset.ribbon });
  if (b.dataset.act === "rotate") return post("/api/rotate");
  if (b.dataset.act === "flipx") return post("/api/flip", { axis: "x" });
  if (b.dataset.act === "flipy") return post("/api/flip", { axis: "y" });
});

// ── geometry ─────────────────────────────────────────────────────────
// TRUE isometric, and the layering is the point:
//
//   floor  = the 8x8 pad plane, lying flat
//   volume = the space above it, where the hand actually is
//   under  = force, shining up THROUGH the floor
//   edges  = the four ribbons, running around the floor's rim
//
// Height is drawn as height. An earlier version slid the hand along an oblique
// axis and leaned on brightness for depth, which is exactly what "I don't see
// it in 3d space" meant.
// One-point perspective rather than isometric, so the edges are edges. In a
// diamond, "north" is a CORNER and there is no upper face -- which made the
// labels unreadable against the actual instrument. Here the north edge is the
// back of the floor: highest on screen and narrower because it is further away.
const FLOOR_W = 0.92;   // width of the near (south) edge
const FLOOR_BACK = 0.5; // width of the far (north) edge -- the perspective
const FLOOR_D = 0.34;   // how far the floor recedes up the screen
const ISO_Z = 0.5;      // how tall the volume stands above the floor

// Fit from the ACTUAL extents rather than a guessed fraction. The ribbons sit
// just outside the floor, so u,v run to +/-0.075 past it; in isometric that
// spans 1.15*size across and 0.575*size down, and the volume adds ISO_Z on top.
const RIM = 0.08;
const SPAN_X = FLOOR_W * (1 + RIM * 2);          // widest, at the near edge
const SPAN_Y = FLOOR_D + ISO_Z + RIM * FLOOR_D;  // floor depth plus the volume
const MARGIN = 0.88;

function stage() {
  const size = Math.min(W / SPAN_X, H / SPAN_Y) * MARGIN;
  // content runs from the near edge up to the top of the volume
  const cy = H / 2 + ((FLOOR_D + ISO_Z) / 2) * size;
  return { cx: W / 2, cy, size };
}

// u = left..right, v = north(0, far) .. south(1, near), h = height above floor
function iso(u, v, h) {
  const { cx, cy, size } = stage();
  const depth = 1 - v;                       // 0 at the near edge, 1 at the far
  const width = FLOOR_W + (FLOOR_BACK - FLOOR_W) * depth;
  return [
    cx + (u - 0.5) * width * size,
    cy - depth * FLOOR_D * size - h * ISO_Z * size,
  ];
}

function glow(x, y, radius, rgb, alpha) {
  if (radius <= 0 || alpha <= 0) return;
  const [r, gg, b] = rgb;
  const g = ctx.createRadialGradient(x, y, 0, x, y, radius);
  g.addColorStop(0.00, `rgba(${r},${gg},${b},${alpha * 0.7})`);
  g.addColorStop(0.20, `rgba(${r},${gg},${b},${alpha * 0.46})`);
  g.addColorStop(0.45, `rgba(${r},${gg},${b},${alpha * 0.2})`);
  g.addColorStop(0.72, `rgba(${r},${gg},${b},${alpha * 0.06})`);
  g.addColorStop(1.00, `rgba(${r},${gg},${b},0)`);
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(x, y, radius, 0, Math.PI * 2);
  ctx.fill();
}

function poly(pts, stroke) {
  ctx.strokeStyle = stroke;
  ctx.lineWidth = 1;
  ctx.beginPath();
  pts.forEach(([px, py], i) => (i ? ctx.lineTo(px, py) : ctx.moveTo(px, py)));
  ctx.closePath();
  ctx.stroke();
}

// ── the box the hand moves in ────────────────────────────────────────
const CORNERS = [[0, 0], [1, 0], [1, 1], [0, 1]];

function drawBox() {
  poly(CORNERS.map(([u, v]) => iso(u, v, 0)), "rgba(92,225,255,0.14)");
  poly(CORNERS.map(([u, v]) => iso(u, v, 1)), "rgba(255,61,120,0.05)");
  ctx.strokeStyle = "rgba(255,61,120,0.045)";
  ctx.beginPath();
  for (const [u, v] of CORNERS) {
    const [ax, ay] = iso(u, v, 0);
    const [bx, by] = iso(u, v, 1);
    ctx.moveTo(ax, ay);
    ctx.lineTo(bx, by);
  }
  ctx.stroke();
}

// The rangefinders sit at the CENTRE of each edge, not at the corners. Drawing
// them where they actually are is the only way the labels mean anything
// against the instrument in front of you.
const LIDAR_AT = {
  top:    [0.5, -RIM],
  bottom: [0.5, 1 + RIM],
  left:   [-RIM, 0.5],
  right:  [1 + RIM, 0.5],
};

function drawLidarSensors(st) {
  const { size } = stage();
  const r = st.raw || {};
  const wall = new Set(r.lidar_background || []);
  const live = new Set(r.lidar_contributing || []);
  for (const [pos, [u, v]] of Object.entries(LIDAR_AT)) {
    const [px, py] = iso(u, v, 0.06);
    const on = live.has(pos);
    ctx.fillStyle = on
      ? "rgba(255,61,120,0.85)"
      : wall.has(pos) ? "rgba(255,61,120,0.16)" : "rgba(255,61,120,0.32)";
    ctx.beginPath();
    ctx.arc(px, py, Math.max(1.5, size * 0.007), 0, Math.PI * 2);
    ctx.fill();
    if (on) glow(px, py, size * 0.06, HUE.distance, 0.5);
  }
}

// ── force: light from beneath the floor ──────────────────────────────
// Aftertouch as a HALO behind the whole scene -- the background itself
// glowing around the instrument, not a lamp under the floor and not a glaze
// over the pads. Behind it, the pressure has the space to itself and the plane
// looks identical at every level, which is what it was asked to do.
function drawForce(force) {
  if (force <= 0.001) return;
  const { cx, cy, size } = stage();
  const [hx, hy] = iso(0.5, 0.5, 0.35);   // centred on the volume, not the floor

  // a wide atmospheric wash, then a tighter core, both well behind the plane
  glow(hx, hy, size * (1.15 + force * 0.5), HUE.force, 0.035 + 0.11 * force);
  glow(hx, hy, size * (0.62 + force * 0.3), HUE.force, 0.03 + 0.13 * force);
  glow(hx, hy, size * (0.3 + force * 0.16), HUE.force, 0.02 + 0.1 * force);
}

// ── the pad plane, lying flat ────────────────────────────────────────
// The resting plane. Drawn normally, never added -- see draw().
function drawIdleLattice(t) {
  const { size } = stage();
  const cell = size * FLOOR_W / GRID;
  for (let gy = 0; gy < GRID; gy++) {
    for (let gx = 0; gx < GRID; gx++) {
      const n = Math.sin(t * 0.0004 + gx * 0.9) * Math.cos(t * 0.0003 + gy * 1.1);
      const [px, py] = iso((gx + 0.5) / GRID, (gy + 0.5) / GRID, 0);
      glow(px, py + n * IDLE_DRIFT, cell * 1.1, HUE.position, 0.05 + 0.018 * n);
    }
  }
}

// The plane looks IDENTICAL at every velocity. Force is carried entirely by
// the light under the floor -- letting it also brighten the pads meant two
// things encoded one quantity, and the plane never held still.
function drawPadField(ch, t) {
  const { size } = stage();
  const cell = size * FLOOR_W / GRID;

  const live = new Set();
  if (ch) for (const p of ch.points) live.add(p.pid);

  if (ch) {
    for (const p of ch.points) {
      const sig = Math.max(p.s[0] || 0, p.s[1] || 0);
      const [px, py] = iso(p.v[0], p.v[1], 0);
      held.set(p.pid, { px, py, amp: 1 });
      glow(px, py, cell * (1.7 + sig * 9), HUE.position, 0.4);
      glow(px, py, cell * 0.8, HUE.position, 0.3);
    }
  }

  for (const [pid, h] of held) {
    if (live.has(pid)) continue;
    h.amp *= 0.9;
    if (h.amp < 0.02) { held.delete(pid); continue; }
    glow(h.px, h.py, cell * 1.5, HUE.position, 0.2 * h.amp);
  }
}

// ── ribbons: around the rim of the floor ─────────────────────────────
function drawRibbons(channels) {
  const { size } = stage();
  const O = RIM;
  const edges = {
    softpot_n: (u) => [[u, -O], [0, -O], [1, -O]],
    softpot_s: (u) => [[u, 1 + O], [0, 1 + O], [1, 1 + O]],
    softpot_w: (u) => [[-O, u], [-O, 0], [-O, 1]],
    softpot_e: (u) => [[1 + O, u], [1 + O, 0], [1 + O, 1]],
  };
  for (const ch of channels) {
    const place = edges[ch.id];
    if (!place) continue;
    const lit = ch.points.length > 0;
    const [at, a, b] = place(lit ? ch.points[0].v[0] : 0.5);
    const [ax, ay] = iso(a[0], a[1], 0);
    const [bx, by] = iso(b[0], b[1], 0);
    ctx.strokeStyle = `rgba(167,123,255,${lit ? 0.2 : 0.055})`;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(ax, ay);
    ctx.lineTo(bx, by);
    ctx.stroke();
    if (lit) {
      const [px, py] = iso(at[0], at[1], 0);
      glow(px, py, size * 0.075, HUE.ribbon, 0.75);
    }
  }
}

// ── the hand, actually above the floor ───────────────────────────────
function drawHand(ch) {
  if (!ch || !ch.points.length) return;
  const { size } = stage();
  const [u, v, h] = ch.points[0].v;
  const sg = ch.points[0].s;
  const sig = Math.max(sg[0] || 0, sg[1] || 0, sg[2] || 0);

  const [px, py] = iso(u, v, h);
  const [sx, sy] = iso(u, v, 0);

  // a shadow on the floor and a line up to the hand: without them, height and
  // brightness are indistinguishable
  glow(sx, sy, size * 0.045 * (1 + sig), [255, 61, 120], 0.1);
  const grad = ctx.createLinearGradient(sx, sy, px, py);
  grad.addColorStop(0, "rgba(255,61,120,0.02)");
  grad.addColorStop(1, "rgba(255,61,120,0.16)");
  ctx.strokeStyle = grad;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(sx, sy);
  ctx.lineTo(px, py);
  ctx.stroke();

  const near = 0.3 + h * 0.7;
  glow(px, py, size * (0.05 + near * 0.13) * (1 + sig * 1.5), HUE.distance,
       0.1 + 0.18 * near);
}

// ── loop ─────────────────────────────────────────────────────────────
function render(t) {
  try {
    draw(t);
  } catch (err) {
    // one bad frame must not stop the loop -- a frozen field looks identical
    // to a dead instrument, and that has cost real debugging time already
    if (!render.warned) { console.error("[viz]", err); render.warned = true; }
  }
  requestAnimationFrame(render);
}

function draw(t) {
  ctx.globalCompositeOperation = "source-over";
  ctx.fillStyle = `rgba(6,8,12,${TRAIL})`;
  ctx.fillRect(0, 0, W, H);

  const channels = frame ? frame.channels : [];
  const byId = Object.fromEntries(channels.map((c) => [c.id, c]));
  const fch = byId.force;
  const force = fch && fch.points.length ? fch.points[0].v[0] : 0;

  // Standing structure, drawn NORMALLY: the box and the resting lattice are
  // present every single frame, so adding them accumulates without bound and
  // the whole field bleaches to white.
  drawBox();
  drawIdleLattice(t);

  // Only what the player is doing is additive, and that decays with the wake.
  ctx.globalCompositeOperation = "lighter";
  drawForce(force);            // from under the floor
  drawPadField(byId.pad, t);
  drawRibbons(channels);
  drawLidarSensors(lastState); // at the edge centres, where they really are
  drawHand(byId.lidar);        // above it
  ctx.globalCompositeOperation = "source-over";
}

// ── raw sensors ──────────────────────────────────────────────────────
// Everything the instrument actually sent, under the fused view. A derived
// point is only worth trusting if you can see what it came from -- and a
// rangefinder reading nothing because nothing is in front of it looks
// identical to a broken one until you can watch it.
const RAW_COLS = [
  // by INPUT NUMBER, not compass: which edge each ribbon is has never been
  // established, and labelling them n/s/e/w just asserted the wiring order.
  // Touch one, read the number that moves, and the mapping is settled.
  { k: "rib 0", cls: "rib", src: (r) => r.analog[0], max: 4095 },
  { k: "rib 1", cls: "rib", src: (r) => r.analog[1], max: 4095 },
  { k: "rib 2", cls: "rib", src: (r) => r.analog[2], max: 4095 },
  { k: "rib 3", cls: "rib", src: (r) => r.analog[3], max: 4095 },
  { k: "fsr 0", cls: "frc", src: (r) => r.analog[4], max: 4095 },
  { k: "fsr 1", cls: "frc", src: (r) => r.analog[5], max: 4095 },
  { k: "fsr 2", cls: "frc", src: (r) => r.analog[6], max: 4095 },
  { k: "fsr 3", cls: "frc", src: (r) => r.analog[7], max: 4095 },
];
const LIDAR_ORDER = ["top", "right", "bottom", "left"];

function cell(key, cls, text, frac, off) {
  const el = document.createElement("div");
  el.className = `rawcol ${cls}${off ? " off" : ""}`;
  const k = document.createElement("div"); k.className = "k"; k.textContent = key;
  const n = document.createElement("div"); n.className = "n"; n.textContent = text;
  const m = document.createElement("div"); m.className = "m";
  const i = document.createElement("i");
  i.style.width = `${Math.max(0, Math.min(1, frac)) * 100}%`;
  m.append(i);
  el.append(k, n, m);
  return el;
}

const RIB_EDGE = { softpot_n: "top", softpot_e: "right", softpot_s: "bottom", softpot_w: "left" };

// Naming a ribbon only means something while one is being touched, so the
// buttons are dead until then and say which input they would bind.
function renderSetup(st) {
  const idx = st.touched_ribbon;
  const hint = document.getElementById("riblive");
  hint.textContent = idx === null || idx === undefined
    ? "touch a ribbon →"
    : `rib ${idx} is →`;
  hint.className = "hint" + (idx === null || idx === undefined ? "" : " live");

  const inv = new Set(st.softpot_invert || []);
  const RIB_CID = { top: "softpot_n", right: "softpot_e", bottom: "softpot_s", left: "softpot_w" };
  for (const b of document.querySelectorAll("#setup button[data-invert]")) {
    b.classList.toggle("on", inv.has(RIB_CID[b.dataset.invert]));
  }

  const map = st.softpot_map || {};
  const owned = {};
  for (const [cid, i] of Object.entries(map)) owned[RIB_EDGE[cid]] = i;
  for (const b of document.querySelectorAll("#setup button[data-ribbon]")) {
    const e = b.dataset.ribbon;
    b.disabled = idx === null || idx === undefined;
    b.textContent = owned[e] === undefined ? e : `${e} = rib ${owned[e]}`;
  }
}

function renderRaws(st) {
  const r = st.raw || {};
  if (!r.analog || !r.lidar) return;
  const out = RAW_COLS.map(({ k, cls, src, max }) => {
    const v = src(r) ?? 0;
    return cell(k, cls, v, v / max, v === 0);
  });

  const layout = st.lidar_layout || {};
  const wall = new Set(r.lidar_background || []);
  for (const pos of LIDAR_ORDER) {
    const idx = layout[pos];
    const v = idx === undefined ? undefined : r.lidar[idx];
    const none = v === undefined || v === 0xffff;
    // a wall is marked, not hidden: it is a real reading, just not of a hand
    const label = wall.has(pos) ? `${pos}·wall` : pos;
    out.push(cell(label, "dst", none ? "—" : v, none ? 0 : v / 1200, none || wall.has(pos)));
  }
  raws.replaceChildren(...out);
}

// ── raw traces ───────────────────────────────────────────────────────
// Every sensor as it actually reads, over the last few seconds. Below the
// fold: this is for when something looks wrong, not for playing. A number
// alone cannot tell a sensor that is dead from one that is merely quiet --
// a trace can.
const TRACE_LEN = 220;
const TRACES = [
  ...[0, 1, 2, 3].map((i) => ({
    key: `ribbon ${i}`, cls: "rib", max: 4095,
    read: (st) => (st.raw.analog || [])[i],
  })),
  ...[0, 1, 2, 3].map((i) => ({
    key: `fsr ${i}`, cls: "frc", max: 4095,
    read: (st) => (st.raw.analog || [])[4 + i],
  })),
  ...["top", "right", "bottom", "left"].map((pos) => ({
    key: `lidar ${pos}`, cls: "dst", max: 400, mm: true,
    read: (st) => {
      const idx = (st.lidar_layout || {})[pos];
      const v = idx === undefined ? undefined : (st.raw.lidar || [])[idx];
      return v === 0xffff ? null : v;
    },
  })),
];
const TRACE_COLOR = { rib: "#a77bff", frc: "#ffab3d", dst: "#ff3d78" };
const history = new Map();
let traceCells = null;

function buildTraces() {
  const host = document.getElementById("rawviz-grid");
  traceCells = TRACES.map((t) => {
    const el = document.createElement("div");
    el.className = "trace";
    el.innerHTML =
      `<div class="hd"><span class="nm"></span><span class="vl">—</span></div>` +
      `<svg viewBox="0 0 ${TRACE_LEN} 46" preserveAspectRatio="none">` +
      `<line x1="0" y1="45" x2="${TRACE_LEN}" y2="45"></line><path></path></svg>`;
    el.querySelector(".nm").textContent = t.key;
    el.querySelector("path").setAttribute("stroke", TRACE_COLOR[t.cls]);
    host.append(el);
    history.set(t.key, []);
    return el;
  });
}

function renderTraces(st) {
  if (!st.raw) return;
  if (!traceCells) buildTraces();
  TRACES.forEach((t, i) => {
    const v = t.read(st);
    const hist = history.get(t.key);
    hist.push(v ?? null);
    if (hist.length > TRACE_LEN) hist.shift();

    const cell = traceCells[i];
    const dead = v === null || v === undefined;
    cell.classList.toggle("off", dead);
    cell.querySelector(".vl").textContent =
      dead ? (t.mm ? "no target" : "—") : t.mm ? `${v}mm` : v;

    // autoscale to what this sensor is actually doing, or nothing is legible
    const seen = hist.filter((h) => h !== null);
    const hi = Math.max(t.max * 0.08, ...seen);
    let d = "", pen = false;
    hist.forEach((h, n) => {
      if (h === null) { pen = false; return; }
      const x = n * (TRACE_LEN / (TRACE_LEN - 1));
      const y = 45 - (h / hi) * 43;
      d += `${pen ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`;
      pen = true;
    });
    cell.querySelector("path").setAttribute("d", d);
  });
}

// ── readout ──────────────────────────────────────────────────────────
// One line per DIMENSION, updated in place. The previous version appended
// a line per change and scrolled, which meant the thing you wanted to read
// was always moving. These rows are created once, in frame order, and then
// only their values change -- a channel that goes quiet shows a dash and
// keeps its place rather than vanishing and shifting everything below it.
const AXES = { 1: [""], 2: ["x", "y"], 3: ["x", "y", "z"] };
const MAX_SLOTS = 4;          // more contacts than this: the count row says so
const rows = new Map();
const slots = new Map();      // channel id -> how many contact slots to show
let lastOrder = "";

// A channel can hold several simultaneous contacts -- four pads down is
// four points in `pad`, each with its own pid. Rendering points[0] only
// would show one finger and silently drop the rest. Slots grow to the most
// ever seen (capped) and then STAY, so a released finger leaves a dash
// where it was instead of collapsing the list under your eyes.
function readoutRows(frame) {
  const out = [];
  for (const ch of frame.channels) {
    const axes = AXES[ch.dims] || [];
    const pts = ch.points || [];
    const want = Math.min(Math.max(slots.get(ch.id) || 1, pts.length, 1), MAX_SLOTS);
    slots.set(ch.id, want);

    for (let k = 0; k < want; k++) {
      const p = pts[k];
      axes.forEach((ax, i) => {
        // slot 0 keeps the plain name so its row never gets renamed
        const base = k === 0 ? ch.id : ch.id + (k + 1);
        out.push({
          key: ax ? base + "." + ax : base,
          v: p ? p.v[i] : null,
          s: p ? p.s[i] : null,
          pid: p ? p.pid : null,
        });
      });
    }
    if (want > 1) {
      out.push({ key: ch.id + ".n", v: pts.length, s: null, count: true });
    }
  }
  return out;
}

function makeRow(key) {
  const el = document.createElement("div");
  el.className = "rdrow";
  el.innerHTML =
    '<span class="rdname"></span>' +
    '<span class="rdbar"><i></i></span>' +
    '<span class="rdval"></span>' +
    '<span class="rdsig"></span>';
  el.querySelector(".rdname").textContent = key;
  return {
    el,
    bar: el.querySelector(".rdbar i"),
    val: el.querySelector(".rdval"),
    sig: el.querySelector(".rdsig"),
  };
}

function renderReadout(frame) {
  if (!frame || !frame.channels) return;
  const list = readoutRows(frame);

  // Only touch the DOM order when the set of rows actually changed (a new
  // contact slot appeared). In steady state nothing moves.
  const order = list.map((r) => r.key).join("|");
  const reorder = order !== lastOrder;
  lastOrder = order;

  for (const r of list) {
    let row = rows.get(r.key);
    if (!row) { row = makeRow(r.key); rows.set(r.key, row); }
    if (reorder) mon.append(row.el);

    if (r.count) {
      row.val.textContent = String(r.v);
      row.sig.textContent = "";
      row.bar.style.width = "0%";
      row.el.classList.toggle("off", !r.v);
      continue;
    }
    const live = r.v !== null && r.v !== undefined;
    row.val.textContent = live ? r.v.toFixed(3) : "\u2014";
    row.sig.textContent = live && r.s != null ? "\u00b1" + r.s.toFixed(2) : "";
    row.bar.style.width = (live ? r.v * 100 : 0).toFixed(1) + "%";
    row.el.classList.toggle("off", !live);
  }
}

async function poll() {
  try {
    const st = await (await fetch("/api/state")).json();
    frame = st.frame;
    lastState = st;
    renderRaws(st);
    renderSetup(st);
    renderTraces(st);
    renderReadout(st.frame);
  } catch {
    /* the field keeps decaying; a dropped poll should not blank it */
  }
}

resize();
requestAnimationFrame(render);
poll();
setInterval(poll, POLL_MS);
