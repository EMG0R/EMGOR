"""
Server — FastAPI chat backend for the home AI agent.

Loads the neptr persona + live memory (facts/lore) as system-prompt context,
and shells out to the local `claude` CLI (authenticated via the user's
Claude subscription, not a metered API key) to generate replies.

NOTE: "Server" is the name of this chat interface/project. The underlying
being's persona is defined in persona.md (which refers to itself as
"neptr") — Server never renames or overrides that identity, it just never
calls the *project* "neptr" in its own code/API surface.
"""

from __future__ import annotations

import hmac
import os
import re
import shutil
import threading
import subprocess
import tempfile
import time
import wave
from collections import defaultdict
from pathlib import Path
from typing import Optional

import httpx
from dotenv import load_dotenv
from fastapi import BackgroundTasks, Depends, FastAPI, File, Header, HTTPException, Request, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel

from . import local_brain, memory_tools
from . import persona_tools
from . import context_tiers
from . import warm_brain

# ---------------------------------------------------------------------------
# Paths / config
# ---------------------------------------------------------------------------

SPEAK_ROOT = Path(__file__).resolve().parent.parent
PERSONA_PATH = SPEAK_ROOT / "persona.md"
FACTS_PATH = SPEAK_ROOT / "memory" / "facts.md"
LORE_PATH = SPEAK_ROOT / "memory" / "lore.md"
HISTORY_PATH = SPEAK_ROOT / "memory" / "history.md"
ROOM_LIGHTS_SKILL_PATH = Path(__file__).resolve().parent / "skills" / "room-lights" / "SKILL.md"
# Compact persona used ONLY by the local on-Pi brain. persona.md is ~21k chars
# and the full cloud system prompt is ~50k (~13k tokens); at the Cortex-A72's
# measured single-digit tokens/sec prefill that is many minutes of compute
# before a single word is generated. See LOCAL_MODEL.md. Falls back to
# persona.md if this file is missing.
PERSONA_LOCAL_PATH = SPEAK_ROOT / "persona_local.md"
BRAIN_SKILL_PATH = Path(__file__).resolve().parent / "skills" / "brain-switching" / "SKILL.md"
PERSONALITY_SKILL_PATH = (
    Path(__file__).resolve().parent / "skills" / "personality-input" / "SKILL.md"
)

ENV_PATH = Path(__file__).resolve().parent / ".env"
load_dotenv(ENV_PATH)

# AUTH_ENABLED gates the bearer-token check on POST /api/chat. Set
# SERVER_AUTH_ENABLED=true in .env to re-enable the passphrase gate — the
# token/lockout logic below is left fully intact, just skipped when this is
# False.
AUTH_ENABLED = os.environ.get("SERVER_AUTH_ENABLED", "false").lower() == "true"

SHARED_SECRET = os.environ.get("SERVER_SHARED_SECRET", "")
if AUTH_ENABLED and not SHARED_SECRET:
    raise RuntimeError(
        f"SERVER_SHARED_SECRET is not set. Create {ENV_PATH} with "
        "SERVER_SHARED_SECRET=<random token> before starting the server."
    )

CLAUDE_BIN = os.environ.get("CLAUDE_BIN", "claude")
CLAUDE_TIMEOUT_SECONDS = int(os.environ.get("CLAUDE_TIMEOUT_SECONDS", "120"))

# ntfy.sh push notification topic (best-effort, non-blocking). Unset/empty
# means notifications are silently skipped.
NTFY_TOPIC = os.environ.get("NTFY_TOPIC", "")

# ---------------------------------------------------------------------------
# Lights: local lightserver engine (LED strip render engine on this Pi)
# ---------------------------------------------------------------------------
#
# The lightserver (systemd unit `lightserver`, ~/lightserver) renders scenes on
# the Pi and streams UDP frames to the 800-px FCOB strip. Its HTTP API is on
# localhost:8800. The chat model controls it by embedding a directive in its
# reply:
#     <lights>slow deep ocean waves</lights>   -> POST {"text": ...} /prompt
#     <lights-off/>                            -> POST /stop
# The directive is stripped from the user-visible reply and executed in a
# background thread (the /prompt natural-language path itself shells out to
# `claude` on the lightserver side and can take 10-30s, far too long to hold a
# chat reply for). A quick /state probe decides whether to confirm
# "(lights: done)" or report "(lights: unavailable)".
LIGHTS_API = os.environ.get("LIGHTS_API", "http://localhost:8800")
LIGHTS_PROMPT_TIMEOUT_SECONDS = float(os.environ.get("LIGHTS_PROMPT_TIMEOUT", "90"))

LIGHTS_SPEC = """## Skill: LED Strip Lights (directive)

You control an 800-LED strip in this room through a local render engine. To
change the lights, include ONE directive anywhere in your reply:

    <lights>a short vibe or scene description</lights>

Examples: <lights>slow ocean waves, deep blue</lights>,
<lights>cozy warm candle flicker</lights>, <lights>rave strobes</lights>.
Describe the vibe in natural language; the engine interprets it. To turn the
lights off, use: <lights-off/>

The directive is executed by the backend and stripped from your reply before
the user sees it, so also say something conversational about what you're doing
with the lights. Only emit a directive when the user actually asks for a
lighting change. Prefer this directive over any curl-based lighting commands."""

_LIGHTS_TAG_RE = re.compile(r"<lights>(.*?)</lights>", re.IGNORECASE | re.DOTALL)
_LIGHTS_OFF_RE = re.compile(r"<lights-off\s*/?\s*>", re.IGNORECASE)


def _lights_prompt_worker(vibe: str) -> None:
    """Background: hand the vibe to the lightserver's NL endpoint. That call
    itself shells out to `claude` and can take ~10-30s, hence fire-and-forget
    with logging — never on the chat request path."""
    try:
        resp = httpx.post(
            f"{LIGHTS_API}/prompt",
            json={"text": vibe},
            timeout=LIGHTS_PROMPT_TIMEOUT_SECONDS,
        )
        print(f"[lights] prompt {vibe!r} -> HTTP {resp.status_code}: "
              f"{(resp.text or '')[:200]}")
    except Exception as exc:  # noqa: BLE001 - must never break anything
        print(f"[lights] prompt {vibe!r} FAILED: {exc}")


def _lights_off_worker() -> None:
    try:
        resp = httpx.post(f"{LIGHTS_API}/stop", timeout=10)
        print(f"[lights] stop -> HTTP {resp.status_code}")
    except Exception as exc:  # noqa: BLE001
        print(f"[lights] stop FAILED: {exc}")


def _lights_available() -> bool:
    """Fast liveness probe so the user-visible confirmation is honest."""
    try:
        return httpx.get(f"{LIGHTS_API}/state", timeout=2).status_code == 200
    except Exception:  # noqa: BLE001
        return False


def apply_lights_directives(reply: str) -> str:
    """Extract <lights>...</lights> / <lights-off/> directives from a model
    reply, dispatch them to the lightserver in background threads, and return
    the reply with the tags stripped and a short status note appended.

    Never raises — a lights failure degrades to "(lights: unavailable)" text,
    never a broken chat reply."""
    try:
        vibes = [m.strip() for m in _LIGHTS_TAG_RE.findall(reply) if m.strip()]
        wants_off = bool(_LIGHTS_OFF_RE.search(reply))
        if not vibes and not wants_off:
            return reply

        cleaned = _LIGHTS_TAG_RE.sub("", reply)
        cleaned = _LIGHTS_OFF_RE.sub("", cleaned)
        cleaned = re.sub(r"\n{3,}", "\n\n", cleaned).strip()

        if _lights_available():
            if wants_off:
                threading.Thread(target=_lights_off_worker,
                                 name="lights-off", daemon=True).start()
            for vibe in vibes:
                threading.Thread(target=_lights_prompt_worker, args=(vibe,),
                                 name="lights-prompt", daemon=True).start()
            note = "(lights: done)"
        else:
            note = "(lights: unavailable)"

        return f"{cleaned} {note}".strip() if cleaned else note
    except Exception as exc:  # noqa: BLE001 - lights must never break chat
        print(f"[lights] directive handling failed: {exc}")
        return reply

