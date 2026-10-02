# NO MANS GOR — multiplayer plan (v1)

Status 2026-10-01: plan only. Nothing built. Runs in parallel with the single-player game, lands later.
Rule zero: with nobody else online, the game is byte-for-byte the single-player game.

## Decision

- **Netlify: unchanged.** Static site stays static. Netlify cannot hold a WebSocket server (functions are request/response, time-limited). No plan change, no new bill.
- **One tiny WebSocket relay on gorcave** (Pi 4): Node + `ws`, `nmg-relay.service`, listens on `127.0.0.1:8796`. Reached publicly as `wss://gorcave.taila85593.ts.net/nmg` through the Tailscale Funnel the site already uses for chat/dash/lights. Cost: 0.
- **Client-authoritative, relay is dumb.** Every player simulates everything locally (flight, enemies, HP, waves). The relay only fans out "where is each ship". No game logic server-side, nothing to cheat-proof, nothing persisted.
- **One shared world, no rooms.** One presence table. Everyone online is in it.
- Why not the others: a managed relay adds an account + a quota to babysit; the Pi relay is ~100 lines and the Funnel path already exists. Pi down = single-player, same as today's gorcave pages degrading.

## What the repo shows about the Funnel (and what it can't)

- Every gorcave page (`gorcave-chat/dash/lights/app/voice.html`) calls `https://gorcave.taila85593.ts.net` on **port 443, path-routed**: `/api/chat`, `/api/dash/*`, `/api/voice/sing`, `/m/<token>/sync`. No other ports appear anywhere. Plain `fetch`, no auth header, no `credentials`, from emgor.online -> so the Pi side already answers cross-origin (CORS) for that origin.
- Funnel only exposes public ports 443, 8443, 10000. So the relay does NOT get its own public port 8796; 8796 stays loopback-only and is mounted as a **path on 443** next to the others.
- Not determinable from this repo: whether the path routing is `tailscale serve/funnel --set-path` entries or a local reverse proxy (Caddy/nginx) sitting behind a single Funnel target. The Pi config is not in this repo. Do on the Pi first (see "Wiring"):
  `tailscale funnel status` (shows path map) and `ss -ltnp | grep -E '8787|8790|8795|8796'`.
- WebSocket through Funnel/serve proxy works (HTTP upgrade passes). Through Caddy/nginx it needs the upgrade headers (shown below).

### Wiring (pick the branch that matches `tailscale funnel status`)

A. Paths are tailscale entries (most likely):
```
sudo tailscale funnel --bg --set-path /nmg http://127.0.0.1:8796
tailscale funnel status          # confirm /nmg appears beside /api/...
```
`serve` strips the `/nmg` prefix before proxying, so the relay accepts both `/` and `/nmg` (it ignores the path).

B. Paths are a local reverse proxy (Caddy):
```
handle /nmg* { reverse_proxy 127.0.0.1:8796 }     # Caddy upgrades WS automatically
```
nginx: `location /nmg { proxy_pass http://127.0.0.1:8796; proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection "upgrade"; proxy_read_timeout 120s; }`

Do not touch existing 8787/8790/8795 mappings. Additive only. Verify existing chat/dash still load after.

## Protocol (JSON text frames, one object per frame)

Client -> relay:
- `{t:'hi', id, name, color}` once on open. `id` = 12-char random, `localStorage['nmg-id']`. `name` default `PILOT-xxxx` (last 4 of id). `color` = hex int 0-0xFFFFFF, default violet 0x8a5cff.
- `{t:'pos', id, x,y,z, qx,qy,qz,qw, v, st, hp}` at **10 Hz only while `state==='piloting'`**. `v` = speed (u/s, int), `st` = flags bitfield (1 boost, 2 pulse, 4 dead), `hp` = own HP 0-255 (percent of max). Position rounded to 0.1, quat to 4 dp.
- `{t:'st', id, s:'docked'|'away'}` once on leaving piloting (stops pos stream; others dock the ghost).
- `{t:'fire', id, w, x,y,z, dx,dy,dz}` per shot (w = weapon, 0 gun / 1 missile if any), at most 10/s. Origin + unit direction at fire time. Shooter does NOT decide hits.
- `{t:'hit', to, dmg}` NOT sent: damage is victim-authoritative (see Friendly fire).
- `{t:'kill', by}` sent by the VICTIM when its own HP reaches 0 and the last damage came from a remote bolt; `by` = shooter id. Relay forwards to everyone (`{t:'kill', id:<victim>, by}`) so shooter's client credits the kill.
- `{t:'hb'}` every 5 s when nothing else was sent in 5 s.
- Later: `{t:'hi', ..., name, color}` re-sent on `/name` or `/color` change (relay treats a repeated `hi` as an update).

