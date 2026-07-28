# EMGOR Universe — Design Spec
**Date:** 2026-07-27 · **Status:** Approved by Emory (design sections), pending spec review

## Vision

Rebuild emgor's site as a **fractal 2D galaxy**: planets are navigation, each planet is a category or project, zooming into a planet reveals its children as a new orbital system. The galaxy is generated from an open-source documentation tree that mirrors the significant work in `_EMGOR_SYNTH`. Everything published is downloadable — the site is an open-hosted archive of all EMGOR work. Adding new work later requires only a markdown node + running the sync script; Netlify deploys on push.

**Hard guarantee:** nothing in `_EMGOR_SYNTH` is ever deleted, moved, or renamed by this system. All sync operations are additive-only. Before any work begins, all un-versioned work is locked into git.

## 1. Rendering engine (the "dope as fuck" rebuild)

Replace the three.js 3D solar system (`js/space.js`, 915 lines) with a **custom Canvas-2D galaxy engine** — zero dependencies.

- **Procedural planet identity:** each node's stable `id` seeds a deterministic generator → color palette, glow hue, ring presence/angle, surface noise pattern, pulse rate. A project's planet looks the same forever, at every depth.
- **Rendering:** layered radial gradients + `globalCompositeOperation: 'lighter'` additive bloom. Starfield, drifting particles, black-hole center, and the big-bang first-visit intro are retained, redrawn in the 2D style.
- **DOM-link labels (kept from old engine):** every planet's clickable surface is a real `<a>` element positioned over the canvas each frame — tappable on phones, keyboard-accessible, crawlable. This is the cross-platform guarantee.
- **Fractal zoom camera:** one continuous 2D camera. Click/tap planet → fly in, children resolve out of the atmosphere as a new orbit system. Back/zoom-out collapses it to a dot in the parent sky. Same mechanic at every depth.
- **Deep-linkable:** URL hash mirrors hierarchy (`#/m4l/emory-devices/the-purp`). Direct load of a hash jumps the camera to that depth.
- Rejected alternatives: SVG (particle-heavy scenes choke), PixiJS (unneeded dependency; canvas additive compositing achieves the glow).

## 2. Content architecture — hierarchy as filepath, IDs for directness

New `universe/` tree in the site repo (`EMG0R/EMGOR`). **One markdown file per node; the filepath IS the hierarchy.**

```
universe/
  m4l/
    _index.md            # the M4L planet itself
    emory-devices/
      _index.md
      the-purp.md        # a leaf planet: one device
      files/THE_PURP.amxd
```

Frontmatter per node:

```yaml
id: emgor.m4l.emory-devices.the-purp   # stable forever, survives moves
title: THE_PURP
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/__THE_PURP.amxd
downloads: [files/THE_PURP.amxd]
tags: [max-for-live, synth]
updated: 2025-03-17
```

- **IDs guarantee directness:** links, planet visuals, and cross-references key on `id`, not path. If a node file moves, nothing breaks.
- A build step compiles the tree into one `galaxy.json` the engine renders from.

### Top-level planets (from the folder scan)

| Planet | Sources in _EMGOR_SYNTH |
|---|---|
| **M4L** | `_M4L` (EMORY DEVICES, machineLab robots), `maxSYNTH`, `geroge`, `____INFINITY_GAUNTLET` |
| **NEPTR** | `_____pi` → `______PHASE2` → `______PHASE3` → `______2026NEW/NEPTR phase4`, `belaSAD`, `SPEAK` |
| **DEMIURGE** | `______2026NEW/DEMIURGE_OS`, `LILITH_PI`, `__csound_PI` |
| **HARDWARE** | `the_watch`, `Pocket-OpGorator`, `4-i-Gor`, `We-Remote`, `neuralGrid`, `______LITE`, `____oFOOTS`, `hyperGuitar`, `hyperTrumpet`, `3dPRINT` |
| **CODE+DSP** | `csound` suite, `______PHASE3/cheese` (ChucK FX lib), `__SUPAH`, `__juco`, `universal_usb_protocol`, `VCV.` |
| **PAPERS** | `______2026NEW/__PAPERS` (NAM-Csound paper + 7 planned), existing `papers.html`, `neuralgrid.html` content |
| **MUSIC** | existing portfolio (`music.html` media), `it-is.html`, `_samps` highlights |
| **LIVE CODE** | Strudel REPL planet (§4) |
| *(reserved)* | **Emory's "AI thing"** — slot reserved; added as its own planet or subcategory when identified |

