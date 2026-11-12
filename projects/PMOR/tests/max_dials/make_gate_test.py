#!/usr/bin/env python3
"""make_gate_test.py OUT.maxpat -- headless proof that an editor draws nothing while closed.
Part 1 (pmor_ui.js gated): counters named p2052/p2053 stand in for controls; every `set` they receive is logged as `SET <tag> <v>`.
  param stream while closed -> 0 SETs; `editor 1` -> one replay of the cached values; stream while open -> live SETs; `editor 0` -> silent again.
Part 2 (pmor_browser.js gated): preset messages while closed -> 0 outlet lines (B0 ...); `editor 1` -> redraw.
Part 3 (pmor_edit.js): `show` opens the [p editor] window -> `E editor 1` + `E dumpparams` + `E pool` + `E presets`; closing the window (thispatcher) -> `E editor 0` within the poll."""
import json, sys
n = [0]; boxes = []; lines = []
def add(cls, text, x, y, outs=1, ins=1, **k):
    n[0] += 1; i = "obj-%d" % n[0]
    d = {"id": i, "maxclass": cls, "numinlets": ins, "numoutlets": outs, "patching_rect": [x, y, 120, 22], "outlettype": [""] * outs}
    if text: d["text"] = text
    d.update(k); boxes.append({"box": d}); return i
def wire(a, o, b, i): lines.append({"patchline": {"source": [a, o], "destination": [b, i]}})
log = add("newobj", "js logger.js", 900, 700, outs=0)
ui = add("newobj", "js pmor_ui.js gated", 300, 200, outs=3, varname="ui")
add("newobj", "js counter.js p2052", 20, 40, outs=0, varname="p2052"); add("newobj", "js counter.js p2053", 20, 80, outs=0, varname="p2053")
br = add("newobj", "js pmor_browser.js 4 gated", 500, 200, outs=4, varname="br")
b0 = add("newobj", "prepend B0", 500, 260, ins=2); wire(br, 0, b0, 0); wire(b0, 0, log, 0)
# part 3: device with an editor subpatcher
sub = {"fileversion": 1, "classnamespace": "box", "rect": [100, 100, 300, 200], "boxes": [
    {"box": {"id": "obj-1", "maxclass": "newobj", "numinlets": 1, "numoutlets": 1, "outlettype": [""], "patching_rect": [10, 10, 100, 22], "text": "thispatcher", "varname": "tp"}}], "lines": []}
ed = add("newobj", "p editor", 700, 100, ins=0, outs=0, patcher=sub, varname="editor")
ej = add("newobj", "js pmor_edit.js full", 700, 150, outs=2)
e0 = add("newobj", "prepend E", 700, 200, ins=2); e1 = add("newobj", "prepend E", 800, 200, ins=2)
wire(ej, 0, e0, 0); wire(ej, 1, e1, 0); wire(e0, 0, log, 0); wire(e1, 0, log, 0)
e2r = add("newobj", "route set", 880, 160, outs=2); e2 = add("newobj", "prepend S", 900, 200, ins=2); wire(e2r, 0, e2, 0); e3 = add("newobj", "prepend R", 1000, 200, ins=2)
wire(ej, 2, e2r, 0); wire(ej, 3, e3, 0); wire(e2, 0, log, 0); wire(e3, 0, log, 0)
tester = add("newobj", "js tester.js", 700, 300, outs=1); wire(tester, 0, log, 0)
add("dial", None, 20, 120, outs=1, varname="p2054", outlettype=["float"], needlecolor=[0.6, 0.9, 0.6, 1.0], outlinecolor=[0.6, 0.9, 0.6, 1.0])
add("toggle", None, 60, 120, outs=1, varname="p2055", outlettype=["int"], checkedcolor=[0.6, 0.9, 0.6, 1.0], elementcolor=[0.6, 0.9, 0.6, 1.0])
add("comment", "x", 100, 120, outs=0, varname="unknown"); add("umenu", None, 140, 120, outs=3, outlettype=["int", "", ""], varname="unk", items=["(none)"])
lb = add("newobj", "loadbang", 600, 20)
def at(ms, text, dst, cls="message"):
    m = add(cls, text, 300, 100, ins=2); d = add("newobj", "delay %d" % ms, 600, 40); wire(lb, 0, d, 0); wire(d, 0, m, 0); wire(m, 0, dst, 0)
def mark(ms, name): at(ms, "mark " + name, log)
at(500, "param 2052 1.0", ui); at(520, "param 2053 17", ui)
at(540, "presetbegin", br); at(545, "preset 4 0 f Bell", br); at(550, "preset 4 1 u Lead", br); at(555, "presetend", br)
mark(800, "closed_end")
at(900, "editor 1", ui); at(900, "editor 1", br)
mark(1100, "replay_end")
at(1200, "param 2052 0.5", ui)
at(1300, "schemabegin 4", ui); at(1301, "schema 2052 A 0 1 0 0 G", ui); at(1302, "schema 2054 B 0 1 0 0 G", ui); at(1303, "schema 777001 NEWX 0 1 0 0 G", ui)
at(1304, "schema 2053 C 0 1 0 0 G", ui); at(1305, "schemaend 4", ui); at(1350, "param 777001 0.5", ui); at(1380, "dump", tester)
mark(1400, "open_end")
at(1500, "editor 0", ui); at(1500, "editor 0", br)
at(1600, "param 2052 0.25", ui); at(1605, "presetbegin", br); at(1610, "preset 4 0 f X", br); at(1615, "presetend", br)
mark(1900, "closed2_end")
at(2000, "show", ej)
mark(3000, "shown_end")
at(2500, "status", ej); at(3100, "closeit", tester); at(3600, "status", ej); at(5000, "status", ej)
mark(6000, "closed_window_end")
at(6200, "show", ej)
mark(7800, "reshown_end")
at(7900, "hide", ej)
mark(8800, "hidden_end")
at(9000, "done", log)
json.dump({"patcher": {"fileversion": 1, "appversion": {"major": 9, "minor": 0, "revision": 8, "architecture": "x64", "modernui": 1}, "classnamespace": "box",
           "rect": [60, 80, 900, 500], "boxes": boxes, "lines": lines}}, open(sys.argv[1], "w"))
