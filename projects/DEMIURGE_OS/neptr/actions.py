"""Patch listing and patch/power dispatch for the companion menu.

Everything that changes system state runs an external command through an
injected `runner`, so the UI stays decoupled from the exact demiurge CLI and
this module is fully unit-testable. Crucially, the companion NEVER edits
live.conf or demiurge-power.service itself — those are owned elsewhere. The UI
only *triggers* a command the user supplies (env-overridable), so power-mode
ownership stays where it belongs.

Runner contract:  runner(cmd: list[str]) -> (returncode: int, stdout: str)
"""

import os
import shlex
import subprocess


def default_runner(cmd, stdin=None):
    """Real runner: run cmd, return (returncode, stdout).

    `stdin` exists for exactly one caller: the Wi-Fi password. Secrets go down
    the pipe, never into `cmd` — argv is world-readable in `ps` and this is the
    only way the UI can hand a password to demiurge-wifi without leaking it."""
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30,
                             input=stdin)
        return out.returncode, out.stdout
    except (OSError, subprocess.SubprocessError) as exc:
        return 1, str(exc)


# ----------------------------------------------------------------- patches

# Config files that live in ~/demiurge/sets/ (via demiurge-set) but should NOT
# appear in the NEPTR companion patches screen (they have their own UI / flow).
_HIDDEN_PATCHES = {"nam"}


def list_patches(patches_dir):
    """Sorted names of *.conf patch files in patches_dir (without extension).
    live.conf (the active one) and _HIDDEN_PATCHES are excluded; missing dir yields []."""
    try:
        names = [
            f[:-5] for f in os.listdir(patches_dir)
            if f.endswith(".conf") and f != "live.conf" and f[:-5] not in _HIDDEN_PATCHES
        ]
    except OSError:
        return []
    return sorted(names)


def _set_tag(path):
    """The '# set:' (or legacy '# patch:') name tag of a demiurge config, or
    None. Same tag `demiurge-set` prints as "Active" and the terminal
    companion / web UI already use to mark the current patch."""
    try:
        with open(path) as fh:
            for line in fh:
                line = line.strip()
                for prefix in ("# set:", "# patch:"):
                    if line.startswith(prefix):
                        return line.split(":", 1)[1].strip() or None
                if line and not line.startswith("#"):
                    return None            # header is over; no tag
    except OSError:
        pass
    return None


def active_patch(patches_dir, live_conf=None):
    """Name of the patch currently loaded into live.conf, or None.

    `demiurge-set <name>` simply copies sets/<name>.conf over live.conf, so the
    active patch is identified exactly the way every other DEMIURGE UI does it
    (terminal companion, web UI): by matching live.conf's `# set:` tag against
    the sets. Falls back to a byte comparison for untagged sets, so an old
    hand-made config still resolves.

    Without this the PATCHES screen has no idea which patch is loaded, the
    cursor sits on row 0, and the alphabetically-first patch (car_mode) looks
    permanently selected — the 2026-08-10 "it keeps going to car mode" report.
    """
    live = live_conf or os.path.join(os.path.dirname(
        os.path.abspath(patches_dir.rstrip("/"))), "live.conf")
    tag = _set_tag(live)
    names = list_patches(patches_dir)
    if tag:
        if tag in names:
            return tag
        for name in names:
            if _set_tag(os.path.join(patches_dir, name + ".conf")) == tag:
                return name
    try:
        with open(live, "rb") as fh:
            live_bytes = fh.read()
    except OSError:
        return None
    for name in names:
        try:
            with open(os.path.join(patches_dir, name + ".conf"), "rb") as fh:
                if fh.read() == live_bytes:
                    return name
        except OSError:
            continue
    return None


def patch_power(patches_dir, name, default="high"):
    """Read the '# power:' header tag from patches_dir/name.conf (low|medium|high).
    Missing tag or unreadable file falls back to `default`."""
    try:
        with open(os.path.join(patches_dir, name + ".conf")) as fh:
            for line in fh:
                line = line.strip()
                if line.startswith("# power:"):
                    return line.split(":", 1)[1].strip() or default
    except OSError:
        pass
    return default


class CommandAction:
    """Runs a configurable command, optionally templated with {kwargs}.
    Used for power-mode set ('demiurge-power {level}') and restart. Never edits
    config files — it only triggers a command the user owns."""

    def __init__(self, cmd_template, runner=default_runner):
        self._template = cmd_template
        self._runner = runner

    def run(self, **kw):
        safe = {k: shlex.quote(str(v)) for k, v in kw.items()}
        cmd = shlex.split(self._template.format(**safe))
        rc, _ = self._runner(cmd)
        return rc == 0


class Activator(CommandAction):
    """Patch switch: CommandAction with an activate(name) convenience."""

    def activate(self, name):
        return self.run(name=name)


