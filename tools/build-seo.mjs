#!/usr/bin/env node
// build-seo.mjs — generates robots.txt and sitemap.xml from galaxy.json.
// Zero dependencies, Node built-ins only, Node >= 18.
//
// Usage:
//   node tools/build-seo.mjs              # write robots.txt + sitemap.xml at repo root
//   node tools/build-seo.mjs --check      # validate only, write nothing
//   node tools/build-seo.mjs --root DIR   # use DIR instead of the repo root
//
// IMPORTANT LIMITATION (read this before trusting the sitemap to do much):
// EMGOR's 80 content nodes live behind `#/hash` client-side routes
// (e.g. https://emgor.online/#/code/bmo). Google, Bing, and every other
// crawler treat the URL fragment as a same-document anchor, not a distinct
// page — they do NOT execute the app's router or index each node
// separately. Listing `https://emgor.online/#/code/bmo` in sitemap.xml is
// harmless but it will not get that node indexed or ranked on its own;
// crawlers collapse it back to `https://emgor.online/`. This script lists
// the hash routes anyway (next-best-effort, costs nothing, and some
// crawlers/social unfurlers do at least resolve the base URL from them),
// but the sitemap's real, load-bearing entries are the standalone .html
// pages, which are genuinely separate, indexable documents.
//
// THE REAL FIX (not built here, per scope — this script only generates
// files, it does not change site architecture):
//   Real, crawlable indexing of the 80 nodes would need actual server-
//   rendered (or statically pre-rendered) HTML at real paths, e.g. a
//   prerendered `/code/bmo/index.html` per node that redirects into the
//   hash-routed app for humans (or a `<noscript>`-style static shell per
//   route), or a switch to History API routing with a prerender/SSG step.
//   Either is a real architecture change and out of scope for a
//   zero-build-step static site — flagging it, not building it.
//
// Second-best crawlable win: the JSON-LD in index.html's <head> (added
// separately) and the enriched <noscript> fallback (see report — not
// applied here, out of this script's file-ownership scope) are both real
// static HTML/data that crawlers do read regardless of hash routing.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

const DEFAULT_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

// Single clearly-marked source of truth for the production domain.
// Determined from: the task brief's own references to "emgor.online", the
// album metadata string 'EMGOR.ONLINE' embedded in ciesen.html, and the
// site being deployed on Netlify (no CNAME file — Netlify custom domains
// don't require one, unlike GitHub Pages). No _headers/netlify.toml entry
// states the domain explicitly, so this is high-confidence, not certain.
const SITE_ORIGIN = 'https://emgor.online';

// Pages deliberately excluded from search per existing site intent.
const NOINDEX_PAGES = ['papers.html', 'neuralgrid.html'];

// Standalone, genuinely-indexable HTML pages at the repo root (the site's
// real crawlable surface). Kept as an explicit allowlist rather than a
// directory scan so gorcave-*, universe/**, and other app-shell / PWA /
// non-content files never leak into the sitemap.
const STANDALONE_PAGES = [
  { file: 'index.html', priority: '1.0', changefreq: 'weekly' },
  { file: 'music.html', priority: '0.7', changefreq: 'monthly' },
  { file: 'research.html', priority: '0.6', changefreq: 'monthly' },
  { file: 'NEPTR.html', priority: '0.6', changefreq: 'monthly' },
  { file: 'ciesen.html', priority: '0.6', changefreq: 'monthly' },
  { file: 'BMO.html', priority: '0.5', changefreq: 'monthly' },
  { file: 'resources.html', priority: '0.5', changefreq: 'monthly' },
  { file: 'livecode.html', priority: '0.5', changefreq: 'monthly' },
  { file: 'it-is.html', priority: '0.4', changefreq: 'yearly' },
];

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
  console.error('\nbuild-seo FAILED — nothing written.');
  for (const e of errors) console.error('  ERROR: ' + e);
  process.exit(1);
}

function xmlEscape(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&apos;');
}

function buildSitemap(nodes) {
  const urls = [];

  for (const p of STANDALONE_PAGES) {
    const loc = p.file === 'index.html' ? `${SITE_ORIGIN}/` : `${SITE_ORIGIN}/${p.file}`;
    urls.push({ loc, changefreq: p.changefreq, priority: p.priority });
  }

  // Hash routes for every galaxy node — see the limitation notice at the
  // top of this file. Root ('/') is skipped, it's already index.html above.
  for (const n of nodes) {
    if (!n.route || n.route === '/') continue;
    urls.push({
      loc: `${SITE_ORIGIN}/#${n.route}`,
      lastmod: n.updated || undefined,
      changefreq: 'monthly',
      priority: '0.3',
    });
  }

  const body = urls
    .map((u) => {
      const parts = [`    <loc>${xmlEscape(u.loc)}</loc>`];
      if (u.lastmod) parts.push(`    <lastmod>${xmlEscape(u.lastmod)}</lastmod>`);
      if (u.changefreq) parts.push(`    <changefreq>${u.changefreq}</changefreq>`);
      if (u.priority) parts.push(`    <priority>${u.priority}</priority>`);
      return `  <url>\n${parts.join('\n')}\n  </url>`;
    })
    .join('\n');

  return `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n${body}\n</urlset>\n`;
}

function buildRobots() {
  const lines = [
    'User-agent: *',
    'Allow: /',
    '',
    '# Deliberately kept out of search — see index.html / site intent.',
  ];
  for (const f of NOINDEX_PAGES) lines.push(`Disallow: /${f}`);
  lines.push('', `Sitemap: ${SITE_ORIGIN}/sitemap.xml`, '');
  return lines.join('\n');
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const errors = [];

  let galaxy;
  try {
    const raw = await fs.readFile(path.join(args.root, 'galaxy.json'), 'utf8');
    galaxy = JSON.parse(raw);
  } catch (e) {
    fail([`could not read/parse galaxy.json: ${e.message}`]);
  }

  const nodes = Array.isArray(galaxy.nodes) ? galaxy.nodes : [];
  if (nodes.length === 0) errors.push('galaxy.json has no nodes[] — sitemap would be nearly empty.');

  for (const p of STANDALONE_PAGES) {
    try {
      await fs.access(path.join(args.root, p.file));
    } catch {
      errors.push(`STANDALONE_PAGES references missing file: ${p.file}`);
    }
  }

  if (errors.length) fail(errors);

  const sitemap = buildSitemap(nodes);
  const robots = buildRobots();

  if (args.check) {
    console.log('OK — sitemap.xml and robots.txt would be valid. Nothing written (--check).');
    return;
  }

  await fs.writeFile(path.join(args.root, 'sitemap.xml'), sitemap, 'utf8');
  await fs.writeFile(path.join(args.root, 'robots.txt'), robots, 'utf8');
  console.log(`Wrote sitemap.xml (${nodes.length} nodes + ${STANDALONE_PAGES.length} standalone pages) and robots.txt.`);
}

main();
