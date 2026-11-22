"""Tests for the per-game launch-code detector — no hardware."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from lib.sequence import SequenceDetector
from lib.config import Config

GALAGA = ["ARCADE", "Y", "ARCADE", "X", "ARCADE", "A", "ARCADE"]
DIGDUG = ["ARCADE", "Y", "ARCADE", "B", "ARCADE", "A", "ARCADE"]
CODES = {"galaga": GALAGA, "digdug": DIGDUG}

# token -> frame field
F = {"ARCADE": "b1", "A": "b9", "B": "b8", "X": "b6", "Y": "b7"}


def _press(det, token, t):
    """Simulate a press+release of one button at time t. Returns any match."""
    r = det.update({F[token]: 1}, t)
    det.update({F[token]: 0}, t + 0.01)
    return r


def _run(det, tokens, t0=0.0, step=0.2):
    last = None
    for i, tok in enumerate(tokens):
        last = _press(det, tok, t0 + i * step) or last
    return last


def test_galaga_code_fires():
    det = SequenceDetector(CODES, gap_seconds=2.5)
    assert _run(det, GALAGA) == "galaga"


def test_digdug_code_fires_not_galaga():
    det = SequenceDetector(CODES, gap_seconds=2.5)
    assert _run(det, DIGDUG) == "digdug"


def test_codes_distinct():
    """Galaga and Dig Dug share a prefix but must resolve to the right game."""
    det = SequenceDetector(CODES, gap_seconds=2.5)
    assert _run(det, GALAGA) == "galaga"
    assert _run(det, DIGDUG) == "digdug"


def test_leading_garbage_then_code_still_fires():
    det = SequenceDetector(CODES, gap_seconds=2.5)
    _run(det, ["A", "B", "X"])                 # noise
    assert _run(det, GALAGA, t0=2.0) == "galaga"   # suffix match


def test_gap_timeout_resets():
    det = SequenceDetector(CODES, gap_seconds=1.0)
    # first 6 of galaga
    _run(det, GALAGA[:6], t0=0.0, step=0.1)
    # final press far too late -> sequence reset, no fire
    assert _press(det, "ARCADE", 10.0) is None


def test_no_partial_fire():
    det = SequenceDetector(CODES, gap_seconds=2.5)
    assert _run(det, GALAGA[:-1]) is None       # one short


def test_config_parses_core_override():
    text = (
        "games =\n"
        "  galaga | galaga.zip | ARCADE,A,ARCADE\n"
        "  gnw | gnw.zip | ARCADE,B,ARCADE | /cores/mame.so\n"
    )
    g = Config.parse_text(text).games()
    assert "core" not in g[0]                 # uses default_core
    assert g[1]["core"] == "/cores/mame.so"   # 4th field honored


def test_all_real_codes_distinct():
    """Every game's code must be unique (no two launch the same / collide)."""
    cfg = Config.load()   # the real games.conf
    codes = [tuple(g["code"]) for g in cfg.games()]
    assert len(codes) == len(set(codes)), "duplicate launch code in games.conf"


def test_config_parses_games_block():
    text = (
        "default_core = /x/fbneo.so\n"
        "games =\n"
        "  galaga | galaga.zip | ARCADE,Y,ARCADE,X,ARCADE,A,ARCADE\n"
        "  digdug | digdug.zip | ARCADE,Y,ARCADE,B,ARCADE,A,ARCADE\n"
        "code_gap_ms = 3000\n"
    )
    cfg = Config.parse_text(text)
    games = cfg.games()
    assert [g["label"] for g in games] == ["galaga", "digdug"]
    assert games[0]["rom"] == "galaga.zip"
    assert games[0]["code"] == GALAGA
    assert games[1]["code"] == DIGDUG
    assert cfg.code_gap_seconds == 3.0
    # the trailing non-indented key ended the block (not swallowed as a game)
    assert all("=" not in g["label"] for g in games)