Relay -> client:
- `{t:'roster', you:id, epoch:<ms>, ships:[{id,name,color,s,x,y,z,qx,qy,qz,qw,v,st,hp}]}` on join (everyone except the receiver), and on every join/leave/rename/state change (not on pos).
- `{t:'pos', ...}` fan-out of a sender's pos frame to all others, unchanged, `id` set by the relay from the connection (never trusted from the payload).
- `{t:'bye', id}` on drop.

Server rules:
- Presence table `Map<id, {ws, name, color, s, last, pos}>`. Drop at 15 s silence (sweep every 5 s). Same `id` reconnecting replaces the old socket (second tab: last wins, old gets closed with 4000).
- `epoch` = relay-start wall-clock ms (`Date.now()` at process start, held constant until restart). Relay also answers `{t:'ping', c}` with `{t:'pong', c, now:Date.now()}` so clients estimate clock offset (min-RTT of 3 pings).
- Pos frames are stored (last known) so a joiner's roster has positions, and are forwarded as-is.
- No persistence, no logs of content, no disk writes. Restart = empty room, clients reconnect.

## Client behavior (`js/ship-net.js`)

- Ghosts are `THREE.Group`s built with `buildHull(THREE)` from `js/ship-hull.js`, re-tinted by `color` (same vertex-color/uniform route `tintAlly` uses; do not clone geometry per ghost beyond what buildHull returns, build once per remote id). Label = DOM `.ship-label`-style button with name, projected each frame like the local one.
- **Interpolation:** keep the last 3 samples per ghost with receive time; render at `now - 150 ms`, lerp position, slerp quat; if the buffer runs dry extrapolate by `v` along forward for max 300 ms then freeze. Snap if jump > 50 000 u (teleport/respawn).
- **Galaxy view (not flying):** every remote ship with `s !== 'piloting'` is a docked slot on the black-hole orbit, same math as `idle()`'s dock (radius 0.35 x root.sysR), spaced evenly by index in sorted-id order so slots do not jump when someone joins at the end; slot angle = dockA + 2*PI*i/(n+1). The local ship keeps its own slot at index 0. Ghosts that ARE piloting appear at their real position, visible from the galaxy camera too (small, so give them a violet glow sprite and a minimum screen size so they read from orbit view).
- **Click to board:** clicking a docked ghost does nothing special in v1 (ships are interchangeable; "click any ship to board yours" -> boarding always enters YOUR ship, i.e. calls local `enter()`). The ghost label is a button whose click calls the same `enter()`. Remote ships are never controllable.
- **Ship mode:** all ghosts rendered in the scene with names; no collision with ghosts. Players can shoot each other (see Friendly fire).
- **Failure:** `connect()` opens the socket; if not OPEN within 2000 ms, close it, set `net.offline`, schedule a retry in 60 s (never faster; backoff to 5 min after 3 fails; reset on success). Mid-session drop: same, ghosts fade out after 3 s and are disposed. Zero console noise beyond one `console.info`. The game never waits on the network and never shows an error in the HUD (a tiny optional "ONLINE n" pip only when connected and n > 1).
- Tab hidden: rAF stops so pos stops; heartbeat keeps running on a `setInterval` (5 s). Relay drops after 15 s hidden-silence if the interval is throttled; reconnect on `visibilitychange`.
- Phones: not loaded (existing ship.js gate: coarse pointer / no pointer lock / width < 900). So no phone ghosts; phones do not appear in the roster in v1.

## Friendly fire (players can shoot each other)

