# Patch folders

One patch = one folder. The folder is a **superset of what already exists**, not a new system.

## Layout

```
~/demiurge/patches/<name>/
  <name>.conf        engine config (exactly today's patch/set .conf, incl. `# set: <name>`, `# power:`)
  <name>.map.json    pool mappings (docs/parameters.md s5, unchanged schema)
  index.html         the patch's phone UI (served at /patch/<name>/)
  state/             saved UI/param snapshots (optional, written by the pool/web layer)
```

Flat layout stays valid: `~/demiurge/patches/<name>.conf` (+ optional `<name>.map.json`) with no UI.
A patch is "a folder" if `patches/<name>/<name>.conf` exists, else "flat" if `patches/<name>.conf` exists.
**Folder wins** if both exist (and `demiurge-patch-new` refuses to start a folder from scratch over a flat patch; use the migration steps).

## One mechanism, not a second

| Job | Existing piece (stays the owner) | What folders change |
|---|---|---|
| Which patch is active | `# set: <name>` header in `~/demiurge/live.conf`, written by the conf copy | nothing |
| Activate a patch | `demiurge-patch` (copies conf -> live.conf, power, restart) / `demiurge-set` | only the **path lookup** (folder first, flat fallback) |
| Engine config | the patch `.conf` | nothing; it just lives in the folder |
| Param mappings | pool `<name>.map.json` (parameters.md s5), follows the `# set:` header by mtime | nothing; the file sits in the folder; pool lookup = `<name>/<name>.map.json`, then flat |
| Phone UI | `demiurge-web` static serving (`_static`, path-contained) | adds one read-only route for `patches/<active>/index.html`; no new server |

`demiurge-patch-new` only **creates files**. It never activates, never restarts, never writes `live.conf`.
It is the folder-layout counterpart of "edit live.conf then `cp` to `sets/myname.conf`".

### Pre-existing split to resolve (found while reading)
The repo already has two copy-to-live.conf mechanisms with different roots:
`demiurge-patch` (`~/demiurge/patches/`, NEPTR pygame PRESETS, `neptr/scripts/`, not in `src/wrappers/`)
and `demiurge-set` (`~/demiurge/sets/`, used by `demiurge_web.py` `actions.patch`, the TUI, `list_sets`).
On this Pi only `sets/` exists (`car_mode`, `neptr_bass`); `patches/` does not. Pool docs say `patches/`.
Folders should not deepen this. Recommendation: **`patches/` is the root; `sets/` becomes a compatibility alias**
(`ln -s patches sets` after migration, or have `demiurge-set` resolve `patches/` first and fall back to `sets/`).
`DEMIURGE_PATCH_DIR`/`PATCH_ROOT` lets `demiurge-patch-new` follow whichever root is decided without code change.

## Required follow-up edits (NOT done here; owned elsewhere)
1. `demiurge-patch` / `demiurge-set`: resolve `PATCH=$ROOT/$NAME/$NAME.conf`, else `$ROOT/$NAME.conf` (2 lines).
2. `demiurge_web.py` `list_sets`/`parse_sets`: also list `*/<dir>.conf`; `parse_sets` already derives name from the `# set:` header.
3. `demiurge_web.py` `do_GET`: `/patch/<name>/<file>` -> `ROOT/<name>/<file>` through the same containment check as `_static`; name must match `^[A-Za-z0-9][A-Za-z0-9_-]*$` and the folder must exist; allow only `index.html`, `*.css`, `*.js`, `*.json` (not `.conf`).
4. Pool: map file lookup folder-first.

## Auto-index rule
- `demiurge-patch-new <name>` writes `index.html` **only if absent**; never regenerated or overwritten.
- Title and `<h1>` = folder name with `_`/`-` -> spaces (`neptr_bass` -> "neptr bass"); `<body data-patch="<name>">` carries the exact name. The folder name is the UI name; renaming the folder renames the UI (rename `<name>.conf`/`.map.json` with it).
- The blank page is dark-purple, no external assets, mobile viewport; it talks to the pool only via `/api/pool/*`.
- Names are restricted to `[A-Za-z0-9][A-Za-z0-9_-]{0,63}` so they are safe in a URL, argv, HTML and the `# set:` header.
- Lazy fallback (optional, in the web layer): if `/patch/<name>/` has a conf but no `index.html`, serve a generated blank page rather than 404, without writing it.

## `demiurge-patch-new <name>`
Creates the folder, `<name>.conf` (seeded from live.conf, re-tagged `# set: <name>`; `SEED=none` for a header-only stub),
empty `<name>.map.json` (`{"version":1,"patch":"<name>","maps":[],"values":{}}`), `index.html`, `state/`.
Idempotent (fills only missing files), atomic writes, `DRY_RUN=1` prints only, exits 2 if a flat patch of that name exists.
Env: `PATCH_ROOT`, `LIVE`, `SEED`, `DRY_RUN`.

## Migration: flat -> folder (additive, copy-only)
```
cd ~/demiurge/patches            # (or sets/)
mkdir <name> && cp <name>.conf <name>/<name>.conf
[ -f <name>.map.json ] && cp <name>.map.json <name>/
DRY_RUN=0 PATCH_ROOT=$PWD demiurge-patch-new <name>   # fills index.html, map.json, state/ only; keeps the copied conf
```
Leave the flat `.conf` in place until the activation scripts resolve folders and a patch has switched cleanly; then move the flat copy to `_attic/` (never delete). `demiurge-patch-new` refuses only when a flat patch exists and no folder does; once the folder exists it just fills what is missing (that is the adopt path).

## Bench verification needed
- Folder-aware `demiurge-patch`/`demiurge-set` switch actually loads the folder conf and the `# set:` header matches (needs permission: it restarts audio).
- Pool picks up `<name>/<name>.map.json` on switch (mtime reload) without restart.
- `/patch/<name>/` serves on the phone; path-traversal tests (`..`, encoded) return 404.
- `demiurge-patch-new` against a real `~/demiurge/patches` (only scratch-tested with `PATCH_ROOT` override; not run on live state).
