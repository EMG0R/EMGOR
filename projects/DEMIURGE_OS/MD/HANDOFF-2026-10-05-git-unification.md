# HANDOFF — cross-device git + shared memory unification (2026-10-05)

Status: **partially done, stopped mid-way at Emory's request.** Memory sync is
finished and verified. Project-repo unification is half-wired and has one
**destructive hazard that must be handled before anyone pulls on the Pi** (§3).

---

## 1. DONE and verified

### Shared memory (EMGOR_SKILLS) — complete
All of Mac, gorcave, NEPTR (demiurge) and the phone share one repo and stay in
lockstep. Proven end to end: a memory written on NEPTR, left unsynced, was
published by the boot timer after a reboot and reached the Mac and gorcave.

- `scripts/sync.sh` rewritten: pulls from the first remote that answers, pushes
  to **every** remote (the GitHub mirror had silently fallen 10 commits behind),
  merges instead of rebasing, and never exits mid-merge.
- **Conflicts never lose a fact.** `.gitattributes` union-merges every
  `MEMORY.md` index so both machines' lines survive; any other conflicted file
  keeps OURS in place and writes THEIRS beside it as
  `<name>.conflict-<host>-<ts>.md`, logged in `memory/_CONFLICTS.md`.
- **Deletion guard**: >5 deleted files under `memory/` and the run refuses to
  commit or push anything (override `EMGOR_SYNC_ALLOW_DELETIONS=1`).
- `sync.sh --doctor` — staleness, every remote, the timer, conflicts, held lock,
  deleted-but-unpushed memories. **Run this first when memory looks wrong.**
- Trigger is **schedule-only, by Emory's instruction**: `emory-core-sync.timer`
  at boot +90s and daily 03:10. Per-session SessionStart/Stop hooks were
  explicitly REMOVED — do not reinstate them.
- `scripts/install-sync.sh` wires a new machine in (idempotent).
- The bar for *what* is worth saving was raised — see "Shared memory rules" in
  `_EMORY.md`. Durable, not derivable, behaviour-changing, general. No journals.
- `tools/emory-mcp/` — the phone's MCP gateway, previously on one SD card in no
  repo with no backup, is now vendored (token NOT in the repo). New
  `memory_health()` tool exposes `--doctor` to the phone.

### Agent toolchain on the Pi — complete
claude 2.1.288 / gemini 0.62.0 / codex 0.160.0 / gh 2.102.0 / tmux / git, all
installed and authenticated as EMG0R. `demiurge-agent` and
`setup-script/pi-agent.sh` work. Phase 11 of the installer reproduces all of it,
including cloning EMGOR_SKILLS.

### DEMIURGE_OS now has a real remote
- Private repo created: **`git@github.com:EMG0R/DEMIURGE_OS.git`**
- Mac `origin` → that repo, pushed (96 commits). Mac is current.
- NEPTR `~/_______DEMIURGE` had **zero remotes** (it could commit but never
  publish — this undercut the whole point of agents on the rig). `origin` now
  added there, fetched, **but NOT yet merged or pushed.**

---

## 2. STATE RIGHT NOW

| machine | repo | state |
|---|---|---|
| Mac | DEMIURGE_OS | `origin` = GitHub, pushed, clean |
| NEPTR | `~/_______DEMIURGE` | `origin` added, **4 behind, 2 files dirty, nothing pushed** |
| gorcave | DEMIURGE_OS | **not cloned yet** |
| all | EMGOR_SKILLS | healthy, in lockstep |

NEPTR's dirty files are real agent work, not junk:
`MD/README.md` (modified) and `MD/HANDOFF-2026-10-05-norris-rack.md` (new).

---

## 3. ⚠️ HAZARD — read before pulling on the Pi

ROMs and MAME artwork were untracked on the Mac (`git rm --cached` + gitignore)
because pushing copyrighted binaries to GitHub is a ToS/copyright problem.
**That commit records a deletion of those paths.** They are still *tracked* in
NEPTR's clone, so merging will **delete 24.3 MB of ROMs from the Pi's working
tree**. `.gitignore` does not protect an already-tracked file.

