#!/usr/bin/env python3
import sys
from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich import box

from engine.router import wordbank_text
from engine.voice import synthesize, VALID_MODES
from engine.lpc import decode
from engine.audio import play

console = Console()

HEADER  = Text("SPEAK & SPELL", style="bold green", justify="center")
SUBHEAD = Text(
    ":v <mode> voice  //  :p pitch  //  :w word bank  //  :a <0-1> TI amount  //  Ctrl+C to exit",
    style="dim green", justify="center",
)

_PITCH_MODE = 'flat'
_USE_BANK   = False
_VOICE      = 'mix'     # modern | ti | mix | espeak
_TI_AMOUNT  = 0.85
_SING       = False     # major-scale chirp modulation


def _play_tuple(pair):
    samples, sr = pair
    play(samples, sr)


def _label(line: str) -> str:
    from data.words import WORDS
    from data.letters import LETTERS
    import re
    cleaned = re.sub(r"[^A-Z0-9' -]", '', line.upper())
    labeled = []
    for w in cleaned.split():
        if _USE_BANK and w in WORDS:
            labeled.append(f"[green]{w}[/green]")
        elif len(w) == 1 and w in LETTERS:
            labeled.append(f"[cyan]{w}[/cyan]")
        else:
            labeled.append(f"[yellow]{w}[/yellow]")
    if not labeled:
        return ''
    tag = (f"  [dim](voice={_VOICE}  pitch={'on' if _PITCH_MODE=='natural' else 'off'}  "
           f"bank={'on' if _USE_BANK else 'off'}  ti={_TI_AMOUNT:.2f})[/dim]")
    return "  " + " ".join(labeled) + tag


def main() -> None:
    global _PITCH_MODE, _USE_BANK, _VOICE, _TI_AMOUNT, _SING
    console.print(Panel.fit(HEADER, subtitle=SUBHEAD.plain, box=box.DOUBLE, style="green"))
    console.print()

    while True:
        try:
            line = console.input("[bold green]> [/bold green]")
        except (KeyboardInterrupt, EOFError):
            console.print("\n[dim green]* POWER OFF *[/dim green]")
            sys.exit(0)

        stripped = line.strip()
        if not stripped:
            continue
        low = stripped.lower()

        # ── Toggles ──────────────────────────────────────────────────────
        if low in (':p', ':pitch'):
            _PITCH_MODE = 'natural' if _PITCH_MODE == 'flat' else 'flat'
            console.print(f"  [dim green]pitch → {_PITCH_MODE}[/dim green]\n")
            continue

        if low in (':s', ':sing'):
            _SING = not _SING
            console.print(f"  [dim green]sing → {'on' if _SING else 'off'}[/dim green]\n")
            continue

        if low in (':w', ':words', ':bank'):
            _USE_BANK = not _USE_BANK
            console.print(f"  [dim green]word bank → {'on' if _USE_BANK else 'off'}[/dim green]\n")
            continue

        if low.startswith(':v'):
            parts = low.split()
            if len(parts) == 2 and parts[1] in VALID_MODES:
                _VOICE = parts[1]
                console.print(f"  [dim green]voice → {_VOICE}[/dim green]\n")
            else:
                console.print(f"  [dim green]voice modes: {', '.join(VALID_MODES)} (current: {_VOICE})[/dim green]\n")
            continue

        if low.startswith(':a'):
            parts = low.split()
            if len(parts) == 2:
                try:
                    _TI_AMOUNT = max(0.0, min(1.0, float(parts[1])))
                    console.print(f"  [dim green]ti_amount → {_TI_AMOUNT:.2f}[/dim green]\n")
                except ValueError:
                    console.print("  [red]usage: :a 0.0-1.0[/red]\n")
            else:
                console.print(f"  [dim green]ti_amount: {_TI_AMOUNT:.2f}[/dim green]\n")
            continue

        # ── Speak ────────────────────────────────────────────────────────
        lbl = _label(stripped)
        if lbl:
            console.print(lbl)
        console.print()

        if _USE_BANK:
            blob = wordbank_text(stripped)
            if blob:
                play(decode(blob), 8000)
        else:
            _play_tuple(synthesize(stripped, mode=_VOICE,
                                   pitch_mode=_PITCH_MODE,
                                   ti_amount=_TI_AMOUNT,
                                   sing=_SING))


if __name__ == '__main__':
    main()
