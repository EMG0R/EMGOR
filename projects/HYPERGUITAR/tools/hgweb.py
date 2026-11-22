#!/usr/bin/env python3
"""
hgweb.py -- browser dashboard for the hyperGuitar Bela rig.

Bridges the Bela's plain-text UDP telemetry to a web page, and sends HGSET
control datagrams back.  Python 3 stdlib only.

    telemetry  Bela --UDP--> this host :9000   (HGMON lines, ~20 Hz)
    control    this host --UDP--> 192.168.7.2:9001   ("HGSET <name> <float>")

The page shows the 4x4 NeoTrellis grid in the SAME physical orientation as
the hardware (menu row on top, FX block rotated 180 degrees: slot = 15 - pad),
labelled with the real preset names parsed out of presets.cpp at startup.

Run:
    ./hgweb.py                      # listen 0.0.0.0:9000, serve :8080
    ./hgweb.py --loopback           # talk to hgweb_fake.py on 127.0.0.1

Design rules:
  * MISSING IS NOT ZERO.  A field absent from the packet stays None and
    renders as a dim dash, never as 0.00 or as "off".
  * STALE IS LOUD.  No packet for >1.5 s and the whole page greys out under
    a banner.  A frozen display that looks live is worse than no display.
  * The preset table is parsed, never retyped.  If the parse fails the
    server refuses to start rather than serve plausible-looking wrong names.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_PRESETS = os.path.normpath(
    os.path.join(HERE, "..", "bela", "hyperGuitar6chPan", "presets.cpp"))
DEFAULT_HEADER = os.path.normpath(
    os.path.join(HERE, "..", "bela", "hyperGuitar6chPan", "hyperguitar.h"))

DEFAULT_UDP_HOST = "0.0.0.0"
DEFAULT_UDP_PORT = 9000
DEFAULT_BELA = "192.168.7.2"
DEFAULT_BELA_PORT = 9001
# 8080 is a crowded port on a dev machine -- and a dual-stack listener on
# *:8080 can steal connections aimed at 127.0.0.1:8080, so the page silently
# becomes someone else's site.  Pick somewhere quiet by default.
DEFAULT_HTTP_PORT = 8747

STALE_AFTER = 1.5          # seconds with no packet before the page screams
SLOTS_PER_CAT = 12
NUM_CATEGORIES = 4

VOL_NAMES = ["sub0", "sub1", "sub2", "sub3", "sub4", "sub5",
             "piezo", "trs", "outA", "outB"]
VOL_MIN, VOL_MAX = 0.0, 4.0
# Per-slider ranges (2026-09-25, owner): TRS and contact mics are far hotter
# than the hex strings, so their sliders span 1% of the old 0-4 range, and the
# strings 10%. The outputs keep 0-4. The firmware clamps to 0-4 regardless.
VOL_RANGE = {"sub0": 0.4, "sub1": 0.4, "sub2": 0.4, "sub3": 0.4, "sub4": 0.4,
             "sub5": 0.4, "piezo": 0.04, "trs": 0.04, "outA": 4.0, "outB": 4.0}

# 2026-11-01 boost: wet makeup gain in dB (firmware: gWetBoostPitchDb / gWetBoostFormantDb,
# HGSET boost_pitch_db / boost_formant_db, saved in hyperguitar-mix.conf, echoed as bp:/bf:)
BOOST_NAMES = ["boost_pitch_db", "boost_formant_db"]
BOOST_MIN, BOOST_MAX = -6.0, 12.0

# 2026-11-05 synth sustain limiter (firmware: gSynSustain, HGSET syn_sustain, 0..1,
# 0 = synth follows the string exactly, saved in hyperguitar-mix.conf, echoed as ss:)
# 2026-11-05 synth filter mod (firmware: gSynFmod, HGSET syn_fmod, 0..1 = 0..4 octaves of level->cutoff
# for every patch; also the pot while the SYNTH pad is armed; saved in hyperguitar-mix.conf, echoed as fm:)
SYN_NAMES = ["syn_sustain", "syn_fmod"]

# 2026-11-01 pwet: persisted PITCH wet per interval stop (HGSET pwet_*, 0..1, echoed as pw:a,b,c,d,e,f)
PWET_NAMES = ["pwet_m12", "pwet_m7", "pwet_m5", "pwet_p5", "pwet_p7", "pwet_p12"]
PWET_LABELS = ["-12", "-7", "-5", "+5", "+7", "+12"]

IN_LABELS = [
    ("TRS tip", "guitar"), ("TRS ring", "guitar"),
    ("Contact A", "piezo"), ("Contact B", "piezo"),
    ("Hex 1", "hex"), ("Hex 2", "hex"), ("Hex 3", "hex"),
    ("Hex 4", "hex"), ("Hex 5", "hex"), ("Hex 6", "hex"),
]
OUT_LABELS = [("Out A", "out"), ("Out B", "out")]


# ---------------------------------------------------------------------------
# preset table parsing -- presets.cpp is the single source of truth
# ---------------------------------------------------------------------------

class PresetParseError(RuntimeError):
    pass


def parse_render_info(render_path: str):
    """Pull OPEN_HZ, STRING_CHANNEL and SYNTH_NAMES out of render.cpp so the
    dashboard's string panel can never disagree with the firmware. Returns
    None if the file is missing or unparseable - the panel then says so
    instead of inventing numbers."""
    try:
        src = open(render_path, encoding="utf-8").read()
    except OSError:
        return None
    def arr(name):
        m = re.search(name + r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", src)
        return m.group(1) if m else None
    hz, ch, syn = arr("OPEN_HZ"), arr("STRING_CHANNEL"), arr("SYNTH_NAMES")
    if not hz or not ch:
        return None
    try:
        open_hz = [float(x.strip().rstrip("fF")) for x in hz.split(",") if x.strip()]
        chans = [int(x.strip()) for x in ch.split(",") if x.strip()]
    except ValueError:
        return None
    names = re.findall(r'"([^"]*)"', syn) if syn else []
    return {"open_hz": open_hz, "string_channel": chans, "synth_names": names}


def strip_comments(src: str) -> str:
    """Remove /* */ and // comments without disturbing string literals."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':
            j = i + 1
            while j < n:
                if src[j] == "\\":
                    j += 2
                    continue
                if src[j] == '"':
                    j += 1
                    break
                j += 1
            out.append(src[i:j])
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
            out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out)


def parse_effect_ids(path: str):
    """Parse the '#define PAD_xxx <n>' effect ids out of hyperguitar.h.

    armed: on the wire is one of these numbers (gArmedPad is an EFFECT id,
    not a pad index), so they have to be the real values, not invented ones.
    """
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            src = fh.read()
    except OSError as exc:
        raise PresetParseError(f"cannot read {path}: {exc}") from exc
    ids = {}
    for name, val in re.findall(
            r"^\s*#\s*define\s+(PAD_[A-Z0-9_]+)\s+(-?\d+)\s*(?:/[*/].*)?$",   # 2026-11-01 boost: allow a trailing comment
            src, re.MULTILINE):
        ids[name] = int(val)
    if not ids:
        raise PresetParseError(f"no '#define PAD_x <n>' effect ids in {path}")
    return ids


def _parse_flat_pads(clean: str):
    """2026-11-01 boost: presets.cpp is now ONE flat gPadFx[16] table (the
    category/page model is gone). Adapt it to the 4x12 shape the page still
    draws: pads 0-11 -> page 0, pads 12-15 -> page 1, pages 2-3 empty.
    Returns (names, cats) or None if there is no gPadFx table."""
    m = re.search(r"gPadFx\s*\[[^\]]*\]\s*=\s*\{", clean)
    if not m:
        return None
    body = clean[m.end():]
    row_re = re.compile(
        r"\{\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,"
        r'\s*"((?:[^"\\]|\\.)*)"\s*\}')
    rows = row_re.findall(body)[:16]
    if len(rows) != 16:
        return None

    def num(tok):
        try:
            return float(tok.strip().rstrip("fF"))
        except ValueError:
            return tok.strip()
    recs = [{"effect": e, "p1": num(a), "p2": num(b), "label": l.strip()}
            for (e, a, b, l) in rows]
    empty = {"effect": "FX_NONE", "p1": 0.0, "p2": 0.0, "label": ""}
    pages = [recs[0:12], recs[12:16] + [dict(empty) for _ in range(8)],
             [dict(empty) for _ in range(12)], [dict(empty) for _ in range(12)]]
    return ["PADS 0-11", "PADS 12-15", "-", "-"], pages


