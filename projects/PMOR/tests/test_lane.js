// node tests/test_lane.js -- pmor_lane.js forwards the right channels unchanged: notes, pitch bend (ch2), CCs on ch4/ch5 incl. seed CCs 110-113; meters drive the UI
const fs = require("fs"), path = require("path"), vm = require("vm");
const src = fs.readFileSync(path.join(__dirname, "..", "max", "devices", "pmor_lane.js"), "utf8");
function lane(args) {
    const out = [], sets = {};
    const inner = { getnamed: n => ({ message: (...a) => { sets[n] = a; } }) };
    const patcher = { getnamed: n => ({ subpatcher: () => inner }) };
    const ctx = { module: { exports: {} }, jsarguments: ["pmor_lane.js"].concat(args), outlet: (i, ...a) => out.push(a), inlets: 0, outlets: 0, post() {}, patcher };
    vm.createContext(ctx); vm.runInContext(src, ctx); ctx.out = out; ctx.sets = sets; return ctx;
}
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
let l = lane(["2"]);
[0xE1, 0x00, 0x50, 0x91, 60, 100].forEach(b => l.module.exports.feed(b));          // ch2 pitch bend, ch2 note-on
eq("synth lane: pitch bend + note pass", l.out, [[0xE1, 0, 0x50], [0x91, 60, 100]]);
l = lane(["2"]); [0xE0, 1, 2, 0x90, 60, 100, 0xB1, 7, 9].forEach(b => l.module.exports.feed(b));
eq("synth lane drops other channels", l.out, [[0xB1, 7, 9]]);
l = lane(["4", "5", "meters"]); l.module.exports.editor(1);
const f = b => l.module.exports.feed(b);
[0xB3, 1, 64, 0xB3, 5, 30, 0xB4, 16, 127, 0xB2, 1, 99].forEach(f);                   // ch4 CC1, ch4 CC5, ch5 CC16 (fx engage), ch3 (ignored)
eq("control lane forwards ch4 and ch5 only", l.out, [[0xB3, 1, 64], [0xB3, 5, 30], [0xB4, 16, 127]]);
{ const c = lane(["4", "5", "meters"]); [0xB3, 1, 64].forEach(b => c.module.exports.feed(b)); eq("meters draw nothing while the editor is closed", c.sets.m1, undefined); c.module.exports.editor(1); eq("meters replay on open", c.sets.m1, ["set", 64]); }
eq("meters: CC1 -> m1, CC5 -> m5", [l.sets.m1, l.sets.m5], [["set", 64], ["set", 30]]);
l = lane(["4", "5", "meters"]); l.module.exports.editor(1);
[0xB3, 110, 1, 0xB3, 111, 2, 0xB3, 112, 3, 0xB3, 113, 4].forEach(b => l.module.exports.feed(b));
eq("seed CCs forwarded unchanged", l.out.map(m => m[1]), [110, 111, 112, 113]);
eq("seed shown", l.sets.seed, ["set", "seed " + ((1 << 21) | (2 << 14) | (3 << 7) | 4)]);
l = lane(["4", "5", "meters"]); [0xFA, 0xF8, 0xB3, 7, 100].forEach(b => l.module.exports.feed(b));
eq("realtime bytes ignored, running status ok", l.out, [[0xB3, 7, 100]]);
l = lane(["4", "5", "meters"]); [0xB3, 1, 10, 2, 20].forEach(b => l.module.exports.feed(b));
eq("running status second message", l.out, [[0xB3, 1, 10], [0xB3, 2, 20]]);
l = lane(["2", "remap"]);   // keyboard / clips arrive on the track's channel (1): re-channelled to the synth lane's ch2
[0x90, 60, 100, 0x80, 60, 0, 0xE0, 0, 0x50, 0xB0, 1, 5].forEach(b => l.module.exports.feed(b));
eq("remap lane: any channel -> ch2 (note on/off, bend, CC)", l.out, [[0x91, 60, 100], [0x81, 60, 0], [0xE1, 0, 0x50], [0xB1, 1, 5]]);
l = lane(["1", "remap"]); [0x95, 36, 90].forEach(b => l.module.exports.feed(b));
eq("remap lane: drums gets ch1", l.out, [[0x90, 36, 90]]);
console.log("test_lane: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
