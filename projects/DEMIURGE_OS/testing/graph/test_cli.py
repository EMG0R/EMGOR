"""demiurge-graph CLI tests.

The tool wraps pw-link, so every test here stands up a FAKE pw-link on PATH
that replays a captured `pw-link -l` listing. That keeps the suite runnable on
a dev Mac with no PipeWire, and lets us pin the two shapes that matter:

  healthy   engine in and out both wired straight to the interface (DIRECT)
  broken    the engine's inputs connected to NOTHING — the exact state the
            device was silently in on 2026-08-09, which is why this whole
            feature exists. If --list ever stops emitting warn|no-input for
            this fixture, the UI stops shouting and the bug becomes invisible
            again.

Run: pytest testing/graph
"""
import os
import pathlib
import subprocess

import pytest

CLI = pathlib.Path(__file__).resolve().parents[2] / "src" / "wrappers" / "demiurge-graph"

# A single wired interface, engine in and out both direct, plus the loopback
# source tap the launcher leaves in place. Captured from the device.
HEALTHY = """\
demiurge-source-input:input_FL
  |<- alsa_input.plat:capture_FL
demiurge-source-input:input_FR
  |<- alsa_input.plat:capture_FR
alsa_output.plat:playback_FL
  |<- csound6:output1
alsa_output.plat:playback_FR
  |<- csound6:output2
alsa_input.plat:capture_FL
  |-> demiurge-source-input:input_FL
  |-> csound6:input1
alsa_input.plat:capture_FR
  |-> demiurge-source-input:input_FR
  |-> csound6:input2
csound6:output1
  |-> alsa_output.plat:playback_FL
csound6:output2
  |-> alsa_output.plat:playback_FR
csound6:input1
  |<- alsa_input.plat:capture_FL
csound6:input2
  |<- alsa_input.plat:capture_FR
"""

# The 2026-08-09 fault: output fine, inputs dangling.
BROKEN_INPUT = """\
alsa_output.plat:playback_FL
  |<- csound6:output1
alsa_output.plat:playback_FR
  |<- csound6:output2
alsa_input.plat:capture_FL
alsa_input.plat:capture_FR
csound6:output1
  |-> alsa_output.plat:playback_FL
csound6:output2
  |-> alsa_output.plat:playback_FR
csound6:input1
csound6:input2
"""

# Output routed through the loopback sink instead of straight to the card —
# the multi-interface / Bluetooth / aloop shape, one quantum slower out.
LOOPBACK_OUT = """\
demiurge-sink:playback_FL
  |<- csound6:output1
demiurge-sink:playback_FR
  |<- csound6:output2
csound6:output1
  |-> demiurge-sink:playback_FL
csound6:output2
  |-> demiurge-sink:playback_FR
csound6:input1
  |<- alsa_input.plat:capture_FL
csound6:input2
  |<- alsa_input.plat:capture_FR
alsa_input.plat:capture_FL
  |-> csound6:input1
alsa_input.plat:capture_FR
  |-> csound6:input2
"""


class Env:
    """A sandboxed demiurge-graph: fake pw-link, own live.conf and graph.conf."""

    def __init__(self, tmp_path, listing, sync_layer="on"):
        self.dir = tmp_path
        self.listing = tmp_path / "listing.txt"
        self.listing.write_text(listing)
        self.calls = tmp_path / "pw-link-calls.txt"
        self.live = tmp_path / "live.conf"
        self.live.write_text("sync_layer = %s\nrate    = 48000\n" % sync_layer)
        self.overrides = tmp_path / "graph.conf"

        bindir = tmp_path / "bin"
        bindir.mkdir()
        fake = bindir / "pw-link"
        # -o / -i are derived from the listing's unindented port lines, split
        # by whether the port name looks like a source or a sink. Every other
        # invocation (connect / disconnect) is recorded, not performed.
        fake.write_text(
            "#!/bin/bash\n"
            'L="%s"\n'
            'C="%s"\n'
            "case \"$1\" in\n"
            "  -o) grep -E '^[^ ].*:' \"$L\" | grep -E ':(output|capture)' ;;\n"
            "  -i) grep -E '^[^ ].*:' \"$L\" | grep -E ':(input|playback)' ;;\n"
            '  -l) cat "$L" ;;\n'
            '  *) echo "$*" >> "$C" ;;\n'
            "esac\n" % (self.listing, self.calls)
        )
        fake.chmod(0o755)
        self.bindir = bindir

    def run(self, *args):
        env = dict(os.environ)
        env["PATH"] = "%s:%s" % (self.bindir, env["PATH"])
        env["DEMIURGE_LIVE_CONF"] = str(self.live)
        env["DEMIURGE_GRAPH_CONF"] = str(self.overrides)
        return subprocess.run(
            ["bash", str(CLI), *args], capture_output=True, text=True, env=env, timeout=20
        )

    def pw_calls(self):
        if not self.calls.exists():
            return []
        return [l for l in self.calls.read_text().splitlines() if l]


