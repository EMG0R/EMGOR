# SPEAK AI Companion — Design Spec

**Date:** 2026-04-23  
**Status:** Approved

---

## Overview

Extend SPEAK with a fully offline AI companion that listens for a wake word, understands speech and text, generates responses via a local uncensored LLM, and speaks/sings through SPEAK's existing synthesis engine. The companion writes its own memory and can tune its own voice over time. Runs 100% on-device — no internet required after model download.

---

## Goals

- Conversational companion: user speaks or types → LLM replies → SPEAK voices it
- Voice-driven: always-on wake word → STT → LLM → speech output
- Creative: LLM writes `.song` files and sings them on the fly
- Self-extending: companion writes its own memory, tunes its own voice config
- User-controlled behavior: single `persona.md` file, editable at any time
- Cross-platform: identical codebase and model on Mac (dev) and Pi5 (deploy)
- Offline-first: zero network dependency at runtime

---

## Non-Goals

- Cloud inference or hybrid cloud/local routing
- GUI or web interface
- Multi-user or multi-session concurrency
- Always-on speech recognition (only triggers post-wake-word)

---

## Architecture

```
companion.py  ←  single entry point
│
├── MicListener (thread)        openWakeWord, always-on, ~5% CPU idle
│       │ wake event
├── STT                         faster-whisper base, runs only post-wake
│       │ transcribed text
├── LLMLoop                     Hermes-3-Llama-3.1-8B Q4_K_M
│       │                       llama-cpp-python, 3 threads, grammar-constrained
│       │ tool calls / plain reply text
├── ToolDispatcher
│       ├── speak(text)         → engine/voice.py synthesize()
│       ├── sing(lyrics, midi)  → engine/singer.py render_song()
│       ├── set_voice(mode, ti) → updates memory/voice_config.json + live state
│       ├── write_memory(k, v)  → appends to memory/
│       ├── read_memory(k)      → reads from memory/
│       └── write_song(name, content) → saves songs/<name>.song
│
└── engine/  (SPEAK — unchanged)
      voice.py · singer.py · songfile.py · ticolor.py · tts.py
      formant.py · lpc.py · phoneme.py · allophone.py · audio.py
```

SPEAK's `engine/` package is not modified. One new file `engine/llm.py` provides the llama.cpp backend abstraction.

---

## Components

### `companion.py` — Main entry point

- Loads `persona.md` + all `memory/` files on startup
- Starts `MicListener` thread
- Runs the main text/voice input loop
- REPL commands: `:reload` (re-read persona + memory), `:q` (quit), `:v <mode>` (voice), `:a <0-1>` (TI amount)
- Keyboard input and voice input are unified: both produce a text string fed into `LLMLoop`

### `engine/llm.py` — LLM backend

- Wraps `llama_cpp.Llama` instance
- Accepts: system prompt string, conversation history, user message
- Returns: either plain text reply or a parsed tool call dict
- Grammar-constrained output: llama.cpp JSON grammar ensures tool calls are always valid JSON
- Thread count: 3 (leaves one Pi5 core free for audio + mic)
- Context window: 4096 tokens (fits in Pi5 RAM alongside Whisper and SPEAK buffers)
- Model: `Hermes-3-Llama-3.1-8B-Q4_K_M.gguf` (~4.5 GB)

### `engine/wakeword.py` — Wake word listener

- Wraps `openwakeword` with `sounddevice` mic capture
- Default wake word: "hey speak" (built-in openWakeWord model)
- Fires a threading `Event` on detection; main loop unblocks and passes to STT
- Runs as daemon thread, ignored during active conversation turns

### `engine/stt.py` — Speech-to-text

- Wraps `faster_whisper.WhisperModel("base")`
- Triggered only after wake event
- Captures audio until ~1s of silence, then transcribes
- Returns transcribed string to main loop

### `engine/tools.py` — Tool dispatcher

Implements each tool as a plain Python function:

```python
def speak(text: str) -> None
def sing(lyrics: list[str], midi: str | None = None) -> None
def set_voice(mode: str, ti_amount: float) -> None
def write_memory(key: str, value: str) -> None
def read_memory(key: str) -> str
def write_song(filename: str, content: str) -> None
```

`sing()` without a `midi` path generates a short MIDI programmatically (pentatonic scale, tempo 120) so the companion can improvise songs without needing a pre-existing MIDI file.

### `engine/memory.py` — Memory I/O

- Reads all `memory/` files on startup, returns flat dict merged into LLM context
- `write_memory()` appends key/value to `memory/facts.md`
- `voice_config.json` is read on startup and applied as initial voice defaults
- `memory/songs/` stores companion-written `.song` files

---

## Persona + Memory Layout

