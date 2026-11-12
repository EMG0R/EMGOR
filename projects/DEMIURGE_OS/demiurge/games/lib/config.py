"""
config.py — parser for games.conf.

Deliberately the SAME `key = value`, `#`-comments format as
~/demiurge/live.conf, so when this graduates out of the games folder the keys
fold straight into live.conf with no syntax change. Until then the arcade
console keeps its own self-contained config file. See ../games.conf.

Also parses a multi-line `games =` block (same indented-continuation style as
live.conf's `chain =`) — each indented line is one launchable game:

    games =
      galaga | galaga.zip | ARCADE,Y,ARCADE,X,ARCADE,A,ARCADE
      digdug | digdug.zip | ARCADE,Y,ARCADE,B,ARCADE,X,ARCADE

i.e. `label | rom-filename | CODE` where CODE is a comma-separated button
sequence (tokens ARCADE/A/B/X/Y) that LAUNCHES that game from synth mode.
"""

from __future__ import annotations

import os
from pathlib import Path

# Defaults — used for any key absent from games.conf so a missing/partial file
# still boots something sane (fail-open, like the rest of Demiurge).
DEFAULTS: dict[str, str] = {
    "enabled":      "on",
    "serial_port":  "/dev/ttyACM0",
    "combo_hold_ms": "1000",
    "freeze_synth": "on",
    "audio_driver": "pipewire",   # console rides the sync layer (PipeWire)
    "default_game": "",           # empty = boot into the RGUI menu; else auto-launch this ROM
    "default_core": "",           # full path to the libretro core for default_game / games block
    "volume":       "80",
    "joy_deadzone": "900",        # 12-bit; |reading-center| past this = a direction
    "joy_rotation": "0",          # 0/90/180/270 CW: rotate stick to match portrait screen
    "joy_invert_x": "off",        # flip left/right (reflection, after rotation)
    "joy_invert_y": "off",        # flip up/down (reflection, after rotation)
    "code_gap_ms":  "2500",       # max gap between presses in a launch code before it resets
    "exit_hold_ms": "1500",       # hold the joystick click (jb) this long in-game to exit
    "boot_skip_ms": "6000",       # auto fast-forward this long after launch to skip the boot/attract
    # Games WITHOUT an infinite-lives cheat get a big one-time COIN BURST on launch
    # so you have tons of credits (continue forever, no coin-dealing). Space-separated
    # labels. Games with native cheats (galaga, mspacman, sf) are NOT listed.
    "autocoin_games": "digdug digdug2 centiped spyhunt",
    "autocoin_burst": "50",       # how many coins to insert at launch
    "autocoin_delay_ms": "6500",  # after the boot-skip, once the game is up
}


def _coerce(v: str):
    s = v.strip()
    low = s.lower()
    if low in ("on", "true", "yes"):
        return True
    if low in ("off", "false", "no"):
        return False
    try:
        return int(s)
    except ValueError:
        return s


class Config:
    def __init__(self, values: dict[str, str], games: list[dict] | None = None):
        merged = dict(DEFAULTS)
        merged.update(values)
        self._raw = merged
        self._games = games or []

    def __getitem__(self, key: str):
        return _coerce(self._raw[key])

    def get(self, key: str, default=None):
        if key in self._raw:
            return _coerce(self._raw[key])
        return default

    @property
    def combo_hold_seconds(self) -> float:
        return int(self._raw["combo_hold_ms"]) / 1000.0

    @property
    def code_gap_seconds(self) -> float:
        return int(self._raw["code_gap_ms"]) / 1000.0

    @property
    def exit_hold_seconds(self) -> float:
        return int(self._raw["exit_hold_ms"]) / 1000.0

    def games(self) -> list[dict]:
        """List of {label, rom, code:[tokens]} from the `games =` block."""
        return self._games

    @staticmethod
    def _parse_games(text: str) -> list[dict]:
        games: list[dict] = []
        in_block = False
        for raw in text.splitlines():
            stripped = raw.split("#", 1)[0].rstrip()
            if not stripped.strip():
                continue
            indented = raw[:1].isspace()
            head = stripped.strip()
            if not in_block:
                if head.replace(" ", "").startswith("games="):
                    in_block = True
                continue
            # in the games block: indented continuation lines are entries;
            # a new non-indented `key =` ends the block.
            if not indented and "=" in head:
                in_block = False
                continue
            parts = [p.strip() for p in head.split("|")]
            if len(parts) >= 3 and parts[0] and parts[1]:
                code = [t.strip().upper() for t in parts[2].split(",") if t.strip()]
                entry = {"label": parts[0], "rom": parts[1], "code": code}
                # optional 4th field = full core path (else default_core is used)
                if len(parts) >= 4 and parts[3]:
                    entry["core"] = parts[3]
                games.append(entry)
        return games

    @classmethod
    def parse_text(cls, text: str) -> "Config":
        values: dict[str, str] = {}
        for line in text.splitlines():
            line = line.split("#", 1)[0].strip()
            if not line or "=" not in line:
                continue
            k, v = line.split("=", 1)
            values[k.strip()] = v.strip()
        return cls(values, games=cls._parse_games(text))

    @classmethod
    def load(cls, path: str | os.PathLike | None = None) -> "Config":
        if path is None:
            path = Path(__file__).resolve().parent.parent / "games.conf"
        p = Path(path)
        if not p.exists():
            return cls({})
        return cls.parse_text(p.read_text())
