// Dimensional UI: calibration guidance, then the live encoder panel.
//
// The run view deliberately shows sigma as a band around every value. That is
// the tuning tool: you move a sensor, you watch the bar, and you can see how
// precisely the device actually knows what it is reporting.

const $ = (id) => document.getElementById(id);
const AXIS_NAMES = { 1: ["v"], 2: ["x", "y"], 3: ["x", "y", "z"] };
const POLL_MS = 100;

let lastMode = null;

async function post(path, body) {
  await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  tick();
}

// ── calibrate ────────────────────────────────────────────────────────
const HW_SOFTPOT = ["softpot 0", "softpot 1", "softpot 2", "softpot 3"];
const HW_LIDAR = ["lidar 0", "lidar 1", "lidar 2", "lidar 3"];

function renderCalibrate(st) {
  const c = st.calibration;
  window.__calKey = c.key;
  $("cal-counter").textContent = `${c.done + 1} / ${c.total}`;
  $("cal-channel").textContent =
    c.kind === "fsr_rest" ? "resting offsets" : `${c.position} edge`;
  $("cal-instruction").textContent = c.instruction;

  const status = $("cal-status");

  if (c.kind === "fsr_rest") {
    const got = c.rest_samples || 0, need = c.rest_needed || 8;
    $("cal-hw").textContent = "all 4 FSRs";
    $("cal-lo").textContent = c.rest ? c.rest.slice(0, 2).join(" / ") : "—";
    $("cal-hi").textContent = c.rest ? c.rest.slice(2, 4).join(" / ") : "—";
    $("cal-span").textContent = `${got}/${need}`;
    $("cal-meter").style.width = `${Math.min(100, (got / need) * 100)}%`;
    status.textContent = c.satisfied
      ? "resting load captured — this is what the housing presses on. Next step."
      : "hold still, hands off…";
    status.className = "status" + (c.satisfied ? " ok" : "");
  } else if (c.kind === "lidar_arc") {
    // The sensors stare at the housing until a hand comes in, so "has anything
    // moved" is the live signal -- not which sensor is nearest, which would
    // just pick whichever one is closest to a wall.
    const resp = c.arc_response || 0;
    $("cal-hw").textContent = "decided after";
    $("cal-lo").textContent = "—";
    $("cal-hi").textContent = "—";
    $("cal-span").textContent = `${resp.toFixed(0)}mm`;
    $("cal-meter").style.width = `${Math.min(100, (resp / 25) * 100)}%`;
    if (c.satisfied) {
      status.textContent = "good — a sensor answered. One more arc won't hurt.";
      status.className = "status ok";
    } else if (resp > 0) {
      status.textContent = `${resp.toFixed(0)}mm of movement so far — keep arcing.`;
      status.className = "status";
    } else {
      status.textContent = "arc your hand over this edge — nothing has moved yet.";
      status.className = "status";
    }
  } else {
    const names = HW_SOFTPOT;
    $("cal-hw").textContent = c.hw_index == null ? "—" : names[c.hw_index];
    const lo = c.lo, hi = c.hi;
    const span = lo != null && hi != null ? hi - lo : null;
    $("cal-lo").textContent = lo != null ? lo : "—";
    $("cal-hi").textContent = hi != null ? hi : "—";
    $("cal-span").textContent = span != null ? span : "—";
    const target = c.kind === "lidar" ? 600 : 3000;
    $("cal-meter").style.width = span != null ? `${Math.min(100, (span / target) * 100)}%` : "0%";

    if (c.satisfied) {
      status.textContent = `good — ${c.position} is ${names[c.hw_index]}. Next step.`;
      status.className = "status ok";
    } else if (c.hw_index != null) {
      status.textContent =
        c.kind === "lidar"
          ? `${names[c.hw_index]} answered — now draw your hand back.`
          : `${names[c.hw_index]} answered — sweep all the way to both ends.`;
      status.className = "status";
    } else {
      status.textContent =
        c.kind === "lidar"
          ? "waiting to see which rangefinder answers…"
          : "waiting to see which ribbon answers…";
      status.className = "status";
    }
  }

  const steps = $("cal-steps");
  steps.replaceChildren(
    ...c.steps.map((s) => {
      const el = document.createElement("button");
      el.className =
        "chip" + (s.satisfied ? " done" : "") + (s.key === c.key ? " here" : "");
      el.textContent =
        s.kind === "fsr_rest" ? "rest" : `${s.kind === "lidar_arc" ? "◎" : "▬"} ${s.position}`;
      el.onclick = () => post("/api/calibrate/redo", { channel: s.key });
      return el;
    })
  );
}

