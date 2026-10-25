// pmor-worklet.js - PMOR audio engine (AudioWorkletProcessor).
//
// Port of POCKETGORATOR_CODE.cpp (Daisy Seed firmware). "L<n>" comments cite
// firmware line numbers. The firmware runs 64-sample blocks at 48 kHz; this file
// runs the same per-block logic twice per 128-sample render quantum. Every
// constant below is written in firmware (48 kHz) terms and converted to the
// context rate once, at init (see "rate scaling").
//
// Plain script, no imports/exports, so it loads as an audio worklet module and
// also under node for the tests.

'use strict';

const PI_F = 3.14159265;
const TWO_PI_F = 6.2831853;
const MAXSTEPS = 128;                       // L639
const WT_LEN = 256, WT_N = 8;               // L529
const TICK_MS = 66;                         // max interval between posted ticks

// L482 scale modes
const MODES = [
  [0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10],
  [0, 2, 4, 6, 7, 9, 11], [0, 2, 4, 5, 7, 9, 10], [0, 2, 3, 5, 7, 8, 10],
  [0, 1, 3, 5, 6, 8, 10], [0, 2, 3, 5, 7, 8, 11], [0, 2, 4, 7, 9, 9, 9],
];
const DETUNE = [-0.012, -0.006, 0, 0.006, 0.012];                      // L507
// L984 master volume table
const KVOL = [0, 0.01, 0.0152, 0.0231, 0.03511, 0.05337, 0.08111, 0.12328,
  0.18738, 0.2848, 0.43287, 0.65793, 1.0, 1.0772, 1.1604, 1.25];

// L1584 / L1617 drum kits, one row per pad (row 0 first): f, amp, dec, noiz, drop, pan, part
const KIT = [
  [6000, 0.42, 0.998, 1, 1, 0.62, 0], [6000, 0.40, 0.999, 1, 1, 0.62, 0],
  [6000, 0.38, 0.99965, 1, 1, 0.62, 0], [6000, 0.33, 0.99988, 1, 1, 0.58, 0],
  [210, 0.80, 0.9995, 0.85, 1.5, 0.45, 0], [240, 0.78, 0.99938, 0.90, 1.6, 0.50, 0],
  [175, 0.80, 0.99968, 0.78, 1.4, 0.45, 0], [300, 0.72, 0.9993, 0.95, 1.2, 0.55, 0],
  [130, 0.92, 0.99982, 0.04, 2.2, 0.42, 0], [190, 0.90, 0.99978, 0.04, 2.2, 0.60, 0],
  [540, 0.90, 0.99945, 0.04, 1.0, 0.55, 1.48], [0, 0.55, 0.9994, 0.06, 1.8, 0.50, 2.717],
  [56, 0.97, 0.99984, 0.06, 4.2, 0.50, 0], [52, 1.00, 0.99993, 0.05, 6.0, 0.50, 0],
  [60, 0.97, 0.9998, 0.10, 4.8, 0.50, 0], [52, 1.00, 0.99995, 0.05, 7.0, 0.50, 0],
];
const KIT2 = [
  [9000, 0.40, 0.9977, 1, 1, 0.60, 0], [9000, 0.38, 0.9988, 1, 1, 0.64, 0],
  [9000, 0.36, 0.99955, 1, 1, 0.66, 0], [7200, 0.34, 0.99986, 1, 1, 0.55, 0],
  [1100, 0.70, 0.99955, 0.95, 1, 0.45, 0], [320, 0.72, 0.9994, 0.80, 1.4, 0.52, 0],
  [180, 0.78, 0.99965, 0.70, 1.5, 0.45, 0], [2200, 0.60, 0.99935, 0.92, 1, 0.58, 0],
  [220, 0.80, 0.99955, 0.04, 3.0, 0.42, 1.50], [330, 0.78, 0.9995, 0.04, 3.0, 0.60, 2.01],
  [660, 0.74, 0.9993, 0.05, 1.0, 0.55, 3.46], [0, 0.55, 0.99935, 0.05, 2.2, 0.50, 4.21],
  [55, 1.00, 0.99988, 0.08, 4.0, 0.50, 0], [52, 1.00, 0.99992, 0.12, 5.5, 0.50, 0],
  [50, 1.00, 0.99996, 0.03, 3.0, 0.50, 0], [58, 1.00, 0.99985, 0.20, 8.0, 0.50, 0],
];

// FX bit indices per bank (L724-826). Pad k = bit k.
const FX_ST4 = 0, FX_ST8 = 1, FX_ST16 = 2, FX_ST32 = 3, FX_REV = 4, FX_CHOR = 5, FX_PAN = 6,
  FX_CRUSH = 7, FX_HALF = 8, FX_DBL = 9, FX_BZZ = 10, FX_MUTE = 11, FX_FLT = 12, FX_WASH = 13,
  FX_DIST = 14, FX_STOP = 15;

// ---------------------------------------------------------------- rate scaling
// SR = context rate. K = SR/48000. Firmware constants are per 48 kHz sample:
//   pcx(c)  per-sample one-pole/slew coefficient -> same time constant at SR (exact)
//   pcq(c)  same, linear (cheap, for coefficients that depend on FSR and change per block)
//   dcy(d)  per-sample decay factor close to 1 (linear form)
//   fcv(f)  raw Chamberlin SVF coefficient -> same cutoff frequency at SR
//   nsm(n)  a length in samples -> same duration at SR (never below 1)
//   nsc(n)  buffer length in samples (ceil)
let SR = 48000, K = 1, IK = 1, SPMS = 48;
function pcx(c) { return K === 1 ? c : 1 - Math.pow(1 - c, IK); }
function pcq(c) { const r = c * IK; return r > 1 ? 1 : r; }
function dcy(d) { return K === 1 ? d : 1 - (1 - d) * IK; }
function fcv(f) {
  if (K === 1) return f;
  let a = Math.asin(Math.min(f, 1.999) * 0.5) * IK;
  if (a > 1.5) a = 1.5;
  return 2 * Math.sin(a);
}
function nsm(n) { const r = Math.round(n * K); return r < 1 ? 1 : r; }
function nsc(n) { return Math.max(2, Math.ceil(n * K)); }
function clampi(v, lo, hi) { v = Math.trunc(+v); if (!(v === v)) v = lo; return v < lo ? lo : v > hi ? hi : v; }

function psin(ph) { const x = ph + ph - 1; return 4 * x * (1 - Math.abs(x)); }     // L583
function polyBlep(t, dt) {                                                           // L593
  if (dt <= 0) return 0;
  if (t < dt) { t /= dt; return t + t - t * t - 1; }
  if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
  return 0;
}
function polyBlamp(t, dt) {                                                          // L599
  if (dt <= 0) return 0;
  if (t < dt) { t = t / dt - 1; return -0.33333333 * t * t * t; }
  if (t > 1 - dt) { t = (t - 1) / dt + 1; return 0.33333333 * t * t * t; }
  return 0;
}

// ---------------------------------------------------------------- global state
// every name is the firmware name where one exists
let lcg_st = 0xBEEF1234;                                                             // L607
function lcg() { lcg_st = (Math.imul(lcg_st, 1664525) + 1013904223) >>> 0; return lcg_st; }

// musical globals
let step_ms = 125, g_key = 0, g_mode = 3, g_swing = 0, g_csyn = 6, g_cpoly = 5, g_cdrum = 9,
  g_csmp = 9, g_wave = 5, g_prise = 0, g_wtscan = 0, g_vsyn = 10, g_vdrum = 10, g_vsmp = 10,
  kit_sel = 0, form_blen = 16, form_bars = 1, seq_mode_syn = 0, vol_pidx = 9, mvol = 0.2848,
  fx_bank = 0, sel_drum = 0, sel_smp = 0, gstep = 0, syn_arm = 0, held_mask = 0;
let FLYD, wtbl, dr_pad, syn_seq, dr_pat, smp_pat;

// sequencer / clock statics (gridTick)
let next_step, vinit, nextv, bstep, bent_prev, rt_dr, rt_nv, rt_notes, step_t, sct, next_dbl,
  rt_prev, rt_armed, rt_lastg, rt_half, next_bz, bz_iv, bz_sct;
let live_dr, live_nt, live_nn;
let vplay, vrel, vgate_t, key_voice, kprs_t;
let stepEdge = false;                  // a master step fired since the last tick post
let dirty = 0;                         // bumped on every change worth autosaving

// FX control state
let fx_held, fx_t, fx_auto, fx_auto_fsr, fx_eff, fx_fsr = 0, fx_fsr_eff = 0, fx_lid = 0, fx_vs = 1;
let fx_rec = false, syn_live = false;
let step_rw = 0, rb_w = 0;
let gstep_samp = 0, gstep_blk = -1;

// voices (structure of arrays)
let vPhase, vRatio, vPhaseInc, vEnv, vEnvT, vPan, vRel, vAtk, vDrift, vDriftT, vFc, vFcT, vFcR,
  vQ, vLp, vBp, vFmi, vFmiT, vPmul, vType, vKpos, vKlen, vNosc, vKinit, vPrise, kpbuf, KPN;

// drums
let dgoF, dgoAmp, dgoDec, dgoNoiz, dgoDrop, dgoPan, dgoPart, dgoStamp;
let drEnv, drPh, drF, drFt, drDec, drNoiz, drAmp, drPanl, drPanr, drPart, drStamp;
let arng = 0x12345678, hatlp = 0;

// sampler
let smp, SMPL_N, smp_len, rec_slot = -1, rec_pos = 0, smp_trig, smp_press, sp_pos, sp_st, sp_act, sp_fr;
let micOk = false;

// tempo tap (L4510)
let tap_prev = 0, tap_gaps, tap_n = 0, tap_i = 0;

// master chain state
let lpL = 0, bpL = 0, lpR = 0, bpR = 0, dlpL = 0, dbpL = 0, dlpR = 0, dbpR = 0;
let ff_s, filter_f, vss = 1;
let axf = 0, ax_target = 0, ax_pending = false, ax_next = 0;
let crL = 0, crR = 0, crn = 0, buf_prev = false, fz_anchor = 0, rev_pos = 0, xw = 0, xlfo = 0,
  panph = 0, mg = 1;
let rbufL, rbufR, RBUF_N, xcL, xcR, XN;
let oL = 0, oR = 0, blkS = 0, blkLid = 0, gslen = 1;
let outPeak = 0, nanFlag = false, errSent = false;

// per-block scratch
let VSS, MIXL, MIXR, DRL, DRR;

// per-bank effect state objects and buffers (created by initFx)
let F1, F2, F3, F4, F5, F6, F7, F8, FM, B;

// rate-scaled constants (set in init)
let C_DRIFT, C_FMI, C_VSS, C_FFS, C_DPITCH, C_HAT, PR_STEP;
let GVL, PLC, MTC;

const tickObj = {
  gstep: 0, songLen: 16, step_ms: 125, bpm: 120, out_amp: 0,
  playing: { syn: 0, held: 0 },
  syn_seq_page: new Uint16Array(16), syn_arm: 0,
  dr_page: [0, 0, 0, 0], sm_page: new Array(16).fill(0),
  dr_counts: [0, 0, 0, 0], sm_counts: new Array(16).fill(0),
  dr_pad: [13, 10, 4, 0], sel_drum: 0, sel_smp: 0,
  smp_len: new Float32Array(16), rec_slot: -1,
  fx: { bank: 0, eff: new Array(8).fill(0), held: new Array(8).fill(0), fsr: 0, lid: 0, rec: false },
  params: {
    wave: 5, dens: 6, poly: 5, vsyn: 10, wtscan: 0, prise: 0, bars: 1, beats: 16, kit: 0,
    cdrum: 9, vdrum: 10, csmp: 9, vsmp: 10, key: 0, mode: 3, swing: 0, mvol_idx: 9, syn_mode: 0,
  },
  micState: 'none', sampleRate: 48000, dirty: 0,
};

function songLen() {                                                                  // L652
  let n = form_blen * form_bars;
  if (n < 1) n = 1;
  if (n > MAXSTEPS) n = MAXSTEPS;
  return n;
}
function seqBit(p, base, s) { return (p[base + (s >> 3)] >> (s & 7)) & 1; }
function popcount8(x) { x = x - ((x >> 1) & 0x55); x = (x & 0x33) + ((x >> 2) & 0x33); return (x + (x >> 4)) & 0x0f; }
function seqCount(p, base) { let c = 0; for (let j = 0; j < 16; j++) c += popcount8(p[base + j]); return c; }

function rebuildScale() {                                                             // L497
  const root = 174.61 * Math.pow(2, g_key / 12);
  const iv = MODES[g_mode];
  for (let i = 0; i < 28; i++) FLYD[i] = root * Math.pow(2, (iv[i % 7] + 12 * ((i / 7) | 0)) / 12);
}

function buildWavetables() {                                                          // L533
  for (let t = 0; t < WT_N; t++) {
    const w = wtbl[t];
    for (let i = 0; i < WT_LEN; i++) {
      const ph = i / WT_LEN; let s = 0;
      switch (t) {
        case 0: s = Math.sin(TWO_PI_F * ph); break;
        case 1: for (let h = 1; h <= 15; h += 2) { const sg = ((h >> 1) & 1) ? -1 : 1; s += sg * Math.sin(TWO_PI_F * h * ph) / (h * h); } break;
        case 2: for (let h = 1; h <= 8; h++) s += Math.sin(TWO_PI_F * h * ph) / h; break;
        case 3: for (let h = 1; h <= 24; h++) s += Math.sin(TWO_PI_F * h * ph) / h; break;
        case 4: for (let h = 1; h <= 23; h += 2) s += Math.sin(TWO_PI_F * h * ph) / h; break;
        case 5: { const a = [1, 0.5, 0.32, 0.22], hh = [1, 2, 4, 8]; for (let j = 0; j < 4; j++) s += a[j] * Math.sin(TWO_PI_F * hh[j] * ph); } break;
        case 6: for (let h = 1; h <= 12; h++) { const d = h - 4; s += Math.exp(-0.35 * d * d) * Math.sin(TWO_PI_F * h * ph); } break;
        case 7: for (let h = 1; h <= 16; h++) s += Math.sin(TWO_PI_F * h * ph) * (0.55 + 0.45 / h); break;
      }
      w[i] = s;
    }
    let pk = 0.0001;
    for (let i = 0; i < WT_LEN; i++) { const a = Math.abs(w[i]); if (a > pk) pk = a; }
    const g = 1 / pk;
    for (let i = 0; i < WT_LEN; i++) w[i] *= g;
  }
}

// ---------------------------------------------------------------- init
function init() {
  SR = (typeof sampleRate === 'number' && sampleRate > 8000) ? sampleRate : 48000;
  K = SR / 48000; IK = 1 / K; SPMS = SR / 1000;

  lcg_st = 0xBEEF1234;                                   // firmware boot seed (L607)
  kn_rr = 0;
  FLYD = new Float64Array(28);
  wtbl = []; for (let t = 0; t < WT_N; t++) wtbl.push(new Float64Array(WT_LEN));
  buildWavetables();
  dr_pad = new Uint8Array([13, 10, 4, 0]);
  syn_seq = new Uint16Array(MAXSTEPS);
  dr_pat = new Uint8Array(64);
  smp_pat = new Uint8Array(256);

  step_ms = 125; g_key = 0; g_mode = 3; g_swing = 0; g_csyn = 6; g_cpoly = 5; g_cdrum = 9; g_csmp = 9;
  g_wave = 5; g_prise = 0; g_wtscan = 0; g_vsyn = g_vdrum = g_vsmp = 10; kit_sel = 0;
  form_blen = 16; form_bars = 1; seq_mode_syn = 0; vol_pidx = 9; mvol = KVOL[9]; fx_bank = 0;
  sel_drum = 0; sel_smp = 0; gstep = 0; syn_arm = 0; held_mask = 0; syn_live = false; fx_rec = false;
  rebuildScale();

  next_step = 0; vinit = false; nextv = 0; bstep = 0; bent_prev = false; rt_dr = 0; rt_nv = 0;
  rt_notes = new Int32Array(8); step_t = 0; sct = 0; next_dbl = 0; rt_prev = 0; rt_armed = false;
  rt_lastg = -1; rt_half = 0; next_bz = 0; bz_iv = 60; bz_sct = 0xFFFFFFFF;
  live_dr = 0; live_nt = new Int32Array(4); live_nn = 0;
  vplay = new Uint8Array(12); vrel = new Float64Array(12); vgate_t = new Float64Array(12);
  key_voice = new Int32Array(16).fill(-1); kprs_t = new Float64Array(16);
  stepEdge = false; dirty = 0;

  fx_held = new Uint16Array(8); fx_t = new Float64Array(16); fx_auto = new Uint16Array(128);
  fx_auto_fsr = new Uint8Array(16); fx_eff = new Uint16Array(8);
  fx_fsr = 0; fx_fsr_eff = 0; fx_lid = 0; fx_vs = 1; step_rw = 0; rb_w = 0; gstep_samp = 0; gstep_blk = -1;

  vPhase = new Float64Array(60); vRatio = new Float64Array(60).fill(1);
  const mk = () => new Float64Array(12);
  vPhaseInc = mk(); vEnv = mk(); vEnvT = mk(); vPan = mk().fill(0.5); vRel = mk().fill(0.001);
  vAtk = mk().fill(0.08); vDrift = mk(); vDriftT = mk(); vFc = mk().fill(0.2); vFcT = mk().fill(0.1);
  vFcR = mk().fill(0.0001); vQ = mk().fill(0.9); vLp = mk(); vBp = mk(); vFmi = mk(); vFmiT = mk();
  vPmul = mk().fill(1);
  vType = new Int32Array(12); vKpos = new Int32Array(12); vKlen = new Int32Array(12).fill(64);
  vNosc = new Int32Array(12).fill(5); vKinit = new Uint8Array(12); vPrise = new Uint8Array(12);
  KPN = nsc(672); kpbuf = new Float32Array(12 * KPN);

  dgoF = new Float64Array(4).fill(60); dgoAmp = new Float64Array(4).fill(0.8); dgoDec = new Float64Array(4).fill(0.9999);
  dgoNoiz = new Float64Array(4); dgoDrop = new Float64Array(4).fill(1); dgoPan = new Float64Array(4).fill(0.5);
  dgoPart = new Float64Array(4); dgoStamp = new Uint32Array(4);
  drEnv = new Float64Array(4); drPh = new Float64Array(4); drF = new Float64Array(4); drFt = new Float64Array(4);
  drDec = new Float64Array(4); drNoiz = new Float64Array(4); drAmp = new Float64Array(4);
  drPanl = new Float64Array(4); drPanr = new Float64Array(4); drPart = new Float64Array(4); drStamp = new Uint32Array(4);
  arng = 0x12345678; hatlp = 0;

  SMPL_N = Math.round(2 * SR);                       // slots stay 2 s (L907)
  smp = new Float32Array(16 * SMPL_N); smp_len = new Uint32Array(16); rec_slot = -1; rec_pos = 0;
  smp_trig = new Uint32Array(16); smp_press = new Float64Array(16);
  sp_pos = new Uint32Array(16); sp_st = new Uint32Array(16); sp_act = new Uint8Array(16); sp_fr = new Float64Array(16);
  micOk = false;
  tap_prev = -1e9; tap_gaps = new Float64Array(4); tap_n = 0; tap_i = 0;

  C_DRIFT = pcx(0.00002); C_FMI = pcx(0.00015); C_VSS = pcx(0.0005); C_FFS = pcx(0.0015);
  C_DPITCH = pcx(0.0012); C_HAT = pcx(0.12); PR_STEP = 0.5 / SR;
  GVL = [1201, 1291, 1487, 1693].map(nsm); PLC = [1789, 2143, 2647, 3119].map(nsm);
  MTC = [149, 179, 211, 251];

  filter_f = fcv(0.62); ff_s = fcv(0.62); axf = 0; ax_target = 0; ax_pending = false; ax_next = 0;
  lpL = bpL = lpR = bpR = dlpL = dbpL = dlpR = dbpR = 0; vss = 1;
  crL = crR = 0; crn = 0; buf_prev = false; fz_anchor = 0; rev_pos = 0; xw = 0; xlfo = 0; panph = 0; mg = 1;
  RBUF_N = nsc(96000); rbufL = new Float32Array(RBUF_N); rbufR = new Float32Array(RBUF_N);
  XN = nsc(4096); xcL = new Float32Array(XN); xcR = new Float32Array(XN);
  outPeak = 0; nanFlag = false; errSent = false;

  VSS = new Float64Array(64); MIXL = new Float64Array(64); MIXR = new Float64Array(64);
  DRL = new Float64Array(64); DRR = new Float64Array(64);
  initFx();
}

