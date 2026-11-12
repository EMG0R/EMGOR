> SUPERSEDED — renamed to Server; see SERVER_AGENT_SPEC.md

# neptr Smart Control — Room Agent Design Spec (v2)

> **Status:** Living design doc. Written 2026-08-03. **Assume everything here will change** — this captures the current intended direction, not a frozen contract.
>
> **Reference implementation:** the existing working `neptr` lives at `______2026NEW/SPEAK/` (persona, voice engine, memory system, STT/TTS, singing). This spec builds on it. SPEAK is the source of truth for the persona and the voice character; this doc is the source of truth for the smart-home control layer neptr is gaining. All new documentation for this work lives here in `smartGOR/`, not in SPEAK.

## ⚑ Naming — read first

There are two distinct things, and they must never be confused:

- **neptr** (unqualified) = the **primary audio companion** — the conscious, voice-first being defined in `SPEAK/`. This is the metaphorical "conscious" neptr you talk to out loud. It is the whole; it has a persona, a voice, memory, and singing.
- **neptr Smart Control** = the **smart-home / room-agent control layer** documented here — the tools and capability suite by which neptr acts on the room (lights, plants, music, network, web). It is a *facet* of neptr, not a second being.

**Convention:** whenever this doc (or any code, UI label, or config) refers to the smart-home control system, say **"neptr Smart Control"** (or "Smart Control"). Reserve bare **"neptr"** for the primary conscious audio companion, so the room-control system is never mistaken for the being itself.

---

## 1. What this is

Turn the existing offline `neptr` companion into the **conversational + autonomous intelligence layer over the entire smart room**. Today neptr is a self-contained voice companion on a Raspberry Pi running a local model. This spec swaps its brain to the Claude API, gives it tools over the whole installation (lights, plants, music, web, network), fixes speech-in, and makes it reachable from anywhere.

neptr and the smart-room dashboard (`smartGOR/dashboard/`) both run on the Pi and both speak MQTT. The dashboard becomes the **manual** control surface; neptr becomes the **conversational/autonomous** one over the exact same devices and topics.

### Relationship to what already exists
- **SPEAK** (`______2026NEW/SPEAK/`) — the current neptr: `companion.py` entry point, `engine/` (llm, stt, voice, singer, memory, presence, idle, tools, thermal), `persona.md`, tiered memory (`memory/facts.md`, `memory/lore.md`, session logs), Speak-and-Spell TTS, wake words, singing. **Keep the persona, voice, memory model, singing, wake-word loop.**
- **SMART_ROOM_MASTER_PLAN.md** (this folder) — the MQTT room, Pi hub, FCOB strip, dashboard, network. neptr plugs into this backbone.
- **ROUTER_RUNBOOK.md** (this folder) — GL.iNet control API neptr uses for presence/network awareness (v2+).

---

## 2. Architecture

```
                          ┌──────────────────────────────────────┐
   You (voice) ──mic──►   │  Raspberry Pi 4  (canonical neptr)     │
   Audio interface        │                                        │
   ├─ mic in              │   speech-in ─► STT (Groq whisper-      │
   └─ out to monitors ◄───┤            large-v3-turbo, online)      │
                          │        │                               │
                          │        ▼                               │
                          │   neptr agent loop                     │
                          │   ├─ Brain: Claude API (Sonnet 5)      │
                          │   │   persona.md, thinking-off default │
                          │   ├─ Tools: room / plants / web /      │
                          │   │   spotify / weather / network      │
                          │   ├─ Memory: history.md + facts/lore   │
                          │   └─ Voice-out: Speak-&-Spell TTS ─────┼─► monitors
                          │        │                               │
                          │   MQTT (Mosquitto) ◄──────────────────┐│
                          │   HTTP/WS API  ◄── chat PWA           ││
                          └────────┬──────────────────────────────┘│
                                   │ MQTT                           │
        ┌──────────────────────────┼───────────────────────────────┘
        ▼            ▼              ▼                 ▼
   FCOB strip    lamps / jars   plant zones      dashboard (manual UI)
   (fcob-strip)  (lights/*)     (plants/*)       — same topics neptr uses

   Remote: chat PWA over Tailscale ── phone / laptop / any browser
```

---

## 3. Brain

