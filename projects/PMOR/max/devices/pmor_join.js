// pmor_join.js -- JOIN: when Live is already playing as the PMOR device becomes ready, tell the firmware where the song is so it can lock on
// (proto: `join <spp16>` -> status `joined <spp16> <tick_at>` | `joinerr <spp16> <code>`; spp16 = song position in 16ths).
//   inlet 0:  int 0/1            the core's connect toggle (nothing is created or observed while 0)
//             state <name>       proto state stream (outlet 2 of pmor.proto goes straight in); `ready` arms the join check
//             joined .. / joinerr ..   logged to the Max console
//   outlet 0: joinprep / join <n>  -> pmor.proto        outlet 1: `set <text>` for the join line comment (joined latency)
// Contract (docs/PROTOCOL.md section 15): JOIN is stamped at the USB IRQ, so it must leave exactly on a Live 16th boundary; it answers ERR busy
// while a bulk reply runs, so `joinprep` (hold bulk chunk requests) goes out exactly ONE 16th BEFORE the join.  Reply `joined <spp16> <late_us>`.
// Armed join: a [metro 16n @quantize 16n] is created at that moment (never saved in the patcher); tick 1 (on a 16th boundary) -> joinprep,
// tick 2 (next boundary) -> join <joinIndex(current_song_time, true)>, then the metro is removed again.
// While connected+ready+playing, current_song_time is observed (callback, no polling): a jump of more than one beat is a relocate and
// re-arms the join after a short settle (the firmware drops a join it does not need; there is no SPP feedback in the host to wait on).
// Without the Live API (plain Max, tests) every Live call is skipped.
inlets = 1; outlets = 2;
var phase = 0, connected = 0, ready = 0, api = null, obsPlay = null, obsTime = null, metro = null, lastT = null, settle = null, playing = 0;

// pure: song time in beats (quarter notes) -> join index in 16ths.
//   now = true : we are standing on a 16th boundary (the quantized tick): nearest index.
//   now = false: mid-16th: the NEXT 16th boundary (an exact boundary counts as "already there", next is the one after).
function joinIndex(beats, now) {
    var q = Number(beats) * 4;
    if (!isFinite(q) || q < 0) q = 0;
    return now ? Math.round(q) : Math.floor(q + 1e-9) + 1;
}
// pure: is a time step a relocate (jump of more than one beat)?
// pure: what the n-th armed tick sends. phase 0 -> joinprep, phase 1 -> join <index of this boundary>, then done.
function joinStep(phase, beats) {
    if (phase === 0) return { msg: "joinprep", next: 1 };
    if (phase === 1) return { msg: "join", n: joinIndex(beats, true), next: 2 };
    return { msg: null, next: 2 };
}
function isJump(prev, cur) { return prev !== null && Math.abs(cur - prev) > 1.0; }

function live() {
    try {
        if (typeof LiveAPI === "undefined") return null;
        var a = new LiveAPI(null, "live_set");
        return (a && a.id != 0) ? a : null;
    } catch (e) { return null; }
}
function songTime() { try { return Number(api.get("current_song_time")[0]); } catch (e) { return null; } }
function isPlaying() { try { return Number(api.get("is_playing")[0]) === 1; } catch (e) { return false; } }

function msg_int(v) {
    connected = v ? 1 : 0;
    if (!connected) teardown();
}
function state(s) {
    ready = (String(s) === "ready") ? 1 : 0;
    if (!connected) return;
    if (ready) startup(); else teardown(true);
}
function startup() {
    if (api) return;
    api = live();
    if (!api) { post("pmor_join: no Live API (not in Live); join disabled\n"); return; }
    playing = isPlaying() ? 1 : 0;
    post("pmor_join: ready, live is_playing=" + playing + " song_time=" + songTime() + "\n");
    observe();
    if (playing) arm();
}
function observe() {
    try {
        obsPlay = new LiveAPI(function (a) { onPlay(a); }, "live_set"); obsPlay.property = "is_playing";
        obsTime = new LiveAPI(function (a) { onTime(a); }, "live_set"); obsTime.property = "current_song_time";
    } catch (e) { post("pmor_join: observe failed " + e + "\n"); }
}
function onPlay(a) { if (!connected || !ready) return; playing = (a && Number(a[a.length - 1]) === 1) ? 1 : 0; if (!playing) disarm(); }
function onTime(a) {
    if (!connected || !ready || !playing) { lastT = null; return; }
    var t = Number(a[a.length - 1]);
    if (isJump(lastT, t)) {
        post("pmor_join: relocate " + lastT + " -> " + t + "\n");
        if (settle) settle.cancel();
        settle = new Task(function () { settle = null; if (connected && ready && playing) arm(); }, this);
        settle.schedule(300);
    }
    lastT = t;
}
function arm() {
    disarm();
    if (!api || !connected || !ready) return;
    try {
        phase = 0;
        metro = this.patcher.newdefault(10, 420, "metro", "16n", "@quantize", "16n");
        this.patcher.connect(metro, 0, this.box, 0);
        metro.message("int", 1);
        post("pmor_join: armed (metro 16n @quantize 16n)\n");
    } catch (e) { post("pmor_join: arm failed " + e + "\n"); metro = null; }
}
function disarm() {
    if (!metro) return;
    var m = metro; metro = null;
    var t = new Task(function () { try { this.patcher.remove(m); } catch (e) { } }, this); t.schedule(20);   // never remove an object from inside its own callback
    try { m.message("int", 0); } catch (e) { }
}
function bang() {                       // a quantized tick: exactly on a 16th
    if (!metro) return;
    if (!connected || !ready) { disarm(); return; }
    var t = songTime();
    if (t === null) { disarm(); return; }
    var st = joinStep(phase, t);
    phase = st.next;
    if (st.msg === "joinprep") { post("pmor_join: joinprep (song_time " + t + ")\n"); outlet(0, "joinprep"); }
    else if (st.msg === "join") { disarm(); post("pmor_join: join " + st.n + " (song_time " + t + ")\n"); outlet(0, "join", st.n); outlet(1, "set", "join " + st.n + " sent"); }
    else disarm();
}
function teardown(keepConn) {
    disarm();
    if (settle) { settle.cancel(); settle = null; }
    obsPlay = null; obsTime = null; api = null; lastT = null; playing = 0;
    if (!keepConn) ready = 0;
}
function joined(spp, late) {
    post("pmor_join: joined " + spp + " late_us " + late + "\n");
    outlet(1, "set", "joined " + spp + "  latency " + (Number(late) / 1000).toFixed(1) + " ms");
}
function joinerr() { var a = arrayfromargs(arguments).join(" "); post("pmor_join: joinerr " + a + "\n"); outlet(1, "set", "join failed: " + a); }
function anything() { }                 // the rest of the status stream
if (typeof module !== "undefined") module.exports = { joinIndex: joinIndex, isJump: isJump, joinStep: joinStep };   // node test (tests/test_join.js)
