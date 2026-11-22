// test helper (see make_gate_test.py): closeit = close the editor window of the test device; dump = log ignoreclick + colour of the compat-test controls
inlets = 1; outlets = 1;
function closeit() { this.patcher.getnamed("editor").message("wclose"); }   // closes the window like the close box (dispose would delete the subpatcher)
function r(a) { var o = []; for (var i = 0; i < a.length; i++) o.push(Math.round(a[i] * 100) / 100); return o.join(","); }
function dump() {
    var d = this.patcher.getnamed("p2054"), t = this.patcher.getnamed("p2055"), c = this.patcher.getnamed("unknown");
    outlet(0, "ATTR", "p2054", "ignoreclick", d.getboxattr("ignoreclick"), "needlecolor", r(d.getattr("needlecolor")));
    outlet(0, "ATTR", "p2055", "ignoreclick", t.getboxattr("ignoreclick"), "checkedcolor", r(t.getattr("checkedcolor")));
}
