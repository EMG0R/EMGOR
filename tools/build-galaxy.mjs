#!/usr/bin/env node
// build-galaxy.mjs — builds galaxy.json from universe/** per docs/universe-schema.md.
// Zero dependencies, Node >= 18.
//
// Usage:
//   node tools/build-galaxy.mjs              # build <repo>/galaxy.json
//   node tools/build-galaxy.mjs --check      # validate only, write nothing
//   node tools/build-galaxy.mjs --root DIR   # use DIR instead of the repo root
//
// Exit codes: 0 = ok, 1 = validation errors (printed loudly).

import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';
import { parseFrontmatter } from './lib/frontmatter.mjs';

const DEFAULT_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function parseArgs(argv) {
  const args = { root: DEFAULT_ROOT, check: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--check') args.check = true;
    else if (a === '--root') {
      const v = argv[++i];
      if (!v) fail(['--root requires a directory argument']);
      args.root = path.resolve(v);
    } else fail([`Unknown argument: ${a}`]);
  }
  return args;
}

function fail(errors) {
  console.error('\nBUILD FAILED — galaxy.json NOT written.');
  for (const e of errors) console.error('  ERROR: ' + e);
  process.exit(1);
}

const REQUIRED = ['id', 'title', 'blurb']; // parent required except on top-level planets
const RECOMMENDED = ['updated', 'tags', 'source'];
const SKIP_DIRS = new Set(['files', '_drafts', 'node_modules']);

/** Recursively collect node descriptors from the universe tree. */
async function collectNodes(universeDir) {
  const nodes = []; // { file (abs), relDir, isIndex }
  async function walk(dirAbs) {
    let entries;
    try {
      entries = await fs.readdir(dirAbs, { withFileTypes: true });
    } catch {
      return;
    }
    entries.sort((a, b) => a.name.localeCompare(b.name));
    for (const ent of entries) {
      if (ent.name.startsWith('.')) continue;
      const abs = path.join(dirAbs, ent.name);
      if (ent.isDirectory()) {
        if (SKIP_DIRS.has(ent.name) || ent.name.startsWith('_')) continue;
        await walk(abs);
      } else if (ent.isFile() && ent.name.endsWith('.md')) {
        nodes.push({ file: abs, isIndex: ent.name === '_index.md' });
      }
    }
  }
  await walk(universeDir);
  return nodes;
}

/** Derive the route from a file path relative to universe/. */
function routeFor(relToUniverse, isIndex) {
  let p = relToUniverse.split(path.sep).join('/');
  if (isIndex) p = path.posix.dirname(p) === '.' ? '' : path.posix.dirname(p);
  else p = p.replace(/\.md$/, '');
  return '/' + p.replace(/^\.$/, '').replace(/^\/+/, '');
}

