// node tests/test_fx.js -- pmor_fx.js (FX browser of the control lane) + the saved control lane patcher: 128 engage cells + distortion algo are real Live parameters
const fs = require("fs"), path = require("path"), vm = require("vm");
const D = path.join(__dirname, "..", "max", "devices");
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
const objs = {}; const mk = n => objs[n] = objs[n] || { hidden: 0, msgs: [], message(...a) { this.msgs.push(a); } };
for (let i = 5120; i < 5248; i++) mk("p" + i); mk("fxsel"); mk("fxeff");
const ctx = { module: { exports: {} }, patcher: { getnamed: n => objs[n] || null }, arrayfromargs: a => Array.from(a), messagename: "" };
vm.createContext(ctx); vm.runInContext(fs.readFileSync(path.join(D, "pmor_fx.js"), "utf8"), ctx);
const m = (name, ...a) => { ctx.messagename = name; ctx.anything(...a); };
ctx.loadbang();
eq("bank 1 visible, others hidden", [objs.p5120.hidden, objs.p5136.hidden, objs.p5247.hidden], [0, 1, 1]);
m("bank", 2); eq("bank 3 visible", [objs.p5120.hidden, objs.p5152.hidden, objs.p5168.hidden], [1, 0, 1]);
const app = objs.fxeff.msgs.filter(x => x[0] === "append").slice(-16).map(x => x[1]);
eq("effect menu = the bank's 16 names", app.length, 16);
ctx.param(5152 + 3, 1); m("eff", 3);
const t = objs.fxsel.msgs.slice(-1)[0][1]; eq("readout names bank.effect and state", /^FX3\.04 \S+  ON/.test(t), true);
ctx.param(5152 + 3, 0); eq("readout follows PMOR", /off/.test(objs.fxsel.msgs.slice(-1)[0][1]), true);
// patcher: every FX cell is a live.text with a unique long name, the algo a live.menu, all wired to setparam
const pj = JSON.parse(fs.readFileSync(path.join(D, "PMOR lane control.maxpat"), "utf8")).patcher;
const bx = pj.boxes.map(b => b.box), lt = bx.filter(b => b.maxclass === "live.text"), lm = bx.filter(b => b.maxclass === "live.menu");
eq("128 live.text engage cells", lt.length, 128); eq("1 live.menu (distortion algo)", lm.length, 1);
const names = lt.concat(lm).map(b => b.saved_attribute_attributes.valueof.parameter_longname);
eq("unique parameter_longname", new Set(names).size, 129);
eq("registered in parameters block (129 FX + 23 per-bank params + the window-rect pattr)", Object.keys(pj.parameters).filter(k => k.startsWith("obj-")).length, 153);
eq("sample longname", lt[0].saved_attribute_attributes.valueof.parameter_longname.indexOf("FX1.01 "), 0);
const ids = new Set(bx.filter(b => /^prepend setparam /.test(b.text || "")).map(b => +b.text.split(" ")[2]));
eq("every cell sends setparam", [5120, 5247, 5248].every(i => ids.has(i)), true);
console.log("test_fx: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
