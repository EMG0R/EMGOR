// driver.js -- headless Max 9 end-to-end driver for the saved PMOR devices against tools/simpmor_udp.
// Loaded by the generated test patcher as [js driver.js <scenario> <logfile> <simport> <simlog> <devdir> <cachedir>].
// Loads the REAL device patchers (PMOR core + 4 lanes) as bpatchers, drives their controls through the Max js API,
// observes the status/param streams the devices themselves broadcast, reads the simulator's log, logs PASS/FAIL, quits Max.
// Max js cannot read a [comment]'s text, so comment widgets are checked through the stream that feeds them (proto outlet 2).
inlets = 1; outlets = 0;
var SCN = jsarguments[1], LOG = jsarguments[2], PORT = parseInt(jsarguments[3], 10), SIMLOG = jsarguments[4], DEV = jsarguments[5], CACHE = jsarguments[6], PORTBUG = jsarguments[7] === "1", SYNCBUG = jsarguments[8] === "1";
var pass = 0, fail = 0, started = false, finished = false;

// ------------------------------------------------------------------ logging
function w(s) { var f = new File(LOG, "write"); f.open(); f.position = f.eof; f.writestring(s + "\n"); f.close(); }
function ok(name, cond, detail) { if (cond) { pass++; w("PASS " + name + (detail ? "  (" + detail + ")" : "")); } else { fail++; w("FAIL " + name + (detail ? "  (" + detail + ")" : "")); } return !!cond; }
function note(s) { w("note " + s); }

// ------------------------------------------------------------------ sim log (incremental)
var simLines = [], simPos = 0;
function simRead() {
    var f = new File(SIMLOG, "read");
    if (!f.isopen) return simLines;
    f.position = simPos; var buf = "";
    while (f.position < f.eof) { var chunk = f.readstring(16384); if (!chunk || !chunk.length) break; buf += chunk; }
    simPos = f.position; f.close();
    if (buf.length) { var parts = buf.split("\n"); if (simPart) { parts[0] = simPart + parts[0]; } simPart = parts.pop(); for (var i = 0; i < parts.length; i++) if (parts[i].length) simLines.push(parts[i]); }
    return simLines;
}
var simPart = "";
function simCount(re, from) { simRead(); var n = 0; for (var i = from || 0; i < simLines.length; i++) if (re.test(simLines[i])) n++; return n; }
function simFind(re, from) { simRead(); var r = []; for (var i = from || 0; i < simLines.length; i++) if (re.test(simLines[i])) r.push(simLines[i]); return r; }
function simMark() { simRead(); return simLines.length; }
function setParams(from) {        // [ [id, value] ... ] of every SET_PARAM the sim received after `from`
    var r = [], L = simFind(/ RX !\S+ SET_PARAM /, from);
    for (var i = 0; i < L.length; i++) { var m = / SET_PARAM (\d+) (\S+?)\*/.exec(L[i]); if (m) r.push([parseInt(m[1], 10), parseFloat(m[2])]); }
    return r;
}

// ------------------------------------------------------------------ sim control (own udpsend, rawbytes)
var simctl = null;
function simCtl(txt) {
    var a = ["rawbytes"]; for (var i = 0; i < txt.length; i++) a.push(txt.charCodeAt(i));
    simctl.message.apply(simctl, a);
}