def parse_presets(path: str):
    """Parse gCatSlot[][] out of presets.cpp.

    Returns (categories, slots) where categories is a list of 4 names and
    slots is a 4x12 list of dicts {effect, p1, p2, label} (label may be "").
    Raises PresetParseError with a specific reason on any doubt.
    """
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            src = fh.read()
    except OSError as exc:
        raise PresetParseError(f"cannot read {path}: {exc}") from exc

    clean = strip_comments(src)

    m = re.search(r"gCatSlot\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{", clean)
    if not m:
        flat = _parse_flat_pads(clean)      # 2026-11-01 boost: v8 flat-grid presets.cpp
        if flat is not None:
            return flat
        raise PresetParseError(
            f"no 'gCatSlot[...][...] = {{' initialiser found in {path}")

    # walk braces from the opening one to its match
    start = m.end() - 1
    depth, i, n = 0, start, len(clean)
    end = -1
    while i < n:
        if clean[i] == "{":
            depth += 1
        elif clean[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
        i += 1
    if end < 0:
        raise PresetParseError("unbalanced braces in the gCatSlot initialiser")

    body = clean[start + 1:end]

    # split into category groups at depth 1
    groups, depth, cur = [], 0, []
    for ch in body:
        if ch == "{":
            depth += 1
            if depth == 1:
                cur = []
                continue
        elif ch == "}":
            depth -= 1
            if depth == 0:
                groups.append("".join(cur))
                continue
        if depth >= 1:
            cur.append(ch)
    if len(groups) != NUM_CATEGORIES:
        raise PresetParseError(
            f"expected {NUM_CATEGORIES} category groups, found {len(groups)}")

    row_re = re.compile(
        r"\{\s*([A-Za-z_][A-Za-z0-9_]*)\s*,"        # effect id
        r"\s*([^,]+?)\s*,"                          # p1
        r"\s*([^,]+?)\s*,"                          # p2
        r'\s*"((?:[^"\\]|\\.)*)"\s*\}')             # label

    def num(tok: str):
        tok = tok.strip().rstrip("fF")
        try:
            return float(tok)
        except ValueError:
            return tok.strip()

    cats = []
    for ci, g in enumerate(groups):
        rows = row_re.findall(g)
        if len(rows) != SLOTS_PER_CAT:
            raise PresetParseError(
                f"category {ci}: expected {SLOTS_PER_CAT} slot rows, "
                f"found {len(rows)}")
        cats.append([
            {"effect": eff, "p1": num(p1), "p2": num(p2), "label": lab.strip()}
            for (eff, p1, p2, lab) in rows])

    # category names
    names = None
    nm = re.search(r"gCatName\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", clean)
    if nm:
        found = re.findall(r'"((?:[^"\\]|\\.)*)"', nm.group(1))
        if len(found) == NUM_CATEGORIES:
            names = found
    if names is None:
        raise PresetParseError(
            "could not parse gCatName[] (expected 4 quoted names)")

    return names, cats


# ---------------------------------------------------------------------------
# telemetry
# ---------------------------------------------------------------------------

def _floats(tok: str):
    out = []
    for part in tok.split(","):
        part = part.strip()
        if not part:
            return None
        try:
            out.append(float(part))
        except ValueError:
            return None
    return out


def _ints(tok: str):
    vals = _floats(tok)
    return None if vals is None else [int(v) for v in vals]


def parse_hgmon(line: str):
    """Parse one HGMON line into a dict of present fields.

    Absent fields are simply absent from the result (never zero-filled).
    Returns None if this is not an HGMON line at all.
    """
    line = line.strip()
    if not line.startswith("HGMON"):
        return None
    out = {}
    for tok in line.split()[1:]:
        if ":" not in tok:
            continue
        key, _, val = tok.partition(":")
        if not val:
            continue
        if key in ("in", "out", "acc", "gyr", "tof", "vol"):
            vals = _floats(val)
            if vals is None:
                continue
            out[key] = vals[0] if key == "tof" and len(vals) == 1 else vals
        elif key in ("menu", "armed"):
            vals = _ints(val)
            if vals:
                out[key] = vals[0]
        elif key in ("slots", "fx"):
            vals = _ints(val)
            if vals is not None:
                # per-slot LED state: bit 0 = lit (live voicing),
                # bit 1 = armed. Raw ints; the page decodes the bits.
                out[key] = [int(v) for v in vals]
        else:
            vals = _floats(val)
            if vals is not None:
                out.setdefault("extra", {})[key] = (
                    vals[0] if len(vals) == 1 else vals)
    return out


class Telemetry:
    """Holds the most recent packet plus link statistics. Thread-safe."""

    def __init__(self):
        self.lock = threading.Lock()
        self.fields = {}
        self.last_rx = 0.0        # monotonic time of last good packet
        self.packets = 0
        self.bad = 0
        self.rate = 0.0
        self.bind_error = None
        self.last_set = None
        self._window = []         # recent arrival times, for pkt/s

    def ingest(self, line: str):
        parsed = parse_hgmon(line)
        now = time.monotonic()
        with self.lock:
            # A line that is not HGMON at all, or that is HGMON-shaped but
            # whose every field failed to parse, is DISCARDED rather than
            # installed: overwriting good state with an empty dict would
            # blank the display for one frame on a single corrupt packet.
            if parsed is None or not parsed:
                self.bad += 1
                return
            # 2026-11-01 boost: flat-grid firmware sends fx: (16 pad LED states)
            # and no menu:/slots:. Present pads 0-11 as page 0 so the pad grid
            # and its missing-field warnings work again (pads 12-15 not shown).
            if "fx" in parsed and "slots" not in parsed:
                parsed["menu"] = 0
                parsed["slots"] = parsed["fx"][:12]
            self.fields = parsed
            self.last_rx = now
            self.packets += 1
            self._window.append(now)
            cutoff = now - 2.0
            while self._window and self._window[0] < cutoff:
                self._window.pop(0)
            span = (self._window[-1] - self._window[0]) if len(self._window) > 1 else 0.0
            self.rate = (len(self._window) - 1) / span if span > 0 else 0.0

    def snapshot(self):
        now = time.monotonic()
        with self.lock:
            age = (now - self.last_rx) if self.last_rx else None
            snap = dict(self.fields)
            snap["_meta"] = {
                "age": age,
                "stale": (age is None) or (age > STALE_AFTER),
                "packets": self.packets,
                "bad": self.bad,
                "rate": round(self.rate, 1),
                "stale_after": STALE_AFTER,
                "bind_error": self.bind_error,
                "last_set": self.last_set,
                "server_time": time.time(),
            }
        return snap


def udp_reader(tel: Telemetry, host: str, port: int, reuseport: bool):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if reuseport and hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    try:
        sock.bind((host, port))
    except OSError as exc:
        msg = (f"UDP bind {host}:{port} FAILED: {exc}. "
               "Another listener (hgmon.py?) already owns the port -- "
               "stop it, or rerun with --reuseport (see the caveat in --help).")
        with tel.lock:
            tel.bind_error = msg
        print("!! " + msg, file=sys.stderr)
        return
    print(f"   telemetry: listening on udp://{host}:{port}")
    buf = b""
    while True:
        try:
            data, _addr = sock.recvfrom(65535)
        except OSError as exc:
            with tel.lock:
                tel.bind_error = f"UDP receive error: {exc}"
            return
        buf += data
        # the wire format is newline terminated; a datagram may carry
        # several lines, or (in theory) a partial one
        while b"\n" in buf:
            line, _, buf = buf.partition(b"\n")
            tel.ingest(line.decode("utf-8", "replace"))
        if buf and len(buf) > 4096:
            buf = b""
        elif buf and buf.startswith(b"HGMON") and b"\n" not in buf:
            # datagram without a trailing newline: treat it as complete
            tel.ingest(buf.decode("utf-8", "replace"))
            buf = b""


class Control:
    def __init__(self, host: str, port: int, tel: Telemetry):
        self.addr = (host, port)
        self.tel = tel
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def set(self, name: str, value: float):
        if name not in VOL_NAMES and name not in BOOST_NAMES and name not in PWET_NAMES and name not in SYN_NAMES and name != "usb":
            raise ValueError(f"unknown control name {name!r}")
        if name in PWET_NAMES or name in SYN_NAMES:   # 2026-11-01 pwet / 2026-11-05 syn_sustain: 0..1
            value = max(0.0, min(1.0, float(value)))
            msg = f"HGSET {name} {value:.3f}\n"
            self.sock.sendto(msg.encode("ascii"), self.addr)
            with self.tel.lock:
                self.tel.last_set = {"name": name, "value": value,
                                     "at": time.time(),
                                     "to": f"{self.addr[0]}:{self.addr[1]}"}
            return msg.strip()
        if name in BOOST_NAMES:           # 2026-11-01 boost: dB, may be negative
            value = max(BOOST_MIN, min(BOOST_MAX, float(value)))
            msg = f"HGSET {name} {value:.2f}\n"
            self.sock.sendto(msg.encode("ascii"), self.addr)
            with self.tel.lock:
                self.tel.last_set = {"name": name, "value": value,
                                     "at": time.time(),
                                     "to": f"{self.addr[0]}:{self.addr[1]}"}
            return msg.strip()
        if name == "usb":                 # on/off switch, not a gain
            value = 1.0 if float(value) > 0.5 else 0.0
        value = max(VOL_MIN, min(VOL_RANGE.get(name, VOL_MAX), float(value)))
        msg = f"HGSET {name} {value:.3f}\n"
        self.sock.sendto(msg.encode("ascii"), self.addr)
        with self.tel.lock:
            self.tel.last_set = {"name": name, "value": value,
                                 "at": time.time(),
                                 "to": f"{self.addr[0]}:{self.addr[1]}"}
        return msg.strip()


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------

class Handler(BaseHTTPRequestHandler):
    server_version = "hgweb/1.0"
    tel: Telemetry = None          # injected
    ctl: Control = None
    page: bytes = b""
    meta: dict = {}

    def log_message(self, fmt, *args):     # quiet; telemetry is the point
        pass

    def _send(self, code, body, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _json(self, obj, code=200):
        self._send(code, json.dumps(obj).encode("utf-8"))

    def do_GET(self):
        u = urlparse(self.path)
        if u.path in ("/", "/index.html"):
            self._send(200, self.page, "text/html; charset=utf-8")
        elif u.path == "/api/state":
            self._json(self.tel.snapshot())
        elif u.path == "/api/meta":
            self._json(self.meta)
        else:
            self._send(404, b'{"error":"not found"}')

    def do_POST(self):
        u = urlparse(self.path)
        if u.path != "/api/set":
            self._send(404, b'{"error":"not found"}')
            return
        try:
            n = int(self.headers.get("Content-Length") or 0)
            payload = json.loads(self.rfile.read(n) or b"{}")
            name = payload["name"]
            value = float(payload["value"])
            sent = self.ctl.set(name, value)
        except Exception as exc:
            self._json({"ok": False, "error": str(exc)}, 400)
            return
        self._json({"ok": True, "sent": sent})


# ---------------------------------------------------------------------------
# page
# ---------------------------------------------------------------------------

PAGE = r"""<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>hyperGuitar</title>
<style>
  :root{
    --bg:#08090c; --panel:#101318; --line:#1e232c; --ink:#dfe6ef;
    --dim:#6d7787; --on:#59c8ff; --onbg:#0d3a55; --armed:#ffcf4d;
    --hot:#ff5f57; --ok:#4fd48a;
    --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--mono);
       font-size:13px;padding:14px 16px 40px;-webkit-font-smoothing:antialiased}
  h2{font-size:11px;letter-spacing:.18em;text-transform:uppercase;
     color:var(--dim);margin:0 0 8px;font-weight:600}
  .wrap{max-width:1180px;margin:0 auto}
  header{display:flex;flex-wrap:wrap;align-items:baseline;gap:14px;margin-bottom:10px}
  header .title{font-size:16px;letter-spacing:.1em}
  header .stat{color:var(--dim);font-size:11px}
  header .stat b{color:var(--ink);font-weight:600}
  #banner{display:none;background:var(--hot);color:#140406;font-weight:700;
    padding:10px 14px;border-radius:6px;margin-bottom:12px;letter-spacing:.04em}
  #banner.show{display:block}
  #bindwarn{display:none;background:#3a2408;color:#ffce7a;border:1px solid #7a5316;
    padding:8px 12px;border-radius:6px;margin-bottom:12px}
  #bindwarn.show{display:block}
  main{display:grid;grid-template-columns:minmax(360px,1.25fr) minmax(300px,1fr);
       gap:16px;align-items:start}
  @media(max-width:860px){main{grid-template-columns:1fr}}
  .panel{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px}
  body.stale main,body.stale .fxmap{opacity:.32;filter:grayscale(.85)}

  /* ---- pad grid ---- */
  #grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
  .pad{position:relative;aspect-ratio:1/1;border:1px solid var(--line);
       border-radius:6px;background:#0b0d11;display:flex;flex-direction:column;
       align-items:center;justify-content:center;text-align:center;padding:4px;
       overflow:hidden}
  .pad .name{font-size:12px;font-weight:700;line-height:1.15;letter-spacing:.02em;
             word-break:break-word}
  .pad .sub{position:absolute;top:3px;left:5px;font-size:9px;color:#39404d}
  .pad .eff{position:absolute;bottom:3px;right:5px;font-size:9px;color:#39404d}
  .pad.empty .name{color:#2c323c;font-weight:400;font-style:italic}
  .pad.on{background:var(--onbg);border-color:var(--on);color:#e8f7ff;
          box-shadow:0 0 0 1px var(--on) inset, 0 0 14px -4px var(--on)}
  .pad.on .sub,.pad.on .eff{color:#7fd4f5}
  .pad.armed{border-color:var(--armed);box-shadow:0 0 0 2px var(--armed) inset;
             animation:breathe 1.6s ease-in-out infinite}
  @keyframes breathe{0%,100%{box-shadow:0 0 0 2px var(--armed) inset}
                     50%{box-shadow:0 0 0 2px var(--armed) inset,0 0 18px -2px var(--armed)}}
  .pad.menu{aspect-ratio:2.4/1;background:#0c1016;border-style:dashed}
  .pad.menu .name{font-size:11px;letter-spacing:.08em;color:var(--dim)}
  .pad.menu.cur{background:#132330;border-color:var(--on);border-style:solid}
  .pad.menu.cur .name{color:var(--on)}
  .gridnote{color:var(--dim);font-size:10px;margin-top:8px;line-height:1.5}

  /* ---- volumes ---- */
  .vrow{display:grid;grid-template-columns:58px 1fr 52px;gap:8px;align-items:center;
        padding:3px 0}
  .vrow label{color:var(--dim);font-size:11px}
  .vrow.grp{border-top:1px solid var(--line);margin-top:6px;padding-top:8px}
  .vrow input[type=range]{width:100%;accent-color:var(--on);height:18px}
  .vrow .val{text-align:right;font-variant-numeric:tabular-nums}
  .vrow .val.drift{color:var(--armed)}

  /* ---- meters ---- */
  .m{display:grid;grid-template-columns:72px 1fr 50px;gap:8px;align-items:center;
     padding:2px 0}
  .m .lbl{color:var(--dim);font-size:11px;white-space:nowrap}
  .m .bar{height:9px;background:#0a0c10;border:1px solid var(--line);border-radius:3px;
          position:relative;overflow:hidden}
  .m .fill{position:absolute;left:0;top:0;bottom:0;width:0;background:var(--ok);
           transition:width .06s linear}
  .m.hex .fill{background:#8ab4ff} .m.piezo .fill{background:#c48aff}
  .m.out .fill{background:var(--on)}
  .m .fill.hot{background:var(--hot)}
  .m .db{text-align:right;color:var(--dim);font-size:11px;font-variant-numeric:tabular-nums}
  .sensors{display:flex;flex-wrap:wrap;gap:18px;color:var(--dim);font-size:11px;
           margin-top:10px;border-top:1px solid var(--line);padding-top:10px}
  .sensors b{color:var(--ink);font-weight:600;font-variant-numeric:tabular-nums}
  .dash{color:#3a4250}
  .nodata{display:none;background:#2a1d08;color:#e0b25f;border:1px solid #6b4d15;
    border-radius:5px;padding:6px 9px;margin-bottom:9px;font-size:11px;line-height:1.5}
  .nodata.show{display:block}
  .unconfirmed .vrow .val{color:#6d7787}

  .usbrow{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:8px}
  .usbrow h2{margin:0}
  .usbbtn{font:600 12px -apple-system,BlinkMacSystemFont,system-ui,sans-serif;cursor:pointer;
    padding:6px 12px;border-radius:6px;border:1px solid var(--line);background:#161a22;color:var(--dim)}
  .usbbtn.on{background:var(--on);border-color:var(--on);color:#04121c}
  .usbbtn:focus-visible{outline:2px solid var(--armed);outline-offset:2px}

  .strrow{display:grid;grid-template-columns:92px 58px 1fr 64px;gap:8px;align-items:center;
          padding:5px 0;border-top:1px solid var(--line);font-size:12px}
  .strrow:first-child{border-top:0}
  .strrow .sname{color:var(--ink);font-weight:600}
  .strrow .sch{color:var(--dim);font-size:11px}
  .strrow .strk{font-variant-numeric:tabular-nums;color:var(--dim)}
  .strrow .strk b{color:var(--ink)}
  .strrow.live .strk b{color:var(--on)}
  .strrow .scf{height:6px;background:#1a1f28;border-radius:3px;overflow:hidden}
  .strrow .scf i{display:block;height:100%;width:0;background:var(--ok)}
  .strrow .sdb{text-align:right;font-variant-numeric:tabular-nums;color:var(--dim)}
  #clipstat.hot b{color:#fff;background:var(--hot);padding:0 5px;border-radius:3px}
  .m.clip .lbl{color:var(--hot)}
  /* ---- effects map: all four pages at once ---- */
  .fxmap{margin-bottom:16px;--lava:#6a3fa0;--lavabg:#1a1024;
         font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",system-ui,sans-serif}
  .fxtop{display:flex;flex-wrap:wrap;align-items:baseline;justify-content:space-between;
         gap:8px 18px;margin-bottom:12px}
  .fxtop h2{margin:0}
  .fxkey{display:flex;flex-wrap:wrap;gap:14px;font-size:12px;color:var(--dim)}
  .fxkey span{display:inline-flex;align-items:center;gap:6px}
  .fxkey i{display:inline-block;width:14px;height:14px;border-radius:4px}
  .fxkey .k-off{background:var(--lavabg);border:1px solid var(--lava)}
  .fxkey .k-on{background:var(--on)}
  .fxkey .k-arm{border:2px solid var(--armed)}
  #fxmap{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}
  @media(max-width:1000px){#fxmap{grid-template-columns:repeat(2,minmax(0,1fr))}}
  @media(max-width:560px){#fxmap{grid-template-columns:1fr}}
  .fxcol{border:1px solid var(--line);border-radius:8px;padding:12px 12px 6px;
         background:#0c0e13;min-width:0}
  .fxcol.cur{border-color:var(--on);box-shadow:0 0 0 1px var(--on) inset;background:#0b1520}
  .fxcolhead{display:flex;align-items:baseline;justify-content:space-between;gap:8px;
             margin-bottom:10px}
  .fxcolhead b{font-size:17px;font-weight:700;letter-spacing:.01em;color:var(--ink)}
  .fxcol.cur .fxcolhead b{color:var(--on)}
  .fxcolhead span{font-size:11px;color:var(--dim);white-space:nowrap}
  .fxcol.cur .fxcolhead span{color:var(--on)}
  .fxgrp{padding:8px 0 10px;border-top:1px solid var(--line)}
  .fxgrp:first-of-type{border-top:0;padding-top:0}
  .fxname{display:flex;align-items:baseline;justify-content:space-between;gap:8px;
          margin-bottom:7px}
  .fxname b{font-size:14px;font-weight:650;color:#c9d2de}
  .fxname em{font-style:normal;font-size:11px;color:var(--dim);white-space:nowrap}
  .fxgrp.live .fxname b{color:#fff}
  .fxgrp.live .fxname em{color:var(--on)}
  .chips{display:flex;flex-wrap:wrap;gap:6px}
  .chip{display:inline-flex;align-items:center;gap:8px;padding:6px 10px 6px 7px;
        border-radius:6px;background:var(--lavabg);border:1px solid #3b2656;
        font-size:13px;color:#d8cdea;line-height:1.1}
  .chip.on{background:var(--on);border-color:var(--on);color:#04121c;font-weight:650}
  .chip.arm{outline:2px solid var(--armed);outline-offset:1px;animation:chipbreathe 2.4s ease-in-out infinite}
  @keyframes chipbreathe{50%{outline-color:rgba(255,207,77,.35)}}
  @media(prefers-reduced-motion:reduce){.chip.arm{animation:none}}
  .chip.unknown{opacity:.55}
  /* where the pad physically sits: 4x4, top row = menu */
  .loc{display:grid;grid-template-columns:repeat(4,5px);grid-auto-rows:5px;gap:2px;flex:none}
  .loc i{border-radius:1.5px;background:rgba(255,255,255,.12)}
  .loc i.m{background:rgba(89,200,255,.55)}
  .loc i.here{background:#fff}
  .chip.on .loc i{background:rgba(4,18,28,.25)}
  .chip.on .loc i.m{background:rgba(4,18,28,.45)}
  .chip.on .loc i.here{background:#04121c}
  .fxnote{font-size:11px;color:var(--dim);margin-top:10px;line-height:1.5}
</style></head><body>
<div class="wrap">
<header>
  <span class="title">hyperGuitar</span>
  <span class="stat">link <b id="rate">--</b> pkt/s</span>
  <span class="stat">pkts <b id="pkts">0</b></span>
  <span class="stat">age <b id="age">--</b></span>
  <span class="stat" id="clipstat" title="input samples at the converter ceiling / output samples squashed by the soft clipper, since the guitar booted">clips <b id="clipn">--</b></span>
  <span class="stat" id="setline"></span>
</header>
<div id="bindwarn"></div>
<div id="banner"></div>
<section class="panel fxmap">
  <div class="fxtop">
    <h2>Effects &mdash; every page</h2>
    <div class="fxkey">
      <span><i class="k-off"></i>available</span>
      <span><i class="k-on"></i>on</span>
      <span><i class="k-arm"></i>armed (encoders)</span>
    </div>
  </div>
  <div id="fxwarn" class="nodata"></div>
  <div id="fxmap"></div>
  <div class="fxnote">The small dot grid on each voicing shows where its pad is, as you look at the
    grid: the top row is the menu, the white dot is the pad. One voicing per effect can be on.</div>
</section>
<main>
  <section class="panel">
    <h2>Pad grid &mdash; as you are looking at it</h2>
    <div id="gridwarn" class="nodata"></div>
    <div id="grid"></div>
    <div class="gridnote">
      Top row = menu categories. FX block is rotated 180&deg;: pad n shows slot 15&minus;n,
      so the bottom-right pad is slot 0. Showing the 12 slots of the live menu.<br>
      Each pad mirrors its LED on the grid: lit = the live voicing, outline = armed.
    </div>
  </section>
  <div style="display:grid;gap:16px">
    <section class="panel">
      <div class="usbrow"><h2>Volumes &mdash; HGSET</h2>
        <button id="usbbtn" class="usbbtn" type="button" aria-pressed="false">USB audio: &mdash;</button></div>
      <div id="volwarn" class="nodata"></div>
      <div id="vols"></div>
    </section>
    <section class="panel">
      <h2>Strings &mdash; pickup to tracker</h2>
      <div id="strwarn" class="nodata"></div>
      <div id="strings"></div>
      <div class="gridnote">Pluck each open string: its row should light and track near
        the expected pitch. A string tracking in the wrong row, or never locking, means the
        channel map or its range is wrong.</div>
    </section>
    <section class="panel">
      <h2>Meters</h2>
      <div id="meters"></div>
    </section>
    <section class="panel">
      <h2>Controls &mdash; physical panel state</h2>
      <div id="ctlwarn" class="nodata"></div>
      <div class="ctlgrid">
        <div class="ctl">
          <div class="ctlhead"><span>Expression pedal</span>
            <b id="c_expr_v" class="dash">&mdash;</b></div>
          <div class="bar"><div id="c_expr_f" class="barf"></div></div>
          <div class="ctlsub" id="c_expr_s">&mdash;</div>
        </div>
        <div class="ctl">
          <div class="ctlhead"><span>Pot</span>
            <b id="c_pot_v" class="dash">&mdash;</b></div>
          <div class="bar"><div id="c_pot_f" class="barf"></div></div>
          <div class="ctlsub" id="c_pot_s">clockwise = up</div>
        </div>
        <div class="ctl">
          <div class="ctlhead"><span>Joystick</span>
            <b id="c_joy_v" class="dash">&mdash;</b></div>
          <div class="joypad"><div id="c_joy_dot" class="joydot"></div>
            <div class="joyx"></div><div class="joyy"></div></div>
          <div class="ctlsub">switch <b id="c_joysw">&mdash;</b> (digital 6)</div>
        </div>
        <div class="ctl">
          <div class="ctlhead"><span>Encoders</span><b></b></div>
          <div class="encrow"><span>ENC1 main</span>
            <b id="c_enc1">&mdash;</b><i id="c_enc1b" class="btn">btn</i></div>
          <div class="encrow"><span>ENC2 aux</span>
            <b id="c_enc2">&mdash;</b><i id="c_enc2b" class="btn">btn</i></div>
          <div class="ctlsub">counts are raw, pre-detent-divide</div>
        </div>
        <div class="ctl">
          <div class="ctlhead"><span>IMU &mdash; MPU6050</span><b></b></div>
          <div class="ctlsub">accel <b id="acc">--</b> g</div>
          <div class="ctlsub">gyro <b id="gyr">--</b> &deg;/s</div>
        </div>
        <div class="ctl">
          <div class="ctlhead"><span>Lidar &mdash; VL53L1X</span>
            <b id="tof">--</b></div>
          <div class="bar"><div id="c_tof_f" class="barf"></div></div>
          <div class="ctlsub">0&ndash;1000 mm</div>
        </div>
      </div>
    </section>
  </div>
</main>
</div>
<style>
.ctlgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px}
.ctl{background:#151821;border:1px solid #262c3a;border-radius:8px;padding:9px 10px}
.ctlhead{display:flex;justify-content:space-between;align-items:baseline;
  font-size:12px;color:#8b93a7;margin-bottom:6px}
.ctlhead b{color:#e6e9f0;font-size:13px;font-variant-numeric:tabular-nums}
.ctlsub{font-size:11px;color:#6b7386;margin-top:5px}
.ctlsub b{color:#c8cedb;font-variant-numeric:tabular-nums}
.bar{height:9px;background:#0d1016;border-radius:5px;overflow:hidden}
.barf{height:100%;width:0%;background:linear-gradient(90deg,#3d7dff,#46e0c8);
  transition:width .08s linear}
.joypad{position:relative;height:84px;background:#0d1016;border-radius:6px;
  border:1px solid #262c3a;overflow:hidden}
.joyx{position:absolute;left:0;right:0;top:50%;height:1px;background:#262c3a}
.joyy{position:absolute;top:0;bottom:0;left:50%;width:1px;background:#262c3a}
.joydot{position:absolute;width:12px;height:12px;border-radius:50%;
  background:#46e0c8;box-shadow:0 0 10px #46e0c8a0;
  left:50%;top:50%;transform:translate(-50%,-50%);transition:all .06s linear}
.encrow{display:flex;justify-content:space-between;align-items:center;
  font-size:12px;color:#8b93a7;padding:2px 0}
.encrow b{color:#e6e9f0;font-variant-numeric:tabular-nums}
.btn{font-style:normal;font-size:10px;padding:1px 6px;border-radius:4px;
  background:#262c3a;color:#6b7386}
.btn.on{background:#46e0c8;color:#0d1016;font-weight:600}
.armdot{color:#46e0c8} .armno{color:#e0a046}
</style>
<script>
const DB_FLOOR = -80, DB_CEIL = 0;
let META = null, LAST = null;
const touched = {};              // name -> ms timestamp of last local edit
const HOLD = 700;                // ignore echoed vol for this long after a drag

function el(t, cls, txt){const e=document.createElement(t);
  if(cls)e.className=cls; if(txt!==undefined)e.textContent=txt; return e;}

/* ---------- build ---------- */
function buildGrid(){
  const g = document.getElementById('grid'); g.innerHTML='';
  for(let pad=0; pad<16; pad++){
    const d = el('div','pad');
    if(pad < 4){
      d.classList.add('menu'); d.dataset.menu = pad;
      d.append(el('span','sub', String(pad)), el('span','name','...'));
    }else{
      const slot = 15 - pad;                // <-- the 180-degree rotation
      d.dataset.slot = slot;
      d.append(el('span','sub','s'+slot), el('span','name','...'), el('span','eff',''));
    }
    g.append(d);
  }
}
function fmtVol(v, vmax){ return v.toFixed(vmax < 0.1 ? 4 : vmax < 1 ? 3 : 2); }
function buildVols(){
  const host = document.getElementById('vols'); host.innerHTML='';
  const groups = [["sub0","sub1","sub2","sub3","sub4","sub5"],["piezo","trs"],["outA","outB"]];
  const pretty = {sub0:"hex 1",sub1:"hex 2",sub2:"hex 3",sub3:"hex 4",sub4:"hex 5",
                  sub5:"hex 6",piezo:"piezo",trs:"trs",outA:"out A",outB:"out B"};
  groups.forEach((grp,gi)=>grp.forEach((name,i)=>{
    const row = el('div','vrow' + (gi>0 && i===0 ? ' grp' : ''));
    const lab = el('label', null, pretty[name]); lab.htmlFor='v_'+name;
    const inp = document.createElement('input');
    const vmax = (META.vol_range && META.vol_range[name]) || 4;
    inp.type='range'; inp.min=0; inp.max=vmax; inp.step=vmax/1000; inp.value=0; inp.id='v_'+name;
    const val = el('span','val','—');
    const push = ()=>{ touched[name]=Date.now(); val.textContent=fmtVol(+inp.value, vmax);
      fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({name:name, value:+inp.value})})
        .then(r=>r.json()).then(j=>{ if(!j.ok) console.error(j); }).catch(e=>console.error(e)); };
    inp.addEventListener('input', push);
    row.append(lab, inp, val); host.append(row);
  }));
  /* 2026-11-01 boost: wet makeup gain sliders (dB, -6..+12), same set/persist path as the gains */
  [["boost_pitch_db","PITCH boost dB"],["boost_formant_db","TIMBRE boost dB"]].forEach(([name,txt],i)=>{
    const row = el('div','vrow' + (i===0 ? ' grp' : ''));
    const lab = el('label', null, txt); lab.htmlFor='v_'+name;
    const inp = document.createElement('input');
    inp.type='range'; inp.min=META.boost_min; inp.max=META.boost_max; inp.step=0.1; inp.value=4; inp.id='v_'+name;
    const val = el('span','val','—');
    const push = ()=>{ touched[name]=Date.now(); val.textContent=(+inp.value).toFixed(1)+' dB';
      fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({name:name, value:+inp.value})})
        .then(r=>r.json()).then(j=>{ if(!j.ok) console.error(j); }).catch(e=>console.error(e)); };
    inp.addEventListener('input', push);
    row.append(lab, inp, val); host.append(row);
  });
  /* 2026-11-05 syn_sustain: synth sustain limiter 0..100 % (0 = follows the string exactly, default) */
  (()=>{ const name='syn_sustain';
    const row = el('div','vrow grp');
    const lab = el('label', null, 'SYNTH sustain'); lab.htmlFor='v_'+name;
    const inp = document.createElement('input');
    inp.type='range'; inp.min=0; inp.max=100; inp.step=1; inp.value=0; inp.id='v_'+name;
    const val = el('span','val','—');
    const push = ()=>{ touched[name]=Date.now(); val.textContent=Math.round(+inp.value)+' %';
      fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({name:name, value:+inp.value/100})})
        .then(r=>r.json()).then(j=>{ if(!j.ok) console.error(j); }).catch(e=>console.error(e)); };
    inp.addEventListener('input', push);
    row.append(lab, inp, val); host.append(row); })();
  /* 2026-11-05 syn_fmod: SYNTH filter mod 0..100 % = 0..4 octaves the string level opens the filter (default 50 %) */
  (()=>{ const name='syn_fmod';
    const row = el('div','vrow');
    const lab = el('label', null, 'SYNTH filter mod'); lab.htmlFor='v_'+name;
    const inp = document.createElement('input');
    inp.type='range'; inp.min=0; inp.max=100; inp.step=1; inp.value=50; inp.id='v_'+name;
    const val = el('span','val','—');
    const push = ()=>{ touched[name]=Date.now(); val.textContent=(+inp.value*0.04).toFixed(1)+' oct';
      fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({name:name, value:+inp.value/100})})
        .then(r=>r.json()).then(j=>{ if(!j.ok) console.error(j); }).catch(e=>console.error(e)); };
    inp.addEventListener('input', push);
    row.append(lab, inp, val); host.append(row); })();
  /* 2026-11-01 pwet: PITCH wet per interval (0..100 %), same set/echo path as the boost sliders */
  META.pwet_names.forEach((name,i)=>{
    const row = el('div','vrow' + (i===0 ? ' grp' : ''));
    const lab = el('label', null, (i===0 ? 'PITCH wet ' : '') + META.pwet_labels[i]); lab.htmlFor='v_'+name;
    const inp = document.createElement('input');
    inp.type='range'; inp.min=0; inp.max=100; inp.step=1; inp.value=100; inp.id='v_'+name;
    const val = el('span','val','—');
    const push = ()=>{ touched[name]=Date.now(); val.textContent=Math.round(+inp.value)+' %';
      fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({name:name, value:+inp.value/100})})
        .then(r=>r.json()).then(j=>{ if(!j.ok) console.error(j); }).catch(e=>console.error(e)); };
    inp.addEventListener('input', push);
    row.append(lab, inp, val); host.append(row);
  });
}
const NOTE = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B'];
function noteName(hz){ if(!(hz>0)) return '—';
  const m = Math.round(12*Math.log2(hz/440)+69), c = Math.round(100*(12*Math.log2(hz/440)+69-m));
  return NOTE[(m%12+12)%12] + (Math.floor(m/12)-1) + (c? (c>0?' +':' ')+c+'c':''); }
function stringInfo(){ return META && META.render ? META.render : null; }
function buildStrings(){
  const host = document.getElementById('strings'); host.innerHTML='';
  const R = stringInfo(), w = document.getElementById('strwarn');
  if(!R){ w.classList.add('show');
    w.textContent='render.cpp is not next to presets.cpp on this machine, so the string map is unknown.';
    return; }
  R.open_hz.forEach((hz,i)=>{
    const r = el('div','strrow'); r.id='str'+i;
    r.append(el('span','sname', (i+1)+' · '+noteName(hz).replace(/ .*/,'')),
             el('span','sch','ch '+R.string_channel[i]),
             (()=>{const t=el('span','strk'); t.innerHTML='<b>—</b>'; return t;})(),
             el('span','sdb','--'));
    const cf = el('div','scf'); cf.append(el('i')); cf.style.gridColumn='3 / 4';
    host.append(r, cf);
  });
}
function renderStrings(s){
  const R = stringInfo(); if(!R) return;
  const hz = s.extra && s.extra.hz, cf = s.extra && s.extra.cf;
  R.open_hz.forEach((open,i)=>{
    const r = document.getElementById('str'+i); if(!r) return;
    const h = hz ? hz[i] : null, c = cf ? cf[i] : null;
    const db = s.in ? s.in[R.string_channel[i]] : null;
    r.classList.toggle('live', h>0);
    r.querySelector('.strk').innerHTML = (h>0)
      ? '<b>'+noteName(h)+'</b> '+h.toFixed(1)+' Hz'
      : '<b>—</b> expects '+noteName(open).replace(/ .*/,'')+' ('+open.toFixed(0)+' Hz) and up';
    r.querySelector('.sdb').textContent = (db===null||db===undefined) ? '--' : db.toFixed(0)+' dB';
    const bar = r.nextElementSibling.querySelector('i');
    bar.style.width = (c===null||c===undefined) ? '0%' : Math.max(0,Math.min(100,c*100))+'%';
  });
}
function buildMeters(){
  const host = document.getElementById('meters'); host.innerHTML='';
  const mk = (kind, label, id)=>{ const r=el('div','m '+kind);
    const bar=el('div','bar'); const f=el('div','fill'); f.id=id+'_f'; bar.append(f);
    const db=el('span','db','--'); db.id=id+'_d';
    r.append(el('span','lbl',label), bar, db); host.append(r); };
  /* guitar + contact mics by channel, then the hex pickup in STRING order
   * with each string's real channel - raw channel order is not string order */
  const R = stringInfo();
  META.in_labels.forEach((l,i)=>{ if(!R || l[1]!=='hex') mk(l[1], l[0], 'in'+i); });
  if(R) R.string_channel.forEach((ch,i)=>mk('hex', 'Str '+(i+1)+' ch'+ch, 'in'+ch));
  META.out_labels.forEach((l,i)=>mk(l[1], l[0], 'out'+i));
}

const FX_NAMES = {PAD_FORMANT:"Formant", PAD_FWAH:"Formant wah", PAD_JAWARI:"Jawari",
  PAD_SITAR:"Sitar", PAD_PITCH:"Pitch shift", PAD_POG:"Pog swell", PAD_SINE:"Synth",
  PAD_FM:"FM synth", PAD_VIB:"Vibrato", PAD_CHORUS:"Chorus", PAD_PAN:"Stereo spread",
  PAD_PLUCKSAG:"Pluck sag", PAD_ENVF:"Auto-wah", PAD_RES:"Resonator",
  PAD_SUS:"Sustainer", PAD_SYMP:"Sympathetic strings", PAD_USB:"USB audio"};
FX_NAMES.PAD_SINE = "Synthesis";
const FX_NOTE = {PAD_PAN:"always on", PAD_USB:"off"};
function fxName(sym){ return FX_NAMES[sym] || sym.replace(/^PAD_/,'').toLowerCase(); }

function locGlyph(cat, slot){
  const g = el('span','loc'); const here = 15 - slot;   /* same rotation as the grid */
  for(let p=0;p<16;p++){ const d=el('i');
    if(p===cat) d.className='m';
    if(p===here) d.className='here';
    g.append(d); }
  g.setAttribute('aria-hidden','true');
  return g;
}
function buildFxMap(){
  const host = document.getElementById('fxmap'); host.innerHTML='';
  META.slots.forEach((cat, ci)=>{
    const col = el('div','fxcol'); col.dataset.cat = ci;
    const head = el('div','fxcolhead');
    const title = META.categories[ci] || ('Page '+ci);
    head.append(el('b', null, title.charAt(0) + title.slice(1).toLowerCase()),
                el('span','pos','menu pad '+(ci+1)));
    col.append(head);
    const groups = [];                      /* effects in order of first slot */
    cat.forEach((rec, si)=>{
      if(!rec.label) return;
      let g = groups.find(x=>x.sym===rec.effect);
      if(!g){ g={sym:rec.effect, slots:[]}; groups.push(g); }
      g.slots.push(si);
    });
    groups.forEach(g=>{
      const grp = el('div','fxgrp'); grp.dataset.cat=ci; grp.dataset.sym=g.sym;
      const nm = el('div','fxname');
      nm.append(el('b', null, fxName(g.sym)), el('em', null, FX_NOTE[g.sym] || 'off'));
      grp.append(nm);
      const chips = el('div','chips');
      g.slots.forEach(si=>{
        const c = el('span','chip'); c.dataset.cat=ci; c.dataset.slot=si;
        c.title = META.slots[ci][si].label + ' - slot ' + si;
        c.append(locGlyph(ci, si), el('span', null, META.slots[ci][si].label));
        chips.append(c);
      });
      grp.append(chips); col.append(grp);
    });
    host.append(col);
  });
}
let usbPending = 0;
function renderUsb(s){
  const b = document.getElementById('usbbtn');
  const v = s.extra && ('usb' in s.extra) ? s.extra.usb : null;
  if(Date.now() - usbPending < 800) return;       /* let the echo catch up */
  if(v === null){ b.textContent = 'USB audio: —'; b.classList.remove('on'); return; }
  const on = v > 0.5;
  b.textContent = on ? 'USB audio: on' : 'USB audio: off';
  b.classList.toggle('on', on); b.setAttribute('aria-pressed', on);
}
function wireUsb(){
  const b = document.getElementById('usbbtn');
  b.addEventListener('click', ()=>{
    const want = !b.classList.contains('on');
    usbPending = Date.now();
    b.classList.toggle('on', want); b.textContent = want ? 'USB audio: on' : 'USB audio: off';
    fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({name:'usb', value: want?1:0})}).catch(e=>console.error(e));
  });
}
const POT_ROLE = {PAD_CHORUS:"chorus depth", PAD_VIB:"vibrato depth",
  PAD_ENVF:"auto-wah sensitivity", PAD_PAN:"spread amount"};
function renderPotRole(s){
  const e = document.getElementById('c_pot_s'); if(!e || !META) return;
  const ids = META.effect_ids || {}; const armed = ('armed' in s) ? s.armed : -1;
  const sym = Object.keys(ids).find(k=>ids[k]===armed);
  const fx = s.fx || []; const pitchOn = META.slots[1] &&
    META.slots[1].some((r,i)=>r.effect==='PAD_PITCH' && (fx[12+i] & 1));
  e.textContent = (sym && POT_ROLE[sym]) ? 'now: ' + POT_ROLE[sym] + ' (moves after 10% turn)'
    : pitchOn ? 'now: pitch dry/wet (TRS guitar ↔ shifted hex)' : 'not assigned right now';
}
let clipPrev = null, clipHotUntil = 0, chanHot = {};
function renderClips(s){
  const X = s.extra || {}; const ic = X.ic, oc = X.oc;
  const n = document.getElementById('clipn'), st = document.getElementById('clipstat');
  if(!ic){ n.textContent = '--'; return; }
  const now = Date.now(), tin = ic.reduce((a,b)=>a+b,0);
  if(clipPrev){
    if(tin > clipPrev.tin || oc > clipPrev.oc) clipHotUntil = now + 3000;
    ic.forEach((v,i)=>{ if(v > clipPrev.ic[i]) chanHot[i] = now + 3000; });
  }
  clipPrev = {tin, oc, ic: ic.slice()};
  n.textContent = 'in ' + tin + ' · out ' + oc;
  st.classList.toggle('hot', now < clipHotUntil);
  for(let i=0;i<10;i++){ const f=document.getElementById('in'+i+'_f');
    if(f) f.closest('.m').classList.toggle('clip', now < (chanHot[i]||0)); }
}
function renderFx(s){
  const menu = ('menu' in s) ? s.menu : null;
  const fx = (s.fx && s.fx.length === META.slots.length*12) ? s.fx : null;
  const warn = document.getElementById('fxwarn');
  if(!fx){ warn.classList.add('show');
    warn.textContent = 'The Bela is not sending all-page state (old firmware), so only the '
      + 'page on the grid right now is shown live. The others are greyed.'; }
  else warn.classList.remove('show');
  document.querySelectorAll('.fxcol').forEach(c=>{
    const ci=+c.dataset.cat, cur = menu===ci;
    c.classList.toggle('cur', cur);
    c.querySelector('.pos').textContent = cur ? 'on the grid now' : 'menu pad '+(ci+1);
  });
  const stateOf = (ci, si)=> fx ? fx[ci*12+si]
                           : (menu===ci && s.slots ? s.slots[si] : null);
  document.querySelectorAll('.chip').forEach(c=>{
    const st = stateOf(+c.dataset.cat, +c.dataset.slot);
    c.classList.toggle('unknown', st===null);
    c.classList.toggle('on', !!(st & 1));
    c.classList.toggle('arm', !!(st & 2));
  });
  document.querySelectorAll('.fxgrp').forEach(g=>{
    const on = [...g.querySelectorAll('.chip.on')].map(c=>c.textContent);
    const known = !g.querySelector('.chip.unknown');
    g.classList.toggle('live', on.length>0);
    const em = g.querySelector('.fxname em'), note = FX_NOTE[g.dataset.sym];
    let extra = '';
    if(g.dataset.sym === 'PAD_SINE' && s.extra && s.extra.syn && stringInfo()){
      const nm = stringInfo().synth_names[s.extra.syn[0]] || ('mode '+s.extra.syn[0]);
      extra = ' · ' + nm.toLowerCase() + ' · ' + (s.extra.syn[1] ? 'poly' : 'mono');
    }
    em.textContent = !known ? 'state unknown'
      : on.length === 0 ? (note || 'off') + extra
      : on.length === 1 ? 'on: ' + on[0] + extra
      : 'on, voicing not chosen yet';
  });
}

/* ---------- render ---------- */
function dbPct(v){ return Math.max(0,Math.min(100,(v-DB_FLOOR)/(DB_CEIL-DB_FLOOR)*100)); }

function render(s){
  const meta = s._meta, stale = meta.stale;
  renderFx(s); renderUsb(s); renderPotRole(s); renderStrings(s); renderClips(s);
  document.body.classList.toggle('stale', stale);
  const b = document.getElementById('banner');
  if(stale){ b.classList.add('show');
    b.textContent = meta.age === null
      ? 'NO TELEMETRY EVER RECEIVED — nothing has arrived on the UDP port.'
      : 'STALE — no packet for ' + meta.age.toFixed(1) + ' s. Display frozen, do not trust it.';
  } else b.classList.remove('show');

  const bw = document.getElementById('bindwarn');
  if(meta.bind_error){ bw.classList.add('show'); bw.textContent = '⚠ ' + meta.bind_error; }
  else bw.classList.remove('show');

  document.getElementById('rate').textContent = stale ? '0.0' : meta.rate.toFixed(1);
  document.getElementById('pkts').textContent = meta.packets + (meta.bad? ' ('+meta.bad+' bad)':'');
  document.getElementById('age').textContent = meta.age===null? '--' : meta.age.toFixed(2)+'s';
  if(meta.last_set){ const ls=meta.last_set;
    document.getElementById('setline').innerHTML =
      'sent <b>HGSET '+ls.name+' '+ls.value.toFixed(2)+'</b> → '+ls.to; }

  /* menu row */
  const menu = ('menu' in s) ? s.menu : null;
  document.querySelectorAll('.pad.menu').forEach(d=>{
    const i = +d.dataset.menu;
    d.querySelector('.name').textContent = META.categories[i] || ('CAT '+i);
    d.classList.toggle('cur', menu === i);
  });

  /* fx pads: names come from the live menu's row of the parsed table */
  const cat = (menu!==null && META.slots[menu]) ? META.slots[menu] : null;
  const slots = s.slots || null;
  const armed = ('armed' in s) ? s.armed : -1;
  document.querySelectorAll('.pad[data-slot]').forEach(d=>{
    const i = +d.dataset.slot;
    const rec = cat ? cat[i] : null;
    const nm = d.querySelector('.name'), ef = d.querySelector('.eff');
    if(!rec || !rec.label){ d.classList.add('empty');
      nm.textContent = cat ? 'empty' : '—'; ef.textContent=''; }
    else { d.classList.remove('empty'); nm.textContent = rec.label;
           ef.textContent = rec.effect.replace(/^PAD_/,''); }
    /* slots[i] mirrors that pad's LED: bit 0 = lit, bit 1 = armed */
    const st = slots ? (slots[i] | 0) : 0;
    d.classList.toggle('on', !!(st & 1) && !!rec && !!rec.label);
    d.classList.toggle('armed', !!(st & 2) && !!rec);
  });

  /* meters */
  const put = (id, v)=>{ const f=document.getElementById(id+'_f'), t=document.getElementById(id+'_d');
    if(v===undefined||v===null){ f.style.width='0%'; t.innerHTML='<span class="dash">—</span>'; return; }
    f.style.width = dbPct(v)+'%'; f.classList.toggle('hot', v > -1.0); t.textContent = v.toFixed(1); };
  for(let i=0;i<META.in_labels.length;i++) put('in'+i, s.in ? s.in[i] : null);
  for(let i=0;i<META.out_labels.length;i++) put('out'+i, s.out ? s.out[i] : null);

  const f3 = a => a ? a.map(v=>v.toFixed(2)).join(' ') : null;
  const setTxt=(id,v)=>{const e=document.getElementById(id);
    if(v===null||v===undefined){e.innerHTML='<span class="dash">—</span>';} else e.textContent=v;};
  setTxt('acc', f3(s.acc)); setTxt('gyr', f3(s.gyr));
  setTxt('tof', ('tof' in s) ? (s.tof < 0 ? 'no reading' : s.tof.toFixed(0)+' mm') : null);

  /* ---------- controls ---------- */
  const X = s.extra || {};
  const setBar=(id,frac)=>{const f=document.getElementById(id);
    if(f) f.style.width = Math.max(0,Math.min(1,frac))*100 + '%';};
  const setB=(id,txt,dash)=>{const e=document.getElementById(id); if(!e) return;
    if(dash){e.innerHTML='<span class="dash">&mdash;</span>';}
    else {e.textContent=txt; e.classList.remove('dash');}};

  /* expression pedal - armed state matters more than the number. An UNARMED
     pedal floats mid-scale and looks perfectly live, which is the exact trap
     the arm-at-zero behaviour exists to avoid, so say so plainly. */
  if('expr' in X){
    setB('c_expr_v', (X.expr*100).toFixed(0)+'%');
    setBar('c_expr_f', X.expr);
    const armed = X.exprarm > 0.5;
    const raw = ('exprraw' in X) ? ' · raw '+X.exprraw.toFixed(3) : '';
    const e=document.getElementById('c_expr_s');
    e.innerHTML = armed
      ? '<span class="armdot">ARMED</span>'+raw
      : '<span class="armno">NOT ARMED</span> — sweep to heel once'+raw;
  } else { setB('c_expr_v',null,true); setBar('c_expr_f',0); }

  if('pot' in X){ setB('c_pot_v',(X.pot*100).toFixed(0)+'%'); setBar('c_pot_f',X.pot); }
  else { setB('c_pot_v',null,true); setBar('c_pot_f',0); }

  if('joy' in X && X.joy.length>=2){
    const jx=X.joy[0], jy=X.joy[1];
    setB('c_joy_v', jx.toFixed(2)+', '+jy.toFixed(2));
    const d=document.getElementById('c_joy_dot');
    d.style.left = (50 + jx*46) + '%';
    d.style.top  = (50 - jy*46) + '%';     // +Y is up on screen
  } else setB('c_joy_v',null,true);
  const jsw=document.getElementById('c_joysw');
  if('joysw' in X){ jsw.textContent = X.joysw>0.5 ? 'HELD' : 'up';
    jsw.className = X.joysw>0.5 ? 'armdot':''; }
  else jsw.innerHTML='<span class="dash">&mdash;</span>';

  /* 2026-11-01 boost: v8 firmware sends ONE encoder (enc:N encbtn:N); older builds sent pairs. Accept both. */
  { const E = ('enc' in X) ? [].concat(X.enc) : [];
    const B = ('encbtn' in X) ? [].concat(X.encbtn) : [];
    setB('c_enc1', E[0], E.length<1); setB('c_enc2', E[1], E.length<2);
    if(B.length>=1) document.getElementById('c_enc1b').classList.toggle('on', B[0]>0.5);
    if(B.length>=2) document.getElementById('c_enc2b').classList.toggle('on', B[1]>0.5);
  }
  setBar('c_tof_f', ('tof' in s && s.tof>0) ? s.tof/1000 : 0);

  /* controls the running binary does not send at all */
  const cw=document.getElementById('ctlwarn');
  const cmiss=[];
  ['expr','joy','pot','enc'].forEach(k=>{ if(!(k in X)) cmiss.push(k+':'); });
  if(cmiss.length && !stale){ cw.classList.add('show');
    cw.textContent = 'Not in the stream: '+cmiss.join(' ')
      +' — the running Bela binary predates these fields.'; }
  else cw.classList.remove('show');

  /* volumes echoed back from the device */
  if(s.vol){ const now=Date.now();
    META.vol_names.forEach((name,i)=>{
      const v = s.vol[i]; if(v===undefined) return;
      const inp = document.getElementById('v_'+name); if(!inp) return;
      const row = inp.parentElement, val = row.querySelector('.val');
      if(now - (touched[name]||0) < HOLD) return;      // user is dragging
      const vmax = +inp.max;
      if(Math.abs(+inp.value - v) > vmax/2000) inp.value = v;
      val.textContent = fmtVol(v, vmax);
      val.classList.toggle('drift', v > vmax*1.0001 || v < 0);   /* outside this slider's range */
    });
  }

  /* 2026-11-01 boost: echo bp:/bf: into the boost sliders */
  [["boost_pitch_db","bp"],["boost_formant_db","bf"]].forEach(([name,key])=>{
    if(!(key in X)) return;
    const inp = document.getElementById('v_'+name); if(!inp) return;
    if(Date.now() - (touched[name]||0) < HOLD) return;
    const v = +X[key];
    if(Math.abs(+inp.value - v) > 0.05) inp.value = v;
    inp.parentElement.querySelector('.val').textContent = v.toFixed(1)+' dB';
  });

  /* 2026-11-05 syn_sustain: echo ss: */
  if('ss' in X){ const inp = document.getElementById('v_syn_sustain');
    if(inp && Date.now() - (touched['syn_sustain']||0) >= HOLD){
      const v = Math.round(+X.ss*100);
      if(Math.abs(+inp.value - v) > 0.5) inp.value = v;
      inp.parentElement.querySelector('.val').textContent = v+' %'; } }

  /* 2026-11-05 syn_fmod: echo fm: */
  if('fm' in X){ const inp = document.getElementById('v_syn_fmod');
    if(inp && Date.now() - (touched['syn_fmod']||0) >= HOLD){
      const v = Math.round(+X.fm*100);
      if(Math.abs(+inp.value - v) > 0.5) inp.value = v;
      inp.parentElement.querySelector('.val').textContent = (v*0.04).toFixed(1)+' oct'; } }

  /* 2026-11-01 pwet: echo pw:a,b,c,d,e,f into the per-interval wet sliders */
  if(Array.isArray(X.pw)) META.pwet_names.forEach((name,i)=>{
    const inp = document.getElementById('v_'+name); if(!inp || X.pw[i]===undefined) return;
    if(Date.now() - (touched[name]||0) < HOLD) return;
    const v = Math.round(+X.pw[i]*100);
    if(Math.abs(+inp.value - v) >= 1) inp.value = v;
    inp.parentElement.querySelector('.val').textContent = v+' %';
  });

  /* be explicit when the stream simply does not carry these fields:
     a slider parked at 1.00 with nothing behind it is a lie */
  const gw = document.getElementById('gridwarn');
  const missing = [];
  if(!('menu' in s)) missing.push('menu:');
  if(!s.slots) missing.push('slots:');
  if(!('armed' in s)) missing.push('armed:');
  if(missing.length && !stale){
    gw.classList.add('show');
    gw.innerHTML = 'No ' + missing.join(' / ') + ' in the telemetry stream. '
      + 'The pads below are NOT reporting the instrument — the running Bela '
      + 'binary predates that part of the HGMON line. Names are from presets.cpp.';
  } else gw.classList.remove('show');

  const vw = document.getElementById('volwarn');
  const vpanel = vw.parentElement;
  if(!s.vol && !stale){
    vw.classList.add('show');
    vw.textContent = 'No vol: in the telemetry stream — the values below are '
      + 'UI defaults, not the instrument’s. HGSET still sends; nothing confirms it.';
    vpanel.classList.add('unconfirmed');
  } else { vw.classList.remove('show'); vpanel.classList.remove('unconfirmed'); }
}

/* ---------- poll ---------- */
async function tick(){
  try{
    const r = await fetch('/api/state',{cache:'no-store'});
    LAST = await r.json(); render(LAST);
  }catch(e){
    document.body.classList.add('stale');
    const b=document.getElementById('banner'); b.classList.add('show');
    b.textContent = 'SERVER UNREACHABLE — hgweb.py is not answering.';
  }
}
(async function main(){
  META = await (await fetch('/api/meta')).json();
  buildGrid(); buildVols(); buildMeters(); buildFxMap(); wireUsb(); buildStrings();
  tick(); setInterval(tick, 100);
})();
</script></body></html>
"""


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="hyperGuitar browser dashboard (UDP <-> HTTP bridge).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
--reuseport caveat: on macOS two sockets bound to the same UDP port with
SO_REUSEPORT do not each get a copy of a unicast datagram -- the kernel
delivers each packet to exactly ONE of them.  Sharing the port with another
listener therefore halves both readers' packet rates.  It is there so you can
start anyway, not because it is a good idea.  Prefer stopping the other tool.
""")
    ap.add_argument("--udp-host", default=DEFAULT_UDP_HOST)
    ap.add_argument("--udp-port", type=int, default=DEFAULT_UDP_PORT)
    ap.add_argument("--reuseport", action="store_true",
                    help="set SO_REUSEPORT on the telemetry socket (see below)")
    ap.add_argument("--bela", default=DEFAULT_BELA,
                    help=f"control target host (default {DEFAULT_BELA})")
    ap.add_argument("--bela-port", type=int, default=DEFAULT_BELA_PORT)
    ap.add_argument("--http-port", type=int, default=DEFAULT_HTTP_PORT)
    ap.add_argument("--http-host", default="127.0.0.1")
    ap.add_argument("--presets", default=DEFAULT_PRESETS,
                    help="path to presets.cpp (parsed for the pad names)")
    ap.add_argument("--header", default=DEFAULT_HEADER,
                    help="path to hyperguitar.h (parsed for the PAD_* effect ids)")
    ap.add_argument("--loopback", action="store_true",
                    help="shorthand: send control to 127.0.0.1 (for hgweb_fake.py)")
    args = ap.parse_args()

    if args.loopback:
        args.bela = "127.0.0.1"

    try:
        cat_names, slots = parse_presets(args.presets)
    except PresetParseError as exc:
        print("FATAL: could not parse the preset table.", file=sys.stderr)
        print("       " + str(exc), file=sys.stderr)
        print("       Refusing to start: serving invented pad names would be "
              "worse than serving none.", file=sys.stderr)
        return 2

    labelled = sum(1 for c in slots for s in c if s["label"])
    print(f"   presets:   {args.presets}")
    print(f"              {len(cat_names)} categories "
          f"({', '.join(cat_names)}), {labelled}/48 slots labelled")

    tel = Telemetry()
    ctl = Control(args.bela, args.bela_port, tel)

    # Resolve every slot's symbolic effect to the firmware's real numeric id,
    # because armed: on the wire carries that number.
    try:
        eff_ids = parse_effect_ids(args.header)
    except PresetParseError as exc:
        print(f"FATAL: {exc}", file=sys.stderr)
        return 2
    missing = sorted({s["effect"] for cat in slots for s in cat
                      if s["effect"] not in eff_ids and s["effect"] != "FX_NONE"})
    if missing:
        print("FATAL: preset table uses effect symbols that are not defined "
              f"in {args.header}: {', '.join(missing)}", file=sys.stderr)
        return 2
    for cat in slots:
        for s in cat:
            s["effect_id"] = eff_ids.get(s["effect"], -1)
            if s["effect"] == "FX_NONE":
                s["label"] = ""

    rinfo = parse_render_info(os.path.join(os.path.dirname(args.presets), "render.cpp"))
    if rinfo is None:
        print("   strings:   render.cpp not found next to presets.cpp - string panel disabled")
    else:
        print(f"   strings:   channels {rinfo['string_channel']}, open Hz {rinfo['open_hz']}")
    meta = {
        "render": rinfo,
        "categories": cat_names,
        "slots": slots,
        "effect_ids": eff_ids,
        "vol_names": VOL_NAMES,
        "boost_min": BOOST_MIN, "boost_max": BOOST_MAX,
        "pwet_names": PWET_NAMES, "pwet_labels": PWET_LABELS,
        "vol_min": VOL_MIN, "vol_max": VOL_MAX, "vol_range": VOL_RANGE,
        "in_labels": IN_LABELS, "out_labels": OUT_LABELS,
        "stale_after": STALE_AFTER,
        "presets_path": args.presets,
        "control_target": f"{args.bela}:{args.bela_port}",
    }

    t = threading.Thread(target=udp_reader,
                         args=(tel, args.udp_host, args.udp_port, args.reuseport),
                         daemon=True)
    t.start()
    time.sleep(0.15)      # let a bind failure print before the URL does

    Handler.tel, Handler.ctl = tel, ctl
    Handler.page = PAGE.encode("utf-8")
    Handler.meta = meta

    # Warn if the HTTP port is already serving something.  A dual-stack
    # listener on *:PORT and our 127.0.0.1:PORT can both bind, and then which
    # one the browser reaches is a coin toss -- you get someone else's page
    # with no error anywhere.
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.settimeout(0.3)
    try:
        probe.connect((args.http_host, args.http_port))
        print(f"!! WARNING: something is already listening on "
              f"{args.http_host}:{args.http_port}. Your browser may reach IT "
              f"instead of this server. Use --http-port to pick another.",
              file=sys.stderr)
    except OSError:
        pass
    finally:
        probe.close()

    httpd = ThreadingHTTPServer((args.http_host, args.http_port), Handler)
    httpd.daemon_threads = True
    print(f"   control:   HGSET -> udp://{args.bela}:{args.bela_port}")
    print(f"   open:      http://{args.http_host}:{args.http_port}/")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n   bye")
    return 0


if __name__ == "__main__":
    sys.exit(main())