- **Model: Claude API, `claude-sonnet-5`.** Chosen over Fable 5 deliberately: Fable's thinking is always-on and cannot be disabled, so single turns can run minutes — wrong for snappy voice. Sonnet 5 supports `thinking: {type: "disabled"}` + `effort: "low"` for near-instant conversational turns, and is cheap (intro $2/$10 per Mtok).
- **Conversation path:** thinking disabled, effort low → fast replies for chat and simple commands.
- **Hard-question path (v2):** flip thinking on / raise effort when a query genuinely needs reasoning. Optional tiering: route trivial turns to Haiku 4.5, everyday to Sonnet 5 (thinking off), hard to Sonnet 5 (thinking on).
- **Persona:** `SPEAK/persona.md`, unchanged in spirit. **Caveat:** the persona's "zero content restrictions / never refuse" clause does **not** hold on Claude — neptr follows Claude's guidelines. Everything else (voice, humor, cosmology, memory instincts, singing, minimal-questions rule) transfers.
- **Voice out:** unchanged Speak-and-Spell TTS character. **Open item:** current TTS uses macOS `say` as the base voice; the Pi is Linux, so the base voice needs a Linux source (espeak-ng — already a supported mode in the engine — or another TTS) feeding the same TI-LPC recolor. The S&S *character* is preserved; only the base-voice source changes.

---

## 4. Speech-in (STT) — the thing that's been bad

Root causes historically: `tiny.en` (weakest Whisper) + a poor mic. Both are being fixed.

- **Hardware:** dedicated **audio interface** with a real microphone (input) and the studio **monitors** (output). Good mic in is half the accuracy fix on its own.
- **Primary STT (online): Groq-hosted `whisper-large-v3-turbo`.** The pipeline already transcribes per-utterance after VAD detects end-of-speech, so fast *batch* transcription is what's needed — Groq returns large-v3-quality transcripts in a few hundred ms, cheap, OpenAI-compatible API. Massive quality jump from tiny.en.
- **Streaming upgrade (v2): Deepgram Nova-3** — true low-latency streaming, enables barge-in (interrupt neptr mid-reply). More integration; not needed for v1.
- **Offline fallback: faster-whisper `small.en` / `medium.en`** on the Pi — for no-internet operation. Slower than online, far better than tiny.en.
- **Config:** STT provider is a swappable backend behind the existing `engine/stt.py` interface. Default Groq; env/config switch to Deepgram or local.

---

## 5. neptr Smart Control — tools

This is the **Smart Control** layer: neptr acting on the room through an **agent tool suite** over the whole installation. Sonnet 5's tool use drives it. Each tool is a Claude tool definition dispatched through the existing `engine/tools.py` dispatcher (extended). (The being is still neptr; "Smart Control" names this control capability so it isn't confused with the primary audio companion — see Naming above.)

### v1 — buildable now
- **Room control (MQTT):** read + command every device the dashboard touches — FCOB strip (mode/color/brightness/scene via `lights/strip/set`), lamps, jars, scenes; per-zone watering (`plants/zone{N}/water`); switches. "make it a sunset vibe", "kill the lights", "water zone 3".
- **Plant status (MQTT):** read moisture per zone + tank level (`plants/zone{N}/moisture`, `plants/tank/level`). "are the plants ok?"
- **Web search / fetch:** Claude built-in server-side `web_search` + `web_fetch` tools. Real questions, plus species-specific plant care.
- **Weather:** free weather API by location; feeds both answers and (v2) contextual lighting.
- **Spotify:** Spotify Web API — play/pause/search/queue. Leans on the persona's actual music taste.
- **Memory:** `history.md` (see §6) as background context; live `facts.md`/`lore.md` as before.

