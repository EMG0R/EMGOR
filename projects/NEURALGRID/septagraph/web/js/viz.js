/**
 * Real-time 7-axis visualizer (Task 2.4b).
 *
 * A passive listener on the decoded Septagraph protocol stream -- it does
 * not care which controller produced the packets (trackpad, webcam, or a
 * neuralGrid tapped in over the bridge). Draws:
 *
 *   (a) one radar plot per live voice: 8 spokes = the 8 coordinate values,
 *       solid polygon = mu, translucent band = mu +/- sigma, colored by a
 *       hash of voice_id, fading out over 500ms after release;
 *   (b) two 2D slices (freq x scale, freq x harm): live voice dots, the
 *       last 2s of /move trail per voice, and catalog points from received
 *       manifests as warm-white dots with instrument label on hover;
 *   (c) a time strip showing the active play-layer shape's mu(t).
 *
 * `makeViz(canvas) -> { onPacket(decodedMsg), setManifests(list) }`.
 * `onPacket` is O(1) amortized (just updates state); actual drawing runs
 * on its own requestAnimationFrame loop at 60fps and never blocks the
 * caller.
 */

const AXES = ["freq", "loud", "harm", "rate", "scale", "az", "spread", "dist", "el"];
const K = 9;   // v2: + el (elevation)
const TRAIL_MS = 2000;
const RELEASE_FADE_MS = 500;

function hashColor(voiceId) {
  let h = 0;
  for (let i = 0; i < voiceId.length; i++) {
    h = (h * 31 + voiceId.charCodeAt(i)) | 0;
  }
  const hue = Math.abs(h) % 360;
  return { hue, css: (alpha = 1) => `hsla(${hue}, 75%, 55%, ${alpha})` };
}