// ---------------------------------------------------------------- voices
// L1000 voiceCharacter: per-note patch. Raw values are firmware (48 kHz) values;
// rateFix() converts them once the note's pitch tracking has been applied.
function voiceCharacter(v, generative) {
  const b = v * 5;
  vDrift[v] = 0;
  vDriftT[v] = ((lcg() % 200) - 100) * 0.00008;
  vNosc[v] = 5;
  vLp[v] = 0; vBp[v] = 0;
  vQ[v] = 0.70 + (lcg() % 35) * 0.01;
  const w = g_wave;
  if (w < 5) {                                         // classic: pure mono tone
    vType[v] = 10 + w; vNosc[v] = 1;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1;
    vDriftT[v] = 0;
    vFc[v] = 0.55; vFcT[v] = 0.18; vFcR[v] = 0.00006; vFmi[v] = 0;
    const ar2 = generative ? (lcg() % 100) : 100;
    vAtk[v] = ar2 < 20 ? 0.012 : 0.08;
    return;
  }
  if (w === 7) {                                       // WT
    vType[v] = 20;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1 + DETUNE[k];
    vFc[v] = 0.55; vFcT[v] = 0.14; vFcR[v] = 0.00006; vFmi[v] = 0;
    const arw = generative ? (lcg() % 100) : 100;
    vAtk[v] = arw < 20 ? 0.012 : 0.08;
    return;
  }
  if (w === 8) {                                       // FOLD
    vType[v] = 23;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1 + DETUNE[k];
    vFc[v] = 0.70; vFcT[v] = 0.40; vFcR[v] = 0.00005; vFmi[v] = 0;
    const arf = generative ? (lcg() % 100) : 100;
    vAtk[v] = arf < 20 ? 0.012 : 0.08;
    return;
  }
  if (w === 9) {                                       // PLUCK (Karplus-Strong)
    vType[v] = 24; vKinit[v] = 1;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1;
    vFc[v] = 0.85; vFcT[v] = 0.85; vFcR[v] = 0.0002; vAtk[v] = 0.5;
    return;
  }
  if (w === 10) {                                      // SUPER
    vType[v] = 10;
    const spread = 0.004 + g_wtscan * 0.0016;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1 + DETUNE[k] * (spread / 0.01);
    vFc[v] = 0.55; vFcT[v] = 0.20; vFcR[v] = 0.00005; vFmi[v] = 0;
    const ars = generative ? (lcg() % 100) : 100;
    vAtk[v] = ars < 20 ? 0.012 : 0.06;
    return;
  }
  if (w === 11) {                                      // CZ
    vType[v] = 26;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1 + DETUNE[k];
    vFc[v] = 0.80; vFcT[v] = 0.40; vFcR[v] = 0.00006; vFmi[v] = 0;
    const arc = generative ? (lcg() % 100) : 100;
    vAtk[v] = arc < 20 ? 0.012 : 0.08;
    return;
  }
  if (w === 12) {                                      // ARCADE
    vType[v] = 14; vNosc[v] = 1;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1;
    vDriftT[v] = 0;
    vFc[v] = 0.92; vFcT[v] = 0.92; vFcR[v] = 0.001; vQ[v] = 0.5; vFmi[v] = 0; vAtk[v] = 0.5;
    return;
  }
  if (w === 6) {                                       // FM
    vType[v] = 2;
    if ((lcg() % 3) !== 0) vRatio[b + 1] = 1 + (lcg() % 6);
    else vRatio[b + 1] = 0.5 + (lcg() % 750) * 0.01;
    vFmi[v] = 0.05 + (lcg() % 70) * 0.01;
    vFmiT[v] = vFmi[v] * 0.4;
    vFc[v] = 0.55; vFcT[v] = 0.18; vFcR[v] = 0.00008;
    const ar3 = generative ? (lcg() % 100) : 100;
    vAtk[v] = ar3 < 20 ? 0.012 : 0.08;
    return;
  }
  // 5 THIEF (default), L1101
  const r = lcg() % 100;
  if (r < 30) {
    vType[v] = 2;
    vRatio[b + 1] = [0.5, 1, 1.003, 2, 2.005, 4][lcg() % 6];
    vFmi[v] = 0.10 + (lcg() % 32) * 0.01;
    vFmiT[v] = 0.02;
    vFc[v] = 0.40; vFcT[v] = 0.15; vFcR[v] = 0.00008;
  } else {
    vType[v] = r < 85 ? 0 : 1;
    for (let k = 0; k < 5; k++) vRatio[b + k] = 1 + DETUNE[k];
    vFc[v] = 0.22 + (lcg() % 21) * 0.01;
    vFcT[v] = 0.025 + (lcg() % 8) * 0.01;
    vFcR[v] = 0.00004 + (lcg() % 9) * 0.00001;
  }
  const ar = generative ? (lcg() % 100) : 100;
  const swell = ar < 15;
  vAtk[v] = swell ? 0.0025 : (ar < 40 ? 0.012 : 0.08);
  if (swell) { vFc[v] = 0.04; vFcT[v] = 0.36; vFcR[v] = 0.00003; }
  if ((lcg() % 100) < 10)                              // glide notes
    vDriftT[v] = ((lcg() & 1) ? 1 : -1) * (0.06 + (lcg() % 10) * 0.012);
}

function filterTrack(v) {                              // L1523
  let trk = (vPhaseInc[v] * SR) / 220;
  if (trk < 0.5) trk = 0.5;
  if (trk > 2.6) trk = 2.6;
  vFc[v] *= trk; vFcT[v] *= trk;
  if (vFc[v] > 0.9) vFc[v] = 0.9;
  if (vFcT[v] > 0.9) vFcT[v] = 0.9;
}

// shared start path of keyNoteOn / fireNote up to filterTrack, plus the rate conversion
function startNote(v, note, gen, now) {
  const b = v * 5;
  for (let j = 0; j < 5; j++) vPhase[b + j] = 0;
  voiceCharacter(v, gen);
  vgate_t[v] = now;
  vPhaseInc[v] = FLYD[note] * 0.5 / SR;                // low octave, lidar unhooked (L1540)
  filterTrack(v);
  vAtk[v] = pcx(vAtk[v]); vFcR[v] = pcx(vFcR[v]);
  vFc[v] = fcv(vFc[v]); vFcT[v] = fcv(vFcT[v]);
}

let kn_rr = 0;
function keyNoteOn(k, now) {                           // L1530
  let v = -1;
  for (let i = 0; i < 12; i++) if (vEnvT[i] === 0 && vEnv[i] < 0.02) { v = i; break; }
  if (v < 0) { v = kn_rr; kn_rr = (kn_rr + 1) % 12; }
  key_voice[k] = v;
  kprs_t[k] = now;
  const note = (3 - (k >> 2)) * 4 + (k & 3);
  startNote(v, note, false, now);
  vPrise[v] = (g_prise > 0 && (lcg() % 9) < g_prise) ? 1 : 0;
  vPmul[v] = vPrise[v] ? 0.5 : 1;
  vRel[v] = pcx(0.0004);                               // prime manual release
  if (live_nn < 4) live_nt[live_nn++] = note;          // latch for stutter/BZZ registers
  vPan[v] = 0.2 + 0.6 * (k & 3) / 3;
  vEnvT[v] = 0.9;
}
function keyNoteOff(k) {                               // L1549
  if (key_voice[k] >= 0) { vEnvT[key_voice[k]] = 0; key_voice[k] = -1; }
}
function allKeysOff() { for (let k = 0; k < 16; k++) keyNoteOff(k); }

function fireNote(note, now) {                         // L1791
  let v = nextv;
  for (let i2 = 0; i2 < 12; i2++) {
    if (!vplay[(nextv + i2) % 12]) { v = (nextv + i2) % 12; break; }
  }
  nextv = (v + 1) % 12;
  startNote(v, note, true, now);
  vPrise[v] = (g_prise > 0 && (lcg() % 9) < g_prise) ? 1 : 0;
  vPmul[v] = vPrise[v] ? 0.5 : 1;
  vEnvT[v] = 0.6 + (lcg() % 35) / 100;
  vPan[v] = 0.15 + (lcg() % 71) / 100;
  const rr = (lcg() % 64) / 63;
  vRel[v] = pcx(0.000048 * Math.pow(30, rr));
  vplay[v] = 1;
  vrel[v] = now + 100;
}

// ---------------------------------------------------------------- drums
function drumTrig(v, f, amp, dec, noiz, drop, pan, part) {             // L1565
  dgoF[v] = f; dgoAmp[v] = amp; dgoDec[v] = dec; dgoNoiz[v] = noiz; dgoDrop[v] = drop;
  dgoPan[v] = pan; dgoPart[v] = part;
  dgoStamp[v]++;
}
function drumPadHit(k) {                                                // L1644
  k &= 15;
  const d = kit_sel ? KIT2[k] : KIT[k];
  let f = d[0], pan = d[5];
  if (f <= 1) {                                        // modular blip: random pitch + pan
    f = 300 + (lcg() % 2100);
    pan = 0.1 + 0.8 * (lcg() % 100) / 99;
  }
  if (k >= 12) {                                       // kicks retune to the key
    f = 43.65 * Math.pow(2, g_key / 12) * (d[0] / 52);
  }
  drumTrig(3 - (k >> 2), f, d[1], d[2], d[3], d[4], pan, d[6]);
  live_dr |= (1 << (3 - (k >> 2)));
}

// ---------------------------------------------------------------- sequencer (gridTick, L1678)
const NOTES = new Int32Array(8);
const DIVS = [2, 3, 4, 6, 8];

function fxOn(b) { return (fx_eff[0] >> b) & 1; }

function stepContent(pos, fire, rtHold, now) {                          // L1819
  pos &= (MAXSTEPS - 1);
  let dmask = 0;
  for (let v = 0; v < 4; v++) if (seqBit(dr_pat, v * 16, pos)) dmask |= (1 << v);
  let nv = 0;
  const sm = seq_mode_syn;
  if (sm === 2) {                                      // GEN
    if ((lcg() % 9) < g_csyn) {
      nv = 1; NOTES[0] = lcg() % 28;
      for (let e = 1; e < 4; e++) {
        if ((lcg() % 9) < g_cpoly) NOTES[nv++] = lcg() % 28;
        else break;
      }
    }
  } else if (sm === 0) {                               // SEQ
    const m = syn_seq[pos];
    for (let p = 0; p < 16 && nv < 8; p++)
      if ((m >> p) & 1) NOTES[nv++] = (3 - (p >> 2)) * 4 + (p & 3);
  }
  if (fire) {
    if (sm === 1) {                                    // ARP: random pad from the union
      let uni = 0; const sl2 = songLen();
      for (let q = 0; q < sl2; q++) uni |= syn_seq[q];
      nv = 0;
      if (uni) {
        let cnt = 0; for (let p = 0; p < 16; p++) cnt += (uni >> p) & 1;
        let pick = lcg() % cnt;
        for (let p = 0; p < 16; p++) if ((uni >> p) & 1) {
          if (pick-- === 0) { NOTES[0] = (3 - (p >> 2)) * 4 + (p & 3); nv = 1; break; }
        }
      }
    }
    for (let v = 0; v < 4; v++)
      if ((dmask & (1 << v)) && (lcg() % 9) < g_cdrum) drumPadHit(dr_pad[v] & 15);
    for (let n = 0; n < nv; n++) fireNote(NOTES[n], now);
  }
  const newdr = dmask | live_dr;
  if (newdr || !rtHold) rt_dr = newdr;
  if (nv === 0 && live_nn > 0) {
    nv = live_nn > 8 ? 8 : live_nn;
    for (let n = 0; n < nv; n++) NOTES[n] = live_nt[n];
  }
  if (nv || !rtHold) {
    rt_nv = nv;
    for (let n = 0; n < nv; n++) rt_notes[n] = NOTES[n];
  }
  live_dr = 0; live_nn = 0;
}

function stutFire(now) {
  for (let v = 0; v < 4; v++) if (rt_dr & (1 << v)) drumPadHit(dr_pad[v] & 15);
  for (let n = 0; n < rt_nv && n < 8; n++) fireNote(rt_notes[n], now);
}

function gridTick(now) {
  if (!vinit) { vinit = true; next_step = now + step_ms; }

  for (let v = 0; v < 12; v++) {
    if (vplay[v] && now >= vrel[v]) { vEnvT[v] = 0; vplay[v] = 0; }
    if (vEnvT[v] > 0 && now - vgate_t[v] > 8000) vEnvT[v] = 0;         // absolute gate cap
  }
  {                                                    // orphan sweep (L1697)
    let owned = 0;
    for (let k2 = 0; k2 < 16; k2++) if (key_voice[k2] >= 0) owned |= (1 << key_voice[k2]);
    for (let v = 0; v < 12; v++)
      if (vEnvT[v] > 0 && !vplay[v] && !((owned >> v) & 1)) vEnvT[v] = 0;
  }
  for (let k2 = 0; k2 < 16; k2++) {                    // stuck-note / stuck-FX safety (L1708)
    if (key_voice[k2] >= 0 && (!((held_mask >> k2) & 1) || now - kprs_t[k2] > 15000)) keyNoteOff(k2);
    if (((fx_held[0] >> k2) & 1) && now - fx_t[k2] > 20000) fx_held[0] &= ~(1 << k2);
  }
  {                                                    // effective FX per bank (L1716)
    const gs = gstep & 15;
    let liveAny = 0, autoAny = 0;
    for (let b = 0; b < 8; b++) {
      const lv = fx_held[b];
      fx_eff[b] = lv ? lv : fx_auto[b * 16 + gs];
      liveAny |= lv; autoAny |= fx_eff[b];
    }
    fx_fsr_eff = liveAny ? fx_fsr : (autoAny ? fx_auto_fsr[gs] / 255 : fx_fsr);
  }
  if (rec_slot < 0 && micOk) {                         // sampler hold -> record arming (L1738)
    for (let k2 = 0; k2 < 16; k2++) {
      if (smp_press[k2] && now - smp_press[k2] > 300) {
        rec_pos = 0; rec_slot = k2; smp_press[k2] = 0;
        break;
      }
    }
  }
  if (fxOn(FX_WASH)) {                                 // L1751
    const wc = pcq(0.00012 - fx_fsr * 0.00008);
    for (let v = 0; v < 12; v++) vRel[v] = wc;
  }

  const half_on = fxOn(FX_HALF), dbl_on = fxOn(FX_DBL), stp_on = fxOn(FX_STOP);
  fx_vs = stp_on ? 0.06 : (half_on ? 0.5 : (dbl_on ? 2 : 1));
  {
    const bent = half_on || dbl_on;
    if (bent && !bent_prev) bstep = gstep;
    bent_prev = bent;
  }

  let rt_per = 0;                                      // L1777
  if (fxOn(FX_ST4)) rt_per = step_ms * 4;
  if (fxOn(FX_ST8)) rt_per = step_ms * 2;
  if (fxOn(FX_ST16)) rt_per = step_ms;
  if (fxOn(FX_ST32)) rt_per = step_ms * 0.5;
  if (rt_per > 0 && rt_per < 6) rt_per = 6;
  const st_on = rt_per > 0;
  const bz_on = fxOn(FX_BZZ);
  const rtHold = st_on || bz_on;

  if (now >= next_step) {                              // MASTER CLOCK (L1883)
    step_t = next_step;
    {
      const sw = g_swing / 9 * 0.34;
      const lng = Math.trunc(step_ms * (1 + sw));
      const sht = step_ms * 2 - lng;
      next_step += (gstep & 1) ? lng : sht;
    }
    if (now >= next_step) next_step = now + step_ms;
    sct++;
    { const sl = songLen(); gstep = gstep + 1; if (gstep >= sl) gstep = 0; }
    step_rw = rb_w;
    stepEdge = true;

    for (let s = 0; s < 16; s++)
      if (seqBit(smp_pat, s * 16, gstep) && smp_len[s] && (lcg() % 9) < g_csmp) smp_trig[s]++;

    if (fx_rec) {                                      // FX automation write head (L1908)
      const fb = fx_bank & 7;
      fx_auto[fb * 16 + (gstep & 15)] = fx_held[fb];
      fx_auto_fsr[gstep & 15] = Math.trunc(fx_fsr * 255);
      dirty++;
    }
    if (syn_live && seq_mode_syn === 0) { syn_seq[gstep] = held_mask; dirty++; }   // L1918

    if (half_on) {
      if ((sct & 1) === 0) { bstep = (bstep + 1) & 15; stepContent(bstep, !st_on && !stp_on, rtHold, now); }
    } else if (dbl_on) {
      bstep = (bstep + 1) & 15;
      stepContent(bstep, !st_on && !stp_on, rtHold, now);
      next_dbl = step_t + (step_ms >> 1);
    } else stepContent(gstep, !st_on && !stp_on, rtHold, now);
  }

  if (dbl_on && next_dbl && now >= next_dbl) {         // DBL mid-step (L1938)
    next_dbl = 0;
    bstep = (bstep + 1) & 15;
    stepContent(bstep, !st_on && !stp_on, rtHold, now);
  }
  if (!dbl_on) next_dbl = 0;

  {                                                    // STUTTER scheduler (L1953)
    if (st_on && !stp_on) {
      let psteps = Math.trunc(rt_per / step_ms + 0.5); if (psteps < 1) psteps = 1;
      const sub16 = rt_per < step_ms * 0.9;
      if (rt_prev !== rt_per) { rt_prev = rt_per; rt_armed = true; rt_half = 0; }
      if (rt_lastg !== gstep) {
        rt_lastg = gstep;
        if ((gstep % psteps) === 0) {
          rt_armed = false;
          stutFire(now);
          if (sub16) rt_half = step_t + (step_ms >> 1);
        }
      }
      if (!rt_armed && sub16 && rt_half && now >= rt_half) { rt_half = 0; stutFire(now); }
    } else { rt_prev = 0; rt_armed = false; rt_half = 0; rt_lastg = -1; }
  }

  {                                                    // BZZ (L1983)
    if (bz_on && !stp_on) {
      if (bz_sct !== sct) {
        bz_sct = sct;
        const idx = (lcg() % 3) + Math.trunc(fx_fsr * 2.49);   // 0..4, live FSR (SPEC 12.8)
        bz_iv = Math.floor(step_ms / DIVS[idx]);
        if (bz_iv < 6) bz_iv = 6;
        next_bz = step_t;
        while (next_bz <= now) next_bz += bz_iv;
      }
      if (now >= next_bz) {
        next_bz += bz_iv;
        if (now >= next_bz) next_bz = now + bz_iv;
        stutFire(now);
      }
    } else bz_sct = 0xFFFFFFFF;
  }
}

