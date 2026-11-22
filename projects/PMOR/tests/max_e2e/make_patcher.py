#!/usr/bin/env python3
"""make_patcher.py OUT.maxpat <scenario> <logfile> <simport> <simlog> <devdir> <cachedir> <portbug 0|1> <syncbug 0|1>
Writes the (never committed) test patcher: loadbang -> delay -> `go` -> [js driver.js ...]; the global streams the devices
broadcast (pmor_rx, pmor_tx) and the spy send e2e_st (proto outlet 2) are forwarded into the driver."""
import json, sys
out, scn, log, port, simlog, dev, cache, portbug, syncbug = sys.argv[1:10]
boxes, lines = [], []
def add(cls, text, x, y, outs=1, ins=1, **k):
    i = "obj-%d" % (len(boxes) + 1)
    d = {"id": i, "maxclass": cls, "numinlets": ins, "numoutlets": outs, "patching_rect": [x, y, 150, 22], "outlettype": [""] * outs, "text": text}
    d.update(k); boxes.append({"box": d}); return i
def wire(a, o, b, i): lines.append({"patchline": {"source": [a, o], "destination": [b, i]}})
js = add("newobj", "js driver.js %s %s %s %s %s %s %s %s" % (scn, log, port, simlog, dev, cache, portbug, syncbug), 20, 300, outs=0)
lb = add("newobj", "loadbang", 20, 20, outlettype=["bang"]); dl = add("newobj", "delay 1500", 20, 50, outlettype=["bang"]); go = add("message", "go", 20, 80, ins=2)
wire(lb, 0, dl, 0); wire(dl, 0, go, 0); wire(go, 0, js, 0)
for i, (r, tag) in enumerate([("e2e_st", "stmsg"), ("pmor_rx", "rx"), ("pmor_tx", "tx"), ("pmor_ui_status", "uis")]):
    rv = add("newobj", "receive " + r, 220 + i * 160, 20); pp = add("newobj", "prepend " + tag, 220 + i * 160, 60, ins=2); wire(rv, 0, pp, 0); wire(pp, 0, js, 0)
json.dump({"patcher": {"fileversion": 1, "appversion": {"major": 9, "minor": 1, "revision": 4, "architecture": "arm64", "modernui": 1}, "classnamespace": "box",
           "rect": [60, 80, 700, 400], "boxes": boxes, "lines": lines}}, open(out, "w"))
