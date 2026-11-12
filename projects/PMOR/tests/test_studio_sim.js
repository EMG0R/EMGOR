// node tests/test_studio_sim.js -- headless sim of the device js layer (no Live, no Max, no serial):
//  A  PMOR-originated MIDI (notes on ch1/2/3, control CCs ch4/5, as PMOR sends over USB MIDI) reaches the core through each lane exactly as sent (what records in Live)
//  B  PMOR is source of truth: values arriving from PMOR (param / editor open / schema sync / algo change) NEVER produce a `setparam`; only a user dial move does
//  C  session: connect -> openport + hello; unplug -> STANDALONE (unplugged); replug -> reopen + hello again (host does the full resync FROM PMOR); disconnect -> bye + closeport
//  D  core status line carries PMOR's role when the 9-arg status arrives
const fs = require("fs"), path = require("path"), vm = require("vm");
const D = path.join(__dirname, "..", "max", "devices");
const rd = f => fs.readFileSync(path.join(D, f), "utf8");
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };

// ---- A
function lane(args) { const out = []; const ctx = { module: { exports: {} }, jsarguments: ["pmor_lane.js"].concat(args), outlet: (i, ...a) => out.push(a), post() {}, patcher: { getnamed: () => ({ message() {} }) } };
    vm.createContext(ctx); vm.runInContext(rd("pmor_lane.js"), ctx); ctx.out = out; return ctx; }
const feed = (l, bytes) => bytes.forEach(b => l.module.exports.feed(b));
// a PMOR bar: kick+hat on ch1, synth chord + bend on ch2, sampler hit on ch3, control CCs ch4, fx engage ch5, interleaved with realtime clock
const bar = [0x90, 36, 100, 0xF8, 0x92, 42, 80, 0x91, 60, 90, 0x91, 64, 90, 0xE1, 0, 70, 0xF8, 0x92, 0, 0x80, 36, 0, 0x81, 60, 0, 0x91, 64, 0,
    0x92, 48, 110, 0xB3, 1, 55, 0xB3, 110, 3, 0xB4, 16, 127, 0xFA];
let l = lane(["1", "remap"]); feed(l, bar.filter((b, i) => true));
// remap lanes take everything the track's input filter hands them, re-channelled: here the track input is channel-filtered, so feed only ch1 bytes
l = lane(["1", "remap"]); feed(l, [0x90, 36, 100, 0xF8, 0x90, 42, 80, 0x80, 36, 0, 0x90, 42, 0]);
eq("drums lane: PMOR ch1 notes (incl. note-on vel 0, realtime in between) forwarded in order", l.out, [[0x90, 36, 100], [0x90, 42, 80], [0x80, 36, 0], [0x90, 42, 0]]);
l = lane(["2", "remap"]); feed(l, [0x91, 60, 90, 0x91, 64, 90, 0xE1, 0, 70, 0x81, 60, 0, 0x91, 64, 0]);
eq("synth lane: chord + pitch bend + releases forwarded on ch2", l.out, [[0x91, 60, 90], [0x91, 64, 90], [0xE1, 0, 70], [0x81, 60, 0], [0x91, 64, 0]]);
l = lane(["3", "remap"]); feed(l, [0x92, 48, 110, 0x82, 48, 0]);
eq("sampler lane: ch3 hit forwarded", l.out, [[0x92, 48, 110], [0x82, 48, 0]]);
l = lane(["4", "5", "meters"]); feed(l, bar);
eq("control lane keeps only ch4/ch5 from a mixed PMOR stream", l.out, [[0xB3, 1, 55], [0xB3, 110, 3], [0xB4, 16, 127]]);
// one forwarded message per PMOR message: lanes together lose nothing and duplicate nothing
const lanes = [lane(["1", "remap"]), lane(["2", "remap"]), lane(["3", "remap"]), lane(["4", "5", "meters"])];
const split = { 0: [0x90, 36, 100, 0x80, 36, 0], 1: [0x91, 60, 90, 0xE1, 1, 2, 0x81, 60, 0], 2: [0x92, 48, 110], 3: [0xB3, 7, 9, 0xB4, 16, 0] };
let tot = 0, got = 0; lanes.forEach((x, i) => { feed(x, split[i]); got += x.out.length; });
Object.values(split).forEach(a => { tot += a.filter(b => b & 0x80).length; });
eq("every PMOR message arrives once across the 4 lanes", got, tot);

