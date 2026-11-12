// ~/demiurge/strudel/strudel-runner.mjs
//
// DEMIURGE Strudel headless runner.
//
// Strudel has no official Node CLI. This script wires @strudel/core +
// @strudel/mini + @strudel/transpiler into a minimal repl.
//
// Two output modes, both live on the same DEMIURGE buses:
//
//   AUDIO (default) — node-web-audio-api drives an AudioContext backed
//                     by cpal -> PipeWire-JACK. Each hap becomes a
//                     saw+env voice on the graph; the cpal client shows
//                     up as `cpal_client_out:out_0/out_1` and the Rust
//                     launcher links it into whatever chain the stage
//                     lives in. This is what Strudel is for — it's a
//                     browser-audio descendant of Tidal, so we run
//                     Strudel's own synth engine.
//
//   MIDI (--midi)   — emit MIDI to "Midi Through" via easymidi so any
//                     other language on the bus can pick the notes up
//                     (ChucK, Csound, Pd, SC, Faust, C++). Useful when
//                     you want Strudel as a pure pattern driver.
//
// BOTH modes listen on Midi Through for clock sync: ch16 CC119 is the
// demiurge-clock BPM broadcast and gets translated to scheduler.setCps.
//
// Usage:
//   node strudel-runner.mjs <pattern-file>            # native audio
//   node strudel-runner.mjs --midi <pattern-file>     # MIDI out mode
//
// Required env on the Pi (so cpal + JACK clients resolve PipeWire-JACK
// instead of jackd2, which hangs):
//
//   LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack \
//     node strudel-runner.mjs pattern.strudel
//
// Install (on Pi, in ~/demiurge/strudel):
//   npm init -y
//   npm i @strudel/core@1.2.0 @strudel/mini@1.2.0 @strudel/transpiler@1.2.0 \
//         @strudel/tonal@1.2.0 easymidi node-web-audio-api
//
// Pin @strudel/* to 1.2.0 — 1.2.6 pulls @kabelsalat/web which breaks in Node.

import fs from 'node:fs';
import path from 'node:path';
import { performance } from 'node:perf_hooks';

import easymidi from 'easymidi';

// --- Arg parse ---
const argv = process.argv.slice(2);
let useMidiOut = false;
const args = [];
for (const a of argv) {
    if (a === '--midi') useMidiOut = true;
    else args.push(a);
}
const patternFile = args[0];
if (!patternFile) {
    console.error('[strudel-runner] usage: node strudel-runner.mjs [--midi] <pattern-file>');
    process.exit(1);
}

// --- Inbound MIDI (clock sync) — always on ---
const inputs = easymidi.getInputs();
const inPortName = inputs.find(o => /midi through/i.test(o));
let midiIn = null;
if (inPortName) {
    midiIn = new easymidi.Input(inPortName);
    console.error('[strudel-runner] midi-in:', inPortName);
} else {
    console.error('[strudel-runner] no Midi Through input — clock sync disabled');
}

// --- Optional MIDI out ---
let midiOut = null;
if (useMidiOut) {
    const outputs = easymidi.getOutputs();
    const outName = outputs.find(o => /midi through/i.test(o))
                 || outputs.find(o => /demiurge/i.test(o))
                 || outputs[0];
    if (!outName) {
        console.error('[strudel-runner] --midi requested but no MIDI output available');
        process.exit(1);
    }
    midiOut = new easymidi.Output(outName);
    console.error('[strudel-runner] midi-out:', outName);
}

// --- AudioContext (native audio path) ---
let audioCtx = null;
if (!useMidiOut) {
    const { AudioContext } = await import('node-web-audio-api');
    audioCtx = new AudioContext({ latencyHint: 'playback' });
    console.error(`[strudel-runner] audio-out: cpal -> JACK sr=${audioCtx.sampleRate} state=${audioCtx.state}`);
}

// --- Load Strudel ---
const core       = await import('@strudel/core');
const mini       = await import('@strudel/mini');
const transpiler = await import('@strudel/transpiler');
let tonal = null;
try { tonal = await import('@strudel/tonal'); } catch {}

const { evalScope, repl } = core;
const { miniAllStrings } = mini;

await evalScope(core, mini, ...(tonal ? [tonal] : []));
miniAllStrings();

