// pmor_browser.js -- preset + sample-pool + status browser helper for the PMOR devices (dependency-light).
// Arguments: a number = default engine for the preset menu (0..3 drum groups, 4..11 synth algos); the word `gated` = editor instance:
//   it keeps its data (presets, pool, status) but draws NOTHING (no outlet traffic) until pmor_edit.js sends `editor 1`, then it redraws once.
// Inlet: the pmor_ui_status stream (proto outlet 2) plus UI messages. Outlets:
//   0 preset umenu commands (clear/append/set)   1 messages for [send pmor_tx]
//   2 pool umenu commands                        3 "<tag> set <text>" for comments (tags: trans store poolinfo)
//   4 `set <engine>` for the engine umenu        5 `set <pad>` for the pad umenu (kept in step by the pad<->group mapping)
//   6 recycle-bin umenu commands (clear/append/set)  -- all delete / bin STATUS TEXT goes to outlet 3 as `poolinfo set <text>` (no new comment tag)
// Pool inbound (proto outlet 2): poolbegin / pool <id> <len_samples> <name> <cached 0|1|2> <slots|-> <patch|-> <rate> / poolend <n>  (cached 2 = bad CRC / refused;
//   slots = current-patch slots using the sample, `3,5`; patch = legacy patch number; the last three args are optional = old 4-arg layout still works). Row text, outlet 2:
//   `[x ][*|-|!] <label> [<name>]  <sec>s <len> [-> slot <list>] [patch <n>]` (x selected, * cached, ! bad).
//   Optional 8th/9th args: crc (`x<8 hex>` = CRC-32 from the manifest, `-` = none/untrusted) and known (1 = current-patch slot map valid, 0 = not synced).
// DUPLICATE GROUPS: entries with the same crc AND length (crc present, not bad) collapse into ONE row `<label> <name> x<N>  <sec>s <len>  [S1100 slot 3,9; S1105 ?; ...]`.
//   `slot n` = referenced by the CURRENT patch; `?` = unknown (other patches are not visible to the host; PMOR only reports the current patch) -- NEVER shown as "unused".
//   `expand <row>` / `expandcur` toggles the group's copies as indented child rows (each markable on its own = the old single-id delete rules).
//   Marking a GROUP row and holding delete removes ONLY copies that are not used by the current patch and whose usage is known (known=1), always keeps >= 1 copy
//   (all current-patch copies, else the lowest id), and ALWAYS needs the second hold (the other patches are unchecked). Nothing deletable -> nothing is sent.
// DELETE / BIN: UI -> this device (all plain messages to the js inlet):
//   sel <i> <0|1> (row i of the pool umenu in/out of the selection set)   selcur (toggle the pool umenu's current row)   selclear
//   delarm (starts the 2 s arm; harmless alone)   delgo (executes only if delarm was 2..10 s ago AND something is selected; if a selected sample is used by a slot of the CURRENT
//   patch the first go only warns, a SECOND delarm + 2 s + delgo executes; else poolinfo says `hold delete for 2 s`)
//   bin (refresh the bin list)   binsel <i> (bin umenu row)   binrestore (restore the selected bin row)   binemptyarm / binemptygo (same 2 s hold, separate arm, permanent)
// SAVE AS USER PRESET (drums + synth editors; arguments `drums` | `synth` choose what selui follows; the default engine number still comes first):
//   savename <text...> (name field; sanitised to A-Z 0-9 space - , <= 9)   saveslotpick <i> (slot umenu row)   dosave (the save button; an EXISTING user slot needs a SECOND dosave
//   within 10 s, `overwrite <name>?`)   selui <algo|pad> <n> (from [receive pmor_ui_sel]: synth follows algo, drums follow pad)   saveparam <id> <v> (from [route param]: id 1280 = synth algo)
//   Outlet 7 = slot umenu commands (clear/append/set); status text = outlet 3 `saveinfo set <text>`; outlet 1 gets `savepreset <engine> <n> <name with _ for spaces>`.
//   Slots offered: drums 0..31, synth 5..31 (0..4 are factory and never listed). Inbound `presetsaved <engine> <n> ok|err <code>`; after ok pmor.proto re-sends the preset list.
// This device -> [send pmor_tx] (outlet 1): deletepool <id...>, bin, binrestore <id>, binempty. proto replies on outlet 2 (inbound here): binbegin / bin <id> <len> <name> <age> <boots_left> /
//   binend <n>, deleted <id> ok|err <code>, binrestored <id> ok|err <code>, binemptied ok <n>|err <code>, nobin.
inlets = 1; outlets = 8;
var pre = [], vis = [], eng = 0, pad = 12, psel_i = 0, open_ = 1, lastinfo = "", havePre = false;   // pad 12 = first KICK pad
var follow = "";
for (var ai = 1; ai < jsarguments.length; ai++) { var av = String(jsarguments[ai]); if (av === "gated") open_ = 0; else if (av === "drums" || av === "synth") follow = av; else if (/^\d+$/.test(av)) eng = av | 0; }
var pl = [], psel = 0, slot = 0, storetxt = "store -", patchslot = "-";

