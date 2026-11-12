/**
 * Trackpad controller (Task 2.4).
 *
 * Turns pointer gestures on a square "device space" pad into Septagraph
 * `/hit` and `/move` messages via the §C window projection
 * (`window.js`), matching the input manifest at
 * `py/controllers/input_manifests/trackpad.json`:
 *
 *   {"controller_id":"web-trackpad","channels":[
 *     {"name":"pad","dims":2,"kind":"continuous","size":null},
 *     {"name":"click","dims":1,"kind":"discrete","size":[1]}]}
 *
 * Default window (matches the neuralGrid grid defaults: x -> freq, y ->
 * scale): center=HALF, extent=[0.45,0.3,0.3,0,0.45,0,0,0],
 * assign={"pad":[0,4]}.
 *
 * Interaction:
 *   pointerdown  -> /hit  onset=1 velocity=0.7 mu=project(pad, (x,y))
 *   pointermove  -> /move (<=100 Hz, one stream per held pointer)
 *   pointerup    -> /hit  onset=0
 *   wheel        -> modulate harm (idx 2) by +-0.05 per notch (bipolar,
 *                   controller-side, applied to every currently-held voice)
 *   multiple simultaneous pointers = multiple independently-held voices.
 *
 * voice_id = "web:<n>" where n increments per new pointer (never reused
 * while other pointers are still held, matching "controllers guarantee
 * uniqueness per held note").
 *
 * Every encoded packet is handed to BOTH sinks passed into `attach()`:
 * the in-page WebChucK renderer and (if connected) the bridge.
 */

import { encodeHit, encodeMove } from "./sept_codec.js";
const K = 9;   // v2 axis count: + el (elevation)
import { makeChannel, modulate, project as projectMu } from "./window.js";

export const HALF = new Array(K).fill(0.5);

export const DEFAULT_WINDOW = {
  center: HALF.slice(),
  extent: [0.45, 0.3, 0.3, 0, 0.45, 0, 0, 0],
  assign: { pad: [0, 4] }, // x -> freq (idx 0), y -> scale (idx 4)
};

export const PAD_CHANNEL = makeChannel("pad", 2, "continuous", null);

const MOVE_MIN_INTERVAL_MS = 1000 / 100; // <=100 Hz
const WHEEL_HARM_STEP = 0.05;
const HARM_IDX = 2;

/**
 * attach(padEl, opts) -> { setWindow(win), destroy(), state }
 *
 * `opts.sinks` — array of `{feed?(bytes), send?(bytes)}`; every encoded
 *   packet is pushed through every sink that has a matching method (the
 *   in-page renderer exposes `feed`, the bridge client exposes `send`).
 * `opts.onPacket(bytes, decodedLikeInfo)` — optional hook fired for every
 *   packet this controller sends, e.g. to drive the hex-dump readout and
 *   the local visualizer (which also gets fed by the renderer's own
 *   decode, but the controller is the simplest single source of truth for
 *   "the last packet this page sent").
 * `opts.window` — initial Window (defaults to DEFAULT_WINDOW).
 */
