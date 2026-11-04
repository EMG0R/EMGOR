// ship-music.js - generative soundtrack (Q19). Two layers in one file:
//   createScheduler(seed) -> { tick(dt, st) -> events[] }   PURE, seeded (no Math.random, no AudioContext), node-tested
//   createMusic(audio)    -> { update(dt, st), stop() }     Web Audio player, lazy, built on ship-audio's bus via audio.graph()
// st = { biome:'lush'|'icy'|'rocky'|'gas'|..., band:'space'|'orbit'|'atmo'|'surface'|'interior', night 0..1, danger 0..1, depth (fractal), speed 0..1, wind 0..1, seed int, mute bool }
// events (shared objects, valid until the next tick): { kind:'pad'|'note'|'drone'|'pulse', freq Hz, dur s, amp 0..1, voice, atk s, rel s }
//   pulse = the danger layer: ONE sustained low fifth (dur = pulse period s), amp 0 = off. Emitted on danger > 0.5 / <= 0.35 edges only. Never a melody.
//   depth lowers every pitch by an octave per level (cap 2, floored at midi 21 by octave lift); harmony drifts on a 4-chord cycle per mode (60-90 s).
// Volume rides ship-audio's master (/volume). Muted = every node stopped and update() returns immediately (zero idle CPU).

export const MODES = {
  lush: [0, 2, 4, 6, 7, 9, 11],      // Lydian
  icy: [0, 2, 4, 6, 7, 9, 10],       // Lydian b7
  rocky: [0, 2, 3, 5, 7, 9, 10],     // Dorian
  gas: [0, 2, 4, 6, 8, 10],          // whole tone
  space: [0, 3, 5, 8, 10],           // quartal stack (fourths folded into an octave)
};
const BIOME_MODE = { lush: 'lush', sea: 'lush', icy: 'icy', rocky: 'rocky', lava: 'rocky', gas: 'gas' };
// Slow harmonic motion: 4 chord roots as scale-degree indices (Lydian: I -> II -> vi -> I), BAR_S seconds per chord -> cycle 64-88 s
export const CHORDS = { lush: [0, 1, 5, 0], icy: [0, 1, 5, 0], rocky: [0, 3, 6, 0], gas: [0, 2, 4, 2], space: [0, 1, 3, 1] };
export const BAR_S = { lush: 18, icy: 20, rocky: 16, gas: 21, space: 22 };
export const PAD_ATK_MIN = 1.5, PAD_REL_MIN = 3, NOTE_ATK = 0.08, DEFAULT_SEED = 1, MIDI_FLOOR = 21;
export function chordAt(mode, t) { var c = CHORDS[mode] || CHORDS.rocky; return c[Math.floor(Math.max(0, t) / (BAR_S[mode] || 16)) % c.length]; }
export function seedOf(st) { var x = st && st.seed; return typeof x === 'number' && isFinite(x) ? x | 0 : DEFAULT_SEED; }
export function depthShift(st) { var d = st && +st.depth; return -12 * (d === d ? Math.min(2, Math.max(0, Math.floor(d))) : 0); }
function lift(m) { while (m < MIDI_FLOOR) m += 12; return m; }
// pad: slow swell in (>= 1.5 s, 40 % of dur), slow release (>= 3 s, 60 % of dur); atk + rel <= dur always
export function envelope(kind, dur) { if (kind === 'pad') { var a = Math.max(PAD_ATK_MIN, dur * 0.4); return { atk: a, rel: Math.max(PAD_REL_MIN, dur - a) }; } return { atk: NOTE_ATK, rel: Math.max(0.5, dur - NOTE_ATK) }; }
// interior = filtered way down; the player has no reverb send of its own, so interior is also reverb-less by construction
export function lpFreq(st) { var n = st.night || 0; return st.band === 'interior' ? 500 + 300 * (1 - n) : 1200 + 2000 * (1 - n); }
export const PAD_MAX = 4, NOTE_MIN_S = 6, NOTE_MAX_S = 14, PAD_MIN_S = 20, PAD_MAX_S = 40;

export function modeName(st) { return st.band === 'space' ? 'space' : (BIOME_MODE[st.biome] || 'rocky'); }
export function keyOf(st) { return modeName(st) + '|' + st.band; }
export function mtof(m) { return 440 * Math.pow(2, (m - 69) / 12); }
function clamp(x, a, b) { return x < a ? a : x > b ? b : x; }
function mulberry(a) { return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; var t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }

export function createScheduler(seed) {
  var rng = mulberry((seed | 0) ^ 0x51ED270B), pool = [], out = [], i;
  for (i = 0; i < 8; i++) pool.push({ kind: '', freq: 0, dur: 0, amp: 0, voice: 0, atk: 0, rel: 0 });
  var padLeft = [0, 0, 0, 0], padDeg = [0, 0, 0, 0], clock = 0, pulseOn = false, pulseSeed = null, noteT = NOTE_MIN_S + rng() * (NOTE_MAX_S - NOTE_MIN_S), walk = 0, droneSeed = null, k = 0;
  function ev(kind, freq, dur, amp, voice) { var e = pool[k++ & 7], en = envelope(kind, dur); e.kind = kind; e.freq = freq; e.dur = dur; e.amp = amp; e.voice = voice; e.atk = en.atk; e.rel = en.rel; out.push(e); return e; }
  return {
    tick: function (dt, st) {
      out.length = 0; k = 0;
      var mn = modeName(st), scale = MODES[mn], n = scale.length, sd = seedOf(st), root = 33 + ((sd >>> 0) % 12), sh = depthShift(st), v, r;
      clock += dt; var chord = chordAt(mn, clock);
      if (droneSeed !== sd) { droneSeed = sd; ev('drone', mtof(lift(root + sh)), 0, 0.5, 0); }   // sub drone sits on the planet's seed (a1..g#2 region); NaN seeds map to DEFAULT_SEED so this fires once
      var danger = clamp(st.danger || 0, 0, 1), voices = st.band === 'space' || st.band === 'interior' ? 2 : 3;
      if (danger > 0.5 && (!pulseOn || pulseSeed !== sd)) { pulseOn = true; pulseSeed = sd; ev('pulse', mtof(lift(root + 7 + sh)), 1.2, 0.09, 4); }   // the danger layer: one low fifth above the drone, tremolo period 1.2 s
      else if (danger <= 0.35 && pulseOn) { pulseOn = false; ev('pulse', mtof(lift(root + 7 + sh)), 1.2, 0, 4); }
      var active = 0;
      for (v = 0; v < PAD_MAX; v++) { if (padLeft[v] > 0) { padLeft[v] -= dt; if (padLeft[v] > 0) active++; else padLeft[v] = 0; } }
      for (v = 0; v < voices && active < voices; v++) {
        if (padLeft[v] > 0) continue;
        r = rng(); var dur = PAD_MIN_S + (PAD_MAX_S - PAD_MIN_S) * rng();
        // chord tones: stacked scale steps (root, 3rd, 5th, 7th-ish) in the pad octave; space = the quartal set as-is
        padDeg[v] = (chord + v * 2 + Math.floor(r * 2)) % n;
        padLeft[v] = dur; active++;
        ev('pad', mtof(lift(root + 12 + sh + scale[padDeg[v]] + (v >= 2 ? 12 : 0))), dur, 0.16 * (1 - 0.35 * (st.night || 0)), v);
      }
      noteT -= dt;
      if (noteT <= 0) {
        var iv = NOTE_MAX_S - (NOTE_MAX_S - NOTE_MIN_S) * danger;
        if (st.band === 'interior') iv *= 1.25;
        noteT = clamp(iv * (0.85 + 0.3 * rng()), NOTE_MIN_S, NOTE_MAX_S);
        walk = clamp(walk + (rng() < 0.5 ? -1 : 1) * (1 + Math.floor(rng() * 2)), -n, n * 2 - 1);
        var deg = ((walk % n) + n) % n, oct = Math.floor(walk / n);
        ev('note', mtof(lift(root + 24 + sh + scale[deg] + 12 * oct)), 2.5 + 2 * rng(), 0.10 + 0.08 * danger, 0);
      }
      return out;
    },
  };
}