Client-authoritative on the VICTIM, shooter only announces.
- Shooter's client fires its bolt locally as today (hits only enemies/local stuff) and sends `fire` per shot.
- Every other client spawns a **remote bolt**: same bolt pool/`fireBolt` visuals, tagged `owner=id`, marked `remote` (never damages enemies/allies, never fires further events). It starts at the fire origin advanced by the shooter's latency (`now - sentTime` is unknown, so advance by half the 150 ms render delay), flies straight with the normal bolt speed/life.
- Hit test happens ONLY on the victim's own machine, against the victim's real ship (not the interpolated ghost), reusing the existing enemy-bolt-vs-player segment test. On hit: apply `dmg` to own HP, play the normal hit FX, kill that remote bolt locally. Victim's new HP rides in its next `pos` (`hp`); watchers lerp a small health pip on the ghost label from it.
- Death: victim's existing death flow runs; sets `st` bit 4, sends `{t:'kill', by}` once. Shooter gets a "KILL <name>" announce and a kill count +1 (local `kills`, same as enemy kills; PvP kills do not advance waves). Respawn = existing restart path (HP reset, back at the dock); `hp` resets in the next pos.
- Cost of this model (accepted): a shooter sees a remote bolt "miss" that the victim registered, or vice versa, by up to ~150 ms of latency. Victim always wins. Cheating = a modded victim client ignoring damage; irrelevant for a chill world.
- Rate/abuse: relay caps `fire` at 12/s per connection (included in the 20 msg/s bucket, bump bucket to 30/s burst 60); clients cap incoming remote bolts at 64 live total.
- Docked/away players are not targetable (no ship in space). Piloting players inside the safe dock radius (0.35 x root.sysR x 1.2) are not damaged, so nobody gets camped at spawn.
- Allies and enemies never target remote players in v1 (local sims know nothing about ghosts). Ghost ships do not collide with anything.
- Bandwidth: fire adds <= 12 x 120 B = 1.4 KB/s per shooter while holding fire; at 8 players all shooting ~+80 KB/s relay out, at 32 ~+1.4 MB/s worst case (never realistic). Table above is flight only; add this for combat.

## Consistency: shared vs local

Shared (relay carries it): ship positions, headings, speed/boost/pulse flags, firing events, HP, deaths/kills, names, colors, docked/piloting state, the **orbital clock**.

Local per player (v1): enemy waves, bosses, allies, wave counter, kills/score, HUD, camera, everything procedural/bodies' meshes. Two players near each other see different enemies. By design.

**The planet clock problem.** Planets orbit by engine `time` (`galaxy3d.js` closure var `time`, exposed read-only as `engine.time`, line ~2302), which starts at 0 on every page load. So a remote ship parked "next to Neptune" lands in empty space on your screen because your Neptune is elsewhere on its orbit. Fix: one shared clock.
- Relay sends `epoch` (above); client computes `sharedT = (Date.now() + offset - epoch) / 1000` where `offset` is from the ping/pong estimate (only matters at ~100 ms scale, orbits are slow; skip the ping in v0 and use raw `Date.now()` if client clocks are within seconds, which they are).
- **galaxy3d.js change (small, additive):** add `setClock(fn | null)` to the engine port: where the frame loop advances `time += dt`, use `time = clockFn ? clockFn() : time + dt`. Single-player/offline = `null` = today's behavior exactly. Also `engine.time` getter unchanged. No other galaxy3d code touches `time` differently.
- ship-net calls `engine.setClock(...)` once the first roster arrives with an epoch, only if n >= 2 players, and eases the jump: lerp from local time to shared time over 2 s (rate-limited catch-up of at most 30% dt) so planets do not teleport. On disconnect, `setClock(null)` continues from the current time (no jump).
- Planets that wobble with `vibF/vibPh` are functions of `time` too, so they sync for free. Anything driven by `Math.random()` or `performance.now()` (particles, star twinkle) stays local and is not visible as inconsistency.
- Net effect: a ship at (x,y,z) near a planet is at the same place relative to the planet for everyone, because world coordinates are absolute and the planets are at the same orbital phase. Fixed `epoch` per relay process means a relay restart shifts everyone to a new phase: clients re-sync on reconnect through the same ease. Persisting epoch to disk would avoid that; not worth it in v1.
- Check before implementing: confirm every moving body (incl. root/black hole accretion, orbit lines) derives from `time` and not its own accumulator; any that don't get converted or accepted as cosmetic.

**v2: shared enemies** (host election, replaces the local-enemy rule). Relay elects host = oldest connection, sends `{t:'host', id}` on change. Host runs `spawnWave`/enemy stepping and streams `{t:'en', e:[...]}` at 10 Hz; non-hosts render interpolated puppets, send `{t:'hit', eid, dmg}` to the host, host applies and broadcasts `{t:'edead', eid, by}`. Host leaves -> next-oldest takes over and the wave restarts from the current wave number. Needs stable enemy ids and a puppet mode in `ship-enemies.js`. Allies stay local or move to the host the same way.

## Enemies and combat in v1

Enemies, waves, bosses, allies and HP are per-player local. What breaks if two players "fight the same" enemies:
- Each player sees different enemies in different places; a player can be shot by an enemy the other cannot see (invisible to the watcher).
- Watching someone fight, you see them dodge nothing and shoot at empty space. Kills and wave counts are not shared; scores are per-player (`localStorage['nmg']` unchanged).
- Player-vs-player bolts ARE synced (Friendly fire above); enemy bolts are not.

v2 path: host-elected enemy ownership, see Consistency above.

