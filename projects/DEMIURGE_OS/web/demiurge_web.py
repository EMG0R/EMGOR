#!/usr/bin/env python3
"""demiurge-web: status daemon + browser mirror of the demiurge companion TUI.
Stdlib only. Layers: parsers (pure) / collector (2s thread) / HTTP server."""
import sys, json, os, re, glob, time, queue, socket, struct, argparse, \
    threading, subprocess
from collections import deque
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import neptr_model as neptr          # noqa: E402  (generated; see gen script)

# ───────────────────────── parsers (pure, no I/O) ─────────────────────────

THROTTLE_BITS = [(0, "UNDERVOLT"), (1, "ARM-CAP"), (2, "THROTTLE"), (3, "TEMP-LIM")]


def parse_throttled(s):
    m = re.search(r"0x([0-9a-fA-F]+)", s or "")
    if not m:
        return None
    v = int(m.group(1), 16)
    return {"hex": "0x%x" % v,
            "now":  [n for b, n in THROTTLE_BITS if v >> b & 1],
            "past": [n for b, n in THROTTLE_BITS if v >> (b + 16) & 1]}


def derive_power(max_khz):
    mhz = max_khz / 1000.0
    return "LOW" if mhz <= 1600 else ("MED" if mhz <= 2400 else "HIGH")


def parse_temp(s):
    m = re.search(r"temp=([\d.]+)", s or "")
    return float(m.group(1)) if m else None


def parse_meminfo(text):
    d = dict(re.findall(r"^(\w+):\s+(\d+)", text, re.M))
    return {"total_kb": int(d["MemTotal"]), "avail_kb": int(d["MemAvailable"])}


def parse_loadavg(text, ncores):
    load1 = float(text.split()[0])
    return {"load1": load1, "pct": min(100.0, 100.0 * load1 / max(1, ncores))}


CHAIN_EXT = re.compile(r"\.(csd|ck|pd|scd|dsp|cpp|strudel|py|rnbo|nam)$")


def parse_liveconf(text):
    out = {"set": None, "power": None, "chains": [], "has_input": False}
    cur = None
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("# set:") or s.startswith("# patch:"):
            if out["set"] is None:
                out["set"] = s.split(":", 1)[1].strip()
        elif s.startswith("# power:"):
            if out["power"] is None:
                out["power"] = s.split(":", 1)[1].strip()
        elif re.match(r"^sync_layer\s*=", line):
            val = line.split("=", 1)[1].split("#", 1)[0].strip()
            if val == "off":
                out["has_input"] = True   # program grabs the interface directly
        elif re.match(r"^chain\s*=", line):
            cur = []
            out["chains"].append(cur)
            rest = line.split("=", 1)[1].strip()
            if rest:
                cur.append(rest)
        elif cur is not None and line[:1] in (" ", "\t") and s and not s.startswith("#"):
            if re.match(r"^in\d+\s*->", s):
                out["has_input"] = True
                s = s.split("->", 1)[1].strip()
            if s and s != "out":
                base = os.path.basename(s)
                if base.endswith(".rnbo"):
                    cur.append("rnbo:" + base[:-len(".rnbo")])
                else:
                    cur.append(CHAIN_EXT.sub("", base))
        elif cur is not None and line[:1] not in (" ", "\t"):
            cur = None
    out["chains"] = [c for c in out["chains"] if c]
    return out


def parse_sets(files):
    out = []
    for fname in sorted(files):
        stem = fname.rsplit(".", 1)[0]
        if stem == "nam":
            continue
        text = files[fname]
        mset = re.search(r"^#\s*set:\s*(.+)$", text, re.M)
        mpow = re.search(r"^#\s*power:\s*(.+)$", text, re.M)
        out.append({"file": fname,
                    "set": (mset.group(1).strip() if mset else stem),
                    "power": (mpow.group(1).strip() if mpow else None)})
    return out


def parse_interfaces(text):
    """`demiurge-interface --list` machine lines: name|card_id|flags[|display]
    (flags = comma-joined subset of active,chosen,inuse; display = optional
    prettified 4th field, falls back to name). Skip malformed."""
    out = []
    for line in (text or "").splitlines():
        parts = line.split("|")
        if len(parts) not in (3, 4):
            continue
        name, card_id, flags = (p.strip() for p in parts[:3])
        if not name:
            continue
        display = parts[3].strip() if len(parts) == 4 else ""
        fl = {f.strip() for f in flags.split(",")}
        out.append({"name": name, "id": card_id,
                    "display": display or name,
                    "active": "active" in fl,
                    "chosen": "chosen" in fl,
                    "inuse": "inuse" in fl})
    return out


SYNC_ROLES = ("opgorator", "demiurge")


def parse_sync(text):
    """`demiurge-sync --list` line: role|bpm|source_alive. Malformed -> None."""
    line = (text or "").strip().splitlines()
    if not line:
        return None
    parts = line[0].split("|")
    if len(parts) != 3:
        return None
    role, bpm_s, alive_s = (p.strip() for p in parts)
    if role not in SYNC_ROLES:
        return None
    try:
        bpm = float(bpm_s)
    except ValueError:
        return None
    return {"role": role, "bpm": bpm, "source_alive": alive_s == "1"}


def parse_touch(text):
    """`demiurge-touch --status-machine` line:
    touch=on|detected=yes|device=/dev/input/event4|name=ADS7846...|base=80|channel=16|slots=1
    Malformed -> None. UIs show the TOUCH tile only when detected."""
    line = (text or "").strip().splitlines()
    if not line:
        return None
    kv = {}
    for part in line[0].split("|"):
        if "=" not in part:
            return None
        k, v = part.split("=", 1)
        kv[k.strip()] = v.strip()
    if "touch" not in kv or "detected" not in kv:
        return None
    try:
        return {"on": kv.get("touch") == "on",
                "detected": kv.get("detected") == "yes",
                "device": kv.get("device", ""),
                "name": kv.get("name", ""),
                "base": int(kv.get("base", 80)),
                "channel": int(kv.get("channel", 16)),
                "slots": int(kv.get("slots", 1))}
    except ValueError:
        return None


AUDIO_KEYS = ("mode", "bits", "quantum", "rate")


def parse_audio(text):
    """`demiurge-audio --list` machine lines: key|value|default|options|effect.

    `effect` is the CLI's own mode-aware sentence — e.g. quantum reads as a
    real latency figure in sync mode and as "CPU headroom only" in bypass.
    UIs must render that string verbatim rather than compute a latency claim
    of their own, which is the whole reason the field exists."""
    out = {}
    for line in (text or "").splitlines():
        parts = line.split("|", 4)
        if len(parts) != 5:
            continue
        key, value, default, options, effect = (p.strip() for p in parts)
        if key not in AUDIO_KEYS or not value:
            continue
        out[key] = {"value": value, "default": default,
                    "options": [o for o in options.split(",") if o],
                    "effect": effect}
    return out or None


GRAPH_MODES = ("sync", "bypass", "nopipewire")
GRAPH_PATHS = ("direct", "loopback", "none")


def parse_graph(text):
    """`demiurge-graph --list` machine lines describing the live audio graph:

        mode|sync
        node|<name>|<kind>|<display>              kind: engine|interface|
                                                        loopback|midi
        port|<name>|<node>|<in|out>|<kind>|<display>|<link count>
        link|<src>|<dst>|<direct|loopback|internal>|<comma flags>
        warn|<code>|<text>                        code: no-input|no-output|
                                                        dangling
        path|<in_path>|<out_path>

    Two facts here exist nowhere else in the system. First, whether each
    direction is DIRECT or via LOOPBACK — the loopback costs a quantum each
    way, and until this landed no UI could tell you which one you had. Second,
    warn| lines: an engine port with zero links is silence, which is exactly
    the state the device sat in undetected on 2026-08-09.

    Malformed lines are skipped. Missing/failed CLI -> None (the section
    hides). `mode` other than "sync" means there is no graph to draw, which is
    a STATE, not an error — the CLI exits 3 for it and says why."""
    if text is None:
        return None
    g = {"mode": None, "nodes": [], "ports": [], "links": [], "warnings": [],
         "in_path": None, "out_path": None}
    for line in text.splitlines():
        parts = line.split("|")
        kind = parts[0].strip()
        if kind == "mode" and len(parts) >= 2:
            m = parts[1].strip()
            g["mode"] = m if m in GRAPH_MODES else None
        elif kind == "node" and len(parts) >= 4:
            g["nodes"].append({"name": parts[1], "kind": parts[2],
                               "display": parts[3]})
        elif kind == "port" and len(parts) >= 7:
            try:
                links = int(parts[6])
            except ValueError:
                continue
            if parts[3] not in ("in", "out"):
                continue
            g["ports"].append({"name": parts[1], "node": parts[2],
                               "dir": parts[3], "kind": parts[4],
                               "display": parts[5], "links": links})
        elif kind == "link" and len(parts) >= 5:
            g["links"].append({"src": parts[1], "dst": parts[2],
                               "path": parts[3],
                               "flags": [f for f in parts[4].split(",") if f]})
        elif kind == "warn" and len(parts) >= 3:
            g["warnings"].append({"code": parts[1],
                                  "text": "|".join(parts[2:])})
        elif kind == "path" and len(parts) >= 3:
            g["in_path"] = parts[1] if parts[1] in GRAPH_PATHS else None
            g["out_path"] = parts[2] if parts[2] in GRAPH_PATHS else None
    if g["mode"] is None:
        return None
    return g


WIFI_STATES = ("connected", "connecting", "disconnected", "nowifi")


def parse_wifi_status(text):
    """`demiurge-wifi --status-machine` line:
    state|ssid|signal|security|ip|iface. Malformed -> None."""
    lines = (text or "").strip().splitlines()
    if not lines:
        return None
    parts = lines[0].split("|")
    if len(parts) != 6:
        return None
    state, ssid, signal, security, ip, iface = (p.strip() for p in parts)
    if state not in WIFI_STATES:
        return None
    try:
        sig = int(signal) if signal else None
    except ValueError:
        sig = None
    return {"state": state, "ssid": ssid, "signal": sig,
            "security": security, "ip": ip, "iface": iface}


def parse_wifi_list(text):
    """`demiurge-wifi --list` lines: ssid|signal|security|flags, already
    deduplicated and sorted strongest-first by the CLI. Skip malformed."""
    out = []
    for line in (text or "").splitlines():
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
        out.append({"ssid": ssid, "signal": sig, "security": security,
                    "active": "active" in fl, "saved": "saved" in fl})
    return out


HOME = Path(os.environ.get("DEMIURGE_HOME", str(Path.home())))

# ───────────────────────── sensor mapping (NEPTR sensors) ─────────────────
#
# demiurge-io (a separate process, built in parallel — this file never
# imports or depends on its internals) owns the physical sensors: it reads
# ~/demiurge/sensors.json, hot-reloads on mtime change, and broadcasts every
# conditioned reading as OSC `/sensor <id> <norm>` to 127.0.0.1:8002 at
# ~20 Hz. This section is the whole contract on our side: read/validate/write
# the config file it consumes, and listen for the values it produces.

SENSOR_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
DROPOUT_MODES = ("hold", "zero")
OUTSIDE_MODES = ("hold", "clamp")
CURVE_SHAPES = ("lin", "exp", "log", "scurve")
MAP_MODES = ("continuous", "gate")
COMBINE_MODES = ("replace", "add", "max", "min")
TARGET_KINDS = ("expr", "cc")


def _is_num(v, lo=None, hi=None):
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        return False
    if lo is not None and v < lo:
        return False
    if hi is not None and v > hi:
        return False
    return True


def validate_sensor_source(s):
    """One entry of `sources`. Returns an error string, or None if valid."""
    if not isinstance(s, dict):
        return "source must be an object"
    sid = s.get("id")
    if not isinstance(sid, str) or not SENSOR_ID_RE.match(sid):
        return "bad source id: %r" % (sid,)
    if not isinstance(s.get("label"), str) or not s.get("label"):
        return "source %s: bad label" % sid
    if not isinstance(s.get("units"), str):
        return "source %s: bad units" % sid
    if not _is_num(s.get("in_lo")) or not _is_num(s.get("in_hi")):
        return "source %s: bad in_lo/in_hi" % sid
    if s.get("dropout") not in DROPOUT_MODES:
        return "source %s: bad dropout %r" % (sid, s.get("dropout"))
    if not _is_num(s.get("deadzone"), 0, 1):
        return "source %s: bad deadzone" % sid
    if not _is_num(s.get("smooth_ms"), 0, 60000):
        return "source %s: bad smooth_ms" % sid
    if not isinstance(s.get("enabled"), bool):
        return "source %s: bad enabled" % sid
    return None


def validate_target(t, valid_expr_ids):
    """A map's `target`. `valid_expr_ids` is the set of the 35 expression
    destination ids from expr_targets.json, or None to skip that check (the
    file is best-effort — its absence must not brick config saving)."""
    if not isinstance(t, dict):
        return "target must be an object"
    if t.get("sink") != "csound":
        return "target: bad sink %r" % (t.get("sink"),)
    kind = t.get("kind")
    if kind == "expr":
        tid = t.get("id")
        if not isinstance(tid, int) or isinstance(tid, bool):
            return "target: bad expr id %r" % (tid,)
        if valid_expr_ids is not None and tid not in valid_expr_ids:
            return "target: unknown expr id %r" % (tid,)
    elif kind == "cc":
        ch, cc = t.get("ch"), t.get("cc")
        if not isinstance(ch, int) or isinstance(ch, bool) or not (0 <= ch <= 15):
            return "target: bad cc channel %r" % (ch,)
        if not isinstance(cc, int) or isinstance(cc, bool) or not (0 <= cc <= 127):
            return "target: bad cc number %r" % (cc,)
    else:
        return "target: bad kind %r" % (kind,)
    return None


def validate_sensor_map(m, source_ids, valid_expr_ids):
    """One entry of `maps`. `source_ids` is the set of ids already declared
    in `sources` — a map may not reference a source that does not exist."""
    if not isinstance(m, dict):
        return "map must be an object"
    mid = m.get("id")
    if not isinstance(mid, str) or not SENSOR_ID_RE.match(mid):
        return "bad map id: %r" % (mid,)
    if m.get("source") not in source_ids:
        return "map %s: unknown source %r" % (mid, m.get("source"))
    if not isinstance(m.get("enabled"), bool):
        return "map %s: bad enabled" % mid
    zl, zh = m.get("zone_lo"), m.get("zone_hi")
    if not _is_num(zl, 0, 1) or not _is_num(zh, 0, 1) or zl > zh:
        return "map %s: bad zone_lo/zone_hi" % mid
    if m.get("outside") not in OUTSIDE_MODES:
        return "map %s: bad outside %r" % (mid, m.get("outside"))
    if not isinstance(m.get("invert"), bool):
        return "map %s: bad invert" % mid
    if m.get("curve") not in CURVE_SHAPES:
        return "map %s: bad curve %r" % (mid, m.get("curve"))
    if not _is_num(m.get("out_lo")) or not _is_num(m.get("out_hi")):
        return "map %s: bad out_lo/out_hi" % mid
    if m.get("mode") not in MAP_MODES:
        return "map %s: bad mode %r" % (mid, m.get("mode"))
    if m.get("combine") not in COMBINE_MODES:
        return "map %s: bad combine %r" % (mid, m.get("combine"))
    if not _is_num(m.get("slew_ms"), 0, 60000):
        return "map %s: bad slew_ms" % mid
    err = validate_target(m.get("target"), valid_expr_ids)
    if err:
        return "map %s: %s" % (mid, err)
    return None