// ---------------------------------------------------------------- controls
function songFix() {
  const sl = songLen();
  if (gstep >= sl) gstep = 0;
}
function setStepMs(v) { step_ms = clampi(v, 60, 500); }

function setParam(name, v) {
  switch (name) {
    case 'wave': g_wave = clampi(v, 0, 12); break;
    case 'dens': g_csyn = clampi(v, 0, 9); break;
    case 'poly': g_cpoly = clampi(v, 0, 9); break;
    case 'vsyn': g_vsyn = clampi(v, 0, 12); break;
    case 'wtscan': g_wtscan = clampi(v, 0, 15); break;
    case 'prise': g_prise = clampi(v, 0, 9); break;
    case 'bars': form_bars = clampi(v, 1, 16); songFix(); break;
    case 'beats': form_blen = clampi(v, 1, 16); songFix(); break;
    case 'kit': kit_sel = clampi(v, 0, 1); break;
    case 'cdrum': g_cdrum = clampi(v, 0, 9); break;
    case 'vdrum': g_vdrum = clampi(v, 0, 12); break;
    case 'csmp': g_csmp = clampi(v, 0, 9); break;
    case 'vsmp': g_vsmp = clampi(v, 0, 12); break;
    case 'step_ms': setStepMs(v); break;
    case 'key': g_key = clampi(v, -24, 24); rebuildScale(); break;
    case 'mode': g_mode = clampi(v, 0, 8); rebuildScale(); break;
    case 'swing': g_swing = clampi(v, 0, 9); break;
    case 'mvol_idx': vol_pidx = clampi(v, 0, 15); mvol = KVOL[vol_pidx]; return;   // global, not persisted
    case 'fsr': { let t = (+v - 400) / 2200; fx_fsr = t < 0 ? 0 : t > 1 ? 1 : t; return; }   // L4829
    case 'lid': { let L = (+v) / 600; fx_lid = L < 0 ? 0 : L > 1 ? 1 : (L === L ? L : 0); return; }
    case 'ax': ax_target = +v || 0; ax_pending = Math.abs(ax_target - axf) > 1e-4; return;   // an unchanged value never moves the boot filter (L4779)
    default: return;
  }
  dirty++;
}

// the firmware's globAdj / engAdj / songAdj step for each parameter (L677-705)
function adjustParam(name, d) {
  d = d < 0 ? -1 : 1;
  switch (name) {
    case 'wave': g_wave = (g_wave + 13 + d) % 13; break;
    case 'dens': g_csyn = clampi(g_csyn + d, 0, 9); break;
    case 'poly': g_cpoly = clampi(g_cpoly + d, 0, 9); break;
    case 'vsyn': g_vsyn = clampi(g_vsyn + d, 0, 12); break;
    case 'wtscan': g_wtscan = clampi(g_wtscan + d, 0, 15); break;
    case 'prise': g_prise = clampi(g_prise + d, 0, 9); break;
    case 'bars': form_bars = clampi(form_bars + d, 1, 8); songFix(); break;
    case 'beats': form_blen = clampi(form_blen + d, 1, 16); songFix(); break;
    case 'kit': kit_sel = (kit_sel + 2 + d) & 1; break;
    case 'cdrum': g_cdrum = clampi(g_cdrum + d, 0, 9); break;
    case 'vdrum': g_vdrum = clampi(g_vdrum + d, 0, 12); break;
    case 'csmp': g_csmp = clampi(g_csmp + d, 0, 9); break;
    case 'vsmp': g_vsmp = clampi(g_vsmp + d, 0, 12); break;
    case 'bpm': case 'step_ms': {
      const bpm = clampi(Math.floor(15000 / step_ms) + d, 30, 250);
      step_ms = Math.floor(15000 / bpm);
    } break;
    case 'key': g_key = clampi(g_key + d, -24, 24); rebuildScale(); break;
    case 'mode': g_mode = (g_mode + 9 + d) % 9; rebuildScale(); break;
    case 'swing': g_swing = clampi(g_swing + d, 0, 9); break;
    case 'mvol_idx': vol_pidx = clampi(vol_pidx + d, 0, 15); mvol = KVOL[vol_pidx]; return;
    default: return;
  }
  dirty++;
}

function tapTempo(ms) {                                                  // L4677
  const g = ms - tap_prev; tap_prev = ms;
  if (g >= 240 && g <= 2000) {
    tap_gaps[tap_i] = g; tap_i = (tap_i + 1) & 3; if (tap_n < 4) tap_n++;
    let s = 0; for (let i = 0; i < tap_n; i++) s += tap_gaps[i];
    let st = Math.floor(Math.floor(s / tap_n) / 4);
    if (st < 60) st = 60;
    if (st > 500) st = 500;
    step_ms = st; dirty++;
  } else { tap_n = 0; tap_i = 0; }
}

function tiltUpdate(now) {                             // L4787: runs only while the UI feeds 'ax'
  if (!ax_pending || now < ax_next) return;
  ax_next = now + 50;
  axf += (ax_target - axf) * 0.35;
  let t = (0.9 - axf) / 1.1;
  if (t > 1) t = 1;
  if (t < 0) t = 0;
  const fc = 500 * Math.pow(10, t);
  filter_f = 2 * Math.sin(PI_F * fc / SR);
  if (Math.abs(ax_target - axf) < 1e-4) ax_pending = false;
}

function samplerRelease(k) {                                             // L4485
  if (rec_slot === k) {
    smp_len[k] = rec_pos; rec_slot = -1; sel_smp = k; dirty++;
  } else if (smp_press[k]) {
    if (smp_len[k]) smp_trig[k]++;
    sel_smp = k;
  }
  smp_press[k] = 0;
}

// ---------------------------------------------------------------- FX state (L2070 onwards)
// All "static" locals of the firmware's audio callback live in per-bank objects.
let DUB_N, CMB_N, F3PLT_N, F3PNG_N, F3SLP_N, F4REP_N, F4MTL_N, B_RP2, B_RP6, B_FILL, B_BNC,
  B_SCR, B_F6D, B_ECHO, B_LOOP, B_PCH, B_FB, B_DT, B_PLFO, B_BU, B_BD, B_WRB, B_F7DL, B_H5,
  FLN, PLAPN, HBN, VBN, CBN, REV_FADE;
let f2dub, f2cmb, f3plt, f3pngL, f3pngR, f3slap, f4rep, f4mtl, f5rpt2, f5repL, f5repR, f5fill,
  f5bnc, f5scrL, f5scrR, f6dL, f6dR, f6ppL, f6ppR, f6echo, f6loopL, f6loopR, f6pchL, f6pchR,
  f6fb, f6dtL, f6dtR, f6plfo, f6bu, f6bd, f6wrb, f7dl;
let FC;                                                // rate-scaled coefficient table

function initFx() {
  const fa = (n) => new Float32Array(n);
  DUB_N = nsc(32768); CMB_N = nsc(4096); F3PLT_N = nsc(24000); F3PNG_N = nsc(96000);
  F3SLP_N = nsc(12000); F4REP_N = nsc(48000); F4MTL_N = nsc(4096);
  B_RP2 = nsc(6000); B_RP6 = nsc(24000); B_FILL = nsc(4000); B_BNC = nsc(8000); B_SCR = nsc(48000);
  B_F6D = nsc(48000); B_ECHO = nsc(24000); B_LOOP = nsc(96000); B_PCH = nsc(12000);
  B_FB = nsc(4096); B_DT = nsc(2048); B_PLFO = nsc(4096); B_BU = nsc(4096); B_BD = nsc(4096);
  B_WRB = nsc(4096); B_F7DL = nsc(2048); B_H5 = nsc(8192);
  FLN = nsc(1024); PLAPN = nsc(512); HBN = nsc(256); VBN = nsc(1024); CBN = nsc(1024);
  REV_FADE = nsm(480);

  f2dub = fa(DUB_N); f2cmb = fa(CMB_N); f3plt = fa(F3PLT_N); f3pngL = fa(F3PNG_N); f3pngR = fa(F3PNG_N);
  f3slap = fa(F3SLP_N); f4rep = fa(F4REP_N); f4mtl = fa(F4MTL_N);
  f5rpt2 = fa(B_RP2); f5repL = fa(B_RP6); f5repR = fa(B_RP6); f5fill = fa(B_FILL); f5bnc = fa(B_BNC);
  f5scrL = fa(B_SCR); f5scrR = fa(B_SCR);
  f6dL = fa(B_F6D); f6dR = fa(B_F6D); f6ppL = fa(B_F6D); f6ppR = fa(B_F6D); f6echo = fa(B_ECHO);
  f6loopL = fa(B_LOOP); f6loopR = fa(B_LOOP); f6pchL = fa(B_PCH); f6pchR = fa(B_PCH);
  f6fb = fa(B_FB); f6dtL = fa(B_DT); f6dtR = fa(B_DT); f6plfo = fa(B_PLFO); f6bu = fa(B_BU);
  f6bd = fa(B_BD); f6wrb = fa(B_WRB); f7dl = fa(B_F7DL);

  FC = {
    mute: pcx(0.004), dubt: pcx(0.0004), iso1: pcx(0.026), iso2: pcx(0.30), wow: pcx(0.5),
    wenvA: pcx(0.05), wenvR: pcx(0.002), warm: pcx(0.45), sub1: pcx(0.05), sub2: pcx(0.01),
    air: pcx(0.6), pump: pcx(0.05), ou: pcx(0.002), rad1: pcx(0.35), rad2: pcx(0.05),
    ngA: pcx(0.4), ngR: pcx(0.004), shm: pcx(0.02), ns: pcx(0.01), puA: pcx(0.5), puR: pcx(0.002),
    ef: pcx(0.0008), bend: pcx(0.0006), g02: pcx(0.02), g03: pcx(0.03), g006: pcx(0.006),
    g05: pcx(0.05), g06: pcx(0.06), tu: pcx(0.25),
    ap241: nsm(241), shd: nsm(4799), spd2400: nsm(2400), n360: nsm(360), n12000: nsm(12000),
    n8000: nsm(8000), n6000: nsm(6000), n3000: nsm(3000), n4000: nsm(4000),
  };

  F2 = {
    since: 0, lastg: -1, dubw: 0, dubt: 9000 * K, gvw: 0, genv: 0, fbuf: fa(FLN), fw: 0, flf: 0, cw2: 0,
    v1: 0, v2: 0, v1b: 0, v2b: 0, xl: 0, xh: 0, xlR: 0, xhR: 0, ph_h: 0, ph_l: 0, ph_hR: 0, ph_lR: 0,
    gpos: 0, wphs: 0, wphf: 0, wd: 0, dhL: 0, dhR: 0, dhc: 0, scbase: 0, scg: -1,
  };
  F3 = {
    since: 0, lastg: -1, plw: 0, paw: 0, plapbuf: fa(PLAPN), shw: 0, shfb: 0, shsgn: 1, pngw: 0, slw: 0,
    phl: new Float64Array(4), phr: new Float64Array(4), php: 0,
    waf: 0, wab: 0, wafR: 0, wabR: 0, wenv: 0, trp: 0, hbuf: fa(HBN), hbw: 0,
    vbuf: fa(VBN), vbw: 0, vbp: 0, cbuf: fa(CBN), cbw: 0, cbp: 0,
    spa: new Float64Array(3), spaR: new Float64Array(3), spw: 0, sirp: 0, sirm: 0, wlpL: 0, wlpR: 0,
    subLP: 0, subph: 0, subenv: 0, airL: 0, airR: 0, pumpenv: 1,
  };
  F4 = {
    since: 0, lastg: -1, rng4: 0xC0FFEE, ouL: 0, ouR: 0, odpL: 0, odpR: 0, odfL: 1, odfR: 1,
    dzL: 0, dzR: 0, dzc: 0, rpw: 0, rpbase: 0, rpg: -1, rgp: 0, rglen: nsm(4800), fzp: 0, fzlen: nsm(2400),
    fzcap: false, mtw: 0, qa: 0, qb: 0, qaR: 0, qbR: 0, fsp: 0, gpat: 0xACE1, gseqg: -1, srbase: 0, srg: -1,
    srrd: 0, vsrd: 0, vsinit: false, rdh: 0, rdl: 0, rdhR: 0, rdlR: 0, rdcar: 0, ngenv: 0,
    shval: 0, shlp: 0, shc: 0,
  };
  F5 = {
    since: 0, lastg: -1, rng5: 0x1234abcd, wp2: 0, wp6: 0, envNs: 1, phQ: 0, envPu: 0, wpFill: 0,
    hpB: 0, phB: 0, wpBnc: 0, tBnc: 0, perBnc: 4000 * K, z1: 0, z2: 0, zh: 0, wpScr: 0, rpScr: 0, phScr: 0,
    rpSf: 0, phSf: 0, wpSf: 0, wpGl: 0, holdGl: 0, lenGl: nsm(2000), phBl: 0, hL: 0, hR: 0, cSh: 0,
    perSh: nsm(200),
  };
  F6 = {
    wpD: 0, wpPp: 0, wpEf: 0, fbEf: 0, wpFb: 0, wpDr: 0, frozen: 0, lenDr: nsm(12000), aFz: 0,
    wpU: 0, rpU: 0, wpUl: 0, rpUl: 0, wpDt: 0, p1: 0, p2: 0, wpPl: 0, phPl: 0, wpBu: 0, spdBu: 1, rpBu: 0,
    wpBd: 0, spdBd: 1, rpBd: 0, wpWr: 0, phWr: 0, wpH5: 0, rpH5: 0, prevSub: 0, flipSub: 1,
    ppLp: 0, capLp: 1, lenLp: nsm(48000),
  };
  F7 = { e4: 1, e8: 1, e16: 1, eTr: 1, phTrem: 0, phPan: 0, eSg: 1, wpW: 0, lpL: 0, lpR: 0, tdL: 0, tdR: 0 };
  F8 = {
    r2: 1, r3: 1, r4: 1, r8: 1, eT2: 1, phRl: 0, phRh: 0, phVb: 0, stChop: -1, lvChop: 1, eHg: 1, eDg: 1,
    stGl: -1, lvGl: 1,
  };
  FM = { rphase: 0, mlp: 0, mbp: 0, mlpR: 0, mbpR: 0, mlfo: 0 };
  hatlp = 0;
}

