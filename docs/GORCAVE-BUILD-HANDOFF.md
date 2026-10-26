# GORCAVE BUILD HANDOFF — unattended backlog builder (2026-10-26)

You are a Claude Code builder running ON gorcave (Raspberry Pi 4, 8 GB, user `server`) in `/home/server/EMGOR`, branch `gorcave-backlog`. Emory's laptop is away; a manager session ("game builder") on his Mac reviews your commits, runs the browser tests, and merges to main. You do not merge to main. You do not push main.

Game: NO MANS GOR, a No Man's Sky style game inside emgor.online (Three.js). Read in this order, targeted (line ranges, not whole files): `docs/GORCAVE-QUEUE.md` (your work list), `docs/GORCAVE-LOG.md` (what earlier runs did), `docs/next-stage-plan.md` (specs A-E), `docs/HANDOFF.md` top 2 sections (state, key map, rules), `docs/ship-mode.md` revisions 31-33, `docs/audit-2026-10-13.md` if present. Code: `js/ship.js` (~9.6k lines) + `js/ship-*.js`, engine `js/galaxy3d.js`, fighter `js/smash-*.js`, relay `server/nmg-relay/`. Tests `js/ship-planet-tests.js` g1-g17 are BROWSER-ONLY: you cannot run them here. Pure modules (ship-styles.js, ship-items.js, ship-lingo.js, species generators) can and must get `node` unit tests under `tests/node/*.test.mjs` (`node --test tests/node`).

## Hard rules on this machine
- Only `/home/server/EMGOR`. Never touch `~/EMGOR-pmor`, `~/SPEAK`, `~/lightserver`, `~/gorcave-gate`, `~/smartGOR`, `~/cod-bus`, `~/EMGOR_SKILLS`, systemd units, or any service. Exception: queue item "relay redeploy" names exactly what to restart.
- Disk is tight (15 GB SD, ~1.5 GB free). Before each item: `df --output=avail -m / | tail -1`; if < 700 MB, run `git -C ~/EMGOR gc --prune=now -q`, re-check, and if still < 700 MB STOP (write the reason in the log). Never download media. `npm install` only if a queue item says so.
- Memory/CPU: you share the Pi with live services. No parallel subagents. No browsers. One `node` process at a time.
- Branch `gorcave-backlog` only. One item per run: do the item, `node --check` every edited .js (copy to .mjs under /tmp/nmgcheck/ if `import`/`export` trips it), run `node --test tests/node` if it exists, update docs, commit, `git push origin gorcave-backlog`. Commit message = the queue item title + one line of what changed; end with:
  `Co-Authored-By: Claude Sonnet <noreply@anthropic.com>`
- Edit in place, additive, no renames, one mechanism per job (extend existing systems: markers, pads, lingo, parts kit, fight session, projectile pool). No per-frame allocations in hot paths. Working names only (MARO/JOSHI/STEEV/SONIK, POKEMANS, GOR BALL); no third-party names, art or code. The sm64 decompilation is off limits as source; community constant tables are fine.
- If an item needs the browser to finish, do everything code-side, write exactly what the Mac must verify in the log entry under `VERIFY ON MAC:`, and move on. Never mark anything shipped that you did not run.
- If an item turns out to be wrong/blocked, write `BLOCKED:` + why in the log, leave the checkbox as `[~]`, commit, move on.
- Never rewrite history, never force-push, never `git stash drop`, never delete files (move dead files to `_attic/`).

## Per-run procedure
1. `cd /home/server/EMGOR && git fetch -q origin && git checkout -q gorcave-backlog && git pull -q --ff-only origin gorcave-backlog` (if main moved, do NOT merge it; the manager rebases).
2. Open `docs/GORCAVE-QUEUE.md`. Take the FIRST item marked `[ ]`. Mark it `[>]` (in progress), commit that alone ("queue: start <id>") and push, so a crash is visible.
3. Do the item per its spec. Keep reads targeted. Budget: stop and log if an item exceeds ~2 hours of work; split it into sub-items in the queue instead.
4. Append to `docs/GORCAVE-LOG.md`:
   ```
   ## <id> <title> — <UTC timestamp> — <commit hash>
   DONE: ...            (facts, file:line)
   TESTS: node --check ok on ...; node --test: N pass / M fail
   VERIFY ON MAC: ...   (exact steps / which g-tests)
   OPEN: ...            (anything left)
   ```
5. Mark the item `[x]` (or `[~]` blocked). If the item was a BENCHMARK line, write the benchmark summary block (see queue). Commit, push.
6. Exit. The runner script starts the next run.

## What the manager does with your work
Pulls `gorcave-backlog` to the Mac, runs g-test batches in Chrome (`runAll` in batches of <=5 with reloads), plays it, merges to main (Netlify deploys emgor.online). Anything you flagged `VERIFY ON MAC` gets checked there. Keep the log precise: it is the only thing the manager reads before testing.