// ------------------------------------------------------------------ streams seen from the devices
var st = { state: "none", synced: 0, patch: -1, infos: [], pool: [], poolends: 0, presetends: 0, presets: [], store: [], assigned: [], samples: [], progress: 0, rx: 0, rxParams: {}, tx: 0, txs: [], status: 0, events: [], tagcount: {}, seq: [] };
function stmsg() {                       // from [r e2e_st] = proto outlet 2 of the core device
    var a = arrayfromargs(arguments), t = String(a[0]);
    st.tagcount[t] = (st.tagcount[t] || 0) + 1; if (st.seq.length < 40) st.seq.push(t);
    if (t === "state") { st.state = String(a[1]); st.events.push("state " + a[1]); }
    else if (t === "synced") { st.synced++; if (!st.syncT0) st.syncT0 = new Date().getTime(); st.syncT1 = new Date().getTime(); st.events.push("synced " + a[1] + " " + a[2]); }
    else if (t === "patch") { st.patch = a[1] | 0; }
    else if (t === "info") { st.infos.push(a.slice(1).join(" ")); }
    else if (t === "poolbegin") { st.pool = []; }
    else if (t === "pool") { st.pool.push({ id: a[1] | 0, len: a[2] | 0, name: String(a[3]), cached: a[4] | 0 }); }
    else if (t === "poolend") { st.poolends++; }
    else if (t === "presetbegin") { st.presets = []; }
    else if (t === "preset") { st.presets.push(a.slice(1).join(" ")); }
    else if (t === "presetend") { st.presetends++; }
    else if (t === "store") { st.store.push(a.slice(1).join(" ")); }
    else if (t === "assigned") { st.assigned.push(a.slice(1).join(" ")); }
    else if (t === "sample") { st.samples.push(a[1] + " " + a[2]); }
    else if (t === "progress") { st.progress++; }
    else if (t === "status") { st.status++; }
}
function rx() { var a = arrayfromargs(arguments); if (a[0] === "param") { st.rx++; st.rxParams[a[1]] = a[2]; } }   // [r pmor_rx]
var uisCount = {};
function uis() { var t = String(arguments[0]); uisCount[t] = (uisCount[t] || 0) + 1; }                       // [r pmor_ui_status]: the stream the status/store/transport comments are fed from
function tx() { st.tx++; st.txs.push(arrayfromargs(arguments).join(" ")); }                                          // [r pmor_tx]

// ------------------------------------------------------------------ devices
var core = null, lanes = {}, coreEd = null, laneEd = {};
function findBox(sub, name) { return sub ? sub.getnamed(name) : null; }
function firstOf(sub, cls) { var b = sub.firstobject; while (b) { if (b.maxclass === cls) return b; b = b.nextobject; } return null; }
function loadDev(file, x, y) {
    var b = this.patcher.newdefault(x, y, "bpatcher", DEV + "/" + file);
    if (!b) return null;
    var s = b.subpatcher();
    return s;
}
function loadAll() {
    core = loadDev("PMOR core.maxpat", 10, 10);
    var ONLYCORE = (SCN === "x");
    if (!ONLYCORE) lanes.drums = loadDev("PMOR lane drums.maxpat", 10, 220);
    if (!ONLYCORE) lanes.synth = loadDev("PMOR lane synth.maxpat", 400, 220);
    if (!ONLYCORE) lanes.sampler = loadDev("PMOR lane sampler.maxpat", 10, 420);
    if (!ONLYCORE) lanes.fx = loadDev("PMOR lane fx.maxpat", 400, 420);
    ok("load: core + 4 lane devices as bpatchers", core && (ONLYCORE || (lanes.drums && lanes.synth && lanes.sampler && lanes.fx)), core ? "ok" : "core missing");
    var proto = findBox(core, "proto");
    ok("load: core has pmor.proto + pmor.core~ + no port object", proto && findBox(core, "core") && !hasClass(core, "serial") && !hasClass(core, "udpsend") && !hasClass(core, "udpreceive"));
    // spy on the status stream (what the status/samples/store comments are fed from)
    var snd = core.newdefault(300, 420, "send", "e2e_st"); core.connect(proto, 2, snd, 0);
    // editor sub-patcher (pool/preset browser) of the core
    var b = core.firstobject; while (b) { if (b.maxclass === "patcher") { coreEd = b.subpatcher(); break; } b = b.nextobject; }
    ok("load: core editor patcher reachable", !!coreEd);
    for (var k in lanes) { var bb = lanes[k].firstobject; while (bb) { if (bb.maxclass === "patcher") { laneEd[k] = bb.subpatcher(); break; } bb = bb.nextobject; } }
    // own udpsend for sim control datagrams
    simctl = this.patcher.newdefault(900, 10, "udpsend", "127.0.0.1", PORT);
    // sample cache in the scratch dir, never ~/Music
    proto.message("cachedir", CACHE);
    proto.message("sessiondir", CACHE + "/session");
}
function hasClass(sub, cls) { var b = sub.firstobject; while (b) { if (b.maxclass === cls) return true; b = b.nextobject; } return false; }
function textButton(sub, label) {
    var b = sub.firstobject;
    while (b) { if (b.maxclass === "textbutton") { var t = b.getattr("text"); if (String(t) === label) return b; } b = b.nextobject; }
    return null;
}
function press(sub, label) { var b = textButton(sub, label); if (!b) { ok("button '" + label + "' found", false); return false; } b.message("bang"); return true; }