// ---------------------------------------------------------------- FX2 bank (L2450)
const PENT = [130.8, 146.8, 164.8, 196, 220, 261.6, 293.7, 329.6];
function fx2() {
  const h2 = fx_eff[1], s = blkS, lid = blkLid, f = F2;
  if (f.lastg !== gstep) { f.lastg = gstep; f.since = 0; }
  // DUB
  if (h2 & 1) {
    const tt = nsm(3840) + lid * (DUB_N - nsm(5000));
    f.dubt += (tt - f.dubt) * FC.dubt;
    let dl = Math.trunc(f.dubt); if (dl > DUB_N - 2) dl = DUB_N - 2;
    const e = f2dub[(f.dubw + DUB_N - dl) % DUB_N];
    const fb = 0.45 + s * 0.6;
    const inp = (oL + oR) * 0.5;
    let wr = inp + e * fb; wr = Math.tanh(wr * 0.8) * 1.25;
    f2dub[f.dubw] = wr; f.dubw = (f.dubw + 1) % DUB_N;
    oL += e * 0.9; oR += e * 0.9;
  } else { f2dub[f.dubw] = (oL + oR) * 0.5; f.dubw = (f.dubw + 1) % DUB_N; }
  // GVERB (shares f2cmb with COMB, as the firmware does)
  if (h2 & 2) {
    const inp = (oL + oR) * 0.5; let rv = 0;
    for (let c = 0; c < 4; c++) rv += f2cmb[(f.gvw + CMB_N - GVL[c]) % CMB_N];
    rv *= 0.25;
    f2cmb[f.gvw] = inp + rv * 0.72; f.gvw = (f.gvw + 1) % CMB_N;
    f.genv = (rv * rv > f.genv) ? rv * rv : f.genv * dcy(0.99988 - s * 0.0003);
    const g = (f.genv > 0.00005) ? 1 : 0;
    oL = oL * 0.4 + rv * g * 1.6; oR = oR * 0.4 + rv * g * 1.6;
  }
  // TGATE
  if (h2 & 4) {
    const st = (s > 0.6) ? ((gstep * 2 + Math.floor(f.since / (Math.floor(step_ms * 24 * K) + 1))) & 15) : (gstep & 15);
    const on = ((0xB6D6 >> st) & 1) ? 1 : (0.6 - s * 0.6);
    oL *= on; oR *= on;
  }
  // FLANG
  if (h2 & 8) {
    const ctr = 6 - 5.4 * s, rate = 0.15 + 6 * (s * s);
    f.flf += rate / SR; if (f.flf >= 1) f.flf -= 1;
    const lfo = 0.5 - 0.5 * Math.cos(f.flf * TWO_PI_F);
    const dms = (ctr + (0.5 + 4.5 * s) * lfo) * SPMS;
    const inp = (oL + oR) * 0.5;
    let rd = f.fw - dms; while (rd < 0) rd += FLN;
    const ri = Math.trunc(rd), fr = rd - ri;
    const a = f.fbuf[ri % FLN], b = f.fbuf[(ri + 1) % FLN];
    const wet = a + (b - a) * fr;
    f.fbuf[f.fw] = inp + wet * 0.7; f.fw = (f.fw + 1) % FLN;
    oL = oL * 0.5 + wet; oR = oR * 0.5 + wet;
  } else { f.fbuf[f.fw] = (oL + oR) * 0.5; f.fw = (f.fw + 1) % FLN; }
  // RING lives in the master chain
  // FOLD
  if (h2 & 32) {
    const g = 1 + s * 6;
    oL = Math.sin(oL * g * 1.5707) * 0.8; oR = Math.sin(oR * g * 1.5707) * 0.8;
  }
  // COMB
  if (h2 & 64) {
    const cf = PENT[Math.trunc(lid * 7.99)];
    let dl = Math.trunc(SR / cf); if (dl >= CMB_N) dl = CMB_N - 1;
    const r = f2cmb[(f.cw2 + CMB_N - dl) % CMB_N];
    const fb = 0.8 + s * 0.19;
    const inp = (oL + oR) * 0.5;
    f2cmb[f.cw2] = inp + r * fb; f.cw2 = (f.cw2 + 1) % CMB_N;
    oL = oL * 0.5 + r * 0.7; oR = oR * 0.5 + r * 0.7;
  }
  // VOWEL
  if (h2 & 128) {
    const f1 = 300 + s * 700, f2f = 900 + s * 1700;
    const inp = (oL + oR) * 0.5;
    const c1 = 2 * Math.sin(3.14159 * f1 / SR), c2 = 2 * Math.sin(3.14159 * f2f / SR);
    f.v1 += c1 * (inp - f.v1 - 0.2 * f.v1b); f.v1b += c1 * f.v1;
    f.v2 += c2 * (inp - f.v2 - 0.2 * f.v2b); f.v2b += c2 * f.v2;
    const wet = (f.v1b + f.v2b) * 0.9;
    oL = oL * 0.25 + wet; oR = oR * 0.25 + wet;
  }
  // ISO trio
  if (h2 & 0x700) {
    const a1 = FC.iso1, a2 = FC.iso2;
    f.xl += a1 * (oL - f.xl); f.xh += a2 * (oL - f.xh);
    const lo = f.xl, hi = oL - f.xh, mid = f.xh - f.xl;
    f.xlR += a1 * (oR - f.xlR); f.xhR += a2 * (oR - f.xhR);
    const loR = f.xlR, hiR = oR - f.xhR, midR = f.xhR - f.xlR;
    let gl = 1, gm = 1, gh = 1;
    if (h2 & 0x100) gl = 1 - s;
    if (h2 & 0x200) gm = 1 - s;
    if (h2 & 0x400) gh = 1 - s;
    oL = lo * gl + mid * gm + hi * gh;
    oR = loR * gl + midR * gm + hiR * gh;
  }
  // PHONE
  if (h2 & 0x800) {
    const la = pcq(0.30 - s * 0.12), ha = pcq(0.04 + s * 0.04);
    f.ph_h += ha * (oL - f.ph_h); const b = oL - f.ph_h; f.ph_l += la * (b - f.ph_l);
    f.ph_hR += ha * (oR - f.ph_hR); const bR = oR - f.ph_hR; f.ph_lR += la * (bR - f.ph_lR);
    const q = 8 - s * 5;
    oL = (Math.floor(f.ph_l * q) / q) * 1.4; oR = (Math.floor(f.ph_lR * q) / q) * 1.4;
  }
  // GRAIN: indexes the ring by absolute position (SPEC 12.9, kept)
  if (h2 & 0x1000) {
    let glen = Math.trunc((5760 - s * 5400) * K); const gmin = nsm(240); if (glen < gmin) glen = gmin;
    const base = Math.trunc(lid * (RBUF_N - glen - 2));
    f.gpos++; if (f.gpos >= glen) f.gpos = 0;
    const rp = (base + f.gpos) % RBUF_N;
    const w = 0.5 - 0.5 * Math.cos(TWO_PI_F * f.gpos / glen);
    oL = rbufL[rp] * w + oL * (1 - w) * 0.3;
    oR = rbufR[rp] * w + oR * (1 - w) * 0.3;
  }
  f.since++;
  // WOW
  if (h2 & 0x2000) {
    f.wphs += 0.7 / SR; if (f.wphs >= 1) f.wphs -= 1;
    f.wphf += 8 / SR; if (f.wphf >= 1) f.wphf -= 1;
    const mod = (psin(f.wphs) * 0.6 + psin(f.wphf) * 0.4) * s * 0.4;
    f.wd += (mod - f.wd) * FC.wow;
    arng = (Math.imul(arng, 1664525) + 1013904223) >>> 0;
    let crk = (arng >>> 9) * (1 / 4194304) - 1;
    crk = (Math.abs(crk) > 0.985) ? crk * s * 0.5 : 0;
    oL = oL * (1 + f.wd) + crk; oR = oR * (1 + f.wd) + crk;
  }
  // DOWN
  if (h2 & 0x4000) {
    let hold = 2 + Math.trunc(s * 40); if (K !== 1) hold = Math.max(1, Math.round(hold * K));
    if (f.dhc === 0) { f.dhL = oL; f.dhR = oR; }
    f.dhc = (f.dhc + 1) % hold; oL = f.dhL; oR = f.dhR;
  } else f.dhc = 0;
  // SCAT
  if (h2 & 0x8000) {
    let slice = Math.trunc(step_ms * SPMS);
    const smin = nsm(480); if (slice < smin) slice = smin; if (slice > (RBUF_N >> 1)) slice = RBUF_N >> 1;
    if (f.scg !== gstep) {
      f.scg = gstep;
      if ((lcg() % 100) < Math.trunc(20 + s * 70)) f.scbase = (lcg() % 8) * slice;
    }
    let rp = (step_rw + RBUF_N - (f.scbase + (f.since % slice))) % RBUF_N; if (rp < 0) rp += RBUF_N;
    const w = (s > 0.05) ? 1 : 0;
    oL = rbufL[rp] * w + oL * (1 - w);
    oR = rbufR[rp] * w + oR * (1 - w);
  }
}

// ---------------------------------------------------------------- FX3 bank (L2608)
function fx3() {
  const h3 = fx_eff[2], s = blkS, lid = blkLid, f = F3;
  if (f.lastg !== gstep) { f.lastg = gstep; f.since = 0; }
  // PLATE
  if (h3 & 1) {
    const inp = (oL + oR) * 0.5; let rv = 0; const dec = 0.74 + s * 0.21;
    for (let c = 0; c < 4; c++) rv += f3plt[(f.plw + F3PLT_N - PLC[c]) % F3PLT_N];
    rv *= 0.25; f3plt[f.plw] = inp + rv * dec; f.plw = (f.plw + 1) % F3PLT_N;
    const ar = f.plapbuf[(f.paw + PLAPN - FC.ap241) % PLAPN];
    const aw = rv + ar * 0.5; f.plapbuf[f.paw] = aw; f.paw = (f.paw + 1) % PLAPN;
    const wet = ar - aw * 0.5; oL = oL * 0.5 + wet; oR = oR * 0.5 + wet;
  }
  // SHIMR (per-sample sign flip is the firmware's, L2628)
  if (h3 & 2) {
    const inp = (oL + oR) * 0.5;
    const t = f3plt[(f.shw + F3PLT_N - FC.shd) % F3PLT_N];
    f.shsgn = -f.shsgn; const oct = Math.abs(t) * f.shsgn;
    f.shfb = inp + (t * 0.6 + oct * (0.4 + s * 0.45));
    f3plt[f.shw] = Math.tanh(f.shfb * 0.8); f.shw = (f.shw + 1) % F3PLT_N;
    oL = oL * 0.5 + t * 0.8; oR = oR * 0.5 + t * 0.8;
  }
  // PONG
  if (h3 & 4) {
    let dl = Math.trunc(step_ms * SPMS * 1.5);
    if (dl >= F3PNG_N) dl = F3PNG_N - 1; const dmin = nsm(480); if (dl < dmin) dl = dmin;
    const eL = f3pngR[(f.pngw + F3PNG_N - dl) % F3PNG_N];
    const eR = f3pngL[(f.pngw + F3PNG_N - dl) % F3PNG_N];
    const fb = 0.35 + lid * 0.6;
    f3pngL[f.pngw] = Math.tanh(oL + eL * fb); f3pngR[f.pngw] = Math.tanh(oR + eR * fb);
    f.pngw = (f.pngw + 1) % F3PNG_N;
    const m = 0.3 + s * 0.6; oL = oL * (1 - m * 0.5) + eL * m; oR = oR * (1 - m * 0.5) + eR * m;
  } else { f3pngL[f.pngw] = oL; f3pngR[f.pngw] = oR; f.pngw = (f.pngw + 1) % F3PNG_N; }
  // SLAP
  if (h3 & 8) {
    let dl = Math.trunc((2880 + s * 9000) * K); if (dl >= F3SLP_N) dl = F3SLP_N - 1;
    const e = f3slap[(f.slw + F3SLP_N - dl) % F3SLP_N]; const inp = (oL + oR) * 0.5;
    f3slap[f.slw] = inp + e * 0.25; f.slw = (f.slw + 1) % F3SLP_N;
    oL = oL + e * 0.55; oR = oR + e * 0.55;
  } else { f3slap[f.slw] = (oL + oR) * 0.5; f.slw = (f.slw + 1) % F3SLP_N; }
  // PHASE
  if (h3 & 16) {
    const rate = 0.08 + s * 1.6; f.php += rate / SR; if (f.php >= 1) f.php -= 1;
    const g = 0.15 + 0.7 * (0.5 - 0.5 * Math.cos(f.php * TWO_PI_F));
    let xL = oL, xR = oR;
    for (let k = 0; k < 4; k++) {
      const yL = -g * xL + f.phl[k]; f.phl[k] = xL + g * yL; xL = yL;
      const yR = -g * xR + f.phr[k]; f.phr[k] = xR + g * yR; xR = yR;
    }
    oL = oL * 0.5 + xL * 0.5; oR = oR * 0.5 + xR * 0.5;
  }
  // WAH
  if (h3 & 32) {
    const inp = (oL + oR) * 0.5, a = Math.abs(inp);
    f.wenv += (a - f.wenv) * ((a > f.wenv) ? FC.wenvA : FC.wenvR);
    let fc = 300 + (f.wenv * (2 + s * 6)) * 3000; if (fc > 5500) fc = 5500;
    const ff = 2 * Math.sin(3.14159 * fc / SR), q = 0.30;
    f.waf += ff * (inp - f.waf - q * f.wab); f.wab += ff * f.waf;
    f.wafR += ff * (oR - f.wafR - q * f.wabR); f.wabR += ff * f.wafR;
    oL = oL * 0.3 + f.wab * 1.4; oR = oR * 0.3 + f.wabR * 1.4;
  }
  // TREM
  if (h3 & 64) {
    const div = (s > 0.6) ? 8 : 4;
    const rate = div / (step_ms * 0.001 * 16);
    f.trp += rate / SR; if (f.trp >= 1) f.trp -= 1;
    const depth = 0.4 + s * 0.6;
    const g = 1 - depth * (0.5 - 0.5 * Math.cos(f.trp * TWO_PI_F));
    oL *= g; oR *= g;
  }
  // HAAS
  if (h3 & 128) {
    f.hbuf[f.hbw] = oR; const d = Math.trunc((8 + s * 22) * K);
    const dr = f.hbuf[(f.hbw + HBN - d) % HBN]; f.hbw = (f.hbw + 1) % HBN;
    const m = (oL + oR) * 0.5;
    oL = m + (oL - m) * (1 + s * 0.6); oR = m + (dr - m) * (1 + s * 0.6);
  }
  // VIBRA
  if (h3 & 256) {
    f.vbuf[f.vbw] = (oL + oR) * 0.5; f.vbp += 5.5 / SR; if (f.vbp >= 1) f.vbp -= 1;
    const dms = (12 + (4 + s * 22) * (0.5 - 0.5 * Math.cos(f.vbp * TWO_PI_F))) * K;
    let rd = f.vbw - dms; while (rd < 0) rd += VBN;
    const ri = Math.trunc(rd), fr = rd - ri;
    const a = f.vbuf[ri % VBN], b = f.vbuf[(ri + 1) % VBN];
    f.vbw = (f.vbw + 1) % VBN; const w = a + (b - a) * fr; oL = w; oR = w;
  } else { f.vbuf[f.vbw] = (oL + oR) * 0.5; f.vbw = (f.vbw + 1) % VBN; }
  // DIMEN
  if (h3 & 512) {
    f.cbuf[f.cbw] = (oL + oR) * 0.5; f.cbp += 0.6 / SR; if (f.cbp >= 1) f.cbp -= 1;
    const l1 = 0.5 - 0.5 * Math.cos(f.cbp * TWO_PI_F);
    const l2 = 0.5 - 0.5 * Math.cos((f.cbp + 0.25) * TWO_PI_F);
    const d1 = (14 + (2 + s * 14) * l1) * K, d2 = (14 + (2 + s * 14) * l2) * K;
    let r1 = f.cbw - d1; while (r1 < 0) r1 += CBN; const i1 = Math.trunc(r1), f1 = r1 - i1;
    let r2 = f.cbw - d2; while (r2 < 0) r2 += CBN; const i2 = Math.trunc(r2), f2c = r2 - i2;
    const w1 = f.cbuf[i1 % CBN] + (f.cbuf[(i1 + 1) % CBN] - f.cbuf[i1 % CBN]) * f1;
    const w2 = f.cbuf[i2 % CBN] + (f.cbuf[(i2 + 1) % CBN] - f.cbuf[i2 % CBN]) * f2c;
    f.cbw = (f.cbw + 1) % CBN; oL = oL * 0.6 + w1 * 0.7; oR = oR * 0.6 + w2 * 0.7;
  } else { f.cbuf[f.cbw] = (oL + oR) * 0.5; f.cbw = (f.cbw + 1) % CBN; }
  // SPRNG (f3slap shared with SLAP)
  if (h3 & 1024) {
    const inp = (oL + oR) * 0.5; const t = f3slap[(f.spw + F3SLP_N - FC.spd2400) % F3SLP_N];
    const g = 0.6; let xL = inp + t * (0.5 + s * 0.4), xR = xL;
    for (let k = 0; k < 3; k++) {
      const yL = -g * xL + f.spa[k]; f.spa[k] = xL + g * yL; xL = yL;
      const yR = -g * xR + f.spaR[k]; f.spaR[k] = xR + g * yR; xR = yR;
    }
    f3slap[f.spw] = Math.tanh(xL * 0.9); f.spw = (f.spw + 1) % F3SLP_N;
    oL = oL * 0.5 + xL * 0.9; oR = oR * 0.5 + xR * 0.9;
  }
  // SIREN
  if (h3 & 2048) {
    f.sirm += 6 / SR; if (f.sirm >= 1) f.sirm -= 1;
    const fr = 180 + lid * 900 + psin(f.sirm) * 60 * s;
    f.sirp += fr / SR; if (f.sirp >= 1) f.sirp -= 1;
    const t = Math.tanh(psin(f.sirp) * 2.2); const lvl = 0.25 + s * 0.5;
    oL = oL * 0.7 + t * lvl; oR = oR * 0.7 + t * lvl;
  }
  // WARM
  if (h3 & 4096) {
    const dr = 1 + s * 4;
    const yL = Math.tanh(oL * dr) * (1 / (1 + s * 1.5));
    const yR = Math.tanh(oR * dr) * (1 / (1 + s * 1.5));
    f.wlpL += FC.warm * (yL - f.wlpL); f.wlpR += FC.warm * (yR - f.wlpR);
    oL = yL * 0.5 + f.wlpL * 0.5; oR = yR * 0.5 + f.wlpR * 0.5;
  }
  // SUB
  if (h3 & 8192) {
    const inp = (oL + oR) * 0.5; f.subLP += FC.sub1 * (inp - f.subLP);
    const a = Math.abs(f.subLP); f.subenv += (a - f.subenv) * FC.sub2;
    f.subph += 0.0006 * IK; if (f.subph >= 1) f.subph -= 1;
    const sub = psin(f.subph) * f.subenv * (2 + s * 3); oL += sub; oR += sub;
  }
  // AIR
  if (h3 & 16384) {
    f.airL += FC.air * (oL - f.airL); const hL = oL - f.airL;
    f.airR += FC.air * (oR - f.airR); const hR = oR - f.airR;
    oL += Math.tanh(hL * 3) * 0.5 * (0.3 + s * 0.9);
    oR += Math.tanh(hR * 3) * 0.5 * (0.3 + s * 0.9);
  }
  // PUMP
  if (h3 & 32768) {
    let beat = Math.trunc(step_ms * SPMS * 4); const bmin = nsm(480); if (beat < bmin) beat = bmin;
    const pos = (f.since + gstep * Math.trunc(step_ms * SPMS)) % beat;
    const t = pos / beat;
    const duck = 1 - (0.3 + s * 0.65) * Math.exp(-t * 6);
    f.pumpenv += (duck - f.pumpenv) * FC.pump; oL *= f.pumpenv; oR *= f.pumpenv;
  }
  f.since++;
}

