"""Tests for ui_state — the persisted "where was I" file.

The contract these lock down:
  * only UI-owned position is stored (no engine/DSP state ever),
  * a missing, truncated or garbage file degrades to defaults instead of
    taking the UI down,
  * writes are atomic and debounced, and identical positions never rewrite.
"""

import os

import pytest

import ui_state


# ------------------------------------------------------------------ loading

def test_missing_file_yields_defaults(tmp_path):
    assert ui_state.load(str(tmp_path / "nope.conf")) == ui_state.DEFAULTS


def test_roundtrip(tmp_path):
    path = str(tmp_path / "ui-state.conf")
    state = dict(ui_state.DEFAULTS, view="dly", menu=3,
                 companion=1, companion_screen="PATCHES", companion_sel=2)
    assert ui_state.save(state, path) is True
    assert ui_state.load(path) == state


def test_partial_file_fills_in_defaults(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_text("view = menu7\n")
    got = ui_state.load(str(path))
    assert got["view"] == "menu7"
    assert got["menu"] == ui_state.DEFAULTS["menu"]


def test_comments_and_blank_lines(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_text("# a comment\n\n  view = menu2   # trailing\n")
    assert ui_state.load(str(path))["view"] == "menu2"


def test_garbage_ints_fall_back_per_key(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_text("view = menu2\nmenu = banana\n")
    got = ui_state.load(str(path))
    assert got["view"] == "menu2"          # good keys survive
    assert got["menu"] == 0                # bad one falls back


def test_binary_garbage_yields_defaults(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_bytes(b"\x00\xff\xfe not a config at all \x00")
    assert ui_state.load(str(path)) == ui_state.DEFAULTS


def test_truncated_write_yields_defaults_for_missing_keys(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_text("view = men")           # cut mid-value
    got = ui_state.load(str(path))
    assert got["view"] == "men"             # harmless; restore_position clamps
    assert got["companion"] == 0


def test_unknown_keys_are_ignored(tmp_path):
    path = tmp_path / "ui-state.conf"
    path.write_text("view = menu1\nbypass_dly = 0\nvol_level = 0.7\n")
    got = ui_state.load(str(path))
    assert set(got) == set(ui_state.DEFAULTS)


def test_no_engine_state_keys_exist():
    """Guard rail: the engine owns effect/param state and rebroadcasts it on
    the CC102 sync. If a DSP-shaped key ever shows up here, we have grown a
    second source of truth."""
    banned = ("state", "bypass", "param", "level", "gain", "preset", "lp",
              "nam_sel", "ir_sel")
    for key in ui_state.DEFAULTS:
        assert not any(b in key for b in banned), key


# ------------------------------------------------------------------ writing

def test_save_creates_missing_directory(tmp_path):
    path = str(tmp_path / "deep" / "dir" / "ui-state.conf")
    assert ui_state.save(dict(ui_state.DEFAULTS), path) is True
    assert os.path.exists(path)


def test_save_leaves_no_temp_files(tmp_path):
    path = str(tmp_path / "ui-state.conf")
    ui_state.save(dict(ui_state.DEFAULTS), path)
    assert [p.name for p in tmp_path.iterdir()] == ["ui-state.conf"]


def test_save_failure_is_swallowed(tmp_path):
    # a directory where the file should be: every write attempt fails
    bad = tmp_path / "ui-state.conf"
    bad.mkdir()
    assert ui_state.save(dict(ui_state.DEFAULTS), str(bad)) is False


def test_state_path_env_override(monkeypatch):
    monkeypatch.setenv("NEPTR_UI_STATE", "/tmp/whatever.conf")
    assert ui_state.state_path() == "/tmp/whatever.conf"


# -------------------------------------------------------------------- Store

def _store(tmp_path, debounce=2.0):
    return ui_state.Store(str(tmp_path / "ui-state.conf"), debounce=debounce)


def test_store_debounces_the_write(tmp_path):
    s = _store(tmp_path)
    snap = dict(ui_state.DEFAULTS, view="menu4")
    assert s.tick(0.0, snap) is False       # noticed, not written
    assert s.tick(1.0, snap) is False       # still inside the window
    assert s.tick(2.0, snap) is True        # settled -> written
    assert ui_state.load(s.path)["view"] == "menu4"


def test_store_restarts_the_window_while_moving(tmp_path):
    s = _store(tmp_path)
    s.tick(0.0, dict(ui_state.DEFAULTS, view="menu1"))
    s.tick(1.5, dict(ui_state.DEFAULTS, view="menu2"))
    assert s.tick(2.0, dict(ui_state.DEFAULTS, view="menu2")) is False
    assert s.tick(3.5, dict(ui_state.DEFAULTS, view="menu2")) is True
    assert ui_state.load(s.path)["view"] == "menu2"


def test_store_never_rewrites_an_unchanged_position(tmp_path):
    s = _store(tmp_path)
    snap = dict(ui_state.DEFAULTS, view="menu4")
    s.tick(0.0, snap)
    s.tick(2.0, snap)
    for i in range(20):
        s.tick(3.0 + i, snap)
    assert s.writes == 1


def test_store_flush_writes_immediately(tmp_path):
    s = _store(tmp_path)
    snap = dict(ui_state.DEFAULTS, view="menu9")
    s.tick(0.0, snap)
    assert s.flush() is True
    assert ui_state.load(s.path)["view"] == "menu9"


def test_store_flush_is_a_noop_when_nothing_changed(tmp_path):
    s = _store(tmp_path)
    snap = dict(ui_state.DEFAULTS, view="menu9")
    s.tick(0.0, snap)
    s.tick(2.0, snap)
    assert s.flush(snap) is False


# ─────────── tab identity is a NAME, not an index (migration) ───────────

def test_tab_is_persisted_by_name():
    import companion
    assert ui_state.DEFAULTS["companion_screen"] == "DASHBOARD"
    assert companion.screen_name(companion.NETWORK) == "NETWORK"
    assert companion.screen_index("NETWORK") == companion.NETWORK


def test_stale_integer_tab_falls_back_to_the_default_not_a_wrong_tab(tmp_path):
    """Files written before the rename hold an INTEGER. Under the old order 3
    was AUDIO; under the new one 3 is SYSTEM. Resolving it as an index would
    silently drop the user on a tab they never chose, so an unrecognised value
    must go to the default tab instead."""
    import companion
    path = tmp_path / "ui-state.conf"
    path.write_text("view = menu0\ncompanion_screen = 3\ncompanion_sel = 0\n")
    loaded = ui_state.load(str(path))
    assert companion.screen_index(loaded["companion_screen"]) == companion.DASHBOARD


def test_unknown_or_removed_tab_name_falls_back(tmp_path):
    """WAVE was a tab and is not one any more — a file naming it must not
    raise and must not resolve to whatever now sits at its old index."""
    import companion
    for value in ("WAVE", "POWER", "", "garbage", None, 4, "4"):
        assert companion.screen_index(value) == companion.DASHBOARD


def test_every_screen_name_round_trips():
    import companion
    for i in range(len(companion.SCREENS)):
        assert companion.screen_index(companion.screen_name(i)) == i