function join(a, from) { var s = []; for (var i = from; i < a.length; i++) s.push(String(a[i])); return s.join(" "); }
function say(tag, text) { if (tag === "poolinfo") lastinfo = text; if (open_) outlet(3, tag, "set", text); }
function editor(v) { open_ = v | 0; if (open_) { if (eng < 4) pad = groupOfPad(pad) === eng ? pad : firstPadOf(eng); outlet(4, "set", eng); outlet(5, "set", pad); refill(); drawpool(); drawbin(); showstore(); drawslots(); if (lastinfo) outlet(3, "poolinfo", "set", lastinfo); } }

// ---- presets
// engine 0..3 = drum group KICK SNARE HAT PERC, 4..11 = synth algo 0..7. The pad menu (0..15) and the drum-group engine are tied by the FIRMWARE's pad<->group
// mapping (pmor_core/drums.cpp drumPadGroup: TL quadrant PERC, TR HAT, BL KICK, BR SNARE), NOT pad/4: picking a pad selects its group, picking a group moves
// the pad to the first pad of that group. Unnamed entries (name `-`) take the schema's names; unnamed user entries read `user <n>`.
//@NAMES-BEGIN (generated by make_devices.py from schema.json)
var DRUM_SOUNDS = [["KSUB", "KCLUB", "808", "909", "KCK1", "KCK2", "KCK3", "K808", "K909", "KHARD", "PHONE", "SUB", "BOOM", "DIST", "RUMBL", "GLICH", "DEEP", "PUNCH", "TRAP", "TAPE", "CLICK", "THUMP", "FOLD", "DUST"], ["SNP1", "808", "GATE", "CLAP9", "SNP2", "SNRF", "CHOP", "CLAP", "ESNR", "RIM", "909", "CLAP8", "TIGHT", "MODAL", "FM", "ZAPN", "SNAP", "ROOM", "RIMS", "FLAM", "CRSH", "TRAPS", "BRUSH", "GLASS"], ["HCL", "CH808", "CRASH", "HOP", "HMD", "WASH", "EHAT1", "EHAT2", "EHAT3", "ECYM", "OH808", "CH909", "OH909", "RIDE", "NOH", "FMCYM", "TICK", "SIZZL", "TRSH", "PEDAL", "SHAKE", "OPEN2", "BELLH", "RING2"], ["TOML", "CHIRP", "COWB", "CLAVE", "TOMM", "TOMH", "BELL", "BLIP", "LZTOM", "FMBEL", "MTL", "COSMO", "TOM9", "CONGA", "LASER", "BONGO", "GEN1", "GEN2", "GEN3", "TOMLO", "TOMHI", "FUNK", "WOOD", "CONG2"]], SYN_PRESETS = ["INIT", "PLUCK", "PAD", "LEAD", "BASS"];
//@NAMES-END
var GRP = ["KICK", "SNARE", "HAT", "PERC"];
function groupOfPad(p) { p = p | 0; var r = p >> 2, c = p & 3; if (r < 2) return c < 2 ? 3 : 2; return c < 2 ? 0 : 1; }
function firstPadOf(g) { for (var p = 0; p < 16; p++) if (groupOfPad(p) === g) return p; return 0; }
function entryName(e, n, k, name) {
    if (name && name !== "-") return name;
    if (k === "u") return "user " + n;
    var t = e < 4 ? (DRUM_SOUNDS[e] || [])[n] : SYN_PRESETS[n];
    return t || ("preset " + n);
}
function presetbegin() { pre = []; havePre = false; }
var dbgN = 0;
// debug probes: OFF unless `set on 1` was stored in [js]-global "pmor_dbg" (Max: send `pmor_dbg 1` via pmor_session.js `debug 1`)
function dbgOn() { try { return new Global("pmor_dbg").on == 1; } catch (e) { return false; } }
function dbg(t) { if (!dbgOn()) return; try { var f = new File("/tmp/pmor_m4l_debug.log", "write"); f.position = f.eof; f.writeline(new Date().toISOString() + " browser " + t); f.close(); } catch (e) {} }
function preset() { var a = arguments; if (a.length < 3) return;   // Max also calls a no-arg `preset` (its own preset protocol, measured in Live): ignore it
    if (dbgN++ < 3) { var x = []; for (var i = 0; i < a.length; i++) x.push(typeof a[i] + ":" + a[i]); dbg("preset n=" + a.length + " " + x.join(" ")); } pre.push({ e: a[0] | 0, n: a[1] | 0, k: String(a[2]), name: entryName(a[0] | 0, a[1] | 0, String(a[2]), join(a, 3)) }); }
