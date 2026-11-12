// node tests/test_session.js -- pmor_session.js against a simulated world (PMOR plugged / unplugged, a Bela on another letter, a proto that takes
// time to sync). No Max, no Live, no serial. Covers: USB-identity port choice (never the Bela / blind scan), unplug -> STANDALONE (unplugged), replug x5
// time-to-ready, yank mid-transfer (syncing), a proto that reports `disconnected` after a good link (DTR loss / 2 s silence), no stale port, manual override.
const fs = require("fs"), path = require("path"), vm = require("vm");
const src = fs.readFileSync(path.join(__dirname, "..", "max", "devices", "pmor_session.js"), "utf8");
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
const ok = (n, c, x) => { if (c) pass++; else { fail++; console.log("FAIL", n, x === undefined ? "" : x); } };

const SYNC_MS = 800;                  // sim: HELLO answered -> ready this long after the first hello that reaches an open, DTR-high port
function world(opts) {
    opts = opts || {};
    const w = { t: 0, plugged: true, openLetter: null, openedAt: 0, opens: [], closes: 0, status: "", state: "disconnected", syncStart: -1, log: [], pmorLetter: "e", belaLetter: "f", opened: [] };
    const ctx = { module: { exports: {} }, post() {}, arrayfromargs: a => Array.from(a), messagename: "", patcher: { newdefault: () => ({ maxclass: "metro", message() {} }), connect() {}, remove() {} }, box: { maxclass: "newobj" } };
    ctx.outlet = (i, ...a) => {
        if (i === 0 && a[0] === "openport") {
            ok("a port is never open twice", w.openLetter === null || w.nowClosed === true, "open while " + w.openLetter);   // pmor_port.js closes first, but session must close explicitly before reopen
            w.openLetter = a[1]; w.openedAt = w.t; w.opens.push(a[1]); w.opened.push(a[1]);
        } else if (i === 0 && a[0] === "closeport") { w.openLetter = null; w.closes++; }
        else if (i === 1 && a[0] === "findport") {
            if (w.plugged) ctx.pmorport(w.pmorLetter, "/dev/cu.usbmodem213103"); else ctx.pmorport("none");
        } else if (i === 1 && a[0] === "hello") {
            // PMOR drops bytes until DTR has been up 400 ms; unplugged / wrong port = no answer
            if (w.plugged && w.openLetter === w.pmorLetter && w.t - w.openedAt >= 400 && w.state !== "ready" && w.syncStart < 0) w.syncStart = w.t;
            if (w.state === "disconnected" && w.syncStart >= 0) { w.state = "hello"; ctx.state("hello"); }
        } else if (i === 1 && a[0] === "closed") { w.syncStart = -1; if (w.state !== "disconnected") { w.state = "disconnected"; ctx.state("disconnected"); } }
        else if (i === 2) w.status = a[1];
    };
    vm.createContext(ctx); vm.runInContext(src, ctx);
    w.ctx = ctx;
    w.step = () => {        // one 250 ms metro tick, then the proto's own progress
        w.t += 250; w.nowClosed = false;
        ctx.bang();
        if (w.syncStart >= 0 && w.state === "hello") { w.state = "syncing"; ctx.state("syncing"); }
        if (w.syncStart >= 0 && w.state === "syncing" && w.t - w.syncStart >= SYNC_MS) { w.state = "ready"; ctx.state("ready"); }
    };
    w.run = ms => { for (let k = 0; k < ms / 250; k++) w.step(); };
    w.until = (pred, max) => { const t0 = w.t; while (!pred() && w.t - t0 < max) w.step(); return w.t - t0; };
    w.unplug = () => { w.plugged = false; w.syncStart = -1; if (w.state !== "disconnected" && opts.protoReports) { w.state = "disconnected"; ctx.state("disconnected"); } };
    w.plug = () => { w.plugged = true; };
    return w;
}

// ---- auto: found by identity, never the Bela letter
let w = world(); w.ctx.connect(1);
eq("connect auto: openport on PMOR's letter e (not the Bela f, no blind scan)", w.opens, ["e"]);
let ms = w.until(() => w.state === "ready", 8000); ok("first connect ready < 3 s", ms < 3000, ms);
ok("status ready", /^ready STUDIO \| port e/.test(w.status), w.status);

