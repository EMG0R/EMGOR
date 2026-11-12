#!/usr/bin/env python3
"""lint_devices.py -- fails if any committed Max patcher / device can open a hardware port just by being loaded.

A Max port object opens its port the moment it is created, so a saved patcher must never contain one
(see max/devices/pmor_port.js: the port object is created only when "connect" is switched on).
Checks every max/**/*.maxpat and *.amxd: no box whose maxclass or first word of its text is a port object
(serial, hi, midiout, midiinfo-like senders are not used; we also refuse udpsend/udpreceive/oscsend/jweb direct hardware paths).
"""
import glob, json, os, re, struct, sys

# scheduler objects: none may exist in any saved patcher (editors included). Timing is done by js Tasks that exist only while needed
# (pmor_session.js while connected, pmor_edit.js while an editor is open, pmor_join.js while armed, pmor_port.js closer): the editor polls nothing when closed.
SCHED = {"metro", "qmetro", "delay", "del", "pipe", "tempo", "cpuclock", "timer", "clocker", "uzi", "transport", "seq", "seq~"}
TASK_OK = {"pmor_edit.js", "pmor_session.js", "pmor_join.js", "pmor_port.js"}
# attribute names that look right but do not exist on the Max 9 object (measured with getattrnames; a wrong name is silently ignored => default grey/white)
WRONG_ATTR = {"dial": ["dialcolor", "color"], "toggle": ["color"], "umenu": ["bgcolor"], "panel": ["bgcolor"], "slider": ["color"]}
BAD = {"serial", "midiout", "noteout", "ctlout", "pgmout", "bendout", "touchout", "polyout", "sxformat",
       "hi", "udpsend", "udpreceive", "oscsend", "oscreceive", "tcpclient", "tcpserver", "mxj.net"}

# colour attributes of live.* objects are checked against the Max refpages when Max is installed (a wrong name is silently ignored and Live paints its own theme colour:
# live.text has no `textoncolor`, live.menu has no `bgcolor`, ...). Skipped without Max.
REFDIRS = ["/Applications/Max.app/Contents/Resources/C74/docs/refpages/m4l-ref"]
_REF = {}
def ref_attrs(cls):
    if cls not in _REF:
        _REF[cls] = None
        for d in REFDIRS:
            f = os.path.join(d, cls + ".maxref.xml")
            if os.path.isfile(f):
                _REF[cls] = set(re.findall(r'<attribute name="([A-Za-z_0-9]+)"', open(f, encoding="utf-8", errors="replace").read()))
    return _REF[cls]

def boxes(node):
    if isinstance(node, dict):
        if "box" in node and isinstance(node["box"], dict):
            yield node["box"]
        for v in node.values():
            yield from boxes(v)
    elif isinstance(node, list):
        for v in node:
            yield from boxes(v)

def patcher_json(path):
    raw = open(path, "rb").read()
    if path.endswith(".amxd"):
        k = raw.find(b"ptch")
        n = struct.unpack("<I", raw[k + 4:k + 8])[0]
        raw = raw[k + 8:k + 8 + n].rstrip(b"\0")
    return json.loads(raw.decode("utf-8", "replace"))

def walk(p, path="top"):
    """yield (path, patcher dict) for a patcher and every nested box 'patcher'"""
    yield path, p
    for b in p.get("boxes", []):
        b = b["box"]
        if isinstance(b.get("patcher"), dict):
            yield from walk(b["patcher"], path + "/" + (b.get("text") or "patcher"))

def js_outlets(root, name):
    f = os.path.join(root, "max", "devices", name)
    m = re.search(r"outlets\s*=\s*(\d+)", open(f).read())
    return int(m.group(1)) if m else None

def count_class(p, word):
    return sum(1 for b in p.get("boxes", []) if (b["box"].get("text") or "").split(" ")[0] == word)