function presetend() { havePre = true; refill(); }
function engine(e) {
    eng = e | 0;
    if (eng < 4 && groupOfPad(pad) !== eng) { pad = firstPadOf(eng); if (open_) outlet(5, "set", pad); }
    refill();
}
function padsel(p) {
    pad = p | 0;
    if (eng < 4 && groupOfPad(pad) !== eng) { eng = groupOfPad(pad); if (open_) outlet(4, "set", eng); refill(); }
}
function presetpick(i) { psel_i = i | 0; }
function refill() {
    vis = [];
    for (var i = 0; i < pre.length; i++) if (pre[i].e === eng) vis.push(pre[i]);
    psel_i = 0;
    drawslots();
    if (!open_) return;
    outlet(0, "clear");
    for (var j = 0; j < vis.length; j++) outlet(0, "append", vis[j].k + " " + vis[j].name);
    if (!vis.length) outlet(0, "append", havePre ? "(none for this engine)" : "(not loaded: connect)");
    outlet(0, "set", 0);
}
function dopreset() {
    if (psel_i < 0 || psel_i >= vis.length) return;
    if (eng < 4) outlet(1, "preset", eng, vis[psel_i].n, pad); else outlet(1, "preset", eng, vis[psel_i].n);
}

// ---- save as user preset
var saveName = "", saveN = -1, saveItems = [], saveArm = null, SAVE_ARM_MAX = 10000, saving = null;
function cleanName(t) { var o = String(t).toUpperCase().replace(/[^A-Z0-9 \-]/g, "").replace(/^ +/, "").substr(0, 9); return o.replace(/ +$/, ""); }
function slotLo() { return eng < 4 ? 0 : 5; }      // synth 0..4 = factory: never offered
function userAt(n) { for (var i = 0; i < pre.length; i++) if (pre[i].e === eng && pre[i].k === "u" && pre[i].n === n) return pre[i]; return null; }
function slotLabel(n) { var u = userAt(n); return n + "  " + (u ? u.name : "(empty)"); }
function drawslots() {
    saveItems = [];
    for (var n = slotLo(); n <= 31; n++) saveItems.push(n);
    var keep = saveItems.indexOf(saveN) >= 0 ? saveN : -1;
    if (keep < 0) { for (var i = 0; i < saveItems.length; i++) if (!userAt(saveItems[i])) { keep = saveItems[i]; break; } if (keep < 0) keep = saveItems[0]; }
    saveN = keep; saveArm = null;
    if (!open_) return;
    outlet(7, "clear");
    for (var j = 0; j < saveItems.length; j++) outlet(7, "append", slotLabel(saveItems[j]));
    outlet(7, "set", saveItems.indexOf(saveN));
}
function savename() { var a = []; for (var i = 0; i < arguments.length; i++) a.push(String(arguments[i])); saveName = cleanName(a.join(" ")); saveArm = null; say("saveinfo", saveName ? "name: " + saveName : "enter a name"); }
function saveslotpick(i) { i = i | 0; if (i >= 0 && i < saveItems.length) saveN = saveItems[i]; saveArm = null; }
function selui(kind, n) {
    kind = String(kind); n = n | 0;
    if (follow === "synth" && kind === "algo" && n >= 0 && n <= 7) engine(4 + n);
    else if (follow === "drums" && kind === "pad" && n >= 0 && n <= 15) padsel(n);
}
function saveparam(id, v) { if ((id | 0) === 1280 && follow === "synth" && v >= 0 && v <= 7 && eng !== 4 + (v | 0)) engine(4 + (v | 0)); }
function dosave() {
    if (!saveName) { say("saveinfo", "enter a name"); return; }
    if (saveN < slotLo() || saveN > 31) { say("saveinfo", "pick a user slot"); return; }
    if (!havePre) { say("saveinfo", "presets not loaded: connect first"); return; }
    var u = userAt(saveN), key = eng + "/" + saveN + "/" + saveName, now = clock();
    if (u && !(saveArm && saveArm.key === key && now - saveArm.t <= SAVE_ARM_MAX)) { saveArm = { key: key, t: now }; say("saveinfo", "overwrite " + u.name + "? click save again"); return; }
    saveArm = null; saving = { e: eng, n: saveN, name: saveName };
    outlet(1, "savepreset", eng, saveN, saveName.replace(/ /g, "_"));
    say("saveinfo", "saving " + saveName + " to slot " + saveN + (eng < 4 ? " (current pad's sound)" : "") + "...");
}
function presetsaved(e, n, r, code) {
    var nm = saving && saving.e === (e | 0) && saving.n === (n | 0) ? saving.name : "preset";
    saving = null; saveArm = null;
    if (r === "ok") { say("saveinfo", "saved " + nm + " to slot " + (n | 0)); return; }
    code = String(code);
    say("saveinfo", code === "unsupported" ? "save not supported by this PMOR firmware" : code === "nosave" ? "PMOR has saving disabled (NOSAVE)" : code === "factory" ? "factory slots cannot be overwritten" : "save failed: " + code);
}