// ---------------------------------------------------------------- FX4 bank (L2757)
function fx4() {
  const h4 = fx_eff[3], s = blkS, f = F4;
  if (f.lastg !== gstep) { f.lastg = gstep; f.since = 0; }
  // OCT+
  if (h4 & 1) {
    const rL = Math.abs(oL) * 2 - 0.5, rR = Math.abs(oR) * 2 - 0.5;
    f.ouL += FC.ou * (rL - f.ouL); f.ouR += FC.ou * (rR - f.ouR);
    oL = oL * (1 - s) + (rL - f.ouL) * s * 1.2; oR = oR * (1 - s) + (rR - f.ouR) * s * 1.2;
  }
  // OCT-
  if (h4 & 2) {
    const sL = oL > 0 ? 1 : 0; if (sL !== f.odpL) { f.odpL = sL; if (sL) f.odfL = -f.odfL; }
    const sR = oR > 0 ? 1 : 0; if (sR !== f.odpR) { f.odpR = sR; if (sR) f.odfR = -f.odfR; }
    oL = oL * (1 - s) + Math.abs(oL) * f.odfL * s * 1.3; oR = oR * (1 - s) + Math.abs(oR) * f.odfR * s * 1.3;
  }
  // DSTRY
  if (h4 & 4) {
    let hold = 1 + Math.trunc(s * 30); if (K !== 1) hold = Math.max(1, Math.round(hold * K));
    if (f.dzc === 0) { f.dzL = oL; f.dzR = oR; } f.dzc = (f.dzc + 1) % hold;
    const q = Math.pow(2, 8 - s * 7); oL = Math.floor(f.dzL * q) / q; oR = Math.floor(f.dzR * q) / q;
  } else f.dzc = 0;
  // REPT
  if (h4 & 8) {
    let slc = Math.trunc(step_ms * SPMS * 0.5);
    const smin = nsm(240); if (slc < smin) slc = smin; if (slc > (F4REP_N >> 1)) slc = F4REP_N >> 1;
    if (f.rpg !== gstep) {
      f.rpg = gstep; f.rng4 = (Math.imul(f.rng4, 1664525) + 1013904223) >>> 0;
      if ((f.rng4 >>> 24) < Math.trunc(40 + s * 200)) f.rpbase = ((f.rng4 >>> 8) % 4) * slc;
    }
    let rd = (f.rpw + F4REP_N - (f.rpbase + (f.since % slc))) % F4REP_N; if (rd < 0) rd += F4REP_N;
    oL = f4rep[rd]; oR = f4rep[rd];
  }
  f4rep[f.rpw] = (oL + oR) * 0.5; f.rpw = (f.rpw + 1) % F4REP_N;
  // RGRN
  if (h4 & 16) {
    f.rglen = Math.trunc((6000 - s * 5500) * K); const rmin = nsm(300); if (f.rglen < rmin) f.rglen = rmin;
    f.rgp++; if (f.rgp >= f.rglen) f.rgp = 0;
    const rd = (f.rpw + F4REP_N - f.rgp) % F4REP_N;
    const w = 0.5 - 0.5 * Math.cos(TWO_PI_F * f.rgp / f.rglen);
    oL = f4rep[rd] * w + oL * (1 - w) * 0.3; oR = f4rep[rd] * w + oR * (1 - w) * 0.3;
  }
  // FREEZ
  if (h4 & 32) {
    if (!f.fzcap) { f.fzcap = true; f.fzp = 0; }
    f.fzlen = nsm(600) + Math.trunc((1 - s) * 4800 * K);
    const rd = (f.rpw + F4REP_N - f.fzlen + (f.fzp % f.fzlen)) % F4REP_N; f.fzp++;
    const w = 0.5 - 0.5 * Math.cos(TWO_PI_F * (f.fzp % f.fzlen) / f.fzlen);
    oL = f4rep[rd] * (0.6 + w * 0.4); oR = f4rep[rd] * (0.6 + w * 0.4);
  } else f.fzcap = false;
  // METAL
  if (h4 & 64) {
    const inp = (oL + oR) * 0.5; let rv = 0; const scale = 0.6 + s * 0.8;
    for (let c = 0; c < 4; c++) {
      let d = Math.trunc(MTC[c] * K * scale); if (d >= F4MTL_N) d = F4MTL_N - 1;
      rv += f4mtl[(f.mtw + F4MTL_N - d) % F4MTL_N];
    }
    rv *= 0.25; f4mtl[f.mtw] = inp + rv * 0.85; f.mtw = (f.mtw + 1) % F4MTL_N;
    oL = oL * 0.3 + rv * 1.3; oR = oR * 0.3 + rv * 1.3;
  }
  // FRMNT
  if (h4 & 128) {
    const sh = 1 + s * 1.6; const f1 = 500 * sh; let f2f = 1500 * sh;
    if (f2f > 6000) f2f = 6000; const inp = (oL + oR) * 0.5;
    const c1 = 2 * Math.sin(3.14159 * f1 / SR), c2 = 2 * Math.sin(3.14159 * f2f / SR);
    f.qa += c1 * (inp - f.qa - 0.18 * f.qb); f.qb += c1 * f.qa;
    f.qaR += c2 * (inp - f.qaR - 0.18 * f.qbR); f.qbR += c2 * f.qaR;
    const wet = (f.qb + f.qbR) * 0.9; oL = oL * 0.3 + wet; oR = oR * 0.3 + wet;
  }
  // FSHFT
  if (h4 & 256) {
    const fc = 20 + s * s * 900; f.fsp += fc / SR; if (f.fsp >= 1) f.fsp -= 1;
    const c = psin(f.fsp), cq = psin(f.fsp + 0.25);
    const wL = oL * c - oR * cq, wR = oR * c + oL * cq;
    oL = oL * 0.3 + wL * 0.9; oR = oR * 0.3 + wR * 0.9;
  }
  // FUZZ
  if (h4 & 512) {
    const g = 2 + s * 18; let yL = oL * g + 0.15, yR = oR * g + 0.15;
    yL = (yL > 0) ? (1 - Math.exp(-yL)) : (-1 + Math.exp(yL));
    yR = (yR > 0) ? (1 - Math.exp(-yR)) : (-1 + Math.exp(yR));
    oL = yL * 0.6; oR = yR * 0.6;
  }
  // GSEQ
  if (h4 & 1024) {
    if (f.gseqg !== gstep && gstep === 0) {
      f.rng4 = (Math.imul(f.rng4, 1664525) + 1013904223) >>> 0; f.gpat = (f.rng4 >>> 8) & 0xFFFF;
    }
    f.gseqg = gstep; const thr = Math.trunc(s * 16);
    const on = (((f.gpat >> (gstep & 15)) & 1) || ((gstep & 3) < thr)) ? 1 : 0;
    oL *= on; oR *= on;
  }
  // SRAMP
  if (h4 & 2048) {
    let slc = Math.trunc(step_ms * SPMS * 0.5); const smin = nsm(480); if (slc < smin) slc = smin;
    if (f.srg !== gstep) { f.srg = gstep; f.srbase = (f.rpw + F4REP_N - slc) % F4REP_N; f.srrd = 0; }
    const spd = 1 + s * 1.5 * (f.since / (step_ms * SPMS + 1));
    f.srrd += spd; if (f.srrd >= slc) f.srrd -= slc;
    const rd = (f.srbase + Math.trunc(f.srrd)) % F4REP_N; oL = f4rep[rd]; oR = f4rep[rd];
  }
  // VSTOP
  if (h4 & 4096) {
    if (!f.vsinit) { f.vsinit = true; f.vsrd = 0; }
    const spd = 0.4 + s * 2.5; f.vsrd += spd; let back = f.vsrd;
    if (back > F4REP_N - 2) back = F4REP_N - 2;
    const rd = (f.rpw + F4REP_N - Math.trunc(back)) % F4REP_N; const damp = 1 - s * 0.5;
    oL = f4rep[rd] * damp; oR = f4rep[rd] * damp;
  } else f.vsinit = false;
  // RADIO
  if (h4 & 8192) {
    const la = FC.rad1, ha = FC.rad2;
    f.rdh += ha * (oL - f.rdh); const bL = oL - f.rdh; f.rdl += la * (bL - f.rdl);
    f.rdhR += ha * (oR - f.rdhR); const bR = oR - f.rdhR; f.rdlR += la * (bR - f.rdlR);
    f.rdcar += (60 + s * 200) / SR; if (f.rdcar >= 1) f.rdcar -= 1;
    const am = 0.6 + 0.4 * psin(f.rdcar);
    f.rng4 = (Math.imul(f.rng4, 1664525) + 1013904223) >>> 0;
    const hiss = ((f.rng4 >>> 9) * (1 / 4194304) - 1) * s * 0.15;
    oL = f.rdl * am * 1.6 + hiss; oR = f.rdlR * am * 1.6 + hiss;
  }
  // NGATE
  if (h4 & 16384) {
    const a = Math.abs(oL) + Math.abs(oR);
    f.ngenv += (a - f.ngenv) * ((a > f.ngenv) ? FC.ngA : FC.ngR);
    const g = (f.ngenv > (0.02 + s * 0.5)) ? 1 : 0; oL *= g; oR *= g;
  }
  // SHMOD
  if (h4 & 32768) {
    let rate = 2400 - Math.trunc(s * 2200); if (rate < 60) rate = 60;
    if (K !== 1) rate = Math.max(1, Math.round(rate * K));
    if (f.shc === 0) { f.rng4 = (Math.imul(f.rng4, 1664525) + 1013904223) >>> 0; f.shval = (f.rng4 >>> 9) * (1 / 4194304) - 1; }
    f.shc = (f.shc + 1) % rate; f.shlp += FC.shm * (f.shval - f.shlp);
    const mod = 0.5 + 0.5 * f.shlp; oL *= mod; oR *= mod;
  } else f.shc = 0;
  f.since++;
}

// ---------------------------------------------------------------- FX5 bank (L2889)
function fx5() {
  const h5 = fx_eff[4], s = blkS, lid = blkLid, f = F5;
  if (f.lastg !== gstep) { f.lastg = gstep; f.since = 0; }
  const sms = step_ms * SPMS;
  // RPT2
  if (h5 & 1) {
    let slc = Math.trunc(sms * 0.5 * (0.4 + 0.6 * s)); const lo = nsm(64);
    if (slc < lo) slc = lo; if (slc > B_RP2 - 1) slc = B_RP2 - 1;
    f5rpt2[f.wp2] = oL; let rp = f.wp2 - (f.since % slc); if (rp < 0) rp += B_RP2;
    oL = oR = f5rpt2[rp]; if (++f.wp2 >= B_RP2) f.wp2 = 0;
  }
  // RPT6
  if (h5 & 2) {
    let len = Math.trunc(sms * (0.5 + 1.0 * s)); const lo = nsm(128);
    if (len < lo) len = lo; if (len > B_RP6 - 1) len = B_RP6 - 1;
    f5repL[f.wp6] = oL; f5repR[f.wp6] = oR; const ph = f.since % len; let rp = f.wp6 - ph; if (rp < 0) rp += B_RP6;
    oL = f5repL[rp]; oR = f5repR[rp]; if (++f.wp6 >= B_RP6) f.wp6 = 0;
  }
  // SHUF
  if (h5 & 4) {
    const sw = Math.trunc(sms * 0.33 * s);
    const g = (f.since < sw && (gstep & 1)) ? 0 : 1;
    f.envNs += (g - f.envNs) * FC.ns; oL *= f.envNs; oR *= f.envNs;
  }
  // Q6/8
  if (h5 & 8) {
    const rate = 3 / (step_ms * 0.001 * 2);
    f.phQ += rate / SR; if (f.phQ >= 1) f.phQ -= 1;
    let g = 0.5 + 0.5 * psin(f.phQ); g = 1 - s * (1 - g); oL *= g; oR *= g;
  }
  // PUNCH
  if (h5 & 16) {
    const inp = Math.abs(oL) + Math.abs(oR); f.envPu += (inp - f.envPu) * ((inp > f.envPu) ? FC.puA : FC.puR);
    const g = 1 + s * 2.0 * (1 / (1 + 8 * f.envPu)); oL = Math.tanh(oL * g); oR = Math.tanh(oR * g);
  }
  // FILL
  if (h5 & 32) {
    let slc = Math.trunc(sms * 0.25 * (1.0 - 0.7 * s)); const lo = nsm(48);
    if (slc < lo) slc = lo; if (slc > B_FILL - 1) slc = B_FILL - 1;
    f5fill[f.wpFill] = oL; let rp = f.wpFill - (f.since % slc); if (rp < 0) rp += B_FILL;
    oL = oR = f5fill[rp]; if (++f.wpFill >= B_FILL) f.wpFill = 0;
  }
  // BUILD
  if (h5 & 64) {
    const fc = pcq(0.02 + 0.45 * lid); f.hpB += (oL - f.hpB) * fc; const h = oL - f.hpB;
    oL = oR = h; f.phB += (4 + lid * 40) / SR; if (f.phB >= 1) f.phB -= 1;
    const g = (f.phB < (0.5 + 0.5 * lid)) ? 1 : 0.2; oL *= g; oR *= g;
  }
  // BALL
  if (h5 & 128) {
    f5bnc[f.wpBnc] = oL; f.tBnc += 1;
    if (f.tBnc >= f.perBnc) { f.tBnc = 0; f.perBnc *= 0.7; if (f.perBnc < 400 * K) f.perBnc = 4000 * K * (0.5 + 0.5 * s); }
    let rp = f.wpBnc - Math.trunc(f.tBnc); if (rp < 0) rp += B_BNC;
    const dec = 1 - (f.tBnc / f.perBnc) * 0.8; oL = oR = f5bnc[rp] * dec; if (++f.wpBnc >= B_BNC) f.wpBnc = 0;
  }
  // LPF
  if (h5 & 256) {
    const fc = pcq(0.03 + 0.5 * (1 - s)); f.z1 += fc * (oL - f.z1); f.z2 += fc * (f.z1 - f.z2); oL = f.z2; oR = f.z2;
  }
  // HPF
  if (h5 & 512) {
    const fc = pcq(0.02 + 0.4 * s); f.zh += fc * (oL - f.zh); oL = oR = oL - f.zh;
  }
  // SQUSH
  if (h5 & 1024) {
    const g = 1 + s * 10, k = 0.7 / (0.3 + s); oL = Math.tanh(oL * g) * k; oR = Math.tanh(oR * g) * k;
  }
  // SCRTC (jog LFO increments are /480 in the firmware, kept)
  if (h5 & 2048) {
    f5scrL[f.wpScr] = oL; f5scrR[f.wpScr] = oR; f.phScr += (2 + s * 4) / 480 * IK; if (f.phScr >= 1) f.phScr -= 1;
    f.rpScr += psin(f.phScr) * 60 * (0.5 + s); let rr = f.wpScr - f.rpScr;
    while (rr < 0) rr += B_SCR; while (rr >= B_SCR) rr -= B_SCR;
    const ri = Math.trunc(rr); oL = f5scrL[ri]; oR = f5scrR[ri]; if (++f.wpScr >= B_SCR) f.wpScr = 0;
  }
  // SCRTF
  if (h5 & 4096) {
    f5scrL[f.wpSf] = oL; f.phSf += (8 + s * 16) / 480 * IK; if (f.phSf >= 1) f.phSf -= 1;
    f.rpSf += psin(f.phSf) * 40; let rr = f.wpSf - f.rpSf;
    while (rr < 0) rr += B_SCR; while (rr >= B_SCR) rr -= B_SCR;
    oL = oR = f5scrL[Math.trunc(rr)]; if (++f.wpSf >= B_SCR) f.wpSf = 0;
  }
  // GLCH
  if (h5 & 8192) {
    f5scrR[f.wpGl] = oL;
    if (--f.holdGl <= 0) {
      f.rng5 = (Math.imul(f.rng5, 1664525) + 1013904223) >>> 0;
      if (((f.rng5 >>> 20) & 255) < Math.trunc(s * 255)) { f.lenGl = nsm(300 + ((f.rng5 >>> 8) & 8191)); f.holdGl = f.lenGl * 2; }
      else f.holdGl = nsm(200);
    }
    if (f.holdGl > 0 && f.lenGl) { let rp = f.wpGl - (f.since % f.lenGl); if (rp < 0) rp += B_SCR; oL = oR = f5scrR[rp]; }
    if (++f.wpGl >= B_SCR) f.wpGl = 0;
  }
  // BLIND
  if (h5 & 16384) {
    f.phBl += (8 + s * 60) / SR; if (f.phBl >= 1) f.phBl -= 1;
    const g = (f.phBl < 0.5) ? 1 : 0; oL *= g; oR *= g;
  }
  // S&H
  if (h5 & 32768) {
    if (--f.cSh <= 0) {
      f.rng5 = (Math.imul(f.rng5, 1664525) + 1013904223) >>> 0;
      f.perSh = Math.max(1, Math.round((20 + Math.trunc((1 - s) * 1500)) * K)); f.cSh = f.perSh; f.hL = oL; f.hR = oR;
    }
    oL = f.hL; oR = f.hR;
  }
  f.since++;
}

