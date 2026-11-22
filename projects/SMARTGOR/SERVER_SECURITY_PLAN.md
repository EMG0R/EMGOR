# SERVER Security Plan — Public Chat Bridge to Server

Status: design doc, not yet implemented. No code in this repo yet.

## What this is

emgor.online (static site, Netlify) will eventually have a "server" planet
where a visitor can open a chat widget and talk to "Server" —
an AI agent process running on a Raspberry Pi at home. The Pi also drives
real smart-home hardware (lighting, plants, network gear via the
`SMART_ROOM_MASTER_PLAN.md` / `ROUTER_RUNBOOK.md` stack), so a compromise of
this chat path isn't just "someone reads a chatbot" — it's a path toward
someone flipping relays or touching the router. The design below treats the
network layer as the part that actually has to be sound, and treats
everything above it as "good enough for a hobby project."

Threat model, stated explicitly:
- Attacker is opportunistic (scanners, script kiddies, maybe a nosy visitor
  who finds the page), not a targeted, resourced adversary.
- The `/server` page is currently `draft: true` — not linked, not indexed.
  Discovery requires already knowing the URL. This buys time, not security,
  and is not relied upon as a control — but it does shrink the realistic
  attacker pool during rollout.
- We are not storing or processing other people's personal data. Worst case
  of a breach is "a stranger can chat with my house" — annoying and
  potentially unsafe (device control), not a privacy/compliance incident.
- Goal: reasonably safe against automated abuse and casual poking, without
  enterprise security theater.

---

## 1. Network exposure — no port forwarding, Tailscale tailnet only

The Pi has zero open inbound ports on the home router. It is joined to a
private Tailscale tailnet (WireGuard-based mesh VPN). Concretely:

- The Pi runs `tailscaled` and has a stable tailnet IP (100.x.y.z) and
  MagicDNS name (e.g. `server-pi.<tailnet>.ts.net`).
- The home router has no port forwards, no DMZ entry, no UPnP-punched holes
  for the Pi. From the raw public internet, the Pi's WAN-facing footprint is
  identical to a device that doesn't exist.
- All other Pi services (FastAPI dashboard, MQTT broker, websocket control
  channel, SSH) stay bound to the tailnet interface / localhost only. They
  are reachable from other devices on the tailnet (phone, laptop) but never
  from the public internet.
- This means the *only* thing that needs a public-internet story at all is
  the one chat endpoint — everything else (lighting control, MQTT, plant
  sensors, router admin) simply has no route in from outside, full stop.
  This is the actual security boundary; everything in sections 3–4 is
  defense-in-depth on top of it, not the primary control.

---

## 2. Bridging the public static site to the private Pi: Tailscale Funnel

### The problem
emgor.online is static and hosted on Netlify's public CDN — it has no
knowledge of the tailnet and can't reach `100.x.y.z` directly (nor should
it; Netlify's build/serve infra isn't a tailnet member and doesn't need to
be). The browser running the chat widget is a random visitor's machine,
also not on the tailnet. So the actual bridge problem is: how does an
anonymous browser, over the open internet, reach one HTTP endpoint on a Pi
that is otherwise dark to the internet?

### Recommended answer: Tailscale Funnel

Funnel is a feature built on top of Tailscale Serve. Serve exposes a local
port to *other tailnet devices* over HTTPS using Tailscale's automatic
TLS certs (via Let's Encrypt, issued for your tailnet's `*.ts.net` domain).
Funnel is the extra flag that additionally exposes that same Serve
configuration to the *public internet*, by routing traffic through
Tailscale's own relay/ingress infrastructure rather than opening any port
on the Pi's router.

How it actually works, mechanically:

1. You run something like:
   ```
   tailscale funnel --bg 8443
   ```
   (exact invocation detailed in the checklist below) pointed at the local
   port your FastAPI chat endpoint listens on.
2. Tailscale provisions a TLS cert for `server-pi.<tailnet>.ts.net` and
   registers a public DNS + ingress route on Tailscale's own edge nodes.
3. Public traffic to `https://server-pi.<tailnet>.ts.net/...` hits
   Tailscale's edge infrastructure first — **not** your home IP. Tailscale
   terminates TLS there and forwards the request into the tailnet, over the
   existing WireGuard mesh, to your Pi.
