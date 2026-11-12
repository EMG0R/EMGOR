// pmor_dryrun.js -- logic of "PMOR dryrun.amxd". Runs once, ~1.5-7.5 s after the device loads, then writes
//   /tmp/pmor_dryrun_result.json (the result) and afterwards /tmp/pmor_dryrun_done (the shell polls for this one).
// Never opens a serial/udp port, never sends `hello`, never shows a dialog. ES5 (Max js).
// Messages: bang (run now), thisdevice <n> (from live.thisdevice), outfile <path>, donefile <path>.
inlets = 1; outlets = 0;
var OUT = "/tmp/pmor_dryrun_result.json", DONE = "/tmp/pmor_dryrun_done";
var HELPERS = ["pmor_ui.js", "pmor_lane.js", "pmor_port.js", "pmor_where.js", "pmor_browser.js", "schema.json"];
var thisdeviceFired = false, ran = false;
var JS = this;   // `this` is not the jsthis inside anonymous functions; capture it once

function outfile(p) { OUT = String(p); }
function donefile(p) { DONE = String(p); }
function thisdevice(v) { thisdeviceFired = true; }
function bang() { run(); }   // manual trigger (message box)
// start: a Task created when the js loads (a [loadbang]->[delay] chain did not fire in a patcher carrying an amxd "project" block).
// Ticks every 1.5 s; runs as soon as LiveAPI answers, or after 5 ticks (7.5 s) when it never does (= outside Live).
var ticks = 0;
function liveUp() { try { var a = new LiveAPI(null, "live_set"); return !!a && String(a.id) !== "0"; } catch (e) { return false; } }
var startTask = new Task(function () {
    ticks++;
    if (ran) return;
    if (liveUp() || ticks >= 5) { post("pmor_dryrun: start at tick " + ticks + "\n"); run(); }
}, this);
startTask.interval = 1500; startTask.repeat(20);

