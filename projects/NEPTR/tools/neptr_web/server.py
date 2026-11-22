#!/usr/bin/env python3
"""neptr_web - preset editor for NEPTR, served from the Pi (stdlib only).

    python3 tools/neptr_web/server.py [--port 8091]

Live controls go to the engine over OSC 7770; bank edits are file writes
followed by /bank_reload. See docs/neptr_web.md.
"""
import argparse, json, os, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import store as S

STATIC = os.path.join(HERE, "static")
CAT = S.catalogue()
EFFECTS = {e["id"]: e for e in CAT["effects"]}
SMAP = S.state_map()
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
         ".css": "text/css; charset=utf-8", ".json": "application/json",
         ".png": "image/png", ".webmanifest": "application/manifest+json"}


def clean_steps(steps):
    if steps is None:
        return None
    out = []
    for st in steps[:S.NSTEPS]:
        t = int(st.get("type", 0))
        if t == 1:
            if int(st["a"]) not in EFFECTS:
                raise ValueError("unknown effect id %s" % st["a"])
            out.append(dict(type=1, a=int(st["a"])))
        elif t == 2:
            out.append(dict(type=2, a=1 if float(st["a"]) >= 0.5 else 0))
        elif t == 3:
            out.append(dict(type=3, a=max(0, min(24, int(st["a"]))),
                            b=max(-12.0, min(18.0, float(st.get("b", 0)))),
                            c=max(-18.0, min(18.0, float(st.get("c", 0))))))
        else:
            raise ValueError("bad step type %s" % t)
    return out


def api_state():
    bank, seq = S.load_bank()
    names = S.load_names()
    slots = []
    for s in range(1, S.NSLOT + 1):
        slots.append(dict(slot=s, occupied=S.occupied(bank, s), name=names.get(str(s), ""),
                          seq=S.get_seq(seq, s)))
    return dict(slots=slots, effects=CAT["effects"], nam=S.nam_profiles(),
                nam_rows=CAT["nam_rows"], live=api_live())


def api_live():
    v = S.read_live()
    if v is None:
        return None
    on = {}
    for e in CAT["effects"]:
        if e["var"] and e["var"] in SMAP:
            on[e["id"]] = v[SMAP[e["var"]]] < 0.5
    ir_row, ir_byp = int(v[SMAP["gkIRActiveRow"]]), v[SMAP["gkIRBypass"]]
    for r in range(5):
        on[49 + r] = (ir_byp < 0.5 and ir_row == r)
    row = int(v[SMAP["gkNAMActiveRow"]])
    sel = int(v[SMAP["gkNAMRowSel[%d]" % row]]) if row >= 0 and ("gkNAMRowSel[%d]" % row) in SMAP else 0
    voc = lambda i, d: (v[i] - 100) if v[i] > 50 else d
    return dict(on=on, nam=dict(profile=row * 5 + sel if row >= 0 else -1,
                                bypass=v[SMAP["gkNAMBypass"]] >= 0.5,
                                inGain=v[SMAP["gkNAMInGain"]], outGain=v[SMAP["gkNAMOutGain"]]),
                voc=dict(on=voc(319, 1) >= 0.5, level=voc(330, 1.0)))


def api_slot(s):
    bank, seq = S.load_bank()
    row = S.slot_row(bank, s)
    vals = {}
    for e in CAT["effects"]:
        for p in e["params"]:
            vals[p["name"]] = row[p["index"]]
    sq = S.get_seq(seq, s)
    derived = None
    if sq is None and S.occupied(bank, s):
        # legacy slot: effects the row has on, in id order, VOLUME last
        derived = []
        for e in CAT["effects"]:
            if e["var"] and e["var"] in SMAP and e["id"] != 1 and not e["generative"] \
                    and row[SMAP[e["var"]]] < 0.5:
                derived.append(dict(type=1, a=e["id"]))
        if row[SMAP["gkVOLpedalBypass"]] < 0.5:
            derived.append(dict(type=1, a=1))
    return dict(slot=s, occupied=S.occupied(bank, s), seq=sq, derived=derived, vals=vals,
                name=S.load_names().get(str(s), ""))