export function makeViz(canvas) {
  const ctx = canvas.getContext("2d");
  let dpr = window.devicePixelRatio || 1;

  // voiceId -> { mu, sigma, color, releasedAt: number|null, trail: [{t, mu}] }
  const voices = new Map();
  let manifests = []; // array of decoded manifest objects {rendererId, kind, name, lo, hi, points}
  let shape = null; // last play-layer shape: {shapeId, durationS, knots}
  let shapeHitMu = null; // mu of the most recent onset hit, to translate the shape preview like the renderer does

  let hoverPoint = null; // {x,y,label} in CSS px, for tooltip
  let running = true;
  let rafId = null;

  function resize() {
    dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const w = Math.max(1, Math.round(rect.width * dpr));
    const h = Math.max(1, Math.round(rect.height * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
  }

  function onPacket(msg) {
    const now = performance.now();
    if (msg.type === "hit") {
      if (msg.onset) {
        let v = voices.get(msg.voiceId);
        if (!v) {
          v = { color: hashColor(msg.voiceId), trail: [], releasedAt: null };
          voices.set(msg.voiceId, v);
        }
        v.mu = msg.mu.slice();
        v.sigma = msg.sigma.slice();
        v.releasedAt = null;
        v.trail.push({ t: now, mu: v.mu.slice() });
        shapeHitMu = msg.mu.slice();
      } else {
        const v = voices.get(msg.voiceId);
        if (v) v.releasedAt = now;
      }
    } else if (msg.type === "move") {
      if (msg.voiceId === "") return; // global move: no per-voice slot to update
      const v = voices.get(msg.voiceId);
      if (!v) return;
      v.mu = msg.mu.slice();
      v.trail.push({ t: now, mu: v.mu.slice() });
      const cutoff = now - TRAIL_MS;
      while (v.trail.length && v.trail[0].t < cutoff) v.trail.shift();
    } else if (msg.type === "shape") {
      if (msg.layer === 0) shape = msg;
    } else if (msg.type === "manifest") {
      const idx = manifests.findIndex((m) => m.rendererId === msg.rendererId);
      if (idx >= 0) manifests[idx] = msg;
      else manifests.push(msg);
    }
  }

  function setManifests(list) {
    manifests = list.slice();
  }

  function prune(now) {
    for (const [id, v] of voices) {
      if (v.releasedAt !== null && now - v.releasedAt > RELEASE_FADE_MS) {
        voices.delete(id);
      }
      const cutoff = now - TRAIL_MS;
      while (v.trail.length && v.trail[0].t < cutoff) v.trail.shift();
    }
  }

  function voiceAlpha(v, now) {
    if (v.releasedAt === null) return 1;
    return Math.max(0, 1 - (now - v.releasedAt) / RELEASE_FADE_MS);
  }

  // --- layout: radar (top-left), two slices (top-right, bottom-left),
  // time strip (bottom, full width) -----------------------------------
  function layout(W, H) {
    const pad = 10 * dpr;
    const radarSize = Math.min(W * 0.42, H * 0.55);
    const radar = { x: pad, y: pad, w: radarSize, h: radarSize };
    const sliceW = (W - radarSize - pad * 3) / 1;
    const sliceH = (H * 0.55 - pad * 3) / 2;
    const sliceFreqScale = { x: radar.x + radar.w + pad, y: pad, w: sliceW, h: sliceH };
    const sliceFreqHarm = {
      x: radar.x + radar.w + pad,
      y: pad * 2 + sliceH,
      w: sliceW,
      h: sliceH,
    };
    const timeStrip = { x: pad, y: H * 0.58, w: W - 2 * pad, h: H * 0.4 };
    return { radar, sliceFreqScale, sliceFreqHarm, timeStrip };
  }

  function drawRadarPanel(rect, now) {
    ctx.save();
    ctx.strokeStyle = "rgba(128,128,128,0.35)";
    ctx.fillStyle = "rgba(128,128,128,0.6)";
    ctx.font = `${10 * dpr}px sans-serif`;
    const cx = rect.x + rect.w / 2;
    const cy = rect.y + rect.h / 2;
    const R = Math.min(rect.w, rect.h) / 2 - 14 * dpr;

    // spokes + rings
    for (let ring = 1; ring <= 4; ring++) {
      ctx.beginPath();
      for (let i = 0; i <= K; i++) {
        const a = (i / K) * Math.PI * 2 - Math.PI / 2;
        const r = (R * ring) / 4;
        const x = cx + Math.cos(a) * r;
        const y = cy + Math.sin(a) * r;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }
    for (let i = 0; i < K; i++) {
      const a = (i / K) * Math.PI * 2 - Math.PI / 2;
      const x = cx + Math.cos(a) * R;
      const y = cy + Math.sin(a) * R;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(x, y);
      ctx.stroke();
      const lx = cx + Math.cos(a) * (R + 10 * dpr);
      const ly = cy + Math.sin(a) * (R + 10 * dpr);
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(AXES[i], lx, ly);
    }

    let n = 0;
    for (const [voiceId, v] of voices) {
      if (!v.mu) continue;
      const alpha = voiceAlpha(v, now);
      if (alpha <= 0) continue;
      n++;
      const pts = [];
      const ptsLo = [];
      const ptsHi = [];
      for (let i = 0; i < K; i++) {
        const a = (i / K) * Math.PI * 2 - Math.PI / 2;
        const mu = v.mu[i];
        const sig = v.sigma ? v.sigma[i] : 0;
        const lo = Math.max(0, mu - sig);
        const hi = Math.min(1, mu + sig);
        pts.push([cx + Math.cos(a) * R * mu, cy + Math.sin(a) * R * mu]);
        ptsLo.push([cx + Math.cos(a) * R * lo, cy + Math.sin(a) * R * lo]);
        ptsHi.push([cx + Math.cos(a) * R * hi, cy + Math.sin(a) * R * hi]);
      }
      // sigma band: hi polygon minus lo polygon (drawn as filled outer, then
      // the inner cut is skipped for simplicity -- just draw a translucent
      // outer band polygon over the lo polygon area too; visually a soft
      // ring is enough for a "band" reading at a glance)
      ctx.beginPath();
      ptsHi.forEach(([x, y], i) => (i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)));
      ctx.closePath();
      ctx.fillStyle = v.color.css(0.12 * alpha);
      ctx.fill();

      ctx.beginPath();
      pts.forEach(([x, y], i) => (i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)));
      ctx.closePath();
      ctx.strokeStyle = v.color.css(alpha);
      ctx.fillStyle = v.color.css(0.2 * alpha);
      ctx.lineWidth = 2 * dpr;
      ctx.fill();
      ctx.stroke();
    }

    ctx.textAlign = "left";
    ctx.fillStyle = "rgba(128,128,128,0.8)";
    ctx.fillText(`${n} live voice${n === 1 ? "" : "s"}`, rect.x, rect.y + rect.h + 12 * dpr);
    ctx.restore();
  }

  function drawSlice(rect, ix, iy, now) {
    ctx.save();
    ctx.strokeStyle = "rgba(128,128,128,0.35)";
    ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
    ctx.fillStyle = "rgba(128,128,128,0.6)";
    ctx.font = `${10 * dpr}px sans-serif`;
    ctx.fillText(AXES[ix], rect.x + rect.w - 30 * dpr, rect.y + rect.h - 4 * dpr);
    ctx.save();
    ctx.translate(rect.x + 4 * dpr, rect.y + 10 * dpr);
    ctx.fillText(AXES[iy], 0, 0);
    ctx.restore();

    const toPx = (mu) => [rect.x + mu[ix] * rect.w, rect.y + (1 - mu[iy]) * rect.h];

    // catalog points: warm-white dots
    for (const m of manifests) {
      if (m.kind !== "catalog" || !m.points) continue;
      for (const p of m.points) {
        const [x, y] = toPx(p.mu);
        ctx.beginPath();
        ctx.arc(x, y, 3 * dpr, 0, Math.PI * 2);
        ctx.fillStyle = "rgba(255,244,214,0.85)";
        ctx.fill();
        const dx = x - (hoverPoint ? hoverPoint.px : -9999);
        const dy = y - (hoverPoint ? hoverPoint.py : -9999);
        if (hoverPoint && Math.hypot(dx, dy) < 6 * dpr) {
          hoverPoint.label = p.instrument;
        }
      }
    }

    // trails + live dots per voice
    for (const [voiceId, v] of voices) {
      if (!v.mu) continue;
      const alpha = voiceAlpha(v, now);
      if (alpha <= 0) continue;
      ctx.beginPath();
      v.trail.forEach((sample, i) => {
        const [x, y] = toPx(sample.mu);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = v.color.css(0.5 * alpha);
      ctx.lineWidth = 1.5 * dpr;
      ctx.stroke();

      const [x, y] = toPx(v.mu);
      ctx.beginPath();
      ctx.arc(x, y, 4 * dpr, 0, Math.PI * 2);
      ctx.fillStyle = v.color.css(alpha);
      ctx.fill();
    }
    ctx.restore();
  }

  function drawTimeStrip(rect, now) {
    ctx.save();
    ctx.strokeStyle = "rgba(128,128,128,0.35)";
    ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
    ctx.fillStyle = "rgba(128,128,128,0.6)";
    ctx.font = `${10 * dpr}px sans-serif`;
    ctx.fillText("play-layer shape mu(t)", rect.x + 4 * dpr, rect.y + 12 * dpr);

    if (!shape || !shape.knots || shape.knots.length === 0) {
      ctx.fillText("(no shape received -- default static shape in effect)", rect.x + 4 * dpr, rect.y + rect.h / 2);
      ctx.restore();
      return;
    }

    const delta = shapeHitMu
      ? shapeHitMu.map((v, i) => v - (shape.knots[0].mu ? shape.knots[0].mu[i] : 0))
      : new Array(K).fill(0);

    const N_SAMPLES = 128;
    for (let i = 0; i < K; i++) {
      ctx.beginPath();
      for (let s = 0; s <= N_SAMPLES; s++) {
        const t = s / N_SAMPLES;
        const mu = interpAt(shape.knots, t, i) + delta[i];
        const x = rect.x + t * rect.w;
        const y = rect.y + rect.h - Math.max(0, Math.min(1, mu)) * rect.h;
        if (s === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.strokeStyle = hashColor(AXES[i]).css(0.8);
      ctx.lineWidth = 1.5 * dpr;
      ctx.stroke();
    }
    ctx.restore();
  }

  function interpAt(knots, t, axisIdx) {
    if (knots.length === 1) return knots[0].mu[axisIdx];
    t = Math.max(0, Math.min(1, t));
    for (let i = 0; i < knots.length - 1; i++) {
      const a = knots[i];
      const b = knots[i + 1];
      if (t >= a.t && t <= b.t) {
        const span = b.t - a.t || 1e-9;
        const f = (t - a.t) / span;
        return a.mu[axisIdx] + (b.mu[axisIdx] - a.mu[axisIdx]) * f;
      }
    }
    return knots[knots.length - 1].mu[axisIdx];
  }

  function frame() {
    if (!running) return;
    resize();
    const now = performance.now();
    prune(now);

    const W = canvas.width;
    const H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    const L = layout(W, H);
    hoverPoint = hoverPoint ? { ...hoverPoint, label: null } : null;
    drawRadarPanel(L.radar, now);
    drawSlice(L.sliceFreqScale, 0, 4, now); // freq x scale
    drawSlice(L.sliceFreqHarm, 0, 2, now); // freq x harm
    drawTimeStrip(L.timeStrip, now);

    if (hoverPoint && hoverPoint.label) {
      ctx.save();
      ctx.fillStyle = "rgba(20,20,20,0.85)";
      ctx.fillRect(hoverPoint.px + 6 * dpr, hoverPoint.py - 18 * dpr, ctx.measureText(hoverPoint.label).width + 12 * dpr, 16 * dpr);
      ctx.fillStyle = "#fff4d6";
      ctx.font = `${10 * dpr}px sans-serif`;
      ctx.fillText(hoverPoint.label, hoverPoint.px + 12 * dpr, hoverPoint.py - 6 * dpr);
      ctx.restore();
    }

    rafId = requestAnimationFrame(frame);
  }

  function onMouseMove(ev) {
    const rect = canvas.getBoundingClientRect();
    hoverPoint = {
      px: (ev.clientX - rect.left) * dpr,
      py: (ev.clientY - rect.top) * dpr,
      label: null,
    };
  }
  canvas.addEventListener("mousemove", onMouseMove);
  canvas.addEventListener("mouseleave", () => {
    hoverPoint = null;
  });

  rafId = requestAnimationFrame(frame);

  return {
    onPacket,
    setManifests,
    destroy() {
      running = false;
      if (rafId) cancelAnimationFrame(rafId);
      canvas.removeEventListener("mousemove", onMouseMove);
    },
  };
}