## 3. Sync pipeline (add stuff → it's live)

`sync` script (Node, lives in site repo `tools/`):

1. Scans `_EMGOR_SYNTH` read-only. Matches projects to universe nodes via `source:` frontmatter; flags significant-looking unmatched projects as *drafts* (generates a proposed `planet.md` for Emory to bless — never auto-publishes).
2. Copies declared artifacts (`.amxd`, `.csd`, `.ck`, `.ino`, code, PDFs, existing markdown docs) into `universe/**/files/`. **Additive-only: the script has no delete/move codepath.**
3. Excludes big media (videos, sample packs, disk images, tarballs — e.g. the 520 MB neptr backup); those get external links instead.
4. Rebuilds `galaxy.json`, commits, pushes → Netlify auto-deploys.

**Git lock-in (Phase 1, before anything else):** every project folder in `_EMGOR_SYNTH` not already under git gets committed (repo at `_EMGOR_SYNTH` root with excludes for disk images/tarballs, or per-folder — decided at implementation with a dry-run shown first). Existing repos (DEMIURGE_OS, NEPTR phase4, SPEAK, hyperGuitar, etc.) are left as-is.

## 4. Strudel — LIVE CODE planet

- **`@strudel/repl` web component** (official, AGPL-3.0), pinned version from jsDelivr — full REPL in-page, no iframe, styled into the galaxy frame, preloaded with EMGOR patterns.
- Additional embeds inside doc pages (runnable examples; DEMIURGE_OS already has a `strudel/` dir — converge there).
- License: fine because the site is open source; site carries an AGPL-compatible license for code, music/content remains Emory's. Full monorepo fork rejected (heavy maintenance for ~5% gain); revisit only if deep REPL customization is needed.

## 5. Open hosting

- Every planet page lists its `files/` with download buttons + source-folder provenance + license.
- The public repo is the "open-hosted GitHub": everything clonable in one place.
- Hosting: **Netlify** (already live). Verify the git-integration auto-deploy, fix the broken `../resources/` media paths in `music.html`, restore/confirm the survey function path.
- Repo hygiene: the stale 2024 outer repo `EMG0R/____EMGOR_ONLINE` gets a README pointing to `EMG0R/EMGOR` (archived, not deleted).

## 6. Error handling & testing

- Engine: graceful fallback if canvas/JS fails — the DOM `<a>` labels still render as a plain list (navigation never breaks).
- Sync: dry-run mode prints every planned copy/commit before doing it; refuses to run if it would touch anything outside the site repo.
- galaxy.json build validates frontmatter (unique IDs, existing parents, existing download files) and fails loudly.
- Manual test matrix: phone (touch), desktop (mouse/keyboard), deep-link hash loads, offline-ish (service worker already present).

## Build order

1. **Phase 1 — Lock-in + architecture:** git lock-in of un-versioned work, `universe/` tree with initial nodes for all significant projects, sync script, galaxy.json build.
2. **Phase 2 — Galaxy engine:** the 2D visual rebuild.
3. **Phase 3 — Strudel planet + doc polish** (drafted docs for the undocumented: EMORY DEVICES, csound suite, cheese, etc.).
4. **Phase 4 — Deploy wiring:** Netlify verification, path fixes, old-repo cleanup.
