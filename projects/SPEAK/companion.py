#!/usr/bin/env python3
from __future__ import annotations
import json
import queue
import re
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

from rich.console import Console
from rich.panel import Panel

from engine import memory, tools, thermal
from engine.stt import StreamingListener
from engine.llm import LLMBackend
from engine.presence import Presence, PresenceState
from engine.idle import IdleLoop
from engine.idle_sounds import NPC_SOUNDS, VERSE_TOPICS

TEXT_MODE = "--text" in sys.argv

console = Console()

_DYNAMIC_SEPARATOR = "\n\n<!-- ===== DYNAMIC CONTEXT BELOW (cache-invalidating) ===== -->\n"

_SESSION_FILE = memory.MEMORY_DIR / "sessions" / f"{datetime.now().strftime('%Y%m%d_%H%M%S')}.jsonl"


def _build_system_prompt(mem_ctx: dict) -> str:
    """Persona first (cache-stable prefix), then dynamic facts only.

    Lore is injected as a synthetic history message, not here, to keep the
    cache-stable prefix as long as possible across turns.
    """
    persona_path = Path("persona.md")
    persona = persona_path.read_text() if persona_path.exists() else (
        "You are neptr, a chill offline AI companion. Keep it short."
    )
    dynamic_parts: list[str] = []
    if "facts" in mem_ctx:
        dynamic_parts.append(f"## What you remember\n{mem_ctx['facts']}")
    if not dynamic_parts:
        return persona
    return persona + _DYNAMIC_SEPARATOR + "\n\n".join(dynamic_parts)


def _lore_history_message() -> dict | None:
    lore = memory.load_lore()
    if not lore:
        return None
    return {"role": "system", "content": f"## Your history (lore)\n{lore}"}


def _dispatch(call: dict) -> str | None:
    name = call.get("tool", "")
    args = call.get("args", {})
    if name == "speak":
        _speak(**args)
    elif name == "sing":
        if not TEXT_MODE:
            tools.sing(**args)
    elif name == "set_voice":
        if not TEXT_MODE:
            tools.set_voice(**args)
    elif name == "write_memory":
        tools.write_memory(**args)
    elif name == "read_memory":
        return tools.read_memory(**args)
    elif name == "delete_memory":
        tools.delete_memory(**args)
    elif name == "list_memory":
        keys = tools.list_memory()
        result = ", ".join(keys) if keys else "nothing saved yet"
        _speak(result)
        return result
    elif name == "write_lore":
        tools.write_lore(**args)
    elif name == "write_song":
        tools.write_song(**args)
    else:
        _speak(f"the fuck is '{name}', I don't know that tool")
    return None


def _speak(text: str) -> None:
    if TEXT_MODE:
        console.print(f"[bold cyan]neptr:[/bold cyan] {text}")
    else:
        tools.speak(text)


_PERSONA_LEAK = re.compile(
    r"(you exist because emory|maximum humbleness|biggest ideas in the simplest"
    r"|never show off|you are chill|speak-and-spell|raspberry pi"
    r"|you wear all of it lightly)",
    re.IGNORECASE,
)

_GREETER_LEAK = re.compile(
    r"\b("
    r"how\s+(can|may|might|could)\s+i\s+(help|assist|be\s+of\s+(help|service|assistance))"
    r"|how\s+can\s+i\s+help\s+you(\s+today)?"
    r"|what\s+(can|may)\s+i\s+(do|help)\s+(for|with)\s+you"
    r"|what\s+would\s+you\s+like\s+(to\s+talk\s+about|me\s+to\s+(do|help))"
    r"|(is\s+there\s+)?anything\s+i\s+can\s+help\s+(you\s+)?with"
    r"|need\s+(any\s+)?help\s+with\s+(anything|something)"
    r"|how\s+may\s+i\s+be\s+of\s+(service|assistance)"
    r"|what(?:'s| is)\s+on\s+your\s+mind(?:\s+today)?"
    r"|what(?:'s| is)\s+going\s+on\s+in\s+that\s+head"
    r"|what(?:'s| is)\s+up\s+with\s+you"
    r"|i(?:'m| am)\s+all\s+ears"
    r"|lay\s+it\s+on\s+me"
    r"|fire\s+away"
    r"|always\s+(down|happy|here)\s+to\s+(chat|talk|help|listen)"
    r"|piqued?\s+your\s+(curiosity|interest)"
    r"|tell\s+me\s+(more\s+)?(about\s+(it|that|yourself)|what(?:'s| is)\s+on\s+your\s+mind)"
    r"|how\s+(are|have|s)\s+(you|things)\s+(doing|going|been)"
    r"|how(?:'s| is)\s+(it|that)\s+sounding\s+to\s+you"
    r")\b",
    re.IGNORECASE,
)


