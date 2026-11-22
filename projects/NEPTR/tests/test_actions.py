"""Tests for actions — patch listing and patch/power dispatch.

All side-effecting commands go through an injected `runner` so tests never
shell out. The UI wires real commands in; actions.py never edits live.conf or
demiurge-power.service directly (the user owns those).
"""

import actions


# ------------------------------------------------------------- patch listing

def test_list_patches_returns_sorted_conf_names(tmp_path):
    (tmp_path / "SURF_NEPTR.conf").write_text("x")
    (tmp_path / "CAR_MODE.conf").write_text("x")
    (tmp_path / "notes.md").write_text("x")           # ignored
    (tmp_path / "live.conf").write_text("x")          # the active file, excluded
    assert actions.list_patches(str(tmp_path)) == ["CAR_MODE", "SURF_NEPTR"]


def test_list_patches_missing_dir_is_empty():
    assert actions.list_patches("/no/such/dir") == []


def test_list_patches_excludes_nam(tmp_path):
    (tmp_path / "car_mode.conf").write_text("x")
    (tmp_path / "nam.conf").write_text("x")
    (tmp_path / "neptr.conf").write_text("x")
    (tmp_path / "testing_ground.conf").write_text("x")
    result = actions.list_patches(str(tmp_path))
    assert "nam" not in result
    assert result == ["car_mode", "neptr", "testing_ground"]


# --------------------------------------------------------------- patch power

def test_patch_power_reads_tag(tmp_path):
    (tmp_path / "car_mode.conf").write_text("# set: car_mode\n# power: low\n")
    assert actions.patch_power(str(tmp_path), "car_mode") == "low"


def test_patch_power_defaults_to_high_when_tag_missing(tmp_path):
    (tmp_path / "neptr_surf.conf").write_text("# set: neptr_surf\nlink = off\n")
    assert actions.patch_power(str(tmp_path), "neptr_surf") == "high"


def test_patch_power_defaults_when_file_missing():
    assert actions.patch_power("/no/such/dir", "whatever") == "high"


# ------------------------------------------------------------- patch activate

# runner contract: runner(cmd: list[str]) -> (returncode: int, stdout: str)

def test_activate_patch_runs_templated_command():
    calls = []
    act = actions.Activator("demiurge patch {name}",
                            runner=lambda cmd: calls.append(cmd) or (0, ""))
    assert act.activate("SURF_NEPTR") is True
    assert calls == [["demiurge", "patch", "SURF_NEPTR"]]


def test_activate_patch_reports_failure():
    act = actions.Activator("demiurge patch {name}", runner=lambda cmd: (1, ""))
    assert act.activate("SURF_NEPTR") is False


# ----------------------------------------------------- generic command action

def test_command_action_runs_templated_with_args():
    calls = []
    a = actions.CommandAction("demiurge-power {level}",
                              runner=lambda cmd: calls.append(cmd) or (0, ""))
    assert a.run(level="high") is True
    assert calls == [["demiurge-power", "high"]]


def test_command_action_runs_plain_command():
    calls = []
    a = actions.CommandAction("sudo systemctl restart demiurge",
                              runner=lambda cmd: calls.append(cmd) or (0, ""))
    assert a.run() is True
    assert calls == [["sudo", "systemctl", "restart", "demiurge"]]


def test_command_action_reports_failure():
    a = actions.CommandAction("x", runner=lambda cmd: (1, ""))
    assert a.run() is False


def test_from_env_defaults(monkeypatch):
    for v in ("NEPTR_PATCHES_DIR", "NEPTR_PATCH_CMD",
              "NEPTR_POWER_SET_CMD", "NEPTR_RESTART_CMD"):
        monkeypatch.delenv(v, raising=False)
    cfg = actions.from_env()
    assert cfg.patches_dir.endswith("demiurge/sets")
    assert isinstance(cfg.activator, actions.Activator)
    assert isinstance(cfg.power_set, actions.CommandAction)
    assert isinstance(cfg.restart, actions.CommandAction)
    # patches switch via demiurge-set (full path — not on UI service PATH)
    assert "demiurge-set" in cfg.activator._template


