// worklet.js -- AudioWorkletProcessor hosting hxsim.wasm (the firmware FX engine + mapper + synthetic trumpet).
// The main thread sends the wasm bytes (no fetch / TextDecoder needed here). Single-threaded: slot swaps run in the
// message handler, i.e. between process() calls, never inside audio (same rule as the firmware's control thread).
//
// TIMING CLOCK: AudioWorkletGlobalScope has no guaranteed performance.now (Chrome: absent). The wasm imports
// env.now_ms; we use performance.now() if it exists, otherwise Date.now() (1 ms resolution). The headline CPU number
// is therefore measured independently of the engine's per-block EMA: wall ms spent in sim_process summed over ~64
// quanta (~170 ms) and divided by the number of 64-sample blocks; ms-quantisation averages out (unbiased), the
// per-slot figures (engine EMA of single blocks) are noisy when only Date.now is available. Never Seed timing.
const NCTL = 21;
const hasPerf = (typeof performance !== 'undefined' && performance && typeof performance.now === 'function');
const nowMs = hasPerf ? () => performance.now() : () => Date.now();

function cstr(mem, ptr) {
  if (!ptr) return '';
  const u8 = new Uint8Array(mem.buffer); let e = ptr; while (u8[e]) e++;
  let s = ''; for (let i = ptr; i < e; i++) s += String.fromCharCode(u8[i]);
  return s;
}

class HxProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null; this.mem = null; this.ready = false;
    this.ctl = new Float32Array(NCTL);
    this.encPending = [0, 0];
    this.tr = [4, 0.6, 5.2, 0.15]; this.latch = true; this.gate = false;
    this.mode = 0;                       // 0 synth, 1 file, 2 mic
    this.file = null; this.filePos = 0;
    this.slotType = [-2, -2, -2, -2]; this.sentVer = [0, 0, 0, 0];
    this.quanta = 0; this.wallSum = 0; this.wallBlocks = 0; this.cpuMs = 0; this.blockMsAvg = 0;
    this.rmsAcc = 0; this.rmsN = 0; this.lastRms = 0; this.peak = 0; this.maxAbs = 0; this.nonFinite = 0;
    this.stateEvery = 8; this.statePending = false;
    this.port.onmessage = (e) => { try { this.onMsg(e.data); } catch (err) { this.port.postMessage({ t: 'error', msg: String(err && err.stack || err) }); } };
  }
  async init(bytes) {
    const mod = await WebAssembly.compile(bytes);
    const imports = { env: { now_ms: nowMs }, wasi_snapshot_preview1: {} };
    const need = WebAssembly.Module.imports(mod);
    for (const i of need) {          // stub anything else the libc wants (WASI fd_*, proc_exit, ...)
      imports[i.module] = imports[i.module] || {};
      if (!imports[i.module][i.name] && i.kind === 'function') imports[i.module][i.name] = () => 0;
    }
    const inst = await WebAssembly.instantiate(mod, imports);
    this.ex = inst.exports; this.mem = this.ex.memory;
    if (this.ex._initialize) this.ex._initialize();
    this.ex.sim_init(sampleRate);
    this.io = this.ex.sim_io() >> 2;                       // float index of scratch
    this.INP = this.io; this.OUTL = this.io + 128; this.OUTR = this.io + 256; this.CTLP = this.io + 384; this.METP = this.io + 384 + 32;
    this.ready = true;
    const effects = [];
    for (let i = 0; i < this.ex.sim_effect_count(); i++)
      effects.push({ id: cstr(this.mem, this.ex.sim_effect_id(i)), name: cstr(this.mem, this.ex.sim_effect_name(i)), v1: cstr(this.mem, this.ex.sim_effect_v1(i)) });
    this.port.postMessage({ t: 'ready', effects, sampleRate, perf: hasPerf, block: this.ex.sim_block() });
    this.sendState(true);
  }
  f32() { return new Float32Array(this.mem.buffer); }
  pushCtl() {
    const f = this.f32(); f.set(this.ctl, this.CTLP);
    f[this.CTLP + 12] = this.encPending[0]; f[this.CTLP + 13] = this.encPending[1];
    this.encPending[0] = this.encPending[1] = 0;
    this.ex.sim_set_controls(this.CTLP << 2);
    this.pushTrumpet();
  }
  pushTrumpet() {
    const b = (this.latch || this.gate) ? this.tr[1] : 0;
    this.ex.sim_set_trumpet(this.tr[0], b, this.tr[2], this.tr[3]);
  }
  onMsg(m) {
    if (m.t === 'wasm') { this.init(m.bytes).catch((err) => this.port.postMessage({ t: 'error', msg: 'wasm init: ' + err })); return; }
    if (!this.ready) return;
    switch (m.t) {
      case 'ctl': this.ctl.set(m.a.slice(0, NCTL)); this.ctl[12] = 0; this.ctl[13] = 0;
                  this.encPending[0] += m.a[12] | 0; this.encPending[1] += m.a[13] | 0; this.pushCtl(); break;
      case 'trumpet': this.tr = m.a; this.latch = !!m.latch; this.gate = !!m.gate; this.pushTrumpet(); break;
      case 'tongue': this.ex.sim_tongue(); break;
      case 'mode': this.mode = m.mode; break;
      case 'file': this.file = m.data; this.filePos = 0; break;
      case 'fx': this.ex.sim_set_slot_effect(m.slot, m.id); this.sendState(true); break;
      case 'param': this.ex.sim_set_param(m.slot, m.idx, m.v); break;
      case 'bypass': this.ex.sim_set_bypass(m.slot, m.b ? 1 : 0); break;
      case 'action': this.ex.sim_action(m.slot, m.id, m.v); break;
      case 'select': this.ex.sim_select_slot(m.slot); this.sendState(true); break;
      case 'patch': this.ex.sim_load_default_patch(); this.sendState(true); break;
      case 'poll': this.sendState(true); break;
      case 'stats': this.port.postMessage({ t: 'stats', rms: this.lastRms, peak: this.peak, maxAbs: this.maxAbs, nonFinite: this.nonFinite, blockMsAvg: this.blockMsAvg, cpuMs: this.cpuMs, quanta: this.quanta }); break;
    }
  }
  describe(slot) {
    const ex = this.ex, n = ex.sim_param_count(slot), params = [];
    for (let p = 0; p < n; p++) {
      const fl = ex.sim_param_flags(slot, p), mn = ex.sim_param_min(slot, p), mx = ex.sim_param_max(slot, p);
      let names = null;
      if (fl & 8) { names = []; for (let k = 0; k <= Math.round(mx - mn); k++) names.push(cstr(this.mem, ex.sim_param_enum_name(slot, p, k))); }
      params.push({ name: cstr(this.mem, ex.sim_param_name(slot, p)), min: mn, max: mx, step: ex.sim_param_step(slot, p), flags: fl, role: ex.sim_param_role(slot, p), names });
    }
    return { type: ex.sim_slot_effect(slot), ok: !!ex.sim_slot_ok(slot), name: cstr(this.mem, ex.sim_slot_name(slot)), params };
  }
  sendState(force) {
    const ex = this.ex; const slots = [];
    for (let s = 0; s < 4; s++) {
      const type = ex.sim_slot_effect(s), np = ex.sim_param_count(s);
      const key = type + ':' + np;
      let desc = null;
      if (force || key !== this.slotKey?.[s]) { desc = this.describe(s); (this.slotKey = this.slotKey || [])[s] = key; }
      const vals = []; for (let p = 0; p < np; p++) vals.push(ex.sim_param_value(s, p));
      slots.push({ type, byp: ex.sim_get_bypass(s), vals, desc });
    }
    const f = this.f32(); ex.sim_read_meters(this.METP << 2);
    const met = Array.from(f.subarray(this.METP, this.METP + 11));
    const leds = Array.from(new Uint8Array(this.mem.buffer, ex.sim_pad_leds(), 48));
    this.port.postMessage({ t: 'state', slots, met, leds, status: cstr(this.mem, ex.sim_status_line()), page: ex.sim_mapper_page(), sel: ex.sim_selected_slot(), selParam: ex.sim_selected_param(),
      cpuBlockMs: this.blockMsAvg, running: this.running !== false, rms: this.lastRms, peak: this.peak });
    this.peak = 0;
  }
  process(inputs, outputs) {
    const out = outputs[0]; const L = out[0], R = out[1] || out[0];
    if (!this.ready) return true;
    const n = L.length, f = this.f32();
    let inMode = this.mode === 0 ? 0 : 1;
    if (this.mode === 1) {
      const d = this.file;
      if (d && d.length) { for (let i = 0; i < n; i++) { f[this.INP + i] = d[this.filePos]; if (++this.filePos >= d.length) this.filePos = 0; } }
      else f.fill(0, this.INP, this.INP + n);
    } else if (this.mode === 2) {
      const ch = inputs[0] && inputs[0][0];
      if (ch && ch.length >= n) f.set(ch.subarray(0, n), this.INP); else f.fill(0, this.INP, this.INP + n);
    }
    const t0 = nowMs();
    this.ex.sim_process(this.mode === 0 ? 0 : (this.INP << 2), this.OUTL << 2, this.OUTR << 2, n, inMode);
    this.wallSum += nowMs() - t0; this.wallBlocks += n / 64;
    const o = new Float32Array(this.mem.buffer, this.OUTL << 2, n), o2 = new Float32Array(this.mem.buffer, this.OUTR << 2, n);
    L.set(o); if (R !== L) R.set(o2);
    let e = 0;
    for (let i = 0; i < n; i++) { const a = o[i], b = o2[i]; e += a * a + b * b; const m = Math.max(Math.abs(a), Math.abs(b)); if (m > this.peak) this.peak = m; if (m > this.maxAbs) this.maxAbs = m; if (!(a - a === 0) || !(b - b === 0)) this.nonFinite++; }
    this.rmsAcc += e; this.rmsN += 2 * n;
    this.quanta++;
    if (this.quanta % 64 === 0) {
      this.blockMsAvg = this.wallSum / this.wallBlocks; this.wallSum = 0; this.wallBlocks = 0;
      this.lastRms = Math.sqrt(this.rmsAcc / Math.max(1, this.rmsN)); this.rmsAcc = 0; this.rmsN = 0;
    }
    if (this.quanta % this.stateEvery === 0) this.sendState(false);   // ~42 Hz
    return true;
  }
}
registerProcessor('hx-sim', HxProcessor);
