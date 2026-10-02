# nmg-relay (NO MANS GOR multiplayer relay)

Dumb WebSocket fan-out + presence for `js/ship-net.js`. Node + `ws`, listens on `127.0.0.1:8796`,
no persistence, no content logging. Design and protocol: `docs/multiplayer-plan.md`.
Netlify is unchanged; the site only needs the client files (`js/ship-net.js` + the hooks in `js/ship.js`, `js/galaxy3d.js`).

## Run locally (Mac)

    cd server/nmg-relay && npm install && node server.js        # port 8796
    # pages served from localhost / 127.0.0.1 automatically use ws://127.0.0.1:8796
    # health: curl http://127.0.0.1:8796/healthz   ->  {"n":<clients>}

Env: `PORT` (8796), `HOST` (127.0.0.1), `ALLOW_NO_ORIGIN=1` (test bots without an Origin header only).

## Deploy to gorcave (run these yourself, in order; do not skip the check in step 1)

1. On the Pi, look at how the existing paths (/api/chat, /api/dash, ...) are routed. Read-only:

       ssh server@gorcave.lan
       tailscale funnel status
       ss -ltnp | grep -E '8787|8790|8795|8796'      # 8796 must be free
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
       curl -s http://127.0.0.1:8796/healthz           # {"n":0}

4. Expose it as a path on 443. Additive only, do not touch the 8787/8790/8795 mappings. Pick ONE branch
   matching what step 1 showed:

   A. Paths are tailscale entries (most likely):

          sudo tailscale funnel --bg --set-path /nmg http://127.0.0.1:8796
          tailscale funnel status                      # /nmg must appear beside /api/...

      (serve strips the /nmg prefix; the relay ignores the path anyway.)

   B. Paths come from Caddy: add `handle /nmg* { reverse_proxy 127.0.0.1:8796 }` and reload Caddy.
      nginx: `location /nmg { proxy_pass http://127.0.0.1:8796; proxy_http_version 1.1;
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

A sandboxed lobby neptr. `{t:'gor', text}` from a client -> one Anthropic Messages API call (no tools,
system prompt = the persona file only, `claude-haiku-4-5-20251001`, 300 max tokens, last 8 exchanges kept in
memory) -> `{t:'gor', from:'neptr', text, re:<askerName>}` broadcast to the room. It never touches the real
gorcave agent. Plain chat: `{t:'chat', text}` (<= 200 chars) -> `{t:'chat', from, text}` to everyone.

Limits: 400-char input, 1 request / 5 s per client, 60 / hour global, 20 s timeout. Any failure (no key,
HTTP error, timeout, rate limit) replies to the asker only: `{t:'gor', from:'neptr', text:'...neptr is thinking too hard. try again.'}`.

Persona: env `NMG_GOR_PERSONA`, default `~/EMGOR_SKILLS/_NERFED_GORCAVE.md` (the repo is on the Pi too).
Loaded once per process; restart the service after editing it. Model override: `NMG_GOR_MODEL`.

API key (never in the repo): create the env file by hand on the Pi. The unit reads it via
`EnvironmentFile=-/home/server/nmg-relay/.env`.

    ssh server@gorcave.lan
    umask 077 && nano ~/nmg-relay/.env        # one line: ANTHROPIC_API_KEY=sk-ant-...
    chmod 600 ~/nmg-relay/.env
    sudo cp ~/nmg-relay/nmg-relay.service /etc/systemd/system/ && sudo systemctl daemon-reload
    sudo systemctl restart nmg-relay

(rsync does not overwrite `.env`; the Mac copy has none.) Without a key the relay runs and `/gorcave` just
answers with the failure line.

Test locally (`ANTHROPIC_API_KEY` optional; without it you should get the failure line):

    ALLOW_NO_ORIGIN=1 node server.js &
    wscat -c ws://127.0.0.1:8796
    > {"t":"hi","id":"abcdef123456","name":"tester"}
    > {"t":"gor","text":"who are you?"}
    < {"t":"gor","from":"neptr","text":"...","re":"tester"}
    > {"t":"chat","text":"hello room"}
