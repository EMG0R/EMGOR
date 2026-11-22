// node tests/test_snap.js -- pmor_snap.js: status line, patchdiff buttons, the 2 s hold before `snapwrite`.
const fs = require("fs"), path = require("path"), vm = require("vm");
const src = fs.readFileSync(path.join(__dirname, "..", "max", "devices", "pmor_snap.js"), "utf8");
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
function mk() {
    const out = [], objs = { snap_adopt: { hidden: 1 }, snap_write: { hidden: 1 } };
    let now = 1000;
    const ctx = { module: { exports: {} }, outlet: (i, ...a) => out.push([i, ...a]), patcher: { getnamed: n => objs[n] || null }, Date: { now: () => now } };
    vm.createContext(ctx); vm.runInContext(src, ctx); ctx.out = out; ctx.objs = objs; ctx.adv = ms => { now += ms; }; return ctx;
}
const last = (x, i) => x.out.filter(m => m[0] === i).pop();
let x = mk(), M = x.module.exports;
M.snapshot("PS1~3~1024/1025~1024:0.5/1025:1"); eq("stored copy shown before PMOR reports", last(x, 0)[2], "set holds patch 3 (2 params), waiting for PMOR");
M.snapstate("adopted", 7, 3); eq("adopted", last(x, 0)[2], "set took PMOR's patch 3 (7 params)"); eq("no buttons", [x.objs.snap_adopt.hidden, x.objs.snap_write.hidden], [1, 1]);
M.snapstate("pushing", 4, 9); eq("pushing", last(x, 0)[2], "writing the set to PMOR: 4 of 9 left");
M.snapstate("synced", 9, 1, 2, 3, 1); eq("synced", last(x, 0)[2], "synced: 9 params written to PMOR patch 3, 1 clamped by PMOR, 2 sample slots, 1 not in pool");
M.snapstate("patchdiff", 5, 3);
eq("patchdiff text", last(x, 0)[2], "PMOR is on patch 5, the set holds patch 3");
eq("patchdiff hint names the write", last(x, 1)[2], "load on PMOR = write the set's params onto PMOR's current patch (5)");
eq("patchdiff shows both buttons", [x.objs.snap_adopt.hidden, x.objs.snap_write.hidden], [0, 0]);
M.adopt(); eq("adopt -> snapadopt", last(x, 2), [2, "snapadopt"]);
M.writearm(); x.adv(500); M.writego(); eq("short press writes nothing", x.out.filter(m => m[0] === 2 && m[1] === "snapwrite").length, 0);
M.writearm(); x.adv(2000); M.writego(); eq("2 s hold -> snapwrite", last(x, 2), [2, "snapwrite"]);
M.snapstate("synced", 1, 0, 0, 5, 0); eq("buttons hidden again", [x.objs.snap_adopt.hidden, x.objs.snap_write.hidden], [1, 1]);
x.out.length = 0; M.writego(); M.adopt(); eq("buttons hidden: clicks do nothing", x.out.filter(m => m[0] === 2).length, 0);
M.snapstate("cleared"); eq("cleared", last(x, 0)[2], "set copy cleared");
console.log("test_snap: %d passed, %d failed", pass, fail); process.exit(fail ? 1 : 0);
