// pmor_session.js -- the PMOR core's connection logic. All timing lives here (js Task), never in saved patcher objects.
// Inlet messages: connect 0|1      the connect toggle (the ONLY thing that opens/closes the port)
//                 port <letter>    selected serial port letter
//                 pmode <name>     mode picked in the perform view (STUDIO | EDIT)
//                 editor 0|1       editor window opened/closed (pmor_edit.js): open => session EDIT, closed => back to the perform mode
//                 state <name>     proto state: disconnected | hello | syncing | ready
//                 pmorport <letter|none> [path]   reply of pmor.proto `findport` (PMOR located by USB identity)
// Outlets: 0 -> pmor_port.js (openport <letter> / closeport)   1 -> pmor.proto (hello <mode> / mode <m> / bye / closed / findport)
//          2 -> status comment (`set <text>`)                   3 -> bang: pmor_where.js re-decides the sample cache dir
//          4 -> mode umenu (`set <index>`, silent)
// Session mode = EDIT while the editor is open, else the perform mode (STUDIO). Connect: where -> openport -> hello <mode>.
// Port choice: "auto" = PMOR's USB identity. [pmor.proto] `findport` (IOKit registry, VID cafe PID 4f70, never opens a port) answers `pmorport <letter> <path>` or
// `pmorport none` on its status outlet; the letter is Max's [serial] letter (index in the sorted /dev/cu.*). No blind letter scan, a Bela usbmodem4488.. is never
// opened. A manual letter (a..f, udp<port>) overrides and is simply (re)opened.
// Link life cycle (the port is opened ONLY while PMOR is enumerated):
//   connect -> findport -> found: openport <letter> (DTR+RTS up) -> 500 ms later `hello <mode>` (re-sent every 1 s until syncing) -> proto does the full resync FROM PMOR.
//   every 1 s: findport again. `none` while the port is open (cable pulled / port vanished) -> `closeport` + `closed` at once (no stale [serial]) -> status
//   STANDALONE (unplugged). Found again (replug) -> reopen -> hello -> resync. proto `state disconnected` after a good link (DTR loss / 2 s silence / lost request) ->
//   same clean close, then reopen on the next poll (<= 1 s). Never connected within 6 s -> close + reopen once more (re-raises DTR).
// Timing: js Task never fires inside Live's Max 9.1.5 (measured 2026-11-10), so a [metro 250] is created on connect, wired to this js, and removed on disconnect.
// Nothing runs while disconnected.
inlets = 1; outlets = 5;
var connected = 0, portLetter = "auto", perform = "STUDIO", editorOpen = 0, pstate = "disconnected", everUp = 0;
var ticker = null, ticks = 0, closing = 0;
var portOpen = 0, openLetter = "", helloAt = -1, noLink = 0, findAt = 0;
var POLL = 4, HELLO_DELAY = 2, HELLO_EVERY = 4, STUCK = 24;       // ticks of 250 ms: poll 1 s, first hello 500 ms after open, re-hello 1 s, reopen after 6 s without a link

function mode() { return editorOpen ? "EDIT" : perform; }
function say(t) { outlet(2, "set", t); }
function cur() { return portOpen ? openLetter : (portLetter === "auto" ? "usb" : portLetter); }
function refresh() {
    if (!connected) { say("disconnected"); return; }
    if (!portOpen) { say(everUp ? "STANDALONE (unplugged)" : (portLetter === "auto" ? "waiting for PMOR (usb)..." : "connecting... port " + portLetter)); return; }
    if (pstate === "disconnected" || (pstate === "hello" && !everUp)) { say(everUp ? "STANDALONE (unplugged)" : "connecting... port " + cur()); return; }
    say(pstate + " " + mode() + " | port " + cur());
}
function startTicker() {
    if (ticker) return;
    try {
        ticker = this.patcher.newdefault(600, 20, "metro", 250); dbg("ticker " + (ticker ? ticker.maxclass : "null") + " box " + (this.box ? this.box.maxclass : "null"));
        this.patcher.connect(ticker, 0, this.box, 0); dbg("ticker wired");
        ticker.message("int", 1); dbg("ticker on"); ticks = 0;
    } catch (e) { dbg("ERR ticker " + e); }
}
function stopTicker() { if (!ticker) return; try { ticker.message("int", 0); this.patcher.remove(ticker); } catch (e) { } ticker = null; }

