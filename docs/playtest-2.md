# Playtest 2 (local 127.0.0.1:8123, rev 28, 2026-10-06, automated Chrome tab)

Method: `?pt`, `/user claude`, `ship-planet-tests` harness (`makeHarness`) to land and walk, `EMGOR_SHIP.rev27/rev28` hooks, frames stepped via `EMGOR_GALAXY.step`. Local relay on 8796 (NMG_DATA=/tmp/nmgdata, ALLOW_NO_ORIGIN=1, NMG_EVENT_EVERY_MS=5000), stopped afterwards. Tab closed.

## What ran and result
1. Cop: grab in view -> walk out with unpaid cart -> copWarn chat + siren loop (R28.siren) + chase -> BUSTED row, fine charged, items returned, siren stops. PASS after 2 fixes.
2. Station: dock -> `STATION` greeting row, warp sound call, bounties board filled (3 quests), shipyard F menu, bought Drifter Explorer (gor deducted, hull owned), trade terminal opens and buys. PASS after 1 fix (terminal unreachable on foot).
3. Landmark: banner + `LANDMARK DISCOVERED` row + claim OK. Trophy: `int.trophies.setDiscoveries` is wired and present in the ship interior; trophy mesh not inspected visually.
4. Scanner: ore rock shows `ORE 60 L` hl marker when in view (scan uses space.scanTargets; 363 targets, kinds ore/crystal/ice/derelict/crate/convoy). PASS. Derelict/crate markers not seen in view.
5. Effects: mirror (double + hue, pink ghosting) and tunnel (wide FOV + contrast vignette) screenshots clearly differ. PASS.
6. Pet feed: F at a pen opens FEED menu; feeding gives happy 1->2, mood 0->1, petHappy sound call. PASS.
7. Burger House: 3 tiers buyable (8/12/28 gor), glow 30/45/60 real minutes. PASS (only 3 tiers exist, not more).
8. Blueprints: `bpDrop` returns a blueprint item and it lands in inventory. Wiring to dkLoot (derelict crate) and questComplete (bounty) read in source, not driven live (no real derelict crate / bounty completion run).
9. Relay: `/user claude secret1` registered, saved (profiles.json written), localStorage wiped + reload + login -> PROFILE LOADED, gor restored. disc.claim announced as `DISCOVERY ... first seen by CLAUDE`. Events (5 s) produced chat rows + banner; meteor event rained shards on foot and pickup gave +1 METEOR SHARD. PASS after 2 fixes.
10. On-foot LMB: mousedown (with a faked pointer lock) sets R27.fire, ffCd cycles, ground enemy hp dropped below 0. PASS.

## Fixed (js/ship.js only)
- Cop walk-out never chased: shopTick set heat to exactly 2, which decays below the chase threshold the same frame -> now 2.6. Also `sec.seen` goes false the instant you step outside (sees() needs you inside), so walking out was always "walked out clean · nobody saw"; now remembers seen for 2.5 s or heat >= 1 (R28.seenAt).
- Station trade terminal could not be triggered on foot (terminal is 1.3 m up, trigger radius 1.2 L measured to the feet) -> 1.8 L.
- Relay profile registration lost: server rate-limits load+save to 1 per 10 s, client saved right after the load and never retried -> on `rate` while loaded it re-sends in ~11 s. Verified profiles.json written.
- Event text "A TITAN IS HUNTING NEAR " dangled when no planet resolved -> falls back to THE SYSTEM.
- Added test hook `EMGOR_SHIP.rev28.pf` (profile).

## Still fails / open
- Reload race: opening the page twice quickly against the relay made the newer tab get "online in another tab; single-player" for good (kicked flag is permanent until reload). Relay replaces the old socket with 4000 but a stale socket from the unloading page can win. ship-net.js was not changed.
- ship-world: heat decays before secUpdate, so grabbing in view only reaches chase if heat is raised again; a chasing cop inside the store cannot reach a player outside (bust happens only once the cop reaches the player; it did after ~8 s in the test, fine 63).
- `/user claude secret1` with a fresh relay always shows `PROFILE REGISTERED`; fine, but the relay data dir only appears after the first save.
- Not driven: derelict crate loot blueprint, bounty-reward blueprint, landmark trophy visual, scanner derelict/crate markers, healthz (8797 not listening in this relay build).
