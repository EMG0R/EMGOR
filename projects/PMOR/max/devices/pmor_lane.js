// pmor_lane.js -- MIDI lane forwarder. Reassembles raw MIDI bytes from [midiin] into 3-byte messages and sends
// "st d1 d2" for the channels this lane owns to the global [send pmor_midi] (the PMOR core device listens).
// Arguments: channel numbers this lane owns, e.g. "1" (drums) "2" (synth, includes pitch bend 0xE0) "3" (sampler) "4 5 6 meters" (control lane:
//   ch4 control CCs incl. the seed CCs 110-113, ch5 FX engage CCs, ch6 FX / perform state CCs). The word `meters` also drives the control lane's perform view:
//   sliders m1..m7 (CC1 FSR, 2 lidar, 3/4 accel X/Y, 5/6 joystick X/Y, 7 fader) and the comment `seed` (CC110..113 -> 28-bit seed).
// `remap` (single-channel lanes drums/synth/sampler): the lane track's own input filter already chose what reaches it, and a keyboard / clip on a
//   Live MIDI track plays on ITS channel (usually 1), not on the lane's PMOR channel. Every channel is accepted and re-channelled to the lane's channel
//   (first numeric argument), so typed notes, recorded takes and clips all reach the right engine. The control lane (4 5) never remaps: it carries two channels.
// Messages are forwarded unchanged (status byte, data1, data2), nothing is filtered by type: notes, CCs, pitch bend all pass.
inlets = 1; outlets = 1;
var chans = {}, meters = false, remap = false, laneCh = 0;
for (var i = 1; i < jsarguments.length; i++) {
    var a = String(jsarguments[i]);
    if (a === "meters") meters = true; else if (a === "remap") remap = true; else if (/^\d+$/.test(a)) { chans[parseInt(a, 10)] = true; if (!laneCh) laneCh = parseInt(a, 10); }
}
var buf = [], need = 0, running = 0, seedParts = [0, 0, 0, 0], last = {};
function msg() { // [midiin] outputs one byte per message
    for (var i = 0; i < arguments.length; i++) feed(arguments[i] | 0);
}
function list() { msg.apply(this, arguments); }
function msg_int(v) { feed(v | 0); }
function feed(b) {
    if (b >= 0xF8) return;                               // realtime: not ours
    if (b & 0x80) { running = b; buf = [b]; need = ((b & 0xF0) === 0xC0 || (b & 0xF0) === 0xD0) ? 2 : 3; if ((b & 0xF0) === 0xF0) { buf = []; running = 0; } }
    else if (running) { if (!buf.length) buf = [running]; buf.push(b); }
    else return;
    if (buf.length === need) {
        var ch = (buf[0] & 0x0F) + 1;
        if (remap && laneCh) { outlet(0, (buf[0] & 0xF0) | (laneCh - 1), buf[1], buf.length > 2 ? buf[2] : 0); }
        else if (chans[ch]) {
            outlet(0, buf[0], buf[1], buf.length > 2 ? buf[2] : 0);
            if (meters && (buf[0] & 0xF0) === 0xB0 && ch === 4) meter(buf[1], buf[2]);
        }
        buf = [];
    }
}
// The meters and the seed text live in the lane's editor window (subpatcher `editor`). They are drawn only while that window is open (pmor_edit.js outlet 1 -> `editor 0|1`);
// the last values are kept and replayed when it opens.
var edOpen = 0, seedText = "";
function ed() { var b = this.patcher.getnamed("editor"); return b ? b.subpatcher() : null; }
function editor(v) {
    edOpen = v | 0;
    if (!edOpen || !meters) return;
    for (var cc in last) draw(cc | 0, last[cc]);
    if (seedText) drawSeed();
}
function draw(cc, v) { var p = ed(); if (!p) return; var s = p.getnamed("m" + cc); if (s) s.message("set", v); }
function drawSeed() { var p = ed(); if (!p) return; var c = p.getnamed("seed"); if (c) c.message("set", seedText); }
function meter(cc, v) {
    if (cc >= 1 && cc <= 7) {
        if (last[cc] === v) return;
        last[cc] = v;
        if (edOpen) draw(cc, v);
    } else if (cc >= 110 && cc <= 113) {
        seedParts[cc - 110] = v;
        if (cc === 113) {
            var S = (seedParts[0] << 21) | (seedParts[1] << 14) | (seedParts[2] << 7) | seedParts[3];
            seedText = "seed " + S;
            if (edOpen) drawSeed();
        }
    }
}
if (typeof module !== "undefined") module.exports = { feed: feed, msg: msg, editor: editor };   // node test (tests/test_lane.js)
