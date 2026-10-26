# GORCAVE LOG — unattended builder runs (append-only, newest at the bottom)

Started 2026-10-26 by the game builder manager. Format per entry in docs/GORCAVE-BUILD-HANDOFF.md.

## Q01 Error logging (warnOnce) — 2026-10-26 — see commit "Q01 Error logging"
DONE: `warnOnce(tag, e)` added once at ship.js top (before the tuning vars; 30 s per tag, console.warn). Wired to: space.update in dkTick (`space.update`), r25CraftAvail, questScan resource + creature blocks, quest reward giveItem, mission item roll, pet floor (r27 follower), scan highlights (outposts/stores + space targets), hubLife/villageLife ticks (errT reset to 0 each failure so later failures log again; replaces the one-shot console.info). ship-net.js: local copy of warnOnce (separate ES module) for `onClockSnap`. DOM, pointer lock, localStorage, audio ignores untouched.
NOTE: `space.update` at the in-flight call (ship.js step, ~5607) has no try/catch at all; left as is (a throw there already surfaces as a frame error).
TESTS: node --check ok on ship.js, ship-net.js (copied to .mjs); tests/node does not exist yet, none run.
VERIFY ON MAC: load game, fly + land + walk a village with console open: no new warns in normal play; g-tests unaffected (no logic change).
OPEN: ship-net keeps its own 4-line warnOnce (module boundary); could be shared later.
