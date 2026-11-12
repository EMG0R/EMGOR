// pmor_guard.js -- the ONLY path from a device's controls to [send pmor_tx]. PMOR is the source of truth: loading a device or a Live set must never write to PMOR.
// A live.* control re-emits its stored / initial value when a device or a set loads (Live restores parameters). Those loads arrive as `setparam ...` within
// milliseconds of the device being created, so every `setparam` is DROPPED until GRACE_MS after this script was instantiated. Everything else (hello, getpatch,
// dumpparams, pool, assign...) passes unchanged: only `setparam` carries a value that could overwrite PMOR.
// After the grace period a setparam passes only if it differs from the last value PMOR reported for that id (`param <id> <v>` from [receive pmor_rx] feeds `seen`),
// so a control that merely re-emits what PMOR already holds is not a write either. `arm` (test hook / `armnow`) ends the grace early.
// Outlets: 0 -> [send pmor_tx]; 1 -> dropped-count comment (optional, `set <text>`).
inlets = 1; outlets = 2;
var GRACE_MS = 3000, t0 = Date.now(), seen = {}, dropped = 0;
function now() { return Date.now(); }
function armnow() { t0 = now() - GRACE_MS - 1; }
function setgrace(ms) { GRACE_MS = ms | 0; }
function param(id, v) { seen[id | 0] = +v; }                 // PMOR -> device values (never forwarded)
function setparam(id, v) {
    id = id | 0; v = +v;
    if (now() - t0 < GRACE_MS) { dropped++; outlet(1, "set", "load guard: " + dropped + " dropped"); return; }
    if (seen[id] !== undefined && Math.abs(seen[id] - v) < 1e-9) return;     // equal to what PMOR holds: not a change
    seen[id] = v;
    outlet(0, "setparam", id, v);
}
function anything() { outlet.apply(this, [0, messagename].concat(arrayfromargs(arguments))); }
function loadbang() { t0 = now(); }
if (typeof module !== "undefined") module.exports = { setparam: setparam, param: param, anything: anything, armnow: armnow, setgrace: setgrace };
