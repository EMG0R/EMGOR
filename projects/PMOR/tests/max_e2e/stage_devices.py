#!/usr/bin/env python3
"""stage_devices.py SRC_DIR DST_DIR -- copies max/devices (maxpat, js, schema) into DST_DIR for the headless run.
If the shipped core still wires the port messages as `open <letter>` / `close` (they are [js] built-ins and never reach pmor_port.js),
the STAGED core copy is rewritten to openport/closeport so the rest of the link can still be tested; prints PORTBUG=1 in that case
(driver logs a FAIL for the shipped device). Everything else is a byte-for-byte copy."""
import json, os, shutil, sys
src, dst = sys.argv[1:3]
os.makedirs(dst, exist_ok=True)
bug = 0; bug2 = 0
for f in sorted(os.listdir(src)):
    p = os.path.join(src, f)
    if not os.path.isfile(p) or not f.endswith((".maxpat", ".js", ".json")):
        continue
    if f == "PMOR core.maxpat":
        d = json.load(open(p))
        def walk(pt):
            global bug
            for b in pt["boxes"]:
                b = b["box"]
                if b.get("text") == "prepend open": b["text"] = "prepend openport"; bug = 1
                if b.get("maxclass") == "message" and b.get("text") == "close": b["text"] = "closeport"; bug = 1
                if isinstance(b.get("patcher"), dict): walk(b["patcher"])
        walk(d["patcher"])
        # core wires `synced -> [t b b] -> [pool]` back into pmor.proto: measured in Max 9.1.4 this floods ~560 `synced` messages in 75 ms and the
        # client never reaches `ready` (see README of this test). Drop that message box in the STAGED copy so the rest can run; flagged as SYNCBUG.
        pt = d["patcher"]; byid = {b["box"]["id"]: b["box"] for b in pt["boxes"]}
        for b in list(pt["boxes"]):
            bx = b["box"]
            if bx.get("maxclass") == "message" and bx.get("text") == "pool":
                srcs = [l["patchline"]["source"][0] for l in pt["lines"] if l["patchline"]["destination"][0] == bx["id"]]
                if any((byid[x].get("text") or "").startswith("t b b") for x in srcs):
                    pt["boxes"].remove(b); pt["lines"] = [l for l in pt["lines"] if bx["id"] not in (l["patchline"]["source"][0], l["patchline"]["destination"][0])]; bug2 = 1
        json.dump(d, open(os.path.join(dst, f), "w"))
    else:
        shutil.copyfile(p, os.path.join(dst, f))
print("PORTBUG=%d; SYNCBUG=%d" % (bug, bug2))