def lines(out, kind):
    return [l for l in out.splitlines() if l.startswith(kind + "|")]


# ───────────────────────── machine output ─────────────────────────

def test_list_reports_sync_mode_and_every_link(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--list")
    assert r.returncode == 0
    assert "mode|sync" in r.stdout.splitlines()
    assert len(lines(r.stdout, "link")) == 6


def test_healthy_graph_is_direct_both_ways(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--list")
    assert "path|direct|direct" in r.stdout.splitlines()
    assert lines(r.stdout, "warn") == []


def test_engine_to_interface_link_is_classified_direct(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--list")
    link = [l for l in lines(r.stdout, "link") if "csound6:output1" in l][0]
    _, src, dst, path, flags = link.split("|")
    assert (src, dst, path) == ("csound6:output1", "alsa_output.plat:playback_FL", "direct")
    # engine-out because it leaves the engine; managed because the launcher
    # scrubs unexpected sources off hardware playback ports.
    assert set(flags.split(",")) == {"engine-out", "managed"}


def test_loopback_output_is_not_reported_as_direct(tmp_path):
    r = Env(tmp_path, LOOPBACK_OUT).run("--list")
    assert "path|direct|loopback" in r.stdout.splitlines()


def test_status_machine_is_one_line(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--status-machine")
    assert r.stdout.strip() == "sync|direct|direct|0|6"


def test_port_lines_carry_node_kind_and_link_count(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--list")
    p = [l for l in lines(r.stdout, "port") if l.startswith("port|csound6:input1|")][0]
    _, full, node, direction, kind, disp, n = p.split("|")
    assert (node, direction, kind, disp, n) == ("csound6", "in", "engine", "IN 1", "1")


def test_hardware_nodes_never_name_a_product(tmp_path):
    """Interface identity is deliberately generic here — see the standing
    instruction that no latency/graph surface names an interface brand."""
    r = Env(tmp_path, HEALTHY).run("--list")
    for l in lines(r.stdout, "node"):
        _, name, kind, display = l.split("|")
        if kind == "interface":
            assert display in ("Interface in", "Interface out")


# ───────────────────────── the fault this exists for ─────────────────────────

def test_dangling_engine_input_is_warned_about(tmp_path):
    r = Env(tmp_path, BROKEN_INPUT).run("--list")
    warns = lines(r.stdout, "warn")
    assert any(w.startswith("warn|no-input|") for w in warns)
    assert sum(w.startswith("warn|dangling|") for w in warns) == 2
    assert "path|none|direct" in r.stdout.splitlines()


def test_dangling_shows_up_in_status_machine(tmp_path):
    r = Env(tmp_path, BROKEN_INPUT).run("--status-machine")
    mode, ip, op, dangling, _ = r.stdout.strip().split("|")
    assert (mode, ip, op, dangling) == ("sync", "none", "direct", "2")


def test_human_output_shouts_about_silence(tmp_path):
    r = Env(tmp_path, BROKEN_INPUT).run()
    assert "NOTHING YOU PLAY REACHES THE ENGINE" in r.stdout
    assert "demiurge-graph reset" in r.stdout


# ───────────────────────── no graph at all ─────────────────────────

def test_bypass_says_the_graph_only_exists_in_sync_mode(tmp_path):
    r = Env(tmp_path, HEALTHY, sync_layer="off").run()
    assert r.returncode == 3
    assert "sync layer is off" in r.stdout.lower()
    assert "pw-link" not in r.stdout          # no raw tool spew at the user


def test_bypass_list_is_still_parseable(tmp_path):
    r = Env(tmp_path, HEALTHY, sync_layer="off").run("--list")
    assert r.returncode == 3
    assert r.stdout.strip() == "mode|bypass"


def test_bypass_status_machine_keeps_the_field_count(tmp_path):
    r = Env(tmp_path, HEALTHY, sync_layer="off").run("--status-machine")
    assert r.stdout.strip().split("|") == ["bypass", "-", "-", "-", "-"]


def test_bypass_refuses_to_connect(tmp_path):
    e = Env(tmp_path, HEALTHY, sync_layer="off")
    r = e.run("connect", "csound6:output1", "alsa_output.plat:playback_FL")
    assert r.returncode == 3
    assert e.pw_calls() == []


# ───────────────────────── writes ─────────────────────────

def test_connect_calls_pw_link_once_and_records_the_override(tmp_path):
    e = Env(tmp_path, BROKEN_INPUT)
    r = e.run("connect", "alsa_input.plat:capture_FL", "csound6:input1")
    assert r.returncode == 0
    assert e.pw_calls() == ["alsa_input.plat:capture_FL csound6:input1"]
    assert "connect = alsa_input.plat:capture_FL -> csound6:input1" in e.overrides.read_text()


def test_connect_to_a_policed_destination_admits_it_may_be_reverted(tmp_path):
    e = Env(tmp_path, BROKEN_INPUT)
    r = e.run("connect", "alsa_input.plat:capture_FL", "csound6:input1")
    assert "polices this destination" in r.stdout


def test_connect_is_idempotent(tmp_path):
    e = Env(tmp_path, HEALTHY)
    r = e.run("connect", "csound6:output1", "alsa_output.plat:playback_FL")
    assert r.returncode == 0 and "already connected" in r.stdout
    assert e.pw_calls() == []


def test_unknown_port_is_refused_before_touching_pw_link(tmp_path):
    e = Env(tmp_path, HEALTHY)
    r = e.run("connect", "typo6:output1", "csound6:input1")
    assert r.returncode == 1
    assert "not a port" in r.stderr
    assert e.pw_calls() == []


def test_direction_is_enforced(tmp_path):
    e = Env(tmp_path, HEALTHY)
    r = e.run("connect", "csound6:input1", "csound6:input2")
    assert r.returncode == 1
    assert "source must be an output port" in r.stderr
    assert e.pw_calls() == []

    r = e.run("connect", "csound6:output1", "csound6:output2")
    assert r.returncode == 1
    assert "destination must be an input port" in r.stderr
    assert e.pw_calls() == []


def test_missing_operand_is_refused(tmp_path):
    e = Env(tmp_path, HEALTHY)
    assert e.run("connect", "csound6:output1").returncode == 1
    assert e.pw_calls() == []


def test_disconnecting_the_last_engine_output_link_warns_about_silence(tmp_path):
    e = Env(tmp_path, HEALTHY)
    r = e.run("disconnect", "csound6:output1", "alsa_output.plat:playback_FL")
    assert r.returncode == 0
    assert "This is silence" in r.stderr
    assert e.pw_calls() == ["-d csound6:output1 alsa_output.plat:playback_FL"]


def test_disconnecting_the_last_engine_input_link_warns(tmp_path):
    e = Env(tmp_path, HEALTHY)
    r = e.run("disconnect", "alsa_input.plat:capture_FL", "csound6:input1")
    assert "Nothing you play will reach the engine" in r.stderr
    assert "demiurge-graph reset" in r.stdout


def test_disconnecting_a_link_that_is_not_there_is_a_no_op(tmp_path):
    e = Env(tmp_path, BROKEN_INPUT)
    r = e.run("disconnect", "alsa_input.plat:capture_FL", "csound6:input1")
    assert r.returncode == 0 and "not connected" in r.stdout
    assert e.pw_calls() == []


def test_disconnect_forgets_a_matching_override(tmp_path):
    e = Env(tmp_path, HEALTHY)
    e.overrides.write_text(
        "# header\nconnect = csound6:output1 -> alsa_output.plat:playback_FL\n"
        "connect = csound6:output2 -> alsa_output.plat:playback_FR\n"
    )
    e.run("disconnect", "csound6:output1", "alsa_output.plat:playback_FL")
    left = e.overrides.read_text()
    assert "csound6:output1 -> alsa_output.plat:playback_FL" not in left
    assert "csound6:output2 -> alsa_output.plat:playback_FR" in left


# ───────────────────────── reset ─────────────────────────

def test_reset_clears_overrides_and_touches_live_conf(tmp_path):
    e = Env(tmp_path, HEALTHY)
    e.overrides.write_text("connect = a:out -> b:in\n")
    before = e.live.read_text()
    old_mtime = e.live.stat().st_mtime
    os.utime(e.live, (old_mtime - 10, old_mtime - 10))

    r = e.run("reset")
    assert r.returncode == 0
    assert not e.overrides.exists()
    # The launcher owns the intended graph; reset only asks it to re-assert.
    # live.conf CONTENT must be untouched — a reset that edited the patch's
    # config would be a second, competing source of truth.
    assert e.live.read_text() == before
    assert e.live.stat().st_mtime > old_mtime - 10
    assert e.pw_calls() == []


def test_reset_does_not_restart_anything(tmp_path):
    """No systemctl: a reset must never drop audio. It rides the launcher's
    live.conf watcher, which reconciles an identical session (no respawn)."""
    e = Env(tmp_path, HEALTHY)
    r = e.run("reset")
    assert "systemctl" not in r.stdout and "systemctl" not in r.stderr


# ───────────────────────── misc ─────────────────────────

def test_unknown_argument_is_refused(tmp_path):
    r = Env(tmp_path, HEALTHY).run("frobnicate")
    assert r.returncode == 1
    assert "unknown argument" in r.stderr


def test_help_prints_usage(tmp_path):
    r = Env(tmp_path, HEALTHY).run("--help")
    assert r.returncode == 0
    assert "demiurge-graph connect" in r.stdout


# ───────────────────────── MIDI is not audio ─────────────────────────
#
# Regression (found on the device 2026-08-10, first run of this tool): the
# real graph carries a dozen MIDI ports (Midi-Bridge, bluez_midi) that are
# unconnected as a matter of course. Counting them as dangling audio buried
# the ONE port that actually meant silence under thirteen that did not.

MIDI_NOISE = """\
Midi-Bridge:Midi Through Port-0 (capture)
Midi-Bridge:Csound (capture)
bluez_midi.server:out
Midi-Bridge:Midi Through Port-0 (playback)
Midi-Bridge:Csound (playback)
bluez_midi.server:in
csound6:output1
  |-> alsa_output.plat:playback_FL
alsa_output.plat:playback_FL
  |<- csound6:output1
csound6:input1
  |<- alsa_input.plat:capture_FL
alsa_input.plat:capture_FL
  |-> csound6:input1
"""


class MidiEnv(Env):
    """pw-link -o/-i split that keeps the MIDI ports, like the real one."""

    def __init__(self, tmp_path):
        super().__init__(tmp_path, MIDI_NOISE)
        (self.bindir / "pw-link").write_text(
            "#!/bin/bash\n"
            'L="%s"\nC="%s"\n'
            "case \"$1\" in\n"
            "  -o) grep -E '^[^ ]' \"$L\" | grep -E ':(output|capture|out$)|\\\\(capture\\\\)' ;;\n"
            "  -i) grep -E '^[^ ]' \"$L\" | grep -E ':(input|playback|in$)|\\\\(playback\\\\)' ;;\n"
            '  -l) cat "$L" ;;\n'
            '  *) echo "$*" >> "$C" ;;\n'
            "esac\n" % (self.listing, self.calls))
        (self.bindir / "pw-link").chmod(0o755)


def test_unconnected_midi_ports_are_not_reported_as_silence(tmp_path):
    r = MidiEnv(tmp_path).run("--list")
    assert lines(r.stdout, "warn") == []


def test_midi_ports_are_typed_as_midi_not_engine(tmp_path):
    r = MidiEnv(tmp_path).run("--list")
    for l in lines(r.stdout, "port"):
        _, full, _node, _dir, kind, _disp, _n = l.split("|")
        if "Midi-Bridge" in full or "bluez_midi" in full:
            assert kind == "midi", full


def test_midi_does_not_change_the_reported_audio_path(tmp_path):
    r = MidiEnv(tmp_path).run("--status-machine")
    assert r.stdout.strip().split("|")[:4] == ["sync", "direct", "direct", "0"]


def test_it_derives_the_pipewire_runtime_dir_when_the_caller_has_none(tmp_path):
    """Regression (2026-08-10): demiurge-web's unit has no XDG_RUNTIME_DIR, so
    pw-link found no daemon and the page reported "no graph" on a machine
    whose graph was plainly running. Reporting a missing socket as a missing
    graph is the invisible-state failure this tool exists to prevent, so the
    tool derives the path itself."""
    e = Env(tmp_path, HEALTHY)
    seen = tmp_path / "runtime.txt"
    (e.bindir / "pw-link").write_text(
        "#!/bin/bash\n"
        'echo "${XDG_RUNTIME_DIR:-UNSET}" > "%s"\n'
        'L="%s"\n'
        "case \"$1\" in\n"
        "  -o) grep -E '^[^ ].*:' \"$L\" | grep -E ':(output|capture)' ;;\n"
        "  -i) grep -E '^[^ ].*:' \"$L\" | grep -E ':(input|playback)' ;;\n"
        '  -l) cat "$L" ;;\n'
        "esac\n" % (seen, e.listing))
    (e.bindir / "pw-link").chmod(0o755)

    env = dict(os.environ)
    env.pop("XDG_RUNTIME_DIR", None)
    env["PATH"] = "%s:%s" % (e.bindir, env["PATH"])
    env["DEMIURGE_LIVE_CONF"] = str(e.live)
    env["DEMIURGE_GRAPH_CONF"] = str(e.overrides)
    subprocess.run(["bash", str(CLI), "--list"], capture_output=True,
                   text=True, env=env, timeout=20)
    got = seen.read_text().strip()
    assert got != "UNSET" and got.startswith("/run/user/")


def test_every_port_gets_a_non_empty_label(tmp_path):
    """Regression (2026-08-10, on the device): the surround channels of the
    loopback nodes (monitor_RL, capture_RR, …) fell through every branch and
    rendered as blank rows. An unlabelled row in a patchbay is one the user
    cannot identify, which is worse than a long one."""
    listing = "\n".join([
        "demiurge-sink:monitor_RL", "demiurge-sink:monitor_RR",
        "demiurge-source:capture_RL", "csound6:output1", "rnbo:out_0",
        "demiurge-sink:playback_RL", "csound6:input1", "rnbo:in_0",
    ]) + "\n"
    e = Env(tmp_path, listing)
    (e.bindir / "pw-link").write_text(
        "#!/bin/bash\n"
        'L="%s"\n'
        "case \"$1\" in\n"
        "  -o) grep -E ':(output|capture|monitor|out_)' \"$L\" ;;\n"
        "  -i) grep -E ':(input|playback|in_)' \"$L\" ;;\n"
        '  -l) cat "$L" ;;\n'
        "esac\n" % e.listing)
    (e.bindir / "pw-link").chmod(0o755)
    r = e.run("--list")
    for l in lines(r.stdout, "port"):
        parts = l.split("|")
        assert parts[5].strip(), "blank label for %s" % parts[1]
    labels = {l.split("|")[1]: l.split("|")[5] for l in lines(r.stdout, "port")}
    assert labels["demiurge-sink:monitor_RL"] == "MON RL"
    assert labels["csound6:output1"] == "OUT 1"
    assert labels["rnbo:out_0"] == "OUT 0"