_FALLBACKS = ("mm", "yeah", "huh", "right", "sure", "go on")

# Banter questions that are always allowed (short reactions, not waiter-mode).
_BANTER_Q = re.compile(
    r"^(huh|what|really|seriously|wait|wait\s+what|for\s+real|no\s+way|right)\??$",
    re.IGNORECASE,
)

# Probability that a non-banter trailing question survives the stripper.
# Set ~0.25 so questions show up occasionally — only the ones that already
# passed _GREETER_LEAK (the obvious waiter patterns are already gone) and
# short enough to feel like a real curious ask, not interrogation.
_QUESTION_PASS_RATE = 0.25
_QUESTION_MAX_WORDS = 8  # only short genuine questions get the dice roll


def _strip_trailing_questions(text: str) -> str:
    """Remove any trailing question sentences from a reply line.

    Hard rule: if the LLM ends a line with a `?`, drop everything from the
    last sentence boundary onward. Banter questions ("huh?", "really?") on
    their own pass through. Short genuine questions get a 25% pass rate.
    If the entire line is a question and the dice roll fails, line empties.
    """
    import random as _random
    s = text.strip()
    if not s:
        return s
    if _BANTER_Q.match(s):
        return s
    if not s.endswith("?"):
        return s
    # Isolate the trailing question segment.
    cut = max(s.rfind("."), s.rfind("!"), s.rfind("\n"))
    question_segment = (s if cut < 0 else s[cut + 1:]).strip()
    word_count = len(question_segment.split())

    # Short genuine question + dice roll says yes → keep the whole line.
    if word_count <= _QUESTION_MAX_WORDS and _random.random() < _QUESTION_PASS_RATE:
        return s

    # Otherwise strip the question.
    if cut < 0:
        return ""
    return s[: cut + 1].strip()


def _scrub_filtered_lines(reply: str) -> str:
    """Strip individual lines that hit leak/greeter filters; keep the rest.
    Also strip trailing questions per line (statements over questions).
    Returns "" if every line was filtered out."""
    kept: list[str] = []
    for line in reply.splitlines():
        s = line.strip()
        if not s:
            continue
        if _PERSONA_LEAK.search(s):
            console.print(f"[dim red](filtered persona-leak: {s[:60]})[/dim red]")
            continue
        if _GREETER_LEAK.search(s):
            console.print(f"[dim red](filtered greeter: {s[:60]})[/dim red]")
            continue
        # JSON tool-call lines pass through untouched.
        if s.startswith("{"):
            kept.append(s)
            continue
        stripped_q = _strip_trailing_questions(s)
        if stripped_q != s:
            console.print(f"[dim yellow](stripped trailing question: {s[len(stripped_q):][:60]})[/dim yellow]")
        if stripped_q:
            kept.append(stripped_q)
    return "\n".join(kept)


def _handle_reply(reply: str) -> list[str]:
    results: list[str] = []

    # Filter at line granularity, not whole-reply. If the model emits one bad
    # line and three good ones, keep the good. If everything filters out,
    # emit a short fallback so the user always gets *something* back — silent
    # drops feel like the system is broken.
    cleaned = _scrub_filtered_lines(reply)
    if not cleaned:
        import random as _random
        fallback = _random.choice(_FALLBACKS)
        console.print(f"[cyan]neptr:[/cyan] {fallback}  [dim](reply was fully filtered)[/dim]")
        tools.speak(fallback)
        return results

    lines = cleaned.splitlines()
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("{"):
            try:
                call = json.loads(stripped)
                if call.get("tool") == "speak":
                    console.print(f"[cyan]neptr:[/cyan] {call.get('args', {}).get('text', '')}")
                result = _dispatch(call)
                if result is not None and call.get("tool") == "read_memory":
                    _speak(result or "nothing saved there yet")
                    results.append(result)
            except json.JSONDecodeError:
                tools.speak(stripped)
        elif stripped:
            console.print(f"[cyan]neptr:[/cyan] {stripped}")
            tools.speak(stripped)
    return results


