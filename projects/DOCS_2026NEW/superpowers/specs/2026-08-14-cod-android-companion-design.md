# Cod — Android companion ("Cod Remote")

Date: 2026-08-14
Author: drafted by agent, for emgor

> ## STATUS: DRAFT — NOT APPROVED
>
> **Proposed, not yet reviewed or approved by the human. No implementation
> should start from this document without a follow-up conversation.** Nothing
> here is a decision; every recommendation below is a proposal with its
> reasoning shown so it can be argued with. Several items in §8 are explicitly
> the human's call and are deliberately left unanswered.

---

## 1. Summary

Cod is a macOS Tauri app that runs Claude Code sessions in freeform panes. This
document plans **Cod Remote**: a native Android app that connects to a *running*
Cod instance on the Mac and lets the human keep working while away from the
computer — read what each session is doing, reply to it, answer permission
prompts, and see usage.

The framing that shapes every decision here (human's words): *"so i can keep
working remotely w my computer running."* The Mac stays on. The phone is a
**thin remote client**, not an independent Claude host. Nothing runs on the
phone except the UI.

Recommended shape, stated up front:

- **Cod's own Rust backend grows a Remote Server** (HTTP + WebSocket) alongside
  the existing approval broker. No new Mac-side companion process; the Swift
  watch app is not extended.
- **Reachability comes from Tailscale** (WireGuard mesh), not from a cloud relay
  and not from port-forwarding. Cod binds to loopback + the Tailscale interface
  only.
- **Auth is a per-device bearer token** issued by a QR pairing flow in Cod's UI,
  revocable, with a biometric gate on the phone for anything that writes.
- **Native Kotlin/Compose app**, matching `the_watch/companion-app` exactly — same
  language, same toolchain, same `com.emgor.*` namespace, one Android skill set
  to maintain.
- **The phone's primary view is a transcript-derived conversation view**, not a
  shrunken terminal — with a raw-terminal tab available when needed.

---

## 2. Goals

- See, from the phone, what every Cod pane is currently doing: live output, and
  whether it is working / waiting on you / finished.
- Reply to a running Claude session from the phone (send a prompt, send control
  keys: Enter, Esc, Ctrl-C, arrows, numbered menu choices).
- Answer permission prompts from the phone, as a **third surface racing the
  watch and the desktop** — same first-response-wins event flow, not a parallel
  mechanism.
- Get notified when a pane needs attention (approval pending, question asked,
  long task finished) without keeping the app open.
- See the usage dashboard (today's tokens/cost, per-session context %) remotely.
- Work from anywhere with internet, not only on the home LAN.
- **Fail-safe, same as everything else in this ecosystem:** if the phone is
  offline, unpaired, or the remote server is off, Cod behaves exactly as it does
  today. The remote path is strictly additive and never a dependency.
- Every remote action is auditable after the fact.

## 3. Non-goals (v1)

- **Running Claude Code on the phone.** No on-device agent, no standalone mode.
  Mac off = app shows "disconnected" and that is the correct behavior.
- **Reproducing the canvas.** The freeform draggable pane layout is a desktop
  affordance. The phone gets a list/switcher, not a miniature canvas.
- **Exposing Cod to the public internet.** No port forwarding, no `0.0.0.0`
  bind, no cloud relay, no inbound-from-anywhere story. See §5.3.
- **iOS.** Android only (matches the existing companion app).
- **Multi-user / multi-Mac.** One human, one Mac, N of their own devices.
- **Editing files or browsing the filesystem** from the phone (later phase idea,
  S12 / Phase R6).
- **Replacing the watch.** The watch stays exactly as it is. The phone is an
  additional surface on the same approval event stream.

---

## 4. Scope breakdown — what "full access and control" concretely means

Ranked by (value on a phone) ÷ (cost + risk). The v1 line is drawn after **S5**.

| # | Capability | Verdict | Why |
|---|---|---|---|
| S1 | **List panes + status** (cwd, model, running/waiting/done, last activity) | **v1** | The single highest-value screen. Answers "what's my machine doing" in one glance. Cheap: the data already exists in `AppState.sessions` + the usage tailer. |
| S2 | **Read a pane live** (streaming output, plus recent scrollback on attach) | **v1** | Without this the app is a status light. Requires the pane-output refactor in §6.1 — the biggest single piece of backend work in this plan. |
| S3 | **Usage view** (today's rollup + per-pane tokens/context %) | **v1** | Nearly free: `usage::read_stats` already computes it; expose over the same API. Genuinely useful to check from a phone. |
| S4 | **Send input to a pane** (prompt text + control keys) | **v1** | This is what makes it "keep working," not "keep watching." `write_to_pane` already exists. The security surface is real — §5.3, §6.4. |
| S5 | **Approve/deny permission prompts** | **v1** | The killer feature and the one with existing prior art. **Depends on a prerequisite** that is not built yet — see §6.5. |
| S6 | **Push notification on attention-needed** | **v1 (local-notification form)** | Needed for S5 to be useful — an approval you don't know about is useless. v1 delivers it from a foreground-service socket, not FCM. §6.6. |
| S7 | **Create a new pane** (pick a cwd from recents, start a session) | later | Useful ("start a build while I'm out") but needs a directory picker and widens the blast radius. Phase R5. |
| S8 | **Close / rename / reorder panes** | later | Convenience, not capability. Phase R5. |
| S9 | **Full canvas layout control** (drag/resize/z-order from the phone) | dropped | Explicit non-goal. Bad ergonomics, no value. |
| S10 | **Advisor feed** (once Cod Phase 5 exists) | later | Read-only feed, trivially added to the same WS stream when it exists. |
| S11 | **Subagent tree** (once Cod Phase 3 exists) | later | Same — a natural extra tab, but it can't precede the desktop feature. |
| S12 | **File browsing / diff review from the phone** | later | Reviewing a diff on a phone before approving a write is *very* attractive, and is the natural v2 headline. Phase R6. |
| S13 | **Voice input** (dictate a prompt to a pane) | later | Android STT is free and this is genuinely good on a phone. Cheap add once S4 exists. |

**v1 scope = S1–S6.** That is a coherent, self-justifying product: *see what your
Mac is doing, answer it, and unblock it, from anywhere.*

---

## 5. Architecture

### 5.1 The three candidate approaches

#### Option A — Extend the Swift watch bridge (`the_watch/mac-app`)

Add HTTP/WS endpoints to `PermissionBridge.swift` and have Android talk to the
Swift menu-bar app, which relays into Cod.

- **For:** reuses a process that already exists, is already a "Mac-side bridge
  for remote devices," and already owns the approval round-trip and its
  `/health` contract.
- **Against — decisive:** the Swift app owns *BLE to the watch*. It owns none of
  the data the phone needs. PTY handles, pane state, scrollback, and usage stats
  all live inside the Cod Rust process (`PtyManager`, `AppState.sessions`,
  `usage.rs`). Routing them through Swift means inventing a **second**
  Cod↔Swift IPC channel purely to hand data to a third process, in a third
  language, that then re-serializes it. Every pane-output byte would cross two
  process boundaries. It also couples "can I use my phone" to "is the watch app
  running," breaking the additivity rule that both existing systems are built
  on.
- Note also that BLE — the transport the watch bridge exists to speak — is the
  wrong transport here anyway: macOS is a poor BLE peripheral, the range is a
  room, and the stated use case is being *away from* the computer.

#### Option B — Cod's Rust backend serves the phone directly *(recommended)*

Cod's own process gains a **Remote Server**: HTTP + WebSocket, in-process, with
direct access to `AppState`.

- **For:** Cod already runs a local HTTP service (`broker.rs`, `tiny_http`, port
  8766) — this is the same pattern one port over, written by the same hand, in
  the same thread-based style. The server has native access to `PtyManager` for
  reads and writes with no IPC. One process to run; if Cod is up, remote is
  possible; if Cod is down, there is nothing to remote *into* anyway, so the
  lifetime coupling is exactly right. Consistent with how the watch system is
  built (a local HTTP service on loopback owned by the process that holds the
  resource) without inheriting BLE.
- **Against:** Cod's Rust backend gets bigger and gains a genuine security
  surface; the pane-output path needs a real refactor (§6.1); `tiny_http` does
  not speak WebSocket, so a dependency is added.
- **Consistency check with the existing ecosystem:** the watch bridge's own
  design comment says it binds loopback-only *because the resource owner should
  serve it directly*. Option B is that same principle applied to Cod. The
  precedent supports B, not A.

#### Option C — Cloud relay

A small server (VPS or managed) that both Mac and phone connect out to; the Mac
holds a persistent outbound connection, the phone talks to the relay.

- **For:** works on any network with zero networking setup; makes FCM push
  natural; no NAT concerns.
- **Against:** an always-on server to build, pay for, patch, and keep up — for
  one user. Worse, **every keystroke and every line of terminal output from a
  personal machine flows through a box on the internet**, and the relay becomes
  a target whose compromise means shell access to the Mac. Achieving equivalent
  security means end-to-end encrypting through the relay, which is
  meaningfully more crypto work than the alternative. For a solo personal tool
  this is the wrong trade.

### 5.2 Recommendation

**Option B, with Tailscale as the network layer.**

```
Android phone (Cod Remote)
        │  HTTPS/WSS over the Tailscale tunnel (WireGuard, E2E encrypted)
        │  + per-device bearer token
        ▼
Mac (100.x.y.z, tailnet address)
   Cod.app
     ├── Remote Server   :8767  ← binds loopback + tailscale0 ONLY
     │      ├── REST: panes, usage, pairing, actions
     │      └── WS:   live pane output, status, approval events
     ├── Approval broker :8766  ← unchanged, loopback only
     ├── PtyManager (owns every pane's pty + new ring buffer)
     └── Tauri webview (desktop UI — an equal peer of the phone)

the_watch/mac-app  :8765  ← untouched. Still the watch's BLE bridge.
```

Tailscale does the hard part: it gives the Mac a stable address reachable from
the phone anywhere in the world, with device-level identity, no open inbound
port on any router, no public exposure, and no server to run. It is a
one-time install on two devices. Cod never binds a publicly routable interface,
so even a total failure of Cod's own auth is not internet-reachable.

*(Fallback if Tailscale is rejected — see §8.1 — the same server works LAN-only
by binding the LAN interface, with everything off-LAN unavailable. That is a
config change, not an architecture change, which is part of why this design is
the safe one to pick now.)*

### 5.3 Security model

This deserves precision because the honest threat statement is blunt:

> **A valid remote token is arbitrary code execution on the Mac.** A pane runs
> `exec claude`; anything typed into it can ask Claude to run anything, and
> approvals can be granted from the same device. There is no version of this
> feature that is not a serious access-control surface. It is worth building
> anyway — but it must be built like it means it.

Layers, defense in depth:

1. **Network scoping.** The listener binds `127.0.0.1` and the Tailscale
   interface address only — never `0.0.0.0`. If `tailscale0` is absent, the
   remote server starts loopback-only and the UI says "remote unavailable: no
   tailnet." Never fall back to a wider bind.
2. **Transport encryption.** Tailscale is WireGuard — already encrypted and
   authenticated device-to-device. Within the tunnel, plain HTTP/WS is
   acceptable and avoids a self-signed-cert mess on Android. *(Alternative:
   Tailscale Serve to get a real cert. Deferred — §8.2.)*
3. **Pairing.** Cod's desktop UI has a **Remote** panel with a "Pair a device"
   button. It displays a QR containing
   `{host, port, pairing_code, fingerprint}`, valid for **90 seconds, one use**.
   The phone scans it and POSTs the code; the server returns a fresh
   **256-bit device token** and a device record `{name, id, created, last_seen}`.
   The pairing code is never a long-lived secret; the token never appears in a
   QR that could be photographed off a screen later.
4. **Tokens.** Per-device, random 256 bits, stored **hashed** on the Mac
   (argon2 or SHA-256 of a random value — it is high-entropy, so a plain hash is
   sufficient) and compared in constant time. Stored on the phone in
   `EncryptedSharedPreferences` backed by the Android Keystore. Sent as
   `Authorization: Bearer`. The Remote panel lists paired devices with
   last-seen, and a **Revoke** button per device plus **Revoke all**.
5. **Write gate on the phone.** Anything that mutates state — sending input,
   approving, creating panes — requires a `BiometricPrompt` unlock. Read-only
   viewing does not. Recommended default: one unlock covers a 15-minute window,
   with an "every action" setting for the paranoid. *(This is genuinely the
   human's call — §8.3.)*
6. **A hard kill switch.** The Remote panel has a master **Remote access:
   on/off** toggle that stops the listener and drops live sockets immediately.
   Also an auto-off default: remote access does not survive a Cod restart
   unless explicitly set to "always on" (§8.4).
7. **Audit log.** Every authenticated remote action appends to
   `cod-remote-audit.jsonl` next to the existing `cod-broker-approvals.jsonl` —
   timestamp, device id, action, pane, and a bounded payload summary. This is
   how the human answers "what did my phone do?" after the fact. Reuse
   `broker.rs::summarize` and its truncation discipline.
8. **Rate limiting + lockout.** Failed auth attempts are logged and throttled
   with backoff; N failures disables pairing until re-enabled in the UI.
9. **Scope ceiling on remote approvals.** The proposal is that a phone may grant
   `once` and `session` scope but **not** `always` — an "approve forever" rule
   written into the cache file from a phone, one-handed, on a small screen, is
   the highest-regret action available. *(§8.5.)*

### 5.4 Real-time transport

**One authenticated WebSocket per connected phone**, multiplexing every stream
by envelope type, rather than a socket per pane or HTTP polling.

```
S→C  {"t":"panes",    "panes":[{id, sessionId, cwd, model, state, title}]}
S→C  {"t":"out",      "pane":"pane-3", "seq":41221, "data":"…"}
S→C  {"t":"state",    "pane":"pane-3", "state":"waiting|working|idle|exited"}
S→C  {"t":"usage",    "global":{…}, "panes":{…}}
S→C  {"t":"approval", "id":17, "pane":"pane-3", "tool":"Bash", "summary":"…", "expires":…}
S→C  {"t":"approvalResolved", "id":17, "by":"watch|desktop|phone", "approve":true}
C→S  {"t":"attach",   "pane":"pane-3", "since":40000}   // resume by seq, no gap
C→S  {"t":"detach",   "pane":"pane-3"}
C→S  {"t":"input",    "pane":"pane-3", "data":"…"}
C→S  {"t":"key",      "pane":"pane-3", "key":"enter|esc|ctrl-c|up|down|1"}
C→S  {"t":"approve",  "id":17, "approve":true, "scope":"once|session"}
```

Why WebSocket: terminal output is bursty and latency-sensitive, and polling
either wastes battery or feels dead. Why one socket: it keeps ordering coherent
across panes and gives the Android foreground service exactly one thing to keep
alive. `seq` numbers plus the server-side ring buffer (§6.1) make reconnect
after a tunnel blip a gap-free resume rather than a reload — important on mobile,
where the socket *will* drop.

**Approvals piggyback on the same socket and the same broker event stream** —
they are not a separate mechanism. This matters: the desktop bottom bar, the
watch card, and the phone must all be views of one pending-request registry with
one first-response-wins resolution, or the three surfaces will disagree about
what is still pending. Hence the prerequisite in §6.5.

---

## 6. Implementation notes on the Mac side

### 6.1 Pane output must become fan-out + buffered *(the main refactor)*

Today, in `lib.rs::spawn_pane`, each pane's `Receiver<PtyEvent>` is consumed by
**one** thread that emits Tauri events to the webview. There is exactly one
consumer, and **no server-side scrollback exists** — the history lives in
xterm.js inside the desktop webview, which the phone cannot see.

Both facts must change:

- Replace the single-consumer `mpsc` hand-off with a **broadcast**: the reader
  thread pushes each chunk to a per-pane `PaneStream` that (a) emits the Tauri
  event as it does now, and (b) fans out to any subscribed remote sockets.
- Add a **bounded ring buffer per pane** (proposal: ~256 KB, or ~2000 lines,
  whichever is hit first) holding recent output with monotonic `seq` offsets.
  This serves both "attach and see context immediately" and "reconnect without a
  gap."

This is the piece most likely to be underestimated. It touches the code path
that every pane's output goes through, so it wants unit tests for ordering,
buffer wraparound, subscribe-mid-stream, and slow-consumer behavior (a phone on
bad cellular must be **dropped or lagged, never allowed to block the pty
reader** — that would stall the desktop UI too).

### 6.2 Pane state classification

The phone's most valuable single datum is *"is this pane waiting for me?"*
Cod's master spec already needs this classifier for activity-driven pane sizing
and for the advisor's prompt-vs-approval distinction. Do it once, in Rust, and
let the desktop, the phone, and the notifier all consume it. Signals available:
transcript tail (turn boundaries, tool-use records), recent pty output
quiescence, and a match on Claude's prompt patterns. v1 can be crude —
`working` / `waiting` / `idle` / `exited` — and improve later.

### 6.3 HTTP/WS server choice

`tiny_http` cannot do WebSocket. Two options:

- **`tungstenite` + `tiny_http`-style threads** — keeps the thread-based,
  runtime-free style the backend has deliberately chosen (see the `Cargo.toml`
  comment rejecting axum/tokio for the broker), at the cost of hand-rolling the
  HTTP upgrade.
- **`axum` + `tokio`** — much less handwritten plumbing and better long-term
  ergonomics, at the cost of introducing an async runtime into a codebase that
  explicitly avoided one.

**Recommendation: `tungstenite` with threads.** The concurrency here is a
handful of sockets, not thousands; consistency with the existing backend is
worth more than async ergonomics at this scale; and a pty reader thread already
fits the threaded model naturally. Revisit only if the socket count or the
endpoint surface grows past what threads make pleasant.

### 6.4 Remote input path

Remote input calls the same `PtyManager::write` the desktop uses. Requirements:

- Every write is audited (§5.3.7) with a bounded summary.
- Control keys are sent as **named enums**, never raw escape sequences from the
  client. The server owns the name→bytes mapping. A remote client must not be
  able to inject arbitrary terminal control sequences.
- Input length is capped, and the pane id is validated against the live session
  map before anything is written.

### 6.5 Approvals — the prerequisite, stated plainly

**The Android approval feature cannot be built before Cod's own approval broker
gains a blocking request/registry endpoint.**

What exists today (`broker.rs`) is *only* the classifier: `POST /classify` →
`SAFE`/`UNSAFE`, never a deny, no pending-request registry, no waiting. The
racing "broadcast to watch AND Cod's bottom bar, first response wins" design is
**Phase 4 of the desktop spec and is not yet implemented**.

So the correct sequencing is:

1. Desktop Phase 4 lands: a pending-approval registry in Cod + a blocking
   `POST /permission` the hook calls when its rules and the classifier are both
   undecided, racing the watch bridge with first-response-wins.
2. The phone then attaches to that **same registry** as a third surface — it
   receives `approval` events on its WS and can resolve them. No new mechanism,
   no second code path, no chance of the surfaces disagreeing.

If the human wants phone approvals sooner than desktop Phase 4, the honest
answer is that Phase 4 is the work — the phone is a thin view on top of it. The
alternative (a phone-only approval path that bypasses the shared registry) would
create exactly the divergence the watch protocol doc warns against, and should be
refused.

Fail-safe rules are inherited unchanged: phone offline, unpaired, or
unresponsive → the request still goes to the watch and still falls through to
the terminal prompt. **The phone can never be the reason a session blocks.**

### 6.6 Notifications

**v1: no cloud, no Firebase.** The Android app runs a **foreground service**
holding the WebSocket (exactly the pattern `the_watch/companion-app`'s
`WatchService.kt` already uses for its persistent BLE link — proven, and the
human has shipped it once). Attention events arriving on the socket raise a
local `NotificationCompat` notification with **Approve / Deny** actions inline,
so the common case is answered from the lock screen without opening the app.

Cost: a persistent socket and a foreground-service notification. On a modern
phone the battery cost of an idle WS with sensible keepalives is small, and the
companion app already pays this cost for BLE.

**Later: FCM.** True push would let the app drop the socket when backgrounded.
It requires a Firebase project and a Mac-side sender — real cloud infrastructure
for a system that otherwise has none, and the payload would have to be
content-free (a wake-up ping, not the tool call) to avoid routing tool details
through Google. Worth it only if the foreground-service approach proves
unreliable in practice. Phase R6.

---

## 7. The Android app

### 7.1 Native Kotlin/Compose *(recommended)*

- **PWA / web-wrapped:** would reuse Cod's React UI, but the desktop UI is a
  freeform drag canvas — the least reusable UI imaginable for a phone, so the
  "reuse" is largely illusory. Add: no reliable background execution for the
  notification path, awkward QR scanning and biometrics, and no Keystore-backed
  secret storage. The one feature that most justifies this app (be notified and
  approve from the lock screen) is the one a PWA does worst. **Rejected.**
- **Cross-platform (Flutter / React Native / KMP):** justified by a second
  platform. There is no second platform (iOS is a non-goal), so this buys a
  toolchain and pays for it in build friction and plugin dependencies for
  biometrics/foreground service/QR. **Rejected.**
- **Native Kotlin + Compose:** the human already has `the_watch/companion-app`
  — Kotlin, Compose, `minSdk 26`, foreground service, notification listener,
  Gradle set up and building. Cod Remote is the same shape of app by the same
  author with the same skills. Foreground service, `BiometricPrompt`, CameraX
  QR scanning, `EncryptedSharedPreferences`, and OkHttp WebSocket are all
  first-party or standard. **Recommended.**

New module: `cod/android/` (or a sibling `cod-remote/`) — its own Gradle
project, namespace `com.emgor.cod.remote`, deliberately **not** merged into the
watch companion app: different device, different transport, different lifecycle,
and merging them would mean one app's crash takes out both.

### 7.2 The key UI decision — conversation view, not a tiny terminal

An 80×24 VT rendered on a phone is miserable. But Cod already tails
`~/.claude/projects/*/*.jsonl`, so the **structured conversation** — user turns,
assistant turns, tool calls, results — is already parsed for the usage bar.

Proposal:

- **Primary tab — Session view.** A chat-style rendering of the transcript:
  assistant messages as text, tool calls as collapsed chips ("Bash `npm test`"),
  expandable to output. This is readable one-handed and is what you actually
  want when checking in from a coffee shop.
- **Secondary tab — Raw terminal.** True VT rendering for when the chat view is
  insufficient (a TUI, a full-screen prompt, colored diff output). Termux's
  `terminal-view` / `terminal-emulator` modules are Apache-2.0 and embeddable;
  otherwise a minimal read-mostly VT renderer over the ring-buffer stream.
- **Composer bar.** A text field (send as a prompt) plus a compact control-key
  row: `Enter` `Esc` `Ctrl-C` `↑` `↓` `1` `2` `3`. Because panes run
  `exec claude`, natural-language text plus those keys covers essentially all
  real interaction.

*(How much raw-terminal fidelity is actually needed is a genuine open question —
§8.6.)*

### 7.3 Screens (v1)

1. **Connection** — paired Mac, status, pair-new-device QR scanner.
2. **Panes** — list, each row: title/cwd, state pill (working/waiting/idle),
   model, context %, last activity. Waiting panes sort to the top.
3. **Session** — the tabbed conversation/terminal view + composer (§7.2).
4. **Approvals** — pending requests with tool + summary, Approve/Deny, and
   inline notification actions. Resolved-elsewhere requests disappear live.
5. **Usage** — today's rollup, per-pane breakdown.
6. **Settings** — biometric policy, notification prefs, revoke/unpair, audit
   log view.

### 7.4 Offline / disconnected behavior

The app is explicit and honest when there is no Mac: last-known state shown
greyed with an "as of HH:MM" stamp, and every write control disabled. It never
queues input to replay later — a prompt sent into a session whose state has
moved on is worse than one not sent.

---

## 8. Open questions — the human's call

These are deliberately not decided here.

1. **Tailscale, yes or no?** The whole reachability story rests on it. If it is
   unwelcome on the Mac or the phone, the fallback is LAN/VPN-only (still a fine
   product — it just doesn't work from a café). Confirm before Phase R1.
2. **Plain HTTP inside the tunnel, or real TLS via Tailscale Serve?** The tunnel
   is already encrypted; TLS inside it is belt-and-braces at the cost of cert
   handling. Recommendation is plain-inside-tunnel; it is your risk appetite.
3. **Biometric policy for writes:** every action, or one unlock per N minutes?
   Recommendation: 15-minute window, configurable. Also — should *reading*
   pane output require unlock too? Your terminal output can contain secrets.
4. **Always-on remote, or explicit enable?** Recommendation: remote off by
   default at launch, toggled on when you know you're leaving — smallest
   exposure window. But "I forgot to turn it on before I left" is exactly the
   failure that makes the feature useless. Genuinely yours.
5. **May the phone grant `always` scope on approvals?** Recommendation: no —
   `once`/`session` only, because an "approve forever" rule created by a mis-tap
   on a phone is the highest-regret possible action and is written to a cache
   file that then silently applies to every future session. Your call.
6. **How much raw terminal fidelity do you actually want?** Full VT emulation on
   the phone is a meaningful chunk of work. If the chat view plus a handful of
   control keys is enough in practice, R2 gets substantially cheaper. Worth a
   deliberate answer before Phase R2 rather than discovering it after.
7. **Does the watch stay in the approval loop once the phone exists,** or does
   the phone become primary with the watch as glanceable backup? This affects
   whether the racing registry needs to support 3 surfaces indefinitely
   (recommendation: yes, it should — but confirm you still want the watch).
8. **Should the phone be able to *spawn* panes (S7)?** Starting a fresh Claude
   session in an arbitrary directory from a phone is powerful and is also the
   widest blast-radius action in the whole feature set. Deferred to R5 here;
   confirm you want it at all.
9. **Where does the Android project live** — inside `cod/android/`, or as a
   sibling top-level project next to `the_watch/companion-app`? Repo-convention
   question; the additive/no-delete rule is satisfied either way.
10. **Is a stolen, unlocked phone an in-scope threat?** Answering "yes" pushes
    toward per-action biometrics and short token lifetimes. Answering "no"
    simplifies a lot. It changes several defaults above, so it is worth an
    explicit answer rather than a drift.

---

## 9. Phased build plan

Same spirit as Cod's own desktop phasing: each phase is independently usable and
testable before the next begins. **Phases R0–R1 are Mac-side only** — no Android
code exists until the server is real and provable from a laptop.

### Phase R0 — Pane streams become remotable *(Rust only, no network)*
Refactor pane output to broadcast + per-pane bounded ring buffer with `seq`
(§6.1). Add pane-state classification (§6.2). Desktop behavior must be
byte-identical afterward.
**Deliverable:** desktop works exactly as before; unit tests cover fan-out
ordering, mid-stream subscribe, ring wraparound, and slow-consumer drop without
stalling the pty reader.

### Phase R1 — Remote Server on the Mac *(no phone yet)*
HTTP + WS on 8767, loopback + tailscale bind, pairing flow, token issue/verify/
revoke, audit log, and the Cod desktop **Remote panel** (toggle, QR, device
list, revoke). Endpoints: panes, usage, attach/stream, input, control keys.
**Deliverable:** from the Mac itself, `curl` lists panes with a token and is
rejected without one; `websocat` attaches to a pane, sees scrollback then live
output, and can type into it. From another tailnet machine, the same works;
from the LAN without the token or without the tailnet, it does not. Audit log
shows every action.

### Phase R2 — Android app: read-only
Kotlin/Compose project, QR pairing, token storage in Keystore, connection
screen, pane list with states, session view (conversation tab first), usage
screen. **No write path at all in this phase** — it cannot type, so it cannot
break anything, which makes it the right phase to shake out the transport,
reconnect/resume, and battery behavior.
**Deliverable:** on the phone, off the home network, you can watch a live Claude
session running on the Mac and see today's usage.

### Phase R3 — Input
Composer + control-key row, biometric gate, per-action audit entries, server-side
validation and caps. Raw-terminal tab if §8.6 says it's wanted.
**Deliverable:** from the phone, send a prompt to a running session, interrupt
one with Ctrl-C, and see the result stream back. Every action appears in the
audit log with the device id.

### Phase R4 — Approvals + notifications
**Blocked on desktop Phase 4** (the pending-approval registry, §6.5). Then:
approval events on the WS, foreground service, lock-screen notifications with
Approve/Deny actions, live removal when the watch or desktop wins the race.
**Deliverable:** trigger a routed tool call on the Mac with the screen locked and
the phone in a pocket — the phone buzzes, one tap approves, the watch card and
desktop bar both clear, and the session proceeds. Kill the phone mid-flight and
confirm the watch/terminal fallback is untouched.

### Phase R5 — Pane management
Create a pane (recent-directories picker), close, rename, resume a persisted
session. Gated behind biometrics like all writes.
**Deliverable:** start a fresh Claude session in a chosen project from the phone
and drive it to a first useful result.

### Phase R6 — Later ambition
FCM push (drop the persistent socket when backgrounded); diff/file review before
approving a write (S12 — the natural v2 headline: *see the diff on your
phone before you approve it*); voice input; advisor feed and subagent tree once
those desktop phases exist; widget/Quick Settings tile showing pane states.

---

## 10. Testing

- **Rust unit tests:** ring buffer wraparound and `seq` continuity, fan-out
  ordering, subscribe-mid-stream, slow-consumer drop, token verify (including
  constant-time compare and rejection paths), pairing-code single-use and
  expiry, control-key name→bytes mapping, input validation/caps.
- **Rust integration:** full remote round-trip over a real socket (in the spirit
  of the existing `#[ignore]`-gated `e2e_health_and_classify_over_http` test),
  and an explicit test that the remote server **never** binds a non-loopback,
  non-tailnet interface.
- **Manual, per phase:** the deliverables above are written as manual test
  scripts on purpose.
- **Security-specific manual checks, every phase:** try each endpoint with no
  token, an expired token, a revoked token, and from an off-tailnet LAN host;
  confirm all four are refused and logged.
- **Fail-safe regression, mandatory before R4 ships:** with the phone off,
  unpaired, and mid-request-killed, confirm approvals still reach the watch and
  still fall through to the terminal prompt. This is the invariant the whole
  ecosystem is built on and it must be re-proved, not assumed.
- No automated Android UI tests (consistent with the desktop spec's non-goals).

---

## 11. Relationship to existing systems — summary

| System | Port | Owner | Changed by this plan? |
|---|---|---|---|
| `watch_approve_hook.py` | — | the_watch | Only by desktop Phase 4, which this plan depends on but does not itself change. |
| Watch Mac bridge (Swift, BLE) | 8765 | the_watch | **No.** Untouched. |
| Cod approval broker (classify) | 8766 | Cod (Rust) | Gains the pending-approval registry in desktop Phase 4 (prerequisite, not part of this plan). |
| **Cod Remote Server** | **8767** | **Cod (Rust)** | **New — this plan.** |
| Cod pane output path | — | Cod (Rust) | **Refactored — Phase R0.** |
| Android watch companion | — | the_watch | **No.** Separate app; Cod Remote is a new Gradle project. |
