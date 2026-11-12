#!/usr/bin/env python3
"""make_ui_test.py OUT.maxpat -- dials s8/s9 (retargeted by pmor_ui.js), p2052/p2053 dials and a umenu p4137 (lo=1) driven by `param <id> <value>`;
each control is banged after the update and its output logged as `<name> <value>`."""
import json, sys
n = [0]; boxes = []; lines = []
def add(cls, text, x, y, outs=1, ins=1, **k):
    n[0] += 1; i = "obj-%d" % n[0]; d = {"id": i, "maxclass": cls, "numinlets": ins, "numoutlets": outs, "patching_rect": [x, y, 120, 22], "outlettype": [""] * outs}
    if text: d["text"] = text
    d.update(k); boxes.append({"box": d}); return i
def wire(a, o, b, i): lines.append({"patchline": {"source": [a, o], "destination": [b, i]}})
log = add("newobj", "js logger.js", 600, 500, outs=0); ui = add("newobj", "js pmor_ui.js", 300, 200, outs=3)
def dial(name, lo, hi, flt):
    a = dict(min=lo, size=1001, mult=(hi - lo) / 1000, floatoutput=1) if flt else dict(min=lo, size=int(hi - lo) + 1, mult=1.0, floatoutput=0)
    return add("dial", None, 20, 40, varname=name, outlettype=["float"], **a)
tg = {"s8": dial("s8", 0, 4, 0), "s9": dial("s9", 0, 1, 1), "p2052": dial("p2052", 0, 2, 1), "p2053": dial("p2053", 0, 23, 0),
      "p4137": add("umenu", None, 20, 80, outs=3, outlettype=["int", "", ""], items=["1", ",", "2", ",", "3", ",", "4"], varname="p4137")}
sends = [("param 1032 2200", "s8"), ("param 1033 0.25", "s9"), ("param 2052 1.0", "p2052"), ("param 2053 17", "p2053"), ("param 4137 3", "p4137")]
lb = add("newobj", "loadbang", 600, 20)
for i, (m, name) in enumerate(sends):
    mb = add("message", m, 300, 100, ins=2); d1 = add("newobj", "delay %d" % (500 + i * 50), 600, 40); wire(lb, 0, d1, 0); wire(d1, 0, mb, 0); wire(mb, 0, ui, 0)
    pre = add("newobj", "prepend " + name, 300, 300); bn = add("message", "bang", 700, 100, ins=2); d2 = add("newobj", "delay %d" % (1200 + i * 50), 600, 80)
    wire(lb, 0, d2, 0); wire(d2, 0, bn, 0); wire(bn, 0, tg[name], 0); wire(tg[name], 0, pre, 0); wire(pre, 0, log, 0)
dl = add("newobj", "delay 2500", 600, 60); dn = add("message", "done", 600, 100, ins=2); wire(lb, 0, dl, 0); wire(dl, 0, dn, 0); wire(dn, 0, log, 0)
json.dump({"patcher": {"fileversion": 1, "appversion": {"major": 9, "minor": 0, "revision": 8, "architecture": "x64", "modernui": 1}, "classnamespace": "box",
           "rect": [60, 80, 900, 500], "boxes": boxes, "lines": lines}}, open(sys.argv[1], "w"))