def check_patcher(root, where, p, schema_ids, nested=False):
    errs = []
    ids = {}
    for b in p.get("boxes", []):
        b = b["box"]
        if b["id"] in ids:
            errs.append("%s: duplicate id %s" % (where, b["id"]))
        ids[b["id"]] = b
        words = (b.get("text") or "").split()
        for a in WRONG_ATTR.get(b.get("maxclass"), []):
            if a in b:
                errs.append("%s: %s '%s' sets attribute '%s' which that object does not have" % (where, b.get("maxclass"), b.get("varname") or b["id"], a))
        if str(b.get("maxclass", "")).startswith("live."):
            known = ref_attrs(b["maxclass"])
            for a in b:
                if known is not None and a.endswith("color") and a not in known:
                    errs.append("%s: %s '%s' sets colour attribute '%s' which the Max refpage does not list (Live would paint its own theme colour)" % (where, b["maxclass"], b.get("varname") or b["id"], a))
        if b.get("maxclass") == "toggle" and "color" in b:
            errs.append("%s: toggle uses checkedcolor/elementcolor, not color" % where)
        if not words:
            continue
        if words[0] in SCHED:
            errs.append("%s: scheduler object '%s' in a saved patcher (use a js Task that exists only while needed)" % (where, words[0]))
        if words[0] == "p" and isinstance(b.get("patcher"), dict):
            ni, no = count_class(b["patcher"], "inlet"), count_class(b["patcher"], "outlet")
            if b["numinlets"] != ni or b["numoutlets"] != no:
                errs.append("%s: [p %s] declares %d/%d inlets/outlets, the subpatcher has %d/%d" % (where, " ".join(words[1:]), b["numinlets"], b["numoutlets"], ni, no))
        if words[0] == "js" and len(words) > 1 and words[1] in ("pmor_ui.js", "pmor_browser.js") and nested and "gated" not in words:
            errs.append("%s: editor-scope %s must be `gated` (draws nothing while the editor is closed)" % (where, words[1]))
        if words[0] in ("send", "receive", "s", "r") and len(words) > 1 and not words[1].startswith("pmor_"):
            errs.append("%s: %s name '%s' not pmor_-prefixed" % (where, words[0], words[1]))
        if words[0] == "js" and len(words) > 1:
            if not os.path.isfile(os.path.join(root, "max", "devices", words[1])):
                errs.append("%s: js file %s missing" % (where, words[1]))
            else:
                n = js_outlets(root, words[1])
                if n is not None and n != b["numoutlets"]:
                    errs.append("%s: %s declares %d outlets, box has %d" % (where, words[1], n, b["numoutlets"]))
                ni = re.search(r"\binlets\s*=\s*(\d+)", open(os.path.join(root, "max", "devices", words[1])).read())
                if ni and int(ni.group(1)) != b["numinlets"]:
                    errs.append("%s: %s declares %d inlets, box has %d (patchcord inlet out of range)" % (where, words[1], int(ni.group(1)), b["numinlets"]))
        if words[0] == "route" and b["numoutlets"] != len(words):
            errs.append("%s: route outlet count %d != %d" % (where, b["numoutlets"], len(words)))
        if words[0] in ("t", "trigger") and b["numoutlets"] != len(words) - 1:
            errs.append("%s: trigger outlet count" % where)
        if words[0] == "unpack" and b["numoutlets"] != len(words) - 1:
            errs.append("%s: unpack outlet count" % where)
        if words[:2] == ["prepend", "setparam"] and len(words) > 2 and int(words[2]) not in schema_ids:
            errs.append("%s: setparam id %s not in schema" % (where, words[2]))
        if words[0] == "setparam" and len(words) > 1 and words[1].isdigit() and int(words[1]) not in schema_ids:
            errs.append("%s: setparam id %s not in schema" % (where, words[1]))
    for l in p.get("lines", []):
        l = l["patchline"]
        (sa, so), (da, di) = l["source"], l["destination"]
        if sa not in ids or da not in ids:
            errs.append("%s: wire references missing id %s -> %s" % (where, sa, da)); continue
        if so >= ids[sa]["numoutlets"]:
            errs.append("%s: wire from %s outlet %d (has %d)" % (where, sa, so, ids[sa]["numoutlets"]))
        if di >= ids[da]["numinlets"]:
            errs.append("%s: wire to %s inlet %d (has %d)" % (where, da, di, ids[da]["numinlets"]))
    return errs

def main(root):
    bad = 0; n = 0
    schema_ids = {e[0] for e in json.load(open(os.path.join(root, "max", "devices", "schema.json")))["schema"]}
    for path in sorted(glob.glob(os.path.join(root, "max", "**", "*.maxpat"), recursive=True) + glob.glob(os.path.join(root, "max", "**", "*.amxd"), recursive=True)):
        n += 1
        try:
            d = patcher_json(path)
        except Exception as e:
            print("FAIL %s: unreadable (%s)" % (path, e)); bad += 1; continue
        for b in boxes(d):
            first = (b.get("text") or "").strip().split(" ")[0]
            cls = b.get("maxclass", "")
            if first in BAD or cls in BAD:
                print("FAIL %s: hardware/network object '%s' (maxclass %s) in a saved patcher" % (path, first or cls, cls)); bad += 1
        if "_attic" in path or os.sep + "dryrun" + os.sep in path:
            continue
        for where, p in walk(d["patcher"]):
            for e in check_patcher(root, os.path.basename(path) + ":" + where, p, schema_ids, nested=(where != "top")):
                print("FAIL", e); bad += 1
    devs = os.path.join(root, "max", "devices")
    for f in sorted(glob.glob(os.path.join(devs, "*.js"))):          # Tasks only where a timer is the design
        src = open(f).read()
        if re.search(r"\bnew Task\b|\bsetInterval\b|\bsetTimeout\b", src) and os.path.basename(f) not in TASK_OK:
            print("FAIL %s: uses a timer (Task/setInterval) but is not one of %s" % (f, sorted(TASK_OK))); bad += 1
    names = sorted(os.path.splitext(os.path.basename(f))[0] for f in glob.glob(os.path.join(devs, "PMOR *.maxpat")))
    want = ["PMOR core", "PMOR lane control", "PMOR lane drums", "PMOR lane sampler", "PMOR lane synth"]
    if names != want:
        print("FAIL device set %s != %s" % (names, want)); bad += 1
    print("lint_devices: %d patcher(s) checked, %d problem(s)" % (n, bad))
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
