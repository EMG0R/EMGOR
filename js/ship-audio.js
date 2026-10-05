// ship-audio.js - procedural WebAudio for ship mode. No files, no deps.
// createAudio() -> { unlock(), play(name, opts), engine(state), setMaster(v), ready }
// play opts: { dist (ship lengths), pitch (ratio), vel 0..1, twin, stop }
// + one-shots: windup stall overheat ram scan shieldHit shieldBreak units(n) buy npc(seed) shard; loops: play('jetpack',{level}) / 'jetpackStop'
// audio.music = { start(), stop(), set({key, mode:'lydian'|'dorian'|'aeolian', intensity}) }; setMusic(v)
// renderPreview(name, seconds) -> Float32Array (OfflineAudioContext, for testing)

const CAPS = { fire: 6, hit: 8, crit: 4, kill: 4, limbSever: 4, bossRoar: 2, explosion: 4, entry: 1, land: 2, liftoff: 2, ui: 4, windup: 3, stall: 3, overheat: 1, ram: 2, scan: 2, shieldHit: 6, shieldBreak: 3, units: 4, buy: 2, npc: 3, shard: 6 };
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

  // ---- procedural music (11 persistent nodes + transient note voices) ----
  const MODES = { lydian: [0, 2, 4, 6, 7, 9, 11], dorian: [0, 2, 3, 5, 7, 9, 10], aeolian: [0, 2, 3, 5, 7, 8, 10] };
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
    if (o.stop && name === 'entry') return play('entryStop');
    const rec = R[name]; if (!rec) return;
    const dist = o.dist || 0, dg = 1 / (1 + dist / 40);
    if (dg < 0.02) return;
    const list = voices[name] || (voices[name] = []);
    while (list.length >= (CAPS[name] || 4)) steal(list, t);
    const v = tracker();
    const out = gain(dg * (o.vel == null ? 1 : 0.35 + 0.65 * o.vel));
    out.connect(bus); v.out = out;
    const p = { pitch: (o.pitch || 1) * rnd(0.94, 1.06), twin: !!o.twin, seed: o.seed, n: o.n, level: o.level };
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
  return { ctx, bus, master, play, engine, E, voices, music };
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
  const api = {
    music,
    ready: false,
    unlock() {
      try {
        if (!core) {
          const AC = window.AudioContext || window.webkitAudioContext;
          if (!AC) return false;
          const ctx = new AC({ latencyHint: 'interactive' });
          core = buildCore(ctx, masterVal);
          core.music.setLevel(musicLevel); core.music.set(musicSet);
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
  else if (name.startsWith('music')) {
    const i = name === 'musicLow' ? 0.2 : name === 'musicHigh' ? 0.9 : parseFloat(name.split(':')[1]) || 0.5;
    core.music.set({ intensity: i }); core.music.start(); core.music.schedule(seconds + 1);
  } else { core.play(name, { twin: name === 'fire', n: 5, seed: 7 }); }
  const buf = await ctx.startRendering();
  return buf.getChannelData(0);
}