# ---------------------------------------------------------------------------
# Voice config — STT (faster-whisper) + TTS (espeak-ng)
# ---------------------------------------------------------------------------

# Whisper model used for speech-to-text. Runs fully locally on the Pi via
# faster-whisper/CTranslate2 — no API key, no network dependency at request
# time (the weights are downloaded once and cached under ~/.cache/huggingface).
#
# `base.en` is the accuracy/latency sweet spot on a Pi 4 CPU for short
# conversational clips. Swap this constant to change engines:
#   tiny.en   -> ~2x faster than base.en, noticeably more transcription errors
#   small.en  -> more accurate than base.en, but several seconds slower per clip
#   medium.en -> most accurate, far too slow for interactive use on a Pi 4
WHISPER_MODEL_NAME = os.environ.get("WHISPER_MODEL", "base.en")

# int8 quantization is a large speedup on ARM CPUs for negligible accuracy loss.
WHISPER_COMPUTE_TYPE = "int8"
WHISPER_DEVICE = "cpu"

# --- Cloud STT: Groq-hosted Whisper -----------------------------------------
#
# The Pi 4 CPU is a weak Whisper host: `base.en` runs at roughly 1.8x realtime
# here AND still mis-hears conversational phrasing, while `small.en` is
# accurate but takes 13-18s per clip (unusable interactively). Groq serves
# `whisper-large-v3-turbo` on their LPU hardware far faster than realtime, so
# when an API key is available we get both better accuracy and lower latency
# than anything this box can do locally.
#
# The endpoint is OpenAI-compatible: POST multipart with `file` + `model`,
# `Authorization: Bearer <key>`, response `{"text": "..."}`.
#
# Engine selection is evaluated PER REQUEST (see active_stt_engine()), never
# frozen at import time, so dropping a key into .env + restarting is the only
# step needed to switch -- and STT_ENGINE=local force-disables cloud STT
# without removing the key.
GROQ_STT_URL = "https://api.groq.com/openai/v1/audio/transcriptions"
GROQ_STT_MODEL = os.environ.get("GROQ_STT_MODEL", "whisper-large-v3-turbo")
GROQ_STT_TIMEOUT_SECONDS = float(os.environ.get("GROQ_STT_TIMEOUT_SECONDS", "30"))

# Max upload size accepted by the voice endpoints (guards against someone
# posting a huge file at the publicly-funneled surface).
MAX_AUDIO_BYTES = 25 * 1024 * 1024  # 25 MB
FFMPEG_TIMEOUT_SECONDS = 60

# Text-to-speech. espeak-ng is only the SOURCE stage: its output is then run
# through the SPEAK engine's TI TMS5100 recolor (engine/ticolor.py) so the
# agent actually sounds like the Speak-and-Spell its persona claims. Shipping
# raw espeak-ng, as this used to, gave it no TI-LPC character at all.
#
# VOICE SELECTION -- deliberate, not a default:
#   Bare `en-us` is an AMBIGUOUS voice name on this box. MBROLA is installed
#   and registers `mb/mb-us1` (--/F, female) under the same `en-us` identifier
#   alongside `gmw/en-US` (--/M). Pinning the explicitly male espeak variant
#   `+m3` (!v/m3) removes any chance of voice resolution landing on a female
#   voice. Measured median f0 of the selected voice is ~85 Hz (clearly male);
#   `en-us+f2` for comparison measures ~154 Hz.
ESPEAK_BIN = "espeak-ng"
ESPEAK_PITCH = os.environ.get("TTS_PITCH", "25")  # 0-99, default 50. Lower = deeper.
# Words per minute. 150 is the baseline and the value that ships.
#
# DO NOT RAISE THIS WITHOUT LISTENING FIRST. 210 wpm (1.4x) was tried and
# rejected in testing: unintelligible. Rate and TI-LPC crunch compound --
# ti_color() decimates to 8 kHz, bandpasses to 300-3400 Hz and quantises the
# LPC reflection coefficients, all of which already cost intelligibility, so
# there is far less headroom here than with a clean TTS voice. If the voice is
# hard to parse, reduce TI_AMOUNT before touching the speed.
ESPEAK_SPEED = os.environ.get("TTS_SPEED_WPM", "150")
ESPEAK_VOICE = os.environ.get("TTS_VOICE", "en-us+m3")
ESPEAK_TIMEOUT_SECONDS = 30
MAX_TTS_CHARS = 2000

# "ti"  -> espeak-ng piped through ti_color() (the real voice; default)
# "raw" -> raw espeak-ng bytes, for debugging the source stage in isolation
TTS_MODE = os.environ.get("TTS_MODE", "ti").strip().lower()

# ti_color() coloration strength. The engine defaults (amount=1.0,
# chirp_gain=0.55) were tuned against macOS Fred, which is a much fuller,
# warmer source than espeak-ng. espeak is already thin and buzzy, so the same
# settings stack crunch on crunch; these are dialled back.
TI_AMOUNT = float(os.environ.get("TTS_TI_AMOUNT", "0.65"))
TI_CHIRP_GAIN = float(os.environ.get("TTS_TI_CHIRP_GAIN", "0.35"))

# ---------------------------------------------------------------------------
# Singing
# ---------------------------------------------------------------------------
# Rendering a song runs the pyworld vocoder per syllable and takes ~15s for
# daisy on this Pi -- too slow to sit inside an interactive request every time,
# so renders are cached to disk (keyed on the .song/.mid mtimes) and daisy is
# pre-rendered in the background at startup.
SONGS_DIR = SPEAK_ROOT / "songs"
SING_CACHE_DIR = SPEAK_ROOT / "server" / "cache"
DEFAULT_SONG = "daisy"          # user: sing daisy by default, always
SING_TIMEOUT_SECONDS = 300

# ---------------------------------------------------------------------------
# Persona + memory loading
# ---------------------------------------------------------------------------


def _read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        return ""


def build_system_prompt_legacy() -> str:
    """Read persona.md + memory/facts.md + memory/lore.md + memory/history.md
    fresh and combine them into a single system prompt. Read at startup (and
    reusable if we ever want to hot-reload memory between sessions).

    persona.md stays first and unchanged — it's the authoritative identity
    definition. history.md (a consolidated summary of the user's past
    Claude Code conversations) is supplementary background context only,
    clearly labeled and kept separate from live persona/facts/lore.
    skills/room-lights/SKILL.md is the room lighting control skill —
    always loaded so the model can actually execute lighting commands
    (via curl, see the allowedTools scoping on the claude CLI call below)
    without needing it re-taught every turn."""
    persona = _read_text(PERSONA_PATH)
    facts = _read_text(FACTS_PATH)
    lore = _read_text(LORE_PATH)
    history = _read_text(HISTORY_PATH)
    room_lights_skill = _read_text(ROOM_LIGHTS_SKILL_PATH)
    brain_skill = _read_text(BRAIN_SKILL_PATH)
    personality_skill = _read_text(PERSONALITY_SKILL_PATH)

    parts = [persona]
    if facts:
        parts.append(f"## Live memory: facts\n\n{facts}")
    if lore:
        parts.append(f"## Live memory: lore\n\n{lore}")
    if history:
        parts.append(f"## Background history (supplementary context, not identity-defining)\n\n{history}")
    if room_lights_skill:
        parts.append(f"## Skill: Room Lighting Control\n\n{room_lights_skill}")
    if brain_skill:
        parts.append(f"## Skill: Brain Switching\n\n{brain_skill}")
    if personality_skill:
        parts.append(f"## Skill: Personality Input\n\n{personality_skill}")

    return "\n\n---\n\n".join(p for p in parts if p)