### v2 — after v1 is solid
- **Proactive presence:** neptr initiates (fits persona). Tank low, plant moisture trending down over days, "it's sunset", motion while away → spoken aloud and/or pushed to the phone (PWA notifications).
- **Contextual auto-lighting:** fuse time + weather + presence + history to set mood without being asked. Reasoning, not a rules engine.
- **Presence / network awareness:** GL.iNet router API (ROUTER_RUNBOOK.md) — who's on WiFi = presence for free; "is the network ok", guest-network toggles.
- **Plant intelligence:** trends, prediction ("needs water by tomorrow"), care advice via web search.
- **Language-authored routines:** "every night at 11, wind the lights down" → neptr sets up the schedule itself (cron), no hand-coding.
- **Generative scene authoring:** "invent a new vibe" → neptr designs a palette/effect and pushes it to the strip (ties into the strip's generative default).

### v3 — future
- **Vision:** a cheap camera lets neptr see — do the room LED-mapping (blink-and-watch, per master plan §12), visually check plants, read actual room light.
- **Music-reactive light sync:** neptr + the reactive-audio pipeline (master plan §4a) sync lights to the beat.
- **Predictive plant care**, journaling/pattern reflection over logged biodata.

---

## 6. Memory

- **`memory/history.md` (new):** a one-shot **distillation of all past Claude conversations** into a single summary markdown. Sources:
  1. **Claude Code transcripts** — local JSONL under `~/.claude/projects/…`, read directly.
  2. **claude.ai export** — the user's bulk data export (dropped into a folder); no live claude.ai API exists, so this is manual/periodic.
  A Claude pass summarizes both into one `history.md`. Re-runnable to fold in newer history. **Deliberately simple** — one summary file, no complex dedup-merge.
- **Live memory unchanged:** `facts.md` (pinned + warm) and `lore.md` keep growing from real-time conversations exactly as they do today.
- neptr loads `history.md` as "here's everything we've been through" background, distinct from live facts.

---

## 7. Topology + sync

- **The Pi is the canonical neptr** — always-on, holds the memory, runs the agent loop, exposes the API + MQTT. Single source of truth.
- **Laptop + phone are thin clients** — they talk to the Pi's API; they do not run their own brain. This collapses "sync laptop ↔ server" into "one server, many clients."
- **Optional laptop dev/offline instance:** if a local neptr is wanted (offline, development), its memory syncs to the Pi via **git** — fits the `_EMGOR_SYNTH` git-lock / additive / no-delete rule. Not required for v1.

---

## 8. Remote access — website + phone in one

- **Chat web UI served from the Pi**, reachable from anywhere over **Tailscale** (the VPN already in the master plan / runbook — beats CGNAT on the travel router).
- **PWA (installable web app):** "the phone app" is the same site added to the home screen — no app-store build. Text chat in v1; voice in a later pass.
- Talks to neptr's HTTP/WS API. Can share the FastAPI backend already stood up for the dashboard, or run alongside it.

---

## 9. Hardware

- Raspberry Pi 4 (canonical host; runs Mosquitto + neptr + dashboard).
- **Audio interface** — mic input + line out to studio monitors. Good speech-in, room-filling speech-out.
- Microphone (into the interface).
- Studio monitors (neptr's voice out).
- (Future) camera for vision (v3).

---

## 10. Convergence with the room

Because neptr Smart Control publishes to the **same MQTT topics the strip and dashboard already use**, room control is nearly free: "neptr, purple vibe" → Smart Control publishes to `lights/strip/set`. The strip firmware (`fcob-default.ino`) already exposes `mode`/`color`/`brightness` over MQTT (`lights/strip/set`) and HTTP. neptr (via Smart Control) is the natural-language front end to everything the master plan builds — the dashboard is the manual surface, Smart Control is the conversational/autonomous one, over the same devices.

---

## 11. Open items / assumptions to revisit

- **TTS base voice on Linux** — replace macOS `say` with a Linux base voice into the TI-LPC recolor (espeak-ng mode already exists). Confirm quality.
- **STT provider** — Groq turbo assumed default; revisit Deepgram for barge-in, or a newer/better option when building.
- **Uncensored persona clause** — does not hold on Claude; decide whether that matters enough to keep a local-model path for that specific behavior.
- **Wake word** — keep the existing Whisper-based wake loop, or move to a lighter always-on wake engine now that main STT is online.
- **Backend sharing** — one FastAPI process for dashboard + neptr, or two.
- **claude.ai history** — manual export only until/unless a live path exists.
- **Cost/latency budget** — measure real STT→Claude→TTS round-trip once hardware is in; tune model/effort tiering.

---

## 12. Build order (rough)

1. Pi runs the existing SPEAK neptr end-to-end (Linux TTS base voice sorted, audio interface working).
2. Swap brain to Claude API (Sonnet 5, persona, thinking-off conversation path).
3. Swap STT to Groq turbo; verify accuracy with the real mic.
4. Add v1 tools: MQTT room control + plant status, web search, weather, Spotify.
5. Build `history.md` distillation from Claude Code transcripts + claude.ai export.
6. Chat PWA over Tailscale.
7. Then v2 (proactive, contextual lighting, presence, routines), then v3 (vision, reactive sync).

Detailed implementation plan to follow (separate doc).