def validate_sensors_config(cfg, valid_expr_ids=None):
    """Full ~/demiurge/sensors.json body. Returns an error string, or None
    when the config is safe to write. Never raises — a malformed body is
    exactly the case this exists to catch, not a server crash."""
    if not isinstance(cfg, dict):
        return "config must be an object"
    sources = cfg.get("sources")
    maps = cfg.get("maps")
    if not isinstance(sources, list):
        return "sources must be a list"
    if not isinstance(maps, list):
        return "maps must be a list"
    seen = set()
    for s in sources:
        err = validate_sensor_source(s)
        if err:
            return err
        if s["id"] in seen:
            return "duplicate source id: %s" % s["id"]
        seen.add(s["id"])
    mseen = set()
    for m in maps:
        err = validate_sensor_map(m, seen, valid_expr_ids)
        if err:
            return err
        if m["id"] in mseen:
            return "duplicate map id: %s" % m["id"]
        mseen.add(m["id"])
    return None


def read_sensors_config(path):
    """Missing file / unreadable / malformed JSON -> the empty config, never
    an exception — this is read on every page load and a torn or absent file
    must render as "no sensors configured yet", not a broken page."""
    try:
        text = Path(path).read_text()
    except Exception:
        return {"sources": [], "maps": []}
    try:
        cfg = json.loads(text)
    except Exception:
        return {"sources": [], "maps": []}
    if not isinstance(cfg, dict):
        return {"sources": [], "maps": []}
    sources = cfg.get("sources")
    maps = cfg.get("maps")
    return {"sources": sources if isinstance(sources, list) else [],
            "maps": maps if isinstance(maps, list) else []}


