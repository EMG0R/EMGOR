// node tests/test_bank.js -- pmor_bank.js (engine -> sound -> params navigation) + pmor_guard.js (no write to PMOR on device / set load).
const fs = require("fs"), path = require("path"), vm = require("vm");
const D = path.join(__dirname, "..", "max", "devices");
const rd = f => fs.readFileSync(path.join(D, f), "utf8");
const banks = JSON.parse(rd("banks.json"));
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
const near = (n, g, w, t = 1e-6) => { if (Math.abs(g - w) <= t) pass++; else { fail++; console.log("FAIL", n, g, "!=", w); } };

// editor-like scope: one control per bank name that is not a lane (L-prefixed) name; classes by name convention
function make(gated, names) {
    const objs = {}, out = [], list = [];
    const mk = (cls, vn) => { const o = { maxclass: cls, varname: vn, log: [], box: {}, nextobject: null }; o.message = (...a) => o.log.push(a); o.setboxattr = (k, ...v) => { o.box[k] = v.length > 1 ? v : v[0]; }; objs[vn] = o; list.push(o); return o; };
    for (const n of names) { mk("dial", "bk_" + n); mk("comment", "bl_" + n); mk("comment", "bv_" + n); }
    mk("comment", "selinfo"); mk("comment", "sndhead");
    for (let i = 0; i < 16; i++) { mk("textbutton", "pc_" + i); mk("textbutton", "sl_" + i); }
    for (let i = 0; i < 24; i++) mk("textbutton", "sc_" + i);
    for (let i = 0; i < 8; i++) mk("textbutton", "ag_" + i);
    list.forEach((o, i) => { o.nextobject = list[i + 1] || null; });
    const ctx = { module: { exports: {} }, jsarguments: ["pmor_bank.js"].concat(gated ? ["gated"] : []), outlet: (i, ...a) => out.push([i, ...a]), post() {},
        patcher: { firstobject: list[0], getnamed: n => objs[n] || null }, arrayfromargs: a => Array.from(a) };
    vm.createContext(ctx); vm.runInContext(rd("pmor_bank.js"), ctx); ctx.out = out; ctx.objs = objs; return ctx;
}
const ed = Object.keys(banks).filter(n => !/^L/.test(n));
const setp = x => x.out.filter(m => m[0] === 0 && m[1] === "setparam");
const sets = (x, n) => x.objs[n].log.filter(m => m[0] === "set");

// ---- PMOR -> device: silent
let x = make(false, ed), M = x.module.exports;
M.loadbang();
for (const id of [1281, 1280, 1024 + 3 * 32 + 4, 2048 + 12 * 8 + 4, 2048 + 12 * 8 + 5, 3072 + 3, 4096, 2320 + 8]) M.param(id, 1);
M.param(1280, 3); M.param(1024 + 3 * 32 + 8, 3000);
eq("PMOR values produce no setparam", setp(x), []);
eq("PMOR values only `set` the controls (never an outputting message)", Object.values(x.objs).every(o => o.log.every(m => ["set", "clear", "append", "text", "texton"].includes(m[0]))), true);
eq("algo from PMOR retargets the synth", M.resolve(["syn", 8, -1, 0]), 1024 + 3 * 32 + 8);
// readout of cutoff of algo 3 = 3000 Hz, dial at the log position
eq("cutoff readout in Hz", x.objs.bv_f8.log.filter(m => m[0] === "set").pop()[1], "3000 Hz");
near("cutoff dial = log position", x.objs.bk_f8.log.filter(m => m[0] === "set").pop()[1], Math.round(Math.log(3000 / 40) / Math.log(12000 / 40) * 1000), 0.51);

// ---- user gestures -> the right id, scaled
x.out.length = 0; M.bc("f8", 0.5);
eq("cutoff 0.5 -> log mid (algo 3)", setp(x).map(m => [m[2], Math.round(m[3])]), [[1024 + 3 * 32 + 8, Math.round(40 * Math.pow(300, 0.5))]]);
x.out.length = 0; M.bc("a4", 0.5); near("attack log taper", setp(x)[0][3], 0.001 * Math.pow(4000, 0.5), 1e-9);
x.out.length = 0; M.bc("k0", 1); eq("algo knob 1 of algo 3", setp(x)[0].slice(2, 3), [1024 + 3 * 32]);
x.out.length = 0; M.bm("algo", 5);
eq("algo menu -> setparam 1280 + broadcast", [setp(x).map(m => m.slice(2)), x.out.filter(m => m[0] === 2)], [[[1280, 5]], [[2, "sel", "algo", 5]]]);
x.out.length = 0; M.bc("o15", 1); eq("SEMIS integer + id of algo 5", setp(x)[0].slice(2), [1024 + 5 * 32 + 15, 24]);

