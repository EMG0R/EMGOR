// node tests/test_compat_ui.js -- pmor_ui.js compat: a schema stream missing known ids and carrying extra ids leaves the right controls
// enabled / disabled and the extras counted; unknown `param` ids never error; empty schema disables nothing; re-sync re-enables.
const fs = require("fs"), path = require("path"), vm = require("vm");
const src = fs.readFileSync(path.join(__dirname, "..", "max", "devices", "pmor_ui.js"), "utf8");
function make(gated) {
    const objs = {}, msgs = [];
    const mk = (cls, vn, colors) => { const o = { maxclass: cls, varname: vn, attrs: Object.assign({}, colors), box: {}, log: [] };
        o.getattr = a => o.attrs[a]; o.message = (...a) => { o.log.push(a); if (o.attrs[a[0]] !== undefined) o.attrs[a[0]] = a.slice(1); };
        o.setboxattr = (a, v) => { o.box[a] = v; }; objs[vn] = o; return o; };
    const col = [0.6, 0.9, 0.6, 1];
    const list = [mk("dial", "p2052", { needlecolor: col, outlinecolor: col }), mk("dial", "p2053", { needlecolor: col, outlinecolor: col }),
        mk("toggle", "p5120", { checkedcolor: col, elementcolor: col }), mk("umenu", "p1280", { textcolor: col }),
        mk("comment", "unknown", {}), mk("umenu", "unk", {}), mk("comment", "ver", {})];
    list.forEach((o, i) => { o.nextobject = list[i + 1] || null; });
    const patcher = { firstobject: list[0], getnamed: n => objs[n] || null };
    const ctx = { module: { exports: {} }, jsarguments: ["pmor_ui.js"].concat(gated ? ["gated"] : []), outlet() {}, post() {}, patcher, arrayfromargs: a => Array.from(a) };
    vm.createContext(ctx); vm.runInContext(src, ctx); ctx.objs = objs; return ctx;
}
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
const stream = (x, ids) => { x.module.exports.schemabegin(ids.length); ids.forEach(i => x.module.exports.schema(i[0], i[1], 0, 1, 0, 0, "G")); x.module.exports.schemaend(ids.length); };
let x = make(false);
stream(x, [[2052, "A"], [1280, "ALGO"], [1024, "SYN0"], [90001, "NEWPARAM"], [90002, "OTHER"]]);   // missing: 2053, 5120 ; extra: 90001, 90002
const ig = n => x.objs[n].box.ignoreclick;
eq("listed control enabled (p2052)", ig("p2052"), 0);
eq("listed control enabled (p1280)", ig("p1280"), 0);
eq("missing control disabled (p2053)", ig("p2053"), 1);
eq("missing control disabled (p5120 toggle)", ig("p5120"), 1);
eq("disabled dial dimmed", x.objs.p2053.attrs.needlecolor, [0.4314, 0.3765, 0.6275, 1]);
eq("enabled dial keeps its colour", x.objs.p2052.attrs.needlecolor, [0.6, 0.9, 0.6, 1]);
eq("unknown ids counted", x.objs.unknown.log.pop(), ["set", "+2 unknown params"]);
eq("unknown list built", x.objs.unk.log.filter(l => l[0] === "append").map(l => l[1]), ["NEWPARAM  -", "OTHER  -"]);
x.module.exports.param(90001, 0.25); x.module.exports.param(424242, 1);                              // value of an unknown id; id not even announced: no error
eq("unknown id value shown (name + value)", x.objs.unk.log.filter(l => l[0] === "append").map(l => l[1]).slice(-2), ["NEWPARAM  0.25", "OTHER  -"]);
eq("unknownIds", x.module.exports.unknownIds(), [90001, 90002]);
stream(x, [[2052, "A"], [2053, "B"], [5120, "C"], [1280, "ALGO"], [1024, "SYN0"]]);                    // later sync lists everything again, no extras
eq("re-enabled p2053", ig("p2053"), 0); eq("re-enabled p5120", ig("p5120"), 0);
eq("restored colour", x.objs.p2053.attrs.needlecolor, [0.6, 0.9, 0.6, 1]);
eq("no unknown -> all known", x.objs.unknown.log.pop(), ["set", "all params known"]);
x = make(false); x.module.exports.schemabegin(0); x.module.exports.schemaend(0);                        // empty schema (old firmware)
eq("empty schema disables nothing", ["p2052", "p2053", "p5120", "p1280"].map(ig), [0, 0, 0, 0]);
x = make(true); stream(x, [[2052, "A"]]);                                                              // gated + closed: nothing touched
eq("gated closed: no control touched", Object.values(x.objs).reduce((n, o) => n + o.log.length + Object.keys(o.box).length, 0), 0);
x.module.exports.editor(1);
eq("gated: applied on open (p2053 disabled)", ig("p2053"), 1);
x.module.exports.info("hello", "fw=abc1234", "proto=1", "schema=deadbeef");
eq("fw/proto line shown", x.objs.ver.log.pop(), ["set", "fw abc1234 proto 1"]);
const nv = x.objs.ver.log.length; x.module.exports.info("clock", "slave");
eq("unrelated info ignored", x.objs.ver.log.length, nv);
console.log("test_compat_ui: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