function setPortMenu() {
    var pm = findBox(core, "port"); pm.message("append", "udp" + PORT);
    var n = 0; var it = pm.getattr("items"); n = (it && it.length) ? it.length : 0;
    pm.message("set", 6);                                     // a..f = 0..5, appended udp<port> = 6
}
function connect(mode) {         // mode 0 STUDIO, 1 EDIT
    findBox(core, "mode").message("set", mode);
    findBox(core, "connect").message("int", 1);
}
function disconnect() { findBox(core, "connect").message("int", 0); }
function resetStream() { st.state = "none"; st.synced = 0; st.patch = -1; st.pool = []; st.poolends = 0; st.presetends = 0; st.presets = []; st.assigned = []; st.samples = []; st.progress = 0; }

// ------------------------------------------------------------------ step engine (never blocks Max)
var steps = [], si = 0, task = null, cur = null, curStart = 0, dog = 0, ticks = 0, lastTick = 0, maxGap = 0, WATCHDOG_MS = 80000;
function DO(f) { steps.push({ k: "do", f: f }); }
function WAIT(name, pred, ms) { steps.push({ k: "wait", name: name, pred: pred, ms: ms || 8000 }); }
function SLEEP(ms) { steps.push({ k: "sleep", ms: ms }); }
function pump() {
    var now = new Date().getTime(); ticks++; if (lastTick && now - lastTick > maxGap) maxGap = now - lastTick; lastTick = now;
    if (finished) return;
    if (now - dog > WATCHDOG_MS) { ok("scenario " + SCN + " finished inside " + WATCHDOG_MS + " ms (watchdog)", false, "at step " + si + (cur && cur.name ? " waiting '" + cur.name + "'" : "")); finish(); return; }
    var guard = 0;
    while (si < steps.length && guard++ < 50) {
        var s = steps[si];
        if (!s.started) { s.started = true; s.t0 = now; }
        try {
            if (s.k === "do") { s.f(); si++; continue; }
            if (s.k === "sleep") { if (now - s.t0 >= s.ms) { si++; continue; } return; }
            if (s.k === "wait") {
                cur = s; var r = false; try { r = s.pred(); } catch (e) { r = false; }
                if (r) { ok(s.name, true, (now - s.t0) + " ms"); si++; continue; }
                if (now - s.t0 > s.ms) { ok(s.name, false, "timeout " + s.ms + " ms; state=" + st.state); si++; continue; }
                return;
            }
        } catch (e) { ok("step " + si + " threw", false, String(e)); si++; }
    }
    if (si >= steps.length) finish();
}
function finish() {
    if (finished) return; finished = true;
    w("note events(" + st.events.length + "): " + st.events.slice(0, 6).join(" / ") + " ... " + st.events.slice(-6).join(" / ")); w("note synced burst: first-last = " + (st.syncT1 - st.syncT0) + " ms"); w("note first tags: " + st.seq.join(",")); w("note tags: " + JSON.stringify(st.tagcount) + " ui_status: " + JSON.stringify(uisCount)); w("note infos: " + st.infos.slice(-6).join(" | "));
    w("SUMMARY " + SCN + " pass=" + pass + " fail=" + fail + " (js ticks " + ticks + ", max tick gap " + maxGap + " ms)");
    w("DONE");
    task.cancel();
    messnamed("max", "quit");
}

