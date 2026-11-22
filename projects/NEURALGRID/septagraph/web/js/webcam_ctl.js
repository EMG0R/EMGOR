/**
 * Webcam 3D controller (Task 4.3), `/move`-only per the fast-plane rule
 * (spec §Global Constraints: "the fast plane never has a model in it" --
 * MediaPipe inference happens once per camera frame and only ever
 * *produces* `/move`/`/hit` bytes; it never sits inline in a hot loop that
 * blocks a renderer).
 *
 * Manifest (`py/controllers/input_manifests/webcam.json`):
 *   {"controller_id":"web-hand","channels":[
 *     {"name":"hand","dims":3,"kind":"continuous"},
 *     {"name":"pinch","dims":1,"kind":"continuous"}]}
 *
 * Device coordinate (one right hand, MediaPipe HandLandmarker landmark
 * indices: 0=wrist, 5=index MCP, 4=thumb tip, 8=index tip):
 *
 *   handSizePx = dist_px(wrist, indexMcp)          -- scaled by frame_h only
 *                                                      (matches spec Task 4.3
 *                                                      literally: "handSize_px
 *                                                      / frame_h"; both
 *                                                      landmark axes are
 *                                                      scaled by the same
 *                                                      frame_h, which is an
 *                                                      approximation for
 *                                                      non-square frames but
 *                                                      is what the spec's
 *                                                      formula says)
 *   d = (1 - wrist.x, 1 - wrist.y, clamp((handSizePx/frame_h - 0.05)/0.25, 0, 1))
 *   pinch = 1 - clamp(dist(thumbTip, indexTip) / handSize, 0, 1)   -- ratio,
 *           computed in the same (unscaled) normalized units so frame_h
 *           cancels out.
 *
 * Window (same default center/extent *pattern* as trackpad_ctl.js, extended
 * with a nonzero extent on `dist` (idx 7) since this window newly assigns
 * it): center=HALF, extent=[0.45,0.3,0.3,0,0.45,0,0,0.45],
 * assign={"hand":[0,4,7], "pinch":[2]} (x->freq, y->scale, depth->dist,
 * pinch->harm).
 *
 * Onsets come from the KEYBOARD, not the hand: Space down -> `/hit onset=1`
 * at the current projected mu for a single fixed voice `"web:hand"`; held ->
 * sustain (the hand keeps streaming `/move` for that voice at camera frame
 * rate, throttled to <=100 Hz); Space up -> `/hit onset=0`. While no key is
 * held, the hand still streams a *global* `/move` (voice_id "") every frame
 * so a passive visualizer (viz.js) can show where the hand is even before
 * the first onset.
 *
 * Exports `handToDevice(landmarks, frameH)` and `pinchFrom(landmarks)` as
 * pure functions (no DOM/camera dependency) so they can be unit-tested with
 * synthetic landmark arrays from `web/test/codec.test.html`.
 */

import { encodeHit, encodeMove } from "./sept_codec.js";
const K = 9;   // v2 axis count: + el (elevation)
import { makeChannel, project as projectMu, modulate } from "./window.js";

// Pinned jsDelivr build -- checked against data.jsdelivr.com at
// implementation time (2026-09-05): latest non-RC published version of
// @mediapipe/tasks-vision was 1.0.1 (RC builds for 2026-08-29..2026-09-04
// exist but are pre-releases; 1.0.1 is the pinned stable).
export const MEDIAPIPE_VERSION = "1.0.1";
const MEDIAPIPE_BASE = `https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@${MEDIAPIPE_VERSION}`;
const MEDIAPIPE_ESM = `${MEDIAPIPE_BASE}/vision_bundle.mjs`;
const MEDIAPIPE_WASM_BASE = `${MEDIAPIPE_BASE}/wasm`;

// Official Google-hosted model asset, per the MediaPipe Hand Landmarker docs.
const HAND_LANDMARKER_MODEL_URL =
  "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task";

// MediaPipe HandLandmarker landmark indices used here.
const WRIST = 0;
const THUMB_TIP = 4;
const INDEX_MCP = 5;
const INDEX_TIP = 8;

export const HALF = new Array(K).fill(0.5);

export const DEFAULT_WINDOW = {
  center: HALF.slice(),
  // Same values as trackpad_ctl.js's DEFAULT_WINDOW at the shared indices
  // (0 freq, 1 loud, 2 harm, 4 scale); idx 7 (dist) is newly assigned here
  // so it gets a matching nonzero extent instead of trackpad's 0.
  extent: [0.45, 0.3, 0.3, 0, 0.45, 0, 0, 0.45],
  assign: { hand: [0, 4, 7], pinch: [2] }, // x->freq, y->scale, depth->dist, pinch->harm
};