// ---- B
function ui(gated) {
    const objs = {}, out = [];
    const mk = (cls, vn, attrs) => { const o = { maxclass: cls, varname: vn, attrs: Object.assign({}, attrs), box: {}, log: [] };
        o.getattr = a => o.attrs[a]; o.message = (...a) => { o.log.push(a); }; o.setboxattr = () => {}; objs[vn] = o; return o; };
    const col = [0.6, 0.9, 0.6, 1];
    const list = [mk("dial", "p2052", { min: 0, mult: 1, size: 100, needlecolor: col, outlinecolor: col }), mk("toggle", "p5120", { checkedcolor: col, elementcolor: col }),
        mk("umenu", "p1280", { textcolor: col }), mk("dial", "p1281", { min: 0, mult: 0.001, size: 1001, needlecolor: col, outlinecolor: col }),
        mk("comment", "unknown", {}), mk("umenu", "unk", {}), mk("comment", "ver", {})];
    list.forEach((o, i) => { o.nextobject = list[i + 1] || null; });
    const ctx = { module: { exports: {} }, jsarguments: ["pmor_ui.js"].concat(gated ? ["gated"] : []), outlet: (i, ...a) => out.push([i, ...a]), post() {},
        patcher: { firstobject: list[0], getnamed: n => objs[n] || null }, arrayfromargs: a => Array.from(a) };
    vm.createContext(ctx); vm.runInContext(rd("pmor_ui.js"), ctx); ctx.out = out; ctx.objs = objs; return ctx;
}
const setp = x => x.out.filter(m => m[0] === 0 && m[1] === "setparam");
for (const gated of [false, true]) {
    const x = ui(gated), M = x.module.exports; if (gated) x.module.exports.editor(0);
    M.param(1281, 0.8); M.param(2052, 40); M.param(5120, 1); M.param(1280, 3); M.param(1024 + 3 * 32 + 1, 0.4);
    M.schemabegin(3); M.schema(1281, "VOLUME"); M.schema(2052, "X"); M.schema(1280, "ALGO"); M.schemaend(3);
    x.loadbang && x.loadbang(); M.editor(1); M.editor(0); M.editor(1);
    M.param(1281, 0.2);
    eq("no setparam from PMOR-originated values (gated=" + gated + ")", setp(x), []);
    eq("controls were set silently via `set` (volume dial)", x.objs.p1281.log.filter(m => m[0] === "set").length > 0, true);
    eq("no control gets a non-`set` value message that would output", x.objs.p1281.log.filter(m => m[0] === "float" || m[0] === "int" || m[0] === "bang").length, 0);
}

// ---- C
function session() {
    const out = [];
    const ctx = { module: { exports: {} }, outlet: (i, ...a) => out.push([i, ...a]), post() {}, arrayfromargs: a => Array.from(a), messagename: "",
        patcher: { newdefault: () => ({ maxclass: "metro", message() {} }), connect() {}, remove() {} }, box: { maxclass: "newobj" } };
    vm.createContext(ctx); vm.runInContext(rd("pmor_session.js"), ctx); ctx.out = out; return ctx;
}
const ticks = (s, n) => { for (let i = 0; i < n; i++) s.bang(); };
const cmds = s => s.out.filter(m => m[0] === 0 || m[0] === 1).map(m => m.slice(0, 1).concat(m.slice(1)).join(" "));
const status = s => { const t = s.out.filter(m => m[0] === 2).pop(); return t ? t[2] : ""; };
let s = session(); s.port("e"); s.connect(1); ticks(s, 2);
eq("connect: openport then hello STUDIO", cmds(s).slice(0, 2).map(x => x.replace(/^0 openport .*/, "0 openport")), ["0 openport", "1 hello STUDIO"]);
s.state("syncing"); s.state("ready"); eq("ready: status line", status(s).indexOf("ready STUDIO"), 0);
s.state("disconnected"); eq("unplug -> STANDALONE", status(s), "STANDALONE (unplugged)");
s.out.length = 0; ticks(s, 12);
const re = cmds(s); eq("while unplugged: openport retried (<= every 2 s)", re.some(c => c.indexOf("0 openport") === 0), true);
ticks(s, 2); eq("replug: hello re-sent for a full resync", cmds(s).filter(c => c === "1 hello STUDIO").length >= 1, true);
s.state("ready"); eq("replug: link back", status(s).indexOf("ready STUDIO"), 0);
s.editor(1); eq("editor open -> mode EDIT sent", cmds(s).slice(-1)[0], "1 mode EDIT");
s.out.length = 0; s.connect(0); ticks(s, 1); eq("disconnect: bye then closeport", cmds(s).filter(c => /bye|closeport|closed/.test(c)), ["1 bye", "0 closeport", "1 closed"]);

// ---- D
const bctx = { module: { exports: {} }, jsarguments: ["pmor_browser.js"], outlet: () => {}, post() {}, arrayfromargs: a => Array.from(a) };
vm.createContext(bctx); vm.runInContext(rd("pmor_browser.js"), bctx);
eq("status with role arg -> role shown", bctx.module.exports.transText("AUTO", 1, 120, 3, 0, 0, 1, "follow"), "run 120.0 | follow | cpu 3 ovr 0 drop 0");
console.log("test_studio_sim: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
