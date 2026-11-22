// node tests/test_browser.js -- pmor_browser.js fed the REAL proto message stream (format: max/pmor.proto/pmor.proto.cpp onPresets / status):
// preset menu contents per engine (24 factory per drum group, 5 factory + user per synth algo), '-' names -> schema names, user -> 'user n',
// the firmware pad<->group mapping (drums.cpp drumPadGroup, NOT pad/4), replay on open, Apply messages, and the transport status line.
const fs = require("fs"), path = require("path"), vm = require("vm");
const src = fs.readFileSync(path.join(__dirname, "..", "max", "devices", "pmor_browser.js"), "utf8");
const sch = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "max", "devices", "schema.json"), "utf8"));
function make(args) {
    const out = [];
    const ctx = { module: { exports: {} }, jsarguments: ["pmor_browser.js"].concat(args || []), outlet: (i, ...a) => out.push([i, ...a]), post() {}, arrayfromargs: a => Array.from(a) };
    vm.createContext(ctx); vm.runInContext(src, ctx); ctx.out = out; return ctx;
}
let pass = 0, fail = 0;
const eq = (n, g, w) => { const a = JSON.stringify(g), b = JSON.stringify(w); if (a === b) pass++; else { fail++; console.log("FAIL", n, a, "!=", b); } };
// what the firmware writes: engine 0..3 24 factory entries (name `-` unless named), 4..11: 5 factory + user entries
function stream(x, opt) {
    x.presetbegin();
    for (let e = 0; e < 12; e++) {
        const nf = e < 4 ? 24 : 5;
        for (let n = 0; n < nf; n++) x.preset(e, n, "f", e < 4 ? "-" : (e === 5 ? "Named" + n : "-"));
        if (e >= 4) { x.preset(e, nf, "u", "My lead"); x.preset(e, nf + 1, "u", "-"); }
    }
    x.presetend(12 * 24);
}
const items = x => { let r = [], on = false; x.out.forEach(m => { if (m[0] === 0 && m[1] === "clear") r = []; if (m[0] === 0 && m[1] === "append") r.push(m[2]); }); return r; };
const last = (x, i) => { for (let k = x.out.length - 1; k >= 0; k--) if (x.out[k][0] === i) return x.out[k].slice(1); return null; };
// ---- the editor browser is `gated`: presets arrive while closed, the menu is filled on open
let x = make(["0", "gated"]);
stream(x);
eq("closed: nothing drawn", x.out.length, 0);
x.editor(1);
let it = items(x);
eq("KICK pad 12 on open: 24 factory names from the schema", it, sch.drum_sounds[0].map(n => "f " + n));
eq("engine menu + pad menu shown", [last(x, 4), last(x, 5)], [["set", 0], ["set", 12]]);
// ---- the engine menu and the firmware pad mapping
x.out.length = 0; x.engine(3);
eq("PERC engine: 24 names", items(x), sch.drum_sounds[3].map(n => "f " + n));
eq("engine PERC moves the pad menu to the first PERC pad (0)", last(x, 5), ["set", 0]);
x.out.length = 0; x.padsel(2);                       // pad 2 = TR quadrant = HAT, not pad/4 = PERC
eq("pad 2 is HAT: engine follows", last(x, 4), ["set", 2]);
eq("pad 2 is HAT: items", items(x), sch.drum_sounds[2].map(n => "f " + n));
eq("pad->group table (drums.cpp)", [0, 1, 2, 3, 4, 8, 12, 14, 15].map(p => x.groupOfPad(p)), [3, 3, 2, 2, 3, 0, 0, 1, 1]);
x.out.length = 0; x.padsel(12); eq("pad 12 -> KICK", last(x, 4), ["set", 0]);
// ---- synth algo: 5 factory + user entries, unnamed user -> user n
x.out.length = 0; x.engine(4);
eq("synth algo 0: 5 factory + 2 user", items(x), sch.synth_presets.map(n => "f " + n).concat(["u My lead", "u user 6"]));
x.out.length = 0; x.engine(5);
eq("explicit names win over schema", items(x).slice(0, 2), ["f Named0", "f Named1"]);
x.out.length = 0; x.engine(11); eq("engine selection changes the items (algo 7)", items(x).length, 7);
// ---- apply
x.out.length = 0; x.engine(0); x.padsel(13); x.presetpick(3); x.out.length = 0; x.dopreset();
eq("drum apply: preset <engine> <n> <pad>", x.out, [[1, "preset", 0, 3, 13]]);
x.engine(6); x.presetpick(2); x.out.length = 0; x.dopreset();
eq("synth apply: preset <4+algo> <n>", x.out, [[1, "preset", 6, 2]]);
x.presetpick(5); x.out.length = 0; x.dopreset(); eq("user entry applies by its own n", x.out, [[1, "preset", 6, 5]]);
// ---- never arrived / empty engine
x = make(["gated"]); x.editor(1);
eq("no stream yet: menu says not loaded", items(x), ["(not loaded: connect)"]);
x.out.length = 0; x.state("ready"); eq("link comes up with the editor open: browser asks for presets", x.out, [[1, "presets"]]);
x.presetbegin(); x.presetend(0); eq("empty result", items(x), ["(none for this engine)"]);
// ---- not gated (lane / default instance): draws at once
x = make(["4"]); x.presetbegin(); x.preset(4, 0, "f", "-"); x.presetend(1); eq("ungated draws on presetend", items(x), ["f INIT"]);
// ---- transport line (status <mode> <policy> <running> <bpm> <cpu> <overruns> <drops> <bus> <role>)
const T = require_(x);
function require_(ctx) { return ctx.module.exports; }
const tt = T.transText;
eq("AUTO + role follow", tt("AUTO", 1, 124, 21, 0, 0, 1, "follow"), "run 124.0 | follow | cpu 21 ovr 0 drop 0");
eq("AUTO + role master", tt("AUTO", 0, 120, 5, 0, 0, 0, "master"), "stop 120.0 | master | cpu 5 ovr 0 drop 0");
eq("policy shown when not AUTO", tt("FOLLOW", 1, 124, 21, 0, 0, 1, "follow"), "run 124.0 | follow (FOLLOW) | cpu 21 ovr 0 drop 0");
eq("role beats a stale bus flag", tt("AUTO", 1, 124, 21, 0, 0, 1, "master"), "run 124.0 | master | cpu 21 ovr 0 drop 0");
eq("old proto without role: inferred from bus", tt("AUTO", 1, 124, 21, 0, 0, 1, undefined), "run 124.0 | follow | cpu 21 ovr 0 drop 0");
eq("old proto without role, no bus: master", tt("AUTO", 1, 124, 21, 0, 0, 0, undefined), "run 124.0 | master | cpu 21 ovr 0 drop 0");
eq("never says AUTO: master", /AUTO/.test(tt("AUTO", 1, 120, 1, 0, 0, 0, "master")), false);
x = make(["gated"]); x.editor(1); x.out.length = 0;
x.status("STUDIO", "AUTO", 1, 124, 21, 0, 0, 1, "follow"); eq("status -> comment", last(x, 3), ["trans", "set", "run 124.0 | follow | cpu 21 ovr 0 drop 0"]);
x.state("disconnected"); eq("not connected -> standalone", last(x, 3), ["trans", "set", "standalone"]);
// ---- pool rows (new `pool` layout), selection, hold-to-confirm delete (injected clock), recycle bin
{
    let now = 1000000; const clk = () => now;
    const mk = () => { const y = make(["gated"]); y._setclock(clk); y.editor(1); y.poolbegin();
        y.pool(1100, 48000, "S1100", 1, "3,9", "-", 48000); y.pool(1101, 96000, "S1101", 0, "-", "-", 48000); y.pool(1102, 24000, "S1102", 2); y.pool(51, 2000, "P03S03", 0, "-", 3, 48000); y.poolend(4); y.out.length = 0; return y; };
    const rows = y => { let r = []; y.out.forEach(m => { if (m[0] === 2 && m[1] === "clear") r = []; if (m[0] === 2 && m[1] === "append") r.push(m[2]); }); return r; };
    const info = y => { for (let k = y.out.length - 1; k >= 0; k--) if (y.out[k][0] === 3 && y.out[k][1] === "poolinfo") return y.out[k][3]; return null; };
    const sent = y => y.out.filter(m => m[0] === 1);
    let y = mk(); y.drawpool();
    eq("pool rows: cached * / not cached - / bad !, seconds, length, slots, legacy patch", rows(y),
        ["  * S1100  1.0s 48000  -> slot 3,9", "  - S1101  2.0s 96000", "  ! S1102  0.5s 24000", "  - P03S03  0.0s 2000  patch 3"]);
    y.sample("S1101", "pulled"); eq("sample pulled -> cached mark", rows(y)[1], "  * S1101  2.0s 96000");
    // old 4-arg layout still works
    y.poolbegin(); y.pool(1200, 48000, "S1200", 1); y.poolend(1); eq("4-arg pool message", rows(y), ["  * S1200  1.0s 48000"]);
    y = mk();
    // no selection / no arm / too early / too late
    y.delarm(); y.delgo(); eq("nothing selected", [info(y), sent(y)], ["nothing selected", []]);
    y.sel(1, 1); eq("sel marks the row", rows(y)[1].slice(0, 2), "x ".slice(0, 2));
    y.delgo(); eq("delgo without arm", [info(y), sent(y)], ["hold delete for 2 s", []]);
    y.delarm(); now += 1500; y.delgo(); eq("1.5 s hold is not enough", [info(y), sent(y)], ["hold delete for 2 s", []]);
    y.delarm(); now += 1999; y.delgo(); eq("1999 ms is not enough", sent(y), []);
    y.delarm(); now += 11000; y.delgo(); eq("a stale arm (11 s) is refused", [info(y), sent(y)], ["hold delete for 2 s", []]);
    y.delarm(); now += 2000; y.delgo(); eq("2 s hold executes: deletepool <id>", sent(y), [[1, "deletepool", 1101]]);
    y.out.length = 0; y.delgo(); eq("selection cleared after the delete; a second go does nothing", sent(y), []);
    y.out.length = 0; y.deleted(1101, "ok"); eq("deleted -> status + bin refresh", [info(y), sent(y)], ["deleted S1101 (in the recycle bin)", [[1, "bin"]]]);
    // multi-select, one message
    y = mk(); y.sel(1, 1); y.sel(3, 1); y.sel(0, 1); y.sel(0, 0); y.delarm(); now += 2500; y.delgo(); eq("multi-select: ids in pool order in ONE message", sent(y), [[1, "deletepool", 1101, 51]]);
    // used by a current slot: two-stage
    y = mk(); y.sel(0, 1); y.sel(1, 1);
    y.delarm(); now += 2500; y.delgo();
    eq("used sample: first valid go only warns (names the slot), sends nothing", [/S1100 in slot 3,9/.test(info(y)), /again/.test(info(y)), sent(y)], [true, true, []]);
    y.delgo(); eq("go again without a new arm: nothing", sent(y), []);
    y.delarm(); now += 2500; y.delgo(); eq("second arm after the warning executes", sent(y), [[1, "deletepool", 1100, 1101]]);
    y = mk(); y.sel(0, 1); y.delarm(); now += 2500; y.delgo(); y.sel(1, 1); y.delarm(); now += 2500; y.delgo();
    eq("changing the selection after the warning warns again", [sent(y), /S1100 in slot/.test(info(y))], [[], true]);
    y = mk(); y.sel(0, 1); y.delarm(); now += 2500; y.delgo(); y.selclear(); y.sel(0, 1); y.delarm(); now += 2500; y.delgo(); eq("clear + reselect resets the warning stage", sent(y), []);
    // reload keeps the selection of ids that still exist
    y = mk(); y.sel(1, 1); y.poolbegin(); y.pool(1101, 96000, "S1101", 1); y.pool(1300, 1000, "S1300", 1); y.poolend(2); eq("selection survives a pool reload", y.selectedIds(), [1101]);
    // bin
    y = mk(); y.out.length = 0; y.bin(); eq("bin (no args) asks proto", sent(y), [[1, "bin"]]);
    y.out.length = 0; y.binbegin(); y.bin(1101, 96000, "S1101", "90", 5); y.bin(1102, 24000, "S1102", "7200", 2); y.binend(2);
    const bi = []; y.out.forEach(m => { if (m[0] === 6 && m[1] === "append") bi.push(m[2]); });
    eq("bin rows (outlet 6): age + boots_left", bi, ["S1101  2.0s  deleted 2m ago, 5 boots left", "S1102  0.5s  deleted 2h ago, 2 boots left"]);
    y.binsel(1); y.out.length = 0; y.binrestore(); eq("binrestore sends the selected row's id", sent(y), [[1, "binrestore", 1102]]);
    y.out.length = 0; y.binrestored(1102, "ok"); eq("binrestored -> refresh bin", [info(y), sent(y)], ["restored S1102", [[1, "bin"]]]);
    y.out.length = 0; y.binemptygo(); eq("empty bin without arm: nothing", [info(y), sent(y)], ["hold empty bin for 2 s", []]);
    y.binemptyarm(); now += 1000; y.binemptygo(); eq("empty bin 1 s: nothing", sent(y), []);
    y.binemptyarm(); now += 2000; y.binemptygo(); eq("empty bin 2 s: binempty", sent(y), [[1, "binempty"]]);
    y.delarm(); now += 2500; y.binemptygo(); eq("a delete arm does not arm the bin", sent(y), [[1, "binempty"]]);
    y = mk(); y.binbegin(); y.binend(0); y.binemptyarm(); now += 2500; y.out.length = 0; y.binemptygo(); eq("empty bin on an empty list: nothing sent", [info(y), sent(y)], ["bin is empty", []]);
    y.out.length = 0; y.nobin(); eq("nobin: status text, nothing sent", [info(y), sent(y)], ["firmware has no recycle bin yet - nothing was changed", []]);
    eq("age formats", [y.module.exports.fmtAge("30"), y.module.exports.fmtAge("120"), y.module.exports.fmtAge("86400")], ["30s", "2m", "24h"]);
}
// ---- save as user preset (drums / synth editors): name sanitising, slot picker (user slots only), overwrite needs a second click, factory slots never offered
{
    let now = 5000000; const clk = () => now;
    const mk = (args) => { const y = make(args); y._setclock(clk); y.presetbegin();
        for (let e = 0; e < 12; e++) { const nf = e < 4 ? 24 : 5; for (let n = 0; n < nf; n++) y.preset(e, n, "f", "-"); }
        y.preset(4, 6, "u", "My lead"); y.preset(4, 9, "u", "-"); y.preset(1, 2, "u", "SNR"); y.presetend(40); y.editor(1); y.out.length = 0; return y; };
    const sinfo = y => { for (let k = y.out.length - 1; k >= 0; k--) if (y.out[k][0] === 3 && y.out[k][1] === "saveinfo") return y.out[k][3]; return null; };
    const slots = y => { let r = []; y.out.forEach(m => { if (m[0] === 7 && m[1] === "clear") r = []; if (m[0] === 7 && m[1] === "append") r.push(m[2]); }); return r; };
    const sent = y => y.out.filter(m => m[0] === 1);
    // synth: follows algo (selui + saveparam 1280), never offers factory 0..4
    let y = mk(["4", "gated", "synth"]);
    y.engine(4); let sl = slots(y);
    eq("synth slot list starts at 5 (no factory slot)", [sl.length, sl[0], sl[sl.length - 1]], [27, "5  (empty)", "31  (empty)"]);
    eq("existing user slots show their name (unnamed -> user n)", [sl[1], sl[4]], ["6  My lead", "9  user 9"]);
    eq("default slot = first empty (5)", y.out.filter(m => m[0] === 7 && m[1] === "set").pop(), [7, "set", 0]);
    y.out.length = 0; y.selui("algo", 3); eq("synth editor follows the algo selection (engine 7)", slots(y).length, 27);
    y.out.length = 0; y.selui("pad", 3); eq("synth editor ignores pad selection", y.out.length, 0);
    y.saveparam(1280, 5); y.savename("my pad!"); y.saveslotpick(0); y.out.length = 0; y.dosave();
    eq("free slot saves at once: engine 9 (algo 5), slot 5, name with _ for space", sent(y), [[1, "savepreset", 9, 5, "MY_PAD"]]);
    y.out.length = 0; y.presetsaved(9, 5, "ok"); eq("ok message", sinfo(y), "saved MY PAD to slot 5");
    // name sanitising
    y.savename("abcdefghijkl"); y.out.length = 0; y.dosave(); eq("name cut to 9", sent(y)[0].slice(1), ["savepreset", 9, 5, "ABCDEFGHI"]);
    y.savename("a;b/c_d"); y.out.length = 0; y.dosave(); eq("only A-Z 0-9 space - survive", sent(y)[0][4], "ABCD");
    y.savename("  "); y.out.length = 0; y.dosave(); eq("empty name: nothing sent", [sent(y), sinfo(y)], [[], "enter a name"]);
    eq("cleanName", [y.module.exports.cleanName("lead-1 x!"), y.module.exports.cleanName("x\ty")], ["LEAD-1 X", "XY"]);
    // overwrite: first click warns, second click (same slot + name, <= 10 s) saves; anything changed in between re-arms
    y = mk(["4", "gated", "synth"]); y.engine(4); y.savename("new"); y.saveslotpick(1); y.out.length = 0;
    y.dosave(); eq("existing user slot: first click only warns", [sent(y), sinfo(y)], [[], "overwrite My lead? click save again"]);
    now += 3000; y.out.length = 0; y.dosave(); eq("second click saves", sent(y), [[1, "savepreset", 4, 6, "NEW"]]);
    y.out.length = 0; y.dosave(); eq("and the arm is spent: warns again", [sent(y), sinfo(y)], [[], "overwrite My lead? click save again"]);
    now += 11000; y.out.length = 0; y.dosave(); eq("second click after 10 s only re-warns", sent(y), []);
    y.dosave(); y.savename("other"); y.out.length = 0; y.dosave(); eq("changing the name re-arms", sent(y), []);
    y.dosave(); y.saveslotpick(4); y.out.length = 0; y.dosave(); eq("changing the slot re-arms (slot 9 = unnamed user preset)", [sent(y), sinfo(y)], [[], "overwrite user 9? click save again"]);
    y.out.length = 0; y.engine(5); y.dosave(); eq("changing the engine re-arms; empty slot there saves at once", sent(y).length, 1);
    // factory slot can never be chosen
    y = mk(["4", "gated", "synth"]); y.engine(4); y.savename("x");
    for (let i = 0; i < 27; i++) { y.saveslotpick(i); y.out.length = 0; y.dosave(); y.dosave(); const m = sent(y)[0]; if (m && m[3] < 5) eq("factory slot offered", m, null); }
    y.saveslotpick(-1); y.saveslotpick(99); y.out.length = 0; y.dosave(); y.dosave(); eq("bad picks cannot reach a factory slot", sent(y).every(m => m[3] >= 5), true);
    // errors
    y.out.length = 0; y.presetsaved(4, 5, "err", "unsupported"); eq("older firmware text", sinfo(y), "save not supported by this PMOR firmware");
    y.presetsaved(4, 5, "err", "nosave"); eq("nosave text", sinfo(y), "PMOR has saving disabled (NOSAVE)");
    y.presetsaved(4, 5, "err", "range"); eq("other error text", sinfo(y), "save failed: range");
    // presets not loaded
    y = make(["4", "gated", "synth"]); y.editor(1); y.savename("x"); y.out.length = 0; y.dosave(); eq("no preset list yet: refuse (cannot know what would be overwritten)", [sent(y), sinfo(y)], [[], "presets not loaded: connect first"]);
    // drums: follows pad -> group, slots 0..31, the pad's sound is saved under the group engine
    y = mk(["0", "gated", "drums"]); y.padsel(2); sl = slots(y);
    eq("drums slots 0..31, group HAT has none yet", [sl.length, sl[0], sl[31]], [32, "0  (empty)", "31  (empty)"]);
    y.selui("pad", 13); y.out.length = 0; y.selui("pad", 13); y.savename("kick a"); y.saveslotpick(7); y.out.length = 0; y.dosave();
    eq("pad 13 (KICK group, engine 0): slot 7 saves engine 0", sent(y), [[1, "savepreset", 0, 7, "KICK_A"]]);
    y.selui("pad", 14); y.saveslotpick(2);   // pad 14 = SNARE (engine 1)
    y.out.length = 0; y.dosave(); eq("pad 14 = SNARE group: slot 2 holds user SNR -> warns", [sent(y), sinfo(y)], [[], "overwrite SNR? click save again"]);
    y.out.length = 0; y.selui("algo", 2); eq("drums editor ignores algo selection", y.out.length, 0);
    // a refreshed list (after ok) redraws the slots with the new name
    y = mk(["4", "gated", "synth"]); y.engine(4); y.out.length = 0; y.presetbegin(); y.preset(4, 0, "f", "-"); y.preset(4, 5, "u", "FRESH"); y.presetend(2);
    eq("preset list refresh redraws the slot picker", slots(y).slice(0, 2), ["5  FRESH", "6  (empty)"]);
}
// ---- duplicate-CRC groups: one row per unique crc+len, usage = current patch only ("?" = unknown, never "unused"), safe group delete
{
    let now = 2000000; const clk = () => now;
    // kick: S1100 (slot 3), S1105, S1109 (x3, crc A); snare S1200 + S1201 (x2, crc B, none in the patch); lone S1300 (crc C); legacy P01S01 (no crc); S1400 same crc A but other length
    const mk = (known) => { const y = make(["gated"]); y._setclock(clk); y.editor(1); y.poolbegin();
        const k = known === undefined ? 1 : known;
        y.pool(1105, 48000, "S1105", 1, "-", "-", 48000, "x0000A1A1", k); y.pool(1100, 48000, "S1100", 1, "3", "-", 48000, "x0000A1A1", k);
        y.pool(1200, 24000, "S1200", 0, "-", "-", 48000, "x0000B2B2", k); y.pool(1109, 48000, "S1109", 0, "-", "-", 48000, "x0000A1A1", k);
        y.pool(1201, 24000, "S1201", 0, "-", "-", 48000, "x0000B2B2", k); y.pool(1300, 9000, "S1300", 0, "-", "-", 48000, "x0000C3C3", k);
        y.pool(17, 1000, "P01S01", 0, "-", 1, 48000, "-", k); y.pool(1400, 1000, "S1400", 0, "-", "-", 48000, "x0000A1A1", k); y.poolend(8); y.out.length = 0; return y; };
    const rows = y => { let r = []; y.out.forEach(m => { if (m[0] === 2 && m[1] === "clear") r = []; if (m[0] === 2 && m[1] === "append") r.push(m[2]); }); return r; };
    const info = y => { for (let k = y.out.length - 1; k >= 0; k--) if (y.out[k][0] === 3 && y.out[k][1] === "poolinfo") return y.out[k][3]; return null; };
    const sent = y => y.out.filter(m => m[0] === 1);
    const go = y => { y.delarm(); now += 2500; y.delgo(); };
    let y = mk(); y.drawpool();
    eq("groups: one row per crc+len, count badge, usage per copy (slot n / ?), singles unchanged", rows(y), [
        "  * S1100 x3  1.0s 48000  [S1100 slot 3; S1105 ?; S1109 ?]",
        "  - S1200 x2  0.5s 24000  [S1200 ?; S1201 ?]",
        "  - S1300  0.2s 9000", "  - P01S01  0.0s 1000  patch 1", "  - S1400  0.0s 1000"]);
    eq("never says unused", rows(y).join("|").indexOf("unused"), -1);
    y.expand(0); eq("expand: indented child rows, one per copy", rows(y).slice(0, 5), [
        "  * S1100 x3  1.0s 48000  [S1100 slot 3; S1105 ?; S1109 ?]", "     * S1100  slot 3", "     * S1105  ?", "     - S1109  ?", "  - S1200 x2  0.5s 24000  [S1200 ?; S1201 ?]"]);
    y.expandcur(); eq("expandcur on the current row collapses again", rows(y).length, 5);
    y.poolpick(0); y.pull(); eq("pull on a group row pulls its representative (the referenced copy)", sent(y).slice(-1), [[1, "pullid", 1100]]);
    // group delete with a referenced copy: deletes ONLY unreferenced copies, keeps the referenced one, needs the second hold
    y = mk(); y.sel(0, 1); y.out.length = 0; go(y);
    eq("group delete: first hold only warns (names delete + keep, other patches unchecked), sends nothing", [sent(y), /delete S1105 S1109, keep S1100 \(slot 3\)/.test(info(y)), /Other patches not checked/.test(info(y))], [[], true, true]);
    y.out.length = 0; go(y); eq("second hold deletes the two unreferenced copies only", sent(y), [[1, "deletepool", 1105, 1109]]);
    // group with no referenced copy: keeps the lowest id
    y = mk(); y.sel(1, 1); go(y); y.out.length = 0; go(y); eq("no referenced copy: lowest id kept, the rest deleted", sent(y), [[1, "deletepool", 1201]]);
    // usage unknown (slot map not synced): nothing is deletable through the group row
    y = mk(0); y.sel(0, 1); y.out.length = 0; go(y);
    eq("usage unknown (known=0): group delete sends nothing, says so", [sent(y), /Nothing to delete/.test(info(y))], [[], true]);
    y = mk(0); y.sel(1, 1); y.out.length = 0; go(y); y.out.length = 0; go(y); eq("known=0 group: still nothing sent after a second hold", sent(y), []);
    // copy of a different length is not grouped; legacy / no-crc not grouped; a bad entry is not grouped
    y = mk(); y.poolbegin(); y.pool(1, 100, "S1", 2, "-", "-", 48000, "x00000001", 1); y.pool(2, 100, "S2", 1, "-", "-", 48000, "x00000001", 1); y.poolend(2);
    y.drawpool(); eq("a bad (refused) entry is never grouped", rows(y).length, 2);
    // explicit child marking keeps the OLD single-id rules (warn if used by the current patch, then force)
    y = mk(); y.expand(0); y.sel(1, 1); y.out.length = 0; go(y);
    eq("child row of a referenced copy: old warning stage", [sent(y), /USED BY CURRENT PATCH: S1100 in slot 3/.test(info(y))], [[], true]);
    y.out.length = 0; go(y); eq("then the old force", sent(y), [[1, "deletepool", 1100]]);
    // marking a group does not delete the whole group even when everything is selectable; mark group + all children = explicit user choice
    y = mk(); y.sel(1, 1); y.selclear(); y.out.length = 0; go(y); eq("selclear drops group marks", [sent(y), info(y)], [[], "nothing selected"]);
    // selection of a group survives a reload, group + marker shows x
    y = mk(); y.sel(0, 1); y.poolbegin(); y.pool(1100, 48000, "S1100", 1, "3", "-", 48000, "x0000A1A1", 1); y.pool(1105, 48000, "S1105", 1, "-", "-", 48000, "x0000A1A1", 1); y.poolend(2);
    y.drawpool(); eq("group mark survives a reload", rows(y)[0].slice(0, 2), "x ");
    y.poolbegin(); y.pool(1100, 48000, "S1100", 1, "3", "-", 48000, "x0000A1A1", 1); y.poolend(1); y.drawpool(); eq("group that collapses to one copy becomes a single row, mark dropped", [rows(y), y.selectedGroups().length], [["  * S1100  1.0s 48000  -> slot 3"], 0]);
}
console.log("test_browser: " + pass + " passed, " + fail + " failed"); process.exit(fail ? 1 : 0);