Before pulling on NEPTR:
```sh
ssh gorpi7v2@demiurge.local 'cd ~/_______DEMIURGE && \
  tar czf ~/games-roms-backup-$(date +%Y%m%d-%H%M%S).tgz \
      demiurge/games/roms demiurge/games/system/mame/artwork'
```
then commit the Pi's own work, merge, restore the files if they vanished, and
push. Verify `ls demiurge/games/roms | wc -l` is 10 afterwards.

Also note: the 24.3 MB of ROM blobs **remain in the 96 commits of history**, so
they did go to GitHub in the first push. Stripping them needs `git filter-repo`,
which rewrites all history and would force-reset every clone — a separate,
destructive job. Emory has NOT approved it. Ask before doing it.

---

## 4. NOT DONE — what Emory asked for next

His words: *"the 2 neptrs need to be on the demiurge git and the neuralgrid needs
to be on the neuralgrid git type shit but server should have access to all of
them and they should be able to navigate my full git stuff and decide to push
pull things autonomously"* and *"agents should at start of big agent not subagent
session always try a push to see whats up"*.

1. **Finish NEPTR** — §3, then merge and push. Its two doc files must survive.
2. **gorcave gets everything** — clone DEMIURGE_OS (and the other project repos)
   so the server can navigate them all. gorcave already has `gh` authed and an
   SSH key to GitHub. The web-console agent confirmed cloning into `~/` is safe.
3. **Second NEPTR ("twink")** — doesn't physically exist yet. When it does it is
   just: clone the repo, run `setup-demiurge.sh 11`. Nothing special needed.
4. **neuralGrid — BLOCKED and needs a decision.**
   - `neuralgrid.local` **does not resolve**; the Pi is offline/renamed. Cannot
     be wired until it is reachable.
   - `______2026NEW/neuralGrid/` (197 MB) is **not its own git repo** — it is
     tracked inside the `_EMGOR_SYNTH` parent repo. Giving it its own repo means
     either a nested repo inside `_EMGOR_SYNTH` (messy) or extracting it. That
     touches the archive, which is **read-only to tooling, additive only** — do
     not do it without asking Emory directly.
5. **Session-start repo sync** — not built. Intended shape: one script in
   EMGOR_SKILLS (so it travels everywhere) that, for the project repo you are
   in, fetches, pushes when ahead and clean, fast-forwards when behind and clean,
   and only reports when diverged or dirty. Wire as a SessionStart hook for main
   sessions only — **never** for subagents, and keep it separate from memory
   sync, which Emory deliberately made schedule-only.

---

## 5. Coordination (other live sessions, 2026-10-05)

- **emgor-37** = gorcave web-console / 2FA agent. Owns `~/gorcave-gate/`,
  `gorcave-gate.service`, the new `gorcave-gate-idle.timer`, `~/dashboard/app.py`,
  and the funnel paths `/api/gate`, `/terminal`, `/api/dash`. **Do not restart
  `gorcave-gate` or the persistent tmux session named `neptr`.** It also edits
  `~/.claude/settings.json` — always read-merge-write that file, never clobber.
- **emgor-9b** = cod Tauri app session. May write one memory file and run
  `sync.sh` once. No infra overlap.

## 6. Rules that bit, or nearly did
- `pkill -f <pattern>` matches your own SSH command line and kills the session.
  Use PIDs.
- `stat -f %m` is BSD; on Linux GNU `stat -f` means *filesystem* and succeeds
  with garbage. GNU `-c %Y` first. This silently broke the lock-breaker on both Pis.
- `/opt/demiurge/bin` is not on sshd's PATH and `~/.npm-global/bin` is only in
  `.bashrc` (never sourced non-interactively). User-facing tools get symlinked
  into `/usr/local/bin`.
- Audio is off-limits this session by Emory's instruction. `neptr-ui` is
  inactive after reboot (pre-existing: `neptr-video` owns the display).