function openPort(letter) {
    dbg("open " + letter);
    openLetter = String(letter); portOpen = 1; noLink = 0; pstate = "disconnected";
    helloAt = ticks + HELLO_DELAY;                       // PMOR drops bytes until it has seen DTR (~400 ms): the first hello waits
    outlet(0, "openport", openLetter);
}
// close cleanly: port object removed first, then proto told (it leaves READY -> DISCONNECTED and stops its own reconnect: we hello again after the reopen)
function closePort(why) {
    dbg("close (" + why + ")");
    var was = portOpen;
    portOpen = 0; openLetter = ""; helloAt = -1; pstate = "disconnected"; noLink = 0;   // before the outlets: proto echoes `state disconnected` synchronously and must find us closed
    if (was) { outlet(0, "closeport"); outlet(1, "closed"); }
}
function find() { findAt = ticks; outlet(1, "findport"); }
// reply of pmor.proto `findport`
function pmorport(letter, path) {
    if (!connected || closing || portLetter !== "auto") return;
    var L = String(letter);
    if (L === "none") { if (portOpen) { closePort("vanished"); refresh(); } return; }
    if (portOpen && L === openLetter) return;
    if (portOpen) closePort("letter changed");
    openPort(L); refresh();
}
function connect(v) {
    v = v | 0;
    dbg("connect " + v + " port " + portLetter);
    if (v === connected) return;
    connected = v;
    if (connected) {
        closing = 0; everUp = 0; pstate = "disconnected"; portOpen = 0; openLetter = ""; helloAt = -1; noLink = 0;
        outlet(3, "bang");                       // sessiondir first
        startTicker.call(this);
        if (portLetter === "auto") find(); else openPort(portLetter);
        refresh();
    } else {
        outlet(1, "bye");
        closing = 1; ticks = 0; startTicker.call(this);   // close the port 250 ms later, after the bye is out
        refresh();
    }
}
function bang() {                                // [metro 250], exists only while connected or closing
    ticks++; if (ticks < 4) dbg("tick " + ticks);
    if (closing) { closing = 0; stopTicker.call(this); portOpen = 0; openLetter = ""; pstate = "disconnected"; outlet(0, "closeport"); outlet(1, "closed"); refresh(); return; }
    if (!connected) { stopTicker.call(this); return; }
    if (portLetter === "auto") { if (ticks - findAt >= POLL) find(); }
    else if (!portOpen && ticks - findAt >= POLL) { findAt = ticks; openPort(portLetter); refresh(); }      // manual letter / udp: reopen once a second
    if (!portOpen) return;
    if (pstate === "disconnected" || pstate === "hello") {
        noLink++;
        if (helloAt >= 0 && ticks >= helloAt) { helloAt = ticks + HELLO_EVERY; dbg("hello " + mode()); outlet(1, "hello", mode()); }
        if (noLink >= STUCK) { closePort("no link in 6 s"); refresh(); if (portLetter === "auto") find(); else openPort(portLetter); }
    } else noLink = 0;
}
function state(s) {
    var was = pstate;
    pstate = String(s); dbg("state " + pstate);
    if (pstate === "syncing" || pstate === "ready") { everUp = 1; noLink = 0; }   // "hello" is only our own request, not a link
    if (pstate === "disconnected" && connected && !closing && portOpen && everUp && was !== "disconnected") {
        closePort("link lost");                                                   // DTR loss / 2 s silence / dead request: drop the port, reopen on the next poll
        findAt = ticks - POLL;                                                    // poll again at the very next tick
    }
    refresh();
}
function port(l) {
    var nl = String(l);
    if (nl === portLetter) return;
    portLetter = nl;
    if (connected && !closing) { if (portOpen) closePort("port changed"); findAt = ticks - POLL; everUp = 0; refresh(); }
}
function pmode(m) {
    perform = String(m) === "EDIT" ? "EDIT" : "STUDIO";
    if (connected && !editorOpen) outlet(1, "mode", perform);
    refresh();
}
var editorClosedAt = 0;
function editor(v) {
    editorOpen = v | 0; dbg("editor " + editorOpen);
    if (!editorOpen) editorClosedAt = Date.now();
    outlet(4, "set", mode() === "EDIT" ? 1 : 0);
    if (connected && pstate !== "disconnected") outlet(1, "mode", mode());
    refresh();
}

// live probe (macbert 2026-11-09): proves in Live that this script loaded and gets messages
function loadbang() { dbg("loaded"); say("disconnected (js ok)"); }
var stN = 0;
function anything() { if (messagename === "status") { if (stN++ % 3 === 0) dbg("status " + arrayfromargs(arguments).join(" ")); return; } if (messagename === "store" || messagename === "pool" || messagename === "progress" || messagename === "sample") return; dbg("got " + messagename); }
function debug(v) { new Global("pmor_dbg").on = v | 0; }   // `debug 1` / `debug 0`: the one switch for every dbg() file probe (default OFF)
// file probe (macbert 2026-11-09): Live's Max console isn't visible from outside, so mirror post() to a file
// debug probes: OFF unless `set on 1` was stored in [js]-global "pmor_dbg" (Max: send `pmor_dbg 1` via pmor_session.js `debug 1`)
function dbgOn() { try { return new Global("pmor_dbg").on == 1; } catch (e) { return false; } }
function dbg(t) { if (!dbgOn()) return; try { var f = new File("/tmp/pmor_m4l_debug.log", "write"); f.position = f.eof; f.writeline(new Date().toISOString() + " session " + t); f.close(); } catch (e) {} }