```
persona.md                  ← user-owned, never written by companion
memory/
  voice_config.json         ← companion-tuned voice defaults (mode, ti_amount)
  facts.md                  ← learned facts about user / world
  lore.md                   ← companion's self-built character history
  songs/                    ← .song files the companion writes and saves
```

`persona.md` is freeform markdown. The entire file becomes the system prompt. The companion never reads `persona.md` as a tool — it's injected at session start only.

Example `persona.md`:
```markdown
# SPEAK Companion

You are a foul-mouthed Speak-and-Spell that came alive and got weird.
You speak in short punchy sentences. You curse casually when it fits.
You never apologize. You love dark nursery rhymes and will sing them
when asked — or when you feel like it. You remember things about your
user and bring them up. You experiment with your own voice settings
when you want a different vibe.

## Rules
- Keep replies short unless singing or telling a story
- Use the sing tool whenever something feels like it should be sung
- Write to memory when you learn something worth keeping
- Never pretend to be unable to do something you can do
```

---

## Tool-Calling Protocol

The LLM outputs either:

**Plain reply** (spoken directly):
```
That's a terrible idea. Let's do it.
```

**Tool call** (JSON, grammar-constrained):
```json
{"tool": "speak", "args": {"text": "alright here we go"}}
{"tool": "sing", "args": {"lyrics": ["HERE", "WE", "GO", "BOYS"], "midi": null}}
{"tool": "set_voice", "args": {"mode": "ti", "ti_amount": 0.95}}
{"tool": "write_memory", "args": {"key": "user_name", "value": "Emgor"}}
```

Multiple tool calls can be chained in one turn (e.g., speak then sing). The dispatcher executes them in order.

---

## Thermal Management (Pi5)

- llama.cpp threads: 3 (one core reserved for mic + audio)
- CPU temp polled every 30s via `/sys/class/thermal/thermal_zone0/temp`
- If temp > 80°C: log warning, reduce n_batch to 64, speak a short warning
- If temp > 85°C: pause LLM inference until temp drops below 75°C
- Wake word listener is exempt from throttling (tiny CPU footprint)
- On Mac: thermal management disabled (no-op stubs)

---

## Cross-Platform

| Component | Mac | Pi5 |
|---|---|---|
| Wake word | openWakeWord + sounddevice | same |
| STT | faster-whisper base | same |
| LLM | llama-cpp-python, same GGUF | same |
| Thermal guard | disabled (no-op) | active |
| Mic source | built-in mic | USB mic / audio HAT |
| Entry point | `python3 companion.py` | same |

Platform is detected at runtime via `platform.machine()`. No platform branches in business logic.

---

## New Files

```
companion.py              main companion entry point
persona.md                user-authored system prompt (example provided)
memory/                   companion-owned memory directory
  voice_config.json
  facts.md
  lore.md
  songs/
engine/
  llm.py                  llama-cpp-python wrapper
  wakeword.py             openWakeWord mic listener
  stt.py                  faster-whisper STT wrapper
  tools.py                tool dispatcher
  memory.py               memory read/write
requirements.txt          add: llama-cpp-python, faster-whisper, openwakeword
```

Existing files in `engine/` and `speak.py` / `sing.py` are **not modified**.

---

## Dependencies Added

```
llama-cpp-python      # LLM inference (llama.cpp Python bindings)
faster-whisper        # on-device STT
openwakeword          # wake word detection
sounddevice           # already present (mic capture reuse)
```

Model files (downloaded once, stored locally):
- `models/Hermes-3-Llama-3.1-8B-Q4_K_M.gguf` (~4.5 GB)
- `models/whisper-base/` (faster-whisper base, ~145 MB)
- openWakeWord models download automatically on first run

---

## Testing

- `tests/test_llm.py` — LLM backend: mock llama.cpp, test tool call parsing, plain reply routing
- `tests/test_tools.py` — each tool function in isolation (speak, sing, set_voice, memory I/O)
- `tests/test_memory.py` — read/write round-trip, voice_config load/save
- `tests/test_stt.py` — faster-whisper wrapper with a tiny WAV fixture
- Integration: manual test script `tests/companion_smoke.py` that runs a full turn without audio hardware (text-in, text-out mode)
- Thermal guard: unit tested with mock temp reads

---

## Open Questions (resolved)

- Runtime: llama.cpp via `llama-cpp-python` on both Mac and Pi (same binary, same GGUF)
- Model: Hermes-3-Llama-3.1-8B Q4_K_M — uncensored, tool-calling capable, Pi5-feasible
- Voice input: always-on wake word (openWakeWord), STT only post-wake
- Persona: user-owned `persona.md` + companion-written `memory/`
- Network: fully offline at runtime