def write_sensors_config(cfg, path):
    """Atomic write: temp file in the same directory, then os.replace(). A
    torn config file — demiurge-io hot-reloads on mtime change and would read
    it mid-write otherwise — must never be observable."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + "." + str(os.getpid()) + ".tmp")
    tmp.write_text(json.dumps(cfg, indent=2))
    os.replace(tmp, path)


def _friendly_label(csound_name):
    """A csound global like `gkMasterVol` -> "Master Vol". Roughly a third of
    expr_targets.json's `label` fields are empty (the ones whose meaning was
    obvious enough in context not to need a comment there) — this is the
    fallback so the mapping editor never shows a blank destination name."""
    name = re.sub(r"^gk", "", csound_name)
    name = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name)  # MasterVol -> Master Vol
    name = name.replace("_", " ").strip()
    return name if name else csound_name


def read_expr_targets(path):
    """The 35 expression destinations from NEPTR's expr_targets.json (a file
    in the separate csound repo, read-only from here). Missing/malformed ->
    an empty list, never an exception."""
    try:
        data = json.loads(Path(path).read_text())
    except Exception:
        return []
    targets = data.get("targets") if isinstance(data, dict) else None
    if not isinstance(targets, list):
        return []
    out = []
    for t in targets:
        if not isinstance(t, dict):
            continue
        tid = t.get("id")
        writes = t.get("writes")
        if not isinstance(tid, int) or isinstance(tid, bool):
            continue
        writes = writes if isinstance(writes, list) else []
        label = t.get("label") or ""
        if not isinstance(label, str) or not label.strip():
            label = _friendly_label(writes[0]) if writes else "target %d" % tid
        out.append({"id": tid, "label": label, "writes": writes})
    out.sort(key=lambda t: t["label"].lower())
    return out


SENSOR_OSC_PORT = int(os.environ.get("DEMIURGE_SENSOR_OSC_PORT", "8002"))


def osc_decode_multi(data):
    """General OSC decode for the tag combinations this app needs (`i`, `f`,
    `s`). The engine's own broadcast is single-argument (see osc_decode
    above, kept as-is since it is proven); demiurge-io's `/sensor <id> <norm>`
    is two arguments (string, float), which is the reason this exists."""
    try:
        end = data.index(b"\0")
    except ValueError:
        return None
    addr = data[:end].decode("ascii", "replace")
    if not addr.startswith("/"):
        return None
    pos = (end + 4) & ~3
    rest = data[pos:]
    if len(rest) < 4 or rest[:1] != b",":
        return None
    try:
        tend = rest.index(b"\0")
    except ValueError:
        return None
    tags = rest[1:tend].decode("ascii", "replace")
    apos = (tend + 4) & ~3
    args = []
    for tag in tags:
        if tag == "f":
            if len(rest) < apos + 4:
                return None
            args.append(struct.unpack(">f", rest[apos:apos + 4])[0])
            apos += 4
        elif tag == "i":
            if len(rest) < apos + 4:
                return None
            args.append(struct.unpack(">i", rest[apos:apos + 4])[0])
            apos += 4
        elif tag == "s":
            try:
                send = rest.index(b"\0", apos)
            except ValueError:
                return None
            args.append(rest[apos:send].decode("ascii", "replace"))
            apos = (send + 4) & ~3
        else:
            return None
    return addr, args


class SensorState:
    """Latest normalized value per sensor id, nothing more — this is a live
    meter feed, not an event log. on_change (optional, same contract as
    NeptrState's) fires after every update so the SSE push channel in
    main() can coalesce raw ~20 Hz OSC arrivals down to SENSOR_PUSH_HZ
    without SensorIn or SensorState knowing anything about HTTP."""

    def __init__(self, on_change=None):
        self.lock = threading.Lock()
        self.values = {}
        self.last_msg = {}
        self.on_change = on_change

    def set(self, sid, val):
        with self.lock:
            self.values[sid] = val
            self.last_msg[sid] = time.time()
        if self.on_change is not None:
            self.on_change()

    def snapshot(self):
        with self.lock:
            return dict(self.values)


class SensorIn:
    """UDP listener for demiurge-io's live sensor broadcast, modelled on
    EngineIn: same bind/retry loop, same "a probe failure is a status field,
    never a crash" story. Unlike EngineIn's OSC port there is no contest here
    — demiurge-io is this stream's only writer — so there is no yield logic,
    only retry against a port that may not have a listener bound to it yet."""

    def __init__(self, state, port=SENSOR_OSC_PORT, host="127.0.0.1",
                 on_param=None):
        self.state = state
        self.on_param = on_param     # fn(path, value) for pool `/param` echoes
        self.port = port
        self.host = host
        self.status = "off"
        self.detail = "not started"
        self.sock = None
        self._stop = threading.Event()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()
        return self

    def _bind(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.5)
        s.bind((self.host, self.port))
        return s

    def _loop(self):
        backoff = 2.0
        while not self._stop.is_set():
            if self.sock is None:
                try:
                    self.sock = self._bind()
                    self.status, self.detail = "live", (
                        "listening on %s:%d" % (self.host, self.port))
                    backoff = 2.0
                except OSError as exc:
                    self.status = "port-busy"
                    self.detail = ("%s:%d is held by another process (%s)"
                                   % (self.host, self.port, exc.strerror
                                      or exc))
                    self._stop.wait(backoff)
                    backoff = min(backoff * 1.5, 15.0)
                    continue
            try:
                data, _ = self.sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                self.sock = None
                continue
            msg = osc_decode_multi(data)
            if msg is None:
                continue
            addr, args = msg
            if addr == "/sensor" and len(args) == 2 \
                    and isinstance(args[0], str) \
                    and isinstance(args[1], (int, float)):
                self.state.set(args[0], float(args[1]))
            elif addr == "/param" and len(args) == 2 \
                    and isinstance(args[0], str) \
                    and isinstance(args[1], (int, float)) \
                    and self.on_param is not None:
                self.on_param(args[0], float(args[1]))

    def stop(self):
        self._stop.set()

    def info(self):
        return {"status": self.status, "detail": self.detail,
                "port": self.port}


SENSORS_CONFIG = Path(os.environ.get("DEMIURGE_SENSORS_CONFIG",
                                     str(HOME / "demiurge" / "sensors.json")))
# The NEPTR engine tree is ~/NEPTR_phase4 on the device — that is the path
# neptr-bridge.service runs serial_bridge.py from. Getting this wrong is silent:
# read_expr_targets() returns [] for a missing file, so the mapping editor's
# destination picker comes up EMPTY rather than erroring. First candidate that
# exists wins, so a dev checkout under a different name still resolves.
EXPR_TARGETS_CANDIDATES = [
    HOME / "NEPTR_phase4" / "csound" / "handler" / "expr_targets.json",
    HOME / "NEPTR" / "csound" / "handler" / "expr_targets.json",
]


def _default_expr_targets_path():
    for p in EXPR_TARGETS_CANDIDATES:
        if p.exists():
            return p
    return EXPR_TARGETS_CANDIDATES[0]


EXPR_TARGETS_PATH = Path(os.environ["DEMIURGE_EXPR_TARGETS"]) \
    if os.environ.get("DEMIURGE_EXPR_TARGETS") else _default_expr_targets_path()


class SensorLink:
    """The sensor side as one object, the same shape as Engine: a config
    path this process reads/writes and an OSC listener for the live feed.
    Paths are constructor args (falling back to the module constants above)
    so tests never touch a real home directory."""

    def __init__(self, config_path=None, expr_targets_path=None,
                port=SENSOR_OSC_PORT, on_change=None, on_param=None):
        self.config_path = Path(config_path) if config_path else SENSORS_CONFIG
        self.expr_targets_path = (Path(expr_targets_path) if expr_targets_path
                                  else EXPR_TARGETS_PATH)
        self.state = SensorState(on_change=on_change)
        self.inp = SensorIn(self.state, port=port, on_param=on_param)

    def start(self):
        self.inp.start()
        return self

    def read_config(self):
        return read_sensors_config(self.config_path)

    def write_config(self, cfg):
        write_sensors_config(cfg, self.config_path)

    def expr_targets(self):
        return read_expr_targets(self.expr_targets_path)

    def snapshot(self):
        return {"live": self.state.snapshot(), "link": self.inp.info()}


# ───────────────────────── parameter pool (docs/parameters.md §7, §11) ────
#
# The pool IS demiurge-io. This side is a thin client: list the registry,
# publish `/p <path> <val>` to the pool's ingress, relay the pool's `/param`
# telemetry echoes as SSE, and write the ACTIVE PATCH's <name>.map.json.
# Registry + hard/soft rules are demiurge_io.pool's (ParamRegistry/Pool.
# can_remap), imported, never re-implemented: if the package cannot be
# imported the registry is empty and map writes fail closed (503).

POOL_HOST = os.environ.get("DEMIURGE_POOL_HOST", "127.0.0.1")
POOL_PORT = int(os.environ.get("DEMIURGE_POOL_PORT", "9102"))

try:
    import demiurge_io.pool as _dio_pool            # noqa: E402
    import demiurge_io.config as _dio_config        # noqa: E402
except ImportError:
    for _cand in (Path(__file__).resolve().parent.parent / "src" / "demiurge-io",
                  Path("/opt/demiurge/src/demiurge-io")):
        if (_cand / "demiurge_io").is_dir():
            sys.path.insert(0, str(_cand))
            break
    try:
        import demiurge_io.pool as _dio_pool        # noqa: E402
        import demiurge_io.config as _dio_config    # noqa: E402
    except ImportError:
        _dio_pool = _dio_config = None


class PoolLink:
    """Everything /api/pool/* needs. All paths are constructor args (falling
    back to $DEMIURGE_HOME/demiurge/...) so tests point it at a temp dir and
    a fake pool socket."""

    def __init__(self, root=None, expr_targets_path=None, host=POOL_HOST,
                 port=POOL_PORT, sensors=None):
        self.root = Path(root) if root else HOME / "demiurge"
        self.host, self.port = host, int(port)
        self.sensors = sensors
        self.values = {}
        self.lock = threading.Lock()
        self.subs = []
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.registry = None
        if _dio_pool is not None:
            et = Path(expr_targets_path) if expr_targets_path else EXPR_TARGETS_PATH
            self.registry = _dio_pool.ParamRegistry(self.root / "params", et, builtins=True)
            self.pool = _dio_pool.Pool(self.registry)

    def _reg(self):
        if self.registry is not None:
            self.registry.refresh()
        return self.registry

    # -- values / events --
    def note(self, path, value):
        """A value changed (UI write or pool `/param` echo): remember + fan out."""
        value = max(0.0, min(1.0, float(value)))
        with self.lock:
            self.values[path] = value
            subs = list(self.subs)
        for q in subs:
            q.put({"path": path, "value": value})

    def subscribe(self):
        q = queue.Queue()
        with self.lock:
            self.subs.append(q)
        return q

    def unsubscribe(self, q):
        with self.lock:
            if q in self.subs:
                self.subs.remove(q)

    # -- reads --
    def params(self):
        reg = self._reg()
        out = []
        if reg is None:
            return out
        with self.lock:
            cur = dict(self.values)
        for path in reg.paths():
            m = reg.meta(path)
            d = m.get("default")
            out.append({"path": path, "label": m.get("label") or path,
                        "stage": m.get("stage"), "default": d,
                        "map": "hard" if reg.is_hard(path) else "soft",
                        "value": cur.get(path, d)})
        return out

    def hard_maps(self):
        reg = self._reg()
        return reg.hard_maps() if reg is not None else []

    def sources(self):
        live = self.sensors.state.snapshot() if self.sensors else {}
        out, seen = [], set()
        try:
            cfg = self.sensors.read_config() if self.sensors else {"sources": []}
        except Exception:
            cfg = {"sources": []}
        for s in cfg.get("sources", []):
            sid = s.get("id")
            if sid is None:
                continue
            seen.add(sid)
            out.append({"id": sid, "label": s.get("label") or sid,
                        "configured": True, "live": live.get(sid)})
        for sid in sorted(set(live) - seen):      # telemetry seen, not configured
            out.append({"id": sid, "label": sid, "configured": False,
                        "live": live[sid]})
        return out

    # -- drive --
    def set(self, path, value):
        if not isinstance(path, str) or not path.startswith("/") \
                or isinstance(value, bool) or not isinstance(value, (int, float)) \
                or value != value:
            return 400, {"ok": False, "error": "need path (string) and value (number)"}
        v = max(0.0, min(1.0, float(value)))
        try:
            pkt = osc_encode("/p", path, v)
        except UnicodeEncodeError:
            return 400, {"ok": False, "error": "path must be ascii"}
        self.sock.sendto(pkt, (self.host, self.port))
        self.note(path, v)
        return 200, {"ok": True, "path": path, "value": v}

    # -- per-patch map file --
    def _patch(self):
        if _dio_config is None:
            return None
        return _dio_config.active_patch(self.root / "live.conf")

    def read_map(self):
        patch = self._patch()
        if patch is None:
            return 200, {"version": 1, "patch": None, "maps": [], "values": {}}
        try:
            p = _dio_config.patch_map_path(patch, self.root)
            doc = json.loads(p.read_text()) if p.is_file() else {}
        except (ValueError, OSError) as e:
            return 500, {"error": "map file unreadable: %s" % e}
        if not isinstance(doc, dict):
            doc = {}
        return 200, {"version": 1, "patch": patch, "maps": doc.get("maps", []),
                     "values": doc.get("values", {})}

    def write_map(self, body):
        """Upsert one map ({source, param, zone_lo, zone_hi, invert, curve,
        mode}) or replace the whole list ({maps:[...]}, what the softmap page
        posts). Refuses (409) any map whose param is hard."""
        reg = self._reg()
        if reg is None:
            return 503, {"ok": False, "error": "demiurge_io not importable; map writes disabled"}
        patch = self._patch()
        if patch is None:
            return 400, {"ok": False, "error": "no active patch (live.conf has no '# set:' header)"}
        whole = isinstance(body.get("maps"), list)
        entries = body["maps"] if whole else [body]
        built = []
        for e in entries:
            if not isinstance(e, dict):
                return 400, {"ok": False, "error": "bad map entry"}
            param = e.get("param") or (e.get("target") or {}).get("param")
            src = e.get("source")
            if not isinstance(param, str) or not isinstance(src, str) or not src:
                return 400, {"ok": False, "error": "map needs source and param"}
            if not self.pool.can_remap(param):
                print("[demiurge-web] map rejected: %s is hard-mapped" % param,
                      file=sys.stderr, flush=True)
                return 409, {"ok": False, "error": "%s is hard-mapped" % param,
                             "param": param}
            if reg.target(param) is None:
                return 400, {"ok": False, "error": "unknown param %s" % param}
            lo, hi = e.get("zone_lo", 0.0), e.get("zone_hi", 1.0)
            if not (_is_num(lo, 0, 1) and _is_num(hi, 0, 1) and lo < hi):
                return 400, {"ok": False, "error": "zone_lo/zone_hi must be 0..1 with lo < hi"}
            curve, mode = e.get("curve", "lin"), e.get("mode", "continuous")
            if curve not in CURVE_SHAPES or mode not in MAP_MODES:
                return 400, {"ok": False, "error": "bad curve or mode"}
            if e.get("invert") not in (None, True, False):
                return 400, {"ok": False, "error": "invert must be boolean"}
            m = {k: v for k, v in e.items() if k not in ("param",)}
            m.update({"id": e.get("id") or "%s-%s" % (src.rsplit("/", 1)[-1],
                                                     param.rsplit("/", 1)[-1]),
                      "source": src, "target": {"param": param},
                      "zone_lo": float(lo), "zone_hi": float(hi),
                      "invert": bool(e.get("invert", False)),
                      "curve": curve, "mode": mode,
                      "combine": e.get("combine", "replace")})
            built.append(m)
        code, cur = self.read_map()
        if code != 200:
            return code, {"ok": False, "error": cur.get("error")}
        if whole:
            maps = built
        else:
            maps = [m for m in cur["maps"]
                    if not (isinstance(m.get("target"), dict)
                            and m["target"].get("param") == built[0]["target"]["param"])]
            maps.append(built[0])
        try:
            p = _dio_config.patch_map_path(patch, self.root)
            _dio_config.atomic_write_json(p, {"version": 1, "patch": patch,
                                              "maps": maps,
                                              "values": cur.get("values", {})})
        except (ValueError, OSError) as e:
            return 500, {"ok": False, "error": str(e)}
        return 200, {"ok": True, "patch": patch, "maps": len(maps)}


def percore_cpu(prev_stat, cur_stat):
    def rows(t):
        return {l.split()[0]: [int(x) for x in l.split()[1:]]
                for l in t.splitlines() if re.match(r"^cpu\d+ ", l)}
    p, c = rows(prev_stat), rows(cur_stat)
    pcts = []
    for k in sorted(c, key=lambda s: int(s[3:])):
        if k not in p:
            continue
        dt = sum(c[k]) - sum(p[k])
        didle = (c[k][3] + c[k][4]) - (p[k][3] + p[k][4])
        pcts.append(0.0 if dt <= 0 else round(100.0 * (dt - didle) / dt, 1))
    return pcts


# ───────────────────────── probes (all I/O lives here) ─────────────────────────

LIVE_CONF = HOME / "demiurge" / "live.conf"
SETS_DIR = HOME / "demiurge" / "sets"          # legacy alias of PATCHES_DIR
PATCHES_DIR = HOME / "demiurge" / "patches"    # canonical (folder or flat)
CPUFREQ = "/sys/devices/system/cpu/cpu0/cpufreq"
XRUN_RE = re.compile("xrun", re.I)


class Probes:
    """Real local probes. run() shells out; read() reads a file.
    Any failure returns None so the frame field goes null."""

    def run(self, cmd, check=True):
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=3)
            if check and r.returncode != 0:
                return None
            return r.stdout
        except Exception:
            return None

    def read(self, path):
        try:
            return Path(path).read_text()
        except Exception:
            return None

    def list_sets(self):
        try:
            # Canonical patches/ first (folder, then flat), sets/ as alias.
            # Same order demiurge-patch resolves in; first name wins.
            out = {}
            cands = (sorted(glob.glob(str(PATCHES_DIR / "*" / "*.conf")))
                     + sorted(glob.glob(str(PATCHES_DIR / "*.conf")))
                     + sorted(glob.glob(str(SETS_DIR / "*.conf"))))
            for f in cands:
                fp = Path(f)
                if fp.parent.parent == PATCHES_DIR and fp.parent.name != fp.stem:
                    continue    # folder patch = patches/<n>/<n>.conf only
                key = os.path.basename(f)
                if key not in out:
                    out[key] = fp.read_text()
            return out
        except Exception:
            return None

    def live_conf(self):
        return self.read(LIVE_CONF)

    def interfaces(self):
        # missing binary / nonzero exit / timeout -> None -> empty list
        return self.run(["demiurge-interface", "--list"])

    def sync(self):
        # missing binary / nonzero exit / timeout -> None -> null sync field
        return self.run(["demiurge-sync", "--list"])

    def touch(self):
        # missing binary / no touchscreen -> None -> null touch field
        return self.run(["demiurge-touch", "--status-machine"])

    def audio_settings(self):
        # missing binary / no live.conf -> None -> null audio_settings field
        return self.run(["demiurge-audio", "--list"])

    def graph(self):
        # demiurge-graph exits 3 for "no graph right now" (bypass mode, or
        # PipeWire not running) and still prints the mode line, so this must
        # NOT check the exit code — the honest answer is "the sync layer is
        # off", not a null field the UI would render as "unavailable".
        return self.run(["demiurge-graph", "--list"], check=False)

    def wifi_status(self):
        return self.run(["demiurge-wifi", "--status-machine"])

    def wifi_list(self):
        # Cached AP list (the CLI passes --rescan no), so this stays fast
        # enough to sit in the 2 s poll loop. An explicit refresh is the
        # /api/wifi/scan action.
        return self.run(["demiurge-wifi", "--list"])


class MockProbes(Probes):
    """Maps known commands/paths to fixture files; unknown or broken -> None."""

    CMD_MAP = {
        "hostname": "hostname.txt",
        "vcgencmd measure_temp": "temp.txt",
        "vcgencmd measure_temp pmic": "temp_pmic.txt",
        "vcgencmd get_throttled": "throttled_0x50005.txt",
        "vcgencmd measure_volts core": "volts_core.txt",
        "vcgencmd pmic_read_adc": "pmic_read_adc.txt",
        "systemctl is-active demiurge": "systemctl_demiurge.txt",
        "systemctl is-active neptr-ui": "systemctl_neptr_ui.txt",
        "demiurge-interface --list": "interfaces.txt",
        "demiurge-sync --list": "sync.txt",
        "demiurge-touch --status-machine": "touch.txt",
        "demiurge-audio --list": "audio.txt",
        "demiurge-graph --list": "graph.txt",
        "demiurge-wifi --status-machine": "wifi_status.txt",
        "demiurge-wifi --list": "wifi_list.txt",
    }
    PATH_MAP = {
        "/proc/loadavg": "loadavg.txt",
        "/proc/meminfo": "meminfo.txt",
        "/proc/uptime": "uptime.txt",
        "/proc/stat": "stat.txt",
        CPUFREQ + "/scaling_cur_freq": "scaling_cur_freq.txt",
        CPUFREQ + "/cpuinfo_min_freq": "cpuinfo_min_freq.txt",
        CPUFREQ + "/scaling_max_freq": "scaling_max_freq.txt",
        CPUFREQ + "/scaling_governor": "scaling_governor.txt",
        "/sys/devices/system/cpu/online": "online.txt",
    }

    def __init__(self, fixture_dir):
        self.fx = Path(fixture_dir)
        self.broken = set()

    def _fx(self, name):
        try:
            return (self.fx / name).read_text()
        except Exception:
            return None

    def run(self, cmd, check=True):
        key = " ".join(cmd)
        if key in self.broken:
            return None
        if key.startswith("journalctl"):
            return self._fx("journalctl_xrun.txt")
        name = self.CMD_MAP.get(key)
        return self._fx(name) if name else None

    def read(self, path):
        s = str(path)
        if s in self.broken:
            return None
        name = self.PATH_MAP.get(s)
        return self._fx(name) if name else None

    def list_sets(self):
        if "sets" in self.broken:
            return None
        return {os.path.basename(f): Path(f).read_text()
                for f in sorted(glob.glob(str(self.fx / "sets" / "*.conf")))}

    def live_conf(self):
        if "live.conf" in self.broken:
            return None
        return self._fx("live.conf")


# ───────────────────────── engine link (NEPTR) ─────────────────────────
#
# Two transports, both pre-existing — see demiurge/docs/engine-ui.md. Nothing
# new is invented here; this is the third client of a protocol the Teensy and
# the on-device touch UI already speak.
#
#   web -> engine   MIDI CCs on the shared `Midi Through` ALSA-seq pool, the
#                   EXACT physical ch1 numbers the Teensy encoders emit. The
#                   pool is many-to-many by design (INSTRUCTIONS.md §4), so a
#                   third writer costs nothing and collides with nothing: the
#                   engine cannot tell a phone from a knob.
#
#   engine -> web   OSC on 127.0.0.1:8000. This one is CONTESTED. `neptr-ui`
#                   binds that port for the whole time it runs, csound's
#                   OSC3x_* UDOs send to exactly one hardcoded destination,
#                   and a UDP unicast port has exactly one owner. So this
#                   listener BINDS ONLY IF THE PORT IS FREE and reports which
#                   of the two it got. It never uses SO_REUSEPORT: two
#                   listeners would make the kernel hand each datagram to one
#                   of them, which would silently half-blind the on-device UI
#                   — the one surface the user plays with their hands.
#
# What that means in the browser is stated in the UI, not hidden: with the
# port free (screen off / `ui = off`, the phone-as-only-surface case) every
# slot lights from the engine's own broadcast. With `neptr-ui` holding it,
# CONTROL still works fully and lit state is reported as unknown rather than
# guessed. Nothing here ever infers a slot state from a message it sent.

ENGINE_OSC_PORT = int(os.environ.get("DEMIURGE_ENGINE_OSC_PORT", "8000"))
OSC_YIELD_RE = re.compile(r"^\s*ui\s*=\s*(\S+)", re.M)


def on_device_ui_claims_osc():
    """True when live.conf says the on-device UI is on, i.e. neptr-ui owns
    the OSC port. Binding is decided by this, NOT by who wins the boot race:
    the pygame UI exits on a busy port, so a race we happen to win puts it in
    a restart loop and leaves the user with a black screen. Unreadable or
    absent live.conf -> assume the screen is on and yield; the surface with
    hands on it is the one that must never lose."""
    try:
        m = OSC_YIELD_RE.search(Path(LIVE_CONF).read_text())
    except Exception:
        return True
    return True if m is None else m.group(1).strip().lower() not in (
        "off", "0", "no", "false")
NEPTR_PUSH_HZ = 12.0            # coalesced browser pushes while state churns
SENSOR_PUSH_HZ = 20.0            # matches demiurge-io's raw OSC broadcast rate;
                                 # a LiDAR calibration by hand-wave reads as
                                 # laggy below this, unlike param sweeps


def osc_decode(data):
    """One OSC message -> (address, value), or None if it is not a shape the
    engine sends. The engine emits only `,i` and `,f` single-argument
    messages (globals.orc OSC3x_i / OSC3x_f), so this stays a dozen lines
    instead of a dependency."""
    try:
        end = data.index(b"\0")
    except ValueError:
        return None
    addr = data[:end].decode("ascii", "replace")
    if not addr.startswith("/"):
        return None
    pos = (end + 4) & ~3
    rest = data[pos:]
    if len(rest) < 8 or rest[:1] != b",":
        return None
    tag = rest[1:2]
    payload = rest[4:8]
    try:
        if tag == b"i":
            return addr, struct.unpack(">i", payload)[0]
        if tag == b"f":
            return addr, struct.unpack(">f", payload)[0]
    except struct.error:
        return None
    return None


class NeptrState:
    """Engine-authoritative mirror of the instrument, fed only by OSC.

    Every field here was broadcast by the engine. Nothing is written on the
    strength of a CC this server sent — the pygame UI has the same rule ("a
    touch toggle does not flip the slot locally; the engine's /xxx_state echo
    does") and it is the reason the two surfaces can never disagree."""

    def __init__(self, on_change=None):
        self.lock = threading.Lock()
        self.on_change = on_change
        self.states = {}            # effect code -> 0 ON / 1 BYPASSED
        self.params = {}            # OSC address -> value
        self.menu = None
        self.last_toggle = None     # which effect the encoders are armed to
        self.preset = None
        self.preset_bank = None
        self.nam_sel = [0] * 5
        self.ir_sel = [0] * 5
        self.lp_states = [0] * 5
        self.lp_pos = [0.0] * 5
        self.lp_speed = [0] * 5
        self.lp_drum_ch = -1
        self.addrs = set()          # distinct addresses = snapshot detector
        self.last_msg = None

    def apply(self, addr, val):
        """Route one message. Mirrors route_osc() in the pygame UI; anything
        it does not recognise is kept as a raw param if it names a known
        (code, param) pair, and dropped otherwise."""
        with self.lock:
            self.addrs.add(addr)
            self.last_msg = time.time()
            if addr == "/menu":
                self.menu = int(val)
            elif addr == "/last_toggle":
                self.last_toggle = int(val)
            elif addr == "/preset":
                self.preset = int(val)
            elif addr == "/preset_bank":
                self.preset_bank = int(val)
            elif addr == "/preamp_state":
                self.states["pre"] = int(val)      # engine name != UI code
            elif addr == "/tgl_state":
                self.states["tgl"] = int(val)
            elif addr in ("/nam_out_gain", "/nam_in_gain"):
                # One global pair, drawn on all five NAM rows.
                which = addr[5:]
                for i in range(5):
                    self.params["/nam%d_%s" % (i, which)] = val
            elif addr.startswith("/nam_sel"):
                self._sel(self.nam_sel, addr[8:], val)
            elif addr.startswith("/ir_sel"):
                self._sel(self.ir_sel, addr[7:], val)
            elif addr == "/lp_drum_ch":
                self.lp_drum_ch = int(val)
            elif addr.startswith("/lp") and "_state" in addr:
                self._lp(self.lp_states, addr, int(val))
            elif addr.startswith("/lp") and "_pos" in addr:
                self._lp(self.lp_pos, addr, float(val))
            elif addr.startswith("/lp") and "_speed" in addr:
                self._lp(self.lp_speed, addr, int(val))
            elif "_" in addr[1:]:
                code, param = addr[1:].split("_", 1)
                if param == "state" and code in neptr.ALL_EFFECTS:
                    self.states[code] = int(val)
                elif any(p[0] == param
                         for p in neptr.EFFECT_PARAMS.get(code, [])):
                    self.params[addr] = val
                else:
                    return                      # not ours; do not grow state
            else:
                return
        if self.on_change is not None:
            self.on_change()

    def _sel(self, arr, digits, val):
        if digits.isdigit() and 0 <= int(digits) < 5:
            arr[int(digits)] = int(val)

    def _lp(self, arr, addr, val):
        ch = int(addr[3]) - 1 if addr[3:4].isdigit() else -1
        if 0 <= ch < 5:
            arr[ch] = val

    def value(self, addr):
        with self.lock:
            return self.params.get(addr)

    def armed_for(self, toggle_id):
        with self.lock:
            return self.last_toggle == toggle_id

    def snapshot(self):
        with self.lock:
            return {"states": dict(self.states), "params": dict(self.params),
                    "menu": self.menu, "last_toggle": self.last_toggle,
                    "preset": self.preset, "preset_bank": self.preset_bank,
                    "nam_sel": list(self.nam_sel), "ir_sel": list(self.ir_sel),
                    "looper": {"states": list(self.lp_states),
                               "pos": list(self.lp_pos),
                               "speed": list(self.lp_speed),
                               "drum_ch": self.lp_drum_ch},
                    "addrs": len(self.addrs),
                    "last_msg": self.last_msg,
                    # A snapshot is >= 8 distinct addresses, the same
                    # threshold the pygame UI uses to tell a real CC102
                    # refresh burst from one stray change message.
                    "synced": len(self.addrs) >= 8}


class EngineIn:
    """UDP listener for the engine's OSC broadcast — if, and only if, the
    port is free. `status` is what the browser is told:

        live       bound; every field below is the engine's own word
        port-busy  another process (normally neptr-ui) owns it; control
                   works, lit state is unknown
        off        listening disabled for this process
    """

    def __init__(self, state, port=ENGINE_OSC_PORT, host="127.0.0.1"):
        self.state = state
        self.port = port
        self.host = host
        self.status = "off"
        self.detail = "not started"
        self.sock = None
        self._stop = threading.Event()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()
        return self

    def _bind(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Deliberately NO SO_REUSEPORT: see the section header. If the port
        # is taken we want the error, not a share of the packets.
        s.settimeout(0.5)
        s.bind((self.host, self.port))
        return s

    def _loop(self):
        backoff = 2.0
        while not self._stop.is_set():
            if self.sock is None:
                if on_device_ui_claims_osc():
                    self.status = "port-busy"
                    self.detail = ("yielded to the on-device UI (live.conf "
                                   "ui = on); control works, lit state "
                                   "unknown")
                    self._stop.wait(backoff)
                    backoff = min(backoff * 1.5, 15.0)
                    continue
                try:
                    self.sock = self._bind()
                    self.status, self.detail = "live", (
                        "listening on %s:%d" % (self.host, self.port))
                    backoff = 2.0
                except OSError as exc:
                    self.status = "port-busy"
                    self.detail = ("%s:%d is held by another process (%s)"
                                   % (self.host, self.port, exc.strerror
                                      or exc))
                    # The on-device UI can stop at any time (ui = off, a
                    # restart); keep trying so the phone picks the feed up
                    # the moment it is free.
                    self._stop.wait(backoff)
                    backoff = min(backoff * 1.5, 15.0)
                    continue
            try:
                data, _ = self.sock.recvfrom(2048)
            except socket.timeout:
                # Cheap 2 Hz re-check: `ui = on` while we hold the port means
                # neptr-ui is about to start and would exit on the bind, so
                # hand it back before it ever tries.
                if on_device_ui_claims_osc():
                    try:
                        self.sock.close()
                    except Exception:
                        pass
                    self.sock = None
                continue
            except OSError:
                self.sock = None
                continue
            msg = osc_decode(data)
            if msg is not None:
                self.state.apply(*msg)

    def stop(self):
        self._stop.set()

    def info(self):
        return {"status": self.status, "detail": self.detail,
                "port": self.port}


class EngineOut:
    """Write side: the exact physical ch1 CCs the Teensy emits, onto the
    Through pool csound reads with -M. Mirrors
    `NEPTR phase4/companion_input.EngineOut` message for message, including
    its retry policy — a failed open is temporary (the pool comes and goes
    around a snd-seq reload), never a session-long disable.

    mido is imported lazily and every failure is non-fatal: on a machine
    without it the rest of the web UI is unaffected and the NEPTR page says
    control is unreachable."""

    RETRY_SECS = 2.0
    RETRY_MAX = 30.0

    def __init__(self, port_name=None, clock=None):
        self._want = port_name or os.environ.get("DEMIURGE_MIDI_OUT")
        self._port = None
        self._clock = clock or time.monotonic
        self._retry_at = 0.0
        self._retry_dt = self.RETRY_SECS
        self._lock = threading.Lock()
        self.detail = "not opened yet"

    def _resolve(self, names):
        if self._want:
            for n in names:
                if self._want in n:
                    return n
            return self._want
        # The Through pool is the only port csound is guaranteed to read
        # (-M 14:0); never write blindly to the Teensy's own port.
        for n in names:
            if "through" in n.lower():
                return n
        return None

    def _open(self):
        import mido
        name = self._resolve(mido.get_output_names())
        if name is None:
            raise RuntimeError("no Through-pool MIDI output found")
        return mido.open_output(name), name

    def _out(self):
        if self._port is not None:
            return self._port
        now = self._clock()
        if now < self._retry_at:
            return None
        try:
            self._port, name = self._open()
            self.detail = "sending on %s" % name
            self._retry_dt = self.RETRY_SECS
        except Exception as exc:
            self._retry_at = now + self._retry_dt
            self._retry_dt = min(self._retry_dt * 2, self.RETRY_MAX)
            self.detail = "MIDI output unavailable (%s); retrying" % exc
            return None
        return self._port

    def cc(self, control, value, channel=0):
        with self._lock:
            port = self._out()
            if port is None:
                return False
            try:
                import mido
                port.send(mido.Message("control_change", channel=channel,
                                       control=control, value=value))
                return True
            except Exception as exc:
                try:
                    self._port.close()
                except Exception:
                    pass
                self._port = None
                self._retry_at = self._clock() + self.RETRY_SECS
                self.detail = "MIDI send failed (%s); reopening" % exc
                return False

    def press(self, control):
        return self.cc(control, 127)

    def release(self, control):
        return self.cc(control, 0)

    def tap(self, control):
        ok = self.press(control)
        self.release(control)
        return ok

    def rotate(self, control, up):
        """One relative encoder detent: CW=127, CCW=0."""
        return self.cc(control, 127 if up else 0)

    def absolute(self, control, value):
        """Absolute 0-127 CC — the expression pedal (CC29) only."""
        return self.cc(control, max(0, min(127, int(value))))

    def sync_request(self):
        ok = self.cc(neptr.CC_UI_SYNC, 127, channel=neptr.UTILITY_CHANNEL)
        self.cc(neptr.CC_UI_SYNC, 0, channel=neptr.UTILITY_CHANNEL)
        return ok

    def info(self):
        return {"ok": self._port is not None, "detail": self.detail}


# ───────────────────────── virtual hardware panel (NEPTR sim) ─────────────
#
# A bare Pi has no Teensy: no encoders, no buttons, no LiDAR, no expression
# pedal. This section is a browser-driven stand-in for all of it, and it
# speaks the TWO PROTOCOLS the real hardware speaks — not one invented one:
#
#   controls (encoders/buttons) -> OSC "/cc" straight to the engine's
#   control-input port (see NEPTR phase4/csound/midi_handler.orc, instr 99).
#   That listener treats a /cc exactly like a Teensy MIDI CC — same kCh/kCC/
#   kVal, same handler_remap.orc permutation — so this sends the PHYSICAL CC
#   numbers the Teensy emits (TEENSYY.ino's ENC_CC/BTN_CC/ENCBTN_CC/
#   JOY_BTN_CC tables), never the logical ones the model layer computes. This
#   is safe here specifically because a bare Pi has no Teensy to collide
#   with; EngineOut (real MIDI, above) is untouched and still the only path
#   for a rig that DOES have hardware attached.
#
#   sensors -> plain-text UDP to demiurge-io's WebSource (sources/web.py in
#   the demiurge-io tree), "<source_id> <raw>\n", one reading per line, RAW
#   in the sensor's own native units — never pre-normalized. This exercises
#   the real conditioning/mapping pipeline instead of bypassing it.

CTL_OSC_PORT = int(os.environ.get("DEMIURGE_CTL_OSC_PORT", "7770"))
SIM_SENSOR_PORT = int(os.environ.get("DEMIURGE_SIM_SENSOR_PORT", "8003"))
# The video engine listens on ONE port for everything it consumes: the
# /sensor + /cc fan-out from demiurge-io, and /nv/param from the browser via
# the endpoint below. Keep them pointed at the same place.
VIDEO_OSC_PORT = int(os.environ.get("DEMIURGE_VIDEO_OSC_PORT", "8004"))

# The Teensy's fixed sensor set (TEENSYY.ino lines ~181-219). Raw, native
# units — conditioning.py in demiurge-io owns the calibration, this is just
# the whitelist that keeps a malformed id from reaching its UDP listener.
SIM_SENSOR_IDS = frozenset(
    ["tof1", "tof2", "exp", "fsr1", "fsr2", "jx", "jy", "ax", "ay", "az",
     "ml", "mr"])


def osc_encode(address, *args):
    """Encode one OSC message. Arg type is inferred: int -> i, float -> f,
    str -> s.

    The engine's `/cc` verb wants three int32 (OSClisten giCtlOSC, "/cc",
    "iii", ...) and ints still encode byte-for-byte as they always did, so
    that path is unchanged. Floats and strings exist for `/nv/param
    <id:s> <val:f>`, which the video engine decodes.

    bool is checked before int deliberately — it is an int subclass in
    Python, and silently sending True as 1 would hide a caller's type bug.

    Address and type-tag string are each null-terminated then padded to a
    4-byte boundary, the same alignment osc_decode() undoes on the way in."""
    def chunk(s):
        b = s.encode("ascii") + b"\0"
        return b + b"\0" * (-len(b) % 4)
    tags, body = "", b""
    for v in args:
        if isinstance(v, bool):
            raise TypeError("bool is not an OSC type: %r" % (v,))
        if isinstance(v, int):
            tags += "i"; body += struct.pack(">i", v)
        elif isinstance(v, float):
            tags += "f"; body += struct.pack(">f", v)
        elif isinstance(v, str):
            tags += "s"; body += chunk(v)
        else:
            raise TypeError("cannot encode %r as OSC" % (v,))
    return chunk(address) + chunk("," + tags) + body



# ───────────────────────── menu order (menus.json) ─────────────────────────
# neptr/menus.json is the single source of truth for the engine's menu-cycle
# ORDER (see neptr/docs/menus.md). The page at #/neptr/menus edits it; a save
# rewrites the file, regenerates the engine include, and pushes the new order
# to the running engine over OSC (/mc, /mc_n -> menu_reload.orc). Menu ids are
# never renumbered; this only reorders and hides.

def neptr_dir():
    """Directory holding menus.json: $NEPTR_DIR, else the repo's neptr/ next
    to web/, else the Pi's ~/NEPTR_phase4."""
    env = os.environ.get("NEPTR_DIR")
    cands = ([Path(env)] if env else []) + [
        Path(__file__).resolve().parent.parent / "neptr",
        HOME / "NEPTR_phase4"]
    for c in cands:
        if (c / "menus.json").exists():
            return c
    return cands[0]


def menus_load(path=None):
    with open(path or (neptr_dir() / "menus.json")) as fh:
        return json.load(fh)


def menus_merge(current, body):
    """(merged_doc, err). The client may only reorder and hide: it sends
    {"menus":[{"id":n,"hidden":bool},...]}; names/slots always come from the
    file on disk. Ids must be unique and exactly the known set."""
    if not isinstance(body, dict) or not isinstance(body.get("menus"), list):
        return None, "body needs a 'menus' list"
    known = {m["id"]: m for m in current["menus"]}
    seen, out = set(), []
    for e in body["menus"]:
        i = e.get("id") if isinstance(e, dict) else None
        if isinstance(i, bool) or not isinstance(i, int):
            return None, "bad menu id: %r" % (i,)
        if i not in known:
            return None, "unknown menu id %d" % i
        if i in seen:
            return None, "duplicate menu id %d" % i
        h = e.get("hidden", False)
        if not isinstance(h, bool):
            return None, "menu %d: hidden must be a bool" % i
        seen.add(i)
        m = dict(known[i]); m["hidden"] = h
        out.append(m)
    missing = sorted(set(known) - seen)
    if missing:
        return None, "menus missing from the list (send them hidden): %s" % missing
    vis = [m for m in out if not m["hidden"]]
    if not vis:
        return None, "at least one menu must be visible"
    seen_cond = False
    for m in vis:
        if m.get("conditional"):
            seen_cond = True
        elif seen_cond:
            return None, "device-conditional menus must come after all always-on menus"
    doc = dict(current); doc["menus"] = out
    return doc, None


def menus_push(doc, host="127.0.0.1", port=None):
    """Send the visible order to the engine: /mc per entry, then /mc_n."""
    cyc = [m["id"] for m in doc["menus"] if not m.get("hidden")]
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        addr = (host, port or CTL_OSC_PORT)
        for pos, mid in enumerate(cyc):
            sock.sendto(osc_encode("/mc", pos, mid), addr)
        sock.sendto(osc_encode("/mc_n", len(cyc)), addr)
        return True
    except OSError:
        return False
    finally:
        sock.close()


def menus_save(body, directory=None, push=menus_push):
    """Validate, write menus.json, regenerate the engine include, push live.
    Returns (http_status, result)."""
    d = Path(directory) if directory else neptr_dir()
    try:
        current = menus_load(d / "menus.json")
    except Exception as e:
        return 500, {"ok": False, "error": "cannot read menus.json: %s" % e}
    doc, err = menus_merge(current, body)
    if err:
        return 400, {"ok": False, "error": err}
    tmp = d / "menus.json.tmp"
    with open(tmp, "w") as fh:
        json.dump(doc, fh, indent=1); fh.write("\n")
    os.replace(tmp, d / "menus.json")
    gen = d / "csound" / "tools" / "gen_menus.py"
    gen_ok, gen_msg = None, "generator not present"
    if gen.exists():
        import subprocess
        try:
            r = subprocess.run([sys.executable, str(gen)], capture_output=True,
                               text=True, timeout=15)
            gen_ok, gen_msg = r.returncode == 0, (r.stdout + r.stderr).strip()
        except Exception as e:
            gen_ok, gen_msg = False, str(e)
    pushed = bool(push(doc))
    return 200, {"ok": True, "generated": gen_ok, "generator": gen_msg,
                 "pushed": pushed,
                 "cycle": [m["id"] for m in doc["menus"] if not m.get("hidden")],
                 "menus": doc["menus"]}


# ───────────────────────── SoundMagic spectral rack ─────────────────────────
# neptr/csound/spectral/sm_params.json is the registry (23 effects, every
# param); sm_heroes.json holds the curated hero/pedal picks and is the input
# gen_sm_params.py reads. The page at #/neptr/spectral drives the engine with
#   /st  ii  fx on          /sp  iif fx p val      -> 127.0.0.1:7770
#   /sh  iii fx k idx       /shp ii  fx idx        (hero slot / pedal, live)
# State comes back by a MIRROR of the engine's /sm_state + /sm_p to port 8005
# (8000 belongs to the pygame UI; 8004 is the video engine). Until the mirror
# is heard the page shows what it last sent and says "unconfirmed".

SM_MIRROR_PORT = int(os.environ.get("DEMIURGE_SM_MIRROR_PORT", "8005"))
SM_MAX_HEROES = 7


def _sm_num(v):
    return (isinstance(v, (int, float)) and not isinstance(v, bool)
            and v == v and abs(v) != float("inf"))


class SmRack:
    """Registry, validation, OSC send and optimistic/confirmed value mirror."""

    def __init__(self, directory=None, sender=None, port=SM_MIRROR_PORT):
        self.dir = Path(directory) if directory else None
        self.sender = sender or self._udp_send
        self.port = port
        self.lock = threading.Lock()
        self.vals = {}          # abs idx -> [value, confirmed]
        self.on = {}            # fx -> [0|1, confirmed]
        self.last_heard = None
        self.status, self.detail = "off", "not started"
        self._stop = threading.Event()

    # -- paths / registry
    def base(self):
        return self.dir or neptr_dir()

    def spectral_dir(self):
        return self.base() / "csound" / "spectral"

    def registry(self):
        with open(self.spectral_dir() / "sm_params.json") as fh:
            return json.load(fh)

    def _effect(self, reg, fx):
        if isinstance(fx, bool) or not isinstance(fx, int):
            return None
        for e in reg["effects"]:
            if e["fx"] == fx:
                return e
        return None

    # -- OSC out
    def _udp_send(self, addr, *args):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.sendto(osc_encode(addr, *args), ("127.0.0.1", CTL_OSC_PORT))
            return True
        except OSError:
            return False
        finally:
            sock.close()

    # -- writes
    def set_param(self, fx, p, val):
        """(status, result). Clamps to the registry range; kinds snap."""
        try:
            reg = self.registry()
        except Exception as e:
            return 503, {"ok": False, "error": "registry unavailable: %s" % e}
        if fx == 23 and p == 0 and _sm_num(val):          # rack default FFT
            v = float(max(0, min(len(reg["fft_sizes"]) - 1, round(val))))
            idx = 23 * reg["stride"]
        else:
            e = self._effect(reg, fx)
            if e is None or e["fx"] == 23:
                return 400, {"ok": False, "error": "bad fx: %r" % (fx,)}
            if isinstance(p, bool) or not isinstance(p, int) \
                    or not 0 <= p < len(e["params"]):
                return 400, {"ok": False, "error": "bad param: %r" % (p,)}
            if not _sm_num(val):
                return 400, {"ok": False, "error": "val must be a number"}
            q = e["params"][p]
            v = float(max(q["min"], min(q["max"], val)))
            if q["kind"] in ("bool", "enum"):
                v = float(round(v))
            idx = q["idx"]
        sent = bool(self.sender("/sp", int(fx), int(p), v))
        with self.lock:
            self.vals[idx] = [v, False]
        return 200, {"ok": True, "fx": fx, "p": p, "val": v, "idx": idx,
                     "sent": sent}

    def set_state(self, fx, on):
        try:
            reg = self.registry()
        except Exception as e:
            return 503, {"ok": False, "error": "registry unavailable: %s" % e}
        e = self._effect(reg, fx)
        if e is None or fx == 23:
            return 400, {"ok": False, "error": "bad fx: %r" % (fx,)}
        if isinstance(on, bool):
            on = int(on)
        if on not in (0, 1) or isinstance(on, float) and on != int(on):
            return 400, {"ok": False, "error": "on must be 0 or 1"}
        on = int(on)
        sent = bool(self.sender("/st", fx, on))
        with self.lock:
            self.on[fx] = [on, False]
        return 200, {"ok": True, "fx": fx, "on": on, "sent": sent}

    def reset(self, fx):
        """Send every param default of one effect (his defaults)."""
        try:
            reg = self.registry()
        except Exception as e:
            return 503, {"ok": False, "error": "registry unavailable: %s" % e}
        e = self._effect(reg, fx)
        if e is None or fx == 23:
            return 400, {"ok": False, "error": "bad fx: %r" % (fx,)}
        out = []
        for q in e["params"]:
            st, res = self.set_param(fx, q["p"], q["default"])
            out.append(res.get("val"))
        return 200, {"ok": True, "fx": fx, "values": out}

    # -- hero pins
    def heroes_save(self, body, push=True):
        """body {fx, heroes:[p|null x<=7] (index = slot P1..P7), pedal:p?}.
        Validate -> write sm_heroes.json -> regenerate -> push /sh /shp."""
        if not isinstance(body, dict):
            return 400, {"ok": False, "error": "body must be an object"}
        try:
            reg = self.registry()
        except Exception as e:
            return 503, {"ok": False, "error": "registry unavailable: %s" % e}
        fx = body.get("fx")
        e = self._effect(reg, fx)
        if e is None or fx == 23:
            return 400, {"ok": False, "error": "bad fx: %r" % (fx,)}
        n = len(e["params"])
        hs = body.get("heroes")
        if not isinstance(hs, list):
            return 400, {"ok": False, "error": "heroes must be a list"}
        if len(hs) > SM_MAX_HEROES:
            return 400, {"ok": False,
                         "error": "at most %d heroes per effect" % SM_MAX_HEROES}
        slots = []
        for h in hs:
            if h is None or (not isinstance(h, bool) and h == -1):
                slots.append(None)
            elif isinstance(h, bool) or not isinstance(h, int) or not 0 <= h < n:
                return 400, {"ok": False, "error": "bad hero param: %r" % (h,)}
            else:
                slots.append(h)
        named = [h for h in slots if h is not None]
        if len(set(named)) != len(named):
            return 400, {"ok": False, "error": "a param can hold only one hero slot"}
        pedal = body.get("pedal", e["pedal"])
        if isinstance(pedal, bool) or not isinstance(pedal, int) \
                or not 0 <= pedal < n:
            return 400, {"ok": False, "error": "bad pedal param: %r" % (pedal,)}
        while slots and slots[-1] is None:
            slots.pop()

        sd = self.spectral_dir()
        hj = sd / "sm_heroes.json"
        gen = sd / "tools" / "gen_sm_params.py"
        if not gen.exists():
            return 500, {"ok": False, "error": "gen_sm_params.py not found"}
        import subprocess

        def run_gen():
            try:
                r = subprocess.run([sys.executable, str(gen)],
                                   capture_output=True, text=True, timeout=30)
                return r.returncode == 0, (r.stdout + r.stderr).strip()
            except Exception as ex:
                return False, str(ex)

        if not hj.exists():                      # first run seeds from OVERLAY
            ok, msg = run_gen()
            if not ok or not hj.exists():
                return 500, {"ok": False, "error": "seed failed: %s" % msg}
        old_text = hj.read_text()
        picks = json.loads(old_text)
        names = [q["name"] for q in e["params"]]
        picks[e["code"]] = {
            "heroes": [None if h is None else names[h] for h in slots],
            "pedal": names[pedal]}
        tmp = hj.with_name("sm_heroes.json.tmp")
        with open(tmp, "w") as fh:
            json.dump(picks, fh, indent=1); fh.write("\n")
        os.replace(tmp, hj)
        gen_ok, gen_msg = run_gen()
        if not gen_ok:                           # never leave a half-applied state
            hj.write_text(old_text)
            run_gen()
            return 500, {"ok": False, "error": "generator failed: %s" % gen_msg}
        pushed = None
        if push:
            base = fx * reg["stride"]
            pushed = True
            for k in range(SM_MAX_HEROES):
                idx = base + slots[k] if k < len(slots) and slots[k] is not None else -1
                pushed &= bool(self.sender("/sh", fx, k, idx))
            pushed &= bool(self.sender("/shp", fx, base + pedal))
        return 200, {"ok": True, "fx": fx, "generated": True,
                     "generator": gen_msg, "pushed": pushed,
                     "heroes": slots, "pedal": pedal}

    # -- mirror in (engine -> web, port 8005)
    def apply(self, addr, args):
        with self.lock:
            self.last_heard = time.time()
            if addr == "/sm_state" and len(args) == 2:
                self.on[int(args[0])] = [int(args[1]), True]
            elif addr == "/sm_p" and len(args) == 3:
                reg_stride = 32
                self.vals[int(args[0]) * reg_stride + int(args[1])] = \
                    [float(args[2]), True]

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()
        return self

    def stop(self):
        self._stop.set()

    def _loop(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.settimeout(0.5)
            s.bind(("127.0.0.1", self.port))
        except OSError as exc:
            self.status = "port-busy"
            self.detail = "127.0.0.1:%d: %s" % (self.port, exc.strerror or exc)
            return
        self.status, self.detail = "live", "listening on 127.0.0.1:%d" % self.port
        while not self._stop.is_set():
            try:
                data, _ = s.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                break
            msg = osc_decode_multi(data)
            if msg is not None:
                self.apply(*msg)
        s.close()

    def snapshot(self):
        with self.lock:
            heard = self.last_heard
            return {
                "mirror": {"status": self.status, "detail": self.detail,
                           "port": self.port,
                           "live": heard is not None and time.time() - heard < 30,
                           "last_heard": heard},
                "on": {str(k): {"on": v[0], "confirmed": v[1]}
                       for k, v in self.on.items()},
                "vals": {str(k): {"v": v[0], "confirmed": v[1]}
                         for k, v in self.vals.items()}}


_SM_DEFAULT = []


def default_sm():
    if not _SM_DEFAULT:
        _SM_DEFAULT.append(SmRack())
    return _SM_DEFAULT[0]


# Discrete controls, by the firmware's own name (TEENSYY.ino), and their CC
# numbers — the inverse of demiurge-io's sources/teensy.py tables. Encoders
# (CC20-26) are deliberately NOT here: an encoder is a relative delta, not a
# level, and a delta on the /sensor bus would latch a phantom control.
SIM_CONTROL_SOURCE = {}
SIM_CONTROL_SOURCE.update({60 + i: "b%d" % (i + 1) for i in range(9)})
SIM_CONTROL_SOURCE.update({70 + i: "eb%d" % (i + 1) for i in range(7)})
SIM_CONTROL_SOURCE[77] = "jb"


class SimOut:
    """Both write paths of the virtual panel, as one small UDP sender. Both
    destinations are connectionless and local — nothing here is opened,
    retried, or kept alive; a failed sendto is just reported to the caller."""

    def __init__(self, cc_host="127.0.0.1", cc_port=CTL_OSC_PORT,
                 sensor_host="127.0.0.1", sensor_port=SIM_SENSOR_PORT):
        self.cc_addr = (cc_host, cc_port)
        self.sensor_addr = (sensor_host, sensor_port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def cc(self, ch, cc, val):
        try:
            self.sock.sendto(osc_encode("/cc", ch, cc, val), self.cc_addr)
        except OSError:
            return False
        # A press must ALSO enter the conditioning path the panel's sensors
        # already use, or a virtual button is not the same event as a real
        # one: it would reach csound and nothing else on the bus — no video
        # engine, no arcade. Sent as a level (0/1) under the control's own
        # firmware name, exactly as demiurge-io names it, so there is one
        # name for one control across the whole system.
        #
        # The /cc above is kept unchanged and unconditionally: csound's
        # control-input path is proven and is not being rerouted through a
        # daemon that may not be running.
        source = SIM_CONTROL_SOURCE.get(cc)
        if source is not None:
            self.sensor(source, 1 if val else 0)
        return True

    def sensor(self, source_id, raw):
        try:
            line = ("%s %s\n" % (source_id, raw)).encode("ascii")
            self.sock.sendto(line, self.sensor_addr)
            return True
        except OSError:
            return False


def osc_decode_any(data):
    """Every message in one datagram, bundle or not, as [(address, [args])].

    The video engine bundles its state broadcast — `#bundle` + an 8-byte time
    tag, then each element prefixed by its own int32 size — so a plain
    single-message decode sees a packet starting with "#bundle" and correctly
    reports it as not-an-address. Unbundling is four lines and is the whole
    difference between "the engine says nothing" and "the engine says
    everything"."""
    if data[:8] == b"#bundle\0":
        out, pos = [], 16
        while pos + 4 <= len(data):
            (size,) = struct.unpack(">i", data[pos:pos + 4])
            pos += 4
            if size < 0 or pos + size > len(data):
                break
            out += osc_decode_any(data[pos:pos + size])
            pos += size
        return out
    one = osc_decode_multi(data)
    return [one] if one else []


class VideoOut:
    """The link to the video engine, both directions, on one UDP socket.

    Deliberately NOT routed through the sensor path. A video fader is a direct
    parameter set, not a transducer reading; declaring one as a "source" in
    sensors.json would put it in the NEPTR sensor list with a calibration
    range, which is wrong on its face and gives two mechanisms for one job.

    ── why this socket also LISTENS ────────────────────────────────────────

    A write-only surface drifts. Load a preset, touch the device's own panel,
    or let the governor thin the swarm, and every remote fader is describing a
    rig that has moved on. The engine already solves that: whichever endpoint
    last sent it traffic gets the whole bank back at 10 Hz as `/nv/sel` plus
    one `/nv/state <id:s> <norm:f>` per parameter.

    That reply comes back to the SOURCE ADDRESS of what we sent — so it lands
    on this exact socket and nowhere else. Which is why the receive side lives
    here rather than in a second listener object: a separate bound port could
    never be the engine's peer, because the engine is not told an address, it
    remembers one.

    ── and why it pings ────────────────────────────────────────────────────

    The engine stops broadcasting five seconds after its peer goes quiet, and
    that is the right behaviour — nothing registers, renews or unregisters,
    and a surface that walked out of wifi range stops costing datagrams. So
    the way to stay subscribed is simply to keep talking, and the cheapest
    honest thing to say is `/nv/ping`: inside the `/nv/` namespace, so the
    receiver drops it rather than feeding it to the mapping table, and not a
    verb it knows, so it changes nothing. It makes us the peer and does
    nothing else.

    We only ping while a browser is actually looking (`want_until`). Idling
    surfaces should not hold peer-ship: there is exactly one, and taking it
    from a surface that is being played would be worse than a stale fader."""

    WANT = 6.0             # s of pinging bought by one browser read
    PING = 1.0             # s between pings, well inside the engine's 5 s
    STALE = 1.5            # s without a broadcast before we call it not-live
    # Every bank sendLists() answers. Named here so an incomplete catalogue is
    # a fact the page can see rather than a list that is quietly short.
    BANKS = frozenset(["param", "kernel", "palette", "color", "trap", "mode",
                       "op"])

    def __init__(self, host="127.0.0.1", port=VIDEO_OSC_PORT):
        self.addr = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Bound explicitly rather than left to the first sendto: the reader
        # thread starts below and would otherwise race an unbound socket.
        self.sock.bind(("", 0))
        self.lock = threading.Lock()
        self.params = {}       # parameter id -> normalized 0..1 TARGET
        self._filling = {}     # the broadcast currently arriving
        # The engine's SELF-DESCRIPTION: bank name -> [{id, name}, ...],
        # including "param", where name packs group|label|min|max|def.
        #
        # Asked for once and cached, because it only changes when the engine
        # restarts or its generator changes the bank's dynamic tail -- and the
        # 10 Hz state broadcast already tells us when that happened. A surface
        # that reads this never has to carry its own copy of the engine's
        # labels, ranges or groups, which is the whole point: the hand-written
        # copy is what goes stale.
        self.lists = {}
        self._lfill = {}       # bank -> list being assembled
        self._asked = 0.0      # when we last sent /nv/list
        self.mode = None
        self.kernel = None
        self.last_rx = 0.0
        self.want_until = 0.0
        for fn in (self._reader, self._keepalive):
            threading.Thread(target=fn, daemon=True).start()

    # ---- out ----

    def _send(self, data):
        try:
            self.sock.sendto(data, self.addr)
            return True
        except OSError:
            return False

    def param(self, param_id, value):
        self.want()
        return self._send(osc_encode("/nv/param", param_id, float(value)))

    def select(self, verb, index):
        """/nv/preset | /nv/mode | /nv/kernel — the discrete selections."""
        self.want()
        return self._send(osc_encode("/nv/" + verb, int(index)))

    def want(self):
        """Somebody is watching: keep us the engine's peer for a few seconds."""
        with self.lock:
            self.want_until = time.monotonic() + self.WANT

    def catalog(self):
        """The engine's self-description, asking for it if we have none.

        Re-asked on a slow timer while incomplete rather than once at startup:
        the engine may not have been running when the first browser looked,
        and `/nv/list` is only answered to the current peer -- so the ask has
        to happen after `want()` has made us one. Once every bank has arrived
        this stops sending anything at all."""
        self.want()
        with self.lock:
            have = set(self.lists)
            due = time.monotonic() - self._asked > 2.0
            if not have >= self.BANKS and due:
                self._asked = time.monotonic()
                ask = True
            else:
                ask = False
            out = {k: list(v) for k, v in self.lists.items()}
        if ask:
            self._send(osc_encode("/nv/list"))
        return {"lists": out, "complete": set(out) >= self.BANKS}

    # ---- in ----

    def _keepalive(self):
        while True:
            with self.lock:
                wanted = time.monotonic() < self.want_until
            if wanted:
                self._send(osc_encode("/nv/ping"))
            time.sleep(self.PING)

    def _reader(self):
        while True:
            try:
                data, _ = self.sock.recvfrom(65535)
            except OSError:
                time.sleep(0.25)
                continue
            now = time.monotonic()
            got = False
            for addr, args in osc_decode_any(data):
                if addr == "/nv/state" and len(args) >= 2 \
                        and isinstance(args[0], str):
                    with self.lock:
                        self._filling[args[0]] = float(args[1])
                    got = True
                elif addr == "/nv/list_item" and len(args) >= 4 \
                        and isinstance(args[0], str):
                    # <bank:s> <index:i> <id:s> <name:s>. Assembled into
                    # _lfill and published only on list_end, for the same
                    # reason the state bank is: a half-arrived list drawn as
                    # if complete is a UI missing controls with no sign that
                    # anything is missing.
                    with self.lock:
                        self._lfill.setdefault(args[0], []).append(
                            {"id": args[2], "name": args[3]})
                    got = True
                elif addr == "/nv/list_end" and len(args) >= 2 \
                        and isinstance(args[0], str):
                    bank = args[0]
                    with self.lock:
                        items = self._lfill.pop(bank, [])
                        # The engine says how many it sent. Trust the count
                        # over what arrived: UDP can drop, and a short list is
                        # indistinguishable from a real one unless we check.
                        if len(items) == int(args[1]):
                            self.lists[bank] = items
                    got = True
                elif addr == "/nv/sel" and len(args) >= 2:
                    # `/nv/sel` is written FIRST in every broadcast, so seeing
                    # one means the previous broadcast is complete: publish it
                    # and start filling the next.
                    #
                    # Replaced, never merged. The bank's dynamic tail is
                    # rebuilt whenever the generator or the chain changes, and
                    # a dict that only ever gained keys would keep serving the
                    # PREVIOUS generator's knobs forever — the browser would
                    # draw live-looking faders for parameters that no longer
                    # exist, and the engine would silently ignore every write
                    # to them. Observed: nine `k.*` ids reported for a
                    # generator that has four.
                    with self.lock:
                        if self._filling:
                            self.params = self._filling
                        self._filling = {}
                        self.mode, self.kernel = int(args[0]), int(args[1])
                    got = True
            if got:
                with self.lock:
                    self.last_rx = now

    def snapshot(self):
        """What the engine last said about itself. `live` is the fact the page
        actually needs: whether these numbers are the engine's or nobody's."""
        self.want()
        with self.lock:
            age = time.monotonic() - self.last_rx if self.last_rx else None
            return {
                "live": age is not None and age < self.STALE,
                "age": round(age, 2) if age is not None else None,
                "port": self.addr[1],
                "mode": self.mode,
                "kernel": self.kernel,
                "params": dict(self.params),
            }


class ParamChase:
    """Closed-loop fader: drive an engine parameter to a target value.

    The engine has no absolute path for these — every param edit is a stream
    of RELATIVE encoder detents — so a fader is a control loop, not a write.
    The pygame UI runs it against the finger at 30 Hz; the browser is too far
    away for that, so the loop lives HERE and the phone only posts targets.

    Two properties are load-bearing and both come straight from the pygame
    UI's tick_param_chase:

    * PARAM_BURST detents per tick, hard ceiling. csound's midiin drains ONE
      event per k-cycle; a sustained 300 msg/s flood was observed to stall
      the engine's MIDI input until restart (2026-08-09).
    * the loop closes on the OSC-ECHOED value, never on a value this process
      predicted, and stops inside a PARAM_DEADBAND fraction of the span.

    With no OSC feed there is no loop to close, so chasing is refused and the
    browser offers single detents instead — one tap, one click of the knob."""

    TICK = 1.0 / 30.0
    LINGER = 1.5           # s of settling allowed after the last target

    def __init__(self, out, state, clock=None, sleep=None):
        self.out = out
        self.state = state
        self.clock = clock or time.monotonic
        self.sleep = sleep or time.sleep
        self.lock = threading.Lock()
        self.job = None
        self.thread = None

    def set_target(self, code, param, phys, lo, hi, target, deadline=None):
        with self.lock:
            self.job = {"code": code, "param": param, "phys": phys,
                        "lo": lo, "hi": hi, "target": target,
                        "until": (deadline if deadline is not None
                                  else self.clock() + self.LINGER)}
            if self.thread is None or not self.thread.is_alive():
                self.thread = threading.Thread(target=self._run, daemon=True)
                self.thread.start()

    def _run(self):
        while True:
            with self.lock:
                job = self.job
                if job is None or self.clock() > job["until"]:
                    self.job = None
                    return
                job = dict(job)
            if not self.step(job):
                with self.lock:
                    if self.job is not None and self.clock() > self.job["until"]:
                        self.job = None
                        return
            self.sleep(self.TICK)

    def step(self, job):
        """One tick. Returns True if any detent went out (so a caller can
        drive this synchronously in a test without a thread)."""
        addr = neptr.osc_addr(job["code"], job["param"])
        cur = self.state.value(addr)
        try:
            cur = float(cur)
        except (TypeError, ValueError):
            return False                    # no echo yet: nothing to close on
        span = (job["hi"] - job["lo"]) or 1
        err = job["target"] - cur
        if abs(err) <= abs(span) * neptr.PARAM_DEADBAND:
            return False
        # ~50 detents per span is the engine's dominant $ENC_ROT convention
        # (0.02 steps on 0..1); burst just enough to close the error.
        step_est = abs(span) / 50.0
        n = max(1, min(neptr.PARAM_BURST, round(abs(err) / step_est)))
        up = (err > 0) == (span > 0)
        for _ in range(n):
            self.out.rotate(job["phys"], up)
        return True


# ───────────────────────── collector ─────────────────────────

_throttle_hist = deque()   # (ts, had_now_bits) over the last 60 s


def _int_or_none(s):
    try:
        return int(s.strip())
    except Exception:
        return None


def collect(probes, prev):
    """One status frame. Probe failure -> field null; frame still ships.
    prev = previous frame (carries '_stat' raw text for per-core deltas)."""
    now = time.time()
    f = {"ts": round(now, 3)}

    host = probes.run(["hostname"])
    f["host"] = host.strip() if host else None

    up = probes.read("/proc/uptime")
    f["uptime_s"] = float(up.split()[0]) if up else None

    f["temp_c"] = parse_temp(probes.run(["vcgencmd", "measure_temp"]))
    f["pmic_temp_c"] = parse_temp(probes.run(["vcgencmd", "measure_temp", "pmic"]))

    cur = _int_or_none(probes.read(CPUFREQ + "/scaling_cur_freq") or "")
    fmin = _int_or_none(probes.read(CPUFREQ + "/cpuinfo_min_freq") or "")
    fmax = _int_or_none(probes.read(CPUFREQ + "/scaling_max_freq") or "")
    f["freq"] = {"cur": cur, "min": fmin, "max": fmax}
    f["pwr"] = derive_power(fmax) if fmax else None

    gov = probes.read(CPUFREQ + "/scaling_governor")
    f["gov"] = gov.strip() if gov else None

    stat = probes.read("/proc/stat")
    ncores = (len([l for l in stat.splitlines() if re.match(r"^cpu\d+ ", l)])
              if stat else (os.cpu_count() or 1))
    f["ncores"] = ncores
    prev_stat = prev.get("_stat") if prev else None
    f["percore"] = percore_cpu(prev_stat, stat) if (prev_stat and stat) else []
    f["_stat"] = stat

    la = probes.read("/proc/loadavg")
    f["load"] = parse_loadavg(la, ncores) if la else None

    mi = probes.read("/proc/meminfo")
    try:
        f["mem"] = parse_meminfo(mi) if mi else None
    except Exception:
        f["mem"] = None

    f["throttled"] = parse_throttled(probes.run(["vcgencmd", "get_throttled"]))

    volt = probes.run(["vcgencmd", "measure_volts", "core"])
    m = re.search(r"volt=([\d.]+)", volt or "")
    f["volt_core"] = float(m.group(1)) if m else None

    adc = probes.run(["vcgencmd", "pmic_read_adc"])
    m = re.search(r"EXT5V_V\s+volt\(\d+\)=([\d.]+)", adc or "")
    f["ext5v"] = float(m.group(1)) if m else None

    online = probes.read("/sys/devices/system/cpu/online")
    f["cores_online"] = online.strip() if online else None

    audio = probes.run(["systemctl", "is-active", "demiurge"], check=False)
    f["audio"] = audio.strip() if audio else None
    ui = probes.run(["systemctl", "is-active", "neptr-ui"], check=False)
    f["ui"] = ui.strip() if ui else None

    lc_text = probes.live_conf()
    if lc_text is not None:
        try:
            lc = parse_liveconf(lc_text)
        except Exception:
            lc = None
    else:
        lc = None
    f["patch"] = lc["set"] if lc else None
    f["power_tag"] = lc["power"] if lc else None
    f["chains"] = lc["chains"] if lc else []
    f["has_input"] = lc["has_input"] if lc else False

    sets = probes.list_sets()
    try:
        f["patches"] = parse_sets(sets) if sets is not None else []
    except Exception:
        f["patches"] = []

    try:
        f["interfaces"] = parse_interfaces(probes.interfaces())
    except Exception:
        f["interfaces"] = []

    try:
        f["sync"] = parse_sync(probes.sync())
    except Exception:
        f["sync"] = None

    try:
        f["touch"] = parse_touch(probes.touch())
    except Exception:
        f["touch"] = None

    try:
        f["audio_settings"] = parse_audio(probes.audio_settings())
    except Exception:
        f["audio_settings"] = None

    try:
        f["graph"] = parse_graph(probes.graph())
    except Exception:
        f["graph"] = None

    try:
        f["wifi"] = parse_wifi_status(probes.wifi_status())
    except Exception:
        f["wifi"] = None

    try:
        f["wifi_list"] = parse_wifi_list(probes.wifi_list())
    except Exception:
        f["wifi_list"] = []

    xr = probes.run(["journalctl", "-u", "demiurge", "--since", "-60s", "-o", "cat"])
    f["xrun_60s"] = (len([l for l in xr.splitlines() if XRUN_RE.search(l)])
                     if xr is not None else None)

    had_now = bool(f["throttled"] and f["throttled"]["now"])
    _throttle_hist.append((now, had_now))
    while _throttle_hist and _throttle_hist[0][0] < now - 60:
        _throttle_hist.popleft()
    f["throttle_60s"] = sum(1 for _, b in _throttle_hist if b)

    return f


def public_frame(frame):
    """Frame minus private keys (raw /proc/stat carried for deltas)."""
    return {k: v for k, v in frame.items() if not k.startswith("_")}


def write_status(frame, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(public_frame(frame)))
    os.replace(tmp, path)


# ───────────────────────── http server ─────────────────────────

MIME = {".html": "text/html; charset=utf-8", ".js": "text/javascript",
        ".css": "text/css", ".svg": "image/svg+xml", ".png": "image/png",
        ".ico": "image/x-icon"}


class App:
    """Shared state: rolling history + SSE subscribers."""

    def __init__(self):
        self.history = deque(maxlen=300)
        self.subs = []
        self.nsubs = []          # NEPTR subscribers (separate, faster channel)
        self.ssubs = []          # sensor-live subscribers (fastest channel —
                                  # meter readout, not a form; see publish_sensors)
        self.lock = threading.Lock()

    @property
    def latest(self):
        return self.history[-1] if self.history else None

    def subscribe(self):
        q = queue.Queue()
        with self.lock:
            self.subs.append(q)
        return q

    def unsubscribe(self, q):
        with self.lock:
            if q in self.subs:
                self.subs.remove(q)

    def publish(self, frame):
        pf = public_frame(frame)
        with self.lock:
            self.history.append(pf)
            subs = list(self.subs)
        for q in subs:
            q.put(pf)

    # NEPTR rides its own channel: slot lighting has to feel like a pedal
    # switch, and the 2 s system poll is far too slow for that. Pushes are
    # coalesced (see publish_neptr) so a param sweep cannot flood the socket.

    def subscribe_neptr(self):
        q = queue.Queue()
        with self.lock:
            self.nsubs.append(q)
        return q

    def unsubscribe_neptr(self, q):
        with self.lock:
            if q in self.nsubs:
                self.nsubs.remove(q)

    def publish_neptr(self, snap):
        with self.lock:
            subs = list(self.nsubs)
        for q in subs:
            q.put(snap)

    # Sensor live values ride a third channel, separate from both of the
    # above: it is keyed to SensorState (values only, no config), coalesced
    # at SENSOR_PUSH_HZ in main() the same way engine changes are, and it
    # exists so the 200 ms /api/neptr/sensors poll (which re-fetched the
    # whole sources/maps config just to read a few floats) can go away.

    def subscribe_sensors(self):
        q = queue.Queue()
        with self.lock:
            self.ssubs.append(q)
        return q

    def unsubscribe_sensors(self, q):
        with self.lock:
            if q in self.ssubs:
                self.ssubs.remove(q)

    def publish_sensors(self, values):
        with self.lock:
            subs = list(self.ssubs)
        for q in subs:
            q.put(values)


EXAMPLES_DIR = Path(__file__).resolve().parent / "examples"


def make_server(app, port, static_dir, actions=None, engine=None, sensors=None,
                sm=None, pool=None, examples_dir=None):
    sm = sm or default_sm()
    examples_root = Path(examples_dir or EXAMPLES_DIR).resolve()
    static_root = Path(static_dir).resolve()
    model_json = json.dumps(neptr.describe())

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):
            pass

        # ---- helpers ----
        def _send(self, status, ctype, body):
            if isinstance(body, str):
                body = body.encode()
            self.send_response(status)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(body)

        def _json(self, obj, status=200):
            self._send(status, "application/json", json.dumps(obj))

        # ---- GET ----
        def do_GET(self):
            path = self.path.split("?", 1)[0]
            if path == "/api/status":
                self._json(app.latest or {})
            elif path == "/api/history":
                self._json(list(app.history))
            elif path == "/api/events":
                self._sse()
            elif path == "/api/neptr/model":
                # The instrument's shape: 19 menus, their slots, every param
                # with its encoder CC and arming toggle. Static for the life
                # of the process — the browser fetches it once.
                self._send(200, "application/json", model_json)
            elif path == "/api/neptr":
                self._json(engine.snapshot() if engine else
                           {"live": False, "link": {"status": "off",
                                                    "detail": "no engine link",
                                                    "port": ENGINE_OSC_PORT}})
            elif path == "/api/neptr/events":
                self._nsse()
            elif path == "/api/neptr/sensors":
                if sensors is None:
                    self._json({"sources": [], "maps": [], "live": {}})
                    return
                cfg = sensors.read_config()
                self._json({"sources": cfg["sources"], "maps": cfg["maps"],
                           "live": sensors.state.snapshot()})
            elif path == "/api/neptr/sensors/events":
                self._ssse()
            elif path == "/api/neptr/menus":
                try:
                    self._json(menus_load())
                except Exception as e:
                    self._json({"error": "menus.json unavailable: %s" % e}, 503)
            elif path == "/api/neptr/sm/registry":
                try:
                    self._json(sm.registry())
                except Exception as e:
                    self._json({"error": "sm_params.json unavailable: %s" % e}, 503)
            elif path == "/api/neptr/sm/state":
                self._json(sm.snapshot())
            elif path == "/api/neptr/expr-targets":
                self._json(sensors.expr_targets() if sensors else [])
            elif path == "/api/pool/params":
                self._json({"params": pool.params() if pool else [],
                            "hard_maps": pool.hard_maps() if pool else []})
            elif path == "/api/pool/sources":
                self._json({"sources": pool.sources() if pool else []})
            elif path == "/api/pool/map":
                status, res = pool.read_map() if pool else (
                    503, {"error": "pool unavailable"})
                self._json(res, status)
            elif path == "/api/pool/events":
                self._psse()
            elif path.startswith("/examples/"):
                self._examples_static(path)
            elif path == "/api/video/catalog":
                # What the engine IS, as opposed to where its knobs are
                # standing. Separate from /state because it is large and
                # nearly constant, while /state is polled four times a second.
                out = getattr(actions, "video", None) if actions else None
                self._json(out.catalog() if out else
                           {"lists": {}, "complete": False})
            elif path == "/api/video/state":
                # The engine's own 10 Hz broadcast, as last heard. Polled
                # rather than pushed: reading it is also what keeps this
                # server the engine's peer (VideoOut.snapshot -> want()), so a
                # page that stopped looking stops subscribing by itself, with
                # nothing to unregister.
                out = getattr(actions, "video", None) if actions else None
                self._json(out.snapshot() if out else
                           {"live": False, "params": {}, "mode": None,
                            "kernel": None, "age": None,
                            "port": VIDEO_OSC_PORT})
            elif path.startswith("/patch/"):
                self._patch_static(path)
            else:
                self._static(path)

        def _sse(self):
            q = app.subscribe()
            try:
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                while True:
                    try:
                        frame = q.get(timeout=15)
                        self.wfile.write(b"data: " + json.dumps(frame).encode()
                                         + b"\n\n")
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                app.unsubscribe(q)

        def _nsse(self):
            q = app.subscribe_neptr()
            try:
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                if engine is not None:
                    self.wfile.write(b"data: "
                                     + json.dumps(engine.snapshot()).encode()
                                     + b"\n\n")
                    self.wfile.flush()
                while True:
                    try:
                        snap = q.get(timeout=15)
                        self.wfile.write(b"data: " + json.dumps(snap).encode()
                                         + b"\n\n")
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                app.unsubscribe_neptr(q)

        def _ssse(self):
            # Same shape as _nsse: send the current values immediately (a
            # screen opened mid-sweep should not show a blank meter until
            # the next OSC packet arrives), then block on the queue with a
            # 15 s keepalive so an idle instrument does not need a raw
            # socket ping from the client side and this connection does not
            # look dead to a proxy in between.
            q = app.subscribe_sensors()
            try:
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                if sensors is not None:
                    self.wfile.write(b"data: "
                                     + json.dumps(sensors.state.snapshot()).encode()
                                     + b"\n\n")
                    self.wfile.flush()
                while True:
                    try:
                        values = q.get(timeout=15)
                        self.wfile.write(b"data: " + json.dumps(values).encode()
                                         + b"\n\n")
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                app.unsubscribe_sensors(q)

        def _psse(self):
            # Pool `/param` echoes (and this server's own /api/pool/set
            # writes) as `data: {"path","value"}`. Same keepalive shape as
            # the other SSE channels.
            if pool is None:
                self._json({"error": "pool unavailable"}, 503)
                return
            q = pool.subscribe()
            try:
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                self.wfile.write(b": open\n\n")
                self.wfile.flush()
                while True:
                    try:
                        ev = q.get(timeout=15)
                        self.wfile.write(b"data: " + json.dumps(ev).encode()
                                         + b"\n\n")
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                pool.unsubscribe(q)

        def _examples_static(self, path):
            """Read-only /examples/<file>.html|css|js|json from web/examples."""
            import urllib.parse
            rel = urllib.parse.unquote(path)[len("/examples/"):]
            try:
                target = (examples_root / rel).resolve()
            except Exception:
                self._json({"error": "not found"}, 404)
                return
            ctype = {".html": "text/html; charset=utf-8", ".css": "text/css",
                     ".js": "text/javascript",
                     ".json": "application/json"}.get(target.suffix.lower())
            if ctype is None or not str(target).startswith(str(examples_root) + os.sep) \
                    or not target.is_file():
                self._json({"error": "not found"}, 404)
                return
            self._send(200, ctype, target.read_bytes())

        def _static(self, path):
            import urllib.parse
            rel = urllib.parse.unquote(path).lstrip("/") or "index.html"
            try:
                target = (static_root / rel).resolve()
            except Exception:
                self._json({"error": "not found"}, 404)
                return
            if not (str(target) == str(static_root)
                    or str(target).startswith(str(static_root) + os.sep)) \
                    or not target.is_file():
                self._json({"error": "not found"}, 404)
                return
            ctype = MIME.get(target.suffix, "application/octet-stream")
            self._send(200, ctype, target.read_bytes())

        def _patch_static(self, path):
            """Read-only /patch/<name>/<file>: a patch folder's phone UI.
            html/css/js/json only, NEVER .conf; same containment check as
            _static, rooted at patches/<name>/."""
            import urllib.parse
            parts = urllib.parse.unquote(path)[len("/patch/"):].split("/", 1)
            name = parts[0]
            rel = (parts[1] if len(parts) > 1 else "") or "index.html"
            if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", name):
                self._json({"error": "not found"}, 404)
                return
            root = (PATCHES_DIR / name).resolve()
            try:
                target = (root / rel).resolve()
            except Exception:
                self._json({"error": "not found"}, 404)
                return
            ctype = {".html": "text/html; charset=utf-8",
                     ".css": "text/css", ".js": "text/javascript",
                     ".json": "application/json"}.get(target.suffix.lower())
            if ctype is None or not root.is_dir() \
                    or not str(target).startswith(str(root) + os.sep) \
                    or not target.is_file():
                self._json({"error": "not found"}, 404)
                return
            self._send(200, ctype, target.read_bytes())

        # ---- POST (actions wired in Task 4) ----
        def do_POST(self):
            if self.path.split("?", 1)[0] in ("/api/pool/set", "/api/pool/map"):
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                    body = json.loads(self.rfile.read(n) or b"{}")
                    if not isinstance(body, dict):
                        raise ValueError
                except Exception:
                    self._json({"ok": False, "error": "bad json"}, 400)
                    return
                if pool is None:
                    self._json({"ok": False, "error": "pool unavailable"}, 503)
                    return
                if self.path.split("?", 1)[0] == "/api/pool/set":
                    status, res = pool.set(body.get("path"), body.get("value"))
                else:
                    status, res = pool.write_map(body)
                self._json(res, status)
                return
            if self.path.split("?", 1)[0].startswith("/api/neptr/sm/"):
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                    body = json.loads(self.rfile.read(n) or b"{}")
                    if not isinstance(body, dict):
                        raise ValueError
                except Exception:
                    self._json({"ok": False, "error": "bad json"}, 400)
                    return
                sp = self.path.split("?", 1)[0]
                if sp == "/api/neptr/sm/param":
                    status, res = sm.set_param(body.get("fx"), body.get("p"),
                                               body.get("val"))
                elif sp == "/api/neptr/sm/state":
                    status, res = sm.set_state(body.get("fx"), body.get("on"))
                elif sp == "/api/neptr/sm/reset":
                    status, res = sm.reset(body.get("fx"))
                else:
                    status, res = 404, {"error": "not found"}
                self._json(res, status)
                return
            if actions is None:
                self._json({"error": "actions unavailable"}, 503)
                return
            try:
                n = int(self.headers.get("Content-Length") or 0)
                body = json.loads(self.rfile.read(n) or b"{}")
                if not isinstance(body, dict):
                    raise ValueError
            except Exception:
                self._json({"error": "bad json"}, 400)
                return
            path = self.path.split("?", 1)[0]
            try:
                if path == "/api/patch":
                    status, res = actions.patch(body.get("name"))
                elif path == "/api/power":
                    status, res = actions.power(body.get("level"))
                elif path == "/api/audio":
                    status, res = actions.audio(body.get("verb"))
                elif path == "/api/interface":
                    status, res = actions.interface(body.get("name"))
                elif path == "/api/sync":
                    status, res = actions.sync(body.get("role"))
                elif path == "/api/touch":
                    status, res = actions.touch(body.get("verb"))
                elif path == "/api/audio-set":
                    status, res = actions.audio_set(body.get("key"),
                                                    body.get("value"))
                elif path == "/api/graph/connect":
                    status, res = actions.graph_connect(body.get("src"),
                                                        body.get("dst"))
                elif path == "/api/graph/disconnect":
                    status, res = actions.graph_disconnect(body.get("src"),
                                                           body.get("dst"))
                elif path == "/api/graph/reset":
                    status, res = actions.graph_reset()
                elif path == "/api/wifi/scan":
                    status, res = actions.wifi_scan()
                elif path == "/api/wifi/join":
                    status, res = actions.wifi_join(body.get("ssid"),
                                                    body.get("password"))
                elif path == "/api/wifi/forget":
                    status, res = actions.wifi_forget(body.get("ssid"))
                elif path == "/api/wifi/disconnect":
                    status, res = actions.wifi_disconnect()
                elif path == "/api/wifi/radio":
                    status, res = actions.wifi_radio(body.get("on"))
                elif path == "/api/neptr/slot":
                    status, res = actions.neptr_slot(body.get("menu"),
                                                     body.get("row"))
                elif path == "/api/neptr/hold":
                    status, res = actions.neptr_hold(body.get("menu"),
                                                     body.get("row"),
                                                     body.get("down"))
                elif path == "/api/neptr/menu":
                    status, res = actions.neptr_menu(body.get("dir"))
                elif path == "/api/neptr/param":
                    status, res = actions.neptr_param(body.get("code"),
                                                      body.get("param"),
                                                      body.get("value"))
                elif path == "/api/neptr/step":
                    status, res = actions.neptr_step(body.get("code"),
                                                     body.get("param"),
                                                     body.get("dir"))
                elif path == "/api/neptr/expr":
                    status, res = actions.neptr_expr(body.get("value"))
                elif path == "/api/neptr/sync":
                    status, res = actions.neptr_sync()
                elif path == "/api/neptr/sim/cc":
                    status, res = actions.neptr_sim_cc(body.get("ch"),
                                                       body.get("cc"),
                                                       body.get("val"))
                elif path == "/api/video/param":
                    status, res = actions.video_param(body.get("id"),
                                                      body.get("value"))
                elif path == "/api/video/select":
                    status, res = actions.video_select(body.get("what"),
                                                       body.get("index"))
                elif path == "/api/neptr/sim/sensor":
                    status, res = actions.neptr_sim_sensor(body.get("id"),
                                                            body.get("raw"))
                else:
                    self._json({"error": "not found"}, 404)
                    return
            except Exception as e:
                self._json({"error": str(e)}, 500)
                return
            self._json(res, status)

        # ---- PUT: the one write path for ~/demiurge/sensors.json ----
        def do_PUT(self):
            path = self.path.split("?", 1)[0]
            if path == "/api/neptr/menus":
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                    body = json.loads(self.rfile.read(n) or b"{}")
                except Exception:
                    self._json({"ok": False, "error": "bad json"}, 400)
                    return
                status, res = menus_save(body)
                self._json(res, status)
                return
            if path == "/api/neptr/sm/heroes":
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                    body = json.loads(self.rfile.read(n) or b"{}")
                except Exception:
                    self._json({"ok": False, "error": "bad json"}, 400)
                    return
                status, res = sm.heroes_save(body)
                self._json(res, status)
                return
            if path != "/api/neptr/sensors":
                self._json({"error": "not found"}, 404)
                return
            if sensors is None:
                self._json({"ok": False, "error": "sensors unavailable"}, 503)
                return
            try:
                n = int(self.headers.get("Content-Length") or 0)
                body = json.loads(self.rfile.read(n) or b"{}")
            except Exception:
                self._json({"ok": False, "error": "bad json"}, 400)
                return
            try:
                targets = sensors.expr_targets()
            except Exception:
                targets = []
            valid_ids = {t["id"] for t in targets} if targets else None
            err = validate_sensors_config(body, valid_expr_ids=valid_ids)
            if err:
                self._json({"ok": False, "error": err}, 400)
                return
            try:
                sensors.write_config(body)
            except Exception as e:
                self._json({"ok": False, "error": str(e)}, 500)
                return
            self._json({"ok": True})

    srv = ThreadingHTTPServer(("", port), Handler)
    srv.daemon_threads = True
    return srv


# ───────────────────────── actions ─────────────────────────

POWER_HELPER = os.environ.get("DEMIURGE_POWER_CMD", "demiurge-power")
ACTION_LOG = HOME / ".demiurge" / "logs" / "web-actions.log"


class Actions:
    """Control actions: patch / power / audio. All serialized by one lock,
    every call logged, each returns (http_status, result_dict)."""

    def __init__(self, runner, sets_provider, log_path, on_change=None,
                 engine=None, sim=None, video=None):
        self.runner = runner
        self.sets_provider = sets_provider
        self.log_path = Path(log_path)
        self.on_change = on_change
        self.engine = engine
        self.sim = sim
        self.video = video
        self.lock = threading.Lock()

    def _log(self, line):
        try:
            self.log_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.log_path, "a") as fh:
                fh.write("%s %s\n"
                         % (time.strftime("%Y-%m-%dT%H:%M:%S"), line))
        except Exception:
            pass

    def _run(self, cmd, stdin=None):
        with self.lock:
            rc, output = self.runner(cmd, stdin) if stdin is not None \
                else self.runner(cmd)
        # cmd_s is what gets LOGGED. Secrets never travel in argv (the wifi
        # password goes down `stdin`), so logging the command line in full is
        # safe by construction rather than by remembering to redact.
        cmd_s = " ".join(cmd)
        self._log("%s rc=%s" % (cmd_s, rc))
        if self.on_change is not None:
            try:
                self.on_change()
            except Exception:
                pass
        ok = (rc == 0)
        return ((200 if ok else 500),
                {"ok": ok, "cmd": cmd_s, "output": output or ""})

    def _reject(self, msg):
        self._log("REJECTED %s" % msg)
        return 400, {"ok": False, "cmd": "", "output": msg}

    def patch(self, name):
        try:
            names = self.sets_provider() or []
        except Exception:
            names = []
        if not name or name not in names:
            return self._reject("unknown patch: %r" % (name,))
        return self._run(["demiurge-set", name])

    def power(self, level):
        if level not in ("low", "med", "high"):
            return self._reject("bad power level: %r" % (level,))
        return self._run([POWER_HELPER, level])

    def interface(self, name):
        if (not isinstance(name, str) or not name.strip()
                or len(name) > 64):
            return self._reject("bad interface name: %r" % (name,))
        return self._run(["demiurge-interface"] + name.split())

    def sync(self, role):
        if role not in SYNC_ROLES:
            return self._reject("bad sync role: %r" % (role,))
        return self._run(["demiurge-sync", role])

    def touch(self, verb):
        # ONE write path: demiurge-touch owns the live.conf `touch =` key
        # (same doctrine as demiurge-interface owning interface.conf).
        if verb not in ("on", "off"):
            return self._reject("bad touch verb: %r" % (verb,))
        return self._run(["demiurge-touch", verb])

    def audio(self, verb):
        if verb not in ("start", "stop", "restart"):
            return self._reject("bad audio verb: %r" % (verb,))
        return self._run(["sudo", "systemctl", verb, "demiurge"])

    # ---- engine settings: ONE write path, demiurge-audio owns live.conf ----

    def audio_set(self, key, value):
        if key not in AUDIO_KEYS:
            return self._reject("bad audio key: %r" % (key,))
        # Values are re-validated by the CLI (it owns the legal set — notably
        # bits rejects 24, which csound's realtime ALSA cannot do). This check
        # only keeps shell-shaped junk out of argv.
        if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9]{1,10}",
                                                          value):
            return self._reject("bad audio value: %r" % (value,))
        return self._run(["demiurge-audio", key, value])

    # ---- graph: ONE write path, demiurge-graph owns pw-link ----
    #
    # No UI anywhere calls pw-link. The CLI re-validates both port names
    # against the live graph (so a stale page can't wire a port that has gone
    # away) and enforces direction; this check only keeps shell-shaped junk
    # out of argv. Confirmation is the UI's job — disconnect takes sound away.

    PORT_RE = re.compile(r"[A-Za-z0-9._:+-]{1,200}")

    def _port(self, name):
        return (isinstance(name, str) and ":" in name
                and self.PORT_RE.fullmatch(name) is not None)

    def graph_connect(self, src, dst):
        if not self._port(src) or not self._port(dst):
            return self._reject("bad port names: %r -> %r" % (src, dst))
        return self._run(["demiurge-graph", "connect", src, dst])

    def graph_disconnect(self, src, dst):
        if not self._port(src) or not self._port(dst):
            return self._reject("bad port names: %r -> %r" % (src, dst))
        return self._run(["demiurge-graph", "disconnect", src, dst])

    def graph_reset(self):
        return self._run(["demiurge-graph", "reset"])

    # ---- wifi ----

    def wifi_scan(self):
        return self._run(["demiurge-wifi", "--scan"])

    def wifi_join(self, ssid, password):
        if not isinstance(ssid, str) or not ssid.strip() or len(ssid) > 32:
            return self._reject("bad ssid: %r" % (ssid,))
        cmd = ["demiurge-wifi", "join", ssid]
        if password:
            if not isinstance(password, str) or not (8 <= len(password) <= 63):
                # Never echo the password itself into the reject message/log.
                return self._reject("password must be 8-63 characters")
            cmd.append("--password-stdin")
            # The secret is handed to the CLI on stdin. It is not in argv (so
            # not in `ps`), not in this server's action log, and not in any
            # status frame. It IS sent over plain HTTP from the browser — the
            # web UI is a LAN tool with no TLS, and the page says so.
            return self._run(cmd, stdin=password + "\n")
        return self._run(cmd)

    def wifi_forget(self, ssid):
        if not isinstance(ssid, str) or not ssid.strip() or len(ssid) > 32:
            return self._reject("bad ssid: %r" % (ssid,))
        return self._run(["demiurge-wifi", "forget", ssid])

    def wifi_disconnect(self):
        return self._run(["demiurge-wifi", "disconnect"])

    def wifi_radio(self, on):
        return self._run(["demiurge-wifi", "radio", "on" if on else "off"])

    # ---- NEPTR: ONE write path, the physical CCs the Teensy emits --------
    #
    # There is no CLI to route this through — the engine's control surface IS
    # the ch1 CC protocol, and this server is simply a third emitter on the
    # bus the Teensy and the touch UI already share. What it does NOT do is
    # invent an alternative: no OSC to the engine, no live.conf pokes, no
    # local prediction of the resulting state. Every number it can send is
    # enumerated in neptr.ALLOWED_PHYS_CC and every request is resolved
    # through the generated model, so a malformed body cannot address a
    # control that the panel does not also address.

    def _engine_ok(self):
        return self.engine is not None

    def _emit(self, what, sent):
        self._log("neptr %s sent=%s" % (what, bool(sent)))
        if not sent:
            return 503, {"ok": False, "cmd": "neptr " + what,
                         "output": self.engine.out.detail}
        return 200, {"ok": True, "cmd": "neptr " + what, "output": ""}

    def _slot(self, menu, row):
        if not isinstance(menu, int) or not isinstance(row, int):
            return None
        if not (0 <= menu < len(neptr.MENUS)) or not (0 <= row < 5):
            return None
        return neptr.slot_button_cc(menu, row)

    def neptr_slot(self, menu, row):
        """Tap a slot button. The engine's /xxx_state echo is what lights the
        slot — this call never touches local state."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        cc = self._slot(menu, row)
        if cc is None:
            return self._reject("bad slot: menu=%r row=%r" % (menu, row))
        return self._emit("slot %s/%s cc%d" % (menu, row, cc),
                          self.engine.out.tap(cc))

    def neptr_hold(self, menu, row, down):
        """Looper rows are HELD buttons, not taps: finger-down -> press CC,
        finger-up -> release CC, so the engine's own gesture classifier (tap /
        double-tap clear / 0.4 s hold overdub) sees true timing."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        cc = self._slot(menu, row)
        if cc is None or not isinstance(down, bool):
            return self._reject("bad hold: menu=%r row=%r down=%r"
                                % (menu, row, down))
        sent = (self.engine.out.press(cc) if down
                else self.engine.out.release(cc))
        return self._emit("hold %s/%s %s" % (menu, row,
                                             "down" if down else "up"), sent)

    def neptr_menu(self, direction):
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        if direction not in ("prev", "next"):
            return self._reject("bad menu direction: %r" % (direction,))
        cc = (neptr.MENU_PREV_CC if direction == "prev"
              else neptr.MENU_NEXT_CC)
        return self._emit("menu " + direction, self.engine.out.tap(cc))

    def _param(self, code, param):
        """(spec, mapping) for a param, or (None, reason)."""
        if not isinstance(code, str) or not isinstance(param, str):
            return None, "bad param names"
        spec = None
        for entry in neptr.EFFECT_PARAMS.get(code, []):
            if entry[0] == param:
                spec = entry
        if spec is None:
            return None, "unknown param: %s.%s" % (code, param)
        mapping = neptr.param_rot_cc(code, param)
        if mapping is None:
            # Real state of the world, not an error: some params simply have
            # no encoder in the engine (the UI shows them as unavailable).
            return None, "no encoder path for %s.%s" % (code, param)
        return (spec, mapping), None

    def _arming(self, toggle_id):
        """(allowed, note). The engine's rotation dispatch is gkLastToggle-
        keyed: a detent edits whichever effect was toggled last, whatever the
        UI thinks it is aiming at. With the OSC feed live we can refuse the
        write outright; without it nobody can know, so the caller is told."""
        st = self.engine.state
        if self.engine.osc_live():
            if st.last_toggle is None:
                return False, "engine has not reported an armed effect yet"
            if st.last_toggle != toggle_id:
                return False, "knobs are armed elsewhere — tap the slot to arm"
            return True, ""
        return True, "arming unknown: the engine state feed is unavailable"

    def neptr_param(self, code, param, value):
        """Set a parameter by closing the loop on the engine's echo."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        got, why = self._param(code, param)
        if got is None:
            return self._reject(why)
        (_name, _label, lo, hi), (phys, toggle_id) = got
        try:
            value = float(value)
        except (TypeError, ValueError):
            return self._reject("bad value: %r" % (value,))
        if not (min(lo, hi) <= value <= max(lo, hi)):
            return self._reject("value %s out of range %s..%s"
                                % (value, lo, hi))
        if not self.engine.osc_live():
            # No echo means no loop; a burst of detents would be an open-loop
            # guess at where the value already is. Refuse and let the browser
            # fall back to single detents, which are honest by construction.
            return 409, {"ok": False, "cmd": "neptr param",
                         "output": "no engine state feed: use single steps"}
        allowed, note = self._arming(toggle_id)
        if not allowed:
            return 409, {"ok": False, "cmd": "neptr param", "output": note}
        self.engine.chase.set_target(code, param, phys, lo, hi, value)
        self._log("neptr param %s.%s -> %s (cc%d)" % (code, param, value,
                                                      phys))
        return 200, {"ok": True, "cmd": "neptr param", "output": note}

    def neptr_step(self, code, param, direction):
        """One encoder detent — literally one click of the physical knob.
        This is the fallback whenever the closed loop is not available, and
        it is exactly as truthful as turning the knob by hand."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        if direction not in ("up", "down"):
            return self._reject("bad step direction: %r" % (direction,))
        got, why = self._param(code, param)
        if got is None:
            return self._reject(why)
        _spec, (phys, toggle_id) = got
        allowed, note = self._arming(toggle_id)
        if not allowed:
            return 409, {"ok": False, "cmd": "neptr step", "output": note}
        sent = self.engine.out.rotate(phys, direction == "up")
        status, res = self._emit("step %s.%s %s" % (code, param, direction),
                                 sent)
        if status == 200:
            res["output"] = note
        return status, res

    def neptr_expr(self, value):
        """Expression pedal: absolute ch1 CC29, exactly what the physical
        pedal sends. No OSC echo exists for it, so the UI's thumb is local —
        the one control on the page that is legitimately unmirrored."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        if not isinstance(value, (int, float)) or isinstance(value, bool) \
                or not (0 <= value <= 127):
            return self._reject("bad expression value: %r" % (value,))
        return self._emit("expr %d" % int(value),
                          self.engine.out.absolute(neptr.EXPR_CC, value))

    def neptr_sync(self):
        """Ask the engine to rebroadcast its full state (ch16 CC102). Only
        useful when this process holds the OSC port, but harmless otherwise —
        the on-device UI simply gets a refresh it can absorb."""
        if not self._engine_ok():
            return self._reject("engine link unavailable")
        return self._emit("sync", self.engine.out.sync_request())

    # ---- virtual hardware panel: the bare-Pi stand-in for the Teensy ------

    def _sim_ok(self):
        return self.sim is not None

    def neptr_sim_cc(self, ch, cc, val):
        """One virtual-panel control event: an OSC /cc, byte-identical to
        what a real encoder click or button press causes on the wire. No
        arming/echo checks here — unlike neptr_param/neptr_step this is not
        chasing a target, it is standing in for the raw hardware event
        itself, exactly one message per click."""
        if not self._sim_ok():
            return self._reject("simulator unavailable")
        nums = (ch, cc, val)
        if any(isinstance(v, bool) or not isinstance(v, int) for v in nums):
            return self._reject("bad sim cc: ch=%r cc=%r val=%r" % nums)
        if not (1 <= ch <= 16) or not (0 <= cc <= 127) or not (0 <= val <= 127):
            return self._reject("sim cc out of range: ch=%d cc=%d val=%d" % nums)
        ok = self.sim.cc(ch, cc, val)
        self._log("sim cc ch=%d cc=%d val=%d sent=%s" % (ch, cc, val, ok))
        return (200 if ok else 500), {"ok": ok, "cmd": "sim cc",
                                      "output": "" if ok else "send failed"}

    def neptr_sim_sensor(self, source_id, raw):
        """One virtual-panel sensor reading, sent RAW (native units) to
        demiurge-io — the same wire format sources/web.py already accepts,
        so this exercises the real conditioning + mapping pipeline rather
        than bypassing it."""
        if not self._sim_ok():
            return self._reject("simulator unavailable")
        if source_id not in SIM_SENSOR_IDS:
            return self._reject("unknown sensor id: %r" % (source_id,))
        if not isinstance(raw, (int, float)) or isinstance(raw, bool):
            return self._reject("bad raw value: %r" % (raw,))
        ok = self.sim.sensor(source_id, raw)
        self._log("sim sensor %s=%s sent=%s" % (source_id, raw, ok))
        return (200 if ok else 500), {"ok": ok, "cmd": "sim sensor",
                                      "output": "" if ok else "send failed"}

    def video_param(self, param_id, value):
        """One direct parameter write to the video engine.

        Ids are opaque here on purpose: the video engine owns its own parameter
        namespace, and mirroring that list into this server would be a second
        copy to keep in sync. We validate SHAPE (non-empty ascii id, numeric
        value) and let the engine reject names it does not know."""
        if self.video is None:
            return self._reject("video output unavailable")
        if not isinstance(param_id, str) or not param_id.strip():
            return self._reject("bad param id: %r" % (param_id,))
        if len(param_id) > 64 or not param_id.isascii():
            return self._reject("bad param id: %r" % (param_id,))
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return self._reject("bad value: %r" % (value,))
        ok = self.video.param(param_id, value)
        self._log("video param %s=%s sent=%s" % (param_id, value, ok))
        return (200 if ok else 500), {"ok": ok, "cmd": "video param",
                                      "output": "" if ok else "send failed"}

    # The three discrete verbs. Whitelisted rather than pasted into the
    # address, because `what` arrives from a browser and an OSC address is
    # otherwise free text — "../" in an address is not a path, but a verb this
    # server does not recognise has no business reaching the engine either.
    VIDEO_SELECT = ("preset", "mode", "kernel")

    def video_select(self, what, index):
        """One discrete selection on the video engine: preset / mode / kernel.

        The engine wraps the kernel index itself and ignores a preset slot it
        has no file for, so this validates shape and hands the rest over —
        same division of labour as video_param: the engine owns its own
        namespace and mirroring its bank into this server would be a second
        copy to keep in sync."""
        if self.video is None:
            return self._reject("video output unavailable")
        if what not in self.VIDEO_SELECT:
            return self._reject("bad selection: %r" % (what,))
        if isinstance(index, bool) or not isinstance(index, int):
            return self._reject("bad index: %r" % (index,))
        ok = self.video.select(what, index)
        self._log("video %s=%s sent=%s" % (what, index, ok))
        return (200 if ok else 500), {"ok": ok, "cmd": "video " + what,
                                      "output": "" if ok else "send failed"}


class Engine:
    """The NEPTR link as one object: OSC in, MIDI out, and the fader loop
    between them. `osc_live()` is the single place the rest of the server asks
    whether engine state is real."""

    def __init__(self, on_change=None, osc_port=ENGINE_OSC_PORT):
        self.state = NeptrState(on_change=on_change)
        self.inp = EngineIn(self.state, port=osc_port)
        self.out = EngineOut()
        self.chase = ParamChase(self.out, self.state)

    def start(self):
        self.inp.start()
        return self

    def osc_live(self):
        return self.inp.status == "live"

    def snapshot(self):
        snap = self.state.snapshot()
        snap["link"] = self.inp.info()
        snap["midi"] = self.out.info()
        snap["live"] = self.osc_live()
        return snap


def make_actions(probes, app, mock=False, engine=None, sim=None, video=None):
    def runner(cmd, stdin=None):
        if mock:
            return 0, "(mock) would run: " + " ".join(cmd)
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=60,
                               input=stdin)
            return r.returncode, (r.stdout or "") + (r.stderr or "")
        except Exception as e:
            return 1, str(e)

    def sets_provider():
        files = probes.list_sets()
        return [s["set"] for s in parse_sets(files)] if files else []

    def on_change():
        # action succeeded or failed — either way, ship a fresh frame now
        app.publish(collect(probes, prev=None))

    return Actions(runner, sets_provider, ACTION_LOG, on_change=on_change,
                   engine=engine, sim=sim, video=video)


# ───────────────────────── main ─────────────────────────

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--mock", metavar="FIXTURE_DIR",
                    help="serve canned probe outputs from this dir (Mac dev)")
    ap.add_argument("--once", action="store_true",
                    help="print one frame JSON and exit")
    ap.add_argument("--interval", type=float, default=2.0)
    ap.add_argument("--status-path",
                    default=str(HOME / ".demiurge" / "status"))
    args = ap.parse_args(argv)

    probes = MockProbes(args.mock) if args.mock else Probes()

    if args.once:
        print(json.dumps(public_frame(collect(probes, prev=None)), indent=2))
        return 0

    app = App()

    # Engine state churns at k-rate during a sweep; the browser only needs to
    # look right. Coalesce to NEPTR_PUSH_HZ: a change arms a one-shot timer,
    # further changes inside the window ride the same push.
    pending = {"timer": None}
    pending_lock = threading.Lock()

    def flush_neptr():
        with pending_lock:
            pending["timer"] = None
        app.publish_neptr(engine.snapshot())

    def on_engine_change():
        with pending_lock:
            if pending["timer"] is not None:
                return
            t = threading.Timer(1.0 / NEPTR_PUSH_HZ, flush_neptr)
            t.daemon = True
            pending["timer"] = t
        t.start()

    engine = Engine(on_change=on_engine_change).start()
    sim = SimOut()
    video = VideoOut()
    actions = make_actions(probes, app, mock=bool(args.mock), engine=engine,
                           sim=sim, video=video)

    # Sensor values arrive from demiurge-io at ~20 Hz per active sensor id,
    # potentially several ids at once during calibration — same coalescing
    # story as the engine channel above, just faster (SENSOR_PUSH_HZ), and
    # keyed to sensors.state instead of engine.snapshot().
    spending = {"timer": None}
    spending_lock = threading.Lock()

    def flush_sensors():
        with spending_lock:
            spending["timer"] = None
        app.publish_sensors(sensors.state.snapshot())

    def on_sensor_change():
        with spending_lock:
            if spending["timer"] is not None:
                return
            t = threading.Timer(1.0 / SENSOR_PUSH_HZ, flush_sensors)
            t.daemon = True
            spending["timer"] = t
        t.start()

    pool = PoolLink()
    sensors = SensorLink(on_change=on_sensor_change,
                         on_param=pool.note).start()
    pool.sensors = sensors

    def loop():
        prev = None
        while True:
            try:
                frame = collect(probes, prev)
                write_status(frame, args.status_path)
                app.publish(frame)
                prev = frame
            except Exception as e:
                print("collector error:", e, flush=True)
            time.sleep(args.interval)

    threading.Thread(target=loop, daemon=True).start()

    sm = default_sm().start()
    srv = make_server(app, args.port, Path(__file__).parent / "static",
                      actions=actions, engine=engine, sensors=sensors, sm=sm,
                      pool=pool)
    print("demiurge-web listening on :%d%s ; engine osc %s"
          % (args.port, " (mock)" if args.mock else "",
             engine.inp.status), flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
