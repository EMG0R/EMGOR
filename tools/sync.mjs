#!/usr/bin/env node
// sync.mjs — additive-only discovery helper.
//
// Scans _EMGOR_SYNTH (READ-ONLY: this script contains no delete/move/rename
// codepaths anywhere, by design) for significant project folders, compares
// them against the `source:` fields already declared in universe/**.md, and
// prints a report of work not yet represented on the site.
//
// Usage:
//   node tools/sync.mjs                 # report only
//   node tools/sync.mjs --draft        # also write draft stubs to universe/_drafts/
//   node tools/sync.mjs --root DIR     # repo root override (default: this repo)
//   node tools/sync.mjs --synth DIR    # scan root override (default: _EMGOR_SYNTH)
//
// Guarantees:
//   - Never writes, moves, renames or deletes anything inside the scan root.
//     The ONLY write operation in this file is creating NEW draft .md stubs
//     under <repo>/universe/_drafts/ (and that directory itself), and only
//     with --draft. Existing stubs are never overwritten.
//   - Never publishes: every stub is written with `draft: true`.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';
import { parseFrontmatter } from './lib/frontmatter.mjs';

const DEFAULT_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DEFAULT_SYNTH = '/Users/emgor/Documents/_EMGOR_SYNTH';

// Heuristic: a folder is a "significant project" if it directly (or in its
// immediate subfolders) contains at least this many interesting files.
const INTERESTING = new Set(['.amxd', '.csd', '.ck', '.ino', '.md']);
const THRESHOLDS = { '.amxd': 1, '.csd': 1, '.ck': 1, '.ino': 1, '.md': 3 };
const MAX_SCAN_DEPTH = 4;
const IGNORE_DIRS = new Set([
  'node_modules', '.git', '.svn', 'build', 'dist', '__pycache__',
  'Backup', 'backups', '_backup',
]);

function parseArgs(argv) {
  const args = { root: DEFAULT_ROOT, synth: DEFAULT_SYNTH, draft: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--draft') args.draft = true;
    else if (a === '--root') args.root = path.resolve(requireVal(argv, ++i, '--root'));
    else if (a === '--synth') args.synth = path.resolve(requireVal(argv, ++i, '--synth'));
    else { console.error(`Unknown argument: ${a}`); process.exit(1); }
  }
  return args;
}
function requireVal(argv, i, flag) {
  if (!argv[i]) { console.error(`${flag} requires a value`); process.exit(1); }
  return argv[i];
}

/** Collect every `source:` value declared anywhere under universe/ (drafts included). */
async function collectSources(universeDir) {
  const sources = new Set();
  async function walk(dir) {
    let entries;
    try { entries = await fs.readdir(dir, { withFileTypes: true }); } catch { return; }
    for (const ent of entries) {
      if (ent.name.startsWith('.')) continue;
      const abs = path.join(dir, ent.name);
      if (ent.isDirectory()) await walk(abs);
      else if (ent.isFile() && ent.name.endsWith('.md')) {
        try {
          const { data } = parseFrontmatter(await fs.readFile(abs, 'utf8'));
          if (data && typeof data.source === 'string' && data.source.trim()) {
            sources.add(normalizeSource(data.source));
          }
        } catch { /* unparseable file — ignore for sync purposes */ }
      }
    }
  }
  await walk(universeDir);
  return sources;
}