// ---------------------------------------------------------------- FX6 bank (L2977)
function fx6() {
  const h6 = fx_eff[5], s = blkS, lid = blkLid, f = F6;
  const sms = step_ms * SPMS;
  // DELAY
  if (h6 & 1) {
    let dt = Math.trunc(sms * 2); if (dt < 1) dt = 1; if (dt > B_F6D - 1) dt = B_F6D - 1;
    let rp = f.wpD - dt; if (rp < 0) rp += B_F6D; const dl = f6dL[rp], dr = f6dR[rp]; const fb = 0.2 + 0.55 * s;
    f6dL[f.wpD] = Math.tanh(oL + dl * fb); f6dR[f.wpD] = Math.tanh(oR + dr * fb); oL += dl * 0.6; oR += dr * 0.6;
    if (++f.wpD >= B_F6D) f.wpD = 0;
  }
  // PNGPG
  if (h6 & 2) {
    let dt = Math.trunc(sms * 2); if (dt < 1) dt = 1; if (dt > B_F6D - 1) dt = B_F6D - 1;
    let rp = f.wpPp - dt; if (rp < 0) rp += B_F6D; const dl = f6ppL[rp], dr = f6ppR[rp]; const fb = 0.3 + 0.55 * s;
    f6ppL[f.wpPp] = Math.tanh(oL + dr * fb); f6ppR[f.wpPp] = Math.tanh(oR + dl * fb); oL += dl * 0.6; oR += dr * 0.6;
    if (++f.wpPp >= B_F6D) f.wpPp = 0;
  }
  // FADE
  if (h6 & 4) {
    f.fbEf += (0.85 - f.fbEf) * FC.ef;
    let dt = Math.trunc(sms * (1 + s * 2)); if (dt < 1) dt = 1; if (dt > B_ECHO - 1) dt = B_ECHO - 1;
    let rp = f.wpEf - dt; if (rp < 0) rp += B_ECHO; const d = f6echo[rp];
    f6echo[f.wpEf] = Math.tanh(oL + d * f.fbEf); oL += d * 0.6; oR += d * 0.6; if (++f.wpEf >= B_ECHO) f.wpEf = 0;
  }
  // FBACK
  if (h6 & 8) {
    const dt = Math.trunc((200 + Math.trunc((1 - s) * 1400)) * K);
    let rp = f.wpFb - dt; if (rp < 0) rp += B_FB; const d = f6fb[rp]; const fb = 0.85 + 0.13 * s;
    f6fb[f.wpFb] = Math.tanh(oL * 0.5 + d * fb); oL = oR = oL * 0.3 + d * 0.8; if (++f.wpFb >= B_FB) f.wpFb = 0;
  }
  // DRONE (captures once, then loops forever: firmware statics, kept)
  if (h6 & 16) {
    if (!f.frozen) { f6loopL[f.wpDr] = oL; f6loopR[f.wpDr] = oR; if (++f.wpDr >= f.lenDr) { f.frozen = 1; f.wpDr = 0; } }
    else {
      const gL = f6loopL[f.wpDr], gR = f6loopR[f.wpDr]; if (++f.wpDr >= f.lenDr) f.wpDr = 0;
      oL = oL * (1 - lid) + gL * lid; oR = oR * (1 - lid) + gR * lid;
    }
  }
  // FREEZ
  if (h6 & 32) {
    let ra = f.aFz - FC.n8000; if (ra < 0) ra += B_ECHO;
    const fa = f6echo[ra]; const fb = 0.9 + 0.099 * s;
    f6echo[f.aFz] = Math.tanh((oL + oR) * 0.5 * (1 - s) + fa * fb);
    oL = oL * (1 - s) + fa * s; oR = oR * (1 - s) + fa * s; if (++f.aFz >= B_ECHO) f.aFz = 0;
  }
  // UNI
  if (h6 & 64) {
    f6pchL[f.wpU] = oL; f6pchR[f.wpU] = oR;
    f.rpU += 1 + 0.012 * s; if (f.rpU >= FC.n6000) f.rpU -= FC.n6000; let rd = f.wpU - Math.trunc(f.rpU); if (rd < 0) rd += B_PCH;
    oL = oL * 0.6 + f6pchL[rd] * 0.6; oR = oR * 0.6 + f6pchR[rd] * 0.6; if (++f.wpU >= B_PCH) f.wpU = 0;
  }
  // UNILO
  if (h6 & 128) {
    f6pchR[f.wpUl] = oL;
    f.rpUl += 1 - 0.02 * s; if (f.rpUl < 0) f.rpUl += FC.n6000; if (f.rpUl >= FC.n6000) f.rpUl -= FC.n6000;
    let rd = f.wpUl - Math.trunc(f.rpUl); if (rd < 0) rd += B_PCH;
    oL = oL * 0.6 + f6pchR[rd] * 0.6; oR = oR * 0.6 + f6pchR[rd] * 0.6; if (++f.wpUl >= B_PCH) f.wpUl = 0;
  }
  // DETUN
  if (h6 & 256) {
    f.p1 += 0.7 / SR; f.p2 += 0.9 / SR; if (f.p1 >= 1) f.p1 -= 1; if (f.p2 >= 1) f.p2 -= 1;
    f6dtL[f.wpDt] = oL; f6dtR[f.wpDt] = oR;
    const d1 = (300 + 250 * s * psin(f.p1)) * K, d2 = (300 + 250 * s * psin(f.p2)) * K;
    let a = (f.wpDt - Math.trunc(d1)) % B_DT; if (a < 0) a += B_DT;
    let b = (f.wpDt - Math.trunc(d2)) % B_DT; if (b < 0) b += B_DT;
    oL = oL * 0.5 + f6dtL[a] * 0.5; oR = oR * 0.5 + f6dtR[b] * 0.5; if (++f.wpDt >= B_DT) f.wpDt = 0;
  }
  // PLFO
  if (h6 & 512) {
    f.phPl += 6 / SR; if (f.phPl >= 1) f.phPl -= 1; f6plfo[f.wpPl] = oL;
    const d = (1000 + 900 * s * psin(f.phPl)) * K; const ip = Math.trunc(d), fr = d - ip;
    let r0 = (f.wpPl - ip) % B_PLFO; if (r0 < 0) r0 += B_PLFO; let r1 = (f.wpPl - ip - 1) % B_PLFO; if (r1 < 0) r1 += B_PLFO;
    oL = oR = f6plfo[r0] + (f6plfo[r1] - f6plfo[r0]) * fr; if (++f.wpPl >= B_PLFO) f.wpPl = 0;
  }
  // BND+
  if (h6 & 1024) {
    f.spdBu += ((1 + s) - f.spdBu) * FC.bend; f6bu[f.wpBu] = oL; f.rpBu += f.spdBu; if (f.rpBu >= FC.n4000) f.rpBu -= FC.n4000;
    let rd = (f.wpBu - Math.trunc(f.rpBu)) % B_BU; if (rd < 0) rd += B_BU; oL = oR = f6bu[rd]; if (++f.wpBu >= B_BU) f.wpBu = 0;
  }
  // BND-
  if (h6 & 2048) {
    f.spdBd += ((1 - 0.6 * s) - f.spdBd) * FC.bend; f6bd[f.wpBd] = oL; f.rpBd += f.spdBd;
    if (f.rpBd < 0) f.rpBd += FC.n4000; if (f.rpBd >= FC.n4000) f.rpBd -= FC.n4000;
    let rd = (f.wpBd - Math.trunc(f.rpBd)) % B_BD; if (rd < 0) rd += B_BD; oL = oR = f6bd[rd]; if (++f.wpBd >= B_BD) f.wpBd = 0;
  }
  // WARBL
  if (h6 & 4096) {
    f.phWr += (7 + s * 12) / SR; if (f.phWr >= 1) f.phWr -= 1; f6wrb[f.wpWr] = oL;
    const d = (800 + 700 * psin(f.phWr) * s) * K; const ip = Math.trunc(d), fr = d - ip;
    let r0 = (f.wpWr - ip) % B_WRB; if (r0 < 0) r0 += B_WRB; let r1 = (f.wpWr - ip - 1) % B_WRB; if (r1 < 0) r1 += B_WRB;
    oL = oR = f6wrb[r0] + (f6wrb[r1] - f6wrb[r0]) * fr; if (++f.wpWr >= B_WRB) f.wpWr = 0;
  }
  // 5TH (indexes the first 8192 samples of f6pchL, as the firmware does)
  if (h6 & 8192) {
    f6pchL[f.wpH5] = oL;
    f.rpH5 += 0.5; if (f.rpH5 >= FC.n3000) f.rpH5 -= FC.n3000; let rd = (f.wpH5 - Math.trunc(f.rpH5)) % B_H5; if (rd < 0) rd += B_H5;
    const fifth = f6pchL[rd]; oL = oL * (1 - s * 0.5) + fifth * s * 0.7; oR = oL; if (++f.wpH5 >= B_H5) f.wpH5 = 0;
  }
  // SUB
  if (h6 & 16384) {
    if (f.prevSub <= 0 && oL > 0) f.flipSub = -f.flipSub; f.prevSub = oL;
    const sub = f.flipSub * Math.abs(oL); oL += sub * s; oR += sub * s;
  }
  // LOOP: the firmware writes then reads the same index, so audibly a pass-through (L3065); length latches once
  if (h6 & 32768) {
    if (f.capLp) {
      f.lenLp = Math.trunc(sms * 16 * (0.25 + 0.75 * s)); if (f.lenLp > B_LOOP) f.lenLp = B_LOOP;
      const lo = nsm(2400); if (f.lenLp < lo) f.lenLp = lo; f.capLp = 0;
    }
    f6loopL[f.ppLp] = oL; f6loopR[f.ppLp] = oR; oL = f6loopL[f.ppLp]; oR = f6loopR[f.ppLp];
    if (++f.ppLp >= f.lenLp) f.ppLp = 0;
  }
}

// ---------------------------------------------------------------- FX7 bank (L3069)
function fx7() {
  const h7 = fx_eff[6], s = blkS, f = F7;
  const sl = gslen;
  const gpos = gstep * sl + gstep_samp;
  let ph;
  if (h7 & 1) { ph = gpos / (sl * 4); ph -= Math.floor(ph); f.e4 += (((ph < 0.5) ? 1 : 0) - f.e4) * FC.g02; oL *= f.e4; oR *= f.e4; }
  if (h7 & 2) { ph = gpos / (sl * 2); ph -= Math.floor(ph); f.e8 += (((ph < 0.5) ? 1 : 0) - f.e8) * FC.g02; oL *= f.e8; oR *= f.e8; }
  if (h7 & 4) { ph = gpos / sl; ph -= Math.floor(ph); f.e16 += (((ph < 0.5) ? 1 : 0) - f.e16) * FC.g02; oL *= f.e16; oR *= f.e16; }
  if (h7 & 8) {
    const gt = ((0xB3A6 >> (gstep & 15)) & 1) ? 1 : 0; f.eTr += (gt - f.eTr) * FC.g03; oL *= f.eTr; oR *= f.eTr;
  }
  if (h7 & 16) {
    f.phTrem += 1 / (sl * 2); if (f.phTrem >= 1) f.phTrem -= 1;
    const d = 0.4 + 0.55 * s; const m = 1 - d * 0.5 * (1 + psin(f.phTrem)); oL *= m; oR *= m;
  }
  if (h7 & 32) {
    f.phPan += 1 / (sl * 4); if (f.phPan >= 1) f.phPan -= 1;
    const p = 0.5 + 0.5 * psin(f.phPan) * (0.4 + 0.6 * s); oL *= (1 - p) * 1.4; oR *= p * 1.4;
  }
  if (h7 & 64) { ph = gpos / (sl * 4); ph -= Math.floor(ph); const duck = 0.25 + 0.75 * ph; oL *= duck; oR *= duck; }
  if (h7 & 128) { ph = gpos / (sl * 4); ph -= Math.floor(ph); const sw = 0.25 + 0.75 * (1 - ph); oL *= sw; oR *= sw; }
  if (h7 & 256) {
    const n = ((lcg() & 0xffff) - 32768) / 32768; const a = 0.02 + 0.06 * s; oL += n * a; oR += n * a;
  }
  if (h7 & 512) {
    f7dl[f.wpW] = oR; let rd = f.wpW - FC.n360; if (rd < 0) rd += B_F7DL;
    oR = oR * 0.4 + f7dl[rd] * 0.7; if (++f.wpW >= B_F7DL) f.wpW = 0;
  }
  if (h7 & 1024) { const m = (oL + oR) * 0.5; oL = m; oR = m; }
  if (h7 & 2048) {
    f.lpL += (oL - f.lpL) * FC.tu; f.lpR += (oR - f.lpR) * FC.tu;
    oL += (oL - f.lpL) * (0.6 + s); oR += (oR - f.lpR) * (0.6 + s);
  }
  if (h7 & 4096) {
    const c = pcq(0.35 - 0.25 * s); f.tdL += (oL - f.tdL) * c; f.tdR += (oR - f.tdR) * c; oL = f.tdL; oR = f.tdR;
  }
  if (h7 & 8192) { const g = 1 + 4 * s; oL = Math.tanh(oL * g) * 0.8; oR = Math.tanh(oR * g) * 0.8; }
  if (h7 & 16384) {
    ph = gpos / (sl * 2); ph -= Math.floor(ph); const gt = (ph < 0.5) ? 1 : 0;
    f.eSg += (gt - f.eSg) * FC.g006; oL *= f.eSg; oR *= f.eSg;
  }
  if (h7 & 32768) { oL = 0; oR = 0; }
}

// ---------------------------------------------------------------- FX8 bank (L3127)
function fx8() {
  const h8 = fx_eff[7], s = blkS, f = F8;
  const sl = gslen;
  const gpos = gstep * sl + gstep_samp;
  let ph;
  if (h8 & 1) { ph = gpos / (sl * 0.5); ph -= Math.floor(ph); f.r2 += (((ph < 0.5) ? 1 : 0) - f.r2) * FC.g05; oL *= f.r2; oR *= f.r2; }
  if (h8 & 2) { ph = gpos / (sl * 0.3333); ph -= Math.floor(ph); f.r3 += (((ph < 0.5) ? 1 : 0) - f.r3) * FC.g05; oL *= f.r3; oR *= f.r3; }
  if (h8 & 4) { ph = gpos / (sl * 0.25); ph -= Math.floor(ph); f.r4 += (((ph < 0.5) ? 1 : 0) - f.r4) * FC.g05; oL *= f.r4; oR *= f.r4; }
  if (h8 & 8) { ph = gpos / (sl * 0.125); ph -= Math.floor(ph); f.r8 += (((ph < 0.5) ? 1 : 0) - f.r8) * FC.g05; oL *= f.r8; oR *= f.r8; }
  if (h8 & 16) { ph = gpos / (sl * 2); ph -= Math.floor(ph); oL *= ph; oR *= ph; }
  if (h8 & 32) {
    const gt = ((0xC5B2 >> (gstep & 15)) & 1) ? 1 : 0; f.eT2 += (gt - f.eT2) * FC.g03; oL *= f.eT2; oR *= f.eT2;
  }
  if (h8 & 64) { ph = gpos / sl; ph -= Math.floor(ph); let a = 1 - ph; a *= a; oL *= a; oR *= a; }
  if (h8 & 128) {
    f.phRl += 90 / SR; if (f.phRl >= 1) f.phRl -= 1; const c = psin(f.phRl);
    const m = 0.4 + 0.55 * s; oL = oL * (1 - m) + oL * c * m; oR = oR * (1 - m) + oR * c * m;
  }
  if (h8 & 256) {
    f.phRh += 360 / SR; if (f.phRh >= 1) f.phRh -= 1; const c = psin(f.phRh);
    const m = 0.4 + 0.55 * s; oL = oL * (1 - m) + oL * c * m; oR = oR * (1 - m) + oR * c * m;
  }
  if (h8 & 512) {                                      // 7 Hz plus the firmware's extra +0.0001/sample
    f.phVb += (7 / 48000 + 0.0001) * IK; if (f.phVb >= 1) f.phVb -= 1;
    const m = 1 - (0.2 + 0.3 * s) * 0.5 * (1 + psin(f.phVb)); oL *= m; oR *= m;
  }
  if (h8 & 1024) {
    if (gstep !== f.stChop) { f.stChop = gstep; f.lvChop = ((lcg() & 3) !== 0) ? 1 : 0; } oL *= f.lvChop; oR *= f.lvChop;
  }
  if (h8 & 2048) {
    ph = gpos / (sl * 8); ph -= Math.floor(ph); const gt = (ph < 0.5) ? 1 : 0;
    f.eHg += (gt - f.eHg) * FC.g02; oL *= f.eHg; oR *= f.eHg;
  }
  if (h8 & 4096) {
    ph = gpos / (sl * 0.5); ph -= Math.floor(ph); const gt = (ph < 0.5) ? 1 : 0;
    f.eDg += (gt - f.eDg) * FC.g06; oL *= f.eDg; oR *= f.eDg;
  }
  if (h8 & 8192) {
    const cs = Math.trunc(gpos / (sl * 0.5));
    if (cs !== f.stGl) { f.stGl = cs; f.lvGl = ((lcg() % 5) < 2) ? 0 : 1; } oL *= f.lvGl; oR *= f.lvGl;
  }
  if (h8 & 16384) { ph = gpos / sl; ph -= Math.floor(ph); oL *= ph * ph; oR *= ph * ph; }
  if (h8 & 32768) { oL = 0; oR = 0; }
}