// ---- unplug (proto stays silent): the port vanishes -> clean close within 1 s, STANDALONE
w.unplug(); ms = w.until(() => w.openLetter === null, 3000);
ok("unplug: port closed within 1 s of vanishing", ms <= 1000, ms);
eq("unplug: status", w.status, "STANDALONE (unplugged)");
eq("unplug: proto told `closed` (state disconnected)", w.state, "disconnected");
// ---- replug x5, measure from the port reappearing
for (let k = 1; k <= 5; k++) {
    w.run(1500); w.plug(); const t0 = w.t; ms = w.until(() => w.state === "ready", 8000);
    ok("replug " + k + ": ready < 3 s after the port reappears (" + ms + " ms)", ms < 3000, ms);
    ok("replug " + k + ": status ready", /^ready STUDIO \| port e/.test(w.status), w.status);
    if (k < 5) { w.unplug(); w.until(() => w.openLetter === null, 3000); eq("replug " + k + " unplugged cleanly", w.status, "STANDALONE (unplugged)"); }
}
ok("never more than one open port", w.opens.length === w.closes + 1, w.opens.length + " opens " + w.closes + " closes");

// ---- yank mid-transfer: the cable goes while syncing
w = world(); w.ctx.connect(1); w.until(() => w.state === "syncing", 4000);
w.unplug(); w.until(() => w.openLetter === null, 3000);
eq("yank mid-sync: closed", [w.openLetter, w.state, w.status], [null, "disconnected", "STANDALONE (unplugged)"]);
w.run(2000); w.plug(); ms = w.until(() => w.state === "ready", 8000); ok("yank mid-sync: replug -> ready < 3 s (" + ms + " ms)", ms < 3000, ms);

// ---- proto reports `disconnected` after a good link (2 s silence / DTR loss) while the port still lists: clean close + reopen
w = world({ protoReports: true }); w.ctx.connect(1); w.until(() => w.state === "ready", 8000); const o0 = w.opens.length;
w.state = "disconnected"; w.syncStart = -1; w.ctx.state("disconnected");
eq("link lost: port closed at once", w.openLetter, null);
ms = w.until(() => w.state === "ready", 8000); ok("link lost: reopened + ready < 3 s (" + ms + " ms)", ms < 3000 && w.opens.length === o0 + 1, ms + " opens " + w.opens.length);

// ---- started unplugged: waits, then connects when PMOR shows up
w = world(); w.plugged = false; w.ctx.connect(1); eq("unplugged start: no port opened", w.opens, []); w.run(2000);
eq("unplugged start: waiting status", w.status, "waiting for PMOR (usb)...");
w.plug(); ms = w.until(() => w.state === "ready", 8000); ok("appears later -> ready < 3 s (" + ms + " ms)", ms < 3000, ms);

// ---- wrong device only (a Bela): never opened
w = world(); w.plugged = false; w.ctx.connect(1); w.run(5000); eq("no PMOR: nothing opened (Bela letter f never touched)", w.opens, []);

// ---- disconnect: bye + closeport + closed, ticker off
w = world(); w.ctx.connect(1); w.until(() => w.state === "ready", 8000); w.ctx.connect(0); w.run(500);
eq("disconnect closes the port", [w.openLetter, w.state, w.status], [null, "disconnected", "disconnected"]);

// ---- manual override: letter opened as given, reopened every 1 s while the link is down
w = world(); w.ctx.port("e"); w.ctx.connect(1); eq("manual: opened as chosen without findport", w.opens, ["e"]); w.until(() => w.state === "ready", 8000);
w.unplug(); w.state = "disconnected"; w.ctx.state("disconnected"); w.run(2000); w.plug(); ms = w.until(() => w.state === "ready", 8000);
ok("manual: unplug/replug -> ready (" + ms + " ms)", w.state === "ready" && ms < 4000, ms);

console.log("test_session: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
