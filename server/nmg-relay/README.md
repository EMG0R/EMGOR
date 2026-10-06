# nmg-relay (NO MANS GOR multiplayer relay)

Dumb WebSocket fan-out + presence for `js/ship-net.js`. Node + `ws`, listens on `127.0.0.1:8796`,
no persistence, no content logging. Design and protocol: `docs/multiplayer-plan.md`.
Netlify is unchanged; the site only needs the client files (`js/ship-net.js` + the hooks in `js/ship.js`, `js/galaxy3d.js`).

## Run locally (Mac)

    cd server/nmg-relay && npm install && node server.js        # port 8796
    # pages served from localhost / 127.0.0.1 automatically use ws://127.0.0.1:8796
    # health: curl http://127.0.0.1:8797/healthz   ->  {"n":<clients>}

Env: `PORT` (8796), `HOST` (127.0.0.1), `ALLOW_NO_ORIGIN=1` (test bots without an Origin header only).

## Deploy to gorcave (run these yourself, in order; do not skip the check in step 1)

1. On the Pi, look at how the existing paths (/api/chat, /api/dash, ...) are routed. Read-only:

       ssh server@gorcave.lan
       tailscale funnel status
       ss -ltnp | grep -E '8787|8790|8795|8796'      # 8797 must be free (8796 is taken by the /m sync service)
       node -v                                        # needs >= 18; install nodejs if missing

2. From the Mac, copy the relay (skip node_modules, the Pi installs its own):

       cd ~/Documents/_EMGOR_SYNTH/____EMGOR_ONLINE/EMGOR
       rsync -av --exclude node_modules server/nmg-relay/ server@gorcave.lan:~/nmg-relay/

3. On the Pi, install and start the service:

       cd ~/nmg-relay && npm ci --omit=dev
       sudo cp nmg-relay.service /etc/systemd/system/
       sudo systemctl daemon-reload
       sudo systemctl enable --now nmg-relay
       systemctl status nmg-relay --no-pager
       curl -s http://127.0.0.1:8797/healthz           # {"n":0}

4. Expose it as a path on 443. Additive only, do not touch the 8787/8790/8795 mappings. Pick ONE branch
   matching what step 1 showed:

   A. Paths are tailscale entries (most likely):

          sudo tailscale funnel --bg --set-path /nmg http://127.0.0.1:8797
          tailscale funnel status                      # /nmg must appear beside /api/...

      (serve strips the /nmg prefix; the relay ignores the path anyway.)

   B. Paths come from Caddy: add `handle /nmg* { reverse_proxy 127.0.0.1:8796 }` and reload Caddy.
      nginx: `location /nmg { proxy_pass http://127.0.0.1:8797; proxy_http_version 1.1;
      proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection "upgrade"; proxy_read_timeout 120s; }`

5. Verify:

       journalctl -u nmg-relay -n 20 --no-pager
       # from the Mac (needs `npm i -g wscat` or any ws client):
       wscat -c wss://gorcave.taila85593.ts.net/nmg -H 'Origin: https://emgor.online'   # connects, send {"t":"hi","id":"abcdef123456"}
       wscat -c wss://gorcave.taila85593.ts.net/nmg -H 'Origin: https://evil.com'       # must be refused (403)
       # open gorcave-chat / gorcave-dash / gorcave-lights: they must still load.

6. Ship the site as usual (git push -> Netlify). Open emgor.online in two desktop windows: each shows the
   other's docked ghost. Phones never load ship mode, so they never connect.

Update after deploy: Pi == repo, and note the service in memory/projects/SMARTGOR/.

## Update the relay later

    rsync -av --exclude node_modules server/nmg-relay/ server@gorcave.lan:~/nmg-relay/
    ssh server@gorcave.lan 'cd ~/nmg-relay && npm ci --omit=dev && sudo systemctl restart nmg-relay'

A restart empties the room and shifts the shared planet clock (clients reconnect and ease to the new phase).

## Limits (server.js)