// ------------------------------------------------------------------ scenarios
function sc_a() {            // STUDIO connect through the port menu
    DO(function () { ok("a0: shipped core sends openport/closeport (`open`/`close` are [js] built-ins and never reach pmor_port.js)", !PORTBUG, PORTBUG ? "core still wires `open`/`close`: staged copy patched for the rest of the run" : ""); });
    DO(function () { ok("a0b: shipped core has no synced -> [pool] -> proto feedback loop (floods synced, never ready)", !SYNCBUG, SYNCBUG ? "staged copy drops the [pool] message box" : ""); });
    DO(function () { setPortMenu(); connect(0); note("port menu -> udp" + PORT + ", mode STUDIO, connect on"); });
    WAIT("a: status reaches ready", function () { return st.state === "ready"; }, 10000);
    WAIT("a: synced reported (params + samples counts)", function () { return st.synced >= 1; }, 5000);
    WAIT("a: patch slot reported (3)", function () { return st.patch === 3; }, 5000);
    DO(function () {
        var m = simFind(/RX !\S+ HELLO STUDIO/);
        ok("a: sim saw HELLO STUDIO over the udp link", m.length >= 1, m.length ? m[0] : "none");
        ok("a: schema/params pulled (GET_SCHEMA, GET_PARAMS, GET_SEQ x3, GET_MANIFEST)", simCount(/ RX !\S+ GET_SCHEMA/) >= 1 && simCount(/ RX !\S+ GET_PARAMS/) >= 1 && simCount(/ RX !\S+ GET_SEQ/) >= 3 && simCount(/ RX !\S+ GET_MANIFEST/) >= 1);
        ok("a: param stream reached pmor_rx (UI + core feed)", st.rx > 100, st.rx + " param msgs");
    });
    SLEEP(4500);
    DO(function () {
        ok("a: still ready after 4.5 s idle (PING liveness)", st.state === "ready");
        ok("a: liveness traffic reached the sim (PING / STATUS poll)", simCount(/ RX !\S+ (PING|STATUS)/) >= 4, simCount(/ RX !\S+ PING/) + " pings, " + simCount(/ RX !\S+ STATUS/) + " status polls");
        ok("a: STATUS polled", st.status >= 1, st.status + " status msgs");
        ok("a: js tick never stalled > 1 s", maxGap < 1000, "max gap " + maxGap);
    });
}

// a [dial] takes/reports STEPS; the PMOR value is min + mult*step (make_devices.scale_attrs)
function dialValue(box) { var v = box.getvalueof(); v = (v instanceof Array) ? v[0] : v; var lo = Number(box.getattr("min")), mu = Number(box.getattr("mult")); return lo + mu * v; }
function dialSet(box, val) { var lo = Number(box.getattr("min")), mu = Number(box.getattr("mult")); box.message("float", (val - lo) / mu); }
function sc_b() {            // lane dial -> SET_PARAM -> stored value returns to the dial
    sc_a_connect_only();
    var m1;
    DO(function () {
        m1 = simMark();
        var syn = lanes.synth, d = findBox(syn, "s8");      // algo 0 slot 8 = CUTOFF (id 1032, 40..12000)
        note("synth CUTOFF dial s8 before: " + dialValue(d));
        st.rxParams[1032] = undefined; dialSet(d, 3000);
    });
    WAIT("b: synth cutoff dial change arrives at sim as SET_PARAM 1032 ~3000", function () { var p = setParams(m1), hit = false; for (var i = 0; i < p.length; i++) if (p[i][0] === 1032 && Math.abs(p[i][1] - 3000) < 15) hit = true; return hit; }, 4000);
    WAIT("b: stored cutoff value returned on pmor_rx", function () { var v = st.rxParams[1032]; return v !== undefined && Math.abs(v - 3000) < 40; }, 3000);
    DO(function () {
        var d = findBox(lanes.synth, "s8"), v = dialValue(d);
        ok("b: synth cutoff dial holds the stored value", Math.abs(v - 3000) < 60, "dial=" + v + " rx=" + st.rxParams[1032]);
        ok("b: only one SET_PARAM per dial move (no echo loop)", setParams(m1).filter(function (p) { return p[0] === 1032; }).length <= 2, setParams(m1).length + " frames");
        m1 = simMark(); st.rxParams[2304] = undefined;
        dialSet(findBox(lanes.drums, "p2304"), 0.5);   // drums KICK volume
    });
    WAIT("b: drums pad volume dial -> SET_PARAM 2304 0.5", function () { var p = setParams(m1); for (var i = 0; i < p.length; i++) if (p[i][0] === 2304 && Math.abs(p[i][1] - 0.5) < 0.02) return true; return false; }, 4000);
    WAIT("b: drums volume value returned on pmor_rx", function () { var v = st.rxParams[2304]; return v !== undefined && Math.abs(v - 0.5) < 0.02; }, 3000);
    DO(function () {
        var v = dialValue(findBox(lanes.drums, "p2304")); ok("b: drums dial holds the stored value", Math.abs(v - 0.5) < 0.03, "dial=" + v);
        // device-side edit (PMOR is the truth): reaches the dial, and the host does not answer a push with SET_PARAM
        m1 = simMark(); st.rxParams[1281] = undefined; simCtl("SIM VOL 0.8");
    });
    WAIT("b: device-side edit (synth volume 0.8) pushed to pmor_rx", function () { var v = st.rxParams[1281]; return v !== undefined && Math.abs(v - 0.8) < 0.01; }, 3000);
    SLEEP(300);
    DO(function () {
        ok("b: pushed value reached the synth VOLUME dial", Math.abs(dialValue(findBox(lanes.synth, "p1281")) - 0.8) < 0.02, "dial=" + dialValue(findBox(lanes.synth, "p1281")));
        ok("b: host did not answer the push with SET_PARAM (no echo loop)", setParams(m1).length === 0, setParams(m1).length + " frames");
    });
}
function sc_a_connect_only() {
    DO(function () { setPortMenu(); connect(0); });
    WAIT("setup: ready + synced", function () { return st.state === "ready" && st.synced >= 1; }, 10000);
}