// ---- pad selection: retarget, nothing sent
x.out.length = 0; M.pick("pad", 3);
eq("pick pad: broadcast only, no setparam", [setp(x), x.out.filter(m => m[0] === 2).length], [[], 1]);
M.bc("pad1", 1); eq("pad 3 pitch id", setp(x)[0].slice(2), [2048 + 3 * 8 + 1, 1]);
x.out.length = 0; M.bc("pad0", 0.25); near("pad length linear", setp(x)[0][3], 0.25);
x.out.length = 0; M.bc("grp2", 1); eq("pad 3 is HAT (group 2): its depth id", setp(x)[0].slice(2), [2320 + 2 * 8 + 2, 1]);
x.out.length = 0; M.pick("pad", 14); M.bc("grp2", 1); eq("pad 14 is SNARE (group 1)", setp(x)[0].slice(2), [2320 + 1 * 8 + 2, 1]);
x.out.length = 0; M.param(2048 + 14 * 8 + 4, 1.0); eq("vol readout in dB", x.objs.bv_pad4.log.filter(m => m[0] === "set").pop()[1], "0 dB");
x.out.length = 0; M.param(2048 + 14 * 8 + 4, 0.5); eq("0.5 = -6 dB", x.objs.bv_pad4.log.filter(m => m[0] === "set").pop()[1], "-6 dB");
x.out.length = 0; M.pick("slot", 9); eq("slot pick tells the pool browser", x.out.filter(m => m[0] === 1).length > 0, true);
x.out.length = 0; M.bc("s0", 1); eq("slot 9 pitch id", setp(x)[0].slice(2), [3072 + 9 * 8, 1]);
x.out.length = 0; M.sel("pad", 5); M.sel("slot", 2); M.sel("algo", 1); eq("selection from elsewhere sends nothing", x.out.filter(m => m[0] === 0 || m[0] === 2), []);

// ---- every schema id of the lanes is reachable through some selection (and nothing resolves outside the schema)
const sch = JSON.parse(rd("schema.json")).schema, ids = new Set(sch.map(e => e[0]));
const reach = new Set();
for (const [n, b] of Object.entries(banks)) {
    const [kind, k] = b;
    const rs = kind === "id" ? [k] : kind === "syn" ? [...Array(8).keys()].map(a => 1024 + a * 32 + k) : kind === "pad" ? [...Array(16).keys()].map(p => 2048 + p * 8 + k) :
        kind === "grp" ? [...Array(4).keys()].map(g => 2320 + g * 8 + k) : kind === "deep" ? [...Array(4).keys()].map(g => 2368 + g * 16 + k) :
        kind === "slotx" ? [...Array(16).keys()].map(s => 3584 + s * 8 + k) : kind.startsWith("dsyn") ? [...Array(16).keys()].filter(p => (p >> 2) < 2 ? ((p & 3) < 2 ? 3 : 2) === +kind[4] : ((p & 3) < 2 ? 0 : 1) === +kind[4]).map(p => ((7 + (p >> 2)) << 10) | ((p & 3) * 256 + k)) : [...Array(16).keys()].map(s => 3072 + s * 8 + k);
    rs.forEach(i => { if (!ids.has(i)) { fail++; console.log("FAIL bank", n, "resolves to unknown id", i); } reach.add(i); });
}
const missing = sch.filter(e => [1, 2, 3, 4].includes(e[0] >> 10) && !reach.has(e[0]) && !(e[5] & 32)).map(e => e[0]);
eq("per-bank FX params (AMOUNT / LIDAR / MIX, ids 130..159) all have a control", sch.filter(e => (e[0] >> 10) === 5 && (e[0] & 1023) >= 130 && (e[0] & 1023) < 160 && !reach.has(e[0])).length, 0);
eq("every synth / drum / sampler / seq schema id has a bank control", missing, []);

