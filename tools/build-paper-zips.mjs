#!/usr/bin/env node
// build-paper-zips.mjs — one resource zip per paper planet -> papers/zips/<id>.zip
// Usage: node tools/build-paper-zips.mjs
// Zero deps; needs the system `zip` CLI. Papers with no resource files get no zip.
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const papersDir = path.join(root, 'universe', 'papers');
const outDir = path.join(root, 'papers', 'zips');

// id -> { dir: node dir under universe/papers, subtree: include every files/ below it,
//         extra: [[source path relative to repo root, path inside the zip folder]] }
const PAPERS = {
  'digital-luthier': { dir: '.', own: 'digital-luthier' },
  // current material only: device files + phase-4 (neptrPhase4.csd, live.conf); legacy lives in neptr-legacy-phases-1-3.zip
  'neptr-performance-system': { dir: 'neptr-performance-system', subtree: true },
  // NEPTR node "current" button: just the phase-4 spine + live.conf
  'neptr-current': { dir: 'neptr-performance-system', list: ['files/neptrPhase4.csd', 'files/live.conf'] },
  'neuralgrid': { dir: '.', own: 'neuralgrid' },
  'demiurgeos': { dir: 'demiurgeos', subtree: true },
  'thesis': { dir: '.', own: 'thesis' },
  // previously listed in downloads: universe/papers/files/** (PDF + opcode source)
  'nam-csound': { dir: '.', filesDir: 'files' },
  'bouba': { dir: '.', own: 'bouba' },
  'omniplex': { dir: '.', own: 'omniplex' },
  'open-pedal': { dir: '.', own: 'open-pedal' },
};

// one resource zip per device subplanet: every path in its md's `downloads:` list, resolved relative to the md
const DEVICES = ['4-i-gor', 'hyperguitar', 'hypertrumpet', 'neuralgrid', 'ofoots', 'pocket-opgorator', 'diy-hemi', 'we-remote'];
for (const d of DEVICES) {
  const md = await fs.readFile(path.join(papersDir, 'neptr-performance-system', d + '.md'), 'utf8');
  const fm = md.split(/^---$/m)[1] || '';
  const block = (fm.match(/^downloads:\n((?:  - .*\n?)+)/m) || [, ''])[1];
  PAPERS[d === 'neuralgrid' ? 'neuralgrid-device' : d] = { dir: 'neptr-performance-system', list: block.split('\n').map((l) => l.replace(/^  - /, '').trim()).filter(Boolean) };
}

async function walk(dir, out = []) {
  let ents;
  try { ents = await fs.readdir(dir, { withFileTypes: true }); } catch { return out; }
  for (const e of ents) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) await walk(p, out);
    else if (e.isFile() && !e.name.startsWith('.')) out.push(p);
  }
  return out;
}

// All files that live inside any directory named "files" under base.
async function filesUnder(base) {
  const all = await walk(base);
  return all.filter((f) => path.relative(base, f).split(path.sep).slice(0, -1).includes('files'));
}

await fs.mkdir(outDir, { recursive: true });
const tmp = await fs.mkdtemp(path.join(os.tmpdir(), 'paperzip-'));
const summary = [];

for (const [id, cfg] of Object.entries(PAPERS)) {
  const base = path.join(papersDir, cfg.dir);
  let files = [];
  if (cfg.subtree) files = (await filesUnder(base)).map((f) => [f, path.relative(base, f)]);
  else if (cfg.list) files = cfg.list.map((r) => [path.join(base, r), path.basename(r)]);
  else if (cfg.filesDir) files = (await walk(path.join(base, cfg.filesDir))).map((f) => [f, path.relative(base, f)]);
  else if (cfg.own) {
    const d = path.join(base, cfg.own, 'files');
    files = (await walk(d)).map((f) => [f, path.relative(path.join(base, cfg.own), f)]);
  }
  const zipPath = path.join(outDir, id + '.zip');
  await fs.rm(zipPath, { force: true });
  if (!files.length) { summary.push({ id, files: 0, size: 0 }); continue; }
  const stage = path.join(tmp, id);
  for (const [src, rel] of files) {
    const dst = path.join(stage, id, rel);
    await fs.mkdir(path.dirname(dst), { recursive: true });
    await fs.copyFile(src, dst);
  }
  execFileSync('zip', ['-X', '-D', '-r', '-q', zipPath, id], { cwd: stage });
  const st = await fs.stat(zipPath);
  summary.push({ id, files: files.length, size: st.size });
}

// Legacy NEPTR phases 1-3, archived at _attic/neptr-phases-1-3/ (top folder neptr-legacy/phase-N, each _index.md as README.md)
{
  const atticDir = path.join(root, '_attic', 'neptr-phases-1-3');
  const zipPath = path.join(outDir, 'neptr-legacy-phases-1-3.zip');
  const stage = path.join(tmp, 'legacy');
  const top = path.join(stage, 'neptr-legacy');
  let n = 0;
  const put = async (src, dst) => { await fs.mkdir(path.dirname(dst), { recursive: true }); await fs.copyFile(src, dst); n++; };
  for (const i of [1, 2, 3]) {
    const pd = path.join(atticDir, 'phase-' + i);
    await put(path.join(pd, '_index.md'), path.join(top, 'phase-' + i, 'README.md'));
    const cheese = i === 3 ? new Set(['chorus.ck', 'delay1.ck', 'phaser.ck', 'reverb1.ck', 'ringmod.ck', 'spectralWah.ck', 'pitchShifter.ck', 'vibrato.ck', 'ANALYSIS.md']) : new Set();
    for (const f of await walk(path.join(pd, 'files'))) {
      const b = path.basename(f);
      await put(f, path.join(top, 'phase-' + i, cheese.has(b) ? 'cheese' : '', b));
    }
    if (i === 3) await put(path.join(pd, 'cheese.md'), path.join(top, 'phase-3', 'cheese', 'README.md'));
  }
  await fs.rm(zipPath, { force: true });
  execFileSync('zip', ['-X', '-D', '-r', '-q', zipPath, 'neptr-legacy'], { cwd: stage });
  summary.push({ id: 'neptr-legacy-phases-1-3', files: n, size: (await fs.stat(zipPath)).size });
}
await fs.rm(tmp, { recursive: true, force: true });

for (const s of summary) {
  const mb = s.size / 1048576;
  console.log(
    s.files ? `${s.id.padEnd(26)} ${String(s.files).padStart(3)} files  ${mb.toFixed(2)} MB${mb > 50 ? '  ** >50MB **' : ''}`
            : `${s.id.padEnd(26)} no resource files — no zip`
  );
}