export const HAND_CHANNEL = makeChannel("hand", 3, "continuous", null);
export const PINCH_CHANNEL = makeChannel("pinch", 1, "continuous", null);

const VOICE_ID = "web:hand";
const MOVE_MIN_INTERVAL_MS = 1000 / 100; // <=100 Hz hard cap (camera runs ~30 Hz)
const LATENCY_WINDOW = 50; // rolling p50 sample count
const DEPTH_NEAR_PX_RATIO = 0.05;
const DEPTH_FAR_PX_RATIO = 0.3; // 0.05 + 0.25

function clamp01(x) {
  x = Number(x);
  if (Number.isNaN(x)) return 0.0;
  return x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x;
}

function dist2(ax, ay, bx, by) {
  return Math.hypot(ax - bx, ay - by);
}

/**
 * handToDevice(landmarks, frameH) -> [x, y, depth] device coordinate in
 * [0,1]^3. `landmarks` is the MediaPipe normalized-landmark array for one
 * hand (each `{x, y, z}` in [0,1] relative to image width/height
 * respectively); `frameH` is the camera frame height in pixels.
 *
 * Pure function -- no DOM dependency, safe to unit test with synthetic
 * landmark arrays.
 */
export function handToDevice(landmarks, frameH) {
  const wrist = landmarks[WRIST];
  const indexMcp = landmarks[INDEX_MCP];
  // Per the spec's literal formula ("handSize_px / frame_h"), both landmark
  // axes are scaled by the same frame_h -- an approximation for non-square
  // aspect ratios, but exactly what Task 4.3 specifies.
  const handSizePx = dist2(
    wrist.x * frameH,
    wrist.y * frameH,
    indexMcp.x * frameH,
    indexMcp.y * frameH
  );
  const depth = clamp01((handSizePx / frameH - DEPTH_NEAR_PX_RATIO) / (DEPTH_FAR_PX_RATIO - DEPTH_NEAR_PX_RATIO));
  return [clamp01(1 - wrist.x), clamp01(1 - wrist.y), depth];
}

/**
 * pinchFrom(landmarks) -> pinch in [0,1]. Ratio-based (thumbTip<->indexTip
 * distance over wrist<->indexMcp "hand size"), so it is scale-invariant and
 * does not need frameH.
 */
export function pinchFrom(landmarks) {
  const wrist = landmarks[WRIST];
  const indexMcp = landmarks[INDEX_MCP];
  const thumbTip = landmarks[THUMB_TIP];
  const indexTip = landmarks[INDEX_TIP];
  const handSize = dist2(wrist.x, wrist.y, indexMcp.x, indexMcp.y);
  if (handSize <= 1e-9) return 0.0;
  const d = dist2(thumbTip.x, thumbTip.y, indexTip.x, indexTip.y);
  return clamp01(1 - clamp01(d / handSize));
}

function median(sorted) {
  const n = sorted.length;
  if (n === 0) return 0;
  const mid = Math.floor(n / 2);
  return n % 2 === 0 ? (sorted[mid - 1] + sorted[mid]) / 2 : sorted[mid];
}

/**
 * attach(overlayCanvas, opts) -> {
 *   start(): Promise<void>,   // requests camera permission + loads model
 *   stop(): void,             // stops camera tracks + detection loop, keeps page usable
 *   setWindow(win), getWindow(),
 *   isRunning(): boolean,
 *   destroy(): void,
 * }
 *
 * `opts.sinks` — same dual-sink array convention as trackpad_ctl.js
 *   (`{feed?(bytes), send?(bytes)}`).
 * `opts.onPacket(bytes)` — fired for every packet this controller sends.
 * `opts.onStatus(text, level)` — status-bar hook; `level` one of
 *   "info"|"ok"|"error".
 * `opts.onLatency(p50Ms, n)` — fired after every detection with the rolling
 *   camera->packet latency p50 in milliseconds.
 * `opts.window` — initial Window (defaults to DEFAULT_WINDOW).
 */