def _consolidate(llm: LLMBackend, system_prompt: str, history: list[dict]) -> None:
    """One self-maintenance LLM turn on exit — neptr curates its own memory."""
    prompt = (
        "Session ending. Review the conversation and your current memory state. "
        "Use write_memory to update or add facts worth keeping. "
        "Use delete_memory to remove anything wrong or stale. "
        "Use write_lore for one sentence about anything significant that happened, "
        "only if something actually worth remembering occurred. "
        "No commentary. Just tool calls if needed, or silence."
    )
    try:
        reply = llm.try_generate(system_prompt=system_prompt, history=history[-10:], user_msg=prompt)
        if reply:
            _handle_reply(reply)
    except Exception:
        pass


def main() -> None:
    if not TEXT_MODE:
        from engine.voice import synthesize
        from engine.audio import play as _play
        _s, _sr = synthesize("waking up", mode="mix", ti_amount=0.85)
        _play(_s, _sr)

    mem_ctx = memory.load_all()
    if not TEXT_MODE:
        vcfg = mem_ctx.get("voice_config", {})
        if vcfg:
            tools.set_voice(vcfg.get("mode", tools._voice_mode),
                            vcfg.get("ti_amount", 0.85))

    console.print("[dim]loading LLM...[/dim]")
    llm = LLMBackend()

    if not TEXT_MODE:
        console.print("[dim]loading STT...[/dim]")
        from engine.stt import _get_model as _warm_stt
        _warm_stt()

    presence = Presence(active_window_seconds=900.0)

    system_prompt = _build_system_prompt(mem_ctx)
    history: list[dict] = []

    # Inject lore as synthetic history message so it doesn't break the cache-stable prefix
    lore_msg = _lore_history_message()
    if lore_msg:
        history.append(lore_msg)

    # Inaudible warmup: run a real turn through the actual system prompt so
    # the persona prefix gets tokenized and KV-cached. The reply is discarded
    # — never spoken, never logged. First real user prompt then reuses the
    # cached prefix and skips the cold-start TTFT penalty.
    console.print("[dim]priming cache...[/dim]")
    try:
        llm.generate(system_prompt, history, "ping")
    except Exception as exc:
        console.print(f"[dim red][warmup error: {exc!r}][/dim red]")

    def _generate_verse(topic: str) -> str:
        prompt = (
            f"Write a single short poetic verse (4-8 short lines) about: {topic}. "
            "No introduction, no commentary, just the verse. Plain text."
        )
        result = llm.try_generate(
            system_prompt=system_prompt,
            history=[],
            user_msg=prompt,
        )
        return result or ""

    if not TEXT_MODE:
        idle_loop = IdleLoop(
            presence=presence,
            speak_fn=tools.speak,
            sing_fn=lambda lyrics: tools.sing(lyrics, midi=None),
            verse_generator=_generate_verse,
            sounds=NPC_SOUNDS,
            topics=VERSE_TOPICS,
        )
        idle_loop.start()

    input_q: queue.Queue[str] = queue.Queue()

    _WAKE_WORDS = ["computer", "computa", "computah", "nepter"]
    _WAKE_RE = r"\b(computer|computah|computa|nepter)\b"

    def _has_wake(text: str) -> bool:
        lower = text.lower()
        return any(w in lower for w in _WAKE_WORDS)

    def _on_text(text: str, during_playback: bool = False) -> None:
        # Barge-in: if neptr was speaking when this was recorded, only act on
        # utterances that contain a wake word. Otherwise it's likely echo of
        # neptr's own voice or talkover noise — silently drop it.
        if during_playback and not _has_wake(text):
            return
        # If a wake word lands while neptr is mid-sentence, kill the audio
        # immediately so the user takes the floor. Drains pending TTS too.
        if _has_wake(text):
            from engine import audio as _audio
            _audio.stop()
            try:
                while True:
                    input_q.get_nowait()
            except queue.Empty:
                pass

        lower = text.lower()
        if presence.state == PresenceState.IDLE:
            if not _has_wake(text):
                console.print(f"[dim](idle, ignored: {text[:40]})[/dim]")
                return
            # wake and strip the wake word from the prompt
            import re as _re
            clean = _re.sub(
                _WAKE_RE, "", text, flags=_re.IGNORECASE
            ).strip(" ,.")
            presence.wake()
            import random as _random
            ack = _random.choice(["yeah", "what's up", "yo", "mm", "here"])
            # Non-blocking ack: speak in a background thread so input_q.put
            # fires immediately and the LLM can start generating in parallel
            # with the acknowledgment TTS. Cuts ~1-2s off first-response feel.
            if not clean:
                _speak(ack)
                return  # just the wake word, nothing else — wait for next input
            threading.Thread(target=_speak, args=(ack,), daemon=True).start()
            text = clean
        else:
            # Already active. Strip wake words mid-conversation so they don't
            # become part of the prompt.
            if _has_wake(text):
                import re as _re
                cleaned = _re.sub(
                    _WAKE_RE, "", text, flags=_re.IGNORECASE
                ).strip(" ,.")
                if not cleaned:
                    # bare wake word during active conversation — re-acknowledge
                    import random as _random
                    _speak(_random.choice(["yeah", "still here", "what's up", "go"]))
                    return
                text = cleaned

        presence.mark_utterance()
        console.print(f"[green]you:[/green] {text}")
        input_q.put(text)

    def keyboard_loop() -> None:
        while True:
            try:
                line = console.input("[bold green]> [/bold green]")
                input_q.put(line)
                presence.mark_utterance()
            except EOFError:
                input_q.put(":q")
                break

    console.print(Panel.fit(
        "[bold green]NEPTR[/bold green]",
        subtitle="say 'computer' or 'nepter' to wake  //  :v <mode>  :a <0-1>  :reload  :q",
    ))
    console.print("[dim]state: IDLE — say wake word to activate[/dim]")

    # Start the listener BEFORE any boot TTS so the InputStream is fully
    # initialized by the time the user could possibly speak. Give it a
    # short head start to ensure the audio device callback is firing.
    if not TEXT_MODE:
        stream_listener = StreamingListener(on_text=_on_text)
        stream_listener.start()
        time.sleep(0.4)
        _speak("I'm awake.")
    threading.Thread(target=keyboard_loop, daemon=True).start()

    thermal_check_at = time.time() + 30.0

    while True:
        try:
            user_input = input_q.get(timeout=1.0)
        except queue.Empty:
            presence.tick()
            if time.time() >= thermal_check_at:
                if thermal.check_and_throttle(llm._llm):
                    tools.speak("getting hot in here, throttling back")
                thermal_check_at = time.time() + 30.0
            continue

        stripped = user_input.strip()
        if not stripped:
            continue

        if stripped.lower() in (":q", ":quit"):
            console.print("[dim green]consolidating memory...[/dim green]")
            _consolidate(llm, system_prompt, history)
            console.print("[dim green]bye[/dim green]")
            break

        if stripped.lower().startswith(":v "):
            mode = stripped[3:].strip()
            tools.set_voice(mode, None)
            console.print(f"[dim green]voice → {mode}[/dim green]")
            continue

        if stripped.lower().startswith(":a "):
            try:
                amount = float(stripped[3:].strip())
                tools.set_voice(None, amount)
                console.print(f"[dim green]TI amount → {amount}[/dim green]")
            except ValueError:
                console.print("[dim red]usage: :a 0.0-1.0[/dim red]")
            continue

        if stripped.lower() == ":reload":
            mem_ctx = memory.load_all()
            system_prompt = _build_system_prompt(mem_ctx)
            console.print("[dim green]persona + memory reloaded[/dim green]")
            continue

        memory.log_turn("user", stripped, _SESSION_FILE)

        try:
            reply = llm.generate(system_prompt, history, stripped)
        except Exception as exc:
            console.print(f"[dim red][llm error: {exc!r}][/dim red]")
            continue

        try:
            memory_results = _handle_reply(reply)
        except Exception as exc:
            console.print(f"[dim red][reply handler error: {exc!r}][/dim red]")
            memory_results = []

        memory.log_turn("assistant", reply, _SESSION_FILE)

        history.append({"role": "user", "content": stripped})
        history.append({"role": "assistant", "content": reply})
        for r in memory_results:
            history.append({"role": "user", "content": f"[memory result: {r}]"})

    if not TEXT_MODE:
        idle_loop.stop()
        stream_listener.stop()


if __name__ == "__main__":
    main()
