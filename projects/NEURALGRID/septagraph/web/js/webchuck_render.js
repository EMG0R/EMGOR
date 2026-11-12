/**
 * WebChucK renderer shim (Task 2.3).
 *
 * Loads `sept_render_core.ck` -- the SAME .ck source that
 * `sept_render_osc.ck` drives natively (see `chuck/README.md`) -- into a
 * WebChucK `Chuck` instance, then decodes incoming Septagraph OSC packets
 * with `sept_codec.js` and drives the core's named globals
 * (`v_mu_<n>_<i>`, `v_sig_<n>_<i>`, `v_vel_<n>`, `v_gate_<n>`, `v_hit_<n>`,
 * `sh_*`) exactly as `sept_render_osc.ck` does, via
 * `chuck.setFloat`/`setInt`/`broadcastEvent` instead of `OscIn`.
 *
 * The core .ck is NOT modified for the browser -- no WebChucK API
 * limitation forced a rename (`setFloat`/`setInt`/`broadcastEvent` all
 * work by the exact global names the core declares).
 *
 * Usage:
 *   import { startRenderer } from "./webchuck_render.js";
 *   const renderer = await startRenderer("../chuck/sept_render_core.ck");
 *   renderer.feed(bytesFromDecodeOrRawOsc);
 */

import { Chuck } from "https://cdn.jsdelivr.net/npm/webchuck@1.2.11/+esm";
import { decode } from "./sept_codec.js";

const N_VOICES = 16;
const K = 9;   // v2: + el (elevation)
const MAX_KNOTS = 32;

/**
 * startRenderer(coreCkUrl, opts?) -> { feed(bytes), chuck, ready }
 *
 * `coreCkUrl` — URL (relative or absolute) to `sept_render_core.ck`.
 * `opts.audioContext` — optional pre-existing AudioContext (WebChucK will
 *   create its own if omitted; either way the caller must have already
 *   satisfied the "user gesture" requirement before calling this).
 * `opts.onLog(line)` — optional sink for ChucK `<<< >>>` print lines
 *   (in addition to the default `console.log`).
 */
export async function startRenderer(coreCkUrl, opts = {}) {
  const chuck = await Chuck.init(
    [{ serverFilename: coreCkUrl, virtualFilename: "sept_render_core.ck" }],
    opts.audioContext
  );

  if (opts.onLog) {
    const origPrint = chuck.chuckPrint.bind(chuck);
    chuck.chuckPrint = (msg) => {
      origPrint(msg);
      try {
        opts.onLog(msg);
      } catch (e) {
        console.error("[webchuck_render] onLog handler error", e);
      }
    };
  }

  await chuck.runFile("sept_render_core.ck");

  // --- voice_id -> slot LRU (16 slots), mirroring sept_render_osc.ck ----
  const slotOfVoice = new Map(); // voiceId -> slot
  const voiceOfSlot = new Array(N_VOICES).fill(null); // slot -> voiceId | null
  const lastUsed = new Array(N_VOICES).fill(0); // slot -> monotonic tick, for LRU steal
  let tick = 0;

  function touchSlot(slot) {
    tick += 1;
    lastUsed[slot] = tick;
  }

  function allocSlot(voiceId) {
    const existing = slotOfVoice.get(voiceId);
    if (existing !== undefined) {
      touchSlot(existing);
      return existing;
    }
    // find a free slot first
    let slot = voiceOfSlot.indexOf(null);
    if (slot === -1) {
      // steal least-recently-used slot
      slot = 0;
      let best = lastUsed[0];
      for (let i = 1; i < N_VOICES; i++) {
        if (lastUsed[i] < best) {
          best = lastUsed[i];
          slot = i;
        }
      }
      const stolenVoice = voiceOfSlot[slot];
      if (stolenVoice !== null) slotOfVoice.delete(stolenVoice);
    }
    voiceOfSlot[slot] = voiceId;
    slotOfVoice.set(voiceId, slot);
    touchSlot(slot);
    return slot;
  }

  function slotForRelease(voiceId) {
    // released voices keep their slot (matches native LRU semantics: a
    // slot is only reassigned on the next onset=1 that needs it), but we
    // must know which slot to gate off.
    return slotOfVoice.has(voiceId) ? slotOfVoice.get(voiceId) : null;
  }

  function applyHit(msg) {
    if (msg.voiceId === "") return; // /hit with empty voice_id: not addressable to a slot
    if (msg.onset) {
      const slot = allocSlot(msg.voiceId);
      for (let i = 0; i < K; i++) {
        chuck.setFloat(`v_mu_${slot}_${i}`, msg.mu[i]);
        chuck.setFloat(`v_sig_${slot}_${i}`, msg.sigma[i]);
      }
      chuck.setFloat(`v_vel_${slot}`, msg.velocity);
      chuck.setInt(`v_gate_${slot}`, 1);
      chuck.broadcastEvent(`v_hit_${slot}`);
    } else {
      const slot = slotForRelease(msg.voiceId);
      if (slot === null) return; // unknown release: ignore (conformance obligation C3)
      chuck.setInt(`v_gate_${slot}`, 0);
      chuck.broadcastEvent(`v_hit_${slot}`);
    }
  }

  function applyMove(msg) {
    if (msg.voiceId === "") return; // global move: not realized in v1, matches native renderer
    const slot = slotOfVoice.get(msg.voiceId);
    if (slot === undefined) return; // unknown voice: drop (renderers may drop /move)
    for (let i = 0; i < K; i++) {
      chuck.setFloat(`v_mu_${slot}_${i}`, msg.mu[i]);
    }
  }

  function applyShape(msg) {
    if (msg.layer !== 0) return; // play layer only in v1, matches sept_render_core.ck
    chuck.setFloat("sh_dur", msg.durationS);
    const n = Math.min(msg.knots.length, MAX_KNOTS);
    chuck.setInt("sh_n", n);
    for (let j = 0; j < n; j++) {
      const knot = msg.knots[j];
      chuck.setFloat(`sh_t_${j}`, knot.t);
      for (let i = 0; i < K; i++) {
        chuck.setFloat(`sh_mu_${j}_${i}`, knot.mu[i]);
        chuck.setFloat(`sh_sig_${j}_${i}`, knot.sigma[i]);
      }
    }
  }

  /** feed(bytes: Uint8Array) -> void. Decodes and applies every Septagraph
   * message in the packet (hit/move/shape). manifest and manifestRequest
   * messages are not handled here -- that's `sept_manifest.ck`'s native-only
   * job; the browser renderer has no reply path of its own in v1. */
  function feed(bytes) {
    let msgs;
    try {
      msgs = decode(bytes);
    } catch (e) {
      console.error("[webchuck_render] decode failed", e);
      return;
    }
    for (const msg of msgs) {
      try {
        if (msg.type === "hit") applyHit(msg);
        else if (msg.type === "move") applyMove(msg);
        else if (msg.type === "shape") applyShape(msg);
        // manifest / manifestRequest: ignored by this in-page renderer.
      } catch (e) {
        console.error("[webchuck_render] error applying message", msg, e);
      }
    }
  }

  return { feed, chuck };
}
