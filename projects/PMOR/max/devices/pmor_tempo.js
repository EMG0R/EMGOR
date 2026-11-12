// pmor_tempo.js -- sends Live's song tempo to PMOR (`tempo <bpm>` -> pmor.proto -> TEMPO <bpm_milli>) so a cold Start lands at the right tempo from tick 0.
// Sent: when the link reaches READY (`state ready` from pmor.proto), when the Live tempo changes, and when playback starts (right before the first 0xFA).
// Live Object Model: live_set tempo / is_playing observers (LiveAPI). Nothing is observed until `connect 1` (the CONNECT button); a failed LiveAPI (not inside Live) is silent.
// Inlet: connect 0|1, state <name> (pmor.proto status). Outlet 0: tempo <bpm>.
inlets = 1; outlets = 1;
var api = null, play = null, bpm = 0, on = 0;
function send() { if (bpm >= 40 && bpm <= 400) outlet(0, "tempo", Math.round(bpm * 1000) / 1000); }
function connect(v) {
    on = v | 0;
    if (!on) { api = null; play = null; return; }
    try {
        api = new LiveAPI(function (a) { if (a[0] === "tempo") { bpm = +a[1]; send(); } }, "live_set"); api.property = "tempo"; bpm = +api.get("tempo");
        play = new LiveAPI(function (a) { if (a[0] === "is_playing" && +a[1] === 1) send(); }, "live_set"); play.property = "is_playing";
    } catch (e) { api = null; play = null; }
}
function state(s) { if (on && String(s) === "ready") send(); }
function anything() { }
if (typeof module !== "undefined") module.exports = { connect: connect, state: state };