// ── run ──────────────────────────────────────────────────────────────
function axisRow(name, value, sigma) {
  const row = document.createElement("div");
  row.className = "axis";

  const nm = document.createElement("div");
  nm.className = "axis-name";
  nm.textContent = name;

  const track = document.createElement("div");
  track.className = "track";

  const band = document.createElement("div");
  band.className = "band";
  const lo = Math.max(0, value - sigma) * 100;
  const hi = Math.min(1, value + sigma) * 100;
  band.style.left = `${lo}%`;
  band.style.width = `${Math.max(hi - lo, 0.5)}%`;

  const dot = document.createElement("div");
  dot.className = "dot";
  dot.style.left = `calc(${value * 100}% - 1.5px)`;

  track.append(band, dot);

  const val = document.createElement("div");
  val.className = "val";
  val.textContent = `${value.toFixed(3)} ±${sigma.toFixed(3)}`;

  row.append(nm, track, val);
  return row;
}

function renderChannel(ch) {
  const el = document.createElement("div");
  el.className = "chan" + (ch.points.length ? "" : " idle");

  const head = document.createElement("div");
  head.className = "chan-head";
  const id = document.createElement("span");
  id.className = "chan-id";
  id.textContent = ch.id;
  const meta = document.createElement("span");
  meta.className = "chan-meta";
  meta.textContent = `${ch.dims}D · ${ch.label}`;
  head.append(id, meta);
  if (ch.assoc && ch.assoc.length) {
    const a = document.createElement("span");
    a.className = "assoc";
    a.textContent = `→ ${ch.assoc.join(", ")}`;
    head.append(a);
  }
  el.append(head);

  if (!ch.points.length) {
    const idle = document.createElement("div");
    idle.className = "idlemsg";
    idle.textContent = "inactive";
    el.append(idle);
    return el;
  }

  const names = AXIS_NAMES[ch.dims] || [];
  ch.points.forEach((p, i) => {
    if (ch.points.length > 1) {
      const tag = document.createElement("div");
      tag.className = "chan-meta";
      tag.textContent = `point ${i + 1} of ${ch.points.length}`;
      el.append(tag);
    }
    p.v.forEach((v, d) => el.append(axisRow(names[d] || String(d), v, p.s[d])));
  });
  return el;
}

function renderRun(st) {
  $("channels").replaceChildren(...st.frame.channels.map(renderChannel));
  if ($("wire").parentElement.open) {
    $("wire").textContent = JSON.stringify(st.frame, null, 1);
  }
}

// ── per-pad FSR ──────────────────────────────────────────────────────
function renderFsr(st) {
  const f = st.fsr;
  if (!f) return;

  if (f.phase === "rest") {
    $("fsr-counter").textContent = `${f.rest_samples}/${f.rest_needed}`;
    $("fsr-title").textContent = "hands off";
    $("fsr-instruction").textContent =
      "Reading the resting load the housing puts on the sensors. " +
      "This is the zero point every pad reading is measured against.";
    $("fsr-status").textContent = "hold still…";
    $("fsr-status").className = "status";
    $("padgrid").replaceChildren();
    return;
  }

  $("fsr-counter").textContent = `${f.done} / ${f.total}`;
  $("fsr-title").textContent = f.complete
    ? "every pad done"
    : `hit pad x${f.x} y${f.y}`;
  $("fsr-instruction").textContent =
    "Sweep the lit pad soft → hard → soft, the way you'd actually play it. " +
    "The whole sweep is the measurement, not the peak — it fits the sensor " +
    "ratio across your full pressure range, then moves on by itself.";

  const done = new Set(f.calibrated), unfelt = new Set(f.unfelt);
  $("padgrid").replaceChildren(
    ...Array.from({ length: f.total }, (_, pad) => {
      const el = document.createElement("i");
      const q = (f.quality_all || {})[pad];
      if (pad === f.pad) el.className = "here";
      else if (unfelt.has(pad)) el.className = q && q.reason === "stab" ? "stab" : "unfelt";
      else if (done.has(pad)) el.className = "done";
      el.title = q
        ? `pad ${pad} — peak ${q.peak}, span ${q.span}, stability ${q.stability}` +
          (q.reason ? ` — REJECTED: ${q.reason}` : "")
        : `pad ${pad} (x${pad % 8} y${Math.floor(pad / 8)})`;
      el.onclick = () => post("/api/fsr/goto", { pad });
      return el;
    })
  );

  renderFsrBank(st, f);
  renderCurve(f.curve || []);

  const status = $("fsr-status");
  const SWEEP_TARGET = 600;
  if (f.holding) {
    const span = f.span || 0;
    const pct = Math.min(100, (span / SWEEP_TARGET) * 100);
    status.innerHTML =
      span < SWEEP_TARGET
        ? `sweeping — soft → hard → soft &nbsp;<b>${pct.toFixed(0)}%</b> of the range covered`
        : `good range — ease back off and let go`;
    status.className = "status" + (span >= SWEEP_TARGET ? " ok" : "");
  } else if (f.complete) {
    status.textContent = "all pads visited — save it.";
    status.className = "status ok";
  } else if (unfelt.size) {
    const qa = f.quality_all || {};
    const stabs = [...unfelt].filter((p) => (qa[p] || {}).reason === "stab").length;
    const faint = unfelt.size - stabs;
    status.textContent =
      `${unfelt.size} to redo — ${stabs} read as a stab (amber), ` +
      `${faint} too faint for any sensor (red). Hover a square for its numbers.`;
    status.className = "status";
  } else {
    status.textContent = "sweep the lit pad: soft → hard → soft, then let go.";
    status.className = "status";
  }
}