4. Your home router still sees *outbound* connections only (the Pi dialing
   out to Tailscale's coordination/relay servers, same as any normal
   Tailscale client traffic). No inbound port is opened on your router or
   your ISP-facing IP at any point. Your home WAN IP is never directly
   contacted by the public visitor.

Security properties this gives you for free:
- **No exposed IP.** The attacker's traffic terminates at Tailscale's edge,
  not your house. Your home IP is not learnable from probing the chat
  endpoint. This kills IP-based reconnaissance and most opportunistic
  scanning entirely — internet-wide scanners never even see a Pi exists.
- **TLS handled for you.** Cert issuance/rotation is Tailscale's problem,
  not something to hand-roll on a Pi (no self-signed cert warnings, no
  certbot cron job to babysit).
- **Scoped by port/service, not by exposing the whole box.** Funnel is
  configured per local port. If the FastAPI chat API listens on, say,
  `127.0.0.1:8787` and you `tailscale serve` + `funnel` only that port, then
  only requests to that specific path/port ever leave the tailnet boundary.
  Everything else on the Pi (SSH on 22, the dashboard on another port, MQTT
  on 1883, the websocket control channel) stays tailnet-only — Funnel
  doesn't implicitly open them. You can further scope by path with
  `tailscale serve --set-path` style rules so only `/chat` (or whatever the
  API path is) is funneled, not a whole service root, if the chat API
  shares a port with anything else.
- **Kill switch is one command.** `tailscale funnel off` (or `tailscale
  funnel --bg=false`) immediately un-publishes the endpoint — useful if
  abuse is detected, no router/firewall reconfig needed.
- **No forwarded router ports = no router-level attack surface.** The
  router's firewall, UPnP, and NAT config are entirely untouched. A bug in
  Funnel's own infra is a Tailscale-side incident, not something that also
  hands an attacker a route to the router admin UI or other LAN devices —
  compare this to classic port-forwarding, where a bug in *your* exposed
  service is immediately adjacent to your whole LAN.

Caveats worth knowing:
- Funnel requires the tailnet's HTTPS certs feature enabled and (depending
  on plan) may require the tailnet admin to allow Funnel via ACL/policy
  file (`nodeAttrs` with `funnel` attribute). One-time setup, not per-request
  overhead.
- Funnel is still a public HTTP endpoint once turned on — Tailscale's
  identity/authentication does NOT apply to funneled traffic (Funnel
  traffic is explicitly for *anonymous* public visitors, unlike normal
  tailnet ACLs which are identity-aware). This is exactly why section 3
  (app-layer auth) is still required — Funnel solves *network reachability*
  scoping, not *who's allowed to talk to the chat API*.
- Traffic amount/rate through Funnel is subject to Tailscale's fair-use
  limits; irrelevant at hobby-chat-widget scale.

### Alternative considered: small cloud relay / reverse proxy

The other standard pattern is a cheap always-on VPS (or a Cloudflare
Tunnel-style service) that sits on the tailnet as a relay: public traffic
hits the VPS, the VPS forwards over the tailnet (or its own private tunnel)
to the Pi, VPS does TLS termination and maybe rate limiting/WAF.

This is a legitimate pattern and is what you'd reach for if you needed
multiple exposed services, custom edge logic (e.g. edge-side caching, geo
rules), or independence from Tailscale's own infra. For a single Pi
exposing a single chat endpoint, though, it's strictly more moving parts
for no real benefit here: it's another machine to patch and pay for, another
TLS cert to manage, another hop that can go down independently, and it
still needs a private link back to the Pi (which would itself probably just
be... Tailscale). Funnel gives the same "public HTTPS endpoint, private
backend" shape with zero extra infrastructure to run. **Recommendation:
Funnel, not a relay VPS**, unless/until this grows past "one chat
endpoint."

---

## 3. Application-layer auth: server-side shared secret, checked on the Pi

Funnel makes the endpoint *reachable*; it does not make it *authenticated*.
The Pi's FastAPI chat handler must independently verify every request,
because anyone who finds the URL can otherwise call it directly with curl —
Funnel does not care, and neither does a JS-only frontend gate (trivially
bypassed by opening devtools, reading the "secret" out of bundled JS, or
just watching the network tab and replaying the request).

Design:
- A single shared secret (a long random token, not a memorable password —
  e.g. 32+ bytes from `secrets.token_urlsafe(32)`) is generated once and
  lives only on the Pi, server-side.