// ---- set-folder resolution: copy of max/devices/pmor_where.js (snapshot 2026-11-09) -------------------------------------------------
function normPath(fp) { return String(fp || "").replace(/^[^\/:][^\/]*:\//, "/"); }
function findSetFolder(fp, ls) {
    fp = normPath(fp);
    if (!fp) return "";
    var parts = fp.split("/");
    parts.pop();
    while (parts.length > 1) {
        if (parts[parts.length - 1] === "User Library") return "";
        var dir = parts.join("/"), kids = ls(dir);
        if (kids) for (var i = 0; i < kids.length; i++) if (kids[i] === "Ableton Project Info" || /\.als$/i.test(kids[i])) return dir;
        parts.pop();
    }
    return "";
}
function sessionFor(setfolder) { return setfolder ? String(setfolder).replace(/\/+$/, "") + "/PMOR" : ""; }
function dirname(p) { p = normPath(p); var i = p.lastIndexOf("/"); return i > 0 ? p.substring(0, i) : ""; }
function lsdir(dir) {
    var f = new Folder(dir), out = [];
    if (!f.pathname) return null;
    f.reset();
    while (!f.end) { out.push(String(f.filename)); f.next(); }
    f.close();
    return out;
}
// -------------------------------------------------------------------------------------------------------------------------------------

var R = { schema: 1, ok: false, inside_live: false, checks: [], max_checks: [], live_checks: [], errors: [] };
function chk(group, name, ok, detail) {
    var c = { group: group, name: name, ok: !!ok, detail: (detail === undefined ? "" : String(detail)) };
    R.checks.push(c);
    (group === "live" ? R.live_checks : R.max_checks).push(c);
    post("pmor_dryrun: [" + group + "] " + name + " " + (ok ? "OK" : "FAIL") + " " + c.detail + "\n");
}
function guard(fn, label) { try { return fn(); } catch (e) { R.errors.push(label + ": " + e); post("pmor_dryrun: " + label + " threw " + e + "\n"); return undefined; } }

function probeObject(cls, msgs) {
    var info = { exists: false, maxclass: "", sent: [] };
    var box = null;
    try { box = JS.patcher.newdefault(10, 10, cls); } catch (e) { info.error = String(e); }
    if (box) {
        info.exists = true;
        try { info.maxclass = String(box.maxclass); } catch (e1) { }
        for (var i = 0; i < msgs.length; i++) {
            try { box.message.apply(box, msgs[i]); info.sent.push(msgs[i].join(" ")); } catch (e2) { info.error = String(e2); }
        }
        try { JS.patcher.remove(box); } catch (e3) { }
    }
    return info;
}

function probeFile(name) {
    var out = { found: false, folder: "" };
    try {
        var f = new File(name);
        if (f.isopen) { out.found = true; try { out.folder = String(f.foldername); } catch (e) { } f.close(); }
    } catch (e2) { out.error = String(e2); }
    return out;
}

function liveProbe() {
    var L = { api: "none" };
    if (typeof LiveAPI === "undefined") { L.note = "LiveAPI undefined"; return L; }
    var s = null;
    try { s = new LiveAPI(null, "live_set"); } catch (e) { L.note = "new LiveAPI threw " + e; return L; }
    var id = 0;
    try { id = s.id; } catch (e1) { }
    L.live_set_id = String(id);
    if (!s || String(id) === "0" || id === 0) { L.note = "live_set id 0: not inside Live (or Live set not ready)"; return L; }
    R.inside_live = true;
    L.api = "ok";
    try { var info = String(s.info); L.live_set_properties = (info.match(/^property\s+(\S+)/gm) || []).map(function (x) { return x.replace(/^property\s+/, ""); }); } catch (e2) { L.info_error = String(e2); }
    var props = ["is_playing", "tempo", "current_song_time"];
    for (var i = 0; i < props.length; i++) {
        (function (p) {
            var v;
            try { v = s.get(p); L[p] = (v && v.length !== undefined && typeof v !== "string") ? v[0] : v; } catch (e) { L[p + "_error"] = String(e); }
        })(props[i]);
    }
    var a = null;
    try { a = new LiveAPI(null, "live_app"); } catch (e3) { L.app_error = String(e3); }
    if (a && String(a.id) !== "0") {
        var parts = [];
        ["get_major_version", "get_minor_version", "get_bugfix_version"].forEach(function (fn) {
            try { var r = a.call(fn); parts.push(r && r.length !== undefined && typeof r !== "string" ? r[0] : r); } catch (e) { L.app_error = String(e); }
        });
        L.live_version = parts.join(".");
    }
    return L;
}

function writeText(path, text) {
    var f = new File(path, "write", "TEXT");
    if (!f.isopen) throw new Error("cannot open " + path);
    f.eof = 0;
    f.writestring(text);
    f.close();
}

function run() {
    if (ran) return;
    ran = true;
    var t0 = new Date();
    guard(function () {
        R.started = t0.toString();
        R.thisdevice_fired = thisdeviceFired;
        R.max = { version: String(max.version), os: String(max.os), osversion: String(max.osversion), searchpath: guard(function () { return String(max.getsystemsearchpath ? max.getsystemsearchpath() : ""); }, "searchpath") || "" };

        // (b) patcher location + set-folder resolution
        var fp = String(JS.patcher.filepath || "");
        R.device = { filepath: fp, name: String(JS.patcher.name || ""), dirname: dirname(fp) };
        chk("max", "patcher.filepath non-empty", fp !== "", fp);
        var kids = lsdir(R.device.dirname);
        R.device.dir_listing = kids || [];
        var sf = findSetFolder(fp, lsdir);
        R.set_folder = sf;
        R.session_dir = sessionFor(sf) || "~/Music/PMOR_Cache (fallback)";
        var alsHere = false;
        if (kids) for (var k = 0; k < kids.length; k++) if (/\.als$/i.test(kids[k])) alsHere = true;
        chk("max", "set-folder resolution", alsHere ? (sf === R.device.dirname) : true,
            "set_folder=" + sf + " session_dir=" + R.session_dir + (alsHere ? " (a .als sits next to the device: expected set_folder == dirname)" : " (no .als next to device: informational)"));

        // (a) externals
        R.externals = {};
        R.externals["pmor.proto"] = probeObject.call(this, "pmor.proto", [["sessiondir", sessionFor(sf) || R.device.dirname + "/PMOR"], ["pool"]]);
        R.externals["pmor.core~"] = probeObject.call(this, "pmor.core~", [["reseed", 0]]);
        chk("max", "pmor.proto instantiates", R.externals["pmor.proto"].exists, JSON.stringify(R.externals["pmor.proto"]));
        chk("max", "pmor.core~ instantiates", R.externals["pmor.core~"].exists, JSON.stringify(R.externals["pmor.core~"]));

        // (d) helper files on the search path
        R.helpers = {};
        for (var h = 0; h < HELPERS.length; h++) {
            R.helpers[HELPERS[h]] = probeFile(HELPERS[h]);
            chk("max", "search path resolves " + HELPERS[h], R.helpers[HELPERS[h]].found, R.helpers[HELPERS[h]].folder);
        }

        // (c) Live
        R.live = liveProbe();
        if (R.inside_live) {
            chk("live", "live_set reachable", R.live.api === "ok", "id " + R.live.live_set_id);
            chk("live", "is_playing readable", R.live.is_playing !== undefined && !R.live.is_playing_error, "is_playing=" + R.live.is_playing);
            chk("live", "tempo readable", typeof R.live.tempo === "number" && R.live.tempo > 0, "tempo=" + R.live.tempo);
            chk("live", "current_song_time readable", typeof R.live.current_song_time === "number", "t=" + R.live.current_song_time);
            chk("live", "live_app version", !!R.live.live_version, "version=" + R.live.live_version);
        } else {
            R.live_note = "LiveAPI unavailable: not inside Live; live_checks skipped";
        }
    }, "run");

    var mok = true, lok = true, i;
    for (i = 0; i < R.max_checks.length; i++) if (!R.max_checks[i].ok) mok = false;
    for (i = 0; i < R.live_checks.length; i++) if (!R.live_checks[i].ok) lok = false;
    if (R.max_checks.length === 0) mok = false;
    R.max_ok = mok; R.live_ok = R.inside_live ? lok : null;
    R.ok = (R.errors.length === 0) && mok && (R.inside_live ? lok : true);
    R.finished = new Date().toString();
    R.elapsed_ms = new Date().getTime() - t0.getTime();
    R.outfile = OUT;

    try { writeText(OUT, JSON.stringify(R, null, 1) + "\n"); } catch (e) { post("pmor_dryrun: result write failed " + e + "\n"); }
    try { writeText(DONE, "done " + (R.ok ? "ok" : "fail") + "\n"); } catch (e2) { post("pmor_dryrun: done write failed " + e2 + "\n"); }
    post("pmor_dryrun: finished ok=" + R.ok + " inside_live=" + R.inside_live + " -> " + OUT + "\n");
}