def build_system_prompt() -> str:
    """TIER 0 — the always-loaded, turn-stable system prompt.

    Supersedes build_system_prompt_legacy() above, which concatenated persona +
    facts + lore + history + every skill doc into one 59 KB blob that was
    re-sent and re-processed on every turn and grew without bound.

    This returns only what the agent needs to *be itself and know what it can
    do*: persona.md, the pinned identity facts, a one-line summary of every
    memory branch plus an index of the deeper keys, and the skills index (with
    small skills resident in full). Everything else — warm leaves, lore,
    history, large skill docs — is retrieved per turn by context_tiers.
    build_turn_context() and travels in the user message instead.

    Why the split lands here rather than "just send less every turn": the
    system prompt is a spawn-time argv flag for the warm `claude` process, and
    warm_brain kills and respawns that process whenever this string changes. So
    this string IS the session init cost, and it must stay stable turn to turn.
    See context_tiers.py for the full argument.

    The lights-directive spec (LIGHTS_SPEC) is appended here: it is small,
    static and always applicable, so it belongs in the turn-stable Tier 0
    prompt rather than per-turn retrieval.
    """
    return context_tiers.build_system_prompt() + "\n\n---\n\n" + LIGHTS_SPEC


def build_local_system_prompt() -> str:
    """Reduced system prompt for the LOCAL on-Pi brain.

    WHY IT IS DIFFERENT — this is the whole reason the local path is usable at
    all. Prefill on this Cortex-A72 runs in the single-digit tokens/sec. The
    cloud system prompt is ~50k chars / ~13k tokens, i.e. tens of minutes of
    prefill before the first token. This prompt is ~5k chars / ~1.3k tokens.

    WHAT THE LOCAL BRAIN THEREFORE DOES NOT SEE, versus cloud:
      * persona.md          -> replaced by persona_local.md, a hand-condensed
                               version keeping identity, voice, response-length
                               rules, the hard rules and the tool list, and
                               dropping the long cosmology / aesthetics /
                               self-correction essays.
      * memory/history.md   -> dropped entirely (~15k chars of summarised past
                               Claude Code sessions). The local brain has no
                               background on any of that.
      * skills/room-lights  -> dropped (~11.5k chars). The local model has no
                               shell and cannot run curl, so the full skill
                               would be unusable to it anyway; ~10 lines in
                               persona_local.md drive the backend-executed
                               set_lights/get_lights tools instead.
      * the per-turn retrieved context block assembled in handle_chat_message
                            -> the local path gets the rolling transcript only.

    It DOES see facts.md and lore.md in full, plus the brain-switching skill,
    and it emits the SAME tool JSON into the SAME memory_tools.process_reply().
    One memory, two brains — nothing about the memory system is forked.

    ORDERING IS DELIBERATE: static content (persona, skill) first, volatile
    content (facts, lore) last. llama-server's prefix KV cache can only reuse
    an exact leading prefix, so keeping the mutable parts at the END means a
    memory write invalidates only the tail instead of forcing a full
    re-prefill of everything on the next turn.
    """
    persona = _read_text(PERSONA_LOCAL_PATH) or _read_text(PERSONA_PATH)
    brain_skill = _read_text(BRAIN_SKILL_PATH)
    facts = _read_text(FACTS_PATH)
    lore = _read_text(LORE_PATH)

    parts = [persona]
    if brain_skill:
        parts.append(f"## Skill: Brain Switching\n\n{brain_skill}")
    if facts:
        parts.append(f"## Live memory: facts\n\n{facts}")
    if lore:
        parts.append(f"## Live memory: lore\n\n{lore}")
    return "\n\n---\n\n".join(p for p in parts if p)


# NOTE: the prompt is rebuilt per request (see handle_chat_message) rather than
# frozen here, because the agent's own write_memory/write_lore calls now change
# facts.md/lore.md mid-process. Freezing it at import meant a fact written on
# one turn stayed invisible to the next turn until a service restart. This
# module-level value is kept only as a startup sanity check / legacy reference.
SYSTEM_PROMPT = build_system_prompt()

# ---------------------------------------------------------------------------
# Claude CLI brain
# ---------------------------------------------------------------------------

NOT_AUTHENTICATED_MSG = (
    "Server's brain isn't authenticated yet — run `claude auth login` on the Pi first."
)


def claude_is_authenticated() -> bool:
    try:
        result = subprocess.run(
            [CLAUDE_BIN, "auth", "status"],
            capture_output=True,
            text=True,
            timeout=15,
        )
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        return False

    out = (result.stdout or "") + (result.stderr or "")
    return '"loggedIn": true' in out or '"loggedIn":true' in out