// ---- pool (array is `pl`; `rows` = what the umenu shows: single entries, duplicate-group rows, and the child rows of expanded groups)
// pool <id> <len_samples> <name> <cached 0|1|2> <slots> <patch> <rate> [<crc x<hex>|-> <known 0|1>]   (first four = old layout; the rest optional)
var rows = [], expanded = {}, selGroups = {};
function pad2(n) { return (n < 10 ? "0" : "") + n; }
function plabel(id) { return id >= 1024 ? "S" + id : "P" + pad2(id >> 4) + "S" + pad2(id & 15); }
function poolbegin() { pl = []; }
function pool(id, len, name, cached, slots, patchn, rate, crc, known) {
    var sl = (slots === undefined || String(slots) === "-") ? "" : String(slots);
    var c = crc === undefined ? "" : String(crc).toLowerCase(); c = (c === "-" || /^x?0*$/.test(c)) ? "" : c;
    pl.push({ id: id | 0, len: len | 0, n: name === undefined ? "" : String(name), c: (cached | 0) === 1 ? 1 : 0, bad: (cached | 0) === 2 ? 1 : 0,
              slots: sl, pat: patchn === undefined || String(patchn) === "-" ? -1 : (patchn | 0), rate: (rate | 0) || 48000,
              crc: c, known: known === undefined ? (sl ? 1 : 0) : (known | 0) });
}
function buildRows() {
    var gm = {}, k, i, p;
    for (i = 0; i < pl.length; i++) { p = pl[i]; if (!p.crc || p.bad) continue; k = p.crc + ":" + p.len; (gm[k] = gm[k] || []).push(p); }
    rows = []; var seen = {};
    for (i = 0; i < pl.length; i++) {
        p = pl[i]; k = p.crc && !p.bad ? p.crc + ":" + p.len : "";
        if (!k || gm[k].length < 2) { rows.push({ kind: "s", p: p }); continue; }
        if (seen[k]) continue; seen[k] = 1;
        var m = gm[k], rep = m[0];
        for (var j = 0; j < m.length; j++) { if (m[j].slots && !rep.slots) rep = m[j]; else if (!!m[j].slots === !!rep.slots && m[j].id < rep.id) rep = m[j]; }
        var g = { kind: "g", key: k, m: m, p: rep };
        rows.push(g);
        if (expanded[k]) { var cs = m.slice().sort(function (x, y) { return (y.slots ? 1 : 0) - (x.slots ? 1 : 0) || x.id - y.id; }); for (j = 0; j < cs.length; j++) rows.push({ kind: "c", p: cs[j], g: g }); }
    }
    var live = {}; for (i = 0; i < rows.length; i++) if (rows[i].kind === "g") live[rows[i].key] = 1;
    for (k in expanded) if (!live[k]) delete expanded[k];
    for (k in selGroups) if (!live[k]) delete selGroups[k];
}
function poolend(count) {
    var keep = {}; for (var i = 0; i < pl.length; i++) if (selIds[pl[i].id]) keep[pl[i].id] = 1; selIds = keep;      // selection survives a reload, minus ids that no longer exist
    buildRows(); drawpool(); say("poolinfo", pl.length + " pool entries" + (rows.length < pl.length ? " (" + rows.length + " rows, duplicates grouped)" : "") + (selCount() ? ", " + selCount() + " selected" : ""));
}
function usage(p) { return p.slots ? "slot " + p.slots : "?"; }
function mark(p) { return p.bad ? "! " : p.c ? "* " : "- "; }
function rowText(p) {
    var lab = plabel(p.id), sec = (p.len / p.rate).toFixed(1);
    return (selIds[p.id] ? "x " : "  ") + mark(p) + lab + (p.n && p.n !== lab ? " " + p.n : "") + "  " + sec + "s " + p.len
        + (p.slots ? "  -> slot " + p.slots : "") + (p.id < 1024 ? "  patch " + (p.pat >= 0 ? p.pat : (p.id >> 4)) : "");
}
function groupText(r) {
    var p = r.p, lab = plabel(p.id), anyC = 0, anyB = 0, u = [], i;
    for (i = 0; i < r.m.length; i++) { if (r.m[i].c) anyC = 1; if (r.m[i].bad) anyB = 1; }
    var ord = r.m.slice().sort(function (a, b) { return (b.slots ? 1 : 0) - (a.slots ? 1 : 0) || a.id - b.id; });
    for (i = 0; i < ord.length; i++) u.push(plabel(ord[i].id) + " " + usage(ord[i]));
    return (selGroups[r.key] ? "x " : "  ") + (anyB ? "! " : anyC ? "* " : "- ") + lab + (p.n && p.n !== lab ? " " + p.n : "") + " x" + r.m.length + "  " + (p.len / p.rate).toFixed(1) + "s " + p.len + "  [" + u.join("; ") + "]";
}
function childText(p) { return (selIds[p.id] ? "x " : "  ") + "   " + mark(p) + plabel(p.id) + "  " + usage(p); }
function textOf(r) { return r.kind === "g" ? groupText(r) : r.kind === "c" ? childText(r.p) : rowText(r.p); }
function drawpool() {
    if (!open_) { if (psel >= rows.length) psel = 0; return; }
    outlet(2, "clear");
    for (var i = 0; i < rows.length; i++) outlet(2, "append", textOf(rows[i]));
    if (!rows.length) outlet(2, "append", "(empty pool)");
    if (psel >= rows.length) psel = 0;
    outlet(2, "set", psel);
}
function sample(name, state) {
    for (var i = 0; i < pl.length; i++) if (plabel(pl[i].id) === String(name) || pl[i].n === String(name)) {
        if (state === "bad") { pl[i].bad = 1; pl[i].c = 0; } else { pl[i].c = 1; pl[i].bad = 0; }
    }
    buildRows(); drawpool();
}
function progress(name, done, total, left) { say("poolinfo", "pull " + name + " " + done + "/" + total + " left " + left); }
function poolpick(i) { psel = i | 0; }
function slotpick(i) { slot = i | 0; }
function pull() { if (rows[psel]) outlet(1, "pullid", rows[psel].p.id); }        // a group row acts on its representative copy
function assign() { if (rows[psel]) outlet(1, "assign", slot, rows[psel].p.id); }
function assigned(s, r, code) { say("poolinfo", "slot " + s + " " + r + (code !== undefined ? " " + code : "")); }
function expand(i) { var r = rows[i | 0]; if (!r || r.kind === "s") return; var k = r.kind === "g" ? r.key : r.g.key; if (expanded[k]) delete expanded[k]; else expanded[k] = 1; buildRows(); drawpool(); say("poolinfo", expanded[k] ? "copies shown" : "copies hidden"); }
function expandcur() { expand(psel); }


