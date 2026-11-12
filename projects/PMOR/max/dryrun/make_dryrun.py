#!/usr/bin/env python3
"""make_dryrun.py -- writes max/dryrun/'PMOR dryrun.maxpat' (editor source) and 'PMOR dryrun.amxd' (Max Instrument container).
   python3 make_dryrun.py
Patcher: [js pmor_dryrun.js] (starts itself with a Task); [live.thisdevice]->[prepend thisdevice]->js; [plugout~] x2 (instrument).
Container = same layout as ~/Documents/_EMGOR_SYNTH/_M4L/CHORUS.amxd and max/devices/make_devices.py amxd(): 'ampf' + 'iiii' + 'meta' + 'ptch'."""
import json, os, struct

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "PMOR dryrun"
KIND = "iiii"          # Max Instrument (the template set's MIDI track carries an MxDeviceInstrument)


def box(i, cls, x, y, w, h, text=None, ins=1, outs=1, otypes=None, **kw):
    b = {"id": "obj-%d" % i, "maxclass": cls, "numinlets": ins, "numoutlets": outs,
         "outlettype": otypes if otypes is not None else [""] * outs, "patching_rect": [x, y, w, h]}
    if text is not None:
        b["text"] = text
    b.update(kw)
    return {"box": b}


def build():
    boxes = [
        box(3, "newobj", 20, 130, 150, 22, "js pmor_dryrun.js", ins=1, outs=0, otypes=[]),
        box(4, "newobj", 200, 20, 100, 22, "live.thisdevice", ins=1, outs=3, otypes=["bang", "int", "int"]),
        box(5, "newobj", 200, 60, 110, 22, "prepend thisdevice", ins=1, outs=1, otypes=[""]),
        box(6, "newobj", 20, 200, 60, 22, "plugout~ 1", ins=1, outs=0, otypes=[]),
        box(7, "newobj", 100, 200, 60, 22, "plugout~ 2", ins=1, outs=0, otypes=[]),
    ]
    lines = [(4, 1, 5, 0), (5, 0, 3, 0)]
    kindnum = struct.unpack(">I", KIND.encode())[0]
    proj = {"version": 1, "creationdate": 0, "modificationdate": 0, "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1,
            "hideprojectwindow": 1, "showdependencies": 1, "autolocalize": 0, "contents": {"patchers": {}}, "layout": {},
            "searchpath": {}, "detailsvisible": 0, "amxdtype": kindnum, "readonly": 0,
            "devpathtype": 0, "devpath": ".", "sortmode": 0, "viewmode": 0, "includepackages": 0}
    return {"patcher": {
        "fileversion": 1, "appversion": {"major": 9, "minor": 0, "revision": 8, "architecture": "x64", "modernui": 1},
        "classnamespace": "box", "rect": [60.0, 80.0, 500.0, 320.0], "openinpresentation": 0, "bglocked": 0,
        "devicewidth": 120.0, "openrect": [0.0, 0.0, 120.0, 40.0],
        "boxes": boxes, "lines": [{"patchline": {"source": [a, ao], "destination": [b, bi]}} for a, ao, b, bi in lines],
        "project": proj, "dependency_cache": [], "parameters": {}, "is_mpe": 0, "latency": 0,
        "minimum_live_version": "", "minimum_max_version": "", "platform_compatibility": 0, "autosave": 0}}


def amxd(patcher_json, kind):
    body = (json.dumps(patcher_json, indent=2) + "\n").encode() + b"\0"
    return (b"ampf" + struct.pack("<I", 4) + kind.encode() + b"meta" + struct.pack("<I", 4) + struct.pack("<I", 0)
            + b"ptch" + struct.pack("<I", len(body)) + body)


if __name__ == "__main__":
    d = build()
    txt = json.dumps(d, indent=2) + "\n"
    json.loads(txt)
    open(os.path.join(HERE, NAME + ".maxpat"), "w").write(txt)
    open(os.path.join(HERE, NAME + ".amxd"), "wb").write(amxd(d, KIND))
    print("wrote", NAME + ".maxpat", NAME + ".amxd")
