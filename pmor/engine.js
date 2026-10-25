// engine.js - main-thread wrapper around the PMOR audio worklet.
//
// Owns the AudioContext, loads pmor-worklet.js, relays the Engine API as
// fire-and-forget messages, forwards ticks, handles iOS unlock/resume, and asks
// for the microphone lazily when a sampler pad hold turns into a recording.
// No audio is computed here.

const REC_HOLD_MS = 300;                      // L1740: hold >= 300 ms = record
const MAX_QUEUE = 4096;

function bytesToB64(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(s);
}
function b64ToBytes(b64) {
  const s = atob(b64), out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
  return out;
}
function pcmToB64(pcm) { return bytesToB64(new Uint8Array(pcm.buffer, pcm.byteOffset, pcm.byteLength)); }
function b64ToPcm(b64) {
  const b = b64ToBytes(b64);
  const pcm = new Int16Array(b.length >> 1);
  new Uint8Array(pcm.buffer).set(b.subarray(0, pcm.length * 2));
  return pcm;
}

export class Engine {
  constructor() {
    this.ready = false;
    this.sampleRate = 0;
    this.onTick = null;
    this.micState = 'none';                   // 'none' | 'asking' | 'ok' | 'denied'
    this.ctx = null;
    this.node = null;
    this._queue = [];
    this._starting = null;
    this._pid = 0;
    this._persistWait = new Map();
    this._recTimers = new Map();
    this._micStream = null;
    this._micSrc = null;
    this._userSuspended = false;
    this._unlockArmed = false;
    this._lastPersist = null;
  }

  // ------------------------------------------------------------- lifecycle
  // Call from a touch/click handler. Resolves when the worklet is running.
  start() {
    if (this._starting) { this.resume(); return this._starting; }
    this._starting = this._start().catch((e) => {          // let the UI retry from the next touch
      this._starting = null; this.ready = false;
      try { if (this.ctx) this.ctx.close(); } catch (e2) { /* ignore */ }
      this.ctx = null; this.node = null;
      throw e;
    });
    return this._starting;
  }