const SENSOR_COLORS = ["#57d1ff", "#ffb545", "#4ade80", "#ff6b9d"];
const FSR_PINS = ["GPIO36 · ADC1", "GPIO39 · ADC1", "GPIO15 · ADC2", "GPIO12 · ADC2"];
const ADC_FULL = 4095;

// All four sensors, live, on the screen where they are being calibrated.
// A channel pinned at exactly 0 is called out: FSR2/FSR3 are the two ADC2
// pins, and both reading a flat zero is a peripheral pattern, not two
// sensors independently dying.
function renderFsrBank(st, f) {
  const analog = (st.raw && st.raw.analog) || [];
  const rest = f.rest || [];
  const cells = [0, 1, 2, 3].map((i) => {
    const raw = analog[4 + i];
    const has = raw !== undefined;
    const zero = has && raw === 0;
    const delta = has && rest[i] !== undefined ? raw - rest[i] : null;

    const el = document.createElement("div");
    el.className = "fsrcell" + (zero ? " dead" : delta > 30 ? " live" : "");

    const nm = document.createElement("div");
    nm.className = "nm";
    nm.textContent = `fsr ${i}`;
    const pin = document.createElement("div");
    pin.className = "pin";
    pin.textContent = FSR_PINS[i];
    const val = document.createElement("div");
    val.className = "raw";
    val.textContent = has ? raw : "—";
    const d = document.createElement("div");
    d.className = "delta" + (delta !== null && delta <= 0 ? " neg" : "");
    d.textContent = zero
      ? "reads 0"
      : delta === null
      ? ""
      : `${delta > 0 ? "+" : ""}${delta} vs rest`;

    const bar = document.createElement("div");
    bar.className = "fsrbar";
    const fill = document.createElement("i");
    fill.style.width = `${Math.min(100, ((raw || 0) / ADC_FULL) * 100)}%`;
    bar.append(fill);

    el.append(nm, pin, val, d, bar);
    return el;
  });
  $("fsrbank").replaceChildren(...cells);
}

function renderCurve(rows) {
  const svg = $("fsr-curve");
  if (!rows.length) {
    svg.replaceChildren();
    return;
  }
  const W = 320, H = 110, PAD = 6;
  const maxV = Math.max(1, ...rows.flat());
  const paths = SENSOR_COLORS.map((color, i) => {
    const d = rows
      .map((r, n) => {
        const x = PAD + (n / Math.max(1, rows.length - 1)) * (W - 2 * PAD);
        const y = H - PAD - ((r[i] || 0) / maxV) * (H - 2 * PAD);
        return `${n ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`;
      })
      .join(" ");
    const el = document.createElementNS("http://www.w3.org/2000/svg", "path");
    el.setAttribute("d", d);
    el.setAttribute("stroke", color);
    return el;
  });
  svg.replaceChildren(...paths);
}

// ── raw ──────────────────────────────────────────────────────────────
const ANALOG_NAMES = [
  "softpot n", "softpot s", "softpot e", "softpot w",
  "fsr 0", "fsr 1", "fsr 2", "fsr 3",
];
// Filled from state.lidar_layout: which hardware index sits at which physical
// edge. The firmware's N/S/E/W is wiring order, NOT position.
let LIDAR_NAMES = ["lidar 0", "lidar 1", "lidar 2", "lidar 3"];

