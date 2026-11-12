// strudel_harness.mjs -- evaluate a .strudel file with NO audio and NO MIDI (stock-check / check.sh).
// Same evaluate path as the runner (core + mini + transpiler) with a no-op output; queries a cycle
// every 100 ms so signal() readers run. Needs node_modules (@strudel/*) next to this file
// (run.sh symlinks them in). usage: node strudel_harness.mjs file.strudel
import fs from 'node:fs';
import { performance } from 'node:perf_hooks';
const core = await import('@strudel/core');
const mini = await import('@strudel/mini');
const transpiler = await import('@strudel/transpiler');
await core.evalScope(core, mini);
mini.miniAllStrings();
const t0 = performance.now();
const { evaluate, scheduler } = core.repl({
  defaultOutput: () => {}, getTime: () => (performance.now() - t0) / 1000, transpiler: transpiler.transpiler });
await evaluate(fs.readFileSync(process.argv[2], 'utf8'));
let c = 0;
setInterval(() => { try { scheduler.pattern.queryArc(c, c + 1); c++; } catch (e) { console.error('query', e.message); } }, 100);