- The frontend chat widget prompts the visitor for the passphrase/token on
  first use, then sends it as a **Bearer token** in the `Authorization`
  header on every request to the chat API:
  ```
  Authorization: Bearer <token>
  ```
  Storing it client-side (e.g. `localStorage`) after first entry is fine for
  this threat model so the visitor isn't re-prompted every message — this is
  not a multi-user system with per-user accounts, it's a single shared
  household secret gating a single endpoint.
- **Never** as a URL query parameter (`?token=...`). Query strings end up in
  server access logs, browser history, Referer headers if the page links
  out anywhere, and any CDN/proxy logs in between (including Tailscale's own
  Funnel edge logs). Header-only, always.
- Server side, the FastAPI dependency does the check before any request
  reaches the actual agent logic:
  ```python
  import hmac, os
  from fastapi import Header, HTTPException, Depends

  SHARED_SECRET = os.environ["SERVER_SHARED_SECRET"]

  def require_auth(authorization: str = Header(default="")):
      scheme, _, token = authorization.partition(" ")
      if scheme.lower() != "bearer" or not hmac.compare_digest(token, SHARED_SECRET):
          raise HTTPException(status_code=401, detail="unauthorized")
  ```
  The key detail: `hmac.compare_digest` (constant-time comparison) instead
  of `token == SHARED_SECRET`, so response timing doesn't leak how many
  leading characters matched — cheap to do, standard practice, no reason to
  skip it even at hobby scale.
- A short-lived session token (issue a JWT/opaque token after password entry,
  expire it in N hours) is a reasonable *optional* refinement but is not
  required for this threat model — sending the shared secret directly as
  the bearer token on every request is acceptable here because: it's already
  header-only over TLS (Funnel terminates HTTPS), there's no multi-tenant
  user base to isolate, and rotating one shared secret if it ever leaks is
  trivial (see section 5). Don't build session infrastructure for this.

---

## 4. Rate limiting / lockout: stop brute-forcing the token

Because the token check is a single string comparison behind a public
endpoint, the remaining risk is an attacker hammering the endpoint trying
random/guessed tokens. Mitigate with a lightweight lockout, not a
full-blown WAF:

- Track failed-auth attempts **per client IP** (available from the request;
  Funnel/Tailscale preserves or provides forwarding info — confirm the real
  client IP arrives via `X-Forwarded-For` from Funnel's edge, and trust that
  header only from Funnel's known ingress since it's the sole path in).
- Simple counter, in-memory dict is fine for a single-process Pi backend
  that doesn't need to survive restarts perfectly:
  ```python
  from collections import defaultdict
  from time import time

  _failures: dict[str, list[float]] = defaultdict(list)
  MAX_ATTEMPTS = 5
  WINDOW_SECONDS = 300     # 5 min window
  LOCKOUT_SECONDS = 900    # 15 min lockout once tripped

  def check_rate_limit(ip: str):
      now = time()
      _failures[ip] = [t for t in _failures[ip] if now - t < WINDOW_SECONDS]
      if len(_failures[ip]) >= MAX_ATTEMPTS:
          raise HTTPException(status_code=429, detail="too many attempts, try later")

  def record_failure(ip: str):
      _failures[ip].append(time())
  ```
  On success, clear that IP's entry. This is a `slowapi`-style
  (`slowapi`/`fastapi-limiter` are real off-the-shelf options if you'd
  rather not hand-roll it) approach — either is fine; hand-rolled is fewer
  dependencies for a single-endpoint hobby project.
- In-memory is acceptable because: the FastAPI process is long-running on
  the Pi (not serverless/ephemeral), a restart clearing counters is a
  non-issue at this scale, and there's no multi-instance/load-balanced
  deployment to keep state in sync across. If that ever changes, swap the
  dict for a small SQLite file or Redis — not needed now.
