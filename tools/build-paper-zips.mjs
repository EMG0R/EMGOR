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
  'neptr-performance-system': { dir: 'neptr-performance-system', subtree: true },
  'neuralgrid': { dir: '.', own: 'neuralgrid' },
  'demiurgeos': { dir: 'demiurgeos', subtree: true },
  'thesis': { dir: '.', own: 'thesis' },
  // previously listed in downloads: universe/papers/files/** (PDF + opcode source)
  'nam-csound': { dir: '.', filesDir: 'files' },
  'bouba': { dir: '.', own: 'bouba' },
  'omniplex': { dir: '.', own: 'omniplex' },
  'open-pedal': { dir: '.', own: 'open-pedal' },
};

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
await fs.rm(tmp, { recursive: true, force: true });

for (const s of summary) {
  const mb = s.size / 1048576;
  console.log(
    s.files ? `${s.id.padEnd(26)} ${String(s.files).padStart(3)} files  ${mb.toFixed(2)} MB${mb > 50 ? '  ** >50MB **' : ''}`
            : `${s.id.padEnd(26)} no resource files — no zip`
  );
}
