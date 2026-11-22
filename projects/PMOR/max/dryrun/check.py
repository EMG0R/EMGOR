#!/usr/bin/env python3
"""check.py -- file validators used by run.sh (stdlib only, never touches Live/Max).
   check.py amxd  <file.amxd>             container parse: 'ampf' + type + 'meta' + 'ptch' + JSON
   check.py als   <file.als> <file.amxd>  gunzip + XML parse + structure + recorded device size/CRC match the .amxd on disk
   check.py result <result.json> [--require-live]   schema + verdict; exit 0 only if ok (and inside_live with --require-live)
   check.py sample                        schema keys present in pmor_dryrun.js
"""
import gzip, json, os, struct, sys
from xml.dom import minidom

HERE = os.path.dirname(os.path.abspath(__file__))
REQUIRED = ["schema", "ok", "inside_live", "max_ok", "live_ok", "checks", "max_checks", "live_checks", "errors", "device", "set_folder",
            "session_dir", "externals", "helpers", "live", "max", "outfile"]


def crc16(data):
    c = 0
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x8005) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def amxd(path):
    d = open(path, "rb").read()
    assert d[:4] == b"ampf" and struct.unpack("<I", d[4:8])[0] == 4, "bad ampf header"
    kind = d[8:12].decode()
    assert kind in ("aaaa", "iiii", "mmmm"), "bad type " + kind
    assert d[12:16] == b"meta" and struct.unpack("<I", d[16:20])[0] == 4, "no meta chunk"
    assert d[24:28] == b"ptch", "no ptch chunk"
    n = struct.unpack("<I", d[28:32])[0]
    assert 32 + n == len(d), "ptch length %d != remaining %d" % (n, len(d) - 32)
    j = json.loads(d[32:].rstrip(b"\0").decode("utf-8"))
    p = j["patcher"]
    assert struct.pack(">I", p["project"]["amxdtype"]).decode() == kind, "amxdtype != header type"
    return "amxd ok: type=%s %d bytes, %d boxes, %d lines" % (kind, len(d), len(p["boxes"]), len(p["lines"]))


def als(path, amxd_path):
    raw = gzip.open(path, "rb").read()
    dom = minidom.parseString(raw)
    n = lambda t: len(dom.getElementsByTagName(t))
    assert n("MidiTrack") == 1 and n("MxDeviceInstrument") == 1, "expected 1 MIDI track + 1 MxDeviceInstrument"
    assert n("AudioTrack") == 0 and n("ReturnTrack") == 0
    ref = dom.getElementsByTagName("MxPatchRef")[0]
    val = lambda t: ref.getElementsByTagName(t)[0].getAttribute("Value")
    data = open(amxd_path, "rb").read()
    assert os.path.abspath(val("Path")) == os.path.abspath(amxd_path), "set points at %s, not %s" % (val("Path"), amxd_path)
    rel = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(path)), val("RelativePath")))
    assert rel == os.path.abspath(amxd_path), "RelativePath resolves to " + rel
    assert int(val("OriginalFileSize")) == len(data) and int(val("OriginalCrc")) == crc16(data), "recorded size/CRC stale: rerun make_set.py"
    return "als ok: gzip+xml, 1 MidiTrack, 1 MxDeviceInstrument, path+size+crc match; %s" % val("Path")


def result(path, require_live):
    r = json.load(open(path))
    miss = [k for k in REQUIRED if k not in r]
    assert not miss, "missing keys " + ",".join(miss)
    for c in r["checks"]:
        assert set(("group", "name", "ok", "detail")) <= set(c), "bad check row"
    bad = [c["name"] for c in r["checks"] if not c["ok"]]
    print("ok=%s inside_live=%s max_ok=%s live_ok=%s failed=%s errors=%s" % (r["ok"], r["inside_live"], r["max_ok"], r["live_ok"], bad, r["errors"]))
    if require_live and not r["inside_live"]:
        print("LiveAPI unavailable: the device did not run inside Live")
        return 4
    return 0 if r["ok"] is True else 1


def sample():
    js = open(os.path.join(HERE, "pmor_dryrun.js")).read()
    missing = [k for k in REQUIRED if ("R." + k) not in js and (k + ":") not in js]
    assert not missing, "pmor_dryrun.js never sets " + ",".join(missing)
    return "result schema: all %d keys produced by pmor_dryrun.js" % len(REQUIRED)


if __name__ == "__main__":
    try:
        c = sys.argv[1]
        if c == "amxd": print(amxd(sys.argv[2]))
        elif c == "als": print(als(sys.argv[2], sys.argv[3]))
        elif c == "result": sys.exit(result(sys.argv[2], "--require-live" in sys.argv))
        elif c == "sample": print(sample())
        else: sys.exit("usage")
    except (AssertionError, OSError, ValueError, KeyError) as e:
        print("FAIL:", e)
        sys.exit(1)