# -------------------------------------------------------------- interfaces

# Audio-interface selection shells out to the demiurge-interface CLI, which
# owns the choice (and restarts the demiurge service itself) — the companion
# never edits live.conf. Interface names are runtime data from the CLI; no
# brand name is ever hardcoded here.

IFACE_LIST_CMD_DEFAULT = "/usr/local/bin/demiurge-interface --list"
IFACE_SET_CMD_DEFAULT = "/usr/local/bin/demiurge-interface {name}"


def _iface_list_cmd():
    return os.environ.get("NEPTR_IFACE_LIST_CMD", IFACE_LIST_CMD_DEFAULT)


def _iface_set_cmd():
    return os.environ.get("NEPTR_IFACE_SET_CMD", IFACE_SET_CMD_DEFAULT)


def list_interfaces(runner=default_runner):
    """Interfaces known to `demiurge-interface --list`: one dict per line of
    `name|card_id|flags` (flags = comma-joined subset of active/chosen/inuse):

      {"name": str, "id": str, "active": bool, "chosen": bool, "inuse": bool}

    A missing or failing command yields [] — the UI shows an empty list."""
    rc, out = runner(shlex.split(_iface_list_cmd()))
    if rc != 0:
        return []
    ifaces = []
    for line in out.splitlines():
        line = line.strip()
        if not line or "|" not in line:
            continue
        parts = line.split("|")
        name = parts[0].strip()
        if not name:
            continue
        flags = {f.strip() for f in parts[2].split(",")} if len(parts) > 2 else set()
        ifaces.append({
            "name": name,
            "id": parts[1].strip() if len(parts) > 1 else "",
            "active": "active" in flags,
            "chosen": "chosen" in flags,
            "inuse": "inuse" in flags,
        })
    return ifaces


def set_interface(name, runner=default_runner):
    """Switch to interface `name`. The CLI takes the name's words as separate
    argv items and restarts demiurge itself, so no quoting or follow-up
    restart is needed here."""
    head, _, tail = _iface_set_cmd().partition("{name}")
    cmd = shlex.split(head) + name.split() + shlex.split(tail)
    rc, _ = runner(cmd)
    return rc == 0


# --------------------------------------------------------- engine settings

# The four live.conf audio keys (sample format, buffer size, sample rate and
# the routing mode) are owned by the demiurge-audio CLI, exactly as interface
# selection is owned by demiurge-interface. The UI reads its --list and posts
# back through it; it never touches live.conf. Interface selection stays with
# demiurge-interface — the AUDIO screen shows both, but each has one owner.

AUDIO_LIST_CMD_DEFAULT = "/usr/local/bin/demiurge-audio --list"
AUDIO_SET_CMD_DEFAULT = "/usr/local/bin/demiurge-audio"

# Order the rows appear in on the AUDIO screen, with musician-facing labels.
AUDIO_ROWS = (("mode", "MODE"), ("quantum", "BUFFER"),
              ("bits", "FORMAT"), ("rate", "RATE"))

# Changing these restarts the engine and can silence a running set, so the UI
# arms them with the same two-tap confirm the PATCH screen uses.
AUDIO_CONFIRM = frozenset(("mode", "rate"))


def audio_settings(runner=default_runner):
    """`demiurge-audio --list` -> {key: {value, default, options, effect}}.

    `effect` is the CLI's own sentence and the UI renders it verbatim. It is
    mode-aware: under `mode = bypass` the quantum row says the buffer size does
    NOT buy latency (the audio backend pins its ring and ignores the setting),
    while under `mode = sync` it gives the real figure. Composing that text in
    the UI would mean three front-ends each able to drift into a false claim.

    A missing or failing command yields {} — the UI hides the rows."""
    cmd = os.environ.get("NEPTR_AUDIO_LIST_CMD", AUDIO_LIST_CMD_DEFAULT)
    rc, out = runner(shlex.split(cmd))
    if rc != 0:
        return {}
    settings = {}
    for line in out.splitlines():
        parts = line.split("|", 4)
        if len(parts) != 5:
            continue
        key, value, default, options, effect = (p.strip() for p in parts)
        if not key or not value:
            continue
        settings[key] = {
            "value": value,
            "default": default,
            "options": [o for o in options.split(",") if o],
            "effect": effect,
        }
    return settings


def set_audio(key, value, runner=default_runner):
    """`demiurge-audio <key> <value>`. The CLI validates (it is the only place
    that knows 24-bit is illegal for csound's realtime ALSA) and restarts the
    engine itself."""
    cmd = os.environ.get("NEPTR_AUDIO_SET_CMD", AUDIO_SET_CMD_DEFAULT)
    rc, _ = runner(shlex.split(cmd) + [str(key), str(value)])
    return rc == 0


# ------------------------------------------------------------------ graph