// ---------------- Web Audio player ----------------
export function createMusic(audio) {
  var sch = null, seed = -1, on = false, key = '', fadeT = 0, windT = 0;
  var nodes = null;                      // { out, lp, windSrc, windBp, windG, drone, droneG, pulse, pulseG, pulseLfo }
  function build() {
    var gr = audio && audio.graph && audio.graph();
    if (!gr) return false;
    var ctx = gr.ctx, n = {}, i, sr = ctx.sampleRate, buf = ctx.createBuffer(1, sr * 2, sr), d = buf.getChannelData(0), s = 0x1234567;
    for (i = 0; i < d.length; i++) { s = (Math.imul(s, 1664525) + 1013904223) | 0; d[i] = (s >>> 8) / 8388608 - 1; }     // seeded noise: no Math.random
    n.ctx = ctx; n.out = ctx.createGain(); n.out.gain.value = 0; n.out.connect(gr.bus);
    n.lp = ctx.createBiquadFilter(); n.lp.type = 'lowpass'; n.lp.frequency.value = 2400; n.lp.connect(n.out);
    n.windSrc = ctx.createBufferSource(); n.windSrc.buffer = buf; n.windSrc.loop = true;
    n.windBp = ctx.createBiquadFilter(); n.windBp.type = 'bandpass'; n.windBp.frequency.value = 400; n.windBp.Q.value = 0.8;
    n.windG = ctx.createGain(); n.windG.gain.value = 0;
    n.windSrc.connect(n.windBp); n.windBp.connect(n.windG); n.windG.connect(n.out); n.windSrc.start();
    n.drone = ctx.createOscillator(); n.drone.type = 'sine'; n.drone.frequency.value = 55;
    n.droneG = ctx.createGain(); n.droneG.gain.value = 0.12; n.drone.connect(n.droneG); n.droneG.connect(n.out); n.drone.start();
    n.pulse = ctx.createOscillator(); n.pulse.type = 'sine'; n.pulse.frequency.value = 82;       // danger fifth: silent until a pulse event arrives
    n.pulseG = ctx.createGain(); n.pulseG.gain.value = 0; n.pulse.connect(n.pulseG); n.pulseG.connect(n.out); n.pulse.start();
    n.pulseLfo = ctx.createOscillator(); n.pulseLfo.frequency.value = 0.83; n.pulseDepth = ctx.createGain(); n.pulseDepth.gain.value = 0; n.pulseLfo.connect(n.pulseDepth); n.pulseDepth.connect(n.pulseG.gain); n.pulseLfo.start();
    nodes = n; return true;
  }
  function teardown() {
    if (!nodes) return;
    try { nodes.windSrc.stop(); nodes.drone.stop(); nodes.pulse.stop(); nodes.pulseLfo.stop(); nodes.out.disconnect(); } catch (e) { /* already stopped */ }
    nodes = null; on = false; key = '';
  }
  function fade(to, secs) { var t = nodes.ctx.currentTime, p = nodes.out.gain; p.cancelScheduledValues(t); p.setValueAtTime(p.value, t); p.linearRampToValueAtTime(to, t + secs); }
  function voice(e) {
    var ctx = nodes.ctx, t = ctx.currentTime + 0.02, o = ctx.createOscillator(), o2 = null, gn = ctx.createGain(), pad = e.kind === 'pad', atk = e.atk, rel = e.rel;
    o.type = pad ? 'sine' : 'triangle'; o.frequency.value = e.freq;
    gn.gain.setValueAtTime(0.0001, t); gn.gain.linearRampToValueAtTime(e.amp, t + atk); gn.gain.setValueAtTime(e.amp, t + e.dur - rel); gn.gain.linearRampToValueAtTime(0.0001, t + e.dur);
    o.connect(gn);
    if (pad) { o2 = ctx.createOscillator(); o2.type = 'triangle'; o2.frequency.value = e.freq * 1.004; o2.connect(gn); o2.start(t); o2.stop(t + e.dur + 0.1); }
    gn.connect(nodes.lp); o.start(t); o.stop(t + e.dur + 0.1);
    o.onended = function () { try { gn.disconnect(); } catch (x) { /* ignore */ } };
  }
  return {
    update: function (dt, st) {
      var mute = !!st.mute;
      if (mute) { if (on) { fade(0, 0.4); on = false; fadeT = 0.5; } if (fadeT > 0) { fadeT -= dt; if (fadeT <= 0) teardown(); } return; }
      if (!nodes && !build()) return;
      fadeT = 0;
      var sd = seedOf(st);
      if (!sch || seed !== sd) { seed = sd; sch = createScheduler(sd); }
      var k = keyOf(st);
      if (!on) { on = true; key = k; fade(1, 2); }
      else if (k !== key) {                                          // 2 s dip on every state change (down 1 s, up 1 s)
        key = k; var t = nodes.ctx.currentTime, p = nodes.out.gain;
        p.cancelScheduledValues(t); p.setValueAtTime(p.value, t); p.linearRampToValueAtTime(0.15, t + 1); p.linearRampToValueAtTime(1, t + 2);
      }
      windT -= dt;
      if (windT <= 0) {
        windT = 0.25;
        var w = clamp(st.wind || 0, 0, 1), t2 = nodes.ctx.currentTime, atmo = st.band === 'surface' || st.band === 'atmo' ? 1 : 0.15;
        nodes.windG.gain.setTargetAtTime(0.02 + 0.22 * w * atmo, t2, 0.4);
        nodes.windBp.frequency.setTargetAtTime(250 + 900 * w, t2, 0.4);
        nodes.lp.frequency.setTargetAtTime(lpFreq(st), t2, 1);
      }
      var ev = sch.tick(dt, st), i, e;
      for (i = 0; i < ev.length; i++) {
        e = ev[i];
        if (e.kind === 'drone') nodes.drone.frequency.setTargetAtTime(e.freq, nodes.ctx.currentTime, 1.5);
        else if (e.kind === 'pulse') {
          var tp = nodes.ctx.currentTime;
          nodes.pulse.frequency.setTargetAtTime(e.freq, tp, 0.5); nodes.pulseLfo.frequency.setTargetAtTime(1 / e.dur, tp, 0.5);
          nodes.pulseG.gain.setTargetAtTime(e.amp * 0.5, tp, 2); nodes.pulseDepth.gain.setTargetAtTime(e.amp * 0.5, tp, 2);   // 2 s swell in / out, no steps
        }
        else voice(e);
      }
    },
    stop: function () { if (nodes) { var n = nodes; fade(0, 0.4); on = false; sch = null; seed = -1; setTimeout(function () { if (nodes === n && !on) teardown(); }, 500); } },
  };
}