export function attach(padEl, opts = {}) {
  const sinks = opts.sinks || [];
  const onPacket = typeof opts.onPacket === "function" ? opts.onPacket : () => {};
  let win = opts.window || DEFAULT_WINDOW;

  // pointerId -> { n, voiceId, lastMoveTs, mu }
  const pointers = new Map();
  let voiceCounter = 0;

  function dispatch(bytes) {
    for (const sink of sinks) {
      if (typeof sink.feed === "function") {
        try {
          sink.feed(bytes);
        } catch (e) {
          console.error("[trackpad_ctl] renderer sink threw", e);
        }
      }
      if (typeof sink.send === "function") {
        try {
          sink.send(bytes);
        } catch (e) {
          console.error("[trackpad_ctl] bridge sink threw", e);
        }
      }
    }
    onPacket(bytes);
  }

  function deviceCoordFromEvent(ev) {
    const rect = padEl.getBoundingClientRect();
    const x = Math.min(1, Math.max(0, (ev.clientX - rect.left) / rect.width));
    // invert y so "up" on the pad reads as higher device-y (screen y grows
    // downward; device space y=1 is conventionally "up"/high on the pad)
    const y = Math.min(1, Math.max(0, 1 - (ev.clientY - rect.top) / rect.height));
    return [x, y];
  }

  function project(d) {
    return projectMu(win, PAD_CHANNEL, d, 0.7);
  }

  function onPointerDown(ev) {
    // Capture is a best-effort convenience (keeps pointermove/pointerup
    // targeting padEl even if the pointer strays outside it while held);
    // it can throw NotFoundError in edge cases the UA doesn't consider an
    // "active" pointer yet, which must never block the /hit this gesture
    // produces.
    try {
      padEl.setPointerCapture(ev.pointerId);
    } catch (e) {
      /* ignore -- capture is best-effort, see comment above */
    }
    const d = deviceCoordFromEvent(ev);
    const mu = project(d);
    voiceCounter += 1;
    const voiceId = `web:${voiceCounter}`;
    const state = { n: voiceCounter, voiceId, lastMoveTs: 0, mu, d };
    pointers.set(ev.pointerId, state);

    const bytes = encodeHit({
      voiceId,
      layer: 0,
      onset: 1,
      velocity: 0.7,
      mu,
      sigma: new Array(K).fill(0),
    });
    dispatch(bytes);
  }

  function onPointerMove(ev) {
    const state = pointers.get(ev.pointerId);
    if (!state) return;
    const now = performance.now();
    if (now - state.lastMoveTs < MOVE_MIN_INTERVAL_MS) return;
    state.lastMoveTs = now;

    const d = deviceCoordFromEvent(ev);
    state.d = d;
    const mu = project(d);
    state.mu = mu;

    const bytes = encodeMove({ voiceId: state.voiceId, mu });
    dispatch(bytes);
  }

  function onPointerUp(ev) {
    const state = pointers.get(ev.pointerId);
    if (!state) return;
    pointers.delete(ev.pointerId);
    try {
      padEl.releasePointerCapture(ev.pointerId);
    } catch (e) {
      /* ignore */
    }

    const bytes = encodeHit({
      voiceId: state.voiceId,
      layer: 0,
      onset: 0,
      velocity: 0.7,
      mu: state.mu,
      sigma: new Array(K).fill(0),
    });
    dispatch(bytes);
  }

  function onWheel(ev) {
    ev.preventDefault();
    const delta = ev.deltaY < 0 ? WHEEL_HARM_STEP : -WHEEL_HARM_STEP;
    // modulate every currently-held voice's harm axis and re-send /move
    for (const state of pointers.values()) {
      state.mu = modulate(state.mu, HARM_IDX, delta, win);
      const bytes = encodeMove({ voiceId: state.voiceId, mu: state.mu });
      dispatch(bytes);
    }
    // also nudge the window center so a fresh pointerdown (no pointers
    // currently held) still reflects the last wheel adjustment
    win = { ...win, center: modulate(win.center, HARM_IDX, delta, win) };
  }

  padEl.addEventListener("pointerdown", onPointerDown);
  padEl.addEventListener("pointermove", onPointerMove);
  padEl.addEventListener("pointerup", onPointerUp);
  padEl.addEventListener("pointercancel", onPointerUp);
  padEl.addEventListener("wheel", onWheel, { passive: false });

  return {
    setWindow(w) {
      win = w;
    },
    getWindow() {
      return win;
    },
    activeVoiceCount() {
      return pointers.size;
    },
    destroy() {
      padEl.removeEventListener("pointerdown", onPointerDown);
      padEl.removeEventListener("pointermove", onPointerMove);
      padEl.removeEventListener("pointerup", onPointerUp);
      padEl.removeEventListener("pointercancel", onPointerUp);
      padEl.removeEventListener("wheel", onWheel);
    },
  };
}

