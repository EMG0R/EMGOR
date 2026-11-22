// pmor_snap.js -- the set <-> PMOR patch sync line of the PMOR core. The Live set holds a copy of the CURRENT patch's params (pmor_host.h "Host snapshot", stored in a pattr).
// Inlet: snapstate <kind> a b c d e   (pmor.proto outlet 2: adopted | pushing | synced | patchdiff | cleared)      snapshot <text>   (the stored copy: PS1~patch~ids~id:val/..)
//        writearm / writego         press / release of the HOLD button: `writego` writes only when >= 2 s after `writearm` (a click does nothing)
// Outlets: 0 `set <text>` -> status comment (varname snapline)   1 `set <text>` -> hint comment (snaphint)   2 snapadopt | snapwrite -> pmor.proto
// Shows the two buttons (varnames snap_adopt, snap_write) and the hint ONLY on `patchdiff` (PMOR is on another patch than the set): there nothing is written until a button is used.
//   adopt = the set takes PMOR's patch.  load on PMOR = write the set's params onto PMOR's CURRENT patch (the firmware has no patch-switch command).
inlets = 1; outlets = 3;
var armedAt = 0, held = null, shown = 0, state = "";
function o(n) { return this.patcher.getnamed(n); }
function say(t) { outlet(0, "set", t); }
function hint(t) { outlet(1, "set", t); }
function buttons(on) {
    on = on ? 1 : 0; shown = on;
    var a = o("snap_adopt"), w = o("snap_write");
    if (a) a.hidden = on ? 0 : 1;
    if (w) w.hidden = on ? 0 : 1;
    if (!on) hint("");
}
function snapshot(t) {                                   // the stored copy (from the Live set or after a change): shown until PMOR reports
    var f = String(t).split("~");
    if (f[0] !== "PS1" || f.length < 3) { held = null; return; }
    held = { patch: f[1] | 0, n: f[2] ? f[2].split("/").length : 0 };
    if (state === "" ) say("set holds patch " + held.patch + " (" + held.n + " params), waiting for PMOR");
}
function snapstate(kind, a, b, c, d, e) {
    kind = String(kind); state = kind; a = a | 0; b = b | 0; c = c | 0; d = d | 0; e = e | 0;
    buttons(kind === "patchdiff");
    if (kind === "adopted") say("set took PMOR's patch " + b + " (" + a + " params)");
    else if (kind === "pushing") say("writing the set to PMOR: " + a + " of " + b + " left");
    else if (kind === "synced") say("synced: " + a + " params written to PMOR patch " + d + (b ? ", " + b + " clamped by PMOR" : "") + (c ? ", " + c + " sample slots" : "") + (e ? ", " + e + " not in pool" : ""));
    else if (kind === "patchdiff") { say("PMOR is on patch " + a + ", the set holds patch " + b); hint("load on PMOR = write the set's params onto PMOR's current patch (" + a + ")"); }
    else if (kind === "cleared") { say("set copy cleared"); state = ""; }
}
function writearm() { armedAt = Date.now(); }
function writego() {
    if (!shown) return;
    if (Date.now() - armedAt >= 2000) outlet(2, "snapwrite"); else hint("hold the button 2 s: load on PMOR = write the set's params onto PMOR's current patch");
}
function adopt() { if (shown) outlet(2, "snapadopt"); }
function anything() { }
if (typeof module !== "undefined") module.exports = { snapstate: snapstate, snapshot: snapshot, writearm: writearm, writego: writego, adopt: adopt };