def api_save_slot(s, body):
    bank, seq = S.load_bank()
    if not S.occupied(bank, s):
        # new preset: start from the live state (what the rig sounds like now)
        base = body.get("from")
        if base and 1 <= int(base) <= S.NSLOT and S.occupied(bank, int(base)):
            row = S.slot_row(bank, int(base))
        else:
            row = S.read_live()
            if row is None:
                raise ValueError("no live state to start from")
        S.set_row(bank, s, row)
    for k, val in (body.get("vals") or {}).items():
        if k in SMAP:
            a = bank.base + (s - 1) * S.SLOT + SMAP[k]
            bank.v[a] = float(val)
    if "seq" in body:
        S.set_seq(seq, s, clean_steps(body["seq"]))
    S.save_bank(bank, seq)
    if "name" in body:
        n = S.load_names()
        if body["name"]:
            n[str(s)] = str(body["name"])[:24]
        else:
            n.pop(str(s), None)
        S.save_names(n)
    return dict(ok=True)


def api_clear(s):
    bank, seq = S.load_bank()
    S.clear_slot(bank, seq, s)
    S.save_bank(bank, seq)
    n = S.load_names(); n.pop(str(s), None); S.save_names(n)
    return dict(ok=True)


def api_copy(s, src):
    bank, seq = S.load_bank()
    if not S.occupied(bank, src):
        raise ValueError("source empty")
    S.set_row(bank, s, S.slot_row(bank, src))
    S.set_seq(seq, s, S.get_seq(seq, src))
    S.save_bank(bank, seq)
    n = S.load_names()
    if str(src) in n:
        n[str(s)] = n[str(src)]; S.save_names(n)
    return dict(ok=True)


def post(path, b):
    parts = path.strip("/").split("/")
    if parts[:2] == ["api", "slot"]:
        s = int(parts[2]); assert 1 <= s <= S.NSLOT
        act = parts[3] if len(parts) > 3 else "save"
        if act == "save":
            return api_save_slot(s, b)
        if act == "clear":
            return api_clear(s)
        if act == "copy":
            return api_copy(s, int(b["from"]))
        if act == "recall":
            S.osc_cc(16, 106, s); return dict(ok=True)
        if act == "capture":                 # device capture (records activation order)
            S.osc_cc(16, 105, s); return dict(ok=True)
    if path == "/api/fx":
        i = int(b["id"]); assert i in EFFECTS
        S.osc_send("/fx", i, 1 if b.get("on") else 0); return dict(ok=True)
    if path == "/api/nam":
        S.osc_send("/nam", int(b["profile"]), float(b["in"]), float(b["out"])); return dict(ok=True)
    if path == "/api/nam_byp":
        S.osc_send("/nam_byp", 1 if b.get("bypass") else 0); return dict(ok=True)
    if path == "/api/vocal":
        if "on" in b:
            S.osc_cc(16, 55, 127 if b["on"] else 0)   # voc_gate x0/x1 (not voc_on FX switch)
        if "level" in b:
            S.osc_cc(16, 51, round(max(0.0, min(4.0, float(b["level"]))) / 4 * 127))
        return dict(ok=True)
    raise KeyError(path)


class H(BaseHTTPRequestHandler):
    server_version = "neptr_web"

    def log_message(self, *a):
        pass

    def _send(self, code, data, ctype="application/json"):
        raw = data if isinstance(data, bytes) else json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        p = urlparse(self.path).path
        try:
            if p == "/api/state":
                return self._send(200, api_state())
            if p == "/api/live":
                return self._send(200, api_live() or {})
            if p.startswith("/api/slot/"):
                return self._send(200, api_slot(int(p.split("/")[3])))
            f = "index.html" if p == "/" else p.lstrip("/")
            full = os.path.normpath(os.path.join(STATIC, f))
            if not full.startswith(STATIC + os.sep) or not os.path.isfile(full):
                return self._send(404, {"error": "not found"})
            self._send(200, open(full, "rb").read(), TYPES.get(os.path.splitext(full)[1], "application/octet-stream"))
        except Exception as e:
            self._send(500, {"error": str(e)})

    def do_POST(self):
        try:
            n = int(self.headers.get("Content-Length") or 0)
            b = json.loads(self.rfile.read(n) or b"{}")
            self._send(200, post(urlparse(self.path).path, b))
        except Exception as e:
            self._send(400, {"error": "%s: %s" % (type(e).__name__, e)})


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8091)
    a = ap.parse_args()
    ThreadingHTTPServer.daemon_threads = True
    ThreadingHTTPServer(("0.0.0.0", a.port), H).serve_forever()