def test_activator_runs_demiurge_set():
    calls = []
    act = actions.Activator("/opt/demiurge/bin/demiurge-set {name}",
                            runner=lambda cmd: calls.append(cmd) or (0, ""))
    assert act.activate("neptr_surf") is True
    assert calls == [["/opt/demiurge/bin/demiurge-set", "neptr_surf"]]


# ------------------------------------------------------------ active patch

def _sets(tmp_path, **files):
    sets_dir = tmp_path / "sets"
    sets_dir.mkdir()
    for name, body in files.items():
        (sets_dir / (name + ".conf")).write_text(body)
    return str(sets_dir)


def test_active_patch_matches_the_set_tag(tmp_path):
    sets_dir = _sets(tmp_path, car_mode="# set: car_mode\naudio=on\n",
                     neptr_surf="# set: neptr_surf\naudio=on\n")
    (tmp_path / "live.conf").write_text("# set: neptr_surf\naudio=on\n")
    assert actions.active_patch(sets_dir) == "neptr_surf"


def test_active_patch_matches_a_tag_that_differs_from_the_filename(tmp_path):
    sets_dir = _sets(tmp_path, alpha="# set: THE RIG\n")
    (tmp_path / "live.conf").write_text("# set: THE RIG\n")
    assert actions.active_patch(sets_dir) == "alpha"


def test_active_patch_accepts_the_legacy_patch_tag(tmp_path):
    sets_dir = _sets(tmp_path, neptr_bass="# patch: neptr_bass\n")
    (tmp_path / "live.conf").write_text("# patch: neptr_bass\n")
    assert actions.active_patch(sets_dir) == "neptr_bass"


def test_active_patch_falls_back_to_byte_equality(tmp_path):
    sets_dir = _sets(tmp_path, car_mode="power=low\n", neptr_surf="power=high\n")
    (tmp_path / "live.conf").write_text("power=high\n")
    assert actions.active_patch(sets_dir) == "neptr_surf"


def test_active_patch_unknown_config_is_none(tmp_path):
    sets_dir = _sets(tmp_path, car_mode="# set: car_mode\n")
    (tmp_path / "live.conf").write_text("# set: something_else\nx=1\n")
    assert actions.active_patch(sets_dir) is None


def test_active_patch_missing_live_conf_is_none(tmp_path):
    sets_dir = _sets(tmp_path, car_mode="# set: car_mode\n")
    assert actions.active_patch(sets_dir) is None


def test_active_patch_ignores_a_tag_below_the_header(tmp_path):
    """A '# set:' comment further down a file is not the name tag."""
    sets_dir = _sets(tmp_path, a="audio=on\n# set: car_mode\n")
    (tmp_path / "live.conf").write_text("# set: car_mode\n")
    assert actions.active_patch(sets_dir) is None


# ─────────────── engine settings (demiurge-audio is the write path) ───────

class _Rec:
    """Runner that records argv and any stdin, and replays canned output."""

    def __init__(self, rc=0, out=""):
        self.rc, self.out = rc, out
        self.calls, self.stdins = [], []

    def __call__(self, cmd, stdin=None):
        self.calls.append(cmd)
        self.stdins.append(stdin)
        return self.rc, self.out


AUDIO_LIST = (
    "mode|sync|sync|sync,bypass|sync: full audio graph\n"
    "bits|32|32|16,32,float|sample format sent to the device\n"
    "quantum|128|128|32,64,128|CPU headroom only — the backend pins its ring\n"
    "rate|48000|48000|44100,48000|sample rate of the whole graph\n"
)


def test_audio_settings_parses_every_row():
    r = _Rec(out=AUDIO_LIST)
    s = actions.audio_settings(runner=r)
    assert set(s) == {"mode", "bits", "quantum", "rate"}
    assert s["bits"]["value"] == "32"
    assert s["bits"]["options"] == ["16", "32", "float"]