// ---------------------------------------------------------------- audio block (AudioCallback, L2070)
// One 64-sample block. Voices, drums and sampler render voice-by-voice into
// scratch buffers (identical sums to the firmware's sample-outer loop, far
// cheaper); everything after the bus sums runs sample by sample in firmware order.
function renderBlock(inp, o0, o1, off, n, now) {
  gridTick(now);
  tiltUpdate(now);

  const Q1 = 0.8, drv = 2.0, mgain = 2.0;
  const fh = fx_eff[0];
  const ffsr = fx_fsr_eff;
  const crush = (fh >> FX_CRUSH) & 1;
  let cint = 4 + Math.trunc(ffsr * 20); if (K !== 1) cint = Math.max(1, Math.round(cint * K));
  const rev = (fh >> FX_REV) & 1;
  if (rev && !buf_prev) { fz_anchor = step_rw; rev_pos = 0; }
  buf_prev = rev === 1;
  let rw = rb_w;
  const revLe = Math.min(Math.floor(step_ms * 4 * SPMS), RBUF_N - nsm(64));
  const gvSyn = g_vsyn * 0.1, gvDrum = g_vdrum * 0.1, gvSmp = g_vsmp * 0.1;
  const mvol_l = mvol;
  if (gstep !== gstep_blk) { gstep_samp = 0; gstep_blk = gstep; }
  gslen = step_ms * SPMS; if (gslen < 1) gslen = 1;
  blkS = ffsr; blkLid = fx_lid;

  // NaN/inf guard on the always-on filter states (L2125)
  if (!(lpL * 0 === 0) || !(bpL * 0 === 0)) { lpL = 0; bpL = 0; }
  if (!(lpR * 0 === 0) || !(bpR * 0 === 0)) { lpR = 0; bpR = 0; }
  if (!(dlpL * 0 === 0) || !(dbpL * 0 === 0)) { dlpL = 0; dbpL = 0; }
  if (!(dlpR * 0 === 0) || !(dbpR * 0 === 0)) { dlpR = 0; dbpR = 0; }
  if (!(ff_s * 0 === 0)) ff_s = fcv(0.62);

  // varispeed slew, per sample
  {
    const vst = fx_vs; let vs = vss;
    for (let i = 0; i < n; i++) { vs += (vst - vs) * C_VSS; VSS[i] = vs; }
    vss = vs;
  }
  for (let i = 0; i < n; i++) { MIXL[i] = 0; MIXR[i] = 0; DRL[i] = 0; DRR[i] = 0; }

  // ---- 12 synth voices (L2154)
  const wtscan = g_wtscan, KPmax = KPN - 1;
  for (let v = 0; v < 12; v++) {
    if (vEnv[v] < 0.0004 && vEnvT[v] === 0) continue;
    const b = v * 5, type = vType[v], phaseInc = vPhaseInc[v], pan = vPan[v], atk = vAtk[v], rel = vRel[v];
    const q = vQ[v], fcR = vFcR[v], nosc = vNosc[v], prise = vPrise[v], fmiT = vFmiT[v], driftT = vDriftT[v];
    let env = vEnv[v], envT = vEnvT[v], drift = vDrift[v], fc = vFc[v], fcT = vFcT[v], lp = vLp[v], bp = vBp[v];
    let fmi = vFmi[v], pmul = vPmul[v];
    for (let i = 0; i < n; i++) {
      if (env < 0.0004 && envT === 0) break;           // idle voice: costs nothing
      env += (envT - env) * (envT > env ? atk : rel);
      drift += (driftT - drift) * C_DRIFT;
      let pm = 1;
      if (prise) {                                     // L2162
        const pT = envT > 0 ? 1 : 0.5;
        if (pmul < pT) { pmul += PR_STEP; if (pmul > pT) pmul = pT; }
        else if (pmul > pT) { pmul -= PR_STEP; if (pmul < pT) pmul = pT; }
        pm = pmul;
      }
      const inc = phaseInc * VSS[i] * (1 + drift) * pm;
      let raw;
      if (type === 2) {                                // FM pair
        fmi += (fmiT - fmi) * C_FMI;
        let p1 = vPhase[b + 1] + inc * vRatio[b + 1]; if (p1 >= 1) p1 -= 1; vPhase[b + 1] = p1;
        let p0 = vPhase[b] + inc; if (p0 >= 1) p0 -= 1; vPhase[b] = p0;
        let p = p0 + psin(p1) * fmi;
        if (p >= 1) p -= 1; else if (p < 0) p += 1;
        raw = psin(p) * 3.2;
      } else if (type === 20) {                        // WT
        let pos = wtscan / 15 * (WT_N - 1);
        pos += env * 1.7;
        if (pos > WT_N - 1) pos = WT_N - 1; if (pos < 0) pos = 0;
        const t0 = Math.trunc(pos); let t1 = t0 + 1; if (t1 >= WT_N) t1 = WT_N - 1;
        const tf = pos - t0, w0 = wtbl[t0], w1 = wtbl[t1];
        raw = 0;
        for (let k = 0; k < 3; k++) {
          let ph = vPhase[b + k] + inc * vRatio[b + k];
          while (ph >= 1) ph -= 1;
          vPhase[b + k] = ph;
          const idx = ph * WT_LEN; const i0 = Math.trunc(idx) & (WT_LEN - 1); const fr = idx - Math.trunc(idx);
          const i1 = (i0 + 1) & (WT_LEN - 1);
          const a = w0[i0] + (w0[i1] - w0[i0]) * fr, bb = w1[i0] + (w1[i1] - w1[i0]) * fr;
          raw += a + (bb - a) * tf;
        }
        raw *= 1.5;
      } else if (type === 23) {                        // FOLD
        const drive = 0.7 + wtscan * 0.45 + env * 3.0;
        raw = 0;
        for (let k = 0; k < 3; k++) {
          let ph = vPhase[b + k] + inc * vRatio[b + k];
          while (ph >= 1) ph -= 1;
          vPhase[b + k] = ph;
          let s = psin(ph) * drive;
          for (let it = 0; it < 6; it++) {
            if (s > 1) s = 2 - s; else if (s < -1) s = -2 - s; else break;
          }
          raw += s;
        }
        raw *= 1.2;
      } else if (type === 24) {                        // PLUCK (Karplus-Strong)
        const kb = v * KPN;
        if (vKinit[v]) {
          let L = Math.trunc(1 / phaseInc); if (L < 2) L = 2; if (L > KPmax) L = KPmax;
          vKlen[v] = L; vKpos[v] = 0;
          for (let j = 0; j < L; j++) kpbuf[kb + j] = (lcg() & 0xffff) / 32768 - 1;
          vKinit[v] = 0;
        }
        const L = vKlen[v], p = vKpos[v]; let pn = p + 1; if (pn >= L) pn = 0;
        const damp = 0.990 + wtscan * 0.00060;      // per loop pass, so independent of the context rate
        const outk = kpbuf[kb + p];
        kpbuf[kb + p] = (kpbuf[kb + p] + kpbuf[kb + pn]) * 0.5 * damp;
        vKpos[v] = pn;
        raw = outk * 2.6;
      } else if (type === 26) {                        // CZ
        const reso = 1 + wtscan * 0.7;
        raw = 0;
        for (let k = 0; k < 3; k++) {
          let ph = vPhase[b + k] + inc * vRatio[b + k];
          while (ph >= 1) ph -= 1;
          vPhase[b + k] = ph;
          const win = 1 - ph;
          let rp = ph * reso; rp -= Math.trunc(rp);
          raw += win * psin(rp) * 1.4;
        }
      } else {                                         // 0/1 THIEF naive, 10..14 band-limited
        raw = 0;
        for (let k = 0; k < nosc; k++) {
          let dt = inc * vRatio[b + k];
          if (dt < 0) dt = -dt; if (dt > 0.45) dt = 0.45;
          let ph = vPhase[b + k] + inc * vRatio[b + k];
          while (ph >= 1) ph -= 1;
          while (ph < 0) ph += 1;
          vPhase[b + k] = ph;
          let os;
          switch (type) {
            case 10: os = ph + ph - 1 - polyBlep(ph, dt); break;
            case 11: { let p2 = ph + 0.5; if (p2 >= 1) p2 -= 1; os = (ph < 0.5 ? 1 : -1) + polyBlep(ph, dt) - polyBlep(p2, dt); os *= 0.85; } break;
            case 14: { const w = 0.25; let p2 = ph - w; if (p2 < 0) p2 += 1; os = (ph < w ? 1 : -1) + polyBlep(ph, dt) - polyBlep(p2, dt); os *= 0.85; } break;
            case 12: { let p2 = ph + 0.5; if (p2 >= 1) p2 -= 1; os = 4 * Math.abs(ph - 0.5) - 1; os += 8 * dt * (polyBlamp(p2, dt) - polyBlamp(ph, dt)); } break;
            case 13: os = psin(ph); break;
            case 1: os = 4 * Math.abs(ph - 0.5) - 1; break;
            default: os = ph + ph - 1; break;
          }
          raw += os;
        }
        if (nosc === 1) raw *= 2.2;
      }
      // per-note VCF, states hard-clamped (L2284)
      fc += (fcT - fc) * fcR;
      lp += fc * bp;
      const vhp = raw - lp - q * bp;
      bp += fc * vhp;
      if (lp > 6) lp = 6; if (lp < -6) lp = -6;
      if (bp > 6) bp = 6; if (bp < -6) bp = -6;
      const sig = lp * (0.05 * env) * gvSyn;
      // firmware mixL/mixR are swapped on the hardware (L2293); the web port puts the left pad column on the left
      MIXL[i] += sig * (1 - pan);
      MIXR[i] += sig * pan;
    }
    vEnv[v] = env; vDrift[v] = drift; vFc[v] = fc; vLp[v] = Math.abs(lp) < 1e-18 ? 0 : lp;
    vBp[v] = Math.abs(bp) < 1e-18 ? 0 : bp; vFmi[v] = fmi; vPmul[v] = pmul;
  }

  // ---- drums (L2322)
  for (let v = 0; v < 4; v++) {
    if (drStamp[v] !== dgoStamp[v]) {
      drStamp[v] = dgoStamp[v];
      drEnv[v] = 1; drPh[v] = 0;
      let ft = dgoF[v]; if (ft < 5) ft = 5; if (ft > 16000) ft = 16000;
      drFt[v] = ft;
      let f = ft * dgoDrop[v]; if (f > 20000) f = 20000;
      drF[v] = f;
      let dec = dgoDec[v]; if (dec > 0.99999) dec = 0.99999;
      drDec[v] = K === 1 ? dec : Math.pow(dec, IK);
      drNoiz[v] = dgoNoiz[v]; drAmp[v] = dgoAmp[v]; drPanl[v] = 1 - dgoPan[v]; drPanr[v] = dgoPan[v];
      drPart[v] = dgoPart[v];
    }
    if (!(drEnv[v] > 0.0008)) continue;
    let env = drEnv[v], ph = drPh[v], f = drF[v];
    const ft = drFt[v], dec = drDec[v], noiz = drNoiz[v], amp = drAmp[v], panl = drPanl[v], panr = drPanr[v], part = drPart[v];
    for (let i = 0; i < n; i++) {
      if (!(env > 0.0008)) break;
      env *= dec;
      f += (ft - f) * C_DPITCH;
      ph += f * VSS[i] / SR; if (ph >= 1) ph -= 1;
      let tone = Math.sin(TWO_PI_F * ph);
      if (part > 0) tone = 0.6 * tone + 0.45 * Math.sin(TWO_PI_F * ph * part);
      arng = (Math.imul(arng, 1664525) + 1013904223) >>> 0;
      let nz = (arng >>> 9) * (1 / 4194304) - 1;
      if (noiz > 0.97) { hatlp += (nz - hatlp) * C_HAT; nz -= hatlp; }   // hat HP, one shared state (SPEC 0.15)
      const sd = (tone * (1 - noiz) + nz * noiz) * env * amp;
      DRL[i] += sd * panl * gvDrum; DRR[i] += sd * panr * gvDrum;
    }
    drEnv[v] = env; drPh[v] = ph; drF[v] = f;
  }

  // ---- sampler: record the mic while a slot is armed, play triggered slots into the drum bus (L2355)
  if (rec_slot >= 0) {
    const base = rec_slot * SMPL_N;
    for (let i = 0; i < n; i++) {
      if (rec_pos < SMPL_N) { smp[base + rec_pos] = inp ? inp[off + i] : 0; rec_pos++; }
    }
  }
  for (let s2 = 0; s2 < 16; s2++) {
    if (sp_st[s2] !== smp_trig[s2]) { sp_st[s2] = smp_trig[s2]; sp_pos[s2] = 0; sp_act[s2] = smp_len[s2] > 0 ? 1 : 0; }
    if (!sp_act[s2]) continue;
    const base = s2 * SMPL_N, len = smp_len[s2];
    let pos = sp_pos[s2], fr = sp_fr[s2];
    for (let i = 0; i < n; i++) {
      const sv = smp[base + pos] * 0.9 * gvSmp;
      DRL[i] += sv; DRR[i] += sv;
      fr += VSS[i];
      while (fr >= 1) { fr -= 1; pos++; }
      if (pos >= len) { sp_act[s2] = 0; break; }
    }
    sp_pos[s2] = pos; sp_fr[s2] = fr;
  }

  // ---- bus, master SVFs, FX chain
  let opk = 0;
  for (let i = 0; i < n; i++) {
    ff_s += (filter_f - ff_s) * C_FFS;
    lpL += ff_s * bpL; const hpL = MIXL[i] - lpL - Q1 * bpL; bpL += ff_s * hpL;
    lpR += ff_s * bpR; const hpR = MIXR[i] - lpR - Q1 * bpR; bpR += ff_s * hpR;
    const dL = Math.tanh(lpL * drv), dR = Math.tanh(lpR * drv);
    dlpL += ff_s * dbpL; const dhpL = DRL[i] - dlpL - Q1 * dbpL; dbpL += ff_s * dhpL;
    dlpR += ff_s * dbpR; const dhpR = DRR[i] - dlpR - Q1 * dbpR; dbpR += ff_s * dhpR;
    // chorus term is cwet = 0 in the firmware (L2133), so it is not computed
    oL = Math.tanh((dL * 0.8 + dlpL * 1.5) * mgain);
    oR = Math.tanh((dR * 0.8 + dlpR * 1.5) * mgain);

    if (!rev) {                                        // capture ring (L2389)
      rbufL[rw] = oL; rbufR[rw] = oR;
      if (++rw >= RBUF_N) rw = 0;
    } else {                                           // REVERSE (L2394)
      rev_pos += 1;
      while (rev_pos >= revLe) rev_pos -= revLe;
      const ri = Math.trunc(rev_pos);
      const rp = (fz_anchor + RBUF_N - 1 - ri) % RBUF_N;
      let g = 1;
      if (ri < REV_FADE) g = ri / REV_FADE;
      else if (revLe - ri < REV_FADE) g = (revLe - ri) / REV_FADE;
      oL = rbufL[rp] * g; oR = rbufR[rp] * g;
    }

    if ((fh >> FX_CHOR) & 1) {                         // CHORUS (L2410)
      xlfo += 0.35 / SR; if (xlfo >= 1) xlfo -= 1;
      const depth = 0.45 + 0.55 * ffsr;
      let xlfo2 = xlfo + 0.25; if (xlfo2 >= 1) xlfo2 -= 1;
      const tapL = (960 + 740 * depth * psin(xlfo)) * K, tapR = (960 + 740 * depth * psin(xlfo2)) * K;
      const tiL = Math.trunc(tapL), frL = tapL - tiL, tiR = Math.trunc(tapR), frR = tapR - tiR;
      let ia = xw - tiL; if (ia < 0) ia += XN; let ia1 = ia - 1; if (ia1 < 0) ia1 += XN;
      let ib = xw - tiR; if (ib < 0) ib += XN; let ib1 = ib - 1; if (ib1 < 0) ib1 += XN;
      const a0 = xcL[ia], a1 = xcL[ia1], b0 = xcR[ib], b1 = xcR[ib1];
      const wxL = a0 + (a1 - a0) * frL, wxR = b0 + (b1 - b0) * frR;
      const fb = 0.55 + 0.42 * ffsr;
      xcL[xw] = oL + wxL * fb; xcR[xw] = oR + wxR * fb;
      if (++xw >= XN) xw = 0;
      oL = oL * 0.5 + wxL * 0.9; oR = oR * 0.5 + wxR * 0.9;
    } else { xcL[xw] = oL; xcR[xw] = oR; if (++xw >= XN) xw = 0; }

    if ((fh >> FX_PAN) & 1) {                          // PAN (L2433)
      panph += (0.5 + 7.5 * ffsr) / SR; if (panph >= 1) panph -= 1;
      const px = panph * 2 - 1;
      const ps = 4 * px * (1 - Math.abs(px));
      oL *= (0.15 + 0.85 * 0.5 * (1 + ps)) * 1.4;
      oR *= (0.15 + 0.85 * 0.5 * (1 - ps)) * 1.4;
    }
    {                                                  // MUTE (L2442)
      const mt = ((fh >> FX_MUTE) & 1) ? 0 : 1;
      mg += (mt - mg) * FC.mute;
      oL *= mg; oR *= mg;
    }

    if (fx_eff[1]) fx2();
    if (fx_eff[2]) fx3();
    if (fx_eff[3]) fx4();
    if (fx_eff[4]) fx5();
    if (fx_eff[5]) fx6();
    if (fx_eff[6]) fx7();
    if (fx_eff[7]) fx8();

    // ordered master chain: RING -> DIST -> FILTER -> CRUSH (L3183)
    if ((fx_eff[1] >> 4) & 1) {
      const fc = 40 * Math.pow(37.5, ffsr) * (1 + 0.3 * fx_lid * Math.sin(FM.rphase * 0.07));
      FM.rphase += fc / SR; if (FM.rphase >= 1) FM.rphase -= 1;
      const c = psin(FM.rphase);
      oL = oL * (1 - ffsr * 0.9) + (oL * c) * ffsr * 0.9;
      oR = oR * (1 - ffsr * 0.9) + (oR * c) * ffsr * 0.9;
    }
    if ((fh >> FX_DIST) & 1) {                         // DIST
      const g = 4 + ffsr * 10; const dc = Math.tanh(0.08 * g) * 0.62;
      oL = Math.tanh((oL + 0.08) * g) * 0.62 - dc; oR = Math.tanh((oR + 0.08) * g) * 0.62 - dc;
    }
    if ((fh >> FX_FLT) & 1) {                          // FILTER: audio-rate sweep, as coded (SPEC 12.4)
      FM.mlfo += (0.025 + ffsr * 0.30) * IK; if (FM.mlfo >= 1) FM.mlfo -= 1;
      const tri = 1 - Math.abs(2 * FM.mlfo - 1);
      const fc = 200 * Math.pow(17.5, tri);
      const f = 2 * Math.sin(PI_F * fc / SR), q = 0.3;
      FM.mlp += f * FM.mbp; const hp = oL - FM.mlp - q * FM.mbp; FM.mbp += f * hp;
      FM.mlpR += f * FM.mbpR; const hpR = oR - FM.mlpR - q * FM.mbpR; FM.mbpR += f * hpR;
      oL = FM.mbp; oR = FM.mbpR;
      if (FM.mlp > 4) FM.mlp = 4; else if (FM.mlp < -4) FM.mlp = -4;
      if (FM.mbp > 4) FM.mbp = 4; else if (FM.mbp < -4) FM.mbp = -4;
      if (FM.mlpR > 4) FM.mlpR = 4; else if (FM.mlpR < -4) FM.mlpR = -4;
      if (FM.mbpR > 4) FM.mbpR = 4; else if (FM.mbpR < -4) FM.mbpR = -4;
    } else { FM.mlp = FM.mbp = FM.mlpR = FM.mbpR = 0; }
    if (crush) {                                       // CRUSH, after the filter (L3218)
      if (crn === 0) { const qs = 24 - ffsr * 18; crL = Math.trunc(oL * qs) / qs; crR = Math.trunc(oR * qs) / qs; }
      crn = (crn + 1) % cint; oL = crL; oR = crR;
    } else crn = 0;

    oL *= mvol_l; oR *= mvol_l;                        // master volume after every effect (L3225)
    if (!(oL > -1e6 && oL < 1e6) || !(oR > -1e6 && oR < 1e6)) { oL = 0; oR = 0; nanFlag = true; }
    if (oL > 1) oL = 1; else if (oL < -1) oL = -1;     // hard safety limiter (L3228)
    if (oR > 1) oR = 1; else if (oR < -1) oR = -1;
    o0[off + i] = oL; o1[off + i] = oR;
    const am = oL < 0 ? -oL : oL; if (am > opk) opk = am;
    gstep_samp++;
  }
  rb_w = rw;
  if (opk > outPeak) outPeak = opk;
  // flush denormals in the always-on filters
  if (Math.abs(lpL) < 1e-18) lpL = 0; if (Math.abs(bpL) < 1e-18) bpL = 0;
  if (Math.abs(lpR) < 1e-18) lpR = 0; if (Math.abs(bpR) < 1e-18) bpR = 0;
  if (Math.abs(dlpL) < 1e-18) dlpL = 0; if (Math.abs(dbpL) < 1e-18) dbpL = 0;
  if (Math.abs(dlpR) < 1e-18) dlpR = 0; if (Math.abs(dbpR) < 1e-18) dbpR = 0;
  if (nanFlag) { nanFlag = false; resetFxAfterNaN(); }
}

