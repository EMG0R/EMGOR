# Keeping this folder in sync with the Pi

This is the canonical, git-tracked copy of the on-device NEPTR touchscreen UI
(`neptrPhase4_UI.py` + `actions.py`, `companion.py`, `wavetable.py`, etc). It
was imported from the Pi's live `~/NEPTR_phase4/` on 2026-08-16 and should be
treated as the source of truth going forward — not a snapshot.

The Pi is still where the UI actually *runs*. Nothing here changes that.
This folder exists so the code is versioned, reviewable, and recoverable —
not to replace the Pi as the runtime location.

## Pull: Pi → here (do this after editing directly on the Pi)

```
rsync -avz --exclude='__pycache__' --exclude='*.pyc' --exclude='.git' \
  --exclude='_backups' --exclude='backups' --exclude='.venv' \
  --exclude='.pytest_cache' --exclude='_recovered-from-legacy' \
  --exclude='README_LEGACY.md' --exclude='.gitignore' \
  gorpi7@demiurge.local:/home/gorpi7/NEPTR_phase4/ \
  /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/
```

Then `git status` / `git diff` in DEMIURGE_OS to see what actually changed,
and commit with a message describing the change — not "sync".

**Check before pulling, not after:** `--exclude='.gitignore'` is deliberate —
the Pi's own copy predates this folder being tracked in DEMIURGE_OS and has a
different, less strict one. Overwriting it here re-opens the door to build
artifacts / venvs / backups being swept into git the way the initial import
had to explicitly exclude them.

## Push: here → Pi (do this after editing in the repo / from Claude)

```
rsync -avz --exclude='__pycache__' --exclude='*.pyc' --exclude='.git' \
  --exclude='_backups' --exclude='backups' --exclude='.venv' \
  --exclude='.pytest_cache' --exclude='_recovered-from-legacy' \
  --exclude='README_LEGACY.md' --exclude='.gitignore' --exclude='SYNC.md' \
  /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/ \
  gorpi7@demiurge.local:/home/gorpi7/NEPTR_phase4/
```

Then on the Pi: `python3 -m py_compile neptrPhase4_UI.py` before restarting,
and `sudo systemctl restart neptr-ui` to actually load the new code — the
running process does not hot-reload.

## Never let these drift apart silently

Before assuming this folder reflects reality, or before pushing a change to
the Pi, dry-run the pull direction first (`rsync -avzn ...`, note the `n`)
and read the diff. An empty diff (other than `.gitignore`) means they're
already in sync. A non-empty one means someone edited one side without the
other — reconcile by hand, don't blindly overwrite either direction.

## `_recovered-from-legacy/`

Unmerged NAM-opcode work recovered from an older, now-legacy local copy
during the 2026-08-16 import (see its own README). Not part of the running
UI. Needs a deliberate decision to integrate, not automatic sync.
