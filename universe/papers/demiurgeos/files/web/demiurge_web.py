#!/usr/bin/env python3
"""demiurge-web: status daemon + browser mirror of the demiurge companion TUI.
Stdlib only. Layers: parsers (pure) / collector (2s thread) / HTTP server."""
import json, os, re, glob, time, queue, argparse, threading, subprocess
from collections import deque
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

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
    """`demiurge-interface --list` machine lines: name|card_id|flags
    (flags = comma-joined subset of active,chosen,inuse). Skip malformed."""
    out = []
    for line in (text or "").splitlines():
        parts = line.split("|")
        if len(parts) != 3:
            continue
        name, card_id, flags = (p.strip() for p in parts)
        if not name:
            continue
        fl = {f.strip() for f in flags.split(",")}
        out.append({"name": name, "id": card_id,
                    "active": "active" in fl,
                    "chosen": "chosen" in fl,
                    "inuse": "inuse" in fl})
    return out


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

HOME = Path(os.environ.get("DEMIURGE_HOME", str(Path.home())))
LIVE_CONF = HOME / "demiurge" / "live.conf"
SETS_DIR = HOME / "demiurge" / "sets"
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
            return {os.path.basename(f): Path(f).read_text()
                    for f in sorted(glob.glob(str(SETS_DIR / "*.conf")))}
        except Exception:
            return None

    def live_conf(self):
        return self.read(LIVE_CONF)

    def interfaces(self):
        # missing binary / nonzero exit / timeout -> None -> empty list
        return self.run(["demiurge-interface", "--list"])


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


def make_server(app, port, static_dir, actions=None):
    static_root = Path(static_dir).resolve()

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

        # ---- POST (actions wired in Task 4) ----
        def do_POST(self):
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
                else:
                    self._json({"error": "not found"}, 404)
                    return
            except Exception as e:
                self._json({"error": str(e)}, 500)
                return
            self._json(res, status)

    srv = ThreadingHTTPServer(("", port), Handler)
    srv.daemon_threads = True
    return srv


# ───────────────────────── actions ─────────────────────────

POWER_HELPER = os.environ.get("DEMIURGE_POWER_CMD", "demiurge-power")
ACTION_LOG = HOME / ".demiurge" / "logs" / "web-actions.log"


class Actions:
    """Control actions: patch / power / audio. All serialized by one lock,
    every call logged, each returns (http_status, result_dict)."""

    def __init__(self, runner, sets_provider, log_path, on_change=None):
        self.runner = runner
        self.sets_provider = sets_provider
        self.log_path = Path(log_path)
        self.on_change = on_change
        self.lock = threading.Lock()

    def _log(self, line):
        try:
            self.log_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.log_path, "a") as fh:
                fh.write("%s %s\n"
                         % (time.strftime("%Y-%m-%dT%H:%M:%S"), line))
        except Exception:
            pass

    def _run(self, cmd):
        with self.lock:
            rc, output = self.runner(cmd)
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

    def audio(self, verb):
        if verb not in ("start", "stop", "restart"):
            return self._reject("bad audio verb: %r" % (verb,))
        return self._run(["sudo", "systemctl", verb, "demiurge"])


def make_actions(probes, app, mock=False):
    def runner(cmd):
        if mock:
            return 0, "(mock) would run: " + " ".join(cmd)
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            return r.returncode, (r.stdout or "") + (r.stderr or "")
        except Exception as e:
            return 1, str(e)

    def sets_provider():
        files = probes.list_sets()
        return [s["set"] for s in parse_sets(files)] if files else []

    def on_change():
        # action succeeded or failed — either way, ship a fresh frame now
        app.publish(collect(probes, prev=None))

    return Actions(runner, sets_provider, ACTION_LOG, on_change=on_change)


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
    actions = make_actions(probes, app, mock=bool(args.mock))

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

    srv = make_server(app, args.port, Path(__file__).parent / "static",
                      actions=actions)
    print("demiurge-web listening on :%d%s"
          % (args.port, " (mock)" if args.mock else ""), flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