32 clients (33rd closed 4001), 4 sockets per IP (4003), 512 B frames, 30 msg/s bucket burst 60 (sustained
over 5 s closes 4002), 12 fire/s, 5 bad frames closes 4004, 15 s silence drops, origin allow-list
(https://emgor.online, https://www.emgor.online, http://localhost:*, http://127.0.0.1:*), same id twice: older
socket closed 4000. Unit has Nice=10, CPUQuota=25%, MemoryMax=64M so the lights never stutter.

Stop / remove: `sudo systemctl disable --now nmg-relay`; the game falls back to single player by itself.

## Nerfed gorcave (`/gorcave`, gor.js)

A sandboxed lobby neptr. `{t:'gor', text}` from a client -> one `claude -p` run -> `{t:'gor', from:'neptr', text, re:<askerName>}`
broadcast to the room. It never touches the real gorcave agent. Plain chat: `{t:'chat', text}` (<= 200 chars) -> `{t:'chat', from, text}`.

No API key. It uses the Pi's own Claude Code login (the subscription, same as the gorcave chat): `claude` must be
logged in as the `server` user (`claude auth login`). Command: `claude -p --model sonnet --system-prompt <persona file>
--tools "" --strict-mcp-config --mcp-config '{"mcpServers":{}}' --disable-slash-commands --no-session-persistence
--output-format text -- <history + message>`; stdin closed, env stripped to PATH/HOME/LANG (no ANTHROPIC_API_KEY),
cwd `~/nmg-relay/sandbox` (created empty), killed at 25 s. Last 8 exchanges are prepended to the prompt.

Limits: 400-char input, 1 request / 5 s per client, 60 / hour global, 25 s timeout. Any failure (not logged in,
exit error, timeout, rate limit) replies to the asker only: `{t:'gor', from:'neptr', text:'...neptr is thinking too hard. try again.'}`.

Persona: env `NMG_GOR_PERSONA`, default `~/EMGOR_SKILLS/_NERFED_GORCAVE.md` (the repo is on the Pi too).
Loaded once per process; restart the service after editing it. Model override: `NMG_GOR_MODEL`; binary: `CLAUDE_BIN`.
The unit sets `Environment=PATH=...` so systemd can find `/usr/local/bin/claude`.

## Persistence + shared state (store.js)

JSON files in `NMG_DATA` (default `~/nmg-relay/data/`): `profiles.json`, `discoveries.json`, `events.json`. Loaded at boot,
written atomically (tmp + rename) debounced 2 s, flushed on SIGTERM, refused above 2 MB per file. Back up this dir.
Env: `NMG_DATA`, `NMG_EVENT_EVERY_MS` (fixed event interval, tests only). Frames are 768 B max except `prof.save` (64 KB data).
All messages below need `hi` first and count against the normal bucket / strike limits.

| Client sends | Reply |
|---|---|
| `{t:'prof.save', name, key, data}` name `[a-z0-9_-]{2,20}` (lowercased), key 4-64 chars, data = JSON object <= 64 KB stored verbatim (keep `v`). First save registers sha256(key); later saves must match. | `{t:'prof.ok', name}` or `{t:'prof.err', why:'key'\|'bad'\|'big'\|'rate'\|'full'}` |
| `{t:'prof.load', name, key}` | `{t:'prof', name, data}` or `{t:'prof.err', why:'key'\|'none'\|'bad'\|'rate'}`. Save/load share one limit: 1 per 10 s per client. |
| `{t:'disc.claim', kind:'planet'\|'creature'\|'resource', id, name, by}` id `[A-Za-z0-9_.:-]{1,48}`, name <= 24 | First claim wins: `{t:'disc', kind, id, name, by, at}` broadcast to the room. Already claimed: `{t:'disc.no', kind, id, name, by}` to the asker only (`why:'full'` at 2000 entries). |
| `{t:'disc.list'}` (1 per 2 s) | `{t:'disc.all', list:[{kind,id,name,by,at}...]}` |
| `{t:'event.now'}` | `{t:'event', kind, minutes, left, planetId?}` (`left` in seconds) or `{t:'event.none'}` |
| (server push) every 20-40 min, seeded from the relay epoch + sequence | `{t:'event', kind:'titan'\|'meteor'\|'friesSale'\|'blockade', minutes, left, planetId?}` broadcast to all; `planetId` (0-999) only for meteor/blockade, the client maps it onto a planet. The current event + next fire time persist across restarts. |

## Fight rooms, planet channels, style (rev 30)
Room traffic (`sm.in`, `sm.snap`, `pl.snap`, `pl.ev`) uses its own bucket (150/s, burst 150) and must have `t` as the FIRST key (it is detected from the first 48 bytes, like `prof.save`). Frame cap 768 B, except `sm.snap` 2 KB and `pl.snap` 4 KB of `data`. Bad frames count as strikes.

| Client sends | Relay does |
|---|---|
| `hi` / `{t:'hi', id, name, color, hs, style, pet, crew}` | `style` <= 16 printable chars, `pet`/`crew` ints 0-99. All three appear on each ship in `roster`. |
| `pos` | `st` now `& 31`: bit 16 = in a fight. |
| `{t:'sm.challenge', to}` (2/s) | To `to`: `{t:'sm.challenge', from, name}` (valid 30 s). Unknown `to`: `{t:'sm.no', why:'gone'}`. |
| `{t:'sm.accept', from}` | Needs a live challenge from `from`. Creates `sm_<8hex>`, host = challenger. Both get `{t:'sm.start', room, host, members:[ids], spec:[ids], seed}`. Else `sm.no` (`expired`/`busy`). |
| `{t:'sm.join', room, fight?}` | Joiner gets `sm.start`; others get `{t:'sm.member', room, id, name, role:'fighter'\|'spec'}`. Cap 4 fighters (`fight:true`, else falls to spectator) + 8 spectators; `sm.no why:'full'\|'none'`. |
| `{t:'sm.leave'}` | Member leaves: `{t:'sm.member', room, id, role:'left', was}`. Host leaving ends the room. |
| `{t:'sm.in', room, f, m}` (guest fighter, <= 60/s) | Forwarded to the HOST only as `{t:'sm.in', room, id, f, m}`. |
| `{t:'sm.snap', room, f, data}` (host, <= 30/s, data <= 2 KB) | `{t:'sm.snap', room, f, data}` to every other member and spectator. |
| `{t:'sm.end', room, result}` (host, result <= 512 B) | `{t:'sm.end', room, result}` to all; room closed. Host disconnect/leave: same with `result:null, reason:'host'`. |
| `{t:'pl.enter', planet}` (int or `[A-Za-z0-9_.:-]{1,24}`; 4/s) | Leaves any previous planet. Host = lowest id (string sort) on the planet. Entrant gets `{t:'pl.host', planet, host}`; all members get it whenever the host changes. |
| `{t:'pl.leave'}` (also on disconnect) | Re-elects; members get `pl.host` if it changed. |
| `{t:'pl.snap', planet, data}` (host only, <= 10/s, data <= 4 KB) | `{t:'pl.snap', planet, data}` to the other members of that planet only. |
| `{t:'pl.ev', planet, data}` (any member, <= 5/s, data <= 512 B) | `{t:'pl.ev', planet, id, data}` to the other members (host included). |

Over-rate frames of these types are dropped quietly. Nothing here is persisted.