# The audio graph — which port is wired to which — is owned by the
# demiurge-graph CLI, which wraps pw-link. The UI never calls pw-link: same
# one-owner rule as demiurge-audio for live.conf and demiurge-wifi for nmcli.
#
# This exists because on 2026-08-09 the engine's inputs were connected to
# NOTHING and no UI could show it: instrument in, silence through. The two
# facts the UI must surface from here are (a) is anything unconnected, and
# (b) is each direction DIRECT or via LOOPBACK — the latter costs a quantum
# and was previously invisible.

GRAPH_CMD_DEFAULT = "/usr/local/bin/demiurge-graph"

# demiurge-graph exits 3 for "there is no graph right now" (bypass mode, or
# PipeWire not running). That is a STATE, not a failure — the mode line is
# still emitted and the UI shows an explanation rather than an empty panel.
GRAPH_NO_GRAPH_RC = 3


def _graph_cmd():
    return os.environ.get("NEPTR_GRAPH_CMD", GRAPH_CMD_DEFAULT)


def graph_state(runner=default_runner):
    """`demiurge-graph --list` -> a dict describing the live audio graph.

      mode      "sync" | "bypass" | "nopipewire" | "unavailable"
      nodes     [{name, kind, display}]           kind: engine|interface|
                                                        loopback|midi
      ports     [{name, node, dir, kind, display, links}]   dir: "in"|"out"
      links     [{src, dst, path, flags}]         path: direct|loopback|internal
      warnings  [{code, text}]                    code: no-input|no-output|
                                                        dangling
      in_path   "direct" | "loopback" | "none" | None
      out_path  same

    A missing CLI yields mode "unavailable" and empty lists, so the UI can say
    so instead of drawing an empty patchbay that looks like real silence."""
    rc, out = runner(shlex.split(_graph_cmd()) + ["--list"])
    if rc not in (0, GRAPH_NO_GRAPH_RC):
        return {"mode": "unavailable", "nodes": [], "ports": [], "links": [],
                "warnings": [], "in_path": None, "out_path": None}

    state = {"mode": "unavailable", "nodes": [], "ports": [], "links": [],
             "warnings": [], "in_path": None, "out_path": None}
    for line in out.splitlines():
        parts = line.rstrip("\n").split("|")
        kind = parts[0]
        if kind == "mode" and len(parts) >= 2:
            state["mode"] = parts[1]
        elif kind == "node" and len(parts) >= 4:
            state["nodes"].append({"name": parts[1], "kind": parts[2],
                                   "display": parts[3]})
        elif kind == "port" and len(parts) >= 7:
            try:
                nlinks = int(parts[6])
            except ValueError:
                continue
            state["ports"].append({"name": parts[1], "node": parts[2],
                                   "dir": parts[3], "kind": parts[4],
                                   "display": parts[5], "links": nlinks})
        elif kind == "link" and len(parts) >= 5:
            state["links"].append({
                "src": parts[1], "dst": parts[2], "path": parts[3],
                "flags": {f for f in parts[4].split(",") if f},
            })
        elif kind == "warn" and len(parts) >= 3:
            state["warnings"].append({"code": parts[1],
                                      "text": "|".join(parts[2:])})
        elif kind == "path" and len(parts) >= 3:
            state["in_path"], state["out_path"] = parts[1], parts[2]
    return state


def graph_connect(src, dst, runner=default_runner):
    """`demiurge-graph connect <src> <dst>` -> (ok, message).

    The CLI validates both names against the live port list, so a stale UI
    row can't silently do nothing — it comes back with a reason."""
    rc, out = runner(shlex.split(_graph_cmd()) + ["connect", str(src), str(dst)])
    return rc == 0, out.strip()


def graph_disconnect(src, dst, runner=default_runner):
    """`demiurge-graph disconnect <src> <dst>` -> (ok, message).

    Callers MUST arm-then-confirm first: pulling the last link off an engine
    port is instant silence."""
    rc, out = runner(shlex.split(_graph_cmd()) + ["disconnect", str(src), str(dst)])
    return rc == 0, out.strip()


def graph_reset(runner=default_runner):
    """`demiurge-graph reset` -> (ok, message). Drops user overrides and asks
    the launcher to re-assert the graph it means to have. This is the way back
    from patching yourself into silence, and it must never need ssh."""
    rc, out = runner(shlex.split(_graph_cmd()) + ["reset"])
    return rc == 0, out.strip()


# ------------------------------------------------------------------- wifi

# Network selection is owned by the demiurge-wifi CLI (it wraps nmcli and
# writes the NetworkManager profile). The UI never shells out to nmcli itself.

WIFI_CMD_DEFAULT = "/usr/local/bin/demiurge-wifi"


def _wifi_cmd():
    return shlex.split(os.environ.get("NEPTR_WIFI_CMD", WIFI_CMD_DEFAULT))