export function attach(overlayCanvas, opts = {}) {
  const sinks = opts.sinks || [];
  const onPacket = typeof opts.onPacket === "function" ? opts.onPacket : () => {};
  const onStatus = typeof opts.onStatus === "function" ? opts.onStatus : () => {};
  const onLatency = typeof opts.onLatency === "function" ? opts.onLatency : () => {};
  let win = opts.window || DEFAULT_WINDOW;

  let video = null;
  let stream = null;
  let landmarker = null;
  let running = false;
  let rafId = null;
  let lastMoveTs = 0;
  let held = false;
  let heldMu = HALF.slice();
  const latencySamples = [];

  function dispatch(bytes) {
    for (const sink of sinks) {
      if (typeof sink.feed === "function") {
        try {
          sink.feed(bytes);
        } catch (e) {
          console.error("[webcam_ctl] renderer sink threw", e);
        }
      }
      if (typeof sink.send === "function") {
        try {
          sink.send(bytes);
        } catch (e) {
          console.error("[webcam_ctl] bridge sink threw", e);
        }
      }
    }
    onPacket(bytes);
  }

  function recordLatency(frameTs) {
    const latency = performance.now() - frameTs;
    latencySamples.push(latency);
    if (latencySamples.length > LATENCY_WINDOW) latencySamples.shift();
    const sorted = latencySamples.slice().sort((a, b) => a - b);
    onLatency(median(sorted), latencySamples.length);
  }

  function project(hand, pinch) {
    let mu = win.center.slice();
    // project() replaces every assigned index from a single channel's
    // window.assign entry; apply hand then pinch by re-projecting on top of
    // each other's un-assigned passthrough (each call starts from
    // win.center, so merge manually here).
    const muHand = projectMu(win, HAND_CHANNEL, hand);
    const muPinch = projectMu(win, PINCH_CHANNEL, pinch);
    const handIdxs = (win.assign && win.assign.hand) || [];
    const pinchIdxs = (win.assign && win.assign.pinch) || [];
    for (const a of handIdxs) if (a >= 0) mu[a] = muHand[a];
    for (const a of pinchIdxs) if (a >= 0) mu[a] = muPinch[a];
    return mu;
  }

  function drawOverlay(landmarks, frameW, frameH) {
    if (!overlayCanvas) return;
    const ctx = overlayCanvas.getContext("2d");
    const w = overlayCanvas.width;
    const h = overlayCanvas.height;
    ctx.clearRect(0, 0, w, h);
    if (!landmarks) return;
    // Mirror x (device coords already mirror the wrist; keep the overlay
    // consistent with a "selfie view" so the hand tracks intuitively).
    const px = (lm) => (1 - lm.x) * w;
    const py = (lm) => lm.y * h;
    const bones = [
      [0, 1], [1, 2], [2, 3], [3, 4], // thumb
      [0, 5], [5, 6], [6, 7], [7, 8], // index
      [5, 9], [9, 10], [10, 11], [11, 12], // middle
      [9, 13], [13, 14], [14, 15], [15, 16], // ring
      [13, 17], [17, 18], [18, 19], [19, 20], // pinky
      [0, 17],
    ];
    ctx.strokeStyle = "rgba(111,182,255,0.6)";
    ctx.lineWidth = 2;
    for (const [a, b] of bones) {
      ctx.beginPath();
      ctx.moveTo(px(landmarks[a]), py(landmarks[a]));
      ctx.lineTo(px(landmarks[b]), py(landmarks[b]));
      ctx.stroke();
    }
    ctx.fillStyle = "#7fdb7f";
    for (const lm of landmarks) {
      ctx.beginPath();
      ctx.arc(px(lm), py(lm), 3, 0, Math.PI * 2);
      ctx.fill();
    }
    // highlight wrist / index MCP / pinch pair
    ctx.fillStyle = "#ffb86c";
    for (const i of [WRIST, INDEX_MCP]) {
      ctx.beginPath();
      ctx.arc(px(landmarks[i]), py(landmarks[i]), 5, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.strokeStyle = "rgba(255,184,108,0.9)";
    ctx.beginPath();
    ctx.moveTo(px(landmarks[THUMB_TIP]), py(landmarks[THUMB_TIP]));
    ctx.lineTo(px(landmarks[INDEX_TIP]), py(landmarks[INDEX_TIP]));
    ctx.stroke();
  }

  function onDetection(result, frameTs, frameH) {
    const hands = result && result.landmarks;
    if (!hands || hands.length === 0) {
      drawOverlay(null);
      return;
    }
    const landmarks = hands[0];
    drawOverlay(landmarks, video.videoWidth, frameH);

    const d = handToDevice(landmarks, frameH);
    const pinch = pinchFrom(landmarks);
    const mu = project(d, [pinch]);

    const now = performance.now();
    if (now - lastMoveTs < MOVE_MIN_INTERVAL_MS) return;
    lastMoveTs = now;

    if (held) {
      heldMu = mu;
      dispatch(encodeMove({ voiceId: VOICE_ID, mu }));
    } else {
      // global move so a passive visualizer shows the hand even with no
      // voice held
      dispatch(encodeMove({ voiceId: "", mu }));
    }
    recordLatency(frameTs);
  }

  async function detectLoop() {
    if (!running || !landmarker || !video) return;
    const frameTs = performance.now();
    let result = null;
    try {
      result = landmarker.detectForVideo(video, frameTs);
    } catch (e) {
      console.error("[webcam_ctl] detectForVideo threw", e);
    }
    if (result) onDetection(result, frameTs, video.videoHeight || 1);

    if (running) {
      if (typeof video.requestVideoFrameCallback === "function") {
        video.requestVideoFrameCallback(() => detectLoop());
      } else {
        rafId = requestAnimationFrame(detectLoop);
      }
    }
  }

  function onKeyDown(ev) {
    if (ev.code !== "Space" || ev.repeat) return;
    if (!running) return;
    ev.preventDefault();
    if (held) return;
    held = true;
    dispatch(
      encodeHit({
        voiceId: VOICE_ID,
        layer: 0,
        onset: 1,
        velocity: 0.7,
        mu: heldMu,
        sigma: new Array(K).fill(0),
      })
    );
  }

  function onKeyUp(ev) {
    if (ev.code !== "Space") return;
    if (!held) return;
    ev.preventDefault();
    held = false;
    dispatch(
      encodeHit({
        voiceId: VOICE_ID,
        layer: 0,
        onset: 0,
        velocity: 0.7,
        mu: heldMu,
        sigma: new Array(K).fill(0),
      })
    );
  }

  async function loadModel() {
    const { HandLandmarker, FilesetResolver } = await import(MEDIAPIPE_ESM);
    const vision = await FilesetResolver.forVisionTasks(MEDIAPIPE_WASM_BASE);
    const base = {
      baseOptions: { modelAssetPath: HAND_LANDMARKER_MODEL_URL, delegate: "GPU" },
      runningMode: "VIDEO",
      numHands: 1,
    };
    try {
      return await HandLandmarker.createFromOptions(vision, base);
    } catch (e) {
      console.warn("[webcam_ctl] GPU delegate failed, falling back to CPU:", e);
      onStatus("webcam: GPU delegate unavailable, using CPU", "info");
      return await HandLandmarker.createFromOptions(vision, {
        ...base,
        baseOptions: { ...base.baseOptions, delegate: "CPU" },
      });
    }
  }

  async function start() {
    if (running) return;
    onStatus("webcam: requesting camera permission…", "info");
    try {
      stream = await navigator.mediaDevices.getUserMedia({
        video: { width: { ideal: 640 }, height: { ideal: 480 } },
        audio: false,
      });
    } catch (e) {
      onStatus(`webcam: camera unavailable (${e.name || e}) — staying on trackpad`, "error");
      throw e;
    }

    video = document.createElement("video");
    video.muted = true;
    video.playsInline = true;
    video.srcObject = stream;
    await video.play();

    onStatus("webcam: loading hand-tracking model…", "info");
    try {
      landmarker = await loadModel();
    } catch (e) {
      onStatus(`webcam: model failed to load (${e.message || e}) — staying on trackpad`, "error");
      stopStream();
      throw e;
    }

    running = true;
    held = false;
    latencySamples.length = 0;
    document.addEventListener("keydown", onKeyDown);
    document.addEventListener("keyup", onKeyUp);
    onStatus("webcam: tracking", "ok");
    detectLoop();
  }

  function stopStream() {
    if (stream) {
      for (const track of stream.getTracks()) track.stop();
      stream = null;
    }
    if (video) {
      video.srcObject = null;
      video = null;
    }
  }

  function stop() {
    if (!running) {
      stopStream();
      return;
    }
    running = false;
    if (rafId != null) cancelAnimationFrame(rafId);
    rafId = null;
    document.removeEventListener("keydown", onKeyDown);
    document.removeEventListener("keyup", onKeyUp);
    if (held) {
      held = false;
      dispatch(
        encodeHit({
          voiceId: VOICE_ID,
          layer: 0,
          onset: 0,
          velocity: 0.7,
          mu: heldMu,
          sigma: new Array(K).fill(0),
        })
      );
    }
    stopStream();
    drawOverlay(null);
    onStatus("webcam: stopped", "info");
  }

  return {
    start,
    stop,
    setWindow(w) {
      win = w;
    },
    getWindow() {
      return win;
    },
    isRunning() {
      return running;
    },
    destroy() {
      stop();
      if (landmarker && typeof landmarker.close === "function") {
        try {
          landmarker.close();
        } catch (e) {
          /* ignore */
        }
      }
      landmarker = null;
    },
  };
}