## Security / abuse

- Origin check on upgrade: allow `https://emgor.online`, `https://www.emgor.online`, `http://localhost:*`, `http://127.0.0.1:*`; reject everything else with 403. (Netlify deploy previews `*.netlify.app` rejected unless Emory adds one.)
- Max 32 clients; the 33rd gets close code 4001 "full" (client treats as offline, 60 s retry).
- `maxPayload` 512 bytes. Parse failures / unknown `t` / bad types: drop the frame, 5 strikes closes the socket.
- Rate limit per connection: token bucket 20 msgs/s, burst 40; over -> drop frames; sustained over for 5 s -> close 4002.
- Sanitize: name = printable ASCII, trimmed, max 16 chars; color clamped to 24-bit int; numbers must be finite and |x| < 1e7 else drop the frame. Relay re-serializes (never forwards raw client text).
- Per-IP cap 4 sockets (use `X-Forwarded-For` first hop via Funnel; fall back to remote address).
- No auth, no accounts, no chat in v1 (no text from strangers except the 16-char name). Names are the only abuse surface; add a tiny blocklist later if needed. Anonymous `id` is not a secret and not an identity.
- Relay binds 127.0.0.1 only. systemd hardening: `NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome=read-only`, `PrivateTmp`, `MemoryMax=64M`.

## Bandwidth and Pi load

pos frame ~ 130 bytes JSON. Only piloting players send at 10 Hz. Worst case, everyone piloting, fan-out N-1.

| players | in (relay) | out (relay) | per client down |
|---|---|---|---|
| 2 | 2 x 1.3 KB/s = 2.6 KB/s | 2.6 KB/s | 1.3 KB/s (~10 kbit/s) |
| 8 | 10 KB/s | 73 KB/s | 9 KB/s (~73 kbit/s) |
| 32 | 42 KB/s | 1.3 MB/s | 40 KB/s (~320 kbit/s) |

Per client up is always 1.3 KB/s. Docked players cost ~0 (heartbeat only). Real traffic is far under the worst case. 32 is the hard cap and comfortable on any home uplink. If it ever matters: drop to 5 Hz beyond 16 players, or binary frames (Float32 + Uint8, ~44 bytes, 3x smaller) — not needed now.

Pi 4 load: Node `ws` fan-out of 32 x 31 x 10 = ~10k small sends/s worst case is ~10-15% of one core; typical (2-8 players) is noise (<1%). RAM ~40 MB. Tailscale/Funnel proxy adds overhead on encrypted egress at 1.3 MB/s: negligible. It shares the Pi with lightserver (:8800, LED render, time-sensitive) — mitigate with `Nice=10` and `CPUQuota=25%` on the unit so the lights never stutter because of a crowd.

## Files

New (in this site repo; Pi repo of record for smartGOR stays `2026NEW/smartGOR/`, relay source lives here because it ships with the client protocol):
- `server/nmg-relay/server.js` (~120 lines, `ws` only dep), `server/nmg-relay/package.json`, `server/nmg-relay/nmg-relay.service`, `server/nmg-relay/README.txt` (deploy + test, 10 lines).
- `js/ship-net.js` (client, ~200 lines). Exports `connect(engine, hooks)`, `sendPos()`, `onRoster(fn)`, `setName(n)`, `setColor(c)`, `destroy()`.
  - `hooks` = `{ getState():'docked'|'piloting'|'away', getPose():{x,y,z,qx,qy,qz,qw,v,st}, buildGhost():Group, tint(group,color), dockSlot(i,n):Vector3, enter() }` — ship-net knows nothing about ship.js internals.
Edited later (not now, ship.js is being changed concurrently):
- `js/ship.js` — minimal hook points, all guarded by `net && ...`:
  1. end of `mount(engine)` (after hull loads, near `import('./ship-hull.js')`): `import('./ship-net.js').then(m => net = m.connect(engine, hooks)).catch(()=>{})`. Lazy, after mount, never blocks.
  2. `step(dt)`: one line at the end: `net && net.sendPos()` (ship-net throttles to 10 Hz itself via its own accumulator; reads pose from `shipRoot.position/quaternion`, `speed`, boost/pulse/`dead` flags).
  3. `idle()` (docked orbit loop, ~line 1498): `net && net.update(dt, dockA)` so ghosts are interpolated and docked slots are laid out even in the galaxy view; also called from `step` so ghosts render in flight.
  4. `enter()` / `exit()`: `net && net.setState('piloting' | 'away')` (sends `st`).
  5. Death/restart path (`dead` set at ~line 739, restart ~1185): flag bit 4 in `st`; ghost hidden while dead.
  6. Slash-command handler (~line 1129, where `boss` is parsed): add `/name <text>` and `/color <hex|name>`, both persist to `localStorage['nmg-prefs']` (separate key; never touch `'nmg'` save) and call `net.setName/setColor`; `/color` also tints the local hull via `tintAlly`-style path.
  7. `dispose`-style teardown (end of file API object ~line 1542): `net && net.destroy()`.
