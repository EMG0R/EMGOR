// Node tests for the PMOR engine. Usage: node pmor/test/engine.test.mjs [sampleRate ...]
// With no argument it runs at 48000 and 44100. A tiny shim stands in for the AudioWorklet scope.
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
let Proc = null;
globalThis.sampleRate = 48000;
globalThis.currentFrame = 0;
globalThis.AudioWorkletProcessor = class {
  constructor() { this.port = { onmessage: null, posted: [], postMessage(m) { this.posted.push(m); } }; }
};
globalThis.registerProcessor = (name, cls) => { if (name === 'pmor') Proc = cls; };
const emitWarning = process.emitWarning;                      // node notes engine.js has no package "type"; browsers do not care
process.emitWarning = (w, ...a) => { if (!/MODULE_TYPELESS/.test(String((a[0] && a[0].code) || a[1] || ''))) emitWarning.call(process, w, ...a); };
await import(join(here, '..', 'pmor-worklet.js'));
if (!Proc) throw new Error('worklet did not register "pmor"');
const { Engine } = await import(join(here, '..', 'engine.js'));

let fails = 0, total = 0;
function check(ok, name, detail = '') {
  total++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail ? ' - ' + detail : ''}`);
  if (!ok) fails++;
}

class Rig {
  constructor(sr) {
    this.sr = sr; this.frame = 0;
    globalThis.sampleRate = sr; globalThis.currentFrame = 0;
    this.p = new Proc();
    this.q = 128;
    this.L = new Float32Array(this.q); this.R = new Float32Array(this.q);
    this.inp = new Float32Array(this.q);
    this.lastTick = null; this.ticks = 0; this.ready = false;
    this.defaults = new Engine().defaults();
  }
  send(m) { globalThis.currentFrame = this.frame; this.p.port.onmessage({ data: m }); }
  // render seconds; returns {L, R, peak, rms, bad}; opts.input(i) gives mic sample at absolute frame i
  run(sec, opts = {}) {
    const n = Math.round(sec * this.sr), L = new Float32Array(n), R = new Float32Array(n);
    let peak = 0, bad = 0, ss = 0, done = 0;
    while (done < n) {
      globalThis.currentFrame = this.frame; globalThis.sampleRate = this.sr;
      if (opts.input) for (let i = 0; i < this.q; i++) this.inp[i] = opts.input(this.frame + i);
      else this.inp.fill(0);
      this.p.process([[this.inp]], [[this.L, this.R]]);
      for (let i = 0; i < this.q && done < n; i++, done++) {
        const l = this.L[i], r = this.R[i];
        if (!Number.isFinite(l) || !Number.isFinite(r)) bad++;
        const a = Math.max(Math.abs(l), Math.abs(r)); if (a > peak) peak = a;
        ss += l * l + r * r; L[done] = l; R[done] = r;
      }
      this.frame += this.q;
      this.drain();
    }
    return { L, R, peak, rms: Math.sqrt(ss / (2 * n)), bad };
  }
  drain() {
    const q = this.p.port.posted; this.p.port.posted = [];
    for (const m of q) {
      if (m.t === 'tick') { this.lastTick = m.d; this.ticks++; if (this.onTick) this.onTick(m.d); }
      else if (m.t === 'ready') this.ready = true;
      else if (m.t === 'persist' && this.persistWait && this.persistWait.id === m.id) this.persistWait.data = m.data;
      else if (m.t === 'error') throw new Error('worklet error: ' + m.msg);
    }
  }
  persist() {
    this.persistWait = { id: 7, data: null };
    this.send({ c: 'getP', id: 7 }); this.drain();
    return this.persistWait.data;
  }
}
function rmsOf(a, i0, i1) { let s = 0; for (let i = i0; i < i1; i++) s += a[i] * a[i]; return Math.sqrt(s / Math.max(1, i1 - i0)); }
function stepCount(rig, sec) {
  let steps = 0, last = rig.lastTick ? rig.lastTick.gstep : -1;
  rig.onTick = (t) => { if (t.gstep !== last) { steps++; last = t.gstep; } };
  rig.run(sec); rig.onTick = null; return steps;
}
function setupBeat(rig) {                                           // kick 4-on-floor, snare, hats
  for (const s of [0, 4, 8, 12]) rig.send({ c: 'tog', eng: 1, idx: 0, step: s });
  for (const s of [4, 12]) rig.send({ c: 'tog', eng: 1, idx: 2, step: s });
  for (let s = 0; s < 16; s += 2) rig.send({ c: 'tog', eng: 1, idx: 3, step: s });
}
function freqEstimate(x, sr, i0, i1) {                               // positive-going zero crossings
  let first = -1, last = -1, n = 0;
  for (let i = i0 + 1; i < i1; i++) if (x[i - 1] <= 0 && x[i] > 0) { const f = i - x[i] / (x[i] - x[i - 1]); if (first < 0) first = f; last = f; n++; }
  return n > 1 ? (n - 1) * sr / (last - first) : 0;
}

function suite(sr) {
  console.log(`\n== sampleRate ${sr} ==`);
  let rig, r;

  // boot
  rig = new Rig(sr);
  r = rig.run(1);
  check(rig.ready && r.bad === 0 && r.peak === 0, 'boot is silent and finite', `peak ${r.peak}`);
  const tk = rig.lastTick;
  check(tk && tk.songLen === 16 && tk.bpm === 120 && tk.params.mvol_idx === 9 && tk.params.wave === 5 && tk.sampleRate === sr,
    'tick shape and boot defaults', `bpm ${tk && tk.bpm}`);
  check(tk.syn_seq_page instanceof Uint16Array && tk.smp_len instanceof Float32Array && tk.dr_page.length === 4 && tk.sm_counts.length === 16 && tk.fx.eff.length === 8,
    'tick field types');

  // sequencer clock
  rig = new Rig(sr);
  let first = null; rig.onTick = (t) => { if (first === null && t.gstep !== 0) first = t.gstep; };
  rig.run(0.5); rig.onTick = null;
  check(first === 1, 'first fired step is 1 (SPEC 0.14)', `first ${first}`);
  let n = stepCount(rig, 10);
  check(Math.abs(n - 80) <= 1, '16 steps per 2.0 s at step_ms 125', `${n} steps in 10 s (want 80)`);
  rig.send({ c: 'set', name: 'swing', value: 9 });
  n = stepCount(rig, 10);
  check(Math.abs(n - 80) <= 1, 'swing 9 keeps the average tempo', `${n}`);
  rig.send({ c: 'set', name: 'step_ms', value: 250 });
  n = stepCount(rig, 10);
  check(Math.abs(n - 40) <= 1, 'step_ms 250 = 40 steps in 10 s', `${n}`);
  rig.send({ c: 'set', name: 'beats', value: 7 }); rig.send({ c: 'set', name: 'swing', value: 0 });
  rig.run(0.6);
  const seen = new Set(); rig.onTick = (t) => seen.add(t.gstep); rig.run(5); rig.onTick = null;
  check(Math.max(...seen) === 6 && seen.size === 7, 'odd song length wraps at 7', [...seen].join(','));

  // tap tempo
  rig = new Rig(sr);
  for (let i = 0; i < 4; i++) rig.send({ c: 'tap', ms: 5000 + i * 500 });
  rig.run(0.15);
  check(rig.lastTick.step_ms === 125, 'tap tempo: 500 ms taps -> step_ms 125', `${rig.lastTick.step_ms}`);
  rig.send({ c: 'tap', ms: 20000 }); rig.send({ c: 'tap', ms: 20400 });
  rig.run(0.15);
  check(rig.lastTick.step_ms === 100, 'tap tempo: window resets after a long gap', `${rig.lastTick.step_ms}`);

  // synth: energy in the first block, release decays, pitch is right at both rates
  rig = new Rig(sr);
  rig.send({ c: 'set', name: 'wave', value: 3 });
  rig.send({ c: 'on', k: 12 });
  r = rig.run(128 / sr);
  check(r.peak > 1e-4, 'noteOn produces energy within one block', `peak ${r.peak.toExponential(2)}`);
  r = rig.run(1.0);
  const f0 = freqEstimate(r.L, sr, Math.round(sr * 0.5), r.L.length);
  check(Math.abs(f0 - 87.31) < 1.5, 'bottom-left pad is F2 (87.3 Hz) with SINE', `${f0.toFixed(2)} Hz`);
  rig.send({ c: 'off', k: 12 });
  r = rig.run(1.5);
  check(rmsOf(r.L, r.L.length - Math.round(sr * 0.1), r.L.length) < 2e-4, 'noteOff decays', `tail rms ${rmsOf(r.L, r.L.length - Math.round(sr * 0.1), r.L.length).toExponential(2)}`);
  rig.send({ c: 'set', name: 'key', value: 12 }); rig.send({ c: 'on', k: 12 });
  r = rig.run(1.5);
  const f1 = freqEstimate(r.L, sr, Math.round(sr * 0.7), r.L.length);
  check(Math.abs(f1 - 174.62) < 3, 'KEY +12 raises the note an octave', `${f1.toFixed(2)} Hz`);
  rig.send({ c: 'off', k: 12 });
  // left column sounds left
  rig = new Rig(sr);
  rig.send({ c: 'set', name: 'wave', value: 3 }); rig.send({ c: 'on', k: 12 });
  r = rig.run(0.4);
  check(rmsOf(r.L, 0, r.L.length) > 2 * rmsOf(r.R, 0, r.R.length), 'left pad column sounds on the left', `L ${rmsOf(r.L, 0, r.L.length).toExponential(2)} R ${rmsOf(r.R, 0, r.R.length).toExponential(2)}`);
  rig.send({ c: 'off', k: 12 }); rig.send({ c: 'on', k: 15 });
  r = rig.run(0.4);
  check(rmsOf(r.R, 0, r.R.length) > 0.5 * rmsOf(r.L, 0, r.L.length), 'right pad column leans right', '');

  // every waveform sounds and stays finite
  let allWaves = true;
  for (let w = 0; w < 13; w++) {
    rig = new Rig(sr); rig.send({ c: 'set', name: 'wave', value: w }); rig.send({ c: 'set', name: 'wtscan', value: w % 16 });
    rig.send({ c: 'on', k: 13 }); rig.send({ c: 'on', k: 6 });
    r = rig.run(0.6);
    if (r.bad || r.peak < 1e-4 || r.peak > 1) allWaves = false;
  }
  check(allWaves, 'all 13 waveforms play, finite, peak <= 1');

  // drums: both kits, every pad
  for (const kit of [0, 1]) {
    let ok = true, weak = [];
    for (let k = 0; k < 16; k++) {
      rig = new Rig(sr); rig.send({ c: 'set', name: 'kit', value: kit });
      rig.send({ c: 'drum', k });
      r = rig.run(0.5);
      if (r.bad || r.peak < 1e-3) { ok = false; weak.push(k); }
    }
    check(ok, `kit ${kit}: all 16 drum pads hit`, weak.length ? 'silent pads ' + weak : '');
  }
  rig = new Rig(sr); rig.send({ c: 'drum', k: 13 }); rig.send({ c: 'drum', k: 3 });
  rig.run(0.01);
  check(rig.persist().dr_pad[0] === 13 && rig.persist().dr_pad[3] === 3 && rig.persist().sel_drum === 3, 'drumHit sets dr_pad and sel_drum');
  rig.send({ c: 'drum', k: 4 });
  check(rig.persist().dr_pad[2] === 4 && rig.persist().sel_drum === 2, 'drumHit row mapping 3-(k>>2)');

  // sequenced drums and chance
  rig = new Rig(sr); setupBeat(rig);
  r = rig.run(4);
  check(r.peak > 0.01 && r.bad === 0, 'drum pattern plays', `peak ${r.peak.toFixed(3)} rms ${r.rms.toFixed(4)}`);
  rig.send({ c: 'set', name: 'cdrum', value: 0 }); rig.run(3.5);
  r = rig.run(2);
  check(r.peak < 1e-4, 'cdrum 0 silences sequenced drums', `peak ${r.peak.toExponential(2)}`);
  check(rig.lastTick.dr_counts.join() === '4,0,2,8' && rig.lastTick.dr_page[0] === 0x1111, 'drum counts and page masks', rig.lastTick.dr_counts.join());
  rig.send({ c: 'toggleDummy' }); rig.send({ c: 'clr', eng: 1, idx: 0 }); rig.run(0.2);
  check(rig.lastTick.dr_counts[0] === 0, 'clearPattern');
  rig.send({ c: 'tog', eng: 1, idx: 1, step: 200 }); rig.run(0.1);
  check(rig.lastTick.dr_counts[1] === 0, 'toggleStep beyond song length is ignored');

  // synth sequencer, ARP, GEN
  rig = new Rig(sr);
  rig.send({ c: 'arm', mask: (1 << 12) | (1 << 14) }); rig.send({ c: 'place', step: 0 });
  rig.send({ c: 'arm', mask: 1 << 13 }); rig.send({ c: 'place', step: 4 });
  rig.run(0.1);
  check(rig.lastTick.syn_seq_page[0] === ((1 << 12) | (1 << 14)) && rig.lastTick.syn_seq_page[4] === (1 << 13) && rig.lastTick.syn_arm === 0, 'synPlace writes the armed chord and clears the arm');
  r = rig.run(3);
  check(r.peak > 1e-3 && r.bad === 0, 'synth SEQ plays the placed steps', `peak ${r.peak.toFixed(3)}`);
  rig.send({ c: 'synMode', m: 1 }); r = rig.run(2);
  check(r.peak > 1e-3, 'ARP plays from the used pads');
  rig.send({ c: 'synMode', m: 2 }); r = rig.run(3);
  check(r.peak > 1e-3 && r.bad === 0, 'GEN plays');
  rig.send({ c: 'set', name: 'dens', value: 0 }); rig.run(1.5);
  r = rig.run(2);
  check(r.peak < 5e-3, 'dens 0 silences GEN (tails gone)', `peak ${r.peak.toExponential(2)}`);
  rig.send({ c: 'synMode', m: 0 }); rig.send({ c: 'synClr' }); rig.run(0.1);
  check(rig.lastTick.syn_seq_page[0] === 0, 'synClear');
  // live write
  rig.send({ c: 'liveW', on: true }); rig.send({ c: 'on', k: 12 });
  rig.run(1.0); rig.send({ c: 'off', k: 12 }); rig.send({ c: 'liveW', on: false });
  let any = false; { const st = rig.persist(); any = st.syn_seq.some((x) => x === (1 << 12)); }
  check(any, 'synLiveWrite records the held pad into steps');

  // FX automation record and replay
  rig = new Rig(sr); setupBeat(rig);
  rig.send({ c: 'fxBank', n: 0 }); rig.send({ c: 'fxHeld', bank: 0, mask: 1 << 11 }); rig.send({ c: 'fxRec', on: true });
  rig.run(2.2); rig.send({ c: 'fxRec', on: false }); rig.send({ c: 'fxHeld', bank: 0, mask: 0 });
  const auto = rig.persist().fx_auto[0];
  check(auto.filter((x) => x === (1 << 11)).length >= 15, 'FX automation records the held mask per step', `${auto.filter((x) => x === (1 << 11)).length}/16 steps`);
  r = rig.run(1.0);
  check(r.peak < 1e-3 && rig.lastTick.fx.eff[0] === (1 << 11), 'automation replays (MUTE) with nothing held', `peak ${r.peak.toExponential(2)}`);
  rig.send({ c: 'fxClr' }); rig.run(0.3);
  check(rig.persist().fx_auto[0].every((x) => x === 0), 'fxClearAuto');

  // sampler: record from the mic, play back, persist
  rig = new Rig(sr);
  const sine = (i) => 0.5 * Math.sin(2 * Math.PI * 440 * i / sr);
  rig.send({ c: 'sPress', k: 5 }); rig.run(0.2, { input: sine });
  check(rig.lastTick.rec_slot === -1, 'no recording before the mic is available');
  rig.send({ c: 'mic', ok: true });
  rig.run(0.6, { input: sine });
  check(rig.lastTick.rec_slot === 5, 'hold >= 300 ms arms recording once the mic is live', `rec_slot ${rig.lastTick.rec_slot}`);
  rig.send({ c: 'sRel', k: 5 }); rig.run(0.1);
  const len = rig.lastTick.smp_len[5];
  check(len > 0.3 && len < 1.2 && rig.lastTick.rec_slot === -1, 'release ends the recording', `${len.toFixed(3)} s`);
  rig.send({ c: 'sPress', k: 5 }); rig.run(0.1); rig.send({ c: 'sRel', k: 5 });
  r = rig.run(0.3);
  check(r.peak > 0.05, 'short tap triggers the slot', `peak ${r.peak.toFixed(3)}`);
  rig.send({ c: 'tog', eng: 2, idx: 5, step: 0 }); rig.run(0.1);
  check(rig.lastTick.sm_counts[5] === 1, 'sampler pattern step toggles');
  r = rig.run(2.5);
  check(r.rms > 0.01, 'sampler sequencer retriggers the slot', `rms ${r.rms.toFixed(4)}`);
  // long record caps at 2 s
  rig = new Rig(sr); rig.send({ c: 'mic', ok: true }); rig.send({ c: 'sPress', k: 0 });
  rig.run(2.6, { input: sine }); rig.send({ c: 'sRel', k: 0 }); rig.run(0.1);
  check(Math.abs(rig.lastTick.smp_len[0] - 2.0) < 0.01, 'recording stops at 2 s', `${rig.lastTick.smp_len[0]}`);

  // persist round trip (including sampler audio) and defaults agreement
  rig = new Rig(sr); setupBeat(rig);
  rig.send({ c: 'mic', ok: true }); rig.send({ c: 'sPress', k: 3 }); rig.run(0.8, { input: sine }); rig.send({ c: 'sRel', k: 3 });
  rig.send({ c: 'set', name: 'wave', value: 10 }); rig.send({ c: 'set', name: 'prise', value: 4 });
  rig.send({ c: 'set', name: 'key', value: -5 }); rig.send({ c: 'set', name: 'mode', value: 7 });
  rig.send({ c: 'set', name: 'bars', value: 3 }); rig.send({ c: 'adj', name: 'bpm', dir: 1 });
  rig.send({ c: 'arm', mask: 0x00f0 }); rig.send({ c: 'place', step: 9 });
  rig.send({ c: 'fxBank', n: 5 });
  rig.run(0.2);
  const A = rig.persist();
  const rig2 = new Rig(sr);
  rig2.send({ c: 'setP', data: A }); rig2.run(0.15);
  const B2 = rig2.persist();
  const same = JSON.stringify(A, (k, v) => (v instanceof Int16Array ? Array.from(v) : v)) === JSON.stringify(B2, (k, v) => (v instanceof Int16Array ? Array.from(v) : v));
  check(same && A.prise === 4 && A.audio[3] && A.audio[3].n > sr * 0.3, 'getPersist -> setPersist round-trips (PRISE and audio included)', `audio ${A.audio[3] && A.audio[3].n} samples`);
  check(rig2.lastTick.gstep <= 1 && rig2.lastTick.params.bars === 3, 'setPersist restarts the playhead and restores params');
  const rig3 = new Rig(sr);
  rig3.send({ c: 'setP', data: rig3.defaults }); rig3.run(0.1);
  const D = rig3.persist(); delete D.audio; const D0 = Object.assign({}, rig3.defaults); delete D0.audio;
  check(JSON.stringify(D) === JSON.stringify(D0), 'Engine.defaults() equals the boot state');
  if (sr !== 48000) {                                          // audio recorded at another rate is resampled
    const Af = JSON.parse(JSON.stringify(A, (k, v) => (v instanceof Int16Array ? Array.from(v) : v)));
    Af.audio[3].pcm = Int16Array.from(Af.audio[3].pcm);
    Af.audio[3].sr = 48000; Af.audio[3].n = Math.round(Af.audio[3].n * 48000 / sr);
    const src = Af.audio[3].pcm, out = new Int16Array(Af.audio[3].n);
    for (let i = 0; i < out.length; i++) out[i] = src[Math.min(src.length - 1, Math.round(i * sr / 48000))];
    Af.audio[3].pcm = out;
    const rig4 = new Rig(sr); rig4.send({ c: 'setP', data: Af }); rig4.run(0.1);
    const L4 = rig4.lastTick.smp_len[3];
    check(Math.abs(L4 - Af.audio[3].n / 48000) < 0.01, 'audio saved at 48 kHz is resampled to the context rate', `${L4.toFixed(3)} s`);
  }

  // parameter clamping and adjust steps
  rig = new Rig(sr);
  rig.send({ c: 'set', name: 'step_ms', value: 10 }); rig.send({ c: 'set', name: 'key', value: 99 });
  rig.send({ c: 'set', name: 'dens', value: -4 }); rig.send({ c: 'set', name: 'mode', value: 20 });
  rig.send({ c: 'adj', name: 'wave', dir: -1 }); rig.send({ c: 'adj', name: 'bars', dir: 1 });
  rig.send({ c: 'adj', name: 'kit', dir: 1 }); rig.send({ c: 'set', name: 'mvol_idx', value: 12 });
  rig.run(0.1);
  const pp = rig.lastTick.params;
  check(rig.lastTick.step_ms === 60 && pp.key === 24 && pp.dens === 0 && pp.mode === 8 && pp.wave === 4 && pp.bars === 2 && pp.kit === 1 && pp.mvol_idx === 12,
    'set clamps, adjust steps and wraps', JSON.stringify(pp));
  rig.send({ c: 'set', name: 'step_ms', value: 125 }); rig.send({ c: 'adj', name: 'bpm', dir: 1 }); rig.run(0.1);
  check(rig.lastTick.step_ms === 123 && rig.lastTick.bpm === 121, 'BPM +1 from 120 gives step_ms 123 (SPEC 1)', `${rig.lastTick.step_ms}`);

  // tilt filter: a boot-time ax of 0 must not move the 0.62 filter; a changed ax sweeps it
  const brightness = (ax) => {
    const g = new Rig(sr); g.send({ c: 'set', name: 'wave', value: 0 }); g.send({ c: 'set', name: 'mvol_idx', value: 12 });
    if (ax !== null) g.send({ c: 'set', name: 'ax', value: ax });
    g.send({ c: 'on', k: 12 }); g.send({ c: 'on', k: 15 });
    const x = g.run(1.5).L; let d = 0, h = 0; for (let i = Math.round(sr * 0.8) + 1; i < x.length; i++) { const v = x[i] - x[i - 1]; d += v * v; h += x[i] * x[i]; }
    return d / (h + 1e-12);
  };
  const bNone = brightness(null), bZero = brightness(0), bOpen = brightness(-0.5), bShut = brightness(1.0);
  check(Math.abs(bNone - bZero) < 1e-9 * (1 + bNone) && bOpen > bShut * 1.2, 'ax tilt sweeps the master filter; unchanged ax leaves it', `open ${bOpen.toExponential(2)} shut ${bShut.toExponential(2)}`);

  // timing consistency across rates: drum decay and an echo delay time
  rig = new Rig(sr); rig.send({ c: 'drum', k: 13 }); r = rig.run(1.2);
  let hit = 0; for (let i = 0; i < r.L.length; i++) if (Math.abs(r.L[i]) > 0.5 * r.peak) { hit = i; break; }
  let t20 = 0; { const w = Math.round(sr * 0.01); const base = rmsOf(r.L, hit, hit + w);
    for (let i = hit; i + w < r.L.length; i += w) if (rmsOf(r.L, i, i + w) < base * 0.1) { t20 = (i - hit) / sr; break; } }
  check(t20 > 0.25 && t20 < 0.85, 'kick decay time is rate independent', `20 dB down after ${t20.toFixed(2)} s`);
  rig = new Rig(sr); rig.send({ c: 'drum', k: 0 }); rig.send({ c: 'fxBank', n: 5 }); rig.send({ c: 'fxHeld', bank: 5, mask: 1 });
  r = rig.run(0.6);
  const e = (a, b) => rmsOf(r.L, Math.round(sr * a), Math.round(sr * b));
  check(e(0.25, 0.30) > 3 * e(0.15, 0.20) && e(0.25, 0.30) > 1e-3, 'FX6 DELAY echoes at step_ms*2 = 250 ms', `250 ms win ${e(0.25, 0.3).toExponential(2)} vs 150 ms win ${e(0.15, 0.2).toExponential(2)}`);

  // master volume table, mute, hard clip
  rig = new Rig(sr); rig.send({ c: 'set', name: 'wave', value: 0 }); rig.send({ c: 'on', k: 12 });
  rig.send({ c: 'set', name: 'mvol_idx', value: 0 }); r = rig.run(0.3);
  check(r.peak === 0, 'master volume pad 0 is silence');
  rig.send({ c: 'set', name: 'mvol_idx', value: 15 }); rig.send({ c: 'on', k: 13 }); rig.send({ c: 'on', k: 14 }); rig.send({ c: 'on', k: 15 });
  rig.send({ c: 'set', name: 'vsyn', value: 12 }); r = rig.run(0.5);
  check(r.peak <= 1 && r.bad === 0, 'master chain hard-clips at +-1', `peak ${r.peak}`);

  // every FX pad of every bank held for 2 s over a dense background
  const heavy = (rg) => {
    setupBeat(rg);
    for (const k of [1, 6, 10, 12]) rg.send({ c: 'on', k });
    rg.send({ c: 'arm', mask: 0x1010 }); rg.send({ c: 'place', step: 2 });
    rg.send({ c: 'mic', ok: true }); rg.send({ c: 'sPress', k: 1 });
  };
  rig = new Rig(sr); heavy(rig); rig.run(0.5, { input: sine }); rig.send({ c: 'sRel', k: 1 });
  rig.send({ c: 'tog', eng: 2, idx: 1, step: 3 }); rig.send({ c: 'tog', eng: 2, idx: 1, step: 11 });
  rig.run(1);
  let worst = 0, bad = 0, loud = [], t0 = Date.now();
  for (let bank = 0; bank < 8; bank++) {
    rig.send({ c: 'fxBank', n: bank });
    for (let k = 0; k < 16; k++) {
      rig.send({ c: 'fxHeld', bank, mask: 1 << k });
      r = rig.run(2.0);
      rig.send({ c: 'fxHeld', bank, mask: 0 });
      const rr = rig.run(0.4);
      bad += r.bad + rr.bad; worst = Math.max(worst, r.peak);
      if (r.peak > 1 || !(r.rms < 0.95)) loud.push(`${bank}:${k}`);
    }
  }
  check(bad === 0 && loud.length === 0, '128 FX pads each held 2 s: finite, <= 1.0, rms < 0.95', `${((Date.now() - t0) / 1000).toFixed(1)} s wall, worst peak ${worst.toFixed(3)} ${loud.join(' ')}`);
  r = rig.run(2);
  check(r.rms > 0.005 && r.bad === 0, 'engine still healthy after the FX sweep', `rms ${r.rms.toFixed(4)}`);

  // HALF / DBL / STOP / STUT / BZZ / WASH / REVERSE: master clock unbent, audio recovers
  for (const [name, pad] of [['HALF', 8], ['DBL', 9], ['STOP', 15], ['STUT16', 2], ['STUT32', 3], ['BZZ', 10], ['WASH', 13], ['REV', 4]]) {
    rig = new Rig(sr); setupBeat(rig); for (const k of [12, 13]) rig.send({ c: 'on', k });
    rig.run(1);
    const before = rig.run(1).rms;
    rig.send({ c: 'fxHeld', bank: 0, mask: 1 << pad });
    const st = stepCount(rig, 2);
    rig.send({ c: 'fxHeld', bank: 0, mask: 0 });
    rig.run(1.5);
    const after = rig.run(1);
    const st2 = stepCount(rig, 2);
    check(Math.abs(st - 16) <= 1 && Math.abs(st2 - 16) <= 1 && after.bad === 0 && after.rms > 0.3 * before && after.rms < 3 * before + 0.01,
      `${name} held 2 s: clock steady (${st} steps), recovers`, `rms before ${before.toFixed(3)} after ${after.rms.toFixed(3)} steps after ${st2}`);
  }
  // HALF really plays at half rate: the shadow position advances every other master step
  rig = new Rig(sr); for (const s of [0, 1, 2, 3, 4, 5, 6, 7]) rig.send({ c: 'tog', eng: 1, idx: 0, step: s });
  rig.run(0.5);
  const kicksNormal = (() => { const x = rig.run(2); let n = 0, in_ = false; const w = Math.round(sr * 0.002);
    for (let i = 0; i < x.L.length; i += w) { const v = rmsOf(x.L, i, i + w) > 0.05; if (v && !in_) n++; in_ = v; } return n; })();
  check(kicksNormal >= 6, 'sanity: 8 kicks per 16 steps register', `${kicksNormal} onsets in 2 s`);

  // soak: 30 s of everything
  rig = new Rig(sr); heavy(rig); rig.send({ c: 'synMode', m: 2 });
  rig.run(0.5, { input: sine }); rig.send({ c: 'sRel', k: 1 }); rig.send({ c: 'tog', eng: 2, idx: 1, step: 5 });
  let seed = 12345; const rnd = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 4294967296;
  let sbad = 0, speak = 0, ssum = 0, sn = 0, t1 = Date.now();
  for (let t = 0; t < 120; t++) {                              // 120 x 250 ms
    if (t % 8 === 0) rig.send({ c: 'fxBank', n: Math.floor(rnd() * 8) });
    const bank = Math.floor(rnd() * 8);
    rig.send({ c: 'fxHeld', bank, mask: (rnd() < 0.3) ? 0 : (1 << Math.floor(rnd() * 16)) | (rnd() < 0.2 ? 1 << Math.floor(rnd() * 16) : 0) });
    if (t % 5 === 0) rig.send({ c: 'drum', k: Math.floor(rnd() * 16) });
    if (t % 7 === 0) rig.send({ c: 'set', name: 'fsr', value: Math.floor(rnd() * 4095) });
    if (t % 9 === 0) rig.send({ c: 'set', name: 'lid', value: Math.floor(rnd() * 700) });
    const x = rig.run(0.25); sbad += x.bad; speak = Math.max(speak, x.peak); ssum += x.rms * x.rms; sn++;
  }
  const srms = Math.sqrt(ssum / sn);
  check(sbad === 0 && speak <= 1 && srms > 0.005 && srms < 0.9, '30 s soak with random FX, drums, GEN, sampler', `${((Date.now() - t1) / 1000).toFixed(1)} s wall, peak ${speak.toFixed(3)}, rms ${srms.toFixed(3)}`);
}


// ---------------------------------------------------------------- Engine wrapper against fakes
async function wrapperSuite() {
  console.log('\n== Engine wrapper (fake AudioContext) ==');
  const SR = 48000;
  let proc = null, frame = 0, engineMsg = null, ctxState = 'suspended';
  const L = new Float32Array(128), R = new Float32Array(128), I = new Float32Array(128);
  let micFeed = null, peak = 0, resumeCalls = 0, mods = [], gum = [];
  class FakeNode {
    constructor(ctx, name, opts) {
      globalThis.sampleRate = SR; globalThis.currentFrame = frame;
      proc = new Proc(); this.opts = opts; this.name = name;
      const self = this;
      this.port = {
        set onmessage(fn) { engineMsg = fn; },
        postMessage(m) { globalThis.currentFrame = frame; proc.port.onmessage({ data: m }); },
      };
    }
    connect() { this.connected = true; }
  }
  class FakeCtx {
    constructor(o) { this.sampleRate = SR; this.opts = o; this.destination = {}; this.audioWorklet = { addModule: async (u) => { mods.push(String(u)); } }; this.state = 'suspended'; }
    resume() { resumeCalls++; this.state = 'running'; return Promise.resolve(); }
    suspend() { this.state = 'suspended'; return Promise.resolve(); }
    createBuffer() { return {}; }
    createBufferSource() { return { connect() {}, start() {} }; }
    createMediaStreamSource(stream) { return { connect(node) { micFeed = (i) => 0.5 * Math.sin(2 * Math.PI * 440 * i / SR); }, disconnect() {} }; }
  }
  const pump = (sec) => {
    const n = Math.ceil(sec * SR / 128);
    for (let q = 0; q < n; q++) {
      globalThis.currentFrame = frame; globalThis.sampleRate = SR;
      for (let i = 0; i < 128; i++) I[i] = (micFeed && ctxState === 'running') ? micFeed(frame + i) : 0;
      proc.process([[I]], [[L, R]]);
      for (let i = 0; i < 128; i++) { const a = Math.abs(L[i]); if (a > peak) peak = a; }
      frame += 128;
      const q2 = proc.port.posted; proc.port.posted = [];
      for (const m of q2) if (engineMsg) engineMsg({ data: m });
    }
  };
  globalThis.AudioContext = FakeCtx; globalThis.AudioWorkletNode = FakeNode;
  Object.defineProperty(globalThis, 'navigator', { configurable: true, value: { mediaDevices: { getUserMedia: async (c) => { gum.push(c); return { getAudioTracks: () => [{}] }; } } } });

  const e = new Engine();
  const custom = e.defaults(); custom.step_ms = 100; custom.wave = 2; custom.key = 3; custom.syn_seq[5] = 0x12;
  e.setPersist(custom);                                         // before start: queued
  e.noteOn(12);                                                 // gestures before start are dropped
  const ticks = []; e.onTick = (t) => ticks.push(t);
  const timer = setInterval(() => pump(0.01), 2);
  const startP = e.start();
  await startP;
  check(e.ready && e.sampleRate === SR && mods.length === 1 && /pmor-worklet\.js$/.test(mods[0]), 'start() loads the worklet module and resolves when it runs', mods[0] && mods[0].split('/').pop());
  check(resumeCalls >= 1, 'start() resumes the context inside the call (iOS unlock)');
  await new Promise((r) => setTimeout(r, 150));
  check(ticks.length > 2 && ticks.every((t) => t.micState === 'none') && ticks[ticks.length - 1].step_ms === 100 && ticks[ticks.length - 1].params.wave === 2, 'queued setPersist applied; ticks carry micState', `${ticks.length} ticks`);
  const p1 = await e.getPersist();
  check(p1.step_ms === 100 && p1.syn_seq[5] === 0x12 && JSON.stringify(p1) === JSON.stringify(JSON.parse(JSON.stringify(p1))), 'getPersist resolves with a JSON-safe object');
  peak = 0; e.noteOn(13); await new Promise((r) => setTimeout(r, 100));
  check(peak > 1e-4, 'noteOn after start sounds', `peak ${peak.toExponential(2)}`);
  e.noteOff(13);
  // mic is lazy: nothing asked until a sampler hold crosses 300 ms
  check(gum.length === 0 && e.micState === 'none', 'no getUserMedia at start');
  e.samplerPress(4); e.samplerRelease(4); await new Promise((r) => setTimeout(r, 400));
  check(gum.length === 0, 'a short sampler tap never asks for the mic');
  ctxState = 'running';
  e.samplerPress(2);
  await new Promise((r) => setTimeout(r, 450));
  check(gum.length === 1 && JSON.stringify(gum[0]) === JSON.stringify({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } }) && e.micState === 'ok',
    'a >= 300 ms hold asks once, with the exact constraints', JSON.stringify(gum[0]));
  await new Promise((r) => setTimeout(r, 250));
  e.samplerRelease(2); await new Promise((r) => setTimeout(r, 100));
  const last = ticks[ticks.length - 1];
  check(last.micState === 'ok' && last.smp_len[2] > 0.2, 'recording from the stream lands in the slot', `${last.smp_len[2].toFixed(3)} s`);
  const p2 = await e.getPersist();
  check(p2.audio[2] && typeof p2.audio[2].b64 === 'string' && p2.audio[2].n === Math.round(last.smp_len[2] * SR), 'getPersist returns sampler audio as base64', `${p2.audio[2] && p2.audio[2].b64.length} chars`);
  e.setPersist(e.defaults()); await new Promise((r) => setTimeout(r, 100));
  check(ticks[ticks.length - 1].smp_len[2] === 0, 'setPersist(defaults) clears the audio');
  e.setPersist(JSON.parse(JSON.stringify(p2))); await new Promise((r) => setTimeout(r, 100));
  const p3 = await e.getPersist();
  check(ticks[ticks.length - 1].smp_len[2] > 0.2 && p3.audio[2].b64 === p2.audio[2].b64, 'base64 audio survives a setPersist round trip bit-exactly');
  e.set('step_ms', 200); e.adjust('bpm', 1); e.tap(); e.fxHeld(0, 3); e.fxBank(2); e.toggleStep(1, 0, 3); e.selectDrum(1); e.selectSlot(7);
  e.synMode(1); e.synArm(5); e.synLiveWrite(false); e.fxRecord(false); e.allKeysOff();
  await new Promise((r) => setTimeout(r, 120));
  const lt = ticks[ticks.length - 1];
  check(lt.step_ms === 197 && lt.fx.bank === 2 && lt.sel_drum === 1 && lt.sel_smp === 7 && lt.params.syn_mode === 1 && lt.syn_arm === 5 && lt.dr_counts[0] === 1 && lt.fx.held[0] === 3,
    'the setters reach the worklet', `step_ms ${lt.step_ms}`);
  e.suspend(); check(e.ctx.state === 'suspended', 'suspend() suspends the context');
  e.resume(); check(e.ctx.state === 'running', 'resume() resumes it');
  clearInterval(timer);

  // denied microphone
  Object.defineProperty(globalThis, 'navigator', { configurable: true, value: { mediaDevices: { getUserMedia: async () => { throw new Error('NotAllowedError'); } } } });
  const e2 = new Engine(); const t2 = setInterval(() => pump(0.01), 2); await e2.start();
  const tk2 = []; e2.onTick = (t) => tk2.push(t);
  e2.samplerPress(0); await new Promise((r) => setTimeout(r, 450)); e2.samplerRelease(0);
  await new Promise((r) => setTimeout(r, 100));
  clearInterval(t2);
  check(e2.micState === 'denied' && tk2.length && tk2[tk2.length - 1].micState === 'denied', "refused permission reports micState 'denied'");
  const e3 = new Engine();
  check((await e3.getPersist()).step_ms === 125, 'getPersist before start returns the defaults');
}

const rates = process.argv.length > 2 ? process.argv.slice(2).map(Number) : [48000, 44100];
for (const sr of rates) { const t0 = total; suite(sr); console.log(`-- ${total - t0} checks at ${sr} Hz`); }
{ const t0 = total; await wrapperSuite(); console.log(`-- ${total - t0} checks`); }
console.log(fails ? `\n${fails} FAILED` : '\nall passed');
process.exit(fails ? 1 : 0);