function sc_c() {            // presets
    sc_a_connect_only();
    WAIT("c: presets loaded (presetend after sync)", function () { return st.presetends >= 1 && st.presets.length > 0; }, 8000);
    var m1;
    DO(function () {
        note("presets seen: " + st.presets.length + " e.g. " + st.presets.slice(0, 3).join(" | "));
        ok("c: preset list has 12 engines x (factory+user)", st.presets.length === 24, st.presets.length + " entries");
        var items = findBox(lanes.drums, "presets").getattr("items");
        note("drums lane preset menu items: " + items);
        var s = String(items);
        ok("c: drums lane preset umenu lists factory + user entries", s.indexOf("SNAPPY") >= 0 && s.indexOf("MYDRUM") >= 0, s);
        findBox(lanes.drums, "pad").message("int", 6);          // pad 6
        findBox(lanes.drums, "presets").message("int", 0);       // factory SNAPPY
        m1 = simMark();
        press(lanes.drums, "apply");
    });
    SLEEP(900);
    DO(function () {
        var p = setParams(m1);
        note("factory drum apply -> " + JSON.stringify(p));
        ok("c: factory drum entry applied: 1 SET_PARAM, value 3 (SNAPPY sound)", p.length === 1 && p[0][1] === 3, p.length + " SET_PARAMs");
        findBox(lanes.drums, "pad").message("int", 9);
        findBox(lanes.drums, "presets").message("int", 1);       // user MYDRUM
        m1 = simMark(); press(lanes.drums, "apply");
    });
    SLEEP(900);
    DO(function () {
        var p = setParams(m1), vals = p.map(function (x) { return x[1]; }).join(",");
        note("user drum apply -> " + JSON.stringify(p));
        function has(v) { for (var i = 0; i < p.length; i++) if (Math.abs(p[i][1] - v) < 1e-3) return true; return false; }
        ok("c: user drum entry applied: base 2 + knobs 0.1..0.4", p.length >= 5 && has(2) && has(0.1) && has(0.2) && has(0.3) && has(0.4), p.length + " SET_PARAMs: " + vals);
        var it = String(findBox(lanes.synth, "presets").getattr("items")); note("synth lane preset items: " + it);
        ok("c: synth lane preset umenu lists PLUCK + MYSOUND", it.indexOf("PLUCK") >= 0 && it.indexOf("MYSOUND") >= 0, it);
        findBox(lanes.synth, "presets").message("int", 1);       // user MYSOUND (engine 4 = algo 0)
        m1 = simMark(); press(lanes.synth, "apply");
    });
    SLEEP(1200);
    DO(function () {
        var p = setParams(m1), cut = null, res = null;
        for (var i = 0; i < p.length; i++) { if (p[i][0] === 1032) cut = p[i][1]; if (p[i][0] === 1033) res = p[i][1]; }
        note("user synth apply -> " + p.length + " SET_PARAMs, cutoff " + cut + " reso " + res);
        ok("c: user synth entry applied: 21 SET_PARAMs, cutoff 1234 reso 0.25", p.length === 21 && cut !== null && Math.abs(cut - 1234) < 1 && res !== null && Math.abs(res - 0.25) < 1e-3, p.length + " SET_PARAMs cutoff=" + cut + " reso=" + res);
        ok("c: applied cutoff came back on pmor_rx", st.rxParams[1032] !== undefined && Math.abs(st.rxParams[1032] - 1234) < 2, "rx=" + st.rxParams[1032]);
    });
}