// ---- delete (multi-select, hold-to-confirm) + recycle bin. The CONFIRM LOGIC lives here; the UI only sends messages.
// Delete never reaches PMOR without: a selection, `delarm`, then `delgo` between 2 s and 10 s later (clock(): Date.now(), injectable for tests via _setclock).
// If a selected id is used by a slot of the CURRENT patch the first valid `delgo` only WARNS (names the slots) and clears the arm: a SECOND `delarm` + 2 s + `delgo` executes.
// Changing the selection resets the arm and the warning. PMOR moves deleted samples to its recycle bin (firmware pending: `nobin` = unsupported, nothing changed).
var selIds = {}, bl = [], bsel = 0, delArmAt = 0, delWarnKey = "", binArmAt = 0, ARM_MIN = 2000, ARM_MAX = 10000;
var clock = function () { return Date.now(); };
function _setclock(f) { clock = f; }
function selectedIds() { var v = []; for (var i = 0; i < pl.length; i++) if (selIds[pl[i].id]) v.push(pl[i].id); return v; }
function selectedGroups() { var v = []; for (var i = 0; i < rows.length; i++) if (rows[i].kind === "g" && selGroups[rows[i].key]) v.push(rows[i]); return v; }
function selCount() { return selectedIds().length + selectedGroups().length; }
function selChanged() { delArmAt = 0; delWarnKey = ""; drawpool(); say("poolinfo", selCount() + " selected"); }
function rowSel(r) { return r.kind === "g" ? !!selGroups[r.key] : !!selIds[r.p.id]; }
function sel(i, v) { i = i | 0; var r = rows[i]; if (!r) return; if (r.kind === "g") { if (v | 0) selGroups[r.key] = 1; else delete selGroups[r.key]; } else if (v | 0) selIds[r.p.id] = 1; else delete selIds[r.p.id]; selChanged(); }
function selcur() { if (rows[psel]) sel(psel, rowSel(rows[psel]) ? 0 : 1); }
function selclear() { selIds = {}; selGroups = {}; selChanged(); }
// what a marked GROUP row deletes: copies not used by the current patch whose usage is known (known=1); every other copy stays; if that would leave none, the representative stays.
function groupPlan(g) {
    var cand = [], keep = [], i, p;
    for (i = 0; i < g.m.length; i++) { p = g.m[i]; if (!p.slots && p.known) cand.push(p); else keep.push(p); }
    if (!keep.length && cand.length) { var lo = 0; for (i = 1; i < cand.length; i++) if (cand[i].id < cand[lo].id) lo = i; keep.push(cand.splice(lo, 1)[0]); }
    return { del: cand, keep: keep };
}
function planText(g, pn) {
    var d = [], k = [], i; for (i = 0; i < pn.del.length; i++) d.push(plabel(pn.del[i].id));
    for (i = 0; i < pn.keep.length; i++) k.push(plabel(pn.keep[i].id) + (pn.keep[i].slots ? " (slot " + pn.keep[i].slots + ")" : ""));
    return "GROUP " + plabel(g.p.id) + " x" + g.m.length + ": " + (d.length ? "delete " + d.join(" ") : "delete none") + ", keep " + k.join(" ");
}
function delarm() { delArmAt = clock(); say("poolinfo", selCount() ? "delete armed: hold 2 s, then delgo" : "nothing selected"); }
function usedWarning(ids) {
    var parts = [];
    for (var i = 0; i < pl.length; i++) if (selIds[pl[i].id] && pl[i].slots) parts.push(plabel(pl[i].id) + " in slot " + pl[i].slots);
    return parts.length ? parts.join(", ") : "";
}
function holdOk(armAt) { if (!armAt) return false; var h = clock() - armAt; return h >= ARM_MIN && h <= ARM_MAX; }
function delgo() {
    var ids = selectedIds(), gs = selectedGroups();
    if (!ids.length && !gs.length) { delArmAt = 0; say("poolinfo", "nothing selected"); return; }
    if (!holdOk(delArmAt)) { delArmAt = 0; say("poolinfo", "hold delete for 2 s"); return; }
    delArmAt = 0;
    var set = {}, i, notes = [], plans = [];
    for (i = 0; i < ids.length; i++) set[ids[i]] = 1;
    for (i = 0; i < gs.length; i++) { var pn = groupPlan(gs[i]); plans.push(pn); notes.push(planText(gs[i], pn)); for (var j = 0; j < pn.del.length; j++) set[pn.del[j].id] = 1; }
    var fin = []; for (i = 0; i < pl.length; i++) if (set[pl[i].id]) fin.push(pl[i].id);
    if (!fin.length) { delWarnKey = ""; say("poolinfo", notes.join(" | ") + ". Nothing to delete (every copy is used or unknown)"); return; }
    var key = fin.join(",") + "/" + gs.length, used = usedWarning(ids);
    if ((used || gs.length) && delWarnKey !== key) {
        delWarnKey = key;
        say("poolinfo", (used ? "USED BY CURRENT PATCH: " + used + ". " : "") + (notes.length ? notes.join(" | ") + ". Other patches not checked. " : "") + "Hold delete 2 s again to confirm");
        return;
    }
    outlet.apply(null, [1, "deletepool"].concat(fin));
    selIds = {}; selGroups = {}; delWarnKey = ""; drawpool(); say("poolinfo", "deleting " + fin.length + " (-> recycle bin)" + (notes.length ? "; " + notes.join(" | ") : ""));
}
function deleted(id, r, code) {
    say("poolinfo", r === "ok" ? "deleted " + plabel(id | 0) + " (in the recycle bin)" : "delete " + plabel(id | 0) + " failed: " + code);
    outlet(1, "bin");
}
function nobin() { bl = []; say("poolinfo", "firmware has no recycle bin yet - nothing was changed"); drawbin(); }
// bin: no arguments = the UI refresh button (asks proto); with arguments = one entry of the stream binbegin / bin <id> <len> <name> <age> <boots_left> / binend <n>
function bin(id, len, name, age, boots) {
    if (arguments.length === 0) { outlet(1, "bin"); return; }
    bl.push({ id: id | 0, len: len | 0, n: String(name), age: String(age), boots: boots | 0 });
}
function binbegin() { bl = []; }
function binend(n) { if (bsel >= bl.length) bsel = 0; drawbin(); say("poolinfo", "bin: " + bl.length + (bl.length === 1 ? " sample" : " samples")); }
function fmtAge(a) {
    var n = Number(a); if (isNaN(n)) return String(a);
    if (n >= 1e9) n = clock() / 1000 - n; if (n < 0) n = 0;
    return n < 90 ? Math.round(n) + "s" : n < 5400 ? Math.round(n / 60) + "m" : n < 129600 ? Math.round(n / 3600) + "h" : Math.round(n / 86400) + "d";
}
function binText(b) { return plabel(b.id) + (b.n && b.n !== plabel(b.id) ? " " + b.n : "") + "  " + (b.len / 48000).toFixed(1) + "s  deleted " + fmtAge(b.age) + " ago, " + b.boots + " boots left"; }
function drawbin() {
    if (!open_) return;
    outlet(6, "clear");
    for (var i = 0; i < bl.length; i++) outlet(6, "append", binText(bl[i]));
    if (!bl.length) outlet(6, "append", "(recycle bin empty)");
    outlet(6, "set", bsel < bl.length ? bsel : 0);
}
function binsel(i) { bsel = i | 0; }
function binrestore() { if (!bl[bsel]) { say("poolinfo", "nothing selected in the bin"); return; } outlet(1, "binrestore", bl[bsel].id); say("poolinfo", "restoring " + plabel(bl[bsel].id)); }
function binrestored(id, r, code) { say("poolinfo", r === "ok" ? "restored " + plabel(id | 0) : "restore " + plabel(id | 0) + " failed: " + code); outlet(1, "bin"); }
function binemptyarm() { binArmAt = clock(); say("poolinfo", "empty bin armed: hold 2 s, then binemptygo (permanent)"); }
function binemptygo() {
    if (!bl.length) { binArmAt = 0; say("poolinfo", "bin is empty"); return; }
    if (!holdOk(binArmAt)) { binArmAt = 0; say("poolinfo", "hold empty bin for 2 s"); return; }
    binArmAt = 0; outlet(1, "binempty"); say("poolinfo", "emptying the bin (" + bl.length + ")");
}
function binemptied(r, n) { say("poolinfo", r === "ok" ? "bin emptied (" + n + ")" : "empty bin failed: " + n); outlet(1, "bin"); }