def wifi_status(runner=default_runner):
    """`demiurge-wifi --status-machine` -> dict, or None when unavailable.
    {"state","ssid","signal","security","ip","iface"}; signal may be None."""
    rc, out = runner(_wifi_cmd() + ["--status-machine"])
    if rc != 0:
        return None
    line = (out or "").strip().splitlines()
    if not line:
        return None
    parts = line[0].split("|")
    if len(parts) != 6:
        return None
    state, ssid, signal, security, ip, iface = (p.strip() for p in parts)
    try:
        sig = int(signal) if signal else None
    except ValueError:
        sig = None
    return {"state": state, "ssid": ssid, "signal": sig,
            "security": security, "ip": ip, "iface": iface}


def wifi_list(rescan=False, runner=default_runner):
    """Visible networks, strongest first (the CLI sorts and dedupes):
    [{"ssid","signal","security","active","saved"}]. `rescan=True` forces a
    fresh AP scan and is SLOW (seconds) — only ever call it off the UI thread.
    A missing or failing command yields []."""
    rc, out = runner(_wifi_cmd() + ["--scan" if rescan else "--list"])
    if rc != 0:
        return []
    nets = []
    for line in (out or "").splitlines():
        parts = line.split("|")
        if len(parts) != 4:
            continue
        ssid, signal, security, flags = (p.strip() for p in parts)
        if not ssid:
            continue
        try:
            sig = int(signal)
        except ValueError:
            continue
        fl = {f.strip() for f in flags.split(",")}
        nets.append({"ssid": ssid, "signal": sig, "security": security,
                     "active": "active" in fl, "saved": "saved" in fl})
    return nets


def wifi_join(ssid, password=None, runner=default_runner):
    """Join `ssid`. Returns (ok, message-for-the-user).

    The password — when there is one — is handed to the CLI on STDIN and is
    never placed in argv, so it cannot be read out of `ps` by anything else on
    the box, and it is never written to a file or log by this UI. Callers
    should drop their copy of the string as soon as this returns."""
    if not ssid:
        return False, "no network chosen"
    cmd = _wifi_cmd() + ["join", ssid]
    if password:
        if not (8 <= len(password) <= 63):
            return False, "password must be 8-63 characters"
        rc, out = runner(cmd + ["--password-stdin"], password + "\n")
    else:
        rc, out = runner(cmd)
    if rc == 0:
        return True, "joined %s" % ssid
    # The CLI's own last line is the most specific thing we can say.
    msg = [l for l in (out or "").splitlines() if l.strip()]
    return False, (msg[-1].strip() if msg else "could not join %s" % ssid)


def wifi_forget(ssid, runner=default_runner):
    rc, _ = runner(_wifi_cmd() + ["forget", ssid])
    return rc == 0


def wifi_disconnect(runner=default_runner):
    rc, _ = runner(_wifi_cmd() + ["disconnect"])
    return rc == 0


def wifi_radio(on, runner=default_runner):
    rc, _ = runner(_wifi_cmd() + ["radio", "on" if on else "off"])
    return rc == 0


# ------------------------------------------------------------------- config

class CompanionActions:
    def __init__(self, activator, power_set, restart, patches_dir):
        self.activator = activator
        self.power_set = power_set      # CommandAction, run(level=...)
        self.restart = restart          # CommandAction, run()
        self.patches_dir = patches_dir


def from_env():
    """Build the companion's actions from env, with defaults matching the
    demiurge layout. Override any command without touching code:

      NEPTR_PATCHES_DIR      default ~/demiurge/sets
      NEPTR_PATCH_CMD        default '/opt/demiurge/bin/demiurge-set {name}'
      NEPTR_POWER_SET_CMD    default 'demiurge-power {level}'  (level low|medium|high)
      NEPTR_RESTART_CMD      default 'sudo systemctl restart demiurge'  (redem)

    The audio-interface commands (NEPTR_IFACE_LIST_CMD / NEPTR_IFACE_SET_CMD)
    are read directly by list_interfaces() / set_interface() above.
    """
    patches_dir = os.environ.get(
        "NEPTR_PATCHES_DIR", os.path.expanduser("~/demiurge/sets"))
    # demiurge-set copies the patch config into live.conf and restarts demiurge.
    # Full path because /opt/demiurge/bin is not on the UI service's PATH.
    patch_cmd = os.environ.get(
        "NEPTR_PATCH_CMD", "/opt/demiurge/bin/demiurge-set {name}")
    power_cmd = os.environ.get("NEPTR_POWER_SET_CMD", "demiurge-power {level}")
    restart_cmd = os.environ.get(
        "NEPTR_RESTART_CMD", "sudo systemctl restart demiurge")
    return CompanionActions(
        Activator(patch_cmd), CommandAction(power_cmd),
        CommandAction(restart_cmd), patches_dir)