function poolIds() { var r = []; for (var i = 0; i < st.pool.length; i++) r.push(st.pool[i].id); return r; }
function sc_d() {            // pool + assign (EDIT)
    DO(function () { setPortMenu(); connect(1); note("EDIT mode"); });
    WAIT("d: ready + synced in EDIT", function () { return st.state === "ready" && st.synced >= 1; }, 10000);
    WAIT("d: pool listed", function () { return st.poolends >= 1 && st.pool.length > 0; }, 8000);
    DO(function () {
        var ids = poolIds(), all = ids.length > 0; for (var i = 0; i < ids.length; i++) if (ids[i] < 1024) all = false;
        ok("d: pool ids all >= 1024", all, "ids " + ids.join(","));
        var pi = String(findBox(coreEd, "pool").getattr("items"));
        ok("d: editor pool umenu filled (" + ids.length + " entries, S<id> labels)", pi.indexOf("S" + ids[0]) >= 0, pi.slice(0, 80));
    });
    WAIT("d: EDIT autopull cached samples (pulled)", function () { var n = 0; for (var i = 0; i < st.samples.length; i++) if (/pulled|cached/.test(st.samples[i])) n++; return n >= st.pool.length && st.pool.length > 0; }, 25000);
    var m1, tid;
    DO(function () {
        tid = st.pool[0].id; st.samples = [];
        findBox(coreEd, "pool").message("int", 0);
        press(coreEd, "pull");
        note("pull one id " + tid + " via the pool browser");
    });
    WAIT("d: 'pull' of the selected pool id answered (sample cached/pulled)", function () { return st.samples.length >= 1; }, 8000);
    DO(function () {
        ok("d: pull answered for the selected id S" + tid, String(st.samples[0]).indexOf("S" + tid + " ") === 0, st.samples[0]);
        findBox(coreEd, "pool").message("int", 1); findBox(coreEd, "slot").message("int", 4);
        st.assigned = []; m1 = simMark(); press(coreEd, "assign to slot");
    });
    WAIT("d: sim got ASSIGN_SAMPLE slot 4 (id of pool entry 1)", function () { var L = simFind(/EV ASSIGN_SAMPLE slot=4 id=(\d+)/, m1); return L.length >= 1 && /id=(\d+)/.exec(L[0])[1] == st.pool[1].id; }, 5000);
    WAIT("d: sim SAMPLE_READY handled", function () { return simCount(/EV SAMPLE_READY slot=4/, m1) >= 1; }, 3000);
    WAIT("d: Max got `assigned 4 ok`", function () { return st.assigned.join("|").indexOf("4 ok") >= 0; }, 5000);
    DO(function () {
        // STUDIO refusal path is covered natively; here: assign pool entry that does not exist is not reachable from the UI
        ok("d: pool refreshed after assign (poolend)", st.poolends >= 2, "poolends " + st.poolends);
    });
}

function sc_e() {            // STORE_RETRY
    sc_a_connect_only();
    WAIT("e: presets + pool loaded first", function () { return st.presetends >= 1 && st.poolends >= 1; }, 8000);
    var m1, pe, po;
    DO(function () { st.store = []; simCtl("SIM STORE 3"); note("sim store -> errors (3)"); });
    WAIT("e: store state 3 reported (<= 2 s STATUS poll)", function () { for (var i = 0; i < st.store.length; i++) if (/^3 /.test(st.store[i])) return true; return false; }, 6000);
    WAIT("e: retry hint reported", function () { for (var i = 0; i < st.infos.length; i++) if (st.infos[i].indexOf("storeretry") >= 0) return true; return false; }, 3000);
    DO(function () { m1 = simMark(); pe = st.presetends; po = st.poolends; st.store = []; press(core, "store retry"); });
    WAIT("e: sim received STORE_RETRY", function () { return simCount(/EV STORE_RETRY/, m1) === 1; }, 3000);
    WAIT("e: store goes migrating (1) then done (2)", function () { var one = false, two = false; for (var i = 0; i < st.store.length; i++) { if (/^1 /.test(st.store[i])) one = true; if (/^2 /.test(st.store[i]) && one) two = true; } return one && two; }, 12000);
    WAIT("e: pool reloaded after retry", function () { return st.poolends > po; }, 8000);
    WAIT("e: presets reloaded after retry (PULL_FILE again)", function () { return st.presetends > pe && simCount(/ RX !\S+ PULL_FILE/, m1) >= 1; }, 8000);
}

