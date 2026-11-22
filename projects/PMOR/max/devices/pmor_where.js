// pmor_where.js -- decides where pmor.proto keeps pulled samples (spec: write to <Live set>/PMOR/, fall back to ~/Music/PMOR_Cache).
//   set folder  = nearest ancestor directory of the device file that holds an "Ableton Project Info" folder (a Live project root) or a *.als file.
//                 Ancestors named "User Library" (and everything above) are never considered: a device loaded from the User Library
//                 belongs to no set.
//   explicit    = message `setfolder <dir>` (sticky; wins), then the Live API probe (`get file_path` on live_set: Live 12 exposes none as far
//                 as known, so the answer is only post()ed and used if it ever is a path to an .als), then the device path.
//   result      = `sessiondir <set folder>/PMOR`  or bare `sessiondir` (proto then falls back to ~/Music/PMOR_Cache).
//   Sent on load and on every `bang` (the core bangs it when `connect` is switched on, so a set saved after loading is picked up).
//   `resolve <path>` = same decision for an explicit device path (tests).  Everything decided is post()ed to the Max console.
inlets = 1; outlets = 1;
var explicitDir = "";

function normPath(fp) { return String(fp || "").replace(/^[^\/:][^\/]*:\//, "/"); }   // "Macintosh HD:/Users/.." (Max) -> "/Users/.."
// pure: nearest ancestor (of the file fp) that is a Live project root or holds a .als; "" if none. ls(dir) -> child names or null
function findSetFolder(fp, ls) {
    fp = normPath(fp);
    if (!fp) return "";
    var parts = fp.split("/");
    parts.pop();                                                // the file itself
    while (parts.length > 1) {
        if (parts[parts.length - 1] === "User Library") return "";
        var dir = parts.join("/"), kids = ls(dir);
        if (kids) for (var i = 0; i < kids.length; i++) if (kids[i] === "Ableton Project Info" || /\.als$/i.test(kids[i])) return dir;
        parts.pop();
    }
    return "";
}
function sessionFor(setfolder) { return setfolder ? String(setfolder).replace(/\/+$/, "") + "/PMOR" : ""; }
function resolveSession(fp, ls) { return sessionFor(findSetFolder(fp, ls)); }
function dirname(p) { p = normPath(p); var i = p.lastIndexOf("/"); return i > 0 ? p.substring(0, i) : ""; }

function lsdir(dir) {
    var f = new Folder(dir), out = [];
    if (!f.pathname) return null;
    f.reset();
    while (!f.end) { out.push(String(f.filename)); f.next(); }
    f.close();
    return out;
}
function liveFilePath() {   // defensive probe: no known property, so only report what comes back
    try {
        if (typeof LiveAPI === "undefined") return "";
        var a = new LiveAPI(null, "live_set");
        if (!a || a.id == 0) { post("pmor_where: liveapi live_set not available (not in Live)\n"); return ""; }
        var r = a.get("file_path");
        post("pmor_where: liveapi live_set get file_path -> " + JSON.stringify(r) + "\n");
        var s = (r && r.length) ? String(r[0]) : "";
        return /\.als$/i.test(s) ? s : "";
    } catch (e) { post("pmor_where: liveapi probe error " + e + "\n"); return ""; }
}
function emit(fp) {
    var how = "", set = "";
    if (explicitDir) { set = explicitDir; how = "setfolder message"; }
    if (!set) { var lp = liveFilePath(); if (lp) { set = dirname(lp); how = "liveapi " + lp; } }
    if (!set) { set = findSetFolder(fp, lsdir); how = "device path"; }
    var d = sessionFor(set);
    post("pmor_where: device=" + fp + " setfolder=" + (set || "(none)") + " via " + how + " -> " + (d ? "sessiondir " + d : "no set, proto uses ~/Music/PMOR_Cache") + "\n");
    if (d) outlet(0, "sessiondir", d); else outlet(0, "sessiondir");
}
function loadbang() { bang(); }
function bang() { emit(this.patcher.filepath || ""); }
function resolve(p) { emit(String(p)); }
function setfolder(p) { explicitDir = String(p || "").replace(/\/+$/, ""); bang(); }
function liveapi() { var lp = liveFilePath(); post("pmor_where: liveapi probe -> " + (lp || "(none)") + "\n"); }
if (typeof module !== "undefined") module.exports = { resolveSession: resolveSession, findSetFolder: findSetFolder, sessionFor: sessionFor };   // node tests (tests/test_where.js)