// a non-finite value in a feedback effect would latch forever; rebuild the FX state
function resetFxAfterNaN() {
  const keep = { hatlp: 0 };
  initFx(); hatlp = keep.hatlp;
  lpL = bpL = lpR = bpR = dlpL = dbpL = dlpR = dbpR = 0; vss = 1; mg = 1; crn = 0;
  rbufL.fill(0); rbufR.fill(0); xcL.fill(0); xcR.fill(0);
}

// ---------------------------------------------------------------- persist (SPEC 8)
const PERSIST_FIELDS = ['step_ms', 'key', 'mode', 'swing', 'wave', 'kit', 'dens', 'poly', 'cdrum', 'csmp',
  'vsyn', 'vdrum', 'vsmp', 'wtscan', 'prise', 'bars', 'beats', 'syn_mode', 'sel_drum', 'sel_smp', 'fx_bank'];

function persistValue(name) {
  switch (name) {
    case 'step_ms': return step_ms; case 'key': return g_key; case 'mode': return g_mode;
    case 'swing': return g_swing; case 'wave': return g_wave; case 'kit': return kit_sel;
    case 'dens': return g_csyn; case 'poly': return g_cpoly; case 'cdrum': return g_cdrum;
    case 'csmp': return g_csmp; case 'vsyn': return g_vsyn; case 'vdrum': return g_vdrum;
    case 'vsmp': return g_vsmp; case 'wtscan': return g_wtscan; case 'prise': return g_prise;
    case 'bars': return form_bars; case 'beats': return form_blen; case 'syn_mode': return seq_mode_syn;
    case 'sel_drum': return sel_drum; case 'sel_smp': return sel_smp; case 'fx_bank': return fx_bank;
  }
  return 0;
}

// Builds the engine's part of a patch. Sampler audio goes out as Int16Array per non-empty slot
// (base64 is applied on the main thread: worklets have no btoa).
function buildPersist() {
  const o = { v: 1 };
  for (const f of PERSIST_FIELDS) o[f] = persistValue(f);
  o.dr_pad = Array.from(dr_pad);
  o.dr_pat = []; for (let v = 0; v < 4; v++) o.dr_pat.push(Array.from(dr_pat.subarray(v * 16, v * 16 + 16)));
  o.smp_pat = []; for (let s = 0; s < 16; s++) o.smp_pat.push(Array.from(smp_pat.subarray(s * 16, s * 16 + 16)));
  o.syn_seq = Array.from(syn_seq);
  o.fx_auto = []; for (let b = 0; b < 8; b++) o.fx_auto.push(Array.from(fx_auto.subarray(b * 16, b * 16 + 16)));
  o.fx_auto_fsr = Array.from(fx_auto_fsr);
  o.audio = {};
  for (let s = 0; s < 16; s++) {
    const len = smp_len[s];
    if (!len) continue;
    const pcm = new Int16Array(len), base = s * SMPL_N;
    for (let i = 0; i < len; i++) {
      let x = smp[base + i]; x = x > 1 ? 1 : x < -1 ? -1 : x; pcm[i] = Math.round(x * 32767);
    }
    o.audio[s] = { sr: SR, n: len, pcm };
  }
  return o;
}

function pick(o, k, def) { const v = o[k]; return (v === undefined || v === null || !(typeof v === 'number')) ? def : v; }

// Load a patch. Missing fields take the boot defaults of SPEC 8.2.
function applyPersist(o) {
  allKeysOff();
  rec_slot = -1; rec_pos = 0; smp_press.fill(0);
  for (let s = 0; s < 16; s++) sp_act[s] = 0;
  g_key = clampi(pick(o, 'key', 0), -24, 24);
  { const m = pick(o, 'mode', 3); g_mode = (m >= 0 && m <= 8) ? Math.trunc(m) : 3; }
  rebuildScale();
  step_ms = clampi(pick(o, 'step_ms', 125), 60, 500);
  g_swing = clampi(pick(o, 'swing', 0), 0, 9);
  { const w = pick(o, 'wave', 5); g_wave = (w >= 0 && w < 13) ? Math.trunc(w) : 5; }
  kit_sel = clampi(pick(o, 'kit', 0), 0, 1) & 1;
  g_csyn = clampi(pick(o, 'dens', 6), 0, 9); g_cpoly = clampi(pick(o, 'poly', 5), 0, 9);
  g_cdrum = clampi(pick(o, 'cdrum', 9), 0, 9); g_csmp = clampi(pick(o, 'csmp', 9), 0, 9);
  g_vsyn = clampi(pick(o, 'vsyn', 10), 0, 12); g_vdrum = clampi(pick(o, 'vdrum', 10), 0, 12); g_vsmp = clampi(pick(o, 'vsmp', 10), 0, 12);
  g_wtscan = clampi(pick(o, 'wtscan', 0), 0, 15);
  g_prise = clampi(pick(o, 'prise', 0), 0, 9);
  { let bl = clampi(pick(o, 'beats', 16), 0, 16); form_blen = bl < 1 ? 16 : bl; }
  form_bars = clampi(pick(o, 'bars', 1), 1, 16);
  seq_mode_syn = clampi(pick(o, 'syn_mode', 0), 0, 2);
  sel_drum = clampi(pick(o, 'sel_drum', 0), 0, 3); sel_smp = clampi(pick(o, 'sel_smp', 0), 0, 15);
  fx_bank = clampi(pick(o, 'fx_bank', 0), 0, 7);
  dr_pad.set([13, 10, 4, 0]);
  if (Array.isArray(o.dr_pad)) for (let i = 0; i < 4; i++) dr_pad[i] = (o.dr_pad[i] | 0) & 15;
  dr_pat.fill(0); smp_pat.fill(0); syn_seq.fill(0); fx_auto.fill(0); fx_auto_fsr.fill(0);
  if (Array.isArray(o.dr_pat)) for (let v = 0; v < 4; v++) if (Array.isArray(o.dr_pat[v])) for (let j = 0; j < 16; j++) dr_pat[v * 16 + j] = (o.dr_pat[v][j] | 0) & 255;
  if (Array.isArray(o.smp_pat)) for (let s = 0; s < 16; s++) if (Array.isArray(o.smp_pat[s])) for (let j = 0; j < 16; j++) smp_pat[s * 16 + j] = (o.smp_pat[s][j] | 0) & 255;
  if (Array.isArray(o.syn_seq)) for (let i = 0; i < MAXSTEPS; i++) syn_seq[i] = (o.syn_seq[i] | 0) & 0xFFFF;
  if (Array.isArray(o.fx_auto)) for (let b = 0; b < 8; b++) if (Array.isArray(o.fx_auto[b])) for (let j = 0; j < 16; j++) fx_auto[b * 16 + j] = (o.fx_auto[b][j] | 0) & 0xFFFF;
  if (Array.isArray(o.fx_auto_fsr)) for (let i = 0; i < 16; i++) fx_auto_fsr[i] = (o.fx_auto_fsr[i] | 0) & 255;
  syn_arm = 0;
  smp_len.fill(0);
  if (o.audio) {
    for (const key of Object.keys(o.audio)) {
      const s = key | 0, a = o.audio[key];
      if (s < 0 || s > 15 || !a || !a.pcm) continue;
      const src = a.pcm, n0 = Math.min(src.length, a.n | 0 || src.length), sr0 = a.sr || SR, base = s * SMPL_N;
      let n = n0;
      if (sr0 === SR) {
        if (n > SMPL_N) n = SMPL_N;
        for (let i = 0; i < n; i++) smp[base + i] = src[i] / 32767;
      } else {                                          // recorded at another rate: linear resample
        n = Math.min(SMPL_N, Math.max(1, Math.round(n0 * SR / sr0)));
        const r = sr0 / SR;
        for (let i = 0; i < n; i++) {
          const p = i * r, i0 = Math.min(n0 - 1, Math.trunc(p)), i1 = Math.min(n0 - 1, i0 + 1), fr = p - i0;
          smp[base + i] = (src[i0] + (src[i1] - src[i0]) * fr) / 32767;
        }
      }
      smp_len[s] = n;
    }
  }
  gstep = 0; songFix();
  dirty++;
}

// ---------------------------------------------------------------- tick
function buildTick(posted) {
  const t = tickObj, gp = gstep >> 4, sl = songLen();
  t.gstep = gstep; t.songLen = sl; t.step_ms = step_ms; t.bpm = Math.trunc(15000 / step_ms);
  t.out_amp = outPeak > 1 ? 1 : outPeak;
  t.playing.syn = syn_seq[gstep & (MAXSTEPS - 1)]; t.playing.held = held_mask;
  for (let i = 0; i < 16; i++) t.syn_seq_page[i] = syn_seq[(gp * 16 + i) & (MAXSTEPS - 1)];
  t.syn_arm = syn_arm;
  for (let v = 0; v < 4; v++) {
    let m = 0; for (let i = 0; i < 16; i++) if (seqBit(dr_pat, v * 16, gp * 16 + i)) m |= (1 << i);
    t.dr_page[v] = m; t.dr_counts[v] = seqCount(dr_pat, v * 16); t.dr_pad[v] = dr_pad[v];
  }
  for (let s = 0; s < 16; s++) {
    let m = 0; for (let i = 0; i < 16; i++) if (seqBit(smp_pat, s * 16, gp * 16 + i)) m |= (1 << i);
    t.sm_page[s] = m; t.sm_counts[s] = seqCount(smp_pat, s * 16); t.smp_len[s] = smp_len[s] / SR;
  }
  t.sel_drum = sel_drum; t.sel_smp = sel_smp; t.rec_slot = rec_slot;
  const fx = t.fx;
  fx.bank = fx_bank; fx.fsr = fx_fsr_eff; fx.lid = fx_lid; fx.rec = fx_rec;
  for (let b = 0; b < 8; b++) { fx.eff[b] = fx_eff[b]; fx.held[b] = fx_held[b]; }
  const p = t.params;
  p.wave = g_wave; p.dens = g_csyn; p.poly = g_cpoly; p.vsyn = g_vsyn; p.wtscan = g_wtscan; p.prise = g_prise;
  p.bars = form_bars; p.beats = form_blen; p.kit = kit_sel; p.cdrum = g_cdrum; p.vdrum = g_vdrum; p.csmp = g_csmp;
  p.vsmp = g_vsmp; p.key = g_key; p.mode = g_mode; p.swing = g_swing; p.mvol_idx = vol_pidx; p.syn_mode = seq_mode_syn;
  t.sampleRate = SR; t.dirty = dirty;
  outPeak = 0;
  return t;
}

// ---------------------------------------------------------------- messages
function nowMs() { return Math.floor(currentFrame * 1000 / SR); }

let portRef = null;
function onMessage(m) {
  if (!m || typeof m.c !== 'string') return;
  const now = nowMs();
  switch (m.c) {
    case 'on': { const k = m.k & 15; held_mask |= (1 << k); keyNoteOn(k, now); } break;
    case 'off': { const k = m.k & 15; held_mask &= ~(1 << k); keyNoteOff(k); } break;
    case 'drum': { const k = m.k & 15; drumPadHit(k); sel_drum = 3 - (k >> 2); dr_pad[sel_drum] = k; dirty++; } break;
    case 'sPress': smp_press[m.k & 15] = Math.max(1, now); break;
    case 'sRel': samplerRelease(m.k & 15); break;
    case 'fxBank': fx_bank = clampi(m.n, 0, 7); dirty++; break;
    case 'fxHeld': {
      const b = clampi(m.bank, 0, 7), mask = (m.mask | 0) & 0xFFFF;
      if (b === 0) for (let k = 0; k < 16; k++) if (((mask >> k) & 1) && !((fx_held[0] >> k) & 1)) fx_t[k] = now;
      fx_held[b] = mask;
    } break;
    case 'fxRec': fx_rec = !!m.on; break;
    case 'fxClr': fx_auto.fill(0); fx_auto_fsr.fill(0); dirty++; break;
    case 'synMode': { const sm = clampi(m.m, 0, 2); if (sm !== seq_mode_syn) { seq_mode_syn = sm; allKeysOff(); dirty++; } } break;
    case 'arm': syn_arm = (m.mask | 0) & 0xFFFF; break;
    case 'place': syn_seq[(m.step | 0) & (MAXSTEPS - 1)] = syn_arm; syn_arm = 0; dirty++; break;
    case 'liveW': syn_live = !!m.on; break;
    case 'synClr': syn_seq.fill(0); syn_arm = 0; dirty++; break;
    case 'tog': {
      const st = m.step | 0;
      if (st < 0 || st >= songLen()) break;
      if (m.eng === 1) { const v = (m.idx | 0) & 3; dr_pat[v * 16 + (st >> 3)] ^= (1 << (st & 7)); dirty++; }
      else if (m.eng === 2) { const s = (m.idx | 0) & 15; smp_pat[s * 16 + (st >> 3)] ^= (1 << (st & 7)); dirty++; }
    } break;
    case 'clr':
      if (m.eng === 1) dr_pat.fill(0, ((m.idx | 0) & 3) * 16, ((m.idx | 0) & 3) * 16 + 16);
      else if (m.eng === 2) smp_pat.fill(0, ((m.idx | 0) & 15) * 16, ((m.idx | 0) & 15) * 16 + 16);
      dirty++; break;
    case 'selD': sel_drum = clampi(m.v, 0, 3); dirty++; break;
    case 'selS': sel_smp = clampi(m.s, 0, 15); dirty++; break;
    case 'set': setParam(m.name, m.value); break;
    case 'adj': adjustParam(m.name, m.dir); break;
    case 'tap': tapTempo(typeof m.ms === 'number' ? m.ms : now); break;
    case 'keysOff': allKeysOff(); break;
    case 'mic': micOk = !!m.ok; break;
    case 'getP': {
      const data = buildPersist(); const tr = [];
      for (const k of Object.keys(data.audio)) tr.push(data.audio[k].pcm.buffer);
      portRef.postMessage({ t: 'persist', id: m.id, data }, tr);
    } break;
    case 'setP': applyPersist(m.data || {}); break;
  }
}

// ---------------------------------------------------------------- processor
class PmorProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    init();
    portRef = this.port;
    this.started = false;
    this.lastPost = 0;
    this.port.onmessage = (e) => onMessage(e.data);
  }
  process(inputs, outputs) {
    const out = outputs[0];
    if (!out || !out[0]) return true;
    const L = out[0], R = out[1] || out[0];
    const inp = (inputs[0] && inputs[0][0] && inputs[0][0].length) ? inputs[0][0] : null;
    const n = L.length;
    try {
      for (let off = 0; off < n;) {
        const m = Math.min(64, n - off);
        renderBlock(inp, L, R, off, m, Math.floor((currentFrame + off) * 1000 / SR));
        off += m;
      }
    } catch (err) {
      for (let i = 0; i < n; i++) { L[i] = 0; R[i] = 0; }
      if (!errSent) { errSent = true; this.port.postMessage({ t: 'error', msg: String(err && err.message || err) }); }
      initFx();
    }
    if (!this.started) { this.started = true; this.port.postMessage({ t: 'ready', sampleRate: SR }); }
    if (stepEdge || currentFrame - this.lastPost >= TICK_MS * SPMS) {
      stepEdge = false; this.lastPost = currentFrame;
      this.port.postMessage({ t: 'tick', d: buildTick() });
    }
    return true;
  }
}
registerProcessor('pmor', PmorProcessor);
