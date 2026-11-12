// pmor_edit.js -- opens a device's editor window ON REQUEST and tracks whether it is open. One instance per device (main patcher).
//   `show` (EXPAND button on): front the subpatcher named `editor`, restore its remembered size / position, tell its gated helpers (js boxes named ui / br / bank) `editor 1`,
//     ask for fresh state (outlet 0 -> [send pmor_tx]: dumpparams, and with the argument `full` also pool + presets), report `editor 1` on outlet 1.
//   `hide` (MINIMIZE): close the window (wclose) and do the same as when the user closes it.
//   While the window is open a [metro 500] (created on `show`, removed on close; Task never fires in Live's Max 9.1.5, see pmor_session.js) checks its visibility and records its
//   rectangle (wind.location): changes go out of outlet 3 as `R~left~top~right~bottom` (one symbol -> a pattr saved in the Live set); `rectload <text>` (the pattr's output) restores it.
//   Nothing runs while the editor is closed.
// Outlets: 0 messages for pmor_tx, 1 `editor 0|1` (core: session logic; control lane: its meters), 2 `set 0|1` -> the EXPAND / MINIMIZE button (silent), 3 rect text -> pattr.
inlets = 1; outlets = 4;
var full = false, isOpen = 0, ticker = null, grace = 0, rect = "", lastRect = "";
for (var i = 1; i < jsarguments.length; i++) if (String(jsarguments[i]) === "full") full = true;

function ed() { return this.patcher.getnamed("editor"); }
function sub() { var b = ed(); return b ? b.subpatcher() : null; }
function tell(v) {                               // gated helper js objects inside the editor
    var p = sub(); if (!p) return;
    var o = p.firstobject;
    while (o) {
        var n = o.varname;
        if (n === "ui" || n === "br" || n === "bank") o.message("editor", v);
        o = o.nextobject;
    }
}
function visible() { var p = sub(); return !!(p && p.wind && p.wind.visible); }
function readRect() {
    var p = sub(); if (!p || !p.wind) return "";
    try { var l = p.wind.location; if (l && l.length === 4 && l[2] - l[0] > 100 && l[3] - l[1] > 100) return "R~" + l.join("~"); } catch (e) { }
    return "";
}
function rectload(t) { var f = String(t).split("~"); rect = (f[0] === "R" && f.length === 5) ? String(t) : ""; lastRect = rect; }
function applyRect() {
    var p = sub(); if (!p || !p.wind || !rect) return;
    var f = rect.split("~"), n = [f[1] | 0, f[2] | 0, f[3] | 0, f[4] | 0];
    if (n[2] - n[0] < 200 || n[3] - n[1] < 100) return;
    try { p.wind.setlocation(n[0], n[1], n[2], n[3]); } catch (e) { }
}
function startTicker() {
    if (ticker) return;
    try { ticker = this.patcher.newdefault(600, 20, "metro", 500); this.patcher.connect(ticker, 0, this.box, 0); ticker.message("int", 1); } catch (e) { ticker = null; }
}
function stopTicker() { if (!ticker) return; try { ticker.message("int", 0); this.patcher.remove(ticker); } catch (e) { } ticker = null; }
function show() {
    var b = ed(); if (!b) { error("pmor_edit: no subpatcher named editor\n"); return; }
    try { b.message("vis"); } catch (e) { }             // (re)opens a window the user closed by hand
    try { var p = sub(); if (p) p.front(); applyRect(); } catch (e2) { post("pmor_edit: front failed " + e2 + "\n"); }
    if (isOpen) return;
    isOpen = 1; grace = 3;                        // the window needs a moment to become visible
    tell(1);
    outlet(2, "set", 1);
    outlet(1, "editor", 1);
    outlet(0, "dumpparams");
    if (full) { outlet(0, "pool"); outlet(0, "presets"); }
    startTicker();
}
function hide() {
    var b = ed();
    if (b) { try { b.message("wclose"); } catch (e) { } }
    closed();
}
function bang() {                                // metro tick (only while open)
    if (!isOpen) { stopTicker(); return; }
    if (grace > 0 && !visible()) { grace--; return; }
    grace = 0;
    if (!visible()) { closed(); return; }
    var r = readRect();
    if (r && r !== lastRect) { lastRect = rect = r; outlet(3, r); }
}
function closed() {
    stopTicker();
    if (!isOpen) return;
    isOpen = 0;
    tell(0);
    outlet(2, "set", 0);
    outlet(1, "editor", 0);
}
function status() { post("pmor_edit: open=" + isOpen + " visible=" + visible() + " rect=" + rect + "\n"); }