def test_audio_settings_never_reports_24_bit():
    s = actions.audio_settings(runner=_Rec(out=AUDIO_LIST))
    assert "24" not in s["bits"]["options"]


def test_audio_settings_keeps_the_effect_sentence_verbatim():
    s = actions.audio_settings(runner=_Rec(out=AUDIO_LIST))
    assert s["quantum"]["effect"] == \
        "CPU headroom only — the backend pins its ring"


def test_audio_settings_failure_is_empty_not_an_exception():
    assert actions.audio_settings(runner=_Rec(rc=1, out="boom")) == {}


def test_audio_settings_skips_malformed_lines():
    s = actions.audio_settings(runner=_Rec(out="junk\nbits|16|32|16,32|f\n"))
    assert list(s) == ["bits"] and s["bits"]["value"] == "16"


def test_set_audio_shells_out_to_the_cli():
    r = _Rec()
    assert actions.set_audio("quantum", 256, runner=r) is True
    assert r.calls[0][-2:] == ["quantum", "256"]
    assert r.calls[0][0].endswith("demiurge-audio")


def test_set_audio_reports_cli_rejection():
    assert actions.set_audio("bits", "24", runner=_Rec(rc=2)) is False


# ─────────────────────────────── wifi ───────────────────────────────

WIFI_LIST = ("HomeNet|80|wpa|active,saved\n"
             "Venue Guest|44|open|\n"
             "junk\n")


def test_wifi_list_parses_flags_and_spaces():
    nets = actions.wifi_list(runner=_Rec(out=WIFI_LIST))
    assert len(nets) == 2
    assert nets[0] == {"ssid": "HomeNet", "signal": 80, "security": "wpa",
                       "active": True, "saved": True}
    assert nets[1]["ssid"] == "Venue Guest"


def test_wifi_list_rescan_uses_the_slow_flag():
    r = _Rec(out="")
    actions.wifi_list(rescan=True, runner=r)
    assert r.calls[0][-1] == "--scan"
    r2 = _Rec(out="")
    actions.wifi_list(runner=r2)
    assert r2.calls[0][-1] == "--list"


def test_wifi_status_parses_and_rejects_malformed():
    ok = actions.wifi_status(
        runner=_Rec(out="connected|HomeNet|80|wpa|192.168.1.7|wlan0\n"))
    assert ok["ssid"] == "HomeNet" and ok["ip"] == "192.168.1.7"
    assert actions.wifi_status(runner=_Rec(out="connected|HomeNet\n")) is None
    assert actions.wifi_status(runner=_Rec(rc=1)) is None


def test_wifi_join_saved_network_sends_no_password():
    r = _Rec()
    ok, _msg = actions.wifi_join("HomeNet", None, runner=r)
    assert ok is True
    assert r.calls[0][-2:] == ["join", "HomeNet"]
    assert r.stdins == [None]


def test_wifi_join_password_travels_on_stdin_never_argv():
    r = _Rec()
    secret = "hunter2hunter2"
    ok, _msg = actions.wifi_join("CoffeeShop", secret, runner=r)
    assert ok is True
    assert r.calls[0][-1] == "--password-stdin"
    assert r.stdins == [secret + "\n"]
    # the secret must appear in NO argument — argv is world-readable in ps
    assert not any(secret in arg for arg in r.calls[0])


def test_wifi_join_rejects_a_short_password_without_running_anything():
    r = _Rec()
    ok, msg = actions.wifi_join("CoffeeShop", "short", runner=r)
    assert ok is False and r.calls == []
    assert "8-63" in msg
    assert "short" not in msg          # never echo the secret back


def test_wifi_join_failure_surfaces_the_cli_message():
    r = _Rec(rc=1, out="demiurge-wifi: could not join 'CoffeeShop'\n")
    ok, msg = actions.wifi_join("CoffeeShop", None, runner=r)
    assert ok is False and "could not join" in msg


def test_wifi_forget():
    r = _Rec()
    assert actions.wifi_forget("OldNet", runner=r) is True
    assert r.calls[0][-2:] == ["forget", "OldNet"]