function normalizeSource(s) {
  return s.replace(/\\/g, '/').replace(/^\.?\//, '').replace(/\/+$/, '').toLowerCase();
}

/**
 * Scan the synth root (READ-ONLY: readdir/stat only) and score folders.
 * Returns [{ rel, counts, total }].
 */
async function scanSynth(synthRoot, repoRoot) {
  const results = [];
  const repoReal = path.resolve(repoRoot);

  async function countFiles(dir, depthLeft) {
    const counts = {};
    let entries;
    try { entries = await fs.readdir(dir, { withFileTypes: true }); } catch { return counts; }
    for (const ent of entries) {
      if (ent.name.startsWith('.')) continue;
      const abs = path.join(dir, ent.name);
      if (ent.isFile()) {
        const ext = path.extname(ent.name).toLowerCase();
        if (INTERESTING.has(ext)) counts[ext] = (counts[ext] || 0) + 1;
      } else if (ent.isDirectory() && depthLeft > 0 && !IGNORE_DIRS.has(ent.name)
                 && path.resolve(abs) !== repoReal) {
        const sub = await countFiles(abs, depthLeft - 1);
        for (const [k, v] of Object.entries(sub)) counts[k] = (counts[k] || 0) + v;
      }
    }
    return counts;
  }

  async function walk(dir, depth) {
    let entries;
    try { entries = await fs.readdir(dir, { withFileTypes: true }); } catch { return; }
    for (const ent of entries) {
      if (!ent.isDirectory() || ent.name.startsWith('.') || IGNORE_DIRS.has(ent.name)) continue;
      const abs = path.join(dir, ent.name);
      if (path.resolve(abs) === repoReal) continue; // the website repo itself is not "unrepresented work"
      const counts = await countFiles(abs, 1);
      const significant = Object.entries(counts).some(([ext, n]) => n >= (THRESHOLDS[ext] ?? Infinity));
      if (significant) {
        const total = Object.values(counts).reduce((a, b) => a + b, 0);
        results.push({ rel: path.relative(synthRoot, abs).split(path.sep).join('/'), counts, total });
      } else if (depth < MAX_SCAN_DEPTH) {
        await walk(abs, depth + 1); // keep looking deeper for nested projects
      }
    }
  }

  await walk(synthRoot, 0);
  return results;
}

function isRepresented(rel, sources) {
  const n = normalizeSource(rel);
  for (const s of sources) {
    if (s === n || s.startsWith(n + '/') || n.startsWith(s + '/')) return true;
    // a source may point at a file inside the folder
    if (s.startsWith(n + '/') || path.posix.dirname(s) === n) return true;
  }
  return false;
}

function slugify(name) {
  return name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'untitled';
}

async function writeDraftStub(repoRoot, synthRel, counts) {
  const draftsDir = path.join(repoRoot, 'universe', '_drafts');
  await fs.mkdir(draftsDir, { recursive: true });
  const base = slugify(path.posix.basename(synthRel));
  const file = path.join(draftsDir, `${base}.md`);
  try {
    await fs.access(file);
    return { file, skipped: true }; // additive-only: never overwrite
  } catch { /* does not exist — ok to create */ }
  const today = new Date().toISOString().slice(0, 10);
  const extsLine = Object.entries(counts).map(([e, n]) => `${n}x ${e}`).join(', ');
  const stub = `---
id: emgor.TODO.${base}
title: ${path.posix.basename(synthRel)}
blurb: TODO — one-line description
parent: emgor
source: ${synthRel}
tags: []
updated: ${today}
draft: true
---

# ${path.posix.basename(synthRel)}

Proposed planet stub generated by \`tools/sync.mjs\` on ${today}.
Found in \`_EMGOR_SYNTH/${synthRel}\` (${extsLine}).

TODO: write the story, fix the id/parent dot-path, move this file into its
place in \`universe/\`, copy shareable artifacts into a \`files/\` dir, then
set \`draft: false\`.
`;
  await fs.writeFile(file, stub, { flag: 'wx' }); // 'wx' = fail rather than overwrite
  return { file, skipped: false };
}

async function main() {
  const { root, synth, draft } = parseArgs(process.argv.slice(2));
  const universeDir = path.join(root, 'universe');

  console.log(`Scanning (read-only): ${synth}`);
  console.log(`Comparing against sources in: ${universeDir}\n`);

  const sources = await collectSources(universeDir);
  const projects = await scanSynth(synth, root);

  const missing = projects.filter((p) => !isRepresented(p.rel, sources));
  const represented = projects.length - missing.length;

  console.log(`Found ${projects.length} significant project folder(s); ${represented} already represented, ${missing.length} not yet on the site.\n`);
  if (missing.length === 0) {
    console.log('Nothing new — the universe is up to date.');
    return;
  }

  console.log('NOT YET REPRESENTED:');
  missing.sort((a, b) => b.total - a.total);
  for (const p of missing) {
    const detail = Object.entries(p.counts).map(([e, n]) => `${n} ${e}`).join(', ');
    console.log(`  - ${p.rel}  (${detail})`);
  }

  if (draft) {
    console.log('\nWriting draft stubs (draft: true) to universe/_drafts/ ...');
    for (const p of missing) {
      const { file, skipped } = await writeDraftStub(root, p.rel, p.counts);
      console.log(`  ${skipped ? 'exists, skipped' : 'created'}: ${file}`);
    }
    console.log('\nStubs are drafts only — they are excluded from galaxy.json until you edit and move them.');
  } else {
    console.log('\nRun with --draft to generate draft stubs in universe/_drafts/ (never published, never overwrites).');
  }
}

main().catch((e) => { console.error(e.stack || String(e)); process.exit(1); });