function sc_f() {            // disconnect / reconnect
    sc_a_connect_only();
    var m1;
    DO(function () { m1 = simMark(); disconnect(); });
    WAIT("f: BYE reached the sim", function () { return simCount(/ RX !\S+ BYE/, m1) >= 1; }, 3000);
    WAIT("f: port closed (sim saw PORTCLOSE)", function () { return simCount(/ RX PORTCLOSE/, m1) >= 1; }, 3000);
    WAIT("f: state reads disconnected", function () { return st.state === "disconnected"; }, 3000);
    DO(function () { var b = simFind(/RX !\S+ BYE/, m1)[0], c = simFind(/RX PORTCLOSE/, m1)[0]; ok("f: BYE came before the port close", simLines.indexOf(b) < simLines.indexOf(c)); resetStream(); m1 = simMark(); connect(0); });
    WAIT("f: reconnect reaches ready", function () { return st.state === "ready"; }, 10000);
    WAIT("f: reconnect resyncs", function () { return st.synced >= 1 && st.patch === 3; }, 5000);
    DO(function () { ok("f: second HELLO reached the sim", simCount(/ RX !\S+ HELLO/, m1) >= 1); });
    SLEEP(600);
    DO(function () { disconnect(); });
    WAIT("f: second disconnect clean", function () { return st.state === "disconnected"; }, 3000);
    DO(function () { resetStream(); connect(1); });
    WAIT("f: third connect (EDIT) reaches ready", function () { return st.state === "ready"; }, 10000);
}

function sc_g() {            // cable yank mid-transfer
    DO(function () { setPortMenu(); connect(1); note("EDIT (pull all) with the sim set to yank after N bytes"); });
    WAIT("g: sim yanked the cable mid-session", function () { return simCount(/EV YANK/) >= 1; }, 15000);
    var t0, m1;
    DO(function () { t0 = new Date().getTime(); note("yank seen; state=" + st.state + " synced=" + st.synced + " progress=" + st.progress); });
    WAIT("g: client reports the dead link (timeout info)", function () { for (var i = 0; i < st.infos.length; i++) if (/timeout|lost|no reply/i.test(st.infos[i])) return true; return false; }, 14000);
    DO(function () { ok("g: Max stays alive (js ticks + still answering)", maxGap < 2500, "max gap " + maxGap + " ms, state " + st.state); note("infos: " + st.infos.slice(-4).join(" | ")); });
    DO(function () { disconnect(); m1 = simMark(); });
    WAIT("g: disconnect while yanked -> PORTCLOSE reaches the sim", function () { return simCount(/ RX PORTCLOSE/, m1) >= 1; }, 3000);
    SLEEP(700);
    DO(function () { resetStream(); st.infos = []; m1 = simMark(); connect(0); });
    WAIT("g: reconnect after yank reaches ready", function () { return st.state === "ready"; }, 12000);
    WAIT("g: resync after yank", function () { return st.synced >= 1 && st.patch === 3; }, 5000);
    SLEEP(2500);
    DO(function () { ok("g: stays ready 2.5 s after recovery", st.state === "ready"); });
}

// ------------------------------------------------------------------ entry
function go() {
    if (started) return; started = true;
    var f0 = new File(LOG, "read"); var dupe = false;
    if (f0.isopen) { if (f0.eof > 0) dupe = true; f0.close(); }
    if (dupe) return;                                    // the patcher can be opened twice by `open`: only the first run drives
    w("START " + SCN);
    dog = new Date().getTime();
    try {
        loadAll();
        var f = { x: sc_a, a: sc_a, b: sc_b, c: sc_c, d: sc_d, e: sc_e, f: sc_f, g: sc_g }[SCN];
        if (!f) { ok("known scenario", false, SCN); finish(); return; }
        f();
    } catch (e) { ok("setup threw", false, String(e)); }
    task = new Task(pump, this); task.interval = 20; task.repeat();
}
