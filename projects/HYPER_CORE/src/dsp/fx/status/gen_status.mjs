// node status/gen_status.mjs  -> writes ../../../../HYPERTRUMPET_V2/docs/FX_STATUS.md
import { execSync } from 'node:child_process'; import fs from 'node:fs'; import path from 'node:path';
const here = path.dirname(new URL(import.meta.url).pathname); const fxdir = path.join(here, '..');
const out = path.join(fxdir, '../../../../HYPERTRUMPET_V2/docs/FX_STATUS.md');
const K = Number(process.env.SEED_SCALE || 10);   // Seed M7@480MHz is assumed ~K x slower than this host per sample. UNVERIFIED.
const sh = (c) => { try { return execSync(c, { cwd: fxdir, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }); } catch (e) { return (e.stdout || '') + (e.stderr || ''); } };
const testOut = sh('make -k test 2>&1');
const benchOut = sh('make bench 2>&1');
const pass = {}; for (const m of testOut.matchAll(/\[(\w+)\] (PASS|FAIL)/g)) pass[m[1]] = m[2];
const ns = {}; for (const m of benchOut.matchAll(/BENCH (\w+) ([\d.]+) ns\/sample/g)) ns[m[1]] = Number(m[2]);
const reg = fs.readFileSync(path.join(fxdir, 'fx_registry.cpp'), 'utf8');
const ids = [...reg.matchAll(/\{ "(\w+)",\s*"([^"]+)",\s*"([^"]*)"/g)].map(m => ({ id: m[1], name: m[2], v1: m[3] }));
const pct = (n) => n == null ? 'n/a' : (n * K * 48000 / 1e9 * 100).toFixed(1) + '%';
let md = `# FX status (generated ${new Date().toISOString().slice(0, 10)} by HYPER_CORE/src/dsp/fx/status/gen_status.mjs)\n\n`;
md += `Est. Seed CPU = host ns/sample (measured here, -O2, block 48, mono-in/stereo-in) x ${K} (assumed Seed M7 480 MHz vs this host) x 48000 / 1e9. **All CPU numbers are UNVERIFIED on the Seed**; the multiplier is a guess, the op counts in the notes are hand-counted. Budget: 4 slots, so sum of the worst 4 must stay well under 100% (target < 70%).\n\n`;
md += `| effect | file | test | host ns/smp (mono / stereo-in) | est. Seed CPU (mono / stereo-in) | mem | notes |\n|---|---|---|---|---|---|---|\n`;
for (const e of ids) {
  let j = {}; const jp = path.join(here, e.id + '.json'); if (fs.existsSync(jp)) { try { j = JSON.parse(fs.readFileSync(jp, 'utf8')); } catch { j = { notes: 'bad json' }; } }
  const file = j.file || `fx_${e.id}.h`;
  const t = pass[e.id] || (fs.existsSync(path.join(fxdir, 'tests', `test_fx_${e.id}.cpp`)) ? 'not run' : 'no test');
  const a = ns[e.id], b = ns[e.id + '_st'];
  const mem = j.memory_bytes != null ? (j.memory_bytes / 1048576).toFixed(2) + ' MB' : '?';
  md += `| ${e.name} (${e.id}) | ${file} | ${t} | ${a ? a.toFixed(0) : 'n/a'} / ${b ? b.toFixed(0) : 'n/a'} | ${pct(a)} / ${pct(b)} | ${mem} | ${(j.ops_per_sample ? 'ops: ' + j.ops_per_sample + '. ' : '') + (j.notes || '')} |\n`;
}
md += `\nv1 origins:\n` + ids.map(e => `- ${e.id}: ${e.v1}`).join('\n') + '\n';
const dec = path.join(here, "decisions.md"); if (fs.existsSync(dec)) md += fs.readFileSync(dec, "utf8");
fs.writeFileSync(out, md); console.log('wrote', out); console.log(md.split('\n').slice(0, 24).join('\n'));
