# BOOTSTRAP — one script, fresh Pi to running Demiurge node

**Bucket: sync** (ships to every Pi). Identity it creates is **local** (`demiurge/local/`, never ships).
For a human or an agent: follow top to bottom. `bootstrap.sh` sits beside this file.

## Prerequisite (only one)
A Pi running **Pi OS Lite 64-bit (Debian 13 Trixie)**, on the network, with a normal user that has sudo.
Run as that user, not root.

## Run it
```bash
# Neural Grid example
DEVICE=neuralgrid-a ROLE=navigator HARDWARE=pi5-8gb PAIR=neuralgrid-b bash bootstrap.sh

# See the plan first. Changes nothing, never calls sudo.
DRY_RUN=1 DEVICE=neuralgrid-a ROLE=navigator HARDWARE=pi5-8gb PAIR=neuralgrid-b bash bootstrap.sh
```
Env: `DEVICE` (required, `a-z0-9-`), `ROLE`, `HARDWARE`, `PAIR`,
`DEMIURGE_REMOTE` (default `https://github.com/EMG0R/demiurge.git` — the PUBLIC distro, cloned anonymously; `DEMIURGE_OS` is the owner's private tree and is never the default), `BRANCH` (default `main`),
`REPO_DIR` (default `~/_______DEMIURGE`). Idempotent: re-run any time.

## What it does (automated)
1. Installs missing prereqs: git, tmux, curl, gh, node/npm; `claude` as an npm global under `~/.npm-global` (PATH gets `~/.npm-global/bin` and `~/.local/bin`). Logs nobody in.
2. Clones the public repo into `REPO_DIR`. If it already exists: "exists, not touching". It never pulls or resets.
3. Runs the repo's `install.sh`, which sets hostname `<DEVICE>`, seeds `device.md`, and runs the phases.
   **The Pi reboots at the end of Phase 8.** After it is back, log in and run the *same command again*; `install.sh` sees `isolcpus=3` and continues with phases 9-11.
4. COD (dormant): fetches public EMG0R/cod, installs its agent persistence + `cod` client, enables linger (details: `setup-script/cod/README.md`).
5. Self-update wiring, fresh installs only (below).

It is a wrapper: install phases, `device.md` seeding, COD (`setup-script/cod/install-cod.sh`) and the updater all stay in their own scripts.

## The one manual step (cannot be automated)
Login is an interactive OAuth flow tied to a person's account. Do it once per Pi:
```bash
claude            # type /login, finish the browser/code flow
gh auth login     # only needed to git push; cloning the public repo needs no login
```
Then pre-accept the first-run trust prompts in `~/.claude.json` (CLAUDE_RC_AUTONOMOUS.md, piece 6) or boot hangs at "trust this folder?". Register an agent with `claude-persist <name>`; check with `claude-persist list`.
Never copy `~/.claude/.credentials.json` or `~/.claude.json` between Pis.

## Self-update afterward
Public GitHub is the source for install and update.
- **stage**: a timer (6 h) compares `DEMIURGE_REMOTE`/`DEMIURGE_BRANCH` from `/demiurge/local/updater.conf` and shallow-clones anything newer into `/demiurge/versions/<hash>`, low priority. Never touches `current`.
- **swap**: at next boot, before `demiurge.service`, `/demiurge/current` flips atomically to the staged version.
- **revert**: `demiurge-update-revert` flips back and pauses updates.

So a fix pushed once reaches every node on its next boot. Nothing restarts at runtime.

## Fresh Pi vs live Pi
The fresh/live call is made once, before install, and remembered in `~/.cache/demiurge-bootstrap/fresh`.
- **Fresh** (no `demiurge` service running, no `/demiurge/current`): bootstrap builds the `/demiurge/current -> versions/<hash>` layout, writes `updater.conf`, and runs `updater/enable.sh` with `CONFIRM_NOT_LIVE_AUDIO=yes`. The timer starts after the next boot.
- **Live** (Demiurge already running or `/demiurge/current` present): the step is **skipped with a loud warning**. A live audio rig is never migrated. Nothing audio is ever restarted.

## Node identity
`MD/DEMIURGE.md` is the parent: what Demiurge is, same everywhere. `demiurge/local/device.md` is the child: YAML front-matter (device, role, hardware, pair) plus prose, which makes this box the named node `<DEVICE>`. Agents read parent then child. Edit `device.md` after install (audio_hat, power, prose); it is never overwritten.

## Known gaps (staged only, untested on a fresh Pi)
- `claude-persist` and the agent units come from the COD repo (fetched by `setup-script/cod/install-cod.sh`); if offline the hook warns and the cod-update timer retries.
- The reboot, phases 1-11, and the layout migration have not been run end to end.