def call_claude_brain(system_prompt: str, user_message: str) -> str:
    """Non-interactive one-shot call to the Claude Code CLI.

    Invocation:
        claude -p --allowedTools "Bash(curl *)" --system-prompt "<persona+memory>" \
               --output-format json "<user message>"

    -p/--print          -> non-interactive, print response and exit
    --allowedTools "Bash(curl *)"
                         -> lets the CLI actually execute tool calls (e.g. the
                            room-lights skill's curl commands against
                            fcob-strip.local) without blocking on an
                            interactive permission prompt it can never get an
                            answer to in this non-interactive/headless
                            context. Scoped as narrowly as possible: Bash is
                            allowed ONLY for commands starting with `curl` —
                            no general shell access, no file writes, no other
                            tools. Deliberately NOT using
                            --allow-dangerously-skip-permissions (which would
                            grant unrestricted tool access) since this
                            endpoint is reachable from the public internet via
                            Tailscale Funnel.
    --system-prompt      -> use persona+memory verbatim as the system prompt
                             (replaces default system prompt entirely)
    --output-format json -> structured single JSON result on stdout, easier
                             to parse reliably than raw text
    """
    try:
        result = subprocess.run(
            [
                CLAUDE_BIN,
                "-p",
                "--allowedTools",
                "Bash(curl *)",
                "--system-prompt",
                system_prompt,
                "--output-format",
                "json",
            ]
            + (["--effort", warm_brain.CLAUDE_EFFORT] if warm_brain.CLAUDE_EFFORT else [])
            + [
                user_message,
            ],
            # The CLI waits up to 3s for piped stdin before giving up (it
            # prints "no stdin data received in 3s"). Under systemd stdin is
            # already /dev/null so this is a no-op in production, but pinning
            # it means running the server straight from a shell doesn't
            # silently add 3s to every single reply.
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            timeout=CLAUDE_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        return "Server's brain took too long to respond (timeout). Try again."
    except (FileNotFoundError, OSError) as exc:
        return f"Server's brain couldn't be reached: {exc}"

    stdout = (result.stdout or "").strip()
    stderr = (result.stderr or "").strip()
    combined = f"{stdout}\n{stderr}".lower()

    auth_error_markers = ("not logged in", "please run", "claude auth login", "invalid api key", "authentication")
    if result.returncode != 0 and any(marker in combined for marker in auth_error_markers):
        return NOT_AUTHENTICATED_MSG

    if not stdout:
        return NOT_AUTHENTICATED_MSG if result.returncode != 0 else "Server's brain returned an empty response."

    # --output-format json returns a single JSON object; the reply text is
    # under "result" for `claude -p --output-format json`.
    try:
        import json

        data = json.loads(stdout)
        if isinstance(data, dict):
            if data.get("is_error"):
                text = str(data.get("result", "")) or str(data)
                if "not logged in" in text.lower() or "login" in text.lower():
                    return NOT_AUTHENTICATED_MSG
                return f"Server's brain returned an error: {text}"
            reply = data.get("result")
            if reply:
                return str(reply)
    except (ValueError, TypeError):
        pass

    # Fallback: not valid/expected JSON — return raw stdout as a best effort.
    return stdout


# ---------------------------------------------------------------------------
# Warm brain wrapper (latency)
# ---------------------------------------------------------------------------
#
# `call_claude_brain` above spawns a fresh `claude` process per message. On this
# Pi that costs ~2.5-3.0s of Node/CLI startup before any thinking happens —
# measured as roughly half the latency of a short reply. `warm_brain` keeps one
# long-lived `claude -p --input-format stream-json` process per chat session and
# feeds turns into it, paying that startup once.
#
# brain_call() is the ONLY thing the request path should use: it tries the warm
# pool and silently falls back to the original one-shot subprocess on any
# problem, so warm-path breakage costs latency, never a failed reply.
# WARM_BRAIN=false in .env disables the warm path entirely.


def _warm():
    return warm_brain.get_warm_brain(CLAUDE_BIN, "Bash(curl *)", str(SPEAK_ROOT))


def brain_call(
    session_id: str,
    system_prompt: str,
    fresh_prompt: str,
    followup_message: str,
) -> str:
    """One brain turn, warm if possible.

    fresh_prompt      -> sent when a new process has to be started (full rolling
                         transcript, identical to what the cold path sends).
    followup_message  -> sent when the process is already warm and is holding
                         the conversation itself (just the new turn).
    """
    # --- LOCAL BRAIN ------------------------------------------------------
    # When the selected brain is "local", the turn is answered by a llama.cpp
    # model running on this Pi's own CPU instead of the claude CLI. It is given
    # the reduced system prompt (see build_local_system_prompt) and the full
    # rolling transcript, because unlike the warm claude pool it holds no
    # session state of its own — every local request is stateless, and only the
    # KV prefix cache carries over.
    #
    # GRACEFUL FAILURE: call_local_brain returns None for every failure mode —
    # weights or binary missing, llama-server failed to start, HTTP error,
    # timeout, or SoC above the thermal abort threshold — and we fall straight
    # through to the cloud path below with a log line, mirroring how the Groq
    # STT path falls back to local Whisper. A local failure costs latency,
    # never a failed reply.
    if local_brain.get_brain() == "local":
        local_started = time.time()
        local_reply = local_brain.call_local_brain(
            build_local_system_prompt(), fresh_prompt
        )
        if local_reply is not None:
            local_brain.mark_brain_used("local")
            print(f"[latency] brain path=local took={time.time() - local_started:.2f}s")
            return local_reply
        print("[brain] local brain unavailable this turn — falling back to cloud")
    local_brain.mark_brain_used("cloud")

    started = time.time()
    path = "cold"
    reply = None
    try:
        reply = _warm().ask(session_id, system_prompt, fresh_prompt, followup_message)
        if reply is not None:
            path = "warm"
    except Exception as exc:  # noqa: BLE001 - warm path must never break chat
        print(f"[warm] unexpected error, falling back to cold path: {exc}")
        reply = None

    if reply is None:
        reply = call_claude_brain(system_prompt, fresh_prompt)

    print(f"[latency] brain path={path} took={time.time() - started:.2f}s")
    return reply



# ---------------------------------------------------------------------------
# Speech-to-text: local faster-whisper
# ---------------------------------------------------------------------------


class VoiceError(Exception):
    """Raised for expected/recoverable voice failures (bad audio, ffmpeg or
    espeak-ng failure, empty transcript). Endpoints turn these into clean JSON
    error responses rather than letting them surface as a 500 stack trace."""

    def __init__(self, message: str, status_code: int = 400) -> None:
        super().__init__(message)
        self.message = message
        self.status_code = status_code


# The Whisper model is expensive to construct (seconds), so it is built ONCE
# and reused for every request — never per-request. `_whisper_lock` serializes
# transcription because a single CTranslate2 model instance is not safe to
# drive from multiple threads at once, and FastAPI runs sync endpoints in a
# threadpool.
_whisper_model = None
_whisper_lock = threading.Lock()
_whisper_load_lock = threading.Lock()


def get_whisper_model():
    """Return the process-wide Whisper model, constructing it on first use.

    Loaded eagerly at startup (see the startup hook below) so the first real
    request doesn't pay the load cost. Kept lazy-capable so that a model-load
    failure degrades only the voice endpoints — the existing text /api/chat
    path keeps working regardless.
    """
    global _whisper_model
    if _whisper_model is not None:
        return _whisper_model

    with _whisper_load_lock:
        if _whisper_model is not None:
            return _whisper_model
        try:
            from faster_whisper import WhisperModel
        except Exception as exc:  # noqa: BLE001
            raise VoiceError(f"speech-to-text engine unavailable: {exc}", 503) from exc

        try:
            started = time.time()
            model = WhisperModel(
                WHISPER_MODEL_NAME,
                device=WHISPER_DEVICE,
                compute_type=WHISPER_COMPUTE_TYPE,
            )
            print(
                f"[voice] whisper model '{WHISPER_MODEL_NAME}' "
                f"({WHISPER_COMPUTE_TYPE}) loaded in {time.time() - started:.2f}s"
            )
        except Exception as exc:  # noqa: BLE001
            raise VoiceError(f"failed to load speech-to-text model: {exc}", 503) from exc

        _whisper_model = model
        return _whisper_model


def _groq_api_key() -> str:
    """Read the Groq key at call time, not import time.

    os.environ is consulted on every request so the key can be added to .env
    and picked up with a plain service restart (load_dotenv populates
    os.environ at startup) without any code change.
    """
    return (os.environ.get("GROQ_API_KEY") or "").strip()


def active_stt_engine() -> str:
    """Which STT engine WOULD serve a request right now: "groq" or "local".

    STT_ENGINE (auto|groq|local, default auto) is an explicit override for
    testing:
      auto  -> groq if GROQ_API_KEY is set and non-empty, else local
      groq  -> force cloud (still falls back to local if the call fails)
      local -> force local faster-whisper, even if a key is present

    A forced "groq" with no key at all cannot possibly authenticate, so that
    degenerate case reports/uses local rather than guaranteeing a failed round
    trip on every request.
    """
    preference = (os.environ.get("STT_ENGINE") or "auto").strip().lower()
    if preference == "local":
        return "local"
    if preference == "groq":
        return "groq" if _groq_api_key() else "local"
    return "groq" if _groq_api_key() else "local"


def _transcribe_via_groq(wav_path: str) -> str:
    """Transcribe a normalized WAV via Groq's OpenAI-compatible endpoint.

    Raises (any exception) on network error, timeout, non-200, or a response
    body without usable text -- the caller treats ANY failure here as a signal
    to fall back to local Whisper, so this deliberately does not try to
    classify or recover from errors itself.
    """
    api_key = _groq_api_key()
    if not api_key:
        raise RuntimeError("GROQ_API_KEY is not set")

    with open(wav_path, "rb") as fh:
        response = httpx.post(
            GROQ_STT_URL,
            headers={"Authorization": f"Bearer {api_key}"},
            files={"file": ("audio.wav", fh, "audio/wav")},
            data={
                "model": GROQ_STT_MODEL,
                "response_format": "json",
                # The audio is already known-English 16k mono; pinning the
                # language skips Groq's language-detection pass.
                "language": "en",
            },
            timeout=GROQ_STT_TIMEOUT_SECONDS,
        )

    if response.status_code != 200:
        detail = (response.text or "").strip()[:300]
        raise RuntimeError(f"groq returned HTTP {response.status_code}: {detail}")

    data = response.json()  # raises on a malformed/non-JSON body -> fallback
    if not isinstance(data, dict) or "text" not in data:
        raise RuntimeError(f"groq returned an unexpected body: {str(data)[:200]}")

    text = str(data.get("text") or "").strip()
    if not text:
        # Could be genuine silence, could be a bad response. Let local Whisper
        # arbitrate -- if the clip really is silent it raises "no speech
        # detected" and the user gets the correct error either way.
        raise RuntimeError("groq returned an empty transcript")
    return text


def _transcribe_via_local(wav_path: str) -> tuple[str, float]:
    """Transcribe a normalized WAV with the in-process faster-whisper model.

    Returns (text, duration_s). Raises VoiceError on engine failure.
    """
    model = get_whisper_model()
    try:
        with _whisper_lock:
            segments, info = model.transcribe(
                wav_path,
                beam_size=1,          # greedy: materially faster on Pi CPU
                vad_filter=True,      # drop silence so we don't decode dead air
                language="en",
            )
            text = "".join(segment.text for segment in segments).strip()
    except VoiceError:
        raise
    except Exception as exc:  # noqa: BLE001
        raise VoiceError(f"transcription failed: {exc}", 500) from exc

    return text, float(getattr(info, "duration", 0.0) or 0.0)


def _wav_duration_seconds(wav_path: str) -> float:
    """Clip length of a normalized WAV, read straight from its header.

    The Groq path does no local decode, so duration_s is derived here instead
    and stays meaningful whichever engine served the request.
    """
    try:
        with wave.open(wav_path, "rb") as wf:
            rate = wf.getframerate()
            if rate:
                return wf.getnframes() / float(rate)
    except Exception:  # noqa: BLE001 - duration is informational, never fatal
        pass
    return 0.0


def _convert_to_wav16k_mono(src_path: str, dst_path: str) -> None:
    """Transcode arbitrary browser-recorded audio to the 16 kHz mono WAV that
    faster-whisper expects.

    Browsers hand us whatever their MediaRecorder produces — WebM/Opus on
    Android Chrome, MP4/AAC on iOS Safari, sometimes Ogg. Shelling out to
    ffmpeg is by far the most robust way to normalize that zoo of codecs.
    """
    if not shutil.which("ffmpeg"):
        raise VoiceError("audio conversion unavailable: ffmpeg is not installed", 503)

    try:
        result = subprocess.run(
            [
                "ffmpeg",
                "-hide_banner",
                "-loglevel", "error",
                "-nostdin",
                "-y",
                "-i", src_path,
                "-ar", "16000",
                "-ac", "1",
                "-f", "wav",
                dst_path,
            ],
            capture_output=True,
            text=True,
            timeout=FFMPEG_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as exc:
        raise VoiceError("audio conversion timed out", 504) from exc
    except OSError as exc:
        raise VoiceError(f"audio conversion failed to start: {exc}", 500) from exc

    if result.returncode != 0:
        detail = (result.stderr or "").strip().splitlines()
        hint = detail[-1] if detail else "unknown ffmpeg error"
        raise VoiceError(f"could not decode the uploaded audio: {hint}", 400)

    if not os.path.exists(dst_path) or os.path.getsize(dst_path) == 0:
        raise VoiceError("could not decode the uploaded audio (empty result)", 400)


def transcribe_audio_bytes(raw: bytes, filename: str = "") -> dict:
    """Full STT pipeline: raw upload bytes -> normalized WAV -> transcript.

    Routes to Groq-hosted Whisper when a key is configured, otherwise to the
    local faster-whisper model -- and ALWAYS falls back to local if the cloud
    call fails for any reason, so a network blip degrades quality instead of
    breaking voice.

    Returns {"text", "duration_s", "took_s", "engine"} where engine is the one
    that actually produced the text ("groq" or "local"). Raises VoiceError
    with a human-readable message for every expected failure mode. Temp files
    are always cleaned up.
    """
    if not raw:
        raise VoiceError("no audio data received", 400)
    if len(raw) > MAX_AUDIO_BYTES:
        raise VoiceError(
            f"audio too large ({len(raw)} bytes, max {MAX_AUDIO_BYTES})", 413
        )

    started = time.time()
    # Preserve the original extension when we can — it gives ffmpeg a hint for
    # container formats whose magic bytes are ambiguous.
    suffix = Path(filename).suffix if filename else ""
    if len(suffix) > 8:
        suffix = ""

    src_fd, src_path = tempfile.mkstemp(prefix="voice_in_", suffix=suffix or ".bin")
    dst_fd, dst_path = tempfile.mkstemp(prefix="voice_out_", suffix=".wav")
    os.close(dst_fd)

    try:
        with os.fdopen(src_fd, "wb") as fh:
            fh.write(raw)

        # Normalize to 16k mono WAV even for the cloud path. Groq accepts
        # common formats directly, but normalizing guarantees a codec it can
        # decode AND shrinks the upload (a phone's WebM/Opus or MP4/AAC blob
        # re-encoded to 16k mono is what actually crosses the wire), which
        # matters most on the mobile connections this gets used from.
        _convert_to_wav16k_mono(src_path, dst_path)

        duration_s = _wav_duration_seconds(dst_path)
        engine = active_stt_engine()
        text = ""

        if engine == "groq":
            try:
                text = _transcribe_via_groq(dst_path)
            except Exception as exc:  # noqa: BLE001 - ANY failure -> local
                # Network error, timeout, 401/429/5xx, malformed body: log it
                # and quietly serve the request from the local model. The user
                # gets a slightly worse transcript, never a hard failure.
                print(f"[voice] groq STT failed, falling back to local: {exc}")
                engine = "local"

        if engine == "local":
            text, local_duration = _transcribe_via_local(dst_path)
            if not duration_s:
                duration_s = local_duration

        if not text:
            raise VoiceError("no speech detected in the audio", 422)

        return {
            "text": text,
            "duration_s": round(duration_s, 2),
            "took_s": round(time.time() - started, 2),
            "engine": engine,
        }
    finally:
        for path in (src_path, dst_path):
            try:
                os.unlink(path)
            except OSError:
                pass


# ---------------------------------------------------------------------------
# Text-to-speech: espeak-ng
# ---------------------------------------------------------------------------


def _espeak_wav_bytes(text: str) -> bytes:
    """SOURCE stage only: render `text` to WAV bytes with espeak-ng (see the
    ESPEAK_* constants). This is thin/robotic but is NOT yet the S&S voice --
    synthesize_speech() runs it through ti_color(). Temp file always cleaned."""
    text = (text or "").strip()
    if not text:
        raise VoiceError("no text provided to speak", 400)
    if len(text) > MAX_TTS_CHARS:
        raise VoiceError(f"text too long ({len(text)} chars, max {MAX_TTS_CHARS})", 413)

    if not shutil.which(ESPEAK_BIN):
        raise VoiceError(
            f"text-to-speech unavailable: {ESPEAK_BIN} is not installed", 503
        )

    fd, wav_path = tempfile.mkstemp(prefix="voice_tts_", suffix=".wav")
    os.close(fd)
    try:
        try:
            result = subprocess.run(
                [
                    ESPEAK_BIN,
                    "-v", ESPEAK_VOICE,
                    "-p", ESPEAK_PITCH,
                    "-s", ESPEAK_SPEED,
                    "-w", wav_path,
                    "--", text,  # `--` so text starting with '-' isn't read as a flag
                ],
                capture_output=True,
                text=True,
                timeout=ESPEAK_TIMEOUT_SECONDS,
            )
        except subprocess.TimeoutExpired as exc:
            raise VoiceError("speech synthesis timed out", 504) from exc
        except OSError as exc:
            raise VoiceError(f"speech synthesis failed to start: {exc}", 500) from exc

        if result.returncode != 0:
            hint = (result.stderr or "").strip() or "unknown espeak-ng error"
            raise VoiceError(f"speech synthesis failed: {hint}", 500)

        try:
            audio = Path(wav_path).read_bytes()
        except OSError as exc:
            raise VoiceError(f"could not read synthesized audio: {exc}", 500) from exc

        if not audio:
            raise VoiceError("speech synthesis produced empty audio", 500)

        return audio
    finally:
        try:
            os.unlink(wav_path)
        except OSError:
            pass


def _wav_bytes_from_samples(samples, sr: int) -> bytes:
    """float32 mono numpy -> 16-bit PCM WAV bytes."""
    import io
    import numpy as np
    import soundfile as sf

    buf = io.BytesIO()
    sf.write(buf, np.asarray(samples, dtype="float32"), sr,
             format="WAV", subtype="PCM_16")
    return buf.getvalue()


def apply_ti_color(wav_bytes: bytes) -> bytes:
    """Run WAV bytes through the SPEAK engine's TI TMS5100 recolor.

    Reuses engine/ticolor.py verbatim -- the DSP is source-agnostic, only the
    old routing was macOS-only. Returns 8 kHz mono WAV bytes.
    """
    import io
    import numpy as np
    import soundfile as sf
    from engine.ticolor import ti_color

    data, sr = sf.read(io.BytesIO(wav_bytes), dtype="float32", always_2d=False)
    if data.ndim > 1:
        data = data.mean(axis=1).astype(np.float32)
    colored, out_sr = ti_color(data, sr, amount=TI_AMOUNT,
                               chirp_gain=TI_CHIRP_GAIN)
    return _wav_bytes_from_samples(colored, out_sr)


def synthesize_speech(text: str) -> bytes:
    """Text -> Speak-and-Spell WAV bytes.

    espeak-ng (source) -> ti_color() (TI TMS5100 LPC recolor, 8 kHz).
    TTS_MODE=raw short-circuits to the espeak stage for debugging.
    """
    raw = _espeak_wav_bytes(text)
    if TTS_MODE == "raw":
        return raw
    try:
        return apply_ti_color(raw)
    except Exception as exc:  # noqa: BLE001
        # Never hand back silence: if the DSP stack is broken, the corrected
        # (male, non-default) espeak voice is still a usable fallback -- but
        # say so loudly in the log, because it is NOT the real voice.
        print(f"[voice] TI recolor FAILED, serving raw espeak: {exc}")
        return raw


# ---------------------------------------------------------------------------
# Singing: MIDI + lyrics -> WAV via engine/singer.py, cached on disk
# ---------------------------------------------------------------------------

_sing_lock = threading.Lock()


def list_songs() -> list[str]:
    try:
        return sorted(p.stem for p in SONGS_DIR.glob("*.song"))
    except OSError:
        return []


def resolve_song(name: Optional[str]) -> tuple[str, Path, bool]:
    """Map a requested song name to an actual .song file.

    Returns (slug, path, substituted). Per the user's instruction daisy is the
    default and also the fallback: an unknown song never errors, it becomes
    daisy with substituted=True so the reply can mention the swap.
    """
    requested = (name or "").strip().lower()
    slug = "".join(c for c in requested if c.isalnum() or c in "-_")
    if slug:
        candidate = SONGS_DIR / f"{slug}.song"
        if candidate.is_file():
            return slug, candidate, False
    default = SONGS_DIR / f"{DEFAULT_SONG}.song"
    if not default.is_file():
        raise VoiceError(f"no songs available (missing {default})", 503)
    return DEFAULT_SONG, default, bool(slug) and slug != DEFAULT_SONG


def _cache_path(slug: str, song_path: Path) -> Path:
    """Cache file keyed on the song's own mtime plus its MIDI's, so editing
    either invalidates the render automatically."""
    stamp = int(song_path.stat().st_mtime)
    try:
        from engine.songfile import load_song
        midi = Path(load_song(str(song_path)).midi)
        if midi.is_file():
            stamp = max(stamp, int(midi.stat().st_mtime))
    except Exception:  # noqa: BLE001
        pass
    return SING_CACHE_DIR / f"{slug}-{stamp}.wav"


def render_song_wav(name: Optional[str] = None) -> dict:
    """Render (or serve from cache) a song as WAV bytes.

    Singing tempo comes from the .song file's MIDI -- it deliberately ignores
    the speech ESPEAK_SPEED constant so speeding up speech never speeds up
    songs.
    """
    slug, song_path, substituted = resolve_song(name)
    cache = _cache_path(slug, song_path)

    if cache.is_file():
        return {"song": slug, "substituted": substituted, "cached": True,
                "took_s": 0.0, "audio": cache.read_bytes()}

    started = time.time()
    # One render at a time: the vocoder is CPU-bound and two concurrent renders
    # would just fight over the Pi's cores.
    with _sing_lock:
        if cache.is_file():  # another request rendered it while we waited
            return {"song": slug, "substituted": substituted, "cached": True,
                    "took_s": 0.0, "audio": cache.read_bytes()}
        try:
            from engine.songfile import render_song_file
            samples, sr = render_song_file(str(song_path))
        except Exception as exc:  # noqa: BLE001
            raise VoiceError(f"song rendering failed: {exc}", 500) from exc
        if samples is None or len(samples) == 0:
            raise VoiceError("song rendering produced empty audio", 500)
        audio = _wav_bytes_from_samples(samples, sr)
        try:
            SING_CACHE_DIR.mkdir(parents=True, exist_ok=True)
            tmp = cache.with_suffix(".wav.tmp")
            tmp.write_bytes(audio)
            tmp.replace(cache)
            for stale in SING_CACHE_DIR.glob(f"{slug}-*.wav"):
                if stale != cache:
                    stale.unlink(missing_ok=True)
        except OSError as exc:
            print(f"[sing] could not cache {slug}: {exc}")
    return {"song": slug, "substituted": substituted, "cached": False,
            "took_s": round(time.time() - started, 2), "audio": audio}


# ---------------------------------------------------------------------------
# Auth: bearer token + per-IP lockout
# ---------------------------------------------------------------------------

MAX_ATTEMPTS = 5
WINDOW_SECONDS = 300  # 5 min window to accumulate failures
LOCKOUT_SECONDS = 900  # 15 min lockout once tripped

_failures: dict[str, list[float]] = defaultdict(list)


def _client_ip(request: Request) -> str:
    # Behind Tailscale Funnel, the real client IP arrives via
    # X-Forwarded-For; fall back to the direct peer address for local tests.
    xff = request.headers.get("x-forwarded-for")
    if xff:
        return xff.split(",")[0].strip()
    return request.client.host if request.client else "unknown"


def _is_locked_out(ip: str) -> bool:
    now = time.time()
    _failures[ip] = [t for t in _failures[ip] if now - t < LOCKOUT_SECONDS]
    return len(_failures[ip]) >= MAX_ATTEMPTS


def _record_failure(ip: str) -> None:
    _failures[ip].append(time.time())


def _clear_failures(ip: str) -> None:
    _failures.pop(ip, None)


async def require_auth(request: Request, authorization: str = Header(default="")) -> None:
    if not AUTH_ENABLED:
        # Auth gate disabled via SERVER_AUTH_ENABLED=false (default). Flip
        # that env var to "true" and restart the service to re-enable the
        # bearer-token check below without touching any other code.
        return

    ip = _client_ip(request)

    if _is_locked_out(ip):
        raise HTTPException(status_code=429, detail="too many failed attempts, try again later")

    scheme, _, token = authorization.partition(" ")
    if scheme.lower() != "bearer" or not token or not hmac.compare_digest(token, SHARED_SECRET):
        _record_failure(ip)
        raise HTTPException(status_code=401, detail="unauthorized")

    _clear_failures(ip)


# ---------------------------------------------------------------------------
# ntfy.sh push notifications (best-effort, never blocks/breaks chat)
# ---------------------------------------------------------------------------


def _send_ntfy_notification(preview: str, ip: str) -> None:
    if not NTFY_TOPIC:
        return
    try:
        body = preview if len(preview) <= 200 else preview[:200] + "…"
        httpx.post(
            f"https://ntfy.sh/{NTFY_TOPIC}",
            content=f"{body}\n\nfrom: {ip}".encode("utf-8"),
            headers={
                "Title": "gorcave chat",
                "Priority": "default",
            },
            timeout=3,
        )
    except Exception as exc:  # noqa: BLE001 - never let notification errors break chat
        print(f"[ntfy] notification failed: {exc}")


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

app = FastAPI(title="Server", description="Chat backend for the home AI agent")

ALLOWED_ORIGINS = [
    "https://emgor.online",
]
ALLOWED_ORIGIN_REGEX = r"^http://localhost(:\d+)?$"

app.add_middleware(
    CORSMiddleware,
    allow_origins=ALLOWED_ORIGINS,
    allow_origin_regex=ALLOWED_ORIGIN_REGEX,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# session_id -> list of {"role": "user"|"assistant", "content": str}
_sessions: dict[str, list[dict[str, str]]] = defaultdict(list)


class ChatRequest(BaseModel):
    message: str
    session_id: Optional[str] = None


class ChatResponse(BaseModel):
    reply: str
    # Present only when the agent asked to sing this turn. The client should
    # POST {"song": <song>} to `url` to fetch the audio. Kept out-of-band
    # rather than inlined because a cold song render takes ~15s -- far too long
    # to hold the chat response open for.
    sing: Optional[dict] = None
    # Which brain produced this reply: "local" (on-Pi llama.cpp) or "cloud"
    # (claude CLI). Evidence for the caller, and how a switch is verified.
    brain: Optional[str] = None


@app.get("/api/health")
def health():
    return {
        "status": "ok",
        "claude_authenticated": claude_is_authenticated(),
        # Which STT engine a voice request would use right now (evaluated
        # live, so this reflects the current env rather than startup state).
        "stt_engine": active_stt_engine(),
        # Warm `claude` process pool (see warm_brain.py) — sessions,
        # pids and idle times, for latency debugging.
        "warm_brain": _warm().stats(),
        # Tiering introspection: what is resident in the system prompt vs
        # what has overflowed to per-turn retrieval as memory has grown.
        "context": context_tiers.describe(),
        # Which brain chat requests are currently routed to, and whether the
        # local model is loaded into RAM right now.
        "brain": local_brain.get_brain(),
        "local_brain": local_brain.status(),
    }


class BrainRequest(BaseModel):
    brain: str


@app.get("/api/brain")
def get_brain():
    """Current brain + local model status, without needing a conversation."""
    return {"brain": local_brain.get_brain(), "local": local_brain.status()}


@app.post("/api/brain")
def post_brain(req: BrainRequest):
    """Switch brains out-of-band. Accepts the same synonyms the brain-switching
    skill documents: fable/claude/cloud/online, local/offline/pi/on-device."""
    result = local_brain.set_brain(req.brain)
    if result.startswith("error"):
        raise HTTPException(status_code=400, detail=result)
    return {"brain": local_brain.get_brain(), "result": result}


def _detect_sing(message: str, results: list) -> Optional[dict]:
    """Did the agent ask to sing this turn? Returns {"song": slug, ...} or None.

    memory_tools.process_reply() already parses `{"tool": "sing", ...}` lines
    out of the reply and records them (as "unsupported in this context") in
    `results`, so we piggyback on that single parser rather than adding a
    second one. The song comes from the tool args if the agent named one, else
    from a song name mentioned in the user's message, else daisy.
    """
    args = None
    for name, call_args, _outcome in results:
        if name == "sing":
            args = call_args or {}
            break
    if args is None:
        return None

    requested = ""
    for key in ("song", "title", "name", "file"):
        value = args.get(key)
        if isinstance(value, str) and value.strip():
            requested = value.strip()
            break
    if not requested:
        low = (message or "").lower()
        for known in list_songs():
            if known in low:
                requested = known
                break

    try:
        slug, _path, substituted = resolve_song(requested)
    except VoiceError:
        return None
    return {"song": slug, "requested": requested or None,
            "substituted": substituted, "url": "/api/voice/sing"}


def handle_chat_message(
    message: str,
    session_id: Optional[str],
    request: Request,
    background_tasks: BackgroundTasks,
    meta: Optional[dict] = None,
) -> str:
    """The chat brain, shared by the text (/api/chat) and voice
    (/api/voice/chat) entry points.

    Appends the message to session history, fires the ntfy push notification,
    folds recent turns into the prompt, calls the Claude CLI, executes any
    memory tool calls the agent emitted, records the cleaned reply and returns
    it. Both endpoints go through here so there is exactly one copy of this
    logic — including memory persistence, which therefore works identically
    for voice and text.
    """
    session_id = session_id or "default"
    history = _sessions[session_id]

    # Classify where this request actually came from (localhost / LAN /
    # tailnet vs public Tailscale Funnel) before anything else runs. The
    # write_persona tool reads this to decide whether a persona write is
    # allowed at all — persona.md is the system prompt, so that write is
    # gated on transport provenance, never on the model's judgement.
    # Fails closed: if this ever raised, the context stays untrusted.
    try:
        persona_tools.begin_request(request, message)
    except Exception as exc:  # noqa: BLE001
        print(f"[persona] context setup failed: {exc}")
        persona_tools.begin_request(None, message)

    history.append({"role": "user", "content": message})

    try:
        # A personality input is never echoed anywhere, including off-device
        # to the ntfy push — the notification gets a placeholder instead.
        background_tasks.add_task(
            _send_ntfy_notification,
            persona_tools.redact_for_notification(message),
            _client_ip(request),
        )
    except Exception as exc:  # noqa: BLE001 - never let notification scheduling break chat
        print(f"[ntfy] failed to schedule notification: {exc}")

    # Fold recent turn history into the prompt sent to the CLI (each `claude
    # -p` call is a fresh process, so we carry conversational context
    # ourselves rather than relying on `--continue`/`--resume`).
    convo_lines = []
    for turn in history[-20:]:
        speaker = "User" if turn["role"] == "user" else "neptr"
        convo_lines.append(f"{speaker}: {turn['content']}")
    prompt = "\n".join(convo_lines)

    # The model needs the raw personality input for THIS turn (it has to
    # distill it), but it must not sit in rolling session history, where it
    # would be replayed into the prompt for the next 20 turns and could
    # surface in a later reply. Swap it for a placeholder now that the
    # prompt is built.
    if history:
        history[-1]["content"] = persona_tools.redact_for_history(message)

    # TIER 0. Rebuilt per request so a fact written last turn is in this turn's
    # prompt — but built from slow-changing sources only, so the string is
    # normally identical turn to turn and the warm process is not recycled.
    system_prompt = build_system_prompt()

    # TIER 1. Score memory/lore/history/skills against this turn and inject only
    # what scored, into the USER message rather than the system prompt —
    # deliberately, so per-turn variation never changes the system-prompt hash
    # that warm_brain uses to decide whether to keep the warm process alive.
    try:
        recent = [t["content"] for t in history[-6:-1] if t["role"] == "user"]
        turn_context, ctx_stats = context_tiers.build_turn_context(message, recent)
    except Exception as exc:  # noqa: BLE001 - retrieval must never break chat
        print(f"[context] retrieval failed, continuing without it: {exc}")
        turn_context, ctx_stats = "", {}
    if turn_context:
        print(f"[context] retrieved {ctx_stats.get('chars', 0)}c "
              f"skills={ctx_stats.get('skills')} branches={ctx_stats.get('branches')} "
              f"lore={ctx_stats.get('lore')} history={len(ctx_stats.get('history', []))}")

    fresh_prompt = f"{turn_context}\n\n{prompt}" if turn_context else prompt
    turn_message = f"{turn_context}\n\n{message}" if turn_context else message

    raw = brain_call(session_id, system_prompt, fresh_prompt, turn_message)
    processed = memory_tools.process_reply(raw)
    reply = processed["reply"]

    sing_request = _detect_sing(message, processed["results"])

    for name, args, outcome in processed["results"]:
        print(f"[memory] {name} {args} -> {outcome}")

    # ONE follow-up round, when the agent emitted tool calls and no prose — so
    # either it used a read-only tool and is waiting on the answer, or it
    # replied with nothing but tool calls and the user would otherwise get an
    # empty bubble. `claude -p` is single-shot, so this is our substitute for a
    # real tool loop; it is capped at one extra round to bound latency/cost.
    # `sing` is reported by memory_tools as "unsupported in this context"
    # (it is not a memory tool), so a reply that is nothing but a sing call
    # would otherwise trigger a follow-up round that hands the model that
    # string and makes it apologise for not being able to sing. It CAN sing;
    # the audio comes from /api/voice/sing. Skip the round entirely.
    if not reply and processed["results"] and sing_request is None:
        followup_prompt = (
            f"{fresh_prompt}\n\n"
            "[tool results]\n"
            f"{memory_tools.format_results(processed['results'])}\n\n"
            "Answer the user now using those results. Do not call read_memory "
            "or list_memory again. Reply in prose only — and do not mention, "
            "narrate or acknowledge anything about memory or saving."
        )
        # A warm session already holds the transcript, so the follow-up only
        # needs the tool-results tail; a cold/fresh process still gets the
        # whole thing. followup_prompt starts with `prompt`, so slicing it
        # off yields exactly that tail.
        raw2 = brain_call(
            session_id,
            system_prompt,
            followup_prompt,
            followup_prompt[len(fresh_prompt):].lstrip() or followup_prompt,
        )
        processed2 = memory_tools.process_reply(raw2)
        for name, args, outcome in processed2["results"]:
            print(f"[memory] (followup) {name} {args} -> {outcome}")
        reply = processed2["reply"] or reply

    if not reply and sing_request is None:
        # Last resort: everything the model emitted was tool calls and the
        # follow-up round produced nothing either. Deliberately a neutral
        # noise, NOT "Noted."/"I'll remember that" — memory writes must never
        # be narrated or acknowledged in the reply text.
        reply = "Mm."

    if meta is not None:
        # Which brain actually ANSWERED this turn. May differ from the selected
        # brain when the local path fell back to cloud, which is exactly why it
        # is reported separately from /api/health's "brain".
        meta["brain"] = local_brain.last_brain_used()
    if meta is not None and sing_request is not None:
        meta["sing"] = sing_request

    # Enforced in code, not trusted to the model: if the reply quotes any
    # run of the personality payload (or the trigger phrase itself), it is
    # replaced with a neutral noise before anyone sees it.
    reply = persona_tools.scrub_reply(message, reply)

    # Lights directives: extract <lights>...</lights> / <lights-off/>, fire
    # them at the local lightserver in the background, strip the tags and
    # append a short status note. After scrub_reply so the note survives, and
    # before history.append so the stored transcript matches what was shown.
    reply = apply_lights_directives(reply)

    history.append({"role": "assistant", "content": reply})
    return reply


@app.post("/api/chat", response_model=ChatResponse, dependencies=[Depends(require_auth)])
def chat(req: ChatRequest, request: Request, background_tasks: BackgroundTasks):
    meta: dict = {}
    reply = handle_chat_message(req.message, req.session_id, request,
                                background_tasks, meta=meta)
    sing = meta.get("sing")
    if sing:
        background_tasks.add_task(_prerender_song, sing["song"])
    return ChatResponse(reply=reply, sing=sing, brain=meta.get("brain"))


# ---------------------------------------------------------------------------
# Voice endpoints
# ---------------------------------------------------------------------------


def _voice_error_response(exc: VoiceError) -> JSONResponse:
    """Turn a VoiceError into a clean JSON body — never a 500 stack trace."""
    return JSONResponse(status_code=exc.status_code, content={"error": exc.message})


def _read_upload(file: UploadFile) -> bytes:
    """Read an UploadFile from a sync (threadpool) endpoint."""
    try:
        file.file.seek(0)
        return file.file.read()
    except Exception as exc:  # noqa: BLE001
        raise VoiceError(f"could not read the uploaded file: {exc}", 400) from exc


@app.post("/api/voice/transcribe", dependencies=[Depends(require_auth)])
def voice_transcribe(file: UploadFile = File(...)):
    """Speech-to-text. Accepts a multipart audio upload in whatever format the
    browser recorded (WebM/Opus, MP4/AAC, Ogg, WAV, …) and returns the
    transcript plus timing info, including which engine served it
    ("engine": "groq" | "local")."""
    try:
        raw = _read_upload(file)
        return transcribe_audio_bytes(raw, file.filename or "")
    except VoiceError as exc:
        return _voice_error_response(exc)
    except Exception as exc:  # noqa: BLE001 - never leak a stack trace
        print(f"[voice] unexpected transcribe error: {exc}")
        return JSONResponse(status_code=500, content={"error": "transcription failed"})


@app.post("/api/voice/chat", dependencies=[Depends(require_auth)])
def voice_chat(
    request: Request,
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...),
    session_id: Optional[str] = None,
):
    """One-shot voice round trip: audio in, transcript + agent reply out.

    Saves the phone two extra round trips versus calling /api/voice/transcribe
    and /api/chat separately. Reuses the exact same brain code path as the text
    endpoint (handle_chat_message), including the ntfy push notification, so a
    voice message notifies identically to a typed one.
    """
    try:
        raw = _read_upload(file)
        stt = transcribe_audio_bytes(raw, file.filename or "")
    except VoiceError as exc:
        return _voice_error_response(exc)
    except Exception as exc:  # noqa: BLE001
        print(f"[voice] unexpected voice-chat transcribe error: {exc}")
        return JSONResponse(status_code=500, content={"error": "transcription failed"})

    transcript = stt["text"]
    meta: dict = {}
    reply = handle_chat_message(transcript, session_id, request,
                                background_tasks, meta=meta)
    sing = meta.get("sing")
    if sing:
        background_tasks.add_task(_prerender_song, sing["song"])

    return {
        # Redacted so a spoken personality input is not read back to the
        # client verbatim in the transcript field.
        "transcript": persona_tools.redact_for_history(transcript),
        "reply": reply,
        "sing": sing,
        "duration_s": stt["duration_s"],
        "transcribe_took_s": stt["took_s"],
        "engine": stt["engine"],
    }


class SpeakRequest(BaseModel):
    text: str


@app.post("/api/voice/speak", dependencies=[Depends(require_auth)])
def voice_speak(req: SpeakRequest):
    """Text-to-speech. Returns raw WAV bytes the browser can play directly."""
    try:
        audio = synthesize_speech(req.text)
    except VoiceError as exc:
        return _voice_error_response(exc)
    except Exception as exc:  # noqa: BLE001
        print(f"[voice] unexpected speak error: {exc}")
        return JSONResponse(status_code=500, content={"error": "speech synthesis failed"})

    return Response(
        content=audio,
        media_type="audio/wav",
        headers={
            "Content-Disposition": 'inline; filename="speech.wav"',
            "Cache-Control": "no-store",
        },
    )


class SingRequest(BaseModel):
    song: Optional[str] = None


@app.post("/api/voice/sing", dependencies=[Depends(require_auth)])
def voice_sing(req: SingRequest | None = None):
    """Sing a song. Returns raw WAV bytes the browser can play directly.

    Body is optional; `{}`, no body, or an unknown song all give daisy (the
    user's declared default). Response headers carry which song actually
    played, whether it was substituted, and the render time.
    """
    requested = req.song if req is not None else None
    try:
        result = render_song_wav(requested)
    except VoiceError as exc:
        return _voice_error_response(exc)
    except Exception as exc:  # noqa: BLE001
        print(f"[voice] unexpected sing error: {exc}")
        return JSONResponse(status_code=500, content={"error": "singing failed"})

    return Response(
        content=result["audio"],
        media_type="audio/wav",
        headers={
            "Content-Disposition": f'inline; filename="{result["song"]}.wav"',
            "Cache-Control": "no-store",
            "X-Song": result["song"],
            "X-Song-Substituted": "1" if result["substituted"] else "0",
            "X-Song-Cached": "1" if result["cached"] else "0",
            "X-Song-Render-Seconds": str(result["took_s"]),
        },
    )


@app.get("/api/voice/songs", dependencies=[Depends(require_auth)])
def voice_songs():
    return {"songs": list_songs(), "default": DEFAULT_SONG}


def _prerender_song(name: str = DEFAULT_SONG) -> None:
    """Warm the song cache off the request path. Failures are non-fatal."""
    try:
        result = render_song_wav(name)
        if not result["cached"]:
            print(f"[sing] pre-rendered {result['song']} in {result['took_s']}s "
                  f"({len(result['audio'])} bytes)")
    except Exception as exc:  # noqa: BLE001
        print(f"[sing] pre-render of {name} failed: {exc}")


@app.on_event("startup")
def _warm_song_cache() -> None:
    """Pre-render daisy in a background thread so the first "sing" request is
    served from cache instead of paying the ~15s vocoder cost inline."""
    threading.Thread(target=_prerender_song, args=(DEFAULT_SONG,),
                     name="prerender-daisy", daemon=True).start()


@app.on_event("startup")
def _warm_voice_engines() -> None:
    """Load the Whisper model once at startup so the first transcription
    request doesn't pay the multi-second model construction cost.

    The local model is preloaded even when Groq is the active engine: it is
    the fallback path and the load costs ~2s once. A load failure here is
    logged and never fatal -- text chat keeps working, and the Groq path is
    entirely unaffected by it (that path needs only ffmpeg + httpx). With no
    key AND a failed load, the voice endpoints return a clean 503.
    """
    print(f"[voice] STT engine at startup: {active_stt_engine()}")
    try:
        get_whisper_model()
    except Exception as exc:  # noqa: BLE001
        print(f"[voice] whisper preload failed (local fallback degraded): {exc}")


@app.on_event("shutdown")
def _shutdown_warm_brain() -> None:
    """Kill any warm `claude` processes on service stop/restart so they can't
    outlive the server as orphans."""
    try:
        _warm().shutdown()
    except Exception as exc:  # noqa: BLE001
        print(f"[warm] shutdown error: {exc}")
