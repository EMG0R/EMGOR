# Demiurge bench-deploy runbook

Everything built this cycle lives in the **private** `DEMIURGE_OS` repo. None of it
is deployed. This is the ordered, safe sequence to get it onto TWINK NEPTR (and
later real NEPTR) at the bench. Order = safest first, most invasive last.

## Standing safety (every step)
- **Never restart `demiurge`, `csound`, `neptr-ui`, or the Bela without a per-time
  go, and never leave the engine down.** Restarts below are marked **⚠AUDIO** or
  **⚠CTRL** (control plane — brief param-control gap, still ask).
- Back up the known-good thing before overwriting it. Verify each step before the next.
- `power = low` is incompatible with the core layout (step 5) — leave it `medium`/`high`.

---

## Step 0 — Backup + git unification (GATING, do first)
TWINK's live tree `~/_______DEMIURGE` is behind origin and has local edits; merging
blindly drops ~24 MB of ROMs (see `MD/HANDOFF-2026-10-05-git-unification.md`).
1. **Back up the whole working tree incl. ROMs + uncommitted edits** to the TANK or a
   safe disk: `tar czf ~/demiurge-tree-backup-$(date +%F).tgz -C ~ _______DEMIURGE`.
   Confirm the ROMs are in the tarball (`tar tzf … | grep -ic rom`).
2. Preserve the local keepers first: `demiurge/local/device.md` is LOCAL bucket — it
   must survive (it's gitignored / nested repo; confirm it's untouched after).
   The `CLAUDE.md/GEMINI.md/AGENTS.md` pointer edits are **already in origin** — the
   Pi's uncommitted copies are redundant and can be discarded.
3. Reconcile to origin per the git-unification handoff (backup-first), verifying the
   ROM count in the working tree is identical before and after. **Do not** `git pull`
   without that check.
Result: TWINK's tree == origin, ROMs intact, `device.md` intact.

## Step 1 — Parameter pool (`demiurge-io` extension)  ⚠CTRL
Source: `src/demiurge-io/` + its `DEPLOY.md`.
- Render the unit: replace `@RIG_USER@` with `gorpi7v2` (DEPLOY.md gives the `sed`).
  **This also fixes the hardcoded `gorpi7` user bug.**
- Deploy the package + unit; then **⚠CTRL restart `demiurge-io`** (brief param gap).
- New sources/sinks are opt-in flags — deploy with them OFF first; only `--osc-in`
  (port 9102) is new-default-safe once confirmed free.
- **Verify:** existing sensor maps still drive csound exactly as before; encoders
  single-step; `port 9102 binds`; journal clean; idle CPU unchanged.
- **Rollback:** reinstall the previous `demiurge-io`, restart it.

## Step 2 — Patches consolidation  ⚠AUDIO (on a switch)
Source: `src/wrappers/demiurge-patch` (new canonical), `demiurge-set` (shim),
`web/demiurge_web.py`, `src/demiurge-io/.../config.py` (folder-first map), `docs/patches-consolidation.md`.
- Wrappers take effect immediately (no restart). Back up `/opt/demiurge/bin/demiurge-set`
  first (`setup-script/deploy-wrapper.sh` does it safely).
- `config.py` change rides step 1's `demiurge-io` restart. Web change needs a
  **web-service restart** (not the audio trio).
- **Verify:** switch a `sets/` patch (car_mode/neptr_bass) **⚠AUDIO** and a folder
  patch **⚠AUDIO**; `# set:` header matches; pool loads `<n>/<n>.map.json` on the
  poll; `/patch/<n>/` serves on the phone; `.conf` + `..`/`%2e%2e` traversal → 404;
  `cd web && pytest`.
- **Rollback:** restore the `demiurge-set` backup; revert the web file.

## Step 3 — Cardinal headless  (CPU-heavy — NOT during a performance)
Source: `setup-script/deploy-cardinal.sh`, `docs/cardinal.md`.
- Run `sudo CONFIRM_NOT_LIVE_AUDIO=yes bash setup-script/deploy-cardinal.sh` — it
  downloads ~1 GB and/or builds under `nice`/`ionice`. **Do it when the engine is
  idle/stopped (your call); a build can xrun a live rig.**
- **Verify:** `cardinal-headless --version` (xvfb wrapper); confirm `.vcv` load arg.
- **Rollback:** remove `/opt/demiurge/cardinal/` + the wrapper.

## Step 4 — Self-update (optional, keep DISABLED on the live rig)
Source: `setup-script/updater/`. Only enable on a non-performing box:
`sudo CONFIRM_NOT_LIVE_AUDIO=yes bash setup-script/updater/enable.sh`, then put
`DEMIURGE_REMOTE=https://github.com/EMG0R/demiurge.git` (the PUBLIC repo) in
`/demiurge/local/updater.conf`. Needs the `/demiurge/current` symlink layout first.

## Step 5 — Core layout  (reboot + boot-config — most invasive, LAST)
Source: `config/pi5-performance/demiurge-coremap.*`, `docs/core-layout.md`.
- Hand-edit `cmdline.txt`: `isolcpus=3` → `isolcpus=1-3` (**you own cmdline.txt**),
  plus the companion changes the doc lists (`demiurge-irq-isolate` mask 7→1,
  `demiurge.service.d/cpu-affinity.conf` `0-2`→`0`, `rt_core = 1` in live.conf).
  Confirm `power` is not `low`. `sync`, reboot.
- **Verify:** `/sys/devices/system/cpu/isolated` reads `1-3`; pins land; run
  `demiurge-ringtest` before/after to catch added cross-core latency; find the
  Cardinal module ceiling with `jack_cpu_load` + xrun count at quantum 64 and 128.
- **Rollback:** restore the old `cmdline.txt`, reboot.

---

## Not in this runbook (separate / future)
- The §1.4 no-RT utility-audio client (new PipeWire node) — design only; bench it alone.
- NEPTR persona UI-regeneration ("the selling point") — a real build, not yet started.
- Public `demiurge` repo + the `publish-public.sh` export — **A's lane**; must exist
  before step 4's self-update has a public source to pull.