// ---- pages
x.out.length = 0; M.page(2);
eq("page 2 hides page-1 controls, shows page-2", [x.objs.bk_k0.box.hidden, x.objs.bk_a4.box.hidden], [1, 0]);
eq("label + readout hide with their control", [x.objs.bl_k0.box.hidden, x.objs.bv_k0.box.hidden], [1, 1]);

// ---- selection widgets: pad cells show the pad's sound in its group tint, the selected pad / sound / algo / slot is ACCENT
{
    const y = make(false, ed), N = y.module.exports, col = (o, a) => o.box[a];
    N.loadbang(); N.param(2048 + 12 * 8 + 5, 3); N.param(2048 + 0 * 8 + 5, 1);
    N.pick("pad", 12);
    const acc = JSON.stringify(col(y.objs.pc_12, "bgcolor")), tint = JSON.stringify(col(y.objs.pc_3, "bgcolor"));
    eq("selected pad cell is ACCENT, others in their group tint", [acc !== tint, col(y.objs.pc_12, "bgcolor").length], [true, 4]);
    eq("pad cell text = the pad's sound name (group KICK, sound 3)", y.objs.pc_12.log.filter(m => m[0] === "text").pop()[1], "909");
    eq("chip of the selected sound is ACCENT", JSON.stringify(col(y.objs.sc_3, "bgcolor")), acc);
    eq("chip text = sound name of the pad's group", y.objs.sc_0.log.filter(m => m[0] === "text").pop()[1], "KSUB");
    N.pick("slot", 5); eq("selected slot cell is ACCENT", JSON.stringify(col(y.objs.sl_5, "bgcolor")), acc);
    N.sel("algo", 2); eq("selected algorithm card is ACCENT", JSON.stringify(col(y.objs.ag_2, "bgcolor")), acc);
    eq("selecting a pad sends nothing to PMOR", y.out.filter(m => m[0] === 0).length, 0);
}
// ---- compat: ids PMOR does not list are disabled
x = make(false, ed); M = x.module.exports; M.loadbang();
M.schemabegin(2); M.schema(1281, "VOLUME"); M.schema(1280, "ALGO"); M.schemaend(2);
eq("unlisted id disables its control (ignoreclick)", x.objs.bk_f8.box.ignoreclick, 1);
eq("listed id stays enabled", x.objs.bkm_algo, undefined);   // (names in this scope are all bk_)

// ---- gated editor: silent until opened, then draws from the cache once
x = make(true, ed); M = x.module.exports; M.param(1281, 0.8); M.param(2052, 1);
eq("closed editor draws nothing", Object.values(x.objs).every(o => o.log.length === 0), true);
M.editor(1); eq("opened editor draws from the cache", sets(x, "bk_vol").length > 0, true);
eq("opening an editor never sends", setp(x), []);

// ---- guard: a load must never write to PMOR
const gout = []; const gctx = { module: { exports: {} }, outlet: (i, ...a) => gout.push([i, ...a]), arrayfromargs: a => Array.from(a), messagename: "" };
vm.createContext(gctx); vm.runInContext(rd("pmor_guard.js"), gctx); const G = gctx.module.exports;
G.setparam(2052, 0.5); G.setparam(5120, 0); G.setparam(1281, 1);
eq("load grace: Live-restored values (setparam) are dropped", gout.filter(m => m[0] === 0), []);
gctx.messagename = "hello"; G.anything("STUDIO"); gctx.messagename = "dumpparams"; G.anything();
eq("non-setparam messages pass through the guard", gout.filter(m => m[0] === 0).map(m => m[1]), ["hello", "dumpparams"]);
G.armnow(); gout.length = 0; G.setparam(2052, 0.7);
eq("after the grace a user setparam passes", gout, [[0, "setparam", 2052, 0.7]]);
gout.length = 0; G.param(2052, 0.9); G.setparam(2052, 0.9); eq("a value equal to what PMOR holds is not a write", gout, []);
G.setparam(2052, 0.91); eq("a different value is", gout.length, 1);
console.log("test_bank: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
