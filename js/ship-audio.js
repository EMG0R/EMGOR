// ship-audio.js - procedural WebAudio for ship mode. No files, no deps.
// createAudio() -> { unlock(), play(name, opts), engine(state), setMaster(v), ready }
// play opts: { dist (ship lengths), pitch (ratio), vel 0..1, twin, stop }
// + one-shots: windup stall overheat ram scan shieldHit shieldBreak units(n) buy npc(seed) shard; loops: play('jetpack',{level}) / 'jetpackStop', play('siren',{level}) / 'sirenStop'
// r27 cues: copWarn grab checkout rockCrack(size) chunk(n) blueprint shipBuy landmark petHappy talk(role,seed) warp questFail
// audio.music = { start(), stop(), set({key, mode:'lydian'|'dorian'|'aeolian', intensity}) }; setMusic(v)
// audio.ambience = { set({place,weather,biome,night,intensity,seed}), stop(), setLevel(v) }; cues: craft questAccept questDone harvest tame hire levelUp
// renderPreview(name | 'amb:place[:weather[:biome[:night]]]', seconds) -> Float32Array (OfflineAudioContext, for testing)

const CAPS = { fire: 6, hit: 8, crit: 4, kill: 4, limbSever: 4, bossRoar: 2, explosion: 4, entry: 1, land: 2, liftoff: 2, ui: 4, windup: 3, stall: 3, overheat: 1, ram: 2, scan: 2, shieldHit: 6, shieldBreak: 3, units: 4, buy: 2, npc: 3, shard: 6, craft: 2, questAccept: 2, questDone: 2, harvest: 3, tame: 2, hire: 2, levelUp: 2, copWarn: 2, grab: 3, checkout: 2, rockCrack: 3, chunk: 6, blueprint: 1, shipBuy: 1, landmark: 1, petHappy: 2, talk: 3, warp: 1, questFail: 1 };
function mulberry(a) { return () => { a |= 0; a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function hashSeed(x) { if (typeof x === 'number') return x | 0; let h = 2166136261; for (const c of String(x)) h = Math.imul(h ^ c.charCodeAt(0), 16777619); return h; }
const VOW = [[730, 1090], [270, 2290], [300, 870], [530, 1840], [660, 1720]];
const rnd = (a, b) => a + Math.random() * (b - a);
const NOISE_SR = 44100;

// ---------- graph (works on a live or offline context) ----------
function buildCore(ctx, masterVal) {
  const sr = ctx.sampleRate;
  // shared 2 s white noise buffer
  const nb = ctx.createBuffer(1, sr * 2, sr), nd = nb.getChannelData(0);
  for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;

  // master chain: bus -> compressor -> limiter -> master -> out
  const bus = ctx.createGain();
  const comp = ctx.createDynamicsCompressor();
  comp.threshold.value = -20; comp.knee.value = 12; comp.ratio.value = 4; comp.attack.value = 0.004; comp.release.value = 0.18;
  const lim = ctx.createDynamicsCompressor();
  lim.threshold.value = -4; lim.knee.value = 0; lim.ratio.value = 20; lim.attack.value = 0.001; lim.release.value = 0.06;
  const master = ctx.createGain(); master.gain.value = masterVal;
  bus.connect(comp); comp.connect(lim); lim.connect(master); master.connect(ctx.destination);

  // cheap feedback delay send (explosions / roars)
  const send = ctx.createGain(); send.gain.value = 1;
  const dly = ctx.createDelay(1); dly.delayTime.value = 0.23;
  const fb = ctx.createGain(); fb.gain.value = 0.42;
  const dlp = ctx.createBiquadFilter(); dlp.type = 'lowpass'; dlp.frequency.value = 1800;
  const wet = ctx.createGain(); wet.gain.value = 0.5;
  send.connect(dly); dly.connect(dlp); dlp.connect(fb); fb.connect(dly); dlp.connect(wet); wet.connect(bus);

  const voices = {}; // name -> [voice]
  let entryVoice = null;

  const now = () => ctx.currentTime;
  const tracker = () => ({ srcs: [], nodes: [] });
  function osc(v, type, f, t) { const o = ctx.createOscillator(); o.type = type; o.frequency.setValueAtTime(f, t); v.srcs.push(o); return o; }
  function noise(v, t, off) { const s = ctx.createBufferSource(); s.buffer = nb; s.loop = true; v.srcs.push(s); s._off = off ?? rnd(0, 1.5); return s; }
  function filt(type, f, q) { const b = ctx.createBiquadFilter(); b.type = type; b.frequency.value = f; if (q != null) b.Q.value = q; return b; }
  function gain(val) { const g = ctx.createGain(); g.gain.value = val; return g; }
  // click-free envelope: 0 -> peak (a), exp decay to ~0 by dur
  function env(g, t, a, peak, dur) {
    g.gain.cancelScheduledValues(t);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(peak, t + a);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  }
  function startAll(v, t, dur) {
    for (const s of v.srcs) { s.start(t, s._off || 0); s.stop(t + dur + 0.05); }
  }

  // ---- recipes: (v, out, t, p) -> duration; build nodes into v, connect to out ----
  const R = {};
  R.fire = (v, out, t, p) => {
    const dur = 0.22, f0 = 900 * p.pitch;
    const layer = (det, delay, amp) => {
      const tt = t + delay;
      const o = osc(v, 'sawtooth', f0 * det, tt);
      o.frequency.exponentialRampToValueAtTime(f0 * det * 0.18, tt + dur * 0.8);
      const lp = filt('lowpass', 5200, 3); lp.frequency.setValueAtTime(5200, tt); lp.frequency.exponentialRampToValueAtTime(500, tt + dur);
      const g = gain(0); env(g, tt, 0.003, 0.5 * amp, dur);
      o.connect(lp); lp.connect(g); g.connect(out);
      const n = noise(v, tt), hp = filt('highpass', 2500, 0.7), ng = gain(0); env(ng, tt, 0.002, 0.45 * amp, 0.05);
      n.connect(hp); hp.connect(ng); ng.connect(out);
    };
    layer(1, 0, 1);
    if (p.twin) layer(1.012, 0.007, 0.8);
    return dur + 0.02;
  };
  R.hit = (v, out, t, p, bright) => {
    const dur = 0.3, f = 150 * p.pitch;
    const car = osc(v, 'sine', f, t); car.frequency.exponentialRampToValueAtTime(f * 0.4, t + 0.12);
    const mod = osc(v, 'sine', f * 1.41, t), mg = gain(0); env(mg, t, 0.002, f * 5, 0.1);
    mod.connect(mg); mg.connect(car.frequency);
    const g = gain(0); env(g, t, 0.003, 0.8, 0.22); car.connect(g); g.connect(out);
    // metallic ring: inharmonic partials
    [1, 2.76, 5.4].forEach((r, i) => {
      const o = osc(v, 'square', 520 * p.pitch * r, t), bp = filt('bandpass', 520 * p.pitch * r, 12), rg = gain(0);
      env(rg, t, 0.002, 0.16 / (i + 1), dur - i * 0.05);
      o.connect(bp); bp.connect(rg); rg.connect(out);
    });
    if (bright) {
      const o = osc(v, 'sine', 2600 * p.pitch, t), o2 = osc(v, 'sine', 3910 * p.pitch, t), bg = gain(0);
      env(bg, t, 0.002, 0.3, 0.45); o.connect(bg); o2.connect(bg); bg.connect(out);
      return 0.5;
    }
    return dur;
  };
  R.crit = (v, out, t, p) => R.hit(v, out, t, p, true);
  R.kill = (v, out, t, p) => {
    const dur = 0.55;
    const n = noise(v, t), lp = filt('lowpass', 7000, 1); lp.frequency.exponentialRampToValueAtTime(300, t + dur);
    const ws = ctx.createWaveShaper(); const c = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = i / 128 - 1; c[i] = Math.round(x * 6) / 6; } // bit-crush staircase
    ws.curve = c;
    const g = gain(0); env(g, t, 0.003, 0.7, dur * 0.7);
    n.connect(ws); ws.connect(lp); lp.connect(g); g.connect(out);
    const s = osc(v, 'sine', 120 * p.pitch, t); s.frequency.exponentialRampToValueAtTime(32, t + dur);
    const sg = gain(0); env(sg, t, 0.004, 0.9, dur); s.connect(sg); sg.connect(out);
    return dur;
  };
  R.limbSever = (v, out, t, p) => {
    const dur = 0.5;
    // tearing: noise gated by a fast random-ish square AM (granular feel) through a swept bandpass
    const n = noise(v, t), bp = filt('bandpass', 2400, 1.5);
    bp.frequency.setValueAtTime(900, t); bp.frequency.exponentialRampToValueAtTime(4200, t + 0.35);
    const am = ctx.createGain(); am.gain.value = 0.5;
    const lfo = osc(v, 'square', 45 * p.pitch, t); lfo.frequency.linearRampToValueAtTime(95 * p.pitch, t + 0.35);
    const lg = gain(0.5); lfo.connect(lg); lg.connect(am.gain);
    const g = gain(0); env(g, t, 0.004, 0.6, 0.38);
    n.connect(bp); bp.connect(am); am.connect(g); g.connect(out);
    const th = osc(v, 'sine', 90 * p.pitch, t + 0.12); th.frequency.exponentialRampToValueAtTime(38, t + dur);
    const tg = gain(0); env(tg, t + 0.12, 0.004, 0.85, dur - 0.12); th.connect(tg); tg.connect(out);
    return dur;
  };
  R.bossRoar = (v, out, t, p) => {
    const dur = rnd(1.5, 2.3), f = rnd(40, 70) * p.pitch;
    const car = osc(v, 'sawtooth', f, t);
    car.frequency.linearRampToValueAtTime(f * 1.35, t + dur * 0.35); car.frequency.linearRampToValueAtTime(f * 0.7, t + dur);
    const mod = osc(v, 'sine', f * 0.5, t), mg = gain(f * 1.8); mod.connect(mg); mg.connect(car.frequency);
    const vib = osc(v, 'sine', 5.5, t), vg = gain(f * 0.05); vib.connect(vg); vg.connect(car.frequency);
    const ws = ctx.createWaveShaper(); const c = new Float32Array(512);
    for (let i = 0; i < 512; i++) c[i] = Math.tanh((i / 256 - 1) * 3);
    ws.curve = c;
    const fm = filt('bandpass', 400, 4); fm.frequency.setValueAtTime(350, t);
    fm.frequency.exponentialRampToValueAtTime(900, t + dur * 0.4); fm.frequency.exponentialRampToValueAtTime(300, t + dur);
    const fmg = gain(1.4);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.8, t + 0.12);
    g.gain.setValueAtTime(0.8, t + dur * 0.6); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    car.connect(ws); ws.connect(g);
    ws.connect(fm); fm.connect(fmg); fmg.connect(g);
    // rasp
    const n = noise(v, t), nbp = filt('bandpass', 700, 2), ng = gain(0.0001);
    ng.gain.linearRampToValueAtTime(0.25, t + 0.3); ng.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    n.connect(nbp); nbp.connect(ng); ng.connect(g);
    g.connect(out); const s = gain(0.45); g.connect(s); s.connect(send);
    return dur + 0.05;
  };
  R.explosion = (v, out, t, p) => {
    const dur = 1.6;
    const mk = (lpS, lpE, t0, amp, d) => {
      const n = noise(v, t0), lp = filt('lowpass', lpS, 0.8);
      lp.frequency.setValueAtTime(lpS, t0); lp.frequency.exponentialRampToValueAtTime(lpE, t0 + d);
      const g = gain(0); env(g, t0, 0.004, amp, d); n.connect(lp); lp.connect(g); g.connect(out); return g;
    };
    mk(9000 * p.pitch, 200, t, 0.9, 1.3);
    mk(2500 * p.pitch, 90, t + 0.03, 0.8, dur);
    const s = osc(v, 'sine', 85 * p.pitch, t); s.frequency.exponentialRampToValueAtTime(26, t + 1.0);
    const sg = gain(0); env(sg, t, 0.005, 1.0, 1.3); s.connect(sg); sg.connect(out);
    const sd = gain(0.5); sg.connect(sd); sd.connect(send);
    const g2 = mk(1500, 150, t + 0.01, 0.4, 1.0); const s2 = gain(0.6); g2.connect(s2); s2.connect(send);
    return dur + 0.05;
  };
  R.entry = (v, out, t, p) => {
    // rising wind bed, held until stopped (max 14 s)
    const n = noise(v, t), bp = filt('bandpass', 300, 0.8);
    bp.frequency.setValueAtTime(300, t); bp.frequency.exponentialRampToValueAtTime(2600, t + 6);
    const n2 = noise(v, t), lp = filt('lowpass', 500, 0.7); lp.frequency.exponentialRampToValueAtTime(2200, t + 6);
    const lfo = osc(v, 'sine', 7, t), lg = gain(0.12); const am = gain(0.8); lfo.connect(lg); lg.connect(am.gain);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.5, t + 6);
    n.connect(bp); bp.connect(am); n2.connect(lp); lp.connect(am); am.connect(g); g.connect(out);
    const rum = osc(v, 'sawtooth', 48 * p.pitch, t); rum.frequency.exponentialRampToValueAtTime(95, t + 6);
    const rl = filt('lowpass', 160, 1), rg = gain(0.25); rum.connect(rl); rl.connect(rg); rg.connect(g);
    v.hold = g; v.release = (tt) => { g.gain.cancelScheduledValues(tt); g.gain.setValueAtTime(g.gain.value, tt); g.gain.exponentialRampToValueAtTime(0.0001, tt + 0.5); };
    return 14;
  };
  R.land = (v, out, t, p) => {
    const dur = 1.5;
    const n = noise(v, t), hp = filt('highpass', 3500, 0.7), g = gain(0);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.28, t + 0.05); g.gain.setValueAtTime(0.28, t + 0.5); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.1);
    const lfo = osc(v, 'sine', 14, t), lg = gain(0.08); lfo.connect(lg); lg.connect(g.gain);
    n.connect(hp); hp.connect(g); g.connect(out);
    [0.55, 0.95].forEach((d, i) => {
      const o = osc(v, 'sine', (i ? 70 : 95) * p.pitch, t + d); o.frequency.exponentialRampToValueAtTime(34, t + d + 0.18);
      const cg = gain(0); env(cg, t + d, 0.003, 0.9, 0.3); o.connect(cg); cg.connect(out);
      const cn = noise(v, t + d), cl = filt('bandpass', 900, 3), cng = gain(0); env(cng, t + d, 0.002, 0.35, 0.06);
      cn.connect(cl); cl.connect(cng); cng.connect(out);
    });
    return dur;
  };
  R.liftoff = (v, out, t, p) => {
    const dur = 2.2;
    const n = noise(v, t), bp = filt('bandpass', 400, 1);
    bp.frequency.setValueAtTime(300, t); bp.frequency.exponentialRampToValueAtTime(3500, t + 0.9); bp.frequency.exponentialRampToValueAtTime(700, t + dur);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.6, t + 0.4); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    n.connect(bp); bp.connect(g); g.connect(out);
    const k = osc(v, 'sawtooth', 55 * p.pitch, t); k.frequency.exponentialRampToValueAtTime(130, t + 0.8);
    const kl = filt('lowpass', 400, 1), kg = gain(0); env(kg, t, 0.01, 0.7, 1.6); k.connect(kl); kl.connect(kg); kg.connect(out);
    return dur + 0.05;
  };
  R.ui = (v, out, t, p) => {
    [0, 0.07].forEach((d, i) => {
      const o = osc(v, 'triangle', (i ? 1500 : 1100) * p.pitch, t + d), g = gain(0);
      env(g, t + d, 0.002, 0.18, 0.04); o.connect(g); g.connect(out);
    });
    return 0.15;
  };

  // ---- Revision 21 one-shots ----
  R.windup = (v, out, t, p) => {
    const dur = 0.7, f = 200 * p.pitch;
    const car = osc(v, 'sine', f, t); car.frequency.exponentialRampToValueAtTime(f * 4.2, t + dur);
    const mod = osc(v, 'sine', f * 1.5, t); mod.frequency.exponentialRampToValueAtTime(f * 6.3, t + dur);
    const mg = gain(0); mg.gain.setValueAtTime(f * 0.3, t); mg.gain.linearRampToValueAtTime(f * 3.5, t + dur);
    mod.connect(mg); mg.connect(car.frequency);
    const sw = osc(v, 'sawtooth', f * 0.5, t); sw.frequency.exponentialRampToValueAtTime(f * 2.1, t + dur);
    const lp = filt('lowpass', 300, 3); lp.frequency.setValueAtTime(300, t); lp.frequency.exponentialRampToValueAtTime(5200, t + dur);
    const sg = gain(0.25);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.5, t + dur - 0.04); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    car.connect(g); sw.connect(lp); lp.connect(sg); sg.connect(g); g.connect(out);
    return dur + 0.02;
  };
  R.stall = (v, out, t, p) => {
    const dur = 1.0;
    [110, 138.6, 164.8, 207.7].forEach((f, i) => {
      const o = osc(v, 'square', f * p.pitch, t); o.detune.value = (i % 2 ? 1 : -1) * rnd(8, 18);
      const lp = filt('lowpass', 1100, 1), g = gain(0); env(g, t, 0.003, 0.16, 0.55);
      o.connect(lp); lp.connect(g); g.connect(out);
    });
    const k = osc(v, 'sine', 95 * p.pitch, t); k.frequency.exponentialRampToValueAtTime(40, t + 0.14);
    const kg = gain(0); env(kg, t, 0.003, 0.8, 0.2); k.connect(kg); kg.connect(out);
    const n = noise(v, t), bp = filt('bandpass', 350, 2), ng = gain(0); env(ng, t, 0.002, 0.3, 0.05);
    n.connect(bp); bp.connect(ng); ng.connect(out);
    const b = osc(v, 'sine', 330 * p.pitch, t), b2 = osc(v, 'sine', 330 * p.pitch * 2.01, t), bg = gain(0); env(bg, t + 0.02, 0.01, 0.14, dur - 0.05);
    b.connect(bg); b2.connect(bg); bg.connect(out);
    return dur;
  };
  R.overheat = (v, out, t, p) => {
    const dur = 0.8;
    const n = noise(v, t), hp = filt('highpass', 2600, 0.7), bp = filt('bandpass', 5200, 0.8), g = gain(0);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.45, t + 0.06); g.gain.setValueAtTime(0.45, t + 0.3); g.gain.exponentialRampToValueAtTime(0.0001, t + dur - 0.05);
    n.connect(hp); hp.connect(bp); bp.connect(g); g.connect(out);
    [0.05, 0.26].forEach((d) => {
      const o = osc(v, 'square', 1760 * p.pitch, t + d), lp = filt('lowpass', 3200, 1), bg = gain(0);
      env(bg, t + d, 0.003, 0.2, 0.09); o.connect(lp); lp.connect(bg); bg.connect(out);
    });
    return dur;
  };
  R.ram = (v, out, t, p) => {
    const dur = 0.7;
    const n = noise(v, t), ws = ctx.createWaveShaper(), c = new Float32Array(256);
    for (let i = 0; i < 256; i++) { const x = i / 128 - 1; c[i] = Math.round(x * 5) / 5; }
    ws.curve = c;
    const bp = filt('bandpass', 1900 * p.pitch, 1.5); bp.frequency.exponentialRampToValueAtTime(500, t + 0.4);
    const g = gain(0); env(g, t, 0.002, 0.7, 0.4);
    n.connect(ws); ws.connect(bp); bp.connect(g); g.connect(out);
    [1, 2.76, 5.4, 8.9].forEach((r, i) => {
      const o = osc(v, 'square', 260 * p.pitch * r, t), b = filt('bandpass', 260 * p.pitch * r, 14), rg = gain(0);
      env(rg, t, 0.002, 0.2 / (i + 1), dur - i * 0.1); o.connect(b); b.connect(rg); rg.connect(out);
    });
    const s = osc(v, 'sine', 80 * p.pitch, t); s.frequency.exponentialRampToValueAtTime(26, t + 0.5);
    const sg = gain(0); env(sg, t, 0.003, 1.0, 0.55); s.connect(sg); sg.connect(out);
    return dur;
  };
  R.scan = (v, out, t, p) => {
    const dur = 0.5;
    const o = osc(v, 'sine', 500 * p.pitch, t); o.frequency.exponentialRampToValueAtTime(2600 * p.pitch, t + dur * 0.8);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.4, t + 0.05); g.gain.setValueAtTime(0.4, t + 0.35); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    const o2 = osc(v, 'sine', 1000 * p.pitch, t); o2.frequency.exponentialRampToValueAtTime(5200 * p.pitch, t + dur * 0.8);
    const g2 = gain(0); env(g2, t, 0.05, 0.1, dur);
    o.connect(g); o2.connect(g2); g.connect(out); g2.connect(out);
    const sd = gain(0.7); g.connect(sd); sd.connect(send);
    return dur + 0.05;
  };
  R.shieldHit = (v, out, t, p) => {
    const pr = p.pitch * rnd(0.85, 1.3), dur = 0.3;
    [[2400, 0.3, 0.25], [3610, 0.2, 0.18], [5330, 0.12, 0.12]].forEach(([f, a, d]) => {
      const o = osc(v, 'sine', f * pr, t), g = gain(0); env(g, t, 0.002, a, d); o.connect(g); g.connect(out);
    });
    const n = noise(v, t), hp = filt('highpass', 6000, 0.7), ng = gain(0); env(ng, t, 0.001, 0.25, 0.02);
    n.connect(hp); hp.connect(ng); ng.connect(out);
    return dur;
  };
  R.shieldBreak = (v, out, t, p) => {
    const dur = 0.8;
    const n = noise(v, t), hp = filt('highpass', 3800, 0.8), am = gain(0.5);
    const lfo = osc(v, 'square', 70, t); lfo.frequency.linearRampToValueAtTime(25, t + 0.5);
    const lg = gain(0.5); lfo.connect(lg); lg.connect(am.gain);
    const g = gain(0); env(g, t, 0.002, 0.6, 0.55);
    n.connect(hp); hp.connect(am); am.connect(g); g.connect(out);
    for (let i = 0; i < 9; i++) {
      const d = i * 0.035 + rnd(0, 0.02), o = osc(v, 'sine', rnd(2000, 7500) * p.pitch, t + d), pg = gain(0);
      env(pg, t + d, 0.001, 0.16, rnd(0.08, 0.22)); o.connect(pg); pg.connect(out);
    }
    const s = osc(v, 'sine', 150 * p.pitch, t); s.frequency.exponentialRampToValueAtTime(38, t + 0.3);
    const sg = gain(0); env(sg, t, 0.003, 0.9, 0.35); s.connect(sg); sg.connect(out);
    return dur;
  };
  R.units = (v, out, t, p) => {
    const step = Math.pow(2, Math.min(12, Math.max(0, (p.n || 0))) / 12);
    const f = 988 * p.pitch * step;
    [[0, 1, 0.07], [0.07, 1.335, 0.3]].forEach(([d, r, len]) => {
      const o = osc(v, 'square', f * r, t + d), lp = filt('lowpass', 4200, 0.8), g = gain(0);
      env(g, t + d, 0.002, 0.14, len); o.connect(lp); lp.connect(g); g.connect(out);
    });
    return 0.45;
  };
  R.buy = (v, out, t, p) => {
    [[784, 0.22, 0], [1175, 0.14, 0.06], [1568, 0.08, 0.12]].forEach(([f, a, d]) => {
      const o = osc(v, 'sine', f * p.pitch, t + d), g = gain(0); env(g, t + d, 0.012, a, 0.9); o.connect(g); g.connect(out);
    });
    return 1.05;
  };
  R.npc = (v, out, t, p) => {
    const rs = mulberry(((p.seed == null ? Math.random() * 1e9 : hashSeed(p.seed)) >>> 0));
    const base = 140 + rs() * 280, wave = ['square', 'sawtooth', 'triangle'][Math.floor(rs() * 3)];
    const vows = [VOW[Math.floor(rs() * VOW.length)], VOW[Math.floor(rs() * VOW.length)]];
    const cnt = 3 + Math.floor(Math.random() * 4);
    let tt = t;
    for (let i = 0; i < cnt; i++) {
      const f = base * p.pitch * Math.pow(2, (Math.floor(Math.random() * 9) - 3) / 12) * (Math.random() < 0.2 ? 2 : 1), len = rnd(0.055, 0.1);
      const o = osc(v, wave, f, tt); o.frequency.linearRampToValueAtTime(f * rnd(0.9, 1.15), tt + len);
      const vw = vows[i % 2], g = gain(0); env(g, tt, 0.006, 0.3, len + 0.02);
      vw.forEach((fc, k) => { const bp = filt('bandpass', fc, 6 + k * 3), bg = gain(k ? 0.6 : 1.4); o.connect(bp); bp.connect(bg); bg.connect(g); });
      g.connect(out);
      tt += len + rnd(0.02, 0.05);
    }
    return tt - t + 0.08;
  };
  R.shard = (v, out, t, p) => {
    const f = rnd(1900, 2400) * p.pitch, dur = 1.3;
    [[1, 0.26, 0.5], [2.76, 0.12, 0.35], [4.07, 0.07, 0.25]].forEach(([r, a, d]) => {
      const o = osc(v, 'sine', f * r, t), g = gain(0); env(g, t, 0.002, a, d); o.connect(g); g.connect(out);
    });
    const s1 = osc(v, 'sine', f * 1.5, t + 0.04), s2 = osc(v, 'sine', f * 1.5 * 1.006, t + 0.04), tr = osc(v, 'sine', 9, t);
    const tg = gain(0.5), sg = gain(0); tr.connect(tg); tg.connect(sg.gain);
    env(sg, t + 0.04, 0.05, 0.12, dur - 0.1); s1.connect(sg); s2.connect(sg); sg.connect(out);
    const sd = gain(0.5); sg.connect(sd); sd.connect(send);
    return dur;
  };

  // ---- jetpack loop (persistent, 6 nodes, created on first use) ----
  let J = null;
  function jetpack(level) {
    const t = now();
    if (!J) {
      const nz = ctx.createBufferSource(); nz.buffer = nb; nz.loop = true;
      const bp = filt('bandpass', 700, 0.9), nG = gain(0.1), lo = ctx.createOscillator(); lo.type = 'sine'; lo.frequency.value = 70;
      const sG = gain(0.1), jg = gain(0);
      nz.connect(bp); bp.connect(nG); nG.connect(jg); lo.connect(sG); sG.connect(jg); jg.connect(bus);
      nz.start(t, 0.7); lo.start(t);
      J = { nz, bp, nG, lo, sG, jg };
    }
    const l = Math.max(0, Math.min(1, level == null ? 0.7 : level));
    J.bp.frequency.setTargetAtTime(450 + l * 1900, t, 0.06);
    J.nG.gain.setTargetAtTime(0.06 + l * 0.34, t, 0.06);
    J.lo.frequency.setTargetAtTime(58 + l * 40, t, 0.08);
    J.sG.gain.setTargetAtTime(0.1 + l * 0.22, t, 0.08);
    J.jg.gain.cancelScheduledValues(t); J.jg.gain.setTargetAtTime(1, t, 0.03);
  }
  function jetpackStop() {
    if (!J) return;
    const t = now();
    J.jg.gain.cancelScheduledValues(t); J.jg.gain.setValueAtTime(J.jg.gain.value, t); J.jg.gain.linearRampToValueAtTime(0, t + 0.15);
  }

  // ---- r27 cues ----
  const nb1 = (v, out, t, type, f, q, len, amp, a) => { const n = noise(v, t), b = filt(type, f, q), g = gain(0); env(g, t, a || 0.002, amp, len); n.connect(b); b.connect(g); g.connect(out); return b; };
  R.copWarn = (v, out, t, p) => {
    [0, 0.2].forEach((d) => {
      const o = osc(v, 'sine', 2900 * p.pitch, t + d), o2 = osc(v, 'sine', 2900 * p.pitch * 1.01, t + d), g = gain(0);
      o.frequency.linearRampToValueAtTime(3300 * p.pitch, t + d + 0.1); o2.frequency.linearRampToValueAtTime(3300 * p.pitch * 1.01, t + d + 0.1);
      env(g, t + d, 0.006, 0.26, 0.13); o.connect(g); o2.connect(g); g.connect(out);
    });
    return 0.45;
  };
  R.grab = (v, out, t, p) => {
    const o = osc(v, 'sine', 170 * p.pitch, t); o.frequency.exponentialRampToValueAtTime(75, t + 0.1);
    const g = gain(0); env(g, t, 0.003, 0.4, 0.14); o.connect(g); g.connect(out);
    nb1(v, out, t, 'bandpass', 900 * p.pitch, 0.8, 0.07, 0.22, 0.003);
    return 0.2;
  };
  R.checkout = (v, out, t, p) => {
    nb1(v, out, t, 'highpass', 4000, 0.7, 0.025, 0.2);
    tn(v, out, t + 0.02, 'sine', 2093 * p.pitch, 0.9, 0.18); tn(v, out, t + 0.02, 'sine', 5600 * p.pitch, 0.5, 0.06);
    tn(v, out, t + 0.02, 'sine', 3136 * p.pitch, 0.7, 0.08);
    const o = osc(v, 'triangle', 140, t + 0.28); o.frequency.exponentialRampToValueAtTime(60, t + 0.4);
    const g = gain(0); env(g, t + 0.28, 0.005, 0.35, 0.18); o.connect(g); g.connect(out);
    nb1(v, out, t + 0.28, 'bandpass', 1400, 1.5, 0.12, 0.2, 0.004);
    nb1(v, out, t + 0.42, 'highpass', 5000, 1, 0.04, 0.12);
    return 1.05;
  };
  R.rockCrack = (v, out, t, p) => {
    const sz = Math.max(0.3, Math.min(2.5, p.size == null ? 1 : p.size)), dur = 0.3 + 0.25 * sz;
    const o = osc(v, 'sine', 110 / Math.sqrt(sz) * p.pitch, t); o.frequency.exponentialRampToValueAtTime(40, t + dur * 0.6);
    const g = gain(0); env(g, t, 0.003, 0.5, dur * 0.7); o.connect(g); g.connect(out);
    const lp = nb1(v, out, t, 'lowpass', 1800, 0.8, dur, 0.5, 0.002); lp.frequency.exponentialRampToValueAtTime(300, t + dur);
    nb1(v, out, t, 'bandpass', 2600, 1.2, 0.04, 0.3, 0.001);
    const rs = mulberry(((p.seed == null ? 99 : hashSeed(p.seed)) >>> 0)), n = 3 + Math.round(sz * 2);
    for (let i = 0; i < n; i++) { const d = 0.05 + rs() * dur; nb1(v, out, t + d, 'bandpass', 1500 + rs() * 3500, 3, 0.02, 0.1 + rs() * 0.1, 0.001); }
    return dur + 0.1;
  };
  R.chunk = (v, out, t, p) => {
    const k = Math.min(12, Math.max(0, p.n == null ? 0 : p.n)), f = 440 * p.pitch * Math.pow(2, k / 12);
    const o = osc(v, 'sine', f, t); o.frequency.exponentialRampToValueAtTime(f * 1.6, t + 0.05);
    const g = gain(0); env(g, t, 0.003, 0.28, 0.12); o.connect(g); g.connect(out);
    tn(v, out, t, 'triangle', f * 2, 0.06, 0.08, 4000);
    return 0.2;
  };
  R.blueprint = (v, out, t, p) => {
    [988, 784, 659, 523].forEach((f, i) => {
      const d = i * 0.17;
      tn(v, out, t + d, 'sine', f * p.pitch, 1.1, 0.2); tn(v, out, t + d, 'sine', f * p.pitch * 2.003, 0.9, 0.08); tn(v, out, t + d, 'sine', f * p.pitch * 3.01, 0.5, 0.03);
      const sg = gain(0), s = osc(v, 'sine', f * p.pitch * 1.005, t + d), tr = osc(v, 'sine', 6, t + d), tg = gain(0.5);
      tr.connect(tg); tg.connect(sg.gain); env(sg, t + d, 0.05, 0.06, 0.9); s.connect(sg); sg.connect(out);
      const sd = gain(0.6); sg.connect(sd); sd.connect(send);
    });
    return 1.9;
  };
  R.shipBuy = (v, out, t, p) => {
    [[130.8, 0.2], [196, 0.18], [261.6, 0.16], [329.6, 0.14], [392, 0.12], [523.3, 0.1]].forEach(([f, a], i) => {
      const o = osc(v, 'sawtooth', f * p.pitch, t + i * 0.03), lp = filt('lowpass', 1800, 0.7), g = gain(0);
      env(g, t + i * 0.03, 0.02, a, 1.5); o.connect(lp); lp.connect(g); g.connect(out);
    });
    const h = nb1(v, out, t + 0.1, 'bandpass', 5500, 0.9, 0.9, 0.3, 0.04); h.frequency.exponentialRampToValueAtTime(2500, t + 1);
    nb1(v, out, t + 0.05, 'lowpass', 300, 0.7, 0.3, 0.3, 0.01);
    return 1.7;
  };
  R.landmark = (v, out, t, p) => {
    [[261.6, 0.1], [392, 0.09], [523.3, 0.07], [659.3, 0.05]].forEach(([f, a]) => {
      const g = gain(0.0001), o = osc(v, 'sine', f * p.pitch, t), o2 = osc(v, 'sine', f * p.pitch * 1.004, t);
      g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(a, t + 1.0); g.gain.linearRampToValueAtTime(a * 0.7, t + 1.4); g.gain.exponentialRampToValueAtTime(0.0001, t + 2.1);
      o.connect(g); o2.connect(g); g.connect(out);
    });
    const sd = gain(0.4); out.connect(sd); sd.connect(send);
    return 2.2;
  };
  R.petHappy = (v, out, t, p) => {
    [0, 0.13].forEach((d, i) => {
      const f = (i ? 1500 : 1200) * p.pitch, o = osc(v, 'sine', f, t + d); o.frequency.exponentialRampToValueAtTime(f * 1.5, t + d + 0.07);
      const g = gain(0); env(g, t + d, 0.004, 0.25, 0.09); o.connect(g); g.connect(out);
    });
    return 0.3;
  };
  // per-role voice: [base Hz, wave, vowel ids, blip len, gap, glide, min, max, q]
  const ROLES = {
    shopper: [250, 'triangle', [3, 4], 0.08, 0.04, 1.1, 3, 5, 6], clerk: [190, 'square', [0, 3], 0.07, 0.035, 0.95, 3, 6, 8],
    dealer: [110, 'sawtooth', [0, 2], 0.11, 0.05, 0.9, 3, 5, 5], cop: [130, 'sawtooth', [2, 0], 0.065, 0.05, 1, 3, 4, 9],
    tourist: [330, 'triangle', [4, 1], 0.1, 0.03, 1.2, 4, 6, 5], kid: [520, 'sine', [4, 1], 0.06, 0.03, 1.25, 4, 6, 4],
    pilot: [170, 'square', [3, 2], 0.05, 0.06, 1, 3, 5, 10], chef: [150, 'triangle', [0, 4], 0.12, 0.04, 1.08, 3, 5, 5],
    miner: [90, 'sawtooth', [2, 0], 0.13, 0.06, 0.88, 3, 4, 4], scout: [400, 'square', [1, 3], 0.045, 0.035, 1.05, 4, 6, 9],
  };
  R.talk = (v, out, t, p) => {
    const r = ROLES[p.role] || ROLES.shopper, rs = mulberry(((p.seed == null ? 1 : hashSeed(p.seed)) >>> 0));
    const cnt = r[6] + Math.floor(rs() * (r[7] - r[6] + 1));
    let tt = t;
    for (let i = 0; i < cnt; i++) {
      const f = r[0] * p.pitch * Math.pow(2, (Math.floor(rs() * 7) - 2) / 12), len = r[3] * (0.8 + rs() * 0.5);
      const o = osc(v, r[1], f, tt); o.frequency.linearRampToValueAtTime(f * r[5], tt + len);
      const g = gain(0); env(g, tt, 0.005, 0.7, len + 0.02);
      VOW[r[2][i % 2]].forEach((fc, k) => { const bp = filt('bandpass', fc, r[8] + k * 2), bg = gain(k ? 0.6 : 1.4); o.connect(bp); bp.connect(bg); bg.connect(g); });
      g.connect(out);
      tt += len + r[4] + rs() * 0.03;
    }
    return tt - t + 0.08;
  };
  R.warp = (v, out, t, p) => {
    const dur = 1.3;
    const b = nb1(v, out, t, 'bandpass', 200, 1.5, dur, 0.5, 0.35); b.frequency.setValueAtTime(200, t); b.frequency.exponentialRampToValueAtTime(3500 * p.pitch, t + 0.6); b.frequency.exponentialRampToValueAtTime(300, t + dur);
    const o = osc(v, 'sawtooth', 60, t), lp = filt('lowpass', 400, 0.7), g = gain(0);
    o.frequency.exponentialRampToValueAtTime(260 * p.pitch, t + 0.55); o.frequency.exponentialRampToValueAtTime(50, t + dur);
    lp.frequency.setValueAtTime(300, t); lp.frequency.exponentialRampToValueAtTime(1500, t + 0.55); lp.frequency.exponentialRampToValueAtTime(200, t + dur);
    env(g, t, 0.3, 0.3, dur); o.connect(lp); lp.connect(g); g.connect(out);
    return dur + 0.1;
  };
  R.questFail = (v, out, t, p) => { tn(v, out, t, 'triangle', 392 * p.pitch, 0.35, 0.3, 2500); tn(v, out, t + 0.2, 'triangle', 262 * p.pitch, 0.7, 0.3, 2000); return 1; };

  // ---- siren loop (persistent, created on first use; wobbly two-tone, quieter than the engine) ----
  let SR = null;
  function siren(level) {
    const t = now();
    if (!SR) {
      const o = ctx.createOscillator(), o2 = ctx.createOscillator(), lfo = ctx.createOscillator(), lg = gain(120), sm = filt('lowpass', 5, 0.5), wob = ctx.createOscillator(), wg = gain(14);
      o.type = 'triangle'; o2.type = 'sine'; o.frequency.value = 740; o2.frequency.value = 740; o2.detune.value = 7; lfo.type = 'square'; lfo.frequency.value = 1.4; wob.frequency.value = 6;
      const lp = filt('lowpass', 2200, 0.7), sg = gain(0.1), sj = gain(0);
      lfo.connect(sm); sm.connect(lg); lg.connect(o.frequency); lg.connect(o2.frequency); wob.connect(wg); wg.connect(o.frequency); wg.connect(o2.frequency);
      o.connect(lp); o2.connect(lp); lp.connect(sg); sg.connect(sj); sj.connect(bus);
      o.start(t); o2.start(t); lfo.start(t); wob.start(t);
      SR = { o, o2, lfo, wob, sg, sj };
    }
    const l = Math.max(0, Math.min(1, level == null ? 0.6 : level));
    SR.sg.gain.setTargetAtTime(0.03 + l * 0.07, t, 0.05);
    SR.sj.gain.cancelScheduledValues(t); SR.sj.gain.setTargetAtTime(1, t, 0.04);
  }
  function sirenStop() {
    if (!SR) return;
    const t = now();
    SR.sj.gain.cancelScheduledValues(t); SR.sj.gain.setValueAtTime(SR.sj.gain.value, t); SR.sj.gain.linearRampToValueAtTime(0, t + 0.2);
  }

  // ---- procedural music (11 persistent nodes + transient note voices) ----
  const MODES = { lydian: [0, 2, 4, 6, 7, 9, 11], dorian: [0, 2, 3, 5, 7, 9, 10], aeolian: [0, 2, 3, 5, 7, 8, 10] };
  // ---- UI / progression cues ----
  const tn = (v, out, t, type, f, len, amp, lp, a) => {
    const o = osc(v, type, f, t), g = gain(0); env(g, t, a || 0.004, amp, len);
    if (lp) { const l = filt('lowpass', lp, 0.7); o.connect(l); l.connect(g); } else o.connect(g);
    g.connect(out); return o;
  };
  R.craft = (v, out, t, p) => {
    const n = noise(v, t), hp = filt('highpass', 3000, 0.7), ng = gain(0); env(ng, t, 0.001, 0.28, 0.03); n.connect(hp); hp.connect(ng); ng.connect(out);
    tn(v, out, t, 'square', 1900 * p.pitch, 0.04, 0.15, 5000, 0.001);
    [1, 2.76, 5.4].forEach((r, i) => { const o = osc(v, 'sine', 900 * p.pitch * r, t), g = gain(0); env(g, t, 0.001, 0.2 / (i + 1), 0.28 - i * 0.07); o.connect(g); g.connect(out); });
    tn(v, out, t + 0.09, 'sine', 1568 * p.pitch, 0.6, 0.2); tn(v, out, t + 0.09, 'sine', 2349 * p.pitch, 0.5, 0.1);
    return 0.75;
  };
  R.questAccept = (v, out, t, p) => { tn(v, out, t, 'triangle', 523 * p.pitch, 0.25, 0.3, 3500); tn(v, out, t + 0.12, 'triangle', 784 * p.pitch, 0.4, 0.3, 3500); return 0.55; };
  R.questDone = (v, out, t, p) => {
    [523, 659, 784].forEach((f, i) => tn(v, out, t + i * 0.13, 'triangle', f * p.pitch, 0.45, 0.28, 4000));
    tn(v, out, t + 0.39, 'sine', 1568 * p.pitch, 0.7, 0.15); return 1.1;
  };
  R.harvest = (v, out, t, p) => {
    [0, 0.09, 0.18].forEach((d, i) => {
      const o = osc(v, 'square', (220 + i * 40) * p.pitch, t + d); o.frequency.exponentialRampToValueAtTime(70, t + d + 0.06);
      const l = filt('lowpass', 1400, 1), g = gain(0); env(g, t + d, 0.002, 0.4, 0.07); o.connect(l); l.connect(g); g.connect(out);
    });
    const o = osc(v, 'sine', 620 * p.pitch, t + 0.3); o.frequency.exponentialRampToValueAtTime(160, t + 0.4);
    const g = gain(0); env(g, t + 0.3, 0.003, 0.5, 0.14); o.connect(g); g.connect(out); return 0.5;
  };
  R.tame = (v, out, t, p) => {
    const f = 520 * p.pitch, o = osc(v, 'sine', f, t); o.frequency.linearRampToValueAtTime(f * 1.25, t + 0.3); o.frequency.linearRampToValueAtTime(f * 0.95, t + 0.6);
    const lf = osc(v, 'sine', 9, t), lg = gain(f * 0.05); lf.connect(lg); lg.connect(o.frequency);
    const g = gain(0); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.28, t + 0.1); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.65);
    o.connect(g); g.connect(out); return 0.7;
  };
  R.hire = (v, out, t, p) => { tn(v, out, t, 'square', 660 * p.pitch, 0.18, 0.13, 3000); tn(v, out, t + 0.14, 'square', 880 * p.pitch, 0.3, 0.13, 3000); return 0.5; };
  R.levelUp = (v, out, t, p) => {
    [523, 659, 784, 1047].forEach((f, i) => tn(v, out, t + i * 0.09, 'triangle', f * p.pitch, 0.5, 0.25, 4500));
    [1047, 1319, 1568].forEach((f) => tn(v, out, t + 0.36, 'sine', f * p.pitch, 1.1, 0.09, 0, 0.02)); return 1.5;
  };

  const PC = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
  const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);
  function parseKey(k) {
    if (typeof k === 'number') return ((Math.round(k) % 12) + 12) % 12;
    const m = /^([A-Ga-g])([#b]?)/.exec(String(k || 'D')); if (!m) return 2;
    return (PC[m[1].toUpperCase()] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + 12) % 12;
  }
  const M = { on: false, level: 0.5, pc: 2, mode: 'lydian', inten: 0.3, N: null, idx: 7, nArp: 0, nOst: 0, nPulse: 0, oi: 0, timer: null, kill: null, live: !(typeof OfflineAudioContext !== 'undefined' && ctx instanceof OfflineAudioContext) };
  const MTRIM = 0.1;
  function musicBuild() {
    const t = now();
    const mg = gain(0), arpSend = gain(0.7), oscA = ctx.createOscillator(), oscB = ctx.createOscillator();
    oscA.type = 'triangle'; oscB.type = 'sine';
    const fA = filt('lowpass', 500, 0.7), fB = filt('lowpass', 700, 0.7);
    const l1 = ctx.createOscillator(), l2 = ctx.createOscillator(), lg = gain(220), sub = ctx.createOscillator(), subG = gain(0.0001);
    l1.frequency.value = 0.07; l2.frequency.value = 0.113; sub.type = 'sine';
    l1.connect(lg); l2.connect(lg); lg.connect(fA.frequency); lg.connect(fB.frequency);
    oscA.connect(fA); oscB.connect(fB); fA.connect(mg); fB.connect(mg); sub.connect(subG); subG.connect(mg);
    mg.connect(bus); arpSend.connect(send);
    M.N = { mg, arpSend, oscA, oscB, fA, fB, l1, l2, lg, sub, subG };
    musicTune(0.001, true);
    [oscA, oscB, l1, l2, sub].forEach((o) => o.start(t));
  }
  function musicTune(tau, instant) {
    const N = M.N; if (!N) return; const t = now(), b = 36 + M.pc;
    const set = (p, v) => (instant ? p.setValueAtTime(v, t) : p.setTargetAtTime(v, t, tau));
    set(N.oscA.frequency, mtof(b + 12)); set(N.oscB.frequency, mtof(b + 19) * 1.003); set(N.sub.frequency, mtof(b));
    set(N.fA.frequency, 380 + M.inten * 900); set(N.fB.frequency, 520 + M.inten * 1100);
    N.lg.gain.setTargetAtTime(160 + M.inten * 260, t, 1.2);
  }
  function musicNote(f, t, att, rel, amp, type) {
    const N = M.N, o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type; o.frequency.value = f;
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(amp, t + att); g.gain.exponentialRampToValueAtTime(0.0001, t + att + rel);
    o.connect(g); g.connect(N.mg); g.connect(N.arpSend);
    o.start(t); o.stop(t + att + rel + 0.05);
    o.onended = () => { try { g.disconnect(); } catch (e) {} };
  }
  function musicSchedule(until) {
    const N = M.N; if (!N) return;
    const sc = MODES[M.mode] || MODES.lydian, b = 36 + M.pc, deg = (i) => sc[((i % 7) + 7) % 7] + 12 * Math.floor(i / 7), inten = M.inten;
    while (M.nArp < until) {
      const r = Math.random(); M.idx += r < 0.4 ? 1 : r < 0.8 ? -1 : r < 0.9 ? 2 : -2;
      M.idx = Math.max(3, Math.min(13, M.idx));
      musicNote(mtof(b + 24 + deg(M.idx - 7)), M.nArp, 0.35, 3.2, 0.2 + 0.1 * inten, 'sine');
      M.nArp += rnd(1.5, 3);
    }
    const per = 1.6 - inten * 0.85, amp = 0.05 + inten * 0.5;
    while (M.nPulse < until) {
      const tp = M.nPulse, g = N.subG.gain;
      g.setValueAtTime(0.0001, tp); g.linearRampToValueAtTime(amp, tp + 0.05); g.exponentialRampToValueAtTime(0.0001, tp + per * 0.85);
      M.nPulse += per;
    }
    if (inten > 0.6) {
      const a = (inten - 0.6) / 0.4;
      while (M.nOst < until) {
        musicNote(mtof(b + 24 + deg(M.oi++ % 2 ? 6 : 2)), M.nOst, 0.12, 1.6, 0.07 + 0.1 * a, 'triangle');
        M.nOst += 1.2 - 0.3 * a;
      }
    } else if (M.nOst < until) M.nOst = until;
  }
  const music = {
    schedule: musicSchedule,
    start() {
      if (M.kill) { clearTimeout(M.kill); M.kill = null; }
      if (!M.N) musicBuild();
      const t = now(); M.on = true;
      M.N.mg.gain.cancelScheduledValues(t); M.N.mg.gain.setTargetAtTime(M.level * MTRIM, t, 0.8);
      M.N.arpSend.gain.setTargetAtTime(0.7 * Math.min(1, M.level * 2), t, 0.1);
      M.nArp = Math.max(M.nArp, t + 0.3); M.nOst = Math.max(M.nOst, t + 0.3); M.nPulse = Math.max(M.nPulse, t + 0.1);
      musicSchedule(t + 2.5);
      if (M.live && !M.timer) M.timer = setInterval(() => musicSchedule(now() + 2.5), 400);
    },
    stop() {
      if (!M.on) return; M.on = false;
      if (M.timer) { clearInterval(M.timer); M.timer = null; }
      const t = now(), N = M.N; if (!N) return;
      N.mg.gain.cancelScheduledValues(t); N.mg.gain.setTargetAtTime(0, t, 0.35);
      if (M.live) M.kill = setTimeout(() => {
        const n = M.N; M.N = null; M.kill = null; if (!n) return;
        [n.oscA, n.oscB, n.l1, n.l2, n.sub].forEach((o) => { try { o.stop(); } catch (e) {} });
        try { n.mg.disconnect(); n.arpSend.disconnect(); } catch (e) {}
      }, 2500);
    },
    set(o = {}) {
      if (o.key != null) M.pc = parseKey(o.key);
      if (o.mode && MODES[o.mode]) M.mode = o.mode;
      if (o.intensity != null) M.inten = Math.max(0, Math.min(1, o.intensity));
      musicTune(1.3, false);
    },
    setLevel(v) {
      M.level = Math.max(0, Math.min(1, v));
      if (M.N && M.on) { const t = now(); M.N.mg.gain.setTargetAtTime(M.level * MTRIM, t, 0.05); M.N.arpSend.gain.setTargetAtTime(0.7 * Math.min(1, M.level * 2), t, 0.05); }
    },
  };

  // ---------- ambience beds (14 persistent nodes, built on first set()) ----------
  const PL = { space: 0, atmo: 1, surface: 2, interior: 3, station: 4, store: 5 };
  const WX = { clear: 0, wind: 1, storm: 2, aurora: 3 };
  const BI = { lush: 0, rocky: 1, icy: 2, gas: 3 };
  const AS = { N: null, sig: -1, place: -1, I: 0.5, w: 0, pad: 0, level: 1, evs: 0, timer: null, seed: 7, jn: 0,
    nx: { gust: 0, chirp: 0, call: 0, tick: 0, shim: 0, knock: 0, pa: 0, jingle: 0, drip: 0, swell: 0 },
    live: !(typeof OfflineAudioContext !== 'undefined' && ctx instanceof OfflineAudioContext) };
  const PENT = [0, 2, 4, 7, 9, 12];
  function ambBuild() {
    const t = now(), out = gain(0), nA = ctx.createBufferSource(), wF = filt('lowpass', 400, 0.7), wG = gain(0);
    const nB = ctx.createBufferSource(), tF = filt('bandpass', 2400, 0.8), tG = gain(0);
    const hA = ctx.createOscillator(), hB = ctx.createOscillator(), hF = filt('lowpass', 300, 0.7), hG = gain(0);
    const pA = ctx.createOscillator(), pB = ctx.createOscillator(), pG = gain(0);
    nA.buffer = nb; nA.loop = true; nB.buffer = nb; nB.loop = true;
    hA.type = hB.type = 'sawtooth'; pA.type = pB.type = 'sine';
    hA.frequency.value = 40; hB.frequency.value = 40.4; pA.frequency.value = 262; pB.frequency.value = 392;
    nA.connect(wF); wF.connect(wG); wG.connect(out); nB.connect(tF); tF.connect(tG); tG.connect(out);
    hA.connect(hF); hB.connect(hF); hF.connect(hG); hG.connect(out); pA.connect(pG); pB.connect(pG); pG.connect(out);
    out.connect(bus); out.gain.setValueAtTime(0.0001, t); out.gain.setTargetAtTime(AS.level, t, 0.5);
    nA.start(t, 0.2); nB.start(t, 1.1); hA.start(t); hB.start(t); pA.start(t); pB.start(t);
    AS.N = { out, nA, nB, wF, wG, tF, tG, hA, hB, hF, hG, pA, pB, pG };
  }
  function ambEv(t, type, f0, f1, len, amp, lp) {
    if (AS.evs > 20) return;
    const o = ctx.createOscillator(), g = ctx.createGain(); o.type = type; o.frequency.setValueAtTime(f0, t);
    if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(f1, t + len);
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(amp, t + Math.min(0.004, len * 0.3)); g.gain.exponentialRampToValueAtTime(0.0001, t + len);
    if (lp) { const l = ctx.createBiquadFilter(); l.frequency.value = lp; o.connect(l); l.connect(g); } else o.connect(g);
    g.connect(AS.N.out); AS.evs++;
    o.onended = () => { AS.evs--; try { g.disconnect(); } catch (e) {} };
    o.start(t); o.stop(t + len + 0.03);
  }
  function ambTune() {
    const N = AS.N, t = now(), tau = 0.5, p = AS.place, I = AS.I, wx = AS.wx, bi = AS.bi, night = AS.night;
    const indoor = p >= 3, k = indoor ? 0.3 : 1;
    let w = 0, wf = 300, hum = 40, hg = 0, hf = 200, pf1 = 262, pf2 = 392, pg = 0;
    if (p === 0) { hum = 38; hg = 0.012; hf = 120; }
    else if (p === 1) { w = 0.2 + 0.2 * I; wf = 350 + 700 * I; }
    else if (p === 2) {
      if (bi === 0) { w = 0.04; wf = 500; } else if (bi === 1) { w = 0.12; wf = 600; }
      else if (bi === 2) { w = 0.12; wf = 1100; pf1 = 2637; pf2 = 3135; pg = 0.012; } else { w = 0.28; wf = 140; hum = 30; hg = 0.03; hf = 90; }
    } else if (p === 3) { hum = 72; hg = 0.035; hf = 400; }
    else if (p === 4) { hum = 55; hg = 0.05; hf = 600; w = 0.02; wf = 300; }
    else { hum = 100; hg = 0.025; hf = 500; }
    let tg = 0;
    if (wx === 1) { w += 0.12 * k; wf += 300 * k; }
    else if (wx === 2) { w += 0.18 * k; wf += 400 * k; tg = 0.22 * k * (0.5 + 0.5 * I); }
    else if (wx === 3 && !indoor && night > 0.01) { pf1 = 262; pf2 = 392; pg = 0.05 * night; }
    AS.w = w; AS.pad = pg; AS.wf = wf; AS.drip = wx === 2 ? 5 + 15 * I * k : 0;
    N.wG.gain.setTargetAtTime(w, t, tau); N.wF.frequency.setTargetAtTime(wf, t, tau);
    N.tG.gain.setTargetAtTime(tg, t, tau);
    N.hA.frequency.setTargetAtTime(hum, t, tau); N.hB.frequency.setTargetAtTime(hum * 1.01, t, tau);
    N.hF.frequency.setTargetAtTime(hf, t, tau); N.hG.gain.setTargetAtTime(hg, t, tau);
    N.pA.frequency.setTargetAtTime(pf1, t, tau); N.pB.frequency.setTargetAtTime(pf2 * 1.004, t, tau); N.pG.gain.setTargetAtTime(pg, t, tau);
  }
  function ambSchedule(until) {
    const N = AS.N; if (!N) return; const nx = AS.nx, p = AS.place, bi = AS.bi, wx = AS.wx, I = AS.I, t0 = now();
    const on = (k, cond) => { if (!cond) { if (nx[k] < until) nx[k] = until; return false; } if (nx[k] < t0 - 0.5) nx[k] = t0; return true; };
    if (on('gust', AS.w > 0.05 || p === 1)) while (nx.gust < until) {
      const t = nx.gust, f = AS.wf, w = AS.w, big = wx === 2 ? 1.5 : 1;
      N.wF.frequency.setTargetAtTime(f * 1.5 * big, t, 0.6); N.wG.gain.setTargetAtTime(w * 1.4, t, 0.6);
      N.wF.frequency.setTargetAtTime(f, t + 1.6, 0.8); N.wG.gain.setTargetAtTime(w, t + 1.6, 0.8);
      nx.gust += (p === 2 && bi === 3 ? 7 : rnd(3, 8)) / (wx === 2 ? 2 : 1);
    }
    if (on('chirp', p === 2 && bi === 0)) while (nx.chirp < until) {
      const n = 3 + ((Math.random() * 4) | 0), f = rnd(4200, 6500);
      for (let i = 0; i < n; i++) ambEv(nx.chirp + i * 0.07, 'sine', f, f * 1.3, 0.05, 0.03);
      nx.chirp += rnd(0.5, 1.8);
    }
    if (on('call', p === 2 && bi === 0)) while (nx.call < until) {
      const f = rnd(500, 900); ambEv(nx.call, 'sine', f, f * 0.6, 0.5, 0.05, 1500); ambEv(nx.call + 0.25, 'sine', f * 1.2, f * 0.7, 0.4, 0.035, 1500);
      nx.call += rnd(6, 14);
    }
    if (on('tick', p === 2 && bi === 1)) while (nx.tick < until) { const f = rnd(1500, 3000); ambEv(nx.tick, 'square', f, f * 0.5, 0.02, 0.025, 4000); nx.tick += rnd(0.5, 2.5); }
    if (on('shim', p === 2 && bi === 2)) while (nx.shim < until) { const f = rnd(3000, 6000); ambEv(nx.shim, 'sine', f, f, 1.2, 0.02); nx.shim += rnd(1.5, 3); }
    if (on('knock', p === 3)) while (nx.knock < until) { const t = nx.knock; ambEv(t, 'sine', 160, 90, 0.12, 0.2); ambEv(t + 0.16, 'sine', 140, 80, 0.1, 0.14); nx.knock += rnd(9, 20); }
    if (on('pa', p === 4)) while (nx.pa < until) { const t = nx.pa; ambEv(t, 'triangle', 1046, 1046, 0.18, 0.06, 2500); ambEv(t + 0.2, 'triangle', 784, 784, 0.25, 0.06, 2500); nx.pa += rnd(20, 40); }
    if (on('jingle', p === 5)) while (nx.jingle < until) {
      const r = mulberry(hashSeed(AS.seed));
      for (let i = 0; i < 4; i++) { const f = mtof(72 + PENT[(r() * 6) | 0]); ambEv(nx.jingle + i * 0.22, 'triangle', f, f, 0.35, 0.06, 3000); }
      nx.jingle += 45;
    }
    if (on('drip', AS.drip > 0)) while (nx.drip < until) { const f = rnd(1800, 4500); ambEv(nx.drip, 'sine', f, f * 0.6, 0.025, 0.03 * (0.5 + 0.5 * I)); nx.drip += 1 / (AS.drip * rnd(0.6, 1.4)); }
    if (on('swell', AS.pad > 0.002)) while (nx.swell < until) { N.pG.gain.setTargetAtTime(AS.pad * rnd(0.3, 1), nx.swell, 2.5); nx.swell += 5; }
  }
  const ambience = {
    schedule: ambSchedule,
    set(o = {}) {
      const p = PL[o.place] ?? 0, wx = WX[o.weather] ?? 0, bi = BI[o.biome] ?? 0, night = Math.max(0, Math.min(1, o.night || 0)), I = o.intensity == null ? 0.5 : Math.max(0, Math.min(1, o.intensity));
      const sig = p + 8 * (wx + 4 * (bi + 4 * (Math.round(night * 20) + 21 * Math.round(I * 20))));
      if (o.seed != null) AS.seed = o.seed;
      if (sig === AS.sig && AS.N) return;
      if (!AS.N) ambBuild();
      const t = now(), changed = p !== AS.place;
      AS.sig = sig; AS.wx = wx; AS.bi = bi; AS.night = night; AS.I = I;
      if (changed) { AS.place = p; if (p === 3) AS.nx.knock = t + rnd(3, 8); if (p === 4) AS.nx.pa = t + rnd(6, 16); if (p === 5) AS.nx.jingle = t + rnd(3, 6); }
      ambTune();
      if (AS.live && !AS.timer) AS.timer = setInterval(() => ambSchedule(now() + 1.2), 250);
      if (AS.live) ambSchedule(t + 1.2);
    },
    stop() {
      if (AS.timer) { clearInterval(AS.timer); AS.timer = null; }
      if (AS.N) AS.N.out.gain.setTargetAtTime(0, now(), 0.4);
      AS.sig = -1;
    },
    setLevel(v) { AS.level = Math.max(0, Math.min(1, v)); if (AS.N) AS.N.out.gain.setTargetAtTime(AS.level, now(), 0.05); },
  };

  function steal(list, tt) {
    const old = list.shift();
    if (!old) return;
    old.out.gain.cancelScheduledValues(tt);
    old.out.gain.setValueAtTime(old.out.gain.value, tt);
    old.out.gain.linearRampToValueAtTime(0, tt + 0.008);
    for (const s of old.srcs) { try { s.stop(tt + 0.012); } catch (e) {} }
  }

  function play(name, o = {}) {
    const t = now() + 0.005;
    if (name === 'entryStop') { if (entryVoice) { entryVoice.release(t); stopLater(entryVoice, t + 0.55); entryVoice = null; } return; }
    if (name === 'jetpack') return jetpack(o.level);
    if (name === 'jetpackStop') return jetpackStop();
    if (name === 'siren') return siren(o.level);
    if (name === 'sirenStop') return sirenStop();
    if (o.stop && name === 'entry') return play('entryStop');
    const rec = R[name]; if (!rec) return;
    const dist = o.dist || 0, dg = 1 / (1 + dist / 40);
    if (dg < 0.02) return;
    const list = voices[name] || (voices[name] = []);
    while (list.length >= (CAPS[name] || 4)) steal(list, t);
    const v = tracker();
    const out = gain(dg * (o.vel == null ? 1 : 0.35 + 0.65 * o.vel));
    out.connect(bus); v.out = out;
    const p = { pitch: (o.pitch || 1) * rnd(0.94, 1.06), twin: !!o.twin, seed: o.seed, n: o.n, level: o.level, role: o.role, size: o.size };
    const dur = rec(v, out, t, p);
    startAll(v, t, dur);
    list.push(v);
    if (name === 'entry') { if (entryVoice) { entryVoice.release(t); } entryVoice = v; }
    stopLater(v, t + dur + 0.1, list);
    return v;
  }
  // voice list cleanup: drive from sources' onended
  function stopLater(v, tEnd, list) {
    const first = v.srcs[0];
    if (!first) return;
    const last = v.srcs[v.srcs.length - 1];
    if (v.release) for (const s of v.srcs) { try { s.stop(tEnd); } catch (e) {} }
    last.onended = () => {
      try { v.out.disconnect(); } catch (e) {}
      for (const k in voices) { const i = voices[k].indexOf(v); if (i >= 0) voices[k].splice(i, 1); }
    };
  }

  // ---------- continuous engine drone (10 nodes, always alive) ----------
  const E = (() => {
    const t = now();
    const sawA = ctx.createOscillator(), sawB = ctx.createOscillator(), oct = ctx.createOscillator();
    sawA.type = sawB.type = oct.type = 'sawtooth';
    sawA.frequency.value = 52; sawB.frequency.value = 52; sawB.detune.value = 9; oct.frequency.value = 104;
    const octG = gain(0);
    const nz = ctx.createBufferSource(); nz.buffer = nb; nz.loop = true;
    const nG = gain(0.0);
    const lp = filt('lowpass', 150, 0.9);
    const eg = gain(0);
    const shBP = filt('bandpass', 5000, 6), shG = gain(0);
    sawA.connect(lp); sawB.connect(lp); oct.connect(octG); octG.connect(lp);
    nz.connect(nG); nG.connect(lp); lp.connect(eg); eg.connect(bus);
    nz.connect(shBP); shBP.connect(shG); shG.connect(bus);
    sawA.start(t); sawB.start(t); oct.start(t); nz.start(t, 0.3);
    return { sawA, sawB, oct, octG, nG, lp, eg, shBP, shG };
  })();
  function engine(s = {}) {
    const th = Math.max(0, Math.min(1, s.throttle || 0)), pu = Math.max(0, Math.min(1, s.pulse || 0));
    const bo = s.boost ? 1 : 0, at = s.inAtmo ? 1 : 0, t = now(), k = 0.08;
    const f = 48 + th * 34 + bo * 8 + pu * 70;
    E.sawA.frequency.setTargetAtTime(f, t, k);
    E.sawB.frequency.setTargetAtTime(f * 1.004, t, k);
    E.oct.frequency.setTargetAtTime(f * 2, t, k);
    E.octG.gain.setTargetAtTime(bo * 0.55, t, 0.06);
    E.lp.frequency.setTargetAtTime(130 + th * th * 1700 + bo * 900 + pu * 2200 + at * 200, t, k);
    E.eg.gain.setTargetAtTime(0.06 + th * 0.26 + bo * 0.1 + pu * 0.1, t, k);
    E.nG.gain.setTargetAtTime(0.03 + th * 0.05 + at * (0.12 + th * 0.2), t, 0.15);
    E.shBP.frequency.setTargetAtTime(4200 + pu * 3800, t, k);
    E.shG.gain.setTargetAtTime(pu * 0.18, t, 0.05);
  }
  return { ctx, bus, master, play, engine, E, voices, music, ambience };
}

// ---------- public API ----------
export function createAudio(opts = {}) {
  let masterVal = opts.master == null ? 0.6 : opts.master;
  let core = null, musicLevel = 0.5, musicWanted = false, musicSet = {};
  const music = {
    start() { musicWanted = true; if (core) core.music.start(); },
    stop() { musicWanted = false; if (core) core.music.stop(); },
    set(o) { Object.assign(musicSet, o || {}); if (core) core.music.set(o); },
  };
  let ambSet = null, ambLevel = 1;
  const ambience = {
    set(o) { ambSet = o || {}; if (core) core.ambience.set(ambSet); },
    stop() { ambSet = null; if (core) core.ambience.stop(); },
    setLevel(v) { ambLevel = v; if (core) core.ambience.setLevel(v); },
  };
  const api = {
    music, ambience,
    ready: false,
    unlock() {
      try {
        if (!core) {
          const AC = window.AudioContext || window.webkitAudioContext;
          if (!AC) return false;
          const ctx = new AC({ latencyHint: 'interactive' });
          core = buildCore(ctx, masterVal);
          core.music.setLevel(musicLevel); core.music.set(musicSet);
          core.ambience.setLevel(ambLevel); if (ambSet) core.ambience.set(ambSet);
        }
        if (core.ctx.state === 'suspended') core.ctx.resume().catch(() => {});
        if (musicWanted) core.music.start();
        api.ready = true;
        return true;
      } catch (e) { console.warn('ship-audio unlock failed', e); return false; }
    },
    play(name, o) {
      if (!core) return;
      if (core.ctx.state === 'suspended') core.ctx.resume().catch(() => {});
      core.play(name, o);
    },
    engine(state) { if (core) core.engine(state); },
    setMusic(v) { musicLevel = Math.max(0, Math.min(1, v)); if (core) core.music.setLevel(musicLevel); },
    setMaster(v) {
      masterVal = Math.max(0, Math.min(1, v));
      if (core) core.master.gain.setTargetAtTime(masterVal, core.ctx.currentTime, 0.02);
    },
  };
  return api;
}

// Offline render for tests. name: a recipe name, or 'engineIdle'|'engineBoost'|'enginePulse'
export async function renderPreview(name, seconds = 2) {
  const sr = 44100, ctx = new OfflineAudioContext(1, Math.ceil(sr * seconds), sr);
  const core = buildCore(ctx, 0.6);
  if (name === 'engineIdle') core.engine({ throttle: 0 });
  else if (name === 'engineBoost') core.engine({ throttle: 1, boost: true, inAtmo: true });
  else if (name === 'enginePulse') core.engine({ throttle: 0.6, pulse: 1 });
  else if (name.startsWith('amb:')) {
    const [, pl, wx, bi, ni] = name.split(':');
    core.ambience.set({ place: pl, weather: wx || 'clear', biome: bi || 'lush', night: ni ? +ni : 0, intensity: 0.7 }); core.ambience.schedule(seconds + 1);
  } else if (name.startsWith('music')) {
    const i = name === 'musicLow' ? 0.2 : name === 'musicHigh' ? 0.9 : parseFloat(name.split(':')[1]) || 0.5;
    core.music.set({ intensity: i }); core.music.start(); core.music.schedule(seconds + 1);
  } else if (name === 'sirenCycle') { core.play('siren', { level: 0.8 }); core.play('siren', { level: 0.3 }); core.play('sirenStop'); core.play('sirenStop'); }
  else if (name.startsWith('talk:')) { core.play('talk', { role: name.slice(5), seed: 7 }); }
  else { core.play(name, { twin: name === 'fire', n: 5, seed: 7 }); }
  const buf = await ctx.startRendering();
  return buf.getChannelData(0);
}
