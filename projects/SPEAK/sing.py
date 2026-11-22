#!/usr/bin/env python3
"""
Chill speak/sing REPL.

Usage:
    python3 sing.py                   → type text + ENTER to speak
    python3 sing.py songs/daisy.song  → sing a .song file and exit

REPL commands:
    :v <mode>      voice  (modern | ti | mix | espeak)   [default: mix]
    :a <0-1>       TI amount
    :p             toggle pitch mode (flat ↔ natural)
    :sing <path>   render a .song file
    :q             quit
"""
from __future__ import annotations
import os
import sys
from rich.console import Console
from rich.panel import Panel
from rich import box

from engine.voice import synthesize, VALID_MODES
from engine.audio import play

console = Console()

HEADER = "SPEAK & SPELL  —  type text, hit ENTER"
SUBHEAD = ":v <mode>  :a <0-1>  :sing <file>  :q"


def _speak(text: str, voice: str, ti_amount: float, pitch: str) -> None:
    samples, sr = synthesize(text, mode=voice, pitch_mode=pitch,
                             ti_amount=ti_amount)
    play(samples, sr)


def _sing_file(path: str) -> None:
    from engine.songfile import render_song_file
    if not os.path.exists(path):
        console.print(f"[red]no such file: {path}[/red]")
        return
    console.print(f"[dim green]rendering {path} …[/dim green]")
    samples, sr = render_song_file(path)
    play(samples, sr)


def main() -> None:
    # CLI mode: run a .song file and exit
    if len(sys.argv) > 1:
        _sing_file(sys.argv[1])
        return

    voice = "mix"
    ti_amount = 0.85
    pitch = "flat"

    console.print(Panel.fit(HEADER, subtitle=SUBHEAD,
                            box=box.DOUBLE, style="green"))
    console.print()

    while True:
        try:
            line = console.input("[bold green]> [/bold green]").strip()
        except (KeyboardInterrupt, EOFError):
            console.print("\n[dim green]* POWER OFF *[/dim green]")
            return

        if not line:
            continue

        low = line.lower()
        if low in (":q", ":quit", ":exit"):
            return

        if low.startswith(":v"):
            parts = line.split()
            if len(parts) == 2 and parts[1] in VALID_MODES:
                voice = parts[1]
                console.print(f"[dim green]voice → {voice}[/dim green]\n")
            else:
                console.print(f"[dim green]voice modes: {', '.join(VALID_MODES)} "
                              f"(current: {voice})[/dim green]\n")
            continue

        if low.startswith(":a"):
            parts = line.split()
            if len(parts) == 2:
                try:
                    ti_amount = max(0.0, min(1.0, float(parts[1])))
                    console.print(f"[dim green]ti_amount → {ti_amount:.2f}[/dim green]\n")
                except ValueError:
                    console.print("[red]usage: :a 0.0-1.0[/red]\n")
            else:
                console.print(f"[dim green]ti_amount: {ti_amount:.2f}[/dim green]\n")
            continue

        if low in (":p", ":pitch"):
            pitch = "natural" if pitch == "flat" else "flat"
            console.print(f"[dim green]pitch → {pitch}[/dim green]\n")
            continue

        if low.startswith(":sing"):
            parts = line.split(maxsplit=1)
            if len(parts) == 2:
                _sing_file(parts[1].strip())
            else:
                console.print("[red]usage: :sing <path/to.song>[/red]\n")
            continue

        # speak it
        _speak(line, voice, ti_amount, pitch)


if __name__ == "__main__":
    main()