- General endpoint rate limit (e.g. N requests/minute regardless of
  auth outcome) is also worth adding purely to keep the Pi's own resources
  (and the AI backend's token/API costs, if it calls out to a hosted LLM)
  from being hammered — separate concern from the brute-force lockout above
  but cheap to add in the same middleware.

---

## 5. What NOT to over-build

Explicitly out of scope, and why, given the stated threat model:

- **OAuth / full identity provider.** There is one household, one shared
  secret, no per-user accounts to manage. OAuth solves a problem (many
  distinct authenticated identities, delegated third-party access) this
  project doesn't have.
- **WAF / managed edge security product.** Tailscale Funnel's edge already
  removes the home IP from the attack surface; the app-layer token +
  lockout handles the remaining realistic threat (credential guessing).
  A WAF is solving for a much larger, more targeted attacker profile.
- **Professional pentesting.** Disproportionate for a hobby project with no
  other people's data at stake. A basic self-review (confirm Funnel scope,
  confirm token isn't logged, confirm lockout works) is enough.
- **Complex session management (refresh tokens, revocation lists, per-device
  sessions, etc).** One shared secret, rotate-by-hand if it ever leaks
  (regenerate the token, update the `.env`, restart the service — a two
  minute fix). Building infrastructure to avoid a two-minute manual fix
  isn't worth it here.
- **Rotating secrets on a schedule / secret managers (Vault, etc).** A
  `.env` file with correct permissions on a single-user Pi is proportionate.
  Revisit only if this ever stops being a single-household hobby project.

The draft/unlisted status of the `/server` page is *not* listed as a
control above and shouldn't be relied on once this goes live — it's
currently just incidental cover during development. Once the page is
published (`draft: false`), assume the URL and endpoint will be found by
scanners within days, and confirm sections 3–4 are actually in place before
that flip.

---

## 6. Implementation checklist (for whoever builds this next)

Ordered, so it can be picked up and executed without re-deriving the design:

1. **Confirm Tailscale is already set up on the Pi** (it should be, given
   the existing Server/dashboard stack) — `tailscale status` shows the Pi
   with a stable tailnet IP / MagicDNS name.
2. **Build the FastAPI chat endpoint on the Pi first, tailnet-only,** bound
   to `127.0.0.1` or the tailnet interface, on its own port (don't share a
   port with the existing dashboard/MQTT/websocket services — keep it
   independently scopable for Funnel). Verify it works from another tailnet
   device before touching Funnel at all.
3. **Generate the shared secret** on the Pi:
   ```
   python3 -c "import secrets; print(secrets.token_urlsafe(32))"
   ```
   Store it in a `.env` file (or systemd `EnvironmentFile=`) that the
   FastAPI process loads at startup, e.g. `/home/server/Server/.env` with
   `SERVER_SHARED_SECRET=...`. **Never hardcode it in source, never commit
   it.** Confirm the `.env` file's permissions are user-read-only
   (`chmod 600`) and that it's covered by `.gitignore` if this directory is
   ever git-tracked.
4. **Implement the auth dependency** (constant-time bearer check, section 3)
   and the **rate-limit/lockout middleware** (section 4) on that endpoint
   before it's ever exposed publicly. Test both locally over the tailnet
   with curl (valid token, invalid token, 6 rapid failures → 429).
5. **Enable Tailscale HTTPS certs** on the tailnet (admin console → DNS →
   enable HTTPS certificates) if not already on.
6. **Serve, then Funnel, scoped to just that port/path:**
   ```
   tailscale serve --bg --https=443 http://127.0.0.1:8787
   tailscale funnel --bg 443
   ```
   (adjust local port to match the actual chat API port; confirm with
   `tailscale funnel status` that only the intended path is published, and
   that other local services are not reachable through the same Serve
   config.)
7. **Test from an actual outside network** (phone on cellular data, not
   home wifi) hitting `https://server-pi.<tailnet>.ts.net/<chat-path>`:
   confirm 401 without a token, confirm success with the correct token,
   confirm lockout triggers on repeated bad tokens.
8. **Build the frontend widget** in the `server` planet:
   repo `/Users/emgor/Documents/_EMGOR_SYNTH/____EMGOR_ONLINE/EMGOR`,
   content at `universe/server/_index.md` (currently `draft: true` — leave
   it draft until steps 1–7 are verified end-to-end). Widget prompts for the
   passphrase once, stores it (e.g. `localStorage`), sends it as
   `Authorization: Bearer <token>` on every chat request to the Funnel URL,
   surfaces a clear error state on 401/429 rather than failing silently.
9. **Only then flip `draft: false`** and deploy the Netlify site. Re-verify
   the endpoint from a cold browser (no localStorage) immediately after
   publishing.
10. **Note the kill switch** somewhere obvious for future-you:
    `tailscale funnel off` on the Pi immediately un-publishes the endpoint
    if anything looks wrong post-launch — no router/DNS changes needed, no
    Netlify redeploy needed, takes effect in seconds.