  async _start() {
    const AC = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!AC) throw new Error('Web Audio is not available');
    const ctx = new AC({ latencyHint: 'interactive' });
    this.ctx = ctx;
    this.sampleRate = ctx.sampleRate;
    // iOS only unlocks inside the gesture: resume now, and play one silent sample.
    try { ctx.resume(); } catch (e) { /* retried below and on the next touch */ }
    try {
      const b = ctx.createBuffer(1, 1, ctx.sampleRate), s = ctx.createBufferSource();
      s.buffer = b; s.connect(ctx.destination); s.start(0);
    } catch (e) { /* not fatal */ }
    ctx.onstatechange = () => {
      if (ctx.state !== 'running' && !this._userSuspended) this._armUnlock();   // 'suspended' or iOS 'interrupted'
    };
    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', () => {
        if (!document.hidden && ctx.state !== 'running' && !this._userSuspended) this._armUnlock();
      });
    }

    await ctx.audioWorklet.addModule(new URL('./pmor-worklet.js', import.meta.url));
    const node = new AudioWorkletNode(ctx, 'pmor', {
      numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2],
      channelCount: 1, channelCountMode: 'explicit',
    });
    this.node = node;
    const ready = new Promise((resolve, reject) => {
      const to = setTimeout(() => reject(new Error('audio worklet did not start')), 5000);
      node.port.onmessage = (e) => {
        const m = e.data;
        if (!m) return;
        if (m.t === 'tick') {
          const t = m.d; t.micState = this.micState;
          if (this.onTick) this.onTick(t);
        } else if (m.t === 'ready') {
          clearTimeout(to); resolve();
        } else if (m.t === 'persist') {
          const w = this._persistWait.get(m.id);
          if (w) { this._persistWait.delete(m.id); w(this._persistOut(m.data)); }
        }
      };
    });
    node.connect(ctx.destination);
    if (ctx.state !== 'running') { try { ctx.resume(); } catch (e) { /* see above */ } }
    await ready;
    this.ready = true;
    this.sampleRate = ctx.sampleRate;
    if (this.micState === 'ok') node.port.postMessage({ c: 'mic', ok: true });
    const q = this._queue; this._queue = [];
    for (const m of q) this._send(m);
    if (ctx.state !== 'running') this._armUnlock();
  }

  suspend() {
    this._userSuspended = true;
    if (this.ctx && this.ctx.state === 'running') { try { this.ctx.suspend(); } catch (e) { /* ignore */ } }
  }
  resume() {
    this._userSuspended = false;
    if (this.ctx && this.ctx.state !== 'running') {
      try { const p = this.ctx.resume(); if (p && p.catch) p.catch(() => this._armUnlock()); } catch (e) { this._armUnlock(); }
    }
  }

  // iOS can suspend or interrupt the context at any time; only a touch brings it back.
  _armUnlock() {
    if (this._unlockArmed || typeof window === 'undefined') return;
    this._unlockArmed = true;
    const evs = ['touchend', 'pointerup', 'click', 'keydown'];
    const go = () => {
      const ctx = this.ctx;
      if (!ctx || this._userSuspended) return;
      let p;
      try { p = ctx.resume(); } catch (e) { return; }
      const done = () => {
        if (ctx.state === 'running') {
          for (const ev of evs) window.removeEventListener(ev, go, true);
          this._unlockArmed = false;
        }
      };
      if (p && p.then) p.then(done, () => {}); else done();
    };
    for (const ev of evs) window.addEventListener(ev, go, { capture: true, passive: true });
  }

  // ------------------------------------------------------------- messaging
  _send(m) { if (this.node) this.node.port.postMessage(m); }
  _post(m) {
    if (this.ready) { this.node.port.postMessage(m); return; }
    if (m.c === 'on' || m.c === 'drum' || m.c === 'sPress') return;    // gestures never replay late
    if (this._queue.length < MAX_QUEUE) this._queue.push(m);
  }

  // pads (k = 4*row + col, row 0 top)
  noteOn(k) { this._post({ c: 'on', k }); }
  noteOff(k) { this._post({ c: 'off', k }); }
  drumHit(k) { this._post({ c: 'drum', k }); }

  samplerPress(k) {
    this._post({ c: 'sPress', k });
    clearTimeout(this._recTimers.get(k));
    // a hold of >= 300 ms becomes a recording: only then ask for the mic
    this._recTimers.set(k, setTimeout(() => { this._recTimers.delete(k); this._ensureMic(); }, REC_HOLD_MS));
  }
  samplerRelease(k) {
    const t = this._recTimers.get(k);
    if (t !== undefined) { clearTimeout(t); this._recTimers.delete(k); }
    this._post({ c: 'sRel', k });
  }

  // FX
  fxBank(n) { this._post({ c: 'fxBank', n }); }
  fxHeld(bank, mask16) { this._post({ c: 'fxHeld', bank, mask: mask16 }); }
  fxRecord(on) { this._post({ c: 'fxRec', on: !!on }); }
  fxClearAuto() { this._post({ c: 'fxClr' }); }

  // synth sequencer
  synMode(m) { this._post({ c: 'synMode', m }); }
  synArm(mask16) { this._post({ c: 'arm', mask: mask16 }); }
  synPlace(step) { this._post({ c: 'place', step }); }
  synLiveWrite(on) { this._post({ c: 'liveW', on: !!on }); }
  synClear() { this._post({ c: 'synClr' }); }

  // drum / sampler sequencer: engineId 1 drums, 2 sampler
  toggleStep(engineId, voiceOrSlot, step) { this._post({ c: 'tog', eng: engineId, idx: voiceOrSlot, step }); }
  clearPattern(engineId, voiceOrSlot) { this._post({ c: 'clr', eng: engineId, idx: voiceOrSlot }); }
  selectDrum(v) { this._post({ c: 'selD', v }); }
  selectSlot(s) { this._post({ c: 'selS', s }); }

  // parameters
  set(name, value) { this._post({ c: 'set', name, value }); }
  adjust(name, dir) { this._post({ c: 'adj', name, dir }); }
  tap() { this._post({ c: 'tap', ms: (typeof performance !== 'undefined' ? performance.now() : Date.now()) }); }
  allKeysOff() { this._post({ c: 'keysOff' }); }

  // ------------------------------------------------------------- patches
  // The engine's part of a patch (SPEC 8): JSON-safe, sampler audio as base64 Int16 per non-empty slot.
  getPersist() {
    if (!this.ready) return Promise.resolve(this._lastPersist ? JSON.parse(JSON.stringify(this._lastPersist)) : this.defaults());
    const id = ++this._pid;
    return new Promise((resolve) => {
      this._persistWait.set(id, resolve);
      this.node.port.postMessage({ c: 'getP', id });
    });
  }
  _persistOut(d) {
    const audio = {};
    for (const k of Object.keys(d.audio || {})) {
      const a = d.audio[k];
      audio[k] = { sr: a.sr, n: a.n, b64: pcmToB64(a.pcm) };
    }
    d.audio = audio;
    this._lastPersist = d;
    return d;
  }
  setPersist(obj) {
    const d = Object.assign({}, obj), audio = {}, tr = [];
    for (const k of Object.keys(obj.audio || {})) {
      const a = obj.audio[k];
      let pcm = null;
      if (a && a.pcm instanceof Int16Array) pcm = a.pcm.slice();
      else if (a && typeof a.b64 === 'string') { try { pcm = b64ToPcm(a.b64); } catch (e) { pcm = null; } }
      if (!pcm || !pcm.length) continue;
      audio[k] = { sr: a.sr, n: pcm.length, pcm }; tr.push(pcm.buffer);
    }
    d.audio = audio;
    this._lastPersist = obj;
    if (this.ready) this.node.port.postMessage({ c: 'setP', data: d }, tr);
    else {
      this._queue = this._queue.filter((m) => m.c !== 'setP');
      this._queue.push({ c: 'setP', data: d });
    }
  }
  // boot defaults, SPEC 8.2 (PRISE included: it persists here, unlike the firmware)
  defaults() {
    const z = (n) => new Array(n).fill(0);
    return {
      v: 1, step_ms: 125, key: 0, mode: 3, swing: 0, wave: 5, kit: 0, dens: 6, poly: 5, cdrum: 9, csmp: 9,
      vsyn: 10, vdrum: 10, vsmp: 10, wtscan: 0, prise: 0, bars: 1, beats: 16, syn_mode: 0, sel_drum: 0,
      sel_smp: 0, fx_bank: 0, dr_pad: [13, 10, 4, 0],
      dr_pat: [z(16), z(16), z(16), z(16)],
      smp_pat: Array.from({ length: 16 }, () => z(16)),
      syn_seq: z(128),
      fx_auto: Array.from({ length: 8 }, () => z(16)),
      fx_auto_fsr: z(16),
      audio: {},
    };
  }

  // ------------------------------------------------------------- microphone
  // Lazy: first sampler hold that reaches the 300 ms record threshold. The stream only
  // feeds the worklet input; it is never connected to the output (SPEC 2.1).
  async _ensureMic() {
    if (this.micState === 'ok' || this.micState === 'asking') return;      // a denied mic is asked again on the next hold
    if (!this.ctx || !this.node) return;
    const md = typeof navigator !== 'undefined' ? navigator.mediaDevices : null;
    if (!md || !md.getUserMedia) { this.micState = 'denied'; return; }
    this.micState = 'asking';
    try {
      const stream = await md.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
      const src = this.ctx.createMediaStreamSource(stream);
      src.connect(this.node);
      this._micStream = stream; this._micSrc = src;
      const tr = stream.getAudioTracks()[0];
      if (tr) tr.onended = () => {
        this.micState = 'none'; this._micStream = null;
        try { src.disconnect(); } catch (e) { /* already gone */ }
        this._post({ c: 'mic', ok: false });
      };
      this.micState = 'ok';
      this._post({ c: 'mic', ok: true });
    } catch (e) {
      this.micState = 'denied';
    }
  }
}
