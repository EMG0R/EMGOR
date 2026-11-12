# Patches consolidation: `patches/` canonical, `sets/` alias

One patch mechanism. STAGED, not deployed.

## Canonical layout
```
~/demiurge/patches/<name>/<name>.conf       folder patch (docs/patch-folders.md)
~/demiurge/patches/<name>/<name>.map.json   pool maps
~/demiurge/patches/<name>/index.html        phone UI -> /patch/<name>/
~/demiurge/patches/<name>.conf              flat patch (still valid)
~/demiurge/sets/<name>.conf                 legacy alias (car_mode, neptr_bass live here today)
```
Active patch = the `# set:` header in `~/demiurge/live.conf` (unchanged).

## Lookup order (everywhere)
1. `patches/<name>/<name>.conf`  2. `patches/<name>.conf`  3. `sets/<name>.conf`. First hit wins.
- `demiurge-patch` resolves in that order. Listing dedupes by name, same order, tagged [folder]/[flat]/[sets].
- Pool map file (`config.py patch_map_path`): `patches/<n>/<n>.map.json`, else `patches/<n>.map.json`; if neither exists (first save) the folder path is used when the folder exists, else flat. Same mtime poll, no daemon logic change.
- Web `Probes.list_sets()` merges the same three sources, same order, first name wins; `parse_sets`/`actions.patch` untouched.

## One mechanism
- `src/wrappers/demiurge-patch` is the only implementation (power apply via `demiurge-power.service` as demiurge-set did, CLI fallback as demiurge-patch did; then restart demiurge).
- `src/wrappers/demiurge-set` is now a 10-line shim that `exec`s `demiurge-patch` with the same args. Callers (`demiurge_web.py actions.patch`, TUI companion, NEPTR `NEPTR_PATCH_CMD`, tests expecting `["demiurge-set", name]`) are unchanged.
- Both NEPTR copies (`neptr/scripts/`, `~/NEPTR_phase4/scripts/`) are now byte-identical to `src/wrappers/demiurge-patch`. They can stay as copies (NEPTR_phase4 is a separate deploy tree) or be dropped in favour of the PATH install; `diff` them against the wrapper when changing it.
- `demiurge-patch-new` only creates files; unchanged.

## Web
`GET /patch/<name>/<file>` (`/patch/<name>/` = index.html): name `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`, root `patches/<name>/`, same resolve-and-contain check as `_static`, only `.html .css .js .json`, never `.conf` (404). Nothing else in the server changed.

## Not covered (follow-up, still working)
`src/companion/demiurge` (TUI list) and `neptr/actions.py` (`NEPTR_PATCHES_DIR`, default `~/demiurge/sets`) list only the flat `sets/` dir. They keep working because activation goes through `demiurge-set`; they just will not show folder-only patches until pointed at `patches/` (or the web list).

## Migration of existing `sets/` (additive, copy-only)
Nothing to do for it to keep working: step 3 of the lookup finds `car_mode.conf`, `neptr_bass.conf`. Optional, once verified:
```
mkdir -p ~/demiurge/patches
for n in car_mode neptr_bass; do
  mkdir -p ~/demiurge/patches/$n && cp -n ~/demiurge/sets/$n.conf ~/demiurge/patches/$n/$n.conf
  PATCH_ROOT=~/demiurge/patches demiurge-patch-new $n    # fills map.json/index.html/state only
done
```
Leave `sets/` in place (never delete; later move to `_attic/`). Do NOT `ln -s patches sets` while `sets/` holds files.

## Bench deploy
```
cd <repo>   # _______DEMIURGE, after copying staged files over it
bash -n src/wrappers/demiurge-patch src/wrappers/demiurge-set
sudo install -m0755 src/wrappers/demiurge-patch /usr/local/bin/demiurge-patch
sudo install -m0755 src/wrappers/demiurge-patch /opt/demiurge/bin/demiurge-patch
sudo install -m0755 src/wrappers/demiurge-set   /opt/demiurge/bin/demiurge-set    # setup-script/deploy-wrapper.sh does this safely
cp -n ~/NEPTR_phase4/scripts/demiurge-patch ~/NEPTR_phase4/scripts/demiurge-patch.bak-pre-consolidate
cp neptr/scripts/demiurge-patch ~/NEPTR_phase4/scripts/demiurge-patch
# web: copy web/demiurge_web.py; pool: copy src/demiurge-io/demiurge_io/config.py
```
Restarts (EMGOR's call): web server restart for the web change; demiurge-io restart for config.py. The shell wrappers take effect immediately with no restart. Back up the old `/opt/demiurge/bin/demiurge-set` first.

Bench checks:
1. `demiurge-patch` and `demiurge-set` (no args) list the same, show car_mode/neptr_bass [sets] and Active.
2. Switch to a sets patch (restarts audio, needs permission): live.conf header matches, power applied.
3. Make a folder patch, switch: folder conf is used; pool loads `<n>/<n>.map.json` on the mtime poll.
4. Web: patch list shows folder + sets patches; `/patch/<n>/` serves; `/patch/<n>/<n>.conf` 404; `/patch/%2e%2e/...` and `/patch/<n>/..%2f..%2flive.conf` 404.
5. `cd web && pytest` (tests use `demiurge-set`; should pass unchanged).

## Rollback
Reinstall the previous `demiurge-set` (backup above) over `/opt/demiurge/bin/demiurge-set`; restore `NEPTR_phase4/scripts/demiurge-patch.bak-pre-consolidate`; restore old `demiurge_web.py` and `config.py` and restart those services. `sets/` and live.conf were never modified, so nothing else to undo.
