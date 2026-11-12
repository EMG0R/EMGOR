// pmor_fx.js -- FX browser of the PMOR control lane. The 128 FX engage controls (live.text p<id>, ids 5120..5247) and the distortion algo (live.menu p5248) are
// real Live parameters (automatable, recorded from PMOR's moves, editable in the clip/automation lane); this js only NAVIGATES them.
// Inlet: bank <0..7>   (bank menu)      eff <0..15> (effect menu inside the bank)      param <id> <value> (from [receive pmor_rx], readout only)
// Hides every effect cell not in the selected bank (hidden objects stay Live parameters and keep following PMOR), refills the effect menu with the
// bank's names and writes the selected effect's name / state into the comment `fxsel`. Values for the cells come from pmor_ui.js (set = silent: no echo to PMOR).
inlets = 1; outlets = 0;
//@NAMES-BEGIN
var NAMES = [["ST4", "ST8", "ST16", "ST32", "REV", "CHOR", "PAN", "CRUSH", "HALF", "DBL", "BZZ", "MUTE", "FLT", "WASH", "DIST", "STOP"], ["DUB", "GVERB", "TGATE", "FLANG", "RING", "FOLD", "COMB", "VOWEL", "ISO-LO", "ISO-MD", "ISO-HI", "PHONE", "GRAIN", "WOW", "DOWN", "SCAT"], ["PLATE", "SHIMR", "PONG", "SLAP", "PHASE", "WAH", "TREM", "HAAS", "VIBRA", "DIMEN", "SPRNG", "SIREN", "WARM", "SUB", "AIR", "PUMP"], ["OCT+", "OCT-", "DSTRY", "REPT", "RGRN", "FREEZ", "METAL", "FRMNT", "FSHFT", "FUZZ", "GSEQ", "SRAMP", "VSTOP", "RADIO", "NGATE", "SHMOD"], ["RPT2", "RPT6", "SHUF", "Q6/8", "PUNCH", "FILL", "BUILD", "BALL", "LPF", "HPF", "SQUSH", "SCRTC", "SCRTF", "GLCH", "BLIND", "S&H"], ["DELAY", "PNGPG", "FADE", "FBACK", "DRONE", "FREEZ", "UNI", "UNILO", "DETUN", "PLFO", "BND+", "BND-", "WARBL", "5TH", "SUB", "LOOP"], ["GATE4", "GATE8", "GAT16", "TRNCE", "TREM", "PAN", "PUMP", "SWELL", "NOISE", "WIDE", "MONO", "BRITE", "DARK", "DRIVE", "SGATE", "KILL"], ["RAT2", "RAT3", "RAT4", "RAT8", "STRUM", "TRNC2", "STAB", "RINGL", "RINGH", "VIBR", "CHOP", "HALF", "DBL", "GLCH", "REVG", "SIL"]];
//@NAMES-END
var bank = 0, eff = 0, vals = {};
function o(n) { return this.patcher.getnamed(n); }
function id(b, e) { return 5120 + b * 16 + e; }
function show() {
    var c = o("fxsel"); if (!c) return;
    var v = vals[id(bank, eff)] >= 0.5;
    c.message("set", "FX" + (bank + 1) + "." + (eff < 9 ? "0" : "") + (eff + 1) + " " + NAMES[bank][eff] + "  " + (v ? "ON" : "off") + "   (engage 0/1)");
}
function layout() {
    for (var b = 0; b < 8; b++) for (var e = 0; e < 16; e++) { var c = o("p" + id(b, e)); if (c) c.hidden = (b !== bank) ? 1 : 0; }
    var m = o("fxeff");
    if (m) { m.message("clear"); for (var i = 0; i < 16; i++) m.message("append", NAMES[bank][i]); m.message("set", eff); }
    show();
}
function bank_(n) { n = n | 0; if (n < 0 || n > 7) return; bank = n; layout(); }
function eff_(n) { n = n | 0; if (n < 0 || n > 15) return; eff = n; show(); }
function loadbang() { layout(); }
function param(i, v) { i = i | 0; if (i >= 5120 && i < 5248) { vals[i] = v; if (i === id(bank, eff)) show(); } }
// Max js cannot name a function `bank` AND keep a var: the patcher sends `bank n` / `eff n`
function anything() { var a = arrayfromargs(arguments); if (messagename === "bank") bank_(a[0]); else if (messagename === "eff") eff_(a[0]); }
if (typeof module !== "undefined") module.exports = { bank: bank_, eff: eff_, param: param };
