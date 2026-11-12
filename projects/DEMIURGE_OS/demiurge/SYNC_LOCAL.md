# SYNC vs LOCAL — the hard rule (bathtub §5)

> "You always need to remember: are we syncing this to the others or are we not?"

Every file in Demiurge belongs to exactly ONE bucket. When you add or change
anything, say which: **sync bucket** or **local bucket**.

| | `sync` | `local` |
|---|---|---|
| Repo path | `demiurge/sync/**` (+ the rest of the tracked shared tree) | `demiurge/local/**` |
| On a Pi | travels in every `/demiurge/versions/<hash>` | `/demiurge/local` — ONE real dir that survives swaps; each version's `demiurge/local` is a symlink to it |
| Ships to other Pis | YES, via the updater | NEVER |
| Git | the shared Demiurge repo | its own per-node repo (`setup-script/node-repo/`) |
| Enforced by | tracked | `.gitignore`: `demiurge/local/*` ignored, only `.gitkeep` + `README.md` tracked |

Test: "If a fix lands on one Pi, should every other Pi get it?" Yes = sync. No = local.
Credentials, keys, device identity, and anything tuned to one box = local. Always.

## What BELONGS where (nothing has been moved; this is the map)

### sync bucket (ships to all)
- Installer + updater: `setup-script/**` (setup-demiurge.sh, install.sh, updater/, node-repo/)
- Engine + tools: `src/**` (launcher, clock, csound host, `src/wrappers/*`, demiurge-io)
- Config templates: `config/**` (pipewire, wireplumber, pi5-performance)
- Docs/agent maps: `MD/DEMIURGE.md`, `MD/device.template.md`, `demiurge/docs/**`, `incidents/`
- Examples: `demiurge/examples/**` (hello_*), `demiurge/strudel/`, `demiurge/bridges/`
- NEPTR engine + UI source: `neptr/csound/**`, `neptr/neptrPhase4_UI.py`, `neptr/scripts/**`, `neptr/csound/ir/`, `neptr/csound/nam/`
- Web UI: `web/**`
- Defaults as shipped: `demiurge/live.conf` as the SEED (see below), `neptr/menus.json` as seed

### local bucket (this box only)
- `device.md` (identity: hostname, role, hardware, audio HAT)
- Runtime `~/demiurge/live.conf` as edited on the box (the installer never overwrites it)
- Pi-edited `menus.json`, `menu_cycle.orc`, `sm_heroes.json` once they diverge (commit 3f70495: deploy seeds only if missing)
- `~/demiurge/sensors.json`, `touch-cal.conf`, saved presets/state (`neptr/state_backups`, looper persist)
- SSH keys, `gh`/agent logins, Tailscale, Wi-Fi credentials, `updater.conf` (remote URL, pause flags)
- Per-node extra files, tank/COD node registration

### Two-faced files (seed in sync, working copy in local)
`live.conf`, `menus.json`, `menu_cycle.orc`, `sm_heroes.json`: the repo copy is the SEED
(sync). The first install copies it into place if missing; after that the box owns it
(local) and the updater never overwrites it. Never ship a Pi's tuned copy back into sync
without EMGOR saying so.

### Not decided yet (EMGOR)
- `neptr/presets/`, `neptr/sets/`: shared recipes (sync) or per-rig (local)?
- `rig-backup/latest/`: currently tracked as the card-death seed; it is device state, so arguably local.
- `personal/`: should be local or a separate repo; it must never reach the shipping repo.
