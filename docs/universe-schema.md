# EMGOR Universe — Data Contract

Single source of truth for the fractal planet system. All tooling and content must conform.

## Tree

The public universe lives at `universe/` in this repo (site root). **Hierarchy = filepath.**

```
universe/
  m4l/
    _index.md              # the planet itself (every dir has one)
    emory-devices/
      _index.md
      the-purp.md          # leaf node
      files/THE_PURP.amxd  # downloadable artifacts for this branch
```

Naming: lowercase-kebab-case dirs/files. `_index.md` = the node for its directory; any other `.md` = leaf child of that directory.

## Frontmatter (YAML, required fields marked *)

```yaml
id: emgor.m4l.emory-devices.the-purp   # * stable forever; dot-path, never reused
title: THE_PURP                        # * display name on the planet label
blurb: Granular purple synth beast     # * one line, shown in orbit hover/panel header
parent: emgor.m4l.emory-devices        # * (omit only on top-level planets, parent = emgor)
source: _M4L/EMORY DEVICES/__THE_PURP.amxd  # provenance path inside _EMGOR_SYNTH (read-only ref)
downloads:                             # files copied into ./files/, repo-relative
  - files/THE_PURP.amxd
links:                                 # external or in-site links
  - { label: "Live demo", url: "ciesen.html" }
tags: [max-for-live, synth]
size: 1.4                              # optional planet visual size multiplier (number, default 1)
updated: 2025-03-17                    # YYYY-MM-DD
draft: false                           # true = excluded from galaxy.json
```

`size` scales the planet's rendered radius relative to its siblings. Optional; when absent
or non-numeric the builder treats it as the default `1` and omits it from `galaxy.json`.

Body: normal markdown — the planet's documentation page.

## galaxy.json

Built by `tools/build-galaxy.mjs` (zero-dependency Node ≥18). Written to site root `galaxy.json`:

```json
{
  "generated": "2026-07-28",
  "nodes": [
    {
      "id": "emgor.m4l",
      "title": "M4L",
      "blurb": "...",
      "parent": "emgor",              // "emgor" = galactic center (black hole)
      "path": "universe/m4l/_index.md", // fetchable markdown
      "route": "/m4l",                 // URL hash route (#/m4l)
      "depth": 1,
      "children": 4,
      "downloads": [{"name":"THE_PURP.amxd","url":"universe/m4l/emory-devices/files/THE_PURP.amxd"}],
      "links": [{"label":"...","url":"..."}],
      "tags": [], "updated": "2026-07-28",
      "size": 1.4                      // only present when frontmatter sets a numeric size ≠ default
    }
  ]
}
```

Validation (build fails loudly): unique ids, every `parent` exists, every `downloads` file exists on disk, every node reachable from `emgor`.

## Rendering contract

- `index.html` + `js/galaxy2d.js` render the galaxy from `galaxy.json`.
- Hash routing mirrors hierarchy: `#/m4l/emory-devices/the-purp`. Loading a hash deep-links the camera.
- Planet visuals are **seeded deterministically from `id`** (hash the id string → palette, ring, pulse). Same id = same planet forever.
- Clicking a node with children = fractal zoom in. Clicking a leaf = content overlay panel that fetches `path` and renders the markdown in-page (galaxy stays alive behind it). Downloads render as buttons in the overlay.
- Every planet is a real DOM `<a>` positioned over the canvas (touch/keyboard/crawler safe).

## Hard rules

1. `_EMGOR_SYNTH` working folders are **READ-ONLY** to all tooling. Never move, rename, or delete anything there.
2. Sync/copy operations are **additive-only** — no delete codepaths.
3. Artifacts >25MB are not copied into the repo; reference via `source` + external `links` instead.
4. Do not commit `.DS_Store` or binaries outside `files/` dirs.