// Both names on purpose: the position you see, and the firmware letter the
// wiring/docs use. Relabelling to position alone made the firmware-N sensor
// read "lidar left" and look like N had vanished.
const FW_LETTER = { top: "S", right: "E", bottom: "W", left: "N" };

function setLidarNames(layout) {
  if (!layout) return;
  const names = ["lidar 0", "lidar 1", "lidar 2", "lidar 3"];
  for (const [pos, idx] of Object.entries(layout)) {
    if (idx >= 0 && idx < 4) names[idx] = `${pos} · fw ${FW_LETTER[pos] || "?"}`;
  }
  LIDAR_NAMES = names;
}
const ADC_MAX = 4095;
const LIDAR_INVALID = 0xffff;

function rawCell(name, text, frac, opts = {}) {
  const el = document.createElement("div");
  el.className = "raw" + (opts.lit ? " lit" : "") + (opts.dead ? " dead" : "");
  const n = document.createElement("div");
  n.className = "raw-name";
  n.textContent = name;
  const v = document.createElement("div");
  v.className = "raw-val";
  v.textContent = text;
  const bar = document.createElement("div");
  bar.className = "raw-bar";
  const fill = document.createElement("i");
  fill.style.width = `${Math.max(0, Math.min(1, frac)) * 100}%`;
  bar.append(fill);
  el.append(n, v, bar);
  return el;
}

function renderRaw(st, mode) {
  const r = st.raw || {};
  const analog = r.analog || [];
  const lidar = r.lidar || [];
  const floor = r.touch_floor ?? 0;
  const cells = [];

  analog.forEach((v, i) =>
    cells.push(rawCell(ANALOG_NAMES[i] || `analog ${i}`, v, v / ADC_MAX, { lit: v > floor }))
  );
  lidar.forEach((v, i) => {
    // "no target" is NORMAL -- nothing is in range -- and must not look like a
    // missing or broken sensor. Both arrive upstream as 0xFFFF.
    const noTarget = v === LIDAR_INVALID;
    cells.push(
      rawCell(
        LIDAR_NAMES[i] || `lidar ${i}`,
        noTarget ? "no target" : `${v}mm`,
        noTarget ? 0 : v / 2000,
        { lit: !noTarget, dead: noTarget }
      )
    );
  });

  $("rawgrid").replaceChildren(...cells);
  const fl = r.force_floor, ce = r.force_ceiling;
  $("rawnote").textContent =
    fl == null
      ? `touch floor ${floor} · force ranging begins in encoder mode`
      : `touch floor ${floor} · force ranges ${Math.round(fl)}…${Math.round(ce)} ` +
        `(resting level learned live — no FSR calibration file)`;
}

// ── poll ─────────────────────────────────────────────────────────────
async function tick() {
  let st;
  try {
    st = await (await fetch("/api/state")).json();
  } catch {
    $("link").textContent = "server unreachable";
    $("link").className = "link down";
    return;
  }

  setLidarNames(st.lidar_layout);
  if (st.capturing_rest) {
    $("link").textContent = "re-zeroing — hands off";
    $("link").className = "link";
  }
  const l = st.link || {};
  $("link").textContent = l.connected
    ? `grid connected · fw ${l.fw_ver ?? "?"} · ${l.n_analog ?? "?"} analog`
    : `no grid${l.port ? ` (${l.port})` : " — no serial port found"}`;
  $("link").className = "link " + (l.connected ? "up" : "down");
  $("ver").textContent = `v${st.format_version}`;

  if (st.mode !== lastMode) {
    $("calibrate").hidden = st.mode !== "calibrate";
    $("fsr").hidden = st.mode !== "fsr";
    $("run").hidden = st.mode !== "run";
    lastMode = st.mode;
  }
  if (st.mode === "calibrate") renderCalibrate(st);
  else if (st.mode === "fsr") renderFsr(st);
  else renderRun(st);
  renderRaw(st, st.mode);
}

$("cal-next").onclick = () => post("/api/calibrate/next");
$("cal-finish").onclick = () => post("/api/calibrate/finish");
$("cal-redo").onclick = () => {
  const key = window.__calKey;
  if (key) post("/api/calibrate/redo", { channel: key });
};
$("run-recal").onclick = () => post("/api/calibrate/start");
$("run-fsr").onclick = () => post("/api/fsr/start");
$("run-rezero").onclick = () => post("/api/fsr/rest");
$("fsr-skip").onclick = () => post("/api/fsr/skip");
$("fsr-finish").onclick = () => post("/api/fsr/finish");

tick();
setInterval(tick, POLL_MS);