- `style/ship.css` — `.ship-ghost-label` (reuse `.ship-label` look, 60% opacity, no pointer events while piloting).
- `js/galaxy3d.js` — `engine.setClock(fn|null)` (3 lines in the frame loop where `time` advances). Only galaxy3d edit.
- `js/ship.js` also: bolt-vs-player test emits `net.onLocalDamage`, remote bolt spawn in `fireBolt` path with `owner`, local `fireBolt` calls `net && net.sendFire(...)`.
- `docs/ship-mode.md` — one-line pointer to this file.
Besides `setClock`, no change to `galaxy3d.js`: ship.js already mounts after `galaxy-ready` and owns its own dock; ghosts hang off the same `engine.scene`. The galaxy-view requirement is met because `idle()` keeps running while `state` is `docked/away`.
No change to `netlify.toml`/`_headers`. CSP: none set in `_headers`, so `wss://gorcave.taila85593.ts.net` needs no allow-listing (re-check if a CSP is ever added: `connect-src wss://gorcave.taila85593.ts.net`).

## Relay deploy

```
# Mac
scp -r server/nmg-relay server@gorcave.lan:~/nmg-relay
# Pi
cd ~/nmg-relay && npm ci --omit=dev
sudo cp nmg-relay.service /etc/systemd/system/ && sudo systemctl daemon-reload
sudo systemctl enable --now nmg-relay
sudo tailscale funnel --bg --set-path /nmg http://127.0.0.1:8796   # or the Caddy branch
journalctl -u nmg-relay -n 20
```
Unit: `User=server`, `WorkingDirectory=/home/server/nmg-relay`, `ExecStart=/usr/bin/node server.js`, `Environment=PORT=8796`, `Restart=always`, `RestartSec=2`, hardening as above. `/healthz` returns `{n: clients}` (loopback only) for the dashboard later. Doc the service in `memory/projects/SMARTGOR/` once live. Update after deploy: Pi == repo.

## Test plan (two browser windows, localhost)

1. Run the relay on the Mac: `cd server/nmg-relay && npm i && PORT=8796 node server.js`. Client dev override: `localStorage['nmg-relay']='ws://localhost:8796'` (ship-net reads it before the Funnel URL; only honored on localhost origins).
2. Single player: relay stopped, load site: game normal, one console.info, socket gives up in 2 s, no retries before 60 s, no HUD change.
3. Start relay, reload window A: no ghost, "ONLINE" pip hidden (n = 1).
4. Open window B (separate profile / incognito so ids differ): A shows a docked ghost on the black-hole orbit in galaxy view within 1 s; B shows A's.
5. A enters ship, flies: B (galaxy view and in ship mode) sees the ghost move smoothly, no stutter at 10 Hz; stop A mid-flight: ghost holds then docks on `st`.
5b. Shoot B's ghost from A: B takes damage, its HP pip drops on A's screen, B dies -> A gets a KILL announce and +1 kill; B respawns at dock. Verify planet sync: park both next to the same planet, it is in the same spot in both windows (compare after 60 s).
6. Set `color` via `/color` on A: B re-tints without reload. `/name` same.
7. Close B: A's ghost disappears immediately (bye); kill B's tab hard (devtools offline): gone within 15 s.
8. Kill relay while both fly: both keep playing, ghosts fade in 3 s, reconnect only at 60 s.
9. Abuse: from the console send 500 frames/s, oversized frame, bad JSON, wrong Origin (`wscat -H 'Origin: https://evil.com'`): rejected/closed, relay stays up. Open 33 sockets: 33rd refused.
10. Same `id` in two tabs: older tab drops to single-player, no duplicate ghost.
11. Perf: 8 scripted bot sockets (small Node script, 10 Hz sine paths) while flying: frame time unchanged within noise.
12. Production: after deploy, repeat 4-5 from emgor.online and a phone-sized window (must NOT load net on phones); confirm gorcave-chat/dash/lights still work.

## Fallback (footnote)

If the Pi relay ever proves flaky or Funnel path routing can't carry WebSockets: same protocol, same `ship-net.js`, swap the URL to a Cloudflare Durable Object / PartyKit room (free tier). Only the URL and the server file change.
