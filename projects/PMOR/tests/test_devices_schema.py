#!/usr/bin/env python3
"""test_devices_schema.py -- every schema-id control in the generated patchers matches max/devices/schema.json (id, range, steps)."""
import glob, json, os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEV = os.path.join(ROOT, "max", "devices")
sch = json.load(open(os.path.join(DEV, "schema.json")))
E = {e[0]: e for e in sch["schema"]}
fails = checked = 0

def fail(m):
    global fails
    fails += 1; print("FAIL", m)

assert len(E) == len(sch["schema"]) and len(E) > 100, "schema empty or duplicate ids"

def walk(p):
    yield p
    for b in p["boxes"]:
        if isinstance(b["box"].get("patcher"), dict):
            yield from walk(b["box"]["patcher"])

SOFT = 73
for f in sorted(glob.glob(os.path.join(DEV, "*.maxpat"))):
    top = json.load(open(f))["patcher"]
    for p in walk(top):
        for b in p["boxes"]:
            b = b["box"]
            m = re.fullmatch(r"p(\d+)", b.get("varname", ""))
            sm = re.fullmatch(r"s(\d+)", b.get("varname", ""))
            if sm and b["maxclass"] == "dial":
                e = E.get(1024 + int(sm.group(1)))
            elif m:
                e = E.get(int(m.group(1)))
                if e is None:
                    fail("%s: %s not in schema" % (os.path.basename(f), b["varname"])); continue
            else:
                continue
            checked += 1
            lo, hi, fl = e[2], e[3], e[5]
            c = b["maxclass"]
            if c == "dial":
                top_v = b["min"] + b["mult"] * (b["size"] - 1)
                if abs(b["min"] - lo) > 1e-9 or abs(top_v - hi) > 1e-6 * max(1, abs(hi)):
                    fail("%s %s: dial range %s..%s vs schema %s..%s" % (os.path.basename(f), b["varname"], b["min"], top_v, lo, hi))
                if bool(fl & SOFT) != (b["floatoutput"] == 0):
                    fail("%s %s: integer steps mismatch" % (os.path.basename(f), b["varname"]))
            elif c == "toggle":
                if not (lo == 0 and hi == 1 and fl & SOFT):
                    fail("%s %s: toggle on non-boolean param" % (os.path.basename(f), b["varname"]))
            elif c == "umenu":
                n = len([i for i in b["items"] if i != ","])
                if n and n != hi - lo + 1 and n != hi + 1:
                    fail("%s %s: umenu has %d items, schema range %s..%s" % (os.path.basename(f), b["varname"], n, lo, hi))

# ---- reachability: every non-hidden schema id has a control somewhere (p<id> controls, bank controls through some selection, FX cells)
banks = json.load(open(os.path.join(DEV, "banks.json")))
def _grp(q): return (3 if q % 4 < 2 else 2) if q // 4 < 2 else (0 if q % 4 < 2 else 1)    # drum group of a pad (KICK 0 SNARE 1 HAT 2 PERC 3)
reach = set()
for f in sorted(glob.glob(os.path.join(DEV, "*.maxpat"))):
    for p in walk(json.load(open(f))["patcher"]):
        for b in p["boxes"]:
            m = re.fullmatch(r"p(\d+)", b["box"].get("varname", ""))
            if m:
                reach.add(int(m.group(1)))
for kind_k in banks.values():
    kind, k = kind_k[0], kind_k[1]
    reach.update({"id": [k], "syn": [1024 + a * 32 + k for a in range(8)], "pad": [2048 + q * 8 + k for q in range(16)], "grp": [2320 + g * 8 + k for g in range(4)],
                  "slot": [3072 + s * 8 + k for s in range(16)], "deep": [2368 + g * 16 + k for g in range(4)], "slotx": [3584 + s * 8 + k for s in range(16)],
                  }.get(kind, [((7 + q // 4) << 10) | ((q % 4) * 256 + k) for q in range(16) if _grp(q) == int(kind[4:])] if kind.startswith("dsyn") else None))
for e in sch["schema"]:
    if e[0] not in reach and not (e[5] & 32):
        fail("schema id %d %s (%s) has no control in any device" % (e[0], e[1], e[6]))
# ---- every path to [send pmor_tx] goes through pmor_guard.js (a load must never write to PMOR); no live.* control restores an initial value into a bank control
guarded = 0
for f in sorted(glob.glob(os.path.join(DEV, "*.maxpat"))):
    for p in walk(json.load(open(f))["patcher"]):
        bx = {b["box"]["id"]: b["box"] for b in p["boxes"]}
        sends = [i for i, b in bx.items() if b.get("text") == "send pmor_tx"]
        for i in sends:
            for ln in p["lines"]:
                if ln["patchline"]["destination"][0] == i:
                    src = bx[ln["patchline"]["source"][0]]
                    guarded += 1
                    if src.get("text") != "js pmor_guard.js":
                        fail("%s: [send pmor_tx] fed by %r, not pmor_guard.js" % (os.path.basename(f), src.get("text")))
        for b in bx.values():
            if re.match(r"bk[rn]?_", b.get("varname", "")) and b["maxclass"].startswith("live."):
                if b["saved_attribute_attributes"]["valueof"].get("parameter_initial_enable"):
                    fail("%s %s: live bank control restores an initial value" % (os.path.basename(f), b["varname"]))
# ---- design system: the palette mirrored into pmor_ui.js is the one in make_devices.py, and the disabled colour pmor_ui.js paints is the DISABLED role
import importlib.util
_sp = importlib.util.spec_from_file_location("make_devices", os.path.join(DEV, "make_devices.py"))
_md = importlib.util.module_from_spec(_sp); _sp.loader.exec_module(_md)
_js = open(os.path.join(DEV, "pmor_ui.js")).read()
_m = re.search(r"//@THEME-BEGIN[^\n]*\nvar THEME = (\{.*?\});\n//@THEME-END", _js, re.S)
if not _m:
    fail("pmor_ui.js has no //@THEME block")
elif json.loads(_m.group(1)) != json.loads(json.dumps(_md.THEME)):
    fail("pmor_ui.js THEME block differs from make_devices.THEME (run make_devices.py)")
if "var DIM = THEME.DISABLED;" not in _js:
    fail("pmor_ui.js DIM is not THEME.DISABLED")
print("test_devices_schema: %d tx paths go through pmor_guard.js; %d ids reachable" % (guarded, len(reach)))
print("test_devices_schema: %d schema-bound controls checked, %d schema entries, %d failure(s)" % (checked, len(E), fails))
sys.exit(1 if fails else 0)