// --- MIDI hap output ---
function midiHap(hap, deadline, duration) {
    const value = hap.value || {};
    const channel = (value.channel ?? 0) & 0x0f;

    if (value.ccn !== undefined && value.ccv !== undefined) {
        const num = Math.max(0, Math.min(127, value.ccn | 0));
        const val = Math.max(0, Math.min(127, Math.round(value.ccv * 127)));
        midiOut.send('cc', { controller: num, value: val, channel });
        return;
    }

    let note = value.note ?? value.n;
    if (note === undefined) return;
    if (typeof note === 'string') {
        try { note = core.noteToMidi(note); } catch { return; }
    }
    const midiNote = Math.max(0, Math.min(127, Math.round(note)));
    const vel = Math.max(1, Math.min(127, Math.round((value.velocity ?? value.gain ?? 0.75) * 127)));
    const durMs = Math.max(20, duration * 1000);

    midiOut.send('noteon',  { note: midiNote, velocity: vel, channel });
    setTimeout(() => {
        midiOut.send('noteoff', { note: midiNote, velocity: 0, channel });
    }, durMs);
}

// --- Native audio hap output ---
// Each hap schedules a saw voice with a short AR envelope on the
// AudioContext. This is a minimal superdough-ish voice — enough for
// melodic patterns to be audibly driven by Strudel alone. Richer
// sound design is on the user by swapping the callback body.
function audioHap(hap, deadline, duration) {
    const value = hap.value || {};
    let n = value.note ?? value.n;
    if (n === undefined) return;
    if (typeof n === 'string') {
        try { n = core.noteToMidi(n); } catch { return; }
    }
    const freq = 440 * Math.pow(2, (n - 69) / 12);
    const when = audioCtx.currentTime + Math.max(0, deadline);
    const dur  = Math.max(0.05, duration);
    const osc = audioCtx.createOscillator();
    const g   = audioCtx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.value = freq;
    const peak = 0.22 * (value.gain ?? 0.8);
    g.gain.setValueAtTime(0, when);
    g.gain.linearRampToValueAtTime(peak, when + 0.005);
    g.gain.linearRampToValueAtTime(0, when + dur);
    osc.connect(g).connect(audioCtx.destination);
    osc.start(when);
    osc.stop(when + dur + 0.02);
}

// --- Build the repl ---
const t0Perf = performance.now();
const getTime = audioCtx
    ? () => audioCtx.currentTime
    : () => (performance.now() - t0Perf) / 1000;

const { evaluate, scheduler } = repl({
    defaultOutput: useMidiOut ? midiHap : audioHap,
    getTime,
    transpiler: transpiler.transpiler,
});

// --- Read and evaluate the pattern file ---
const code = fs.readFileSync(path.resolve(patternFile), 'utf8');
console.error('[strudel-runner] evaluating', patternFile);

try {
    await evaluate(code);
    scheduler.start();
    console.error('[strudel-runner] scheduler started');
} catch (err) {
    console.error('[strudel-runner] evaluate error:', err);
    process.exit(1);
}

// --- Clock sync: ch16 CC119 BPM broadcast -> scheduler.setCps ---
// easymidi gives channel 0..15; ch16 == 15. Map raw 0..127 to 40..240 BPM.
// 1 cycle == 2 beats (half-note feel) so Strudel bars line up with the
// DEMIURGE musical grid.
if (midiIn) {
    midiIn.on('cc', (msg) => {
        if (msg.controller !== 119 || msg.channel !== 15) return;
        const bpm = 40 + (msg.value / 127) * 200;
        const cps = bpm / 60 / 2;
        try { scheduler.setCps(cps); } catch {}
    });
    midiIn.on('start',    () => { try { scheduler.start(); } catch {} });
    midiIn.on('stop',     () => { try { scheduler.stop();  } catch {} });
    midiIn.on('continue', () => { try { scheduler.start(); } catch {} });
}

// --- Shutdown ---
const shutdown = () => {
    try { scheduler.stop(); } catch {}
    try { if (midiOut) midiOut.close(); } catch {}
    try { if (midiIn)  midiIn.close();  } catch {}
    process.exit(0);
};
process.on('SIGINT',  shutdown);
process.on('SIGTERM', shutdown);
setInterval(() => {}, 1 << 30);