// ---- status
function store(s, pend, err, n) { var nm = ["idle", "migrating", "done", "errors (press store retry)"][s] || s; storetxt = "store " + nm + " pend " + pend + " err " + err + " pool " + n; showstore(); }
function patch(s) { patchslot = String(s); showstore(); }
function showstore() { say("store", "patch " + patchslot + "  " + storetxt); }
// status <mode> <policy> <running> <bpm> <cpu> <overruns> <drops> <bus 0|1> <role master|follow>: role is PMOR's REAL clock role (firmware role= when
// present, else inferred from bus-clock liveness). Shown as `follow` / `master`; the policy only when it is not AUTO: `follow (FOLLOW)`.
function transText(pol, running, bpm, cpu, over, drops, bus, role) {
    var r = (role === "follow" || role === "master") ? role : ((bus | 0) ? "follow" : "master");
    var clk = (pol && String(pol) !== "AUTO") ? r + " (" + pol + ")" : r;
    return ((running | 0) ? "run " : "stop ") + Number(bpm).toFixed(1) + " | " + clk + " | cpu " + cpu + " ovr " + over + " drop " + drops;
}
var linkUp = false;
function state(s) {
    s = String(s);
    if (s === "ready" && open_ && !havePre) outlet(1, "presets");   // editor already open when the link comes up
    var up = s !== "disconnected";
    if (up !== linkUp || !up) { linkUp = up; if (!up) say("trans", "standalone"); }
}
function status(mode, pol, running, bpm, cpu, over, drops, bus, role) {
    linkUp = true;
    say("trans", transText(pol, running, bpm, cpu, over, drops, bus, role));
}

// the status stream also carries messages this helper does not use (state, info, synced, sample, ...): ignore them quietly
function anything() { }

if (typeof module !== "undefined") module.exports = { cleanName: cleanName, transText: transText, groupOfPad: groupOfPad, entryName: entryName, fmtAge: fmtAge };   // node test