async function main() {
  const { root, check } = parseArgs(process.argv.slice(2));
  const universeDir = path.join(root, 'universe');
  const errors = [];
  const warnings = [];

  try {
    const st = await fs.stat(universeDir);
    if (!st.isDirectory()) throw new Error('not a directory');
  } catch {
    fail([`universe/ not found at ${universeDir}`]);
  }

  const files = await collectNodes(universeDir);
  if (files.length === 0) fail([`no .md files found under ${universeDir}`]);

  const nodes = [];
  const byId = new Map();
  let hasRootIndex = false;

  for (const f of files) {
    const rel = path.relative(universeDir, f.file);
    const repoRel = path.posix.join('universe', rel.split(path.sep).join('/'));
    let raw;
    try {
      raw = await fs.readFile(f.file, 'utf8');
    } catch (e) {
      errors.push(`${repoRel}: cannot read file (${e.message})`);
      continue;
    }
    let parsed;
    try {
      parsed = parseFrontmatter(raw);
    } catch (e) {
      errors.push(`${repoRel}: frontmatter parse error: ${e.message}`);
      continue;
    }
    const fm = parsed.data;
    if (!fm) {
      errors.push(`${repoRel}: missing YAML frontmatter block`);
      continue;
    }
    if (fm.draft === true) continue; // excluded from galaxy.json

    const route = routeFor(rel, f.isIndex);
    const depth = route === '/' ? 0 : route.split('/').length - 1;
    const isRootIndex = f.isIndex && depth === 0;
    if (isRootIndex) hasRootIndex = true;

    // Required fields
    for (const k of REQUIRED) {
      if (fm[k] === undefined || fm[k] === null || fm[k] === '') {
        errors.push(`${repoRel}: missing required frontmatter field "${k}"`);
      }
    }
    let parent = fm.parent;
    if (parent === undefined || parent === null || parent === '') {
      if (isRootIndex) parent = null; // galactic center has no parent
      else if (depth === 1) parent = 'emgor'; // top-level planets may omit it
      else errors.push(`${repoRel}: missing required frontmatter field "parent"`);
    }

    // Recommended fields (warn only)
    for (const k of RECOMMENDED) {
      if (fm[k] === undefined) warnings.push(`${repoRel}: missing recommended field "${k}"`);
    }

    let updated = fm.updated;
    if (updated === undefined) {
      const st = await fs.stat(f.file);
      updated = st.mtime.toISOString().slice(0, 10);
    } else if (typeof updated !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(updated)) {
      errors.push(`${repoRel}: "updated" must be YYYY-MM-DD (got ${JSON.stringify(updated)})`);
      updated = String(updated);
    }

    // Downloads: paths relative to the node's directory (or already universe/-rooted).
    const nodeDirAbs = path.dirname(f.file);
    const downloads = [];
    const rawDownloads = fm.downloads ?? [];
    if (!Array.isArray(rawDownloads)) {
      errors.push(`${repoRel}: "downloads" must be a list`);
    } else {
      for (const d of rawDownloads) {
        if (typeof d !== 'string') {
          errors.push(`${repoRel}: downloads entries must be path strings (got ${JSON.stringify(d)})`);
          continue;
        }
        const abs = d.startsWith('universe/') ? path.join(root, d) : path.join(nodeDirAbs, d);
        const url = path.posix.normalize(
          path.relative(root, abs).split(path.sep).join('/')
        );
        try {
          const st = await fs.stat(abs);
          if (!st.isFile()) throw new Error('not a file');
          if (st.size > 25 * 1024 * 1024) {
            warnings.push(`${repoRel}: download "${d}" is >25MB (contract: reference via source/links instead)`);
          }
        } catch {
          errors.push(`${repoRel}: download points at nonexistent file: ${d} (resolved: ${url})`);
        }
        downloads.push({ name: path.posix.basename(url), url });
      }
    }

    const links = [];
    const rawLinks = fm.links ?? [];
    if (!Array.isArray(rawLinks)) {
      errors.push(`${repoRel}: "links" must be a list`);
    } else {
      for (const l of rawLinks) {
        if (l && typeof l === 'object' && typeof l.label === 'string' && typeof l.url === 'string') {
          links.push({ label: l.label, url: l.url });
        } else {
          errors.push(`${repoRel}: links entries must be { label, url } objects (got ${JSON.stringify(l)})`);
        }
      }
    }

    const tags = Array.isArray(fm.tags) ? fm.tags.map(String) : fm.tags === undefined ? [] : null;
    if (tags === null) errors.push(`${repoRel}: "tags" must be a list`);

    // Optional visual size multiplier (number, default 1). Non-numeric/absent => omitted.
    let size;
    if (fm.size !== undefined) {
      if (typeof fm.size === 'number' && Number.isFinite(fm.size) && fm.size > 0) {
        size = fm.size;
      } else {
        warnings.push(`${repoRel}: "size" must be a positive number (got ${JSON.stringify(fm.size)}) — ignored`);
      }
    }

    const node = {
      id: typeof fm.id === 'string' ? fm.id : String(fm.id ?? ''),
      title: fm.title !== undefined ? String(fm.title) : '',
      blurb: fm.blurb !== undefined ? String(fm.blurb) : '',
      parent: parent ?? null,
      path: repoRel,
      route,
      depth,
      children: 0,
      downloads,
      links,
      tags: tags ?? [],
      updated,
      ...(size !== undefined ? { size } : {}),
    };

    if (node.id) {
      if (byId.has(node.id)) {
        errors.push(`duplicate id "${node.id}" in ${repoRel} (already used by ${byId.get(node.id).path})`);
      } else {
        byId.set(node.id, node);
      }
    }
    nodes.push(node);
  }

  // Synthesize the galactic center if universe/_index.md doesn't provide it.
  if (!byId.has('emgor')) {
    if (hasRootIndex) {
      errors.push(`universe/_index.md exists but its id is not "emgor" — the root node must have id "emgor"`);
    } else {
      const rootNode = {
        id: 'emgor',
        title: 'EMGOR',
        blurb: 'Galactic center',
        parent: null,
        path: 'universe/_index.md',
        route: '/',
        depth: 0,
        children: 0,
        downloads: [],
        links: [],
        tags: [],
        updated: new Date().toISOString().slice(0, 10),
      };
      byId.set('emgor', rootNode);
      nodes.unshift(rootNode);
      warnings.push('universe/_index.md not found — synthesized the "emgor" galactic-center node');
    }
  }

  // Parent existence + children counts.
  for (const n of nodes) {
    if (n.id === 'emgor') continue;
    if (!n.parent) continue; // already errored above
    const p = byId.get(n.parent);
    if (!p) {
      errors.push(`${n.path}: parent "${n.parent}" does not exist (or is a draft)`);
    } else {
      p.children++;
    }
  }

  // Reachability from "emgor" via parent links.
  const reachable = new Set(['emgor']);
  let grew = true;
  while (grew) {
    grew = false;
    for (const n of nodes) {
      if (n.id && !reachable.has(n.id) && n.parent && reachable.has(n.parent)) {
        reachable.add(n.id);
        grew = true;
      }
    }
  }
  for (const n of nodes) {
    if (n.id && !reachable.has(n.id)) {
      errors.push(`${n.path}: node "${n.id}" is unreachable from root "emgor"`);
    }
  }

  // Sanity: parent should match the filepath-derived hierarchy (warn only).
  for (const n of nodes) {
    if (!n.parent || n.parent === 'emgor' || !byId.has(n.parent)) continue;
    const p = byId.get(n.parent);
    const expectedParentRoute = n.route.split('/').slice(0, -1).join('/') || '/';
    if (p.route !== expectedParentRoute) {
      warnings.push(`${n.path}: parent "${n.parent}" (route ${p.route}) does not match filepath parent (${expectedParentRoute})`);
    }
  }

  for (const w of warnings) console.warn('  WARN: ' + w);
  if (errors.length) fail(errors);

  nodes.sort((a, b) => a.route.localeCompare(b.route));
  const galaxy = {
    generated: new Date().toISOString().slice(0, 10),
    nodes,
  };

  const outPath = path.join(root, 'galaxy.json');
  if (check) {
    console.log(`OK (check mode): ${nodes.length} nodes valid, ${warnings.length} warning(s). Nothing written.`);
  } else {
    await fs.writeFile(outPath, JSON.stringify(galaxy, null, 2) + '\n', 'utf8');
    console.log(`Wrote ${outPath} — ${nodes.length} nodes, ${warnings.length} warning(s).`);
  }
}

main().catch((e) => fail([e.stack || String(e)]));
