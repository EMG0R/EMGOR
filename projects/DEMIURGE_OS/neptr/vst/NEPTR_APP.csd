<Cabbage>
form caption("NEPTR PHASE 4"), size(750, 646), colour(28, 0, 30), pluginid("NPTR"), guiMode("queue")
label bounds(10, 10, 140, 20), text("NEPTR PHASE 4"), fontSize(15), align("left"), fontColour(220, 186, 222)
label bounds(160, 13, 18, 12), text("IN"), fontSize(10), align("left"), fontColour(155, 114, 158)
hslider bounds(182, 10, 104, 22), channel("meter_in"), range(0, 1, 0), colour(202, 42, 235), trackerColour(202, 42, 235), active(0)
label bounds(338, 13, 54, 13), text("PRESET"), fontSize(10), align("right"), fontColour(155, 114, 158)
combobox bounds(398, 8, 230, 24), channel("presetCombo"), populate("*.snaps"), channelType("string"), colour(40, 10, 44), fontColour(220, 186, 222)
filebutton bounds(632, 8, 110, 24), text("SAVE PRESET", "SAVE PRESET"), mode("named snapshot"), populate("*.snaps"), corners(3), fontSize(11), colour:0(245, 44, 218), colour:1(245, 44, 218), fontColour:0(255, 255, 255), fontColour:1(255, 255, 255)
label bounds(8, 40, 46, 12), text("VOLUME"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 34, 60, 26), channel("on_vol"), text("VOLUME"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 34, 60, 26), channel("on_atk"), text("ATTACK"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 34, 60, 26), channel("on_les"), text("LESLIE"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 34, 60, 26), channel("on_rm"), text("RINGMOD"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(310, 34, 60, 26), channel("on_tape"), text("TAPE"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 69, 46, 12), text("SPECTRAL"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 63, 60, 26), channel("on_fms"), text("FORMS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 63, 60, 26), channel("on_fmt"), text("FORMANT"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 63, 60, 26), channel("on_bsc"), text("BINSCATTER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 63, 60, 26), channel("on_bin"), text("BINRED"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(310, 63, 60, 26), channel("on_bc"), text("BCRUSH"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 98, 46, 12), text("PITCH"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 92, 60, 26), channel("on_h90"), text("H90"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 92, 60, 26), channel("on_ps"), text("OLD PS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 92, 60, 26), channel("on_gps"), text("GPS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 92, 60, 26), channel("on_pf"), text("P-FORK"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 127, 46, 12), text("FILTER"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 121, 60, 26), channel("on_lsy"), text("LOSSY"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 121, 60, 26), channel("on_res"), text("RESON"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 121, 60, 26), channel("on_wah"), text("WAH"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 121, 60, 26), channel("on_mog"), text("MOOG"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 156, 46, 12), text("MOD"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 150, 60, 26), channel("on_vib"), text("VIBRATO"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 150, 60, 26), channel("on_cho"), text("CHORUS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 150, 60, 26), channel("on_pha"), text("PHASER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 150, 60, 26), channel("on_trm"), text("TREMOLO"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(310, 150, 60, 26), channel("on_fla"), text("FLANGER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 185, 46, 12), text("DRIVE"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 179, 60, 26), channel("on_od"), text("OD"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 179, 60, 26), channel("on_mar"), text("MARSH"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 179, 60, 26), channel("on_rmz"), text("RAT-MZ"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 179, 60, 26), channel("on_dhg"), text("DS-1K"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(310, 179, 60, 26), channel("on_muf"), text("BIGMUFF"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 214, 46, 12), text("PREAMP"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 208, 60, 26), channel("on_pre"), text("MAINPRE"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 208, 60, 26), channel("on_bpl"), text("BASS L"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(184, 208, 60, 26), channel("on_bph"), text("BASS H"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(247, 208, 60, 26), channel("on_bds"), text("BASSDST"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(310, 208, 60, 26), channel("on_cmp"), text("COMP"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(8, 243, 46, 12), text("GRANULAR"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(58, 237, 60, 26), channel("on_grn"), text("GRAIN"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(121, 237, 60, 26), channel("on_mod"), text("MOOD"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 40, 46, 12), text("DIGIVERB"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 34, 60, 26), channel("on_neb"), text("NEBULA"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 34, 60, 26), channel("on_shm"), text("SHIMMER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 34, 60, 26), channel("on_frz"), text("FREEZE"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 34, 60, 26), channel("on_gvb"), text("GRAINVERB"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(682, 34, 60, 26), channel("on_vrb"), text("REVERB"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 69, 46, 12), text("DELAY"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 63, 60, 26), channel("on_dly"), text("DELAY"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 63, 60, 26), channel("on_sdl"), text("SPECDLY"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 63, 60, 26), channel("on_tdl"), text("TAPE DELAY"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 63, 60, 26), channel("on_mvd"), text("POLY DLY"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 98, 46, 12), text("BASS"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 92, 60, 26), channel("on_oct"), text("OCTAVER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 92, 60, 26), channel("on_env"), text("ENVFLT"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 127, 46, 12), text("SYNTH"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 121, 60, 26), channel("on_tsy"), text("TRKSYN"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 121, 60, 26), channel("on_pll"), text("PLLFUZZ"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 121, 60, 26), channel("on_teq"), text("TONE EQ"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 156, 46, 12), text("AIR"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 150, 60, 26), channel("on_fbk"), text("FEEDBK"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 150, 60, 26), channel("on_dif"), text("DIFFUS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 150, 60, 26), channel("on_cfb"), text("CHAINFB"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 185, 46, 12), text("CH BLISS"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 179, 60, 26), channel("on_blp"), text("BLOOPER"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 179, 60, 26), channel("on_onw"), text("ONWARD"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 179, 60, 26), channel("on_hab"), text("HABIT"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 179, 60, 26), channel("on_md2"), text("MOOD II"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 214, 46, 12), text("GLITCH"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 208, 60, 26), channel("on_imp"), text("IMPULSE"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 208, 60, 26), channel("on_bbc"), text("BBCUT"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 208, 60, 26), channel("on_rpt"), text("REPEAT"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 208, 60, 26), channel("on_chp"), text("CHOP"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(682, 208, 60, 26), channel("on_rvs"), text("RVRS"), corners(3), fontSize(9), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 243, 46, 12), text("NAM"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 237, 60, 26), channel("nam_row0"), text("AC30"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 237, 60, 26), channel("nam_row1"), text("DELUXE"), corners(3), fontSize(11), value(1), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 237, 60, 26), channel("nam_row2"), text("FENDER"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 237, 60, 26), channel("nam_row3"), text("TWEED"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(682, 237, 60, 26), channel("nam_row4"), text("CRUNCH"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(380, 272, 46, 12), text("IR VERB"), fontSize(9), align("left"), fontColour(155, 114, 158)
button bounds(430, 266, 60, 26), channel("ir_row0"), text("SPRING"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(493, 266, 60, 26), channel("ir_row1"), text("PLATE"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(556, 266, 60, 26), channel("ir_row2"), text("HALL"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(619, 266, 60, 26), channel("ir_row3"), text("CHAMBER"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(682, 266, 60, 26), channel("ir_row4"), text("WOOD RM"), corners(3), fontSize(11), value(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(55, 25, 62), outlineThickness(1), corners(6), channel("pnl_none"), visible(1) {
label bounds(0, 46, 734, 18), text("CLICK AN EFFECT TO EDIT ITS CONTROLS"), fontSize(12), align("centre"), fontColour(155, 114, 158)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_vol"), visible(0) {
label bounds(12, 4, 300, 16), text("VOLUME"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("vol_level"), range(0, 1, 1.0, 1, 0.001), text("LEVEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_atk"), visible(0) {
label bounds(12, 4, 300, 16), text("ATTACK"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("atk_mix"), range(0, 1, 1, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("atk_time"), range(10, 2000, 150, 0.5, 0.001), text("ATK MS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("atk_sens"), range(0, 1, 0.5, 1, 0.001), text("SENS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("atk_shape"), range(0, 1, 0.5, 1, 0.001), text("SHAPE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("atk_range"), range(0, 1, 0.7, 1, 0.001), text("RANGE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(327, 24, 58, 70), channel("atk_retrig"), range(20, 150, 45, 1, 0.001), text("RETRIG"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_les"), visible(0) {
label bounds(12, 4, 300, 16), text("LESLIE"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("les_speed"), range(0.1, 10, 7.5, 0.5, 0.001), text("SPEED HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("les_depth"), range(0, 1, 1.0, 1, 0.001), text("MIC DIST"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_rm"), visible(0) {
label bounds(12, 4, 300, 16), text("RINGMOD"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("rm_depth"), range(0, 1, 1.0, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("rm_freq"), range(0.05, 250, 0.1, 0.25, 0.001), text("FREQ HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_tape"), visible(0) {
label bounds(12, 4, 300, 16), text("TAPE"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("tape_amt"), range(0, 1, 0.4, 1, 0.001), text("AMOUNT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("tape_fail"), range(0, 1, 0.25, 1, 0.001), text("FAIL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("tape_model"), range(0, 1, 0.35, 1, 0.001), text("MODEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_fms"), visible(0) {
label bounds(12, 4, 300, 16), text("FORMS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("fms_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("fms_shift"), range(-12, 12, 0.0, 1, 0.5), text("SHIFT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_fmt"), visible(0) {
label bounds(12, 4, 300, 16), text("FORMANT"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("fmt_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("fmt_shift"), range(-12, 12, 0.0, 1, 0.5), text("SHIFT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_sdl"), visible(0) {
label bounds(12, 4, 300, 16), text("SPECDLY"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("sdl_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("sdl_maxdel"), range(0, 1, 0.3, 1, 0.001), text("MAX DEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("sdl_fback"), range(0, 1, 0.8, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bin"), visible(0) {
label bounds(12, 4, 300, 16), text("BIN REDU"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bin_pedal"), range(0, 1, 1, 1, 0.001), text("REDUCE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bc"), visible(0) {
label bounds(12, 4, 300, 16), text("BCRUSHER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bc_pedal"), range(0, 1, 0, 1, 0.001), text("COARSE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bsc"), visible(0) {
label bounds(12, 4, 300, 16), text("BINSCATTER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bsc_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("bsc_scatter"), range(0, 1, 0.7, 1, 0.001), text("SCATTER"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("bsc_fback"), range(0.5, 1.1, 0.95, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("bsc_window"), range(0.02, 1.0, 0.15, 1, 0.001), text("WINDOW"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_h90"), visible(0) {
label bounds(12, 4, 300, 16), text("H90"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("h90_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("h90_semi"), range(-12, 12, 0.0, 1, 0.5), text("SEMI"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_ps"), visible(0) {
label bounds(12, 4, 300, 16), text("OLD PS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("ps_mix"), range(0, 100, 100.0, 1, 5), text("DRY %"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("ps_semi"), range(-12, 12, 0.0, 1, 0.5), text("SEMI"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_gps"), visible(0) {
label bounds(12, 4, 300, 16), text("GPS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("gps_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("gps_semi"), range(-12, 12, 12.0, 1, 0.5), text("SEMI"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("gps_fback"), range(0, 0.99, 0.0, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("gps_dly"), range(0, 0.1, 0.001, 1, 0.001), text("DELAY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_pf"), visible(0) {
label bounds(12, 4, 300, 16), text("P-FORK"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("pf_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("pf_semi"), range(-12, 12, 5.0, 1, 2.5), text("SEMI"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_lsy"), visible(0) {
label bounds(12, 4, 300, 16), text("LOSSY"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("lsy_mix"), range(0, 1, 1, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("lsy_loss"), range(0, 1, 0.35, 1, 0.001), text("LOSS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("lsy_bw"), range(0, 1, 0.8, 1, 0.001), text("BANDW"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("lsy_pkt"), range(0, 1, 0, 1, 0.001), text("PACKETS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_res"), visible(0) {
label bounds(12, 4, 300, 16), text("RESONATR"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("res_mix"), range(0, 1, 0.4, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("res_onset"), range(0, 1, 0.5, 1, 0.001), text("ONSET"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("res_fb"), range(0, 1, 0.6, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("res_oct"), range(-1, 1, 0, 1, 0.001), text("OCTAVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(264, 42, 104, 12), text("FREEZE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(264, 55, 104, 24), channel("res_frz"), items("RUN", "FREEZE"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_wah"), visible(0) {
label bounds(12, 4, 300, 16), text("WAH"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("wah_freq"), range(0, 1, 0.5, 1, 0.001), text("FREQ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("wah_res"), range(0.5, 0.97, 0.7, 1, 0.001), text("RES"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_mog"), visible(0) {
label bounds(12, 4, 300, 16), text("AUTOMOOG"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("mog_sens"), range(0, 1, 0.8, 1, 0.001), text("SENS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("mog_res"), range(0, 0.95, 0.85, 1, 0.001), text("RES"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(138, 42, 104, 12), text("TYPE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(138, 55, 104, 24), channel("mog_type"), items("TYPE 1", "TYPE 2", "TYPE 3", "TYPE 4"), value(2), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(264, 24, 58, 70), channel("mog_level"), range(0, 2, 1.0, 1, 0.001), text("LEVEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_vib"), visible(0) {
label bounds(12, 4, 300, 16), text("VIBRATO"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("vib_depth"), range(0, 1, 0.8, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("vib_rate"), range(0.05, 12, 1.2, 0.5, 0.001), text("RATE HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("vib_fback"), range(0, 0.7, 0.0, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_cho"), visible(0) {
label bounds(12, 4, 300, 16), text("CHORUS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("cho_mix"), range(0, 1, 0.9, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("cho_rate"), range(0.05, 8, 0.7, 0.5, 0.001), text("RATE HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("cho_depth"), range(0, 1, 0.4, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_pha"), visible(0) {
label bounds(12, 4, 300, 16), text("PHASER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("pha_depth"), range(0, 1, 0.5, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("pha_rate"), range(0.05, 12, 2.0, 0.5, 0.001), text("RATE HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("pha_fback"), range(0, 0.9, 0.2, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("pha_stages"), range(1, 24, 8.0, 1, 1), text("STAGES"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_trm"), visible(0) {
label bounds(12, 4, 300, 16), text("TREMOLO"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("trm_depth"), range(0, 1, 0.7, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(75, 42, 104, 12), text("STYLE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(75, 55, 104, 24), channel("trm_mode"), items("SINE", "OPTO", "BIAS", "HARM"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(201, 24, 58, 70), channel("trm_rate"), range(0.5, 20, 7.38, 0.5, 0.001), text("RATE HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_fla"), visible(0) {
label bounds(12, 4, 300, 16), text("FLANGER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("fla_depth"), range(0, 1, 0.5, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("fla_rate"), range(0.05, 12, 2.0, 0.5, 0.001), text("RATE HZ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("fla_fback"), range(-1, 1, 0.2, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_od"), visible(0) {
label bounds(12, 4, 300, 16), text("TUBESCREAM"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("od_drive"), range(0, 1, 0.4, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("od_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("od_morph"), range(0, 1, 0.5, 1, 0.001), text("BODY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("od_blend"), range(0, 1, 0.0, 1, 0.001), text("CLEAN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_mar"), visible(0) {
label bounds(12, 4, 300, 16), text("MARSHALL"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("mar_drive"), range(0, 1, 0.75, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("mar_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("mar_pres"), range(0, 1, 0.4, 1, 0.001), text("PRESENCE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("mar_tight"), range(0, 1, 0.3, 1, 0.001), text("TIGHT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_rmz"), visible(0) {
label bounds(12, 4, 300, 16), text("RAT-MZ"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("rmz_drive"), range(0, 1, 0.75, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("rmz_tone"), range(0, 1, 0.55, 1, 0.001), text("FILTER"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("rmz_midf"), range(0, 1, 0.4, 1, 0.001), text("MID FRQ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("rmz_midg"), range(0, 1, 0.5, 1, 0.001), text("MID GAIN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_dhg"), visible(0) {
label bounds(12, 4, 300, 16), text("DS-1K"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("dhg_drive"), range(0, 1, 0.75, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("dhg_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("dhg_blend"), range(0, 1, 0.5, 1, 0.001), text("FAT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("dhg_tight"), range(0, 1, 0.25, 1, 0.001), text("EDGE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_muf"), visible(0) {
label bounds(12, 4, 300, 16), text("BIGMUFF"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("muf_drive"), range(0, 1, 0.75, 1, 0.001), text("SUSTAIN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("muf_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("muf_mids"), range(0, 1, 0.15, 1, 0.001), text("MIDS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("muf_voice"), range(0, 1, 0.85, 1, 0.001), text("VOICING"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_pre"), visible(0) {
label bounds(12, 4, 300, 16), text("MAIN PRE"), fontSize(14), align("left"), fontColour(220, 186, 222)
label bounds(12, 56, 500, 16), fontSize(12), align("left"), fontColour(155, 114, 158), text("No parameters — just the toggle.")
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bpl"), visible(0) {
label bounds(12, 4, 300, 16), text("BASS PRE L"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bpl_drive"), range(0, 1, 0.25, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bph"), visible(0) {
label bounds(12, 4, 300, 16), text("BASS PRE H"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bph_drive"), range(0, 1, 0.55, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bds"), visible(0) {
label bounds(12, 4, 300, 16), text("BASS DIST"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bds_drive"), range(0, 1, 0.6, 1, 0.001), text("DRIVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("bds_blend"), range(0, 1, 0.35, 1, 0.001), text("CLEAN LOW"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("bds_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_cmp"), visible(0) {
label bounds(12, 4, 300, 16), text("COMPRESS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("cmp_amt"), range(0, 1, 0, 1, 0.001), text("AMT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("cmp_speed"), range(0, 1, 0, 1, 0.001), text("SPEED"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("cmp_makeup"), range(0, 1, 0, 1, 0.001), text("MAKEUP"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("cmp_blend"), range(0, 1, 0, 1, 0.001), text("BLEND"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("cmp_gate"), range(0, 1, 0, 1, 0.001), text("GATE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_dly"), visible(0) {
label bounds(12, 4, 300, 16), text("DELAY"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("dly_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("dly_time"), range(0, 700, 350, 1, 0.001), text("TIME MS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("dly_fback"), range(0, 1.2, 0.5, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("dly_semi"), range(-5, 12, -0.01, 1, 1), text("SEMI"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(264, 42, 104, 12), text("REPEATS"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(264, 55, 104, 24), channel("dly_mode"), items("NORMAL", "REVERSE"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_tdl"), visible(0) {
label bounds(12, 4, 300, 16), text("TAPE DELAY"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("tdl_mix"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("tdl_time"), range(60, 1200, 300, 0.5, 0.001), text("TIME MS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("tdl_fback"), range(0, 1.15, 0.35, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("tdl_fctr"), range(250, 5000, 1200, 0.5, 0.001), text("FILT CTR"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("tdl_fq"), range(0.5, 6, 0.7, 1, 0.001), text("FILT Q"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(327, 42, 104, 12), text("HEAD MODE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(327, 55, 104, 24), channel("tdl_head"), items("1", "2", "3", "2+3", "1+2", "1+3", "1+2+3"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_mvd"), visible(0) {
label bounds(12, 4, 300, 16), text("POLY DLY"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("mvd_numl"), range(1, 16, 1, 1, 0.001), text("NUM L"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("mvd_denl"), range(1, 16, 2, 1, 0.001), text("DEN L"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("mvd_numr"), range(1, 16, 1, 1, 0.001), text("NUM R"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("mvd_denr"), range(1, 16, 3, 1, 0.001), text("DEN R"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("mvd_att"), range(0, 1, 0, 1, 0.001), text("ATTACK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(327, 24, 58, 70), channel("mvd_dec"), range(0, 1, 0, 1, 0.001), text("DECAY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(390, 24, 58, 70), channel("mvd_reps"), range(1, 8, 4, 1, 0.001), text("REPEATS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(453, 24, 58, 70), channel("mvd_intv"), range(-12, 12, 7, 1, 0.001), text("INTERVAL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_neb"), visible(0) {
label bounds(12, 4, 300, 16), text("NEBULA"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("neb_mix"), range(0, 1, 1, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("neb_dens"), range(0, 1, 0.4, 1, 0.001), text("DENSITY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("neb_size"), range(0, 1, 0.6, 1, 0.001), text("SIZE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_shm"), visible(0) {
label bounds(12, 4, 300, 16), text("SHIMMER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("shm_mix"), range(0, 1, 1, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(75, 42, 104, 12), text("INTERVAL"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(75, 55, 104, 24), channel("shm_intv"), items("+12", "+7", "+5", "-12"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(201, 24, 58, 70), channel("shm_bloom"), range(0, 1, 0.5, 1, 0.001), text("BLOOM"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_frz"), visible(0) {
label bounds(12, 4, 300, 16), text("FREEZE"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("frz_bed"), range(0, 1, 0.8, 1, 0.001), text("BED LVL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("frz_damp"), range(0, 1, 0.5, 1, 0.001), text("DAMP"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("frz_listen"), range(0, 1, 0.5, 1, 0.001), text("LISTEN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
button bounds(201, 24, 58, 70), channel("frz_clear"), text("CLEAR"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_gvb"), visible(0) {
label bounds(12, 4, 300, 16), text("GRAINVERB"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("gvb_mix"), range(0, 1, 1, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("gvb_spacing"), range(0, 1, 0.5, 1, 0.001), text("SPACING"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("gvb_decay"), range(0, 1, 0.5, 1, 0.001), text("DECAY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_vrb"), visible(0) {
label bounds(12, 4, 300, 16), text("REVERB"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("vrb_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("vrb_room"), range(0, 0.999, 0.92, 1, 0.001), text("ROOM"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("vrb_damp"), range(0, 1, 0.9, 1, 0.001), text("DAMP"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_grn"), visible(0) {
label bounds(12, 4, 300, 16), text("GRANULAR"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("grn_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("grn_dens"), range(10, 400, 80.0, 1, 10), text("DENSITY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("grn_size"), range(0.05, 1.2, 0.55, 1, 0.001), text("SIZE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("grn_pitch"), range(0, 24, 6.0, 1, 1), text("PITCH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_mod"), visible(0) {
label bounds(12, 4, 300, 16), text("MOOD"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("mod_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("mod_clock"), range(0, 1, 0.0, 1, 0.001), text("PITCH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("mod_dens"), range(0, 1400, 200.0, 1, 0.001), text("DENSITY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("mod_dly"), range(0, 1, 0.7, 1, 0.001), text("MAX DLY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_oct"), visible(0) {
label bounds(12, 4, 300, 16), text("OCTAVER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("oct_sub"), range(0, 1, 0, 1, 0.001), text("SUB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("oct_tone"), range(0, 1, 0, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("oct_char"), range(0, 1, 0, 1, 0.001), text("CHAR"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_env"), visible(0) {
label bounds(12, 4, 300, 16), text("ENV FILTER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("env_wet"), range(0, 1, 0, 1, 0.001), text("WET"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("env_sens"), range(0, 1, 0, 1, 0.001), text("SENS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("env_mode"), range(0, 1, 0, 1, 0.001), text("MODE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("env_att"), range(0, 1, 0, 1, 0.001), text("ATTACK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("env_dec"), range(0, 1, 0, 1, 0.001), text("DECAY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_tsy"), visible(0) {
label bounds(12, 4, 300, 16), text("TRACKSYN"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("tsy_mix"), range(0, 1, 0, 1, 0.001), text("BLEND"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("tsy_cut"), range(0, 1, 0, 1, 0.001), text("CUTOFF"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("tsy_res"), range(0, 1, 0, 1, 0.001), text("RES"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("tsy_glide"), range(0, 1, 0, 1, 0.001), text("GLIDE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("tsy_sub"), range(0, 1, 0, 1, 0.001), text("SUB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(327, 24, 58, 70), channel("tsy_det"), range(0, 1, 0, 1, 0.001), text("DETUNE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_pll"), visible(0) {
label bounds(12, 4, 300, 16), text("PLLFUZZ"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("pll_mix"), range(0, 1, 0, 1, 0.001), text("DRY/WET"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(75, 42, 104, 12), text("INTERVAL"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(75, 55, 104, 24), channel("pll_intv"), items("/4", "/2", "x1", "x2", "x3"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(201, 24, 58, 70), channel("pll_lock"), range(0, 1, 0, 1, 0.001), text("LOCK SPD"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("pll_vmix"), range(0, 1, 0, 1, 0.001), text("VOICE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_teq"), visible(0) {
label bounds(12, 4, 300, 16), text("TONE EQ"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("teq_mid"), range(0, 1, 0.5, 1, 0.001), text("MID"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("teq_freq"), range(0, 1, 0.5, 1, 0.001), text("FREQ"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("teq_tilt"), range(0, 1, 0.5, 1, 0.001), text("TILT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("teq_low"), range(0, 1, 0.5, 1, 0.001), text("LOW"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("teq_high"), range(0, 1, 0.5, 1, 0.001), text("HIGH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_fbk"), visible(0) {
label bounds(12, 4, 300, 16), text("FEEDBACKER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("fbk_level"), range(0, 1, 0, 1, 0.001), text("LEVEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(75, 42, 104, 12), text("HARMONIC"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(75, 55, 104, 24), channel("fbk_harm"), items("FUND", "+OCT", "+OCT5", "+2OCT"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(201, 24, 58, 70), channel("fbk_bloom"), range(0, 1, 0, 1, 0.001), text("BLOOM"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("fbk_vib"), range(0, 1, 0, 1, 0.001), text("VIBRATO"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_dif"), visible(0) {
label bounds(12, 4, 300, 16), text("DIFFUSION"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("dif_wet"), range(0, 1, 0, 1, 0.001), text("DRY/WET"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("dif_depth"), range(0, 1, 0, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("dif_rate"), range(0, 1, 0, 1, 0.001), text("MOD RATE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("dif_duck"), range(0, 1, 0, 1, 0.001), text("DUCK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_cfb"), visible(0) {
label bounds(12, 4, 300, 16), text("CHAIN FB"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("cfb_ret"), range(0, 1, 0.4, 1, 0.001), text("RETURN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("cfb_gain"), range(0, 1, 0.5, 1, 0.001), text("FEEDBK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("cfb_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("cfb_ceil"), range(0.1, 0.95, 0.8, 1, 0.001), text("CEILING"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_blp"), visible(0) {
label bounds(12, 4, 300, 16), text("BLOOPER"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("blp_motion"), range(0, 1, 0.45, 1, 0.001), text("MOTION"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("blp_scat"), range(0, 1, 0.45, 1, 0.001), text("MOD AMT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("blp_size"), range(0, 1, 0.5, 1, 0.001), text("GRAIN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(201, 42, 104, 12), text("THROW"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(201, 55, 104, 24), channel("blp_char"), items("NEAR", "THROW", "FAR"), value(2), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_onw"), visible(0) {
label bounds(12, 4, 300, 16), text("ONWARD"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("onw_mix"), range(0, 1, 0.6, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("onw_blend"), range(0, 1, 0.75, 1, 0.001), text("BLEND"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("onw_size"), range(0, 1, 0.3, 1, 0.001), text("SIZE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("onw_follow"), range(0, 1, 0.6, 1, 0.001), text("FOLLOW"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(264, 24, 58, 70), channel("onw_density"), range(0, 1, 0.15, 1, 0.001), text("DENSITY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(327, 24, 58, 70), channel("onw_err"), range(0, 1, 0.35, 1, 0.001), text("ERROR"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(390, 24, 58, 70), channel("onw_oct"), range(-1, 1, 0, 1, 1), text("OCTAVE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_hab"), visible(0) {
label bounds(12, 4, 300, 16), text("HABIT"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("hab_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("hab_size"), range(0, 1, 0.35, 1, 0.001), text("SIZE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("hab_repeats"), range(0, 1, 0.45, 1, 0.001), text("REPEATS"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("hab_blend"), range(0, 1, 1.0, 1, 0.001), text("BLEND"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(264, 42, 104, 12), text("BUFFER"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(264, 55, 104, 24), channel("hab_rec"), items("FROZEN", "REC"), value(2), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_md2"), visible(0) {
label bounds(12, 4, 300, 16), text("MOOD II"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("md2_mix"), range(0, 1, 0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("md2_clock"), range(0, 1, 0, 1, 0.001), text("CLOCK"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("md2_length"), range(0, 1, 0, 1, 0.001), text("LENGTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("md2_gen"), range(0, 1, 0, 1, 0.001), text("GENERATE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_imp"), visible(0) {
label bounds(12, 4, 300, 16), text("IMPULSE"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("imp_mix"), range(0, 1, 0.5, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("imp_prob"), range(0, 1, 0.5, 1, 0.001), text("PROB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("imp_dec"), range(0, 1, 0.25, 1, 0.001), text("DECAY"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("imp_tone"), range(0, 1, 0.5, 1, 0.001), text("TONE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_bbc"), visible(0) {
label bounds(12, 4, 300, 16), text("BBCUT"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("bbc_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("bbc_prob"), range(0, 1, 0.5, 1, 0.001), text("PROB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("bbc_flt"), range(0, 1, 0.6, 1, 0.001), text("FILTER"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_rpt"), visible(0) {
label bounds(12, 4, 300, 16), text("REPEAT"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("rpt_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("rpt_prob"), range(0, 1, 0.5, 1, 0.001), text("PROB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("rpt_char"), range(0, 1, 0, 1, 0.001), text("CHAR"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(201, 42, 104, 12), text("LEN"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(201, 55, 104, 24), channel("rpt_len"), items("1/2 TICK", "1 TICK", "2 TICK"), value(2), colour(40, 10, 44), fontColour(220, 186, 222)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_chp"), visible(0) {
label bounds(12, 4, 300, 16), text("CHOP"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("chp_depth"), range(0, 1, 1.0, 1, 0.001), text("DEPTH"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("chp_prob"), range(0, 1, 0.5, 1, 0.001), text("PROB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("chp_rate"), range(0, 1, 0, 1, 0.001), text("RATE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(201, 24, 58, 70), channel("chp_shape"), range(0, 1, 0, 1, 0.001), text("SHAPE"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_rvs"), visible(0) {
label bounds(12, 4, 300, 16), text("RVRS"), fontSize(14), align("left"), fontColour(220, 186, 222)
rslider bounds(12, 24, 58, 70), channel("rvs_mix"), range(0, 1, 1.0, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(75, 24, 58, 70), channel("rvs_prob"), range(0, 1, 0.5, 1, 0.001), text("PROB"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(138, 24, 58, 70), channel("rvs_len"), range(0, 1, 0, 1, 0.001), text("LEN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_nam"), visible(0) {
label bounds(12, 5, 120, 16), text("NAM AMP"), fontSize(14), align("left"), fontColour(220, 186, 222)
label bounds(12, 24, 108, 12), text("AC30"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(12, 37, 108, 24), channel("nam_sel0"), items("AC30 JAZZ CLEAN", "AC30 JAZZ EDGE", "AC30 JAZZ SATURATED", "AC30 TOPBOOST LOWGAIN", "AC30 TOPBOOST FULLGAIN"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
label bounds(128, 24, 108, 12), text("DELUXE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(128, 37, 108, 24), channel("nam_sel1"), items("DELUXE REVERB CLEAN", "DELUXE REVERB2 CLEAN", "DELUXE REVERB2 LIGHTDRIVE", "DELUXE REVERB2 CRUNCH", "DELUXE REVERB2 CRUNCH HOT"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
label bounds(244, 24, 108, 12), text("FENDER"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(244, 37, 108, 24), channel("nam_sel2"), items("FENDER TWIN CLEAN", "SUPER REVERB 1977", "PRINCETON CLEAN V3", "PRINCETON EDGE V5", "PRINCETON CRUNCH V7"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
label bounds(360, 24, 108, 12), text("TWEED"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(360, 37, 108, 24), channel("nam_sel3"), items("TWEED 5E3 CLEAN", "TWEED 5E3 EDGE", "TWEED 5E3 BREAKUP", "TWEED 5E3 BREAKUP 421", "TWEED 5E3 BREAKUP 160"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
label bounds(476, 24, 108, 12), text("CRUNCH"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(476, 37, 108, 24), channel("nam_sel4"), items("JC120 CLEAN", "VOX NORMAL MIDGAIN", "PLEXI SL59 LOWDRIVE", "PLEXI SL59 MIDDRIVE", "MARSHALL SL68 DRIVE"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(600, 12, 58, 90), channel("nam_in_gain"), range(-12, 18, 0.0, 1, 0.001), text("IN"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(666, 12, 58, 90), channel("nam_out_gain"), range(-18, 18, 0.0, 1, 0.001), text("OUT"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
image bounds(8, 307, 734, 114), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6), channel("pnl_ir"), visible(0) {
label bounds(12, 5, 120, 16), text("IR VERB"), fontSize(14), align("left"), fontColour(220, 186, 222)
label bounds(12, 16, 108, 12), text("SPRING"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(12, 29, 108, 24), channel("ir_sel0"), items("SPRING SHORT FLAT", "SPRING MED FLAT", "SPRING LONG FLAT", "SPRING MED BRIGHT", "SPRING MED DARK"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(42, 50, 46, 48), channel("ir_mix0"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(128, 16, 108, 12), text("PLATE"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(128, 29, 108, 24), channel("ir_sel1"), items("PLATE EMT140 1S", "PLATE EMT140 2S", "PLATE EMT140 3S", "PLATE FOIL SHORT", "PLATE FOIL LONG"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(158, 50, 46, 48), channel("ir_mix1"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(244, 16, 108, 12), text("HALL"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(244, 29, 108, 24), channel("ir_sel2"), items("HALL JACK LYONS", "HALL CENTRAL YORK", "HALL USINA ARTE", "HALL WIDE ECHO", "HALL MUSIKVEREIN"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(274, 50, 46, 48), channel("ir_mix2"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(360, 16, 108, 12), text("CHAMBER"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(360, 29, 108, 24), channel("ir_sel3"), items("CHAMBER YORK GUILDHALL", "CHAMBER MAUSOLEUM", "CHAMBER CATHEDRAL", "CHAMBER SILO", "CHAMBER COLUMNS"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(390, 50, 46, 48), channel("ir_mix3"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
label bounds(476, 16, 108, 12), text("WOOD RM"), fontSize(10), align("centre"), fontColour(155, 114, 158)
combobox bounds(476, 29, 108, 24), channel("ir_sel4"), items("WOOD TERRYS TYPING", "WOOD GENESIS6 LIVE", "WOOD SPOKANE CLUB", "WOOD RARE BOOKS", "WOOD DRUM ROOM"), value(1), colour(40, 10, 44), fontColour(220, 186, 222)
rslider bounds(506, 50, 46, 48), channel("ir_mix4"), range(0, 1, 0.35, 1, 0.001), text("MIX"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(624, 18, 52, 68), channel("ir_damp"), range(0, 1, 0.5, 1, 0.001), text("DAMP"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
rslider bounds(684, 18, 52, 68), channel("ir_level"), range(0, 1, 0.8, 1, 0.001), text("LEVEL"), fontSize(10), textColour(155, 114, 158), colour(138, 44, 245), trackerColour(220, 186, 222), outlineColour(55, 25, 62), markerColour(255, 255, 255)
}
label bounds(10, 433, 90, 12), text("EXPR PEDAL"), fontSize(10), fontColour(155, 114, 158)
hslider bounds(10, 445, 322, 28), channel("expr"), range(0, 1, 0, 1, 0.001), colour(145, 89, 255), trackerColour(145, 89, 255)
label bounds(342, 451, 260, 14), text("PEDAL TARGET: -"), fontSize(11), align("left"), fontColour(220, 186, 222), channel("exprLbl")
checkbox bounds(614, 449, 126, 20), channel("stereo_in"), text("STEREO IN"), value(1), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(220, 186, 222)
image bounds(8, 495, 734, 95), colour(40, 10, 44), outlineColour(113, 64, 117), outlineThickness(1), corners(6)
label bounds(14, 501, 70, 12), text("LOOPER"), fontSize(10), align("left"), fontColour(155, 114, 158)
button bounds(14, 515, 70, 24), channel("lp1_btn"), text("LOOP 1"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(14, 542, 70, 13), text("IDLE +1x"), fontSize(9), align("centre"), fontColour(155, 114, 158), channel("lp1_state")
combobox bounds(14, 558, 70, 22), channel("lp1_speed"), items("-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"), value(7), colour(40, 10, 44), fontColour(220, 186, 222), fontSize(9)
button bounds(92, 515, 70, 24), channel("lp2_btn"), text("LOOP 2"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(92, 542, 70, 13), text("IDLE +1x"), fontSize(9), align("centre"), fontColour(155, 114, 158), channel("lp2_state")
combobox bounds(92, 558, 70, 22), channel("lp2_speed"), items("-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"), value(7), colour(40, 10, 44), fontColour(220, 186, 222), fontSize(9)
button bounds(170, 515, 70, 24), channel("lp3_btn"), text("LOOP 3"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(170, 542, 70, 13), text("IDLE +1x"), fontSize(9), align("centre"), fontColour(155, 114, 158), channel("lp3_state")
combobox bounds(170, 558, 70, 22), channel("lp3_speed"), items("-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"), value(7), colour(40, 10, 44), fontColour(220, 186, 222), fontSize(9)
button bounds(248, 515, 70, 24), channel("lp4_btn"), text("LOOP 4"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(248, 542, 70, 13), text("IDLE +1x"), fontSize(9), align("centre"), fontColour(155, 114, 158), channel("lp4_state")
combobox bounds(248, 558, 70, 22), channel("lp4_speed"), items("-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"), value(7), colour(40, 10, 44), fontColour(220, 186, 222), fontSize(9)
button bounds(326, 515, 70, 24), channel("lp5_btn"), text("LOOP 5"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(326, 542, 70, 13), text("IDLE +1x"), fontSize(9), align("centre"), fontColour(155, 114, 158), channel("lp5_state")
combobox bounds(326, 558, 70, 22), channel("lp5_speed"), items("-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"), value(7), colour(40, 10, 44), fontColour(220, 186, 222), fontSize(9)
label bounds(10, 609, 46, 12), text("DRUMS"), fontSize(10), align("left"), fontColour(155, 114, 158)
button bounds(60, 602, 30, 26), channel("drum_prev"), text("◀"), corners(3), fontSize(11), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(93, 602, 30, 26), channel("drum_next"), text("▶"), corners(3), fontSize(11), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(126, 602, 80, 26), channel("drum_play"), text("PLAY/STOP"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
button bounds(209, 602, 56, 26), channel("drum_clear"), text("CLEAR"), corners(3), fontSize(10), value(0), latched(0), colour:0(40, 10, 44), colour:1(202, 42, 235), fontColour:0(155, 114, 158), fontColour:1(255, 255, 255), outlineColour(113, 64, 117), outlineThickness(1)
label bounds(275, 609, 465, 13), text("-"), fontSize(11), align("left"), fontColour(220, 186, 222), channel("drum_lbl")
</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d --opcode-lib=/Users/emgor/Library/CsoundPlugins/csound-nam.so
</CsOptions>
<CsInstruments>

; ==========================================================================
; NEPTR VST — GENERATED FILE. Do not hand-edit; run vst/generate_vst.py.
; DSP = the same .orc files the hardware runs (absolute-path includes).
; NAM: same csound-nam.so the NAMDrop plugin loads, profiles from the repo.
; ==========================================================================

#define PRESET_BANK #0#
#define STEREO_IN #1#
#define KSMPS #64#
#define IR_DIR #/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/ir#
#define DRUMLOOP_DIR #/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/samples/drumloops#
#define VST_NAM ##
#define NAM_DIR #/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/nam/profiles/live#
#define STATE_PERSIST ##
#define STATE_VST ##
#define STATE_RESTORE_WIDGETS ##
#define AUDIO_DEBUG ##
#define STATE_DIR #/Users/emgor/Library/Application Support/NEPTR#

#ifdef CLI_CHECK
opcode cabbageSet, 0, kSS
kTrig, SChan, SProp xin
endop
opcode cabbageSetValue, 0, Skk
SChan, kVal, kTrig xin
endop
#endif

#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/globals.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/preamp.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/attack.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/h90.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/pitchfork.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/pitchshifter.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/gps.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/phaser.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/flanger.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/chorus.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/vibrato.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/leslie.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/overdrive.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/marshall.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/dualhigain.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/ratmz.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/bigmuff.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/bass_preamp.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/bass_dist.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/tremolo.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/forms.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/formant.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/bincrush.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/bincrusher.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/lossy.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/tape.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/ringmod.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/automoog.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/wah.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/spectraldelay.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/habit.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/mood.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/granular.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/delay.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/reverb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/stutter.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/looper.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/drumloops.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/looper_persist.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/chase_bliss_clock.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/movedelay.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/tapedelay.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/nebula.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/shimmerverb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/freezeverb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/grainverb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/onset_pick.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/onward.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/impulse_synth.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/resonator.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/ir_reverb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/octaver.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/envfilter.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/blooper.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/mood2.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/compressor.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/toneeq.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/chainfb.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/tracksynth.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/pllfuzz.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/feedbacker.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/diffusion.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/signal_chain.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/state_persist.orc"
#include "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/csound/effects/nam.orc"

instr 98  ; ============ VST CONTROL SURFACE (generated) ============
kInit init 1
kExprTgt init 0
kSel init 11  ; boot on H90 so controls show immediately
kSelChg = 0
kSelBoot init 1
if (kSelBoot == 1) then
  kSelBoot = 0
  kSelChg = 1
endif

; ---------- input level meter + audio debug ----------
aMtL, aMtR ins
kRmsL rms aMtL, 8
kRmsR rms aMtR, 8
kInRms = (kRmsL + kRmsR) * 0.5
kMetT metro 12
if (kMetT == 1) then
  cabbageSetValue "meter_in", limit:k(kInRms * 4, 0, 1), k(1)
endif
#ifdef AUDIO_DEBUG
  SDbg sprintf "%s/audio_debug.log", "$STATE_DIR"
  kDbgInit init 1
  if (kDbgInit == 1) then
    kDbgInit = 0
    prints "NEPTR-AUDIO sr=%d ksmps=%d nchnls_i=%d nchnls=%d\n", sr, ksmps, nchnls_i, nchnls
  endif
  ; running window peak, reset every log tick
  kPk init 0
  kAbs = abs(k(aMtL)) > abs(k(aMtR)) ? abs(k(aMtL)) : abs(k(aMtR))
  kPk = kAbs > kPk ? kAbs : kPk
  aMonL, aMonR monitor
  kOutL rms aMonL, 8
  kOutR rms aMonR, 8
  kDbgT metro 2
  if (kDbgT == 1) then
    fprintks SDbg, "sr=%d nch_i=%d nch_o=%d IN(L=%.6f R=%.6f pk=%.6f) OUT(L=%.6f R=%.6f)\n", sr, nchnls_i, nchnls, kRmsL, kRmsR, kPk, kOutL, kOutR
    kPk = 0
  endif
#endif

; ---------- stereo input switch ----------
kStIn chnget "stereo_in"
gkStereoIn = kStIn

; ---------- effect toggles + params + selection ----------
; -- VOLUME (toggle 1)
kOn_vol chnget "on_vol"
kClk_vol changed2 kOn_vol
if (kClk_vol == 1) then
  kSel = 0
  kSelChg = 1
endif
kPv_vol init 0
if (kOn_vol > 0.5 && kPv_vol < 0.5) then
  kExprTgt = 1
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: VOLUME\")"
endif
kPv_vol = kOn_vol
gkVOLpedalBypass = 1 - kOn_vol
kP_vol_level chnget "vol_level"
kC_vol_level changed2 kP_vol_level
if (kC_vol_level == 1 || kInit == 1) then
  gkVOLpedal = kP_vol_level
endif

; -- ATTACK (toggle 2)
kOn_atk chnget "on_atk"
kClk_atk changed2 kOn_atk
if (kClk_atk == 1) then
  kSel = 1
  kSelChg = 1
endif
kPv_atk init 0
if (kOn_atk > 0.5 && kPv_atk < 0.5) then
  kExprTgt = 2
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: ATTACK\")"
endif
kPv_atk = kOn_atk
gkATTK_bypass = 1 - kOn_atk
kP_atk_mix chnget "atk_mix"
kC_atk_mix changed2 kP_atk_mix
if (kC_atk_mix == 1 || kInit == 1) then
  gkATTK_mix = kP_atk_mix
endif
kP_atk_time chnget "atk_time"
kC_atk_time changed2 kP_atk_time
if (kC_atk_time == 1 || kInit == 1) then
  gkATTK_atkMs = kP_atk_time
endif
kP_atk_sens chnget "atk_sens"
kC_atk_sens changed2 kP_atk_sens
if (kC_atk_sens == 1 || kInit == 1) then
  gkATTK_sens = kP_atk_sens
endif
kP_atk_shape chnget "atk_shape"
kC_atk_shape changed2 kP_atk_shape
if (kC_atk_shape == 1 || kInit == 1) then
  gkATTK_shape = kP_atk_shape
endif
kP_atk_range chnget "atk_range"
kC_atk_range changed2 kP_atk_range
if (kC_atk_range == 1 || kInit == 1) then
  gkATTK_range = kP_atk_range
endif
kP_atk_retrig chnget "atk_retrig"
kC_atk_retrig changed2 kP_atk_retrig
if (kC_atk_retrig == 1 || kInit == 1) then
  gkATTK_retrigMs = kP_atk_retrig
endif

; -- LESLIE (toggle 10)
kOn_les chnget "on_les"
kClk_les changed2 kOn_les
if (kClk_les == 1) then
  kSel = 2
  kSelChg = 1
endif
kPv_les init 0
if (kOn_les > 0.5 && kPv_les < 0.5) then
  kExprTgt = 10
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: LESLIE\")"
endif
kPv_les = kOn_les
gkLeslieBypass = 1 - kOn_les
kP_les_speed chnget "les_speed"
kC_les_speed changed2 kP_les_speed
if (kC_les_speed == 1 || kInit == 1) then
  gkLeslieSpeed = kP_les_speed
endif
kP_les_depth chnget "les_depth"
kC_les_depth changed2 kP_les_depth
if (kC_les_depth == 1 || kInit == 1) then
  gkLeslieDepth = kP_les_depth
endif

; -- RINGMOD (toggle 11)
kOn_rm chnget "on_rm"
kClk_rm changed2 kOn_rm
if (kClk_rm == 1) then
  kSel = 3
  kSelChg = 1
endif
kPv_rm init 0
if (kOn_rm > 0.5 && kPv_rm < 0.5) then
  kExprTgt = 11
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: RINGMOD\")"
endif
kPv_rm = kOn_rm
gkBypassRM = 1 - kOn_rm
kP_rm_depth chnget "rm_depth"
kC_rm_depth changed2 kP_rm_depth
if (kC_rm_depth == 1 || kInit == 1) then
  gkARModDepth = kP_rm_depth
endif
kP_rm_freq chnget "rm_freq"
kC_rm_freq changed2 kP_rm_freq
if (kC_rm_freq == 1 || kInit == 1) then
  gkARModFreq = kP_rm_freq
endif

; -- TAPE (toggle 21)
kOn_tape chnget "on_tape"
kClk_tape changed2 kOn_tape
if (kClk_tape == 1) then
  kSel = 4
  kSelChg = 1
endif
kPv_tape init 0
if (kOn_tape > 0.5 && kPv_tape < 0.5) then
  kExprTgt = 21
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: TAPE\")"
endif
kPv_tape = kOn_tape
gkTapeBypass = 1 - kOn_tape
kP_tape_amt chnget "tape_amt"
kC_tape_amt changed2 kP_tape_amt
if (kC_tape_amt == 1 || kInit == 1) then
  gkTapeAmount = kP_tape_amt
endif
kP_tape_fail chnget "tape_fail"
kC_tape_fail changed2 kP_tape_fail
if (kC_tape_fail == 1 || kInit == 1) then
  gkTapeFail = kP_tape_fail
endif
kP_tape_model chnget "tape_model"
kC_tape_model changed2 kP_tape_model
if (kC_tape_model == 1 || kInit == 1) then
  gkTapeModel = kP_tape_model
endif

; -- FORMS (toggle 28)
kOn_fms chnget "on_fms"
kClk_fms changed2 kOn_fms
if (kClk_fms == 1) then
  kSel = 5
  kSelChg = 1
endif
kPv_fms init 0
if (kOn_fms > 0.5 && kPv_fms < 0.5) then
  kExprTgt = 28
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: FORMS\")"
endif
kPv_fms = kOn_fms
gkFormsBypass = 1 - kOn_fms
kP_fms_mix chnget "fms_mix"
kC_fms_mix changed2 kP_fms_mix
if (kC_fms_mix == 1 || kInit == 1) then
  gkFormsMix = kP_fms_mix
endif
kP_fms_shift chnget "fms_shift"
kC_fms_shift changed2 kP_fms_shift
if (kC_fms_shift == 1 || kInit == 1) then
  gkFormsShift = kP_fms_shift
endif

; -- FORMANT (toggle 5)
kOn_fmt chnget "on_fmt"
kClk_fmt changed2 kOn_fmt
if (kClk_fmt == 1) then
  kSel = 6
  kSelChg = 1
endif
kPv_fmt init 0
if (kOn_fmt > 0.5 && kPv_fmt < 0.5) then
  kExprTgt = 5
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: FORMANT\")"
endif
kPv_fmt = kOn_fmt
gkFormantBypass = 1 - kOn_fmt
kP_fmt_mix chnget "fmt_mix"
kC_fmt_mix changed2 kP_fmt_mix
if (kC_fmt_mix == 1 || kInit == 1) then
  gkFormantMix = kP_fmt_mix
endif
kP_fmt_shift chnget "fmt_shift"
kC_fmt_shift changed2 kP_fmt_shift
if (kC_fmt_shift == 1 || kInit == 1) then
  gkFormantShift = kP_fmt_shift
endif

; -- SPECDLY (toggle 16)
kOn_sdl chnget "on_sdl"
kClk_sdl changed2 kOn_sdl
if (kClk_sdl == 1) then
  kSel = 7
  kSelChg = 1
endif
kPv_sdl init 0
if (kOn_sdl > 0.5 && kPv_sdl < 0.5) then
endif
kPv_sdl = kOn_sdl
gkSpecDelBypass = 1 - kOn_sdl
kP_sdl_mix chnget "sdl_mix"
kC_sdl_mix changed2 kP_sdl_mix
if (kC_sdl_mix == 1 || kInit == 1) then
  gkSpecDelMix = kP_sdl_mix
endif
kP_sdl_maxdel chnget "sdl_maxdel"
kC_sdl_maxdel changed2 kP_sdl_maxdel
if (kC_sdl_maxdel == 1 || kInit == 1) then
  gkSpecDelMaxDel = kP_sdl_maxdel
endif
kP_sdl_fback chnget "sdl_fback"
kC_sdl_fback changed2 kP_sdl_fback
if (kC_sdl_fback == 1 || kInit == 1) then
  gkSpecDelFB = kP_sdl_fback
endif

; -- BIN REDU (toggle 38)
kOn_bin chnget "on_bin"
kClk_bin changed2 kOn_bin
if (kClk_bin == 1) then
  kSel = 8
  kSelChg = 1
endif
kPv_bin init 0
if (kOn_bin > 0.5 && kPv_bin < 0.5) then
  kExprTgt = 38
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BIN REDU\")"
endif
kPv_bin = kOn_bin
gkBinBypass = 1 - kOn_bin
kP_bin_pedal chnget "bin_pedal"
kC_bin_pedal changed2 kP_bin_pedal
if (kC_bin_pedal == 1 || kInit == 1) then
  gkBinPedal = kP_bin_pedal
endif

; -- BCRUSHER (toggle 44)
kOn_bc chnget "on_bc"
kClk_bc changed2 kOn_bc
if (kClk_bc == 1) then
  kSel = 9
  kSelChg = 1
endif
kPv_bc init 0
if (kOn_bc > 0.5 && kPv_bc < 0.5) then
  kExprTgt = 44
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BCRUSHER\")"
endif
kPv_bc = kOn_bc
gkBcBypass = 1 - kOn_bc
kP_bc_pedal chnget "bc_pedal"
kC_bc_pedal changed2 kP_bc_pedal
if (kC_bc_pedal == 1 || kInit == 1) then
  gkBcPedal = kP_bc_pedal
endif

; -- BINSCATTER (toggle 67)
kOn_bsc chnget "on_bsc"
kClk_bsc changed2 kOn_bsc
if (kClk_bsc == 1) then
  kSel = 10
  kSelChg = 1
endif
kPv_bsc init 0
if (kOn_bsc > 0.5 && kPv_bsc < 0.5) then
endif
kPv_bsc = kOn_bsc
gkBscBypass = 1 - kOn_bsc
kP_bsc_mix chnget "bsc_mix"
kC_bsc_mix changed2 kP_bsc_mix
if (kC_bsc_mix == 1 || kInit == 1) then
  gkBscMix = kP_bsc_mix
endif
kP_bsc_scatter chnget "bsc_scatter"
kC_bsc_scatter changed2 kP_bsc_scatter
if (kC_bsc_scatter == 1 || kInit == 1) then
  gkBscScatter = kP_bsc_scatter
endif
kP_bsc_fback chnget "bsc_fback"
kC_bsc_fback changed2 kP_bsc_fback
if (kC_bsc_fback == 1 || kInit == 1) then
  gkBscFB = kP_bsc_fback
endif
kP_bsc_window chnget "bsc_window"
kC_bsc_window changed2 kP_bsc_window
if (kC_bsc_window == 1 || kInit == 1) then
  gkBscMaxDel = kP_bsc_window
endif

; -- H90 (toggle 26)
kOn_h90 chnget "on_h90"
kClk_h90 changed2 kOn_h90
if (kClk_h90 == 1) then
  kSel = 11
  kSelChg = 1
endif
kPv_h90 init 0
if (kOn_h90 > 0.5 && kPv_h90 < 0.5) then
  kExprTgt = 26
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: H90\")"
endif
kPv_h90 = kOn_h90
gkBypassH90 = 1 - kOn_h90
kP_h90_mix chnget "h90_mix"
kC_h90_mix changed2 kP_h90_mix
if (kC_h90_mix == 1 || kInit == 1) then
  gkH90mix = kP_h90_mix
endif
kP_h90_semi chnget "h90_semi"
kC_h90_semi changed2 kP_h90_semi
if (kC_h90_semi == 1 || kInit == 1) then
  gkSemiH90 = kP_h90_semi
endif

; -- OLD PS (toggle 3)
kOn_ps chnget "on_ps"
kClk_ps changed2 kOn_ps
if (kClk_ps == 1) then
  kSel = 12
  kSelChg = 1
endif
kPv_ps init 0
if (kOn_ps > 0.5 && kPv_ps < 0.5) then
  kExprTgt = 3
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: OLD PS\")"
endif
kPv_ps = kOn_ps
gkBypassPS = 1 - kOn_ps
kP_ps_mix chnget "ps_mix"
kC_ps_mix changed2 kP_ps_mix
if (kC_ps_mix == 1 || kInit == 1) then
  gkDryPctPS = kP_ps_mix
endif
kP_ps_semi chnget "ps_semi"
kC_ps_semi changed2 kP_ps_semi
if (kC_ps_semi == 1 || kInit == 1) then
  gkSemi = kP_ps_semi
endif

; -- GPS (toggle 4)
kOn_gps chnget "on_gps"
kClk_gps changed2 kOn_gps
if (kClk_gps == 1) then
  kSel = 13
  kSelChg = 1
endif
kPv_gps init 0
if (kOn_gps > 0.5 && kPv_gps < 0.5) then
  kExprTgt = 4
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: GPS\")"
endif
kPv_gps = kOn_gps
gkBypassGPS = 1 - kOn_gps
kP_gps_mix chnget "gps_mix"
kC_gps_mix changed2 kP_gps_mix
if (kC_gps_mix == 1 || kInit == 1) then
  gkGPSmix = kP_gps_mix
endif
kP_gps_semi chnget "gps_semi"
kC_gps_semi changed2 kP_gps_semi
if (kC_gps_semi == 1 || kInit == 1) then
  gkGPSsemis = kP_gps_semi
endif
kP_gps_fback chnget "gps_fback"
kC_gps_fback changed2 kP_gps_fback
if (kC_gps_fback == 1 || kInit == 1) then
  gkGPSfback = kP_gps_fback
endif
kP_gps_dly chnget "gps_dly"
kC_gps_dly changed2 kP_gps_dly
if (kC_gps_dly == 1 || kInit == 1) then
  gkGPSdly = kP_gps_dly
endif

; -- P-FORK (toggle 27)
kOn_pf chnget "on_pf"
kClk_pf changed2 kOn_pf
if (kClk_pf == 1) then
  kSel = 14
  kSelChg = 1
endif
kPv_pf init 0
if (kOn_pf > 0.5 && kPv_pf < 0.5) then
  kExprTgt = 27
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: P-FORK\")"
endif
kPv_pf = kOn_pf
gkBypassPF = 1 - kOn_pf
kP_pf_mix chnget "pf_mix"
kC_pf_mix changed2 kP_pf_mix
if (kC_pf_mix == 1 || kInit == 1) then
  gkPFmix = kP_pf_mix
endif
kP_pf_semi chnget "pf_semi"
kC_pf_semi changed2 kP_pf_semi
if (kC_pf_semi == 1 || kInit == 1) then
  gkSemiPF = kP_pf_semi
endif

; -- LOSSY (toggle 46)
kOn_lsy chnget "on_lsy"
kClk_lsy changed2 kOn_lsy
if (kClk_lsy == 1) then
  kSel = 15
  kSelChg = 1
endif
kPv_lsy init 0
if (kOn_lsy > 0.5 && kPv_lsy < 0.5) then
endif
kPv_lsy = kOn_lsy
gkLsyBypass = 1 - kOn_lsy
kP_lsy_mix chnget "lsy_mix"
kC_lsy_mix changed2 kP_lsy_mix
if (kC_lsy_mix == 1 || kInit == 1) then
  gkLSY_mix = kP_lsy_mix
endif
kP_lsy_loss chnget "lsy_loss"
kC_lsy_loss changed2 kP_lsy_loss
if (kC_lsy_loss == 1 || kInit == 1) then
  gkLSY_loss = kP_lsy_loss
endif
kP_lsy_bw chnget "lsy_bw"
kC_lsy_bw changed2 kP_lsy_bw
if (kC_lsy_bw == 1 || kInit == 1) then
  gkLSY_bw = kP_lsy_bw
endif
kP_lsy_pkt chnget "lsy_pkt"
kC_lsy_pkt changed2 kP_lsy_pkt
if (kC_lsy_pkt == 1 || kInit == 1) then
  gkLSY_pkt = kP_lsy_pkt
endif

; -- RESONATR (toggle 47)
kOn_res chnget "on_res"
kClk_res changed2 kOn_res
if (kClk_res == 1) then
  kSel = 16
  kSelChg = 1
endif
kPv_res init 0
if (kOn_res > 0.5 && kPv_res < 0.5) then
endif
kPv_res = kOn_res
gkRES_bypass = 1 - kOn_res
kP_res_mix chnget "res_mix"
kC_res_mix changed2 kP_res_mix
if (kC_res_mix == 1 || kInit == 1) then
  gkRES_mix = kP_res_mix
endif
kP_res_onset chnget "res_onset"
kC_res_onset changed2 kP_res_onset
if (kC_res_onset == 1 || kInit == 1) then
  gkRES_onset = kP_res_onset
endif
kP_res_fb chnget "res_fb"
kC_res_fb changed2 kP_res_fb
if (kC_res_fb == 1 || kInit == 1) then
  gkRES_fb = kP_res_fb
endif
kP_res_oct chnget "res_oct"
kC_res_oct changed2 kP_res_oct
if (kC_res_oct == 1 || kInit == 1) then
  gkRES_oct = kP_res_oct
endif
kP_res_frz chnget "res_frz"
kC_res_frz changed2 kP_res_frz
if (kC_res_frz == 1 || kInit == 1) then
  gkRES_frz = (kP_res_frz - 1) * 1
endif

; -- WAH (toggle 22)
kOn_wah chnget "on_wah"
kClk_wah changed2 kOn_wah
if (kClk_wah == 1) then
  kSel = 17
  kSelChg = 1
endif
kPv_wah init 0
if (kOn_wah > 0.5 && kPv_wah < 0.5) then
  kExprTgt = 22
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: WAH\")"
endif
kPv_wah = kOn_wah
gkWAH_bypass = 1 - kOn_wah
kP_wah_freq chnget "wah_freq"
kC_wah_freq changed2 kP_wah_freq
if (kC_wah_freq == 1 || kInit == 1) then
  gkWAH_freq = kP_wah_freq
endif
kP_wah_res chnget "wah_res"
kC_wah_res changed2 kP_wah_res
if (kC_wah_res == 1 || kInit == 1) then
  gkWAH_res = kP_wah_res
endif

; -- AUTOMOOG (toggle 14)
kOn_mog chnget "on_mog"
kClk_mog changed2 kOn_mog
if (kClk_mog == 1) then
  kSel = 18
  kSelChg = 1
endif
kPv_mog init 0
if (kOn_mog > 0.5 && kPv_mog < 0.5) then
endif
kPv_mog = kOn_mog
gkAUTOMOOG_bypass = 1 - kOn_mog
kP_mog_sens chnget "mog_sens"
kC_mog_sens changed2 kP_mog_sens
if (kC_mog_sens == 1 || kInit == 1) then
  gkAUTOMOOG_sens = kP_mog_sens
endif
kP_mog_res chnget "mog_res"
kC_mog_res changed2 kP_mog_res
if (kC_mog_res == 1 || kInit == 1) then
  gkAUTOMOOG_res = kP_mog_res
endif
kP_mog_type chnget "mog_type"
kC_mog_type changed2 kP_mog_type
if (kC_mog_type == 1 || kInit == 1) then
  gkAUTOMOOG_type = (kP_mog_type - 1) * 1 + 1
endif
kP_mog_level chnget "mog_level"
kC_mog_level changed2 kP_mog_level
if (kC_mog_level == 1 || kInit == 1) then
  gkAUTOMOOG_level = kP_mog_level
endif

; -- VIBRATO (toggle 6)
kOn_vib chnget "on_vib"
kClk_vib changed2 kOn_vib
if (kClk_vib == 1) then
  kSel = 19
  kSelChg = 1
endif
kPv_vib init 0
if (kOn_vib > 0.5 && kPv_vib < 0.5) then
endif
kPv_vib = kOn_vib
gkVIB_bypass = 1 - kOn_vib
kP_vib_depth chnget "vib_depth"
kC_vib_depth changed2 kP_vib_depth
if (kC_vib_depth == 1 || kInit == 1) then
  gkVIB_depth = kP_vib_depth * 0.005
endif
kP_vib_rate chnget "vib_rate"
kC_vib_rate changed2 kP_vib_rate
if (kC_vib_rate == 1 || kInit == 1) then
  gkVIB_freq = kP_vib_rate
endif
kP_vib_fback chnget "vib_fback"
kC_vib_fback changed2 kP_vib_fback
if (kC_vib_fback == 1 || kInit == 1) then
  gkVIB_fback = kP_vib_fback
endif

; -- CHORUS (toggle 7)
kOn_cho chnget "on_cho"
kClk_cho changed2 kOn_cho
if (kClk_cho == 1) then
  kSel = 20
  kSelChg = 1
endif
kPv_cho init 0
if (kOn_cho > 0.5 && kPv_cho < 0.5) then
endif
kPv_cho = kOn_cho
gkBypassChorus = 1 - kOn_cho
kP_cho_mix chnget "cho_mix"
kC_cho_mix changed2 kP_cho_mix
if (kC_cho_mix == 1 || kInit == 1) then
  gkChorusMix = kP_cho_mix
endif
kP_cho_rate chnget "cho_rate"
kC_cho_rate changed2 kP_cho_rate
if (kC_cho_rate == 1 || kInit == 1) then
  gkChorusRate = kP_cho_rate
endif
kP_cho_depth chnget "cho_depth"
kC_cho_depth changed2 kP_cho_depth
if (kC_cho_depth == 1 || kInit == 1) then
  gkChorusDepth = kP_cho_depth
endif

; -- PHASER (toggle 8)
kOn_pha chnget "on_pha"
kClk_pha changed2 kOn_pha
if (kClk_pha == 1) then
  kSel = 21
  kSelChg = 1
endif
kPv_pha init 0
if (kOn_pha > 0.5 && kPv_pha < 0.5) then
  kExprTgt = 8
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: PHASER\")"
endif
kPv_pha = kOn_pha
gkPHA_bypass = 1 - kOn_pha
kP_pha_depth chnget "pha_depth"
kC_pha_depth changed2 kP_pha_depth
if (kC_pha_depth == 1 || kInit == 1) then
  gkPHA_depth = kP_pha_depth
endif
kP_pha_rate chnget "pha_rate"
kC_pha_rate changed2 kP_pha_rate
if (kC_pha_rate == 1 || kInit == 1) then
  gkPHA_rate = kP_pha_rate
endif
kP_pha_fback chnget "pha_fback"
kC_pha_fback changed2 kP_pha_fback
if (kC_pha_fback == 1 || kInit == 1) then
  gkPHA_fback = kP_pha_fback
endif
kP_pha_stages chnget "pha_stages"
kC_pha_stages changed2 kP_pha_stages
if (kC_pha_stages == 1 || kInit == 1) then
  gkPHA_stages = kP_pha_stages
endif

; -- TREMOLO (toggle 43)
kOn_trm chnget "on_trm"
kClk_trm changed2 kOn_trm
if (kClk_trm == 1) then
  kSel = 22
  kSelChg = 1
endif
kPv_trm init 0
if (kOn_trm > 0.5 && kPv_trm < 0.5) then
endif
kPv_trm = kOn_trm
gkTremBypass = 1 - kOn_trm
kP_trm_depth chnget "trm_depth"
kC_trm_depth changed2 kP_trm_depth
if (kC_trm_depth == 1 || kInit == 1) then
  gkTremDepth = kP_trm_depth
endif
kP_trm_mode chnget "trm_mode"
kC_trm_mode changed2 kP_trm_mode
if (kC_trm_mode == 1 || kInit == 1) then
  gkTremMode = (kP_trm_mode - 1) * 1
endif
kP_trm_rate chnget "trm_rate"
kC_trm_rate changed2 kP_trm_rate
if (kC_trm_rate == 1 || kInit == 1) then
  gkTremRate = kP_trm_rate
endif

; -- FLANGER (toggle 9)
kOn_fla chnget "on_fla"
kClk_fla changed2 kOn_fla
if (kClk_fla == 1) then
  kSel = 23
  kSelChg = 1
endif
kPv_fla init 0
if (kOn_fla > 0.5 && kPv_fla < 0.5) then
endif
kPv_fla = kOn_fla
gkFLA_bypass = 1 - kOn_fla
kP_fla_depth chnget "fla_depth"
kC_fla_depth changed2 kP_fla_depth
if (kC_fla_depth == 1 || kInit == 1) then
  gkFLA_depth = kP_fla_depth * 0.01
endif
kP_fla_rate chnget "fla_rate"
kC_fla_rate changed2 kP_fla_rate
if (kC_fla_rate == 1 || kInit == 1) then
  gkFLA_rate = kP_fla_rate
endif
kP_fla_fback chnget "fla_fback"
kC_fla_fback changed2 kP_fla_fback
if (kC_fla_fback == 1 || kInit == 1) then
  gkFLA_fback = kP_fla_fback
endif

; -- TUBESCREAM (toggle 31)
kOn_od chnget "on_od"
kClk_od changed2 kOn_od
if (kClk_od == 1) then
  kSel = 24
  kSelChg = 1
endif
kPv_od init 0
if (kOn_od > 0.5 && kPv_od < 0.5) then
  kExprTgt = 31
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: TUBESCREAM\")"
endif
kPv_od = kOn_od
gkOD_bypass = 1 - kOn_od
kP_od_drive chnget "od_drive"
kC_od_drive changed2 kP_od_drive
if (kC_od_drive == 1 || kInit == 1) then
  gkOD_drive = kP_od_drive
endif
kP_od_tone chnget "od_tone"
kC_od_tone changed2 kP_od_tone
if (kC_od_tone == 1 || kInit == 1) then
  gkOD_tone = kP_od_tone
endif
kP_od_morph chnget "od_morph"
kC_od_morph changed2 kP_od_morph
if (kC_od_morph == 1 || kInit == 1) then
  gkOD_wc1 = kP_od_morph
endif
kP_od_blend chnget "od_blend"
kC_od_blend changed2 kP_od_blend
if (kC_od_blend == 1 || kInit == 1) then
  gkOD_wc2 = kP_od_blend
endif

; -- MARSHALL (toggle 32)
kOn_mar chnget "on_mar"
kClk_mar changed2 kOn_mar
if (kClk_mar == 1) then
  kSel = 25
  kSelChg = 1
endif
kPv_mar init 0
if (kOn_mar > 0.5 && kPv_mar < 0.5) then
  kExprTgt = 32
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: MARSHALL\")"
endif
kPv_mar = kOn_mar
gkMAR_bypass = 1 - kOn_mar
kP_mar_drive chnget "mar_drive"
kC_mar_drive changed2 kP_mar_drive
if (kC_mar_drive == 1 || kInit == 1) then
  gkMAR_drive = kP_mar_drive
endif
kP_mar_tone chnget "mar_tone"
kC_mar_tone changed2 kP_mar_tone
if (kC_mar_tone == 1 || kInit == 1) then
  gkMAR_tone = kP_mar_tone
endif
kP_mar_pres chnget "mar_pres"
kC_mar_pres changed2 kP_mar_pres
if (kC_mar_pres == 1 || kInit == 1) then
  gkMAR_wc1 = kP_mar_pres
endif
kP_mar_tight chnget "mar_tight"
kC_mar_tight changed2 kP_mar_tight
if (kC_mar_tight == 1 || kInit == 1) then
  gkMAR_wc2 = kP_mar_tight
endif

; -- RAT-MZ (toggle 34)
kOn_rmz chnget "on_rmz"
kClk_rmz changed2 kOn_rmz
if (kClk_rmz == 1) then
  kSel = 26
  kSelChg = 1
endif
kPv_rmz init 0
if (kOn_rmz > 0.5 && kPv_rmz < 0.5) then
  kExprTgt = 34
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: RAT-MZ\")"
endif
kPv_rmz = kOn_rmz
gkRMZ_bypass = 1 - kOn_rmz
kP_rmz_drive chnget "rmz_drive"
kC_rmz_drive changed2 kP_rmz_drive
if (kC_rmz_drive == 1 || kInit == 1) then
  gkRMZ_drive = kP_rmz_drive
endif
kP_rmz_tone chnget "rmz_tone"
kC_rmz_tone changed2 kP_rmz_tone
if (kC_rmz_tone == 1 || kInit == 1) then
  gkRMZ_tone = kP_rmz_tone
endif
kP_rmz_midf chnget "rmz_midf"
kC_rmz_midf changed2 kP_rmz_midf
if (kC_rmz_midf == 1 || kInit == 1) then
  gkRMZ_wc1 = kP_rmz_midf
endif
kP_rmz_midg chnget "rmz_midg"
kC_rmz_midg changed2 kP_rmz_midg
if (kC_rmz_midg == 1 || kInit == 1) then
  gkRMZ_wc2 = kP_rmz_midg
endif

; -- DS-1K (toggle 33)
kOn_dhg chnget "on_dhg"
kClk_dhg changed2 kOn_dhg
if (kClk_dhg == 1) then
  kSel = 27
  kSelChg = 1
endif
kPv_dhg init 0
if (kOn_dhg > 0.5 && kPv_dhg < 0.5) then
  kExprTgt = 33
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: DS-1K\")"
endif
kPv_dhg = kOn_dhg
gkDHG_bypass = 1 - kOn_dhg
kP_dhg_drive chnget "dhg_drive"
kC_dhg_drive changed2 kP_dhg_drive
if (kC_dhg_drive == 1 || kInit == 1) then
  gkDHG_drive = kP_dhg_drive
endif
kP_dhg_tone chnget "dhg_tone"
kC_dhg_tone changed2 kP_dhg_tone
if (kC_dhg_tone == 1 || kInit == 1) then
  gkDHG_tone = kP_dhg_tone
endif
kP_dhg_blend chnget "dhg_blend"
kC_dhg_blend changed2 kP_dhg_blend
if (kC_dhg_blend == 1 || kInit == 1) then
  gkDHG_wc1 = kP_dhg_blend
endif
kP_dhg_tight chnget "dhg_tight"
kC_dhg_tight changed2 kP_dhg_tight
if (kC_dhg_tight == 1 || kInit == 1) then
  gkDHG_wc2 = kP_dhg_tight
endif

; -- BIGMUFF (toggle 35)
kOn_muf chnget "on_muf"
kClk_muf changed2 kOn_muf
if (kClk_muf == 1) then
  kSel = 28
  kSelChg = 1
endif
kPv_muf init 0
if (kOn_muf > 0.5 && kPv_muf < 0.5) then
  kExprTgt = 35
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BIGMUFF\")"
endif
kPv_muf = kOn_muf
gkMUF_bypass = 1 - kOn_muf
kP_muf_drive chnget "muf_drive"
kC_muf_drive changed2 kP_muf_drive
if (kC_muf_drive == 1 || kInit == 1) then
  gkMUF_drive = kP_muf_drive
endif
kP_muf_tone chnget "muf_tone"
kC_muf_tone changed2 kP_muf_tone
if (kC_muf_tone == 1 || kInit == 1) then
  gkMUF_tone = kP_muf_tone
endif
kP_muf_mids chnget "muf_mids"
kC_muf_mids changed2 kP_muf_mids
if (kC_muf_mids == 1 || kInit == 1) then
  gkMUF_wc1 = kP_muf_mids
endif
kP_muf_voice chnget "muf_voice"
kC_muf_voice changed2 kP_muf_voice
if (kC_muf_voice == 1 || kInit == 1) then
  gkMUF_wc2 = kP_muf_voice
endif

; -- MAIN PRE (toggle 39)
kOn_pre chnget "on_pre"
kClk_pre changed2 kOn_pre
if (kClk_pre == 1) then
  kSel = 29
  kSelChg = 1
endif
kPv_pre init 0
if (kOn_pre > 0.5 && kPv_pre < 0.5) then
endif
kPv_pre = kOn_pre
gkPreamp_bypass = 1 - kOn_pre

; -- BASS PRE L (toggle 40)
kOn_bpl chnget "on_bpl"
kClk_bpl changed2 kOn_bpl
if (kClk_bpl == 1) then
  kSel = 30
  kSelChg = 1
endif
kPv_bpl init 0
if (kOn_bpl > 0.5 && kPv_bpl < 0.5) then
  cabbageSetValue "on_bph", k(0), k(1)
endif
kPv_bpl = kOn_bpl
gkBPL_bypass = 1 - kOn_bpl
kP_bpl_drive chnget "bpl_drive"
kC_bpl_drive changed2 kP_bpl_drive
if (kC_bpl_drive == 1 || kInit == 1) then
  gkBPL_drive = kP_bpl_drive
endif

; -- BASS PRE H (toggle 41)
kOn_bph chnget "on_bph"
kClk_bph changed2 kOn_bph
if (kClk_bph == 1) then
  kSel = 31
  kSelChg = 1
endif
kPv_bph init 0
if (kOn_bph > 0.5 && kPv_bph < 0.5) then
  cabbageSetValue "on_bpl", k(0), k(1)
endif
kPv_bph = kOn_bph
gkBPH_bypass = 1 - kOn_bph
kP_bph_drive chnget "bph_drive"
kC_bph_drive changed2 kP_bph_drive
if (kC_bph_drive == 1 || kInit == 1) then
  gkBPH_drive = kP_bph_drive
endif

; -- BASS DIST (toggle 42)
kOn_bds chnget "on_bds"
kClk_bds changed2 kOn_bds
if (kClk_bds == 1) then
  kSel = 32
  kSelChg = 1
endif
kPv_bds init 0
if (kOn_bds > 0.5 && kPv_bds < 0.5) then
  kExprTgt = 42
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BASS DIST\")"
endif
kPv_bds = kOn_bds
gkBDS_bypass = 1 - kOn_bds
kP_bds_drive chnget "bds_drive"
kC_bds_drive changed2 kP_bds_drive
if (kC_bds_drive == 1 || kInit == 1) then
  gkBDS_drive = kP_bds_drive
endif
kP_bds_blend chnget "bds_blend"
kC_bds_blend changed2 kP_bds_blend
if (kC_bds_blend == 1 || kInit == 1) then
  gkBDS_blend = kP_bds_blend
endif
kP_bds_tone chnget "bds_tone"
kC_bds_tone changed2 kP_bds_tone
if (kC_bds_tone == 1 || kInit == 1) then
  gkBDS_tone = kP_bds_tone
endif

; -- COMPRESS (toggle 57)
kOn_cmp chnget "on_cmp"
kClk_cmp changed2 kOn_cmp
if (kClk_cmp == 1) then
  kSel = 33
  kSelChg = 1
endif
kPv_cmp init 0
if (kOn_cmp > 0.5 && kPv_cmp < 0.5) then
endif
kPv_cmp = kOn_cmp
gkCMP_bypass = 1 - kOn_cmp
kP_cmp_amt chnget "cmp_amt"
kC_cmp_amt changed2 kP_cmp_amt
if (kC_cmp_amt == 1 || kInit == 1) then
  gkCMP_amt = kP_cmp_amt
endif
kP_cmp_speed chnget "cmp_speed"
kC_cmp_speed changed2 kP_cmp_speed
if (kC_cmp_speed == 1 || kInit == 1) then
  gkCMP_speed = kP_cmp_speed
endif
kP_cmp_makeup chnget "cmp_makeup"
kC_cmp_makeup changed2 kP_cmp_makeup
if (kC_cmp_makeup == 1 || kInit == 1) then
  gkCMP_makeup = kP_cmp_makeup
endif
kP_cmp_blend chnget "cmp_blend"
kC_cmp_blend changed2 kP_cmp_blend
if (kC_cmp_blend == 1 || kInit == 1) then
  gkCMP_blend = kP_cmp_blend
endif
kP_cmp_gate chnget "cmp_gate"
kC_cmp_gate changed2 kP_cmp_gate
if (kC_cmp_gate == 1 || kInit == 1) then
  gkCMP_gate = kP_cmp_gate
endif

; -- DELAY (toggle 15)
kOn_dly chnget "on_dly"
kClk_dly changed2 kOn_dly
if (kClk_dly == 1) then
  kSel = 34
  kSelChg = 1
endif
kPv_dly init 0
if (kOn_dly > 0.5 && kPv_dly < 0.5) then
  kExprTgt = 15
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: DELAY\")"
endif
kPv_dly = kOn_dly
gkBypassDelay = 1 - kOn_dly
kP_dly_mix chnget "dly_mix"
kC_dly_mix changed2 kP_dly_mix
if (kC_dly_mix == 1 || kInit == 1) then
  gkDelMix = kP_dly_mix
endif
kP_dly_time chnget "dly_time"
kC_dly_time changed2 kP_dly_time
if (kC_dly_time == 1 || kInit == 1) then
  gkDelayMS = kP_dly_time * 1000
endif
kP_dly_fback chnget "dly_fback"
kC_dly_fback changed2 kP_dly_fback
if (kC_dly_fback == 1 || kInit == 1) then
  gkFeedback = kP_dly_fback
endif
kP_dly_semi chnget "dly_semi"
kC_dly_semi changed2 kP_dly_semi
if (kC_dly_semi == 1 || kInit == 1) then
  gkSemiDel = kP_dly_semi
endif
kP_dly_mode chnget "dly_mode"
kC_dly_mode changed2 kP_dly_mode
if (kC_dly_mode == 1 || kInit == 1) then
  gkDlyMode = (kP_dly_mode - 1) * 1
endif

; -- TAPE DELAY (toggle 68)
kOn_tdl chnget "on_tdl"
kClk_tdl changed2 kOn_tdl
if (kClk_tdl == 1) then
  kSel = 35
  kSelChg = 1
endif
kPv_tdl init 0
if (kOn_tdl > 0.5 && kPv_tdl < 0.5) then
endif
kPv_tdl = kOn_tdl
gkTdlBypass = 1 - kOn_tdl
kP_tdl_mix chnget "tdl_mix"
kC_tdl_mix changed2 kP_tdl_mix
if (kC_tdl_mix == 1 || kInit == 1) then
  gkTdlMix = kP_tdl_mix
endif
kP_tdl_time chnget "tdl_time"
kC_tdl_time changed2 kP_tdl_time
if (kC_tdl_time == 1 || kInit == 1) then
  gkTdlTimeMs = kP_tdl_time
endif
kP_tdl_fback chnget "tdl_fback"
kC_tdl_fback changed2 kP_tdl_fback
if (kC_tdl_fback == 1 || kInit == 1) then
  gkTdlFeedback = kP_tdl_fback
endif
kP_tdl_fctr chnget "tdl_fctr"
kC_tdl_fctr changed2 kP_tdl_fctr
if (kC_tdl_fctr == 1 || kInit == 1) then
  gkTdlFiltCtr = kP_tdl_fctr
endif
kP_tdl_fq chnget "tdl_fq"
kC_tdl_fq changed2 kP_tdl_fq
if (kC_tdl_fq == 1 || kInit == 1) then
  gkTdlFiltQ = kP_tdl_fq
endif
kP_tdl_head chnget "tdl_head"
kC_tdl_head changed2 kP_tdl_head
if (kC_tdl_head == 1 || kInit == 1) then
  gkTdlHeadMode = (kP_tdl_head - 1) * 1 + 1
endif

; -- POLY DLY (toggle 69)
kOn_mvd chnget "on_mvd"
kClk_mvd changed2 kOn_mvd
if (kClk_mvd == 1) then
  kSel = 36
  kSelChg = 1
endif
kPv_mvd init 0
if (kOn_mvd > 0.5 && kPv_mvd < 0.5) then
  kExprTgt = 69
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: POLY DLY\")"
endif
kPv_mvd = kOn_mvd
gkMvdBypass = 1 - kOn_mvd
kP_mvd_numl chnget "mvd_numl"
kC_mvd_numl changed2 kP_mvd_numl
if (kC_mvd_numl == 1 || kInit == 1) then
  gkMvdNumL = kP_mvd_numl
endif
kP_mvd_denl chnget "mvd_denl"
kC_mvd_denl changed2 kP_mvd_denl
if (kC_mvd_denl == 1 || kInit == 1) then
  gkMvdDenL = kP_mvd_denl
endif
kP_mvd_numr chnget "mvd_numr"
kC_mvd_numr changed2 kP_mvd_numr
if (kC_mvd_numr == 1 || kInit == 1) then
  gkMvdNumR = kP_mvd_numr
endif
kP_mvd_denr chnget "mvd_denr"
kC_mvd_denr changed2 kP_mvd_denr
if (kC_mvd_denr == 1 || kInit == 1) then
  gkMvdDenR = kP_mvd_denr
endif
kP_mvd_att chnget "mvd_att"
kC_mvd_att changed2 kP_mvd_att
if (kC_mvd_att == 1 || kInit == 1) then
  gkMvdAtt = kP_mvd_att
endif
kP_mvd_dec chnget "mvd_dec"
kC_mvd_dec changed2 kP_mvd_dec
if (kC_mvd_dec == 1 || kInit == 1) then
  gkMvdDec = kP_mvd_dec
endif
kP_mvd_reps chnget "mvd_reps"
kC_mvd_reps changed2 kP_mvd_reps
if (kC_mvd_reps == 1 || kInit == 1) then
  gkMvdReps = kP_mvd_reps
endif
kP_mvd_intv chnget "mvd_intv"
kC_mvd_intv changed2 kP_mvd_intv
if (kC_mvd_intv == 1 || kInit == 1) then
  gkMvdIntv = kP_mvd_intv
endif

; -- NEBULA (toggle 70)
kOn_neb chnget "on_neb"
kClk_neb changed2 kOn_neb
if (kClk_neb == 1) then
  kSel = 37
  kSelChg = 1
endif
kPv_neb init 0
if (kOn_neb > 0.5 && kPv_neb < 0.5) then
  kExprTgt = 70
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: NEBULA\")"
endif
kPv_neb = kOn_neb
gkNebBypass = 1 - kOn_neb
kP_neb_mix chnget "neb_mix"
kC_neb_mix changed2 kP_neb_mix
if (kC_neb_mix == 1 || kInit == 1) then
  gkNebMix = kP_neb_mix
endif
kP_neb_dens chnget "neb_dens"
kC_neb_dens changed2 kP_neb_dens
if (kC_neb_dens == 1 || kInit == 1) then
  gkNebDensity = kP_neb_dens
endif
kP_neb_size chnget "neb_size"
kC_neb_size changed2 kP_neb_size
if (kC_neb_size == 1 || kInit == 1) then
  gkNebSize = kP_neb_size
endif

; -- SHIMMER (toggle 71)
kOn_shm chnget "on_shm"
kClk_shm changed2 kOn_shm
if (kClk_shm == 1) then
  kSel = 38
  kSelChg = 1
endif
kPv_shm init 0
if (kOn_shm > 0.5 && kPv_shm < 0.5) then
endif
kPv_shm = kOn_shm
gkShmBypass = 1 - kOn_shm
kP_shm_mix chnget "shm_mix"
kC_shm_mix changed2 kP_shm_mix
if (kC_shm_mix == 1 || kInit == 1) then
  gkShmMix = kP_shm_mix
endif
kP_shm_intv chnget "shm_intv"
kC_shm_intv changed2 kP_shm_intv
if (kC_shm_intv == 1 || kInit == 1) then
  gkShmInterval = (kP_shm_intv - 1) * 1
endif
kP_shm_bloom chnget "shm_bloom"
kC_shm_bloom changed2 kP_shm_bloom
if (kC_shm_bloom == 1 || kInit == 1) then
  gkShmFbAmt = kP_shm_bloom
endif

; -- FREEZE (toggle 72)
kOn_frz chnget "on_frz"
kClk_frz changed2 kOn_frz
if (kClk_frz == 1) then
  kSel = 39
  kSelChg = 1
endif
kPv_frz init 0
if (kOn_frz > 0.5 && kPv_frz < 0.5) then
  kExprTgt = 72
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: FREEZE\")"
endif
kPv_frz = kOn_frz
gkFrzBypass = 1 - kOn_frz
kP_frz_bed chnget "frz_bed"
kC_frz_bed changed2 kP_frz_bed
if (kC_frz_bed == 1 || kInit == 1) then
  gkFrzBedLevel = kP_frz_bed
endif
kP_frz_damp chnget "frz_damp"
kC_frz_damp changed2 kP_frz_damp
if (kC_frz_damp == 1 || kInit == 1) then
  gkFrzDamp = kP_frz_damp
endif
kP_frz_listen chnget "frz_listen"
kC_frz_listen changed2 kP_frz_listen
if (kC_frz_listen == 1 || kInit == 1) then
  gkFrzListenGain = kP_frz_listen
endif

; -- GRAINVERB (toggle 73)
kOn_gvb chnget "on_gvb"
kClk_gvb changed2 kOn_gvb
if (kClk_gvb == 1) then
  kSel = 40
  kSelChg = 1
endif
kPv_gvb init 0
if (kOn_gvb > 0.5 && kPv_gvb < 0.5) then
endif
kPv_gvb = kOn_gvb
gkGvbBypass = 1 - kOn_gvb
kP_gvb_mix chnget "gvb_mix"
kC_gvb_mix changed2 kP_gvb_mix
if (kC_gvb_mix == 1 || kInit == 1) then
  gkGvbMix = kP_gvb_mix
endif
kP_gvb_spacing chnget "gvb_spacing"
kC_gvb_spacing changed2 kP_gvb_spacing
if (kC_gvb_spacing == 1 || kInit == 1) then
  gkGvbSpacing = kP_gvb_spacing
endif
kP_gvb_decay chnget "gvb_decay"
kC_gvb_decay changed2 kP_gvb_decay
if (kC_gvb_decay == 1 || kInit == 1) then
  gkGvbDecay = kP_gvb_decay
endif

; -- REVERB (toggle 19)
kOn_vrb chnget "on_vrb"
kClk_vrb changed2 kOn_vrb
if (kClk_vrb == 1) then
  kSel = 41
  kSelChg = 1
endif
kPv_vrb init 0
if (kOn_vrb > 0.5 && kPv_vrb < 0.5) then
endif
kPv_vrb = kOn_vrb
gkverbBypass = 1 - kOn_vrb
kP_vrb_mix chnget "vrb_mix"
kC_vrb_mix changed2 kP_vrb_mix
if (kC_vrb_mix == 1 || kInit == 1) then
  gkverbMix = kP_vrb_mix
endif
kP_vrb_room chnget "vrb_room"
kC_vrb_room changed2 kP_vrb_room
if (kC_vrb_room == 1 || kInit == 1) then
  gkReverbFB = kP_vrb_room
endif
kP_vrb_damp chnget "vrb_damp"
kC_vrb_damp changed2 kP_vrb_damp
if (kC_vrb_damp == 1 || kInit == 1) then
  gkReverbDamp = kP_vrb_damp
endif

; -- GRANULAR (toggle 18)
kOn_grn chnget "on_grn"
kClk_grn changed2 kOn_grn
if (kClk_grn == 1) then
  kSel = 42
  kSelChg = 1
endif
kPv_grn init 0
if (kOn_grn > 0.5 && kPv_grn < 0.5) then
endif
kPv_grn = kOn_grn
gkGranBypass = 1 - kOn_grn
kP_grn_mix chnget "grn_mix"
kC_grn_mix changed2 kP_grn_mix
if (kC_grn_mix == 1 || kInit == 1) then
  gkGranMix = kP_grn_mix
endif
kP_grn_dens chnget "grn_dens"
kC_grn_dens changed2 kP_grn_dens
if (kC_grn_dens == 1 || kInit == 1) then
  gkGranDens = kP_grn_dens
endif
kP_grn_size chnget "grn_size"
kC_grn_size changed2 kP_grn_size
if (kC_grn_size == 1 || kInit == 1) then
  gkGranSize = kP_grn_size
endif
kP_grn_pitch chnget "grn_pitch"
kC_grn_pitch changed2 kP_grn_pitch
if (kC_grn_pitch == 1 || kInit == 1) then
  gkGranPitch = kP_grn_pitch
endif

; -- MOOD (toggle 17)
kOn_mod chnget "on_mod"
kClk_mod changed2 kOn_mod
if (kClk_mod == 1) then
  kSel = 43
  kSelChg = 1
endif
kPv_mod init 0
if (kOn_mod > 0.5 && kPv_mod < 0.5) then
  kExprTgt = 17
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: MOOD\")"
endif
kPv_mod = kOn_mod
gkMOOD_Bypass = 1 - kOn_mod
kP_mod_mix chnget "mod_mix"
kC_mod_mix changed2 kP_mod_mix
if (kC_mod_mix == 1 || kInit == 1) then
  gkMOOD_Mix = kP_mod_mix
endif
kP_mod_clock chnget "mod_clock"
kC_mod_clock changed2 kP_mod_clock
if (kC_mod_clock == 1 || kInit == 1) then
  gkMOOD_Clock = kP_mod_clock
endif
kP_mod_dens chnget "mod_dens"
kC_mod_dens changed2 kP_mod_dens
if (kC_mod_dens == 1 || kInit == 1) then
  gkMOOD_Dens = kP_mod_dens
endif
kP_mod_dly chnget "mod_dly"
kC_mod_dly changed2 kP_mod_dly
if (kC_mod_dly == 1 || kInit == 1) then
  gkMOOD_Dly = kP_mod_dly
endif

; -- OCTAVER (toggle 54)
kOn_oct chnget "on_oct"
kClk_oct changed2 kOn_oct
if (kClk_oct == 1) then
  kSel = 44
  kSelChg = 1
endif
kPv_oct init 0
if (kOn_oct > 0.5 && kPv_oct < 0.5) then
endif
kPv_oct = kOn_oct
gkOCT_bypass = 1 - kOn_oct
kP_oct_sub chnget "oct_sub"
kC_oct_sub changed2 kP_oct_sub
if (kC_oct_sub == 1 || kInit == 1) then
  gkOCT_sub = kP_oct_sub
endif
kP_oct_tone chnget "oct_tone"
kC_oct_tone changed2 kP_oct_tone
if (kC_oct_tone == 1 || kInit == 1) then
  gkOCT_tone = kP_oct_tone
endif
kP_oct_char chnget "oct_char"
kC_oct_char changed2 kP_oct_char
if (kC_oct_char == 1 || kInit == 1) then
  gkOCT_char = kP_oct_char
endif

; -- ENV FILTER (toggle 55)
kOn_env chnget "on_env"
kClk_env changed2 kOn_env
if (kClk_env == 1) then
  kSel = 45
  kSelChg = 1
endif
kPv_env init 0
if (kOn_env > 0.5 && kPv_env < 0.5) then
endif
kPv_env = kOn_env
gkENV_bypass = 1 - kOn_env
kP_env_wet chnget "env_wet"
kC_env_wet changed2 kP_env_wet
if (kC_env_wet == 1 || kInit == 1) then
  gkENV_wet = kP_env_wet
endif
kP_env_sens chnget "env_sens"
kC_env_sens changed2 kP_env_sens
if (kC_env_sens == 1 || kInit == 1) then
  gkENV_sens = kP_env_sens
endif
kP_env_mode chnget "env_mode"
kC_env_mode changed2 kP_env_mode
if (kC_env_mode == 1 || kInit == 1) then
  gkENV_mode = kP_env_mode
endif
kP_env_att chnget "env_att"
kC_env_att changed2 kP_env_att
if (kC_env_att == 1 || kInit == 1) then
  gkENV_att = kP_env_att
endif
kP_env_dec chnget "env_dec"
kC_env_dec changed2 kP_env_dec
if (kC_env_dec == 1 || kInit == 1) then
  gkENV_dec = kP_env_dec
endif

; -- TRACKSYN (toggle 61)
kOn_tsy chnget "on_tsy"
kClk_tsy changed2 kOn_tsy
if (kClk_tsy == 1) then
  kSel = 46
  kSelChg = 1
endif
kPv_tsy init 0
if (kOn_tsy > 0.5 && kPv_tsy < 0.5) then
  kExprTgt = 61
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: TRACKSYN\")"
endif
kPv_tsy = kOn_tsy
gkTSY_bypass = 1 - kOn_tsy
kP_tsy_mix chnget "tsy_mix"
kC_tsy_mix changed2 kP_tsy_mix
if (kC_tsy_mix == 1 || kInit == 1) then
  gkTSY_mix = kP_tsy_mix
endif
kP_tsy_cut chnget "tsy_cut"
kC_tsy_cut changed2 kP_tsy_cut
if (kC_tsy_cut == 1 || kInit == 1) then
  gkTSY_cut = kP_tsy_cut
endif
kP_tsy_res chnget "tsy_res"
kC_tsy_res changed2 kP_tsy_res
if (kC_tsy_res == 1 || kInit == 1) then
  gkTSY_res = kP_tsy_res
endif
kP_tsy_glide chnget "tsy_glide"
kC_tsy_glide changed2 kP_tsy_glide
if (kC_tsy_glide == 1 || kInit == 1) then
  gkTSY_glide = kP_tsy_glide
endif
kP_tsy_sub chnget "tsy_sub"
kC_tsy_sub changed2 kP_tsy_sub
if (kC_tsy_sub == 1 || kInit == 1) then
  gkTSY_sub = kP_tsy_sub
endif
kP_tsy_det chnget "tsy_det"
kC_tsy_det changed2 kP_tsy_det
if (kC_tsy_det == 1 || kInit == 1) then
  gkTSY_det = kP_tsy_det
endif

; -- PLLFUZZ (toggle 62)
kOn_pll chnget "on_pll"
kClk_pll changed2 kOn_pll
if (kClk_pll == 1) then
  kSel = 47
  kSelChg = 1
endif
kPv_pll init 0
if (kOn_pll > 0.5 && kPv_pll < 0.5) then
endif
kPv_pll = kOn_pll
gkPLL_bypass = 1 - kOn_pll
kP_pll_mix chnget "pll_mix"
kC_pll_mix changed2 kP_pll_mix
if (kC_pll_mix == 1 || kInit == 1) then
  gkPLL_mix = kP_pll_mix
endif
kP_pll_intv chnget "pll_intv"
kC_pll_intv changed2 kP_pll_intv
if (kC_pll_intv == 1 || kInit == 1) then
  gkPLL_intv = (kP_pll_intv - 1) * 1
endif
kP_pll_lock chnget "pll_lock"
kC_pll_lock changed2 kP_pll_lock
if (kC_pll_lock == 1 || kInit == 1) then
  gkPLL_lock = kP_pll_lock
endif
kP_pll_vmix chnget "pll_vmix"
kC_pll_vmix changed2 kP_pll_vmix
if (kC_pll_vmix == 1 || kInit == 1) then
  gkPLL_vmix = kP_pll_vmix
endif

; -- TONE EQ (toggle 58)
kOn_teq chnget "on_teq"
kClk_teq changed2 kOn_teq
if (kClk_teq == 1) then
  kSel = 48
  kSelChg = 1
endif
kPv_teq init 0
if (kOn_teq > 0.5 && kPv_teq < 0.5) then
endif
kPv_teq = kOn_teq
gkTEQ_bypass = 1 - kOn_teq
kP_teq_mid chnget "teq_mid"
kC_teq_mid changed2 kP_teq_mid
if (kC_teq_mid == 1 || kInit == 1) then
  gkTEQ_mid = kP_teq_mid
endif
kP_teq_freq chnget "teq_freq"
kC_teq_freq changed2 kP_teq_freq
if (kC_teq_freq == 1 || kInit == 1) then
  gkTEQ_freq = kP_teq_freq
endif
kP_teq_tilt chnget "teq_tilt"
kC_teq_tilt changed2 kP_teq_tilt
if (kC_teq_tilt == 1 || kInit == 1) then
  gkTEQ_tilt = kP_teq_tilt
endif
kP_teq_low chnget "teq_low"
kC_teq_low changed2 kP_teq_low
if (kC_teq_low == 1 || kInit == 1) then
  gkTEQ_low = kP_teq_low
endif
kP_teq_high chnget "teq_high"
kC_teq_high changed2 kP_teq_high
if (kC_teq_high == 1 || kInit == 1) then
  gkTEQ_high = kP_teq_high
endif

; -- FEEDBACKER (toggle 63)
kOn_fbk chnget "on_fbk"
kClk_fbk changed2 kOn_fbk
if (kClk_fbk == 1) then
  kSel = 49
  kSelChg = 1
endif
kPv_fbk init 0
if (kOn_fbk > 0.5 && kPv_fbk < 0.5) then
  kExprTgt = 63
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: FEEDBACKER\")"
endif
kPv_fbk = kOn_fbk
gkFBK_bypass = 1 - kOn_fbk
kP_fbk_level chnget "fbk_level"
kC_fbk_level changed2 kP_fbk_level
if (kC_fbk_level == 1 || kInit == 1) then
  gkFBK_level = kP_fbk_level
endif
kP_fbk_harm chnget "fbk_harm"
kC_fbk_harm changed2 kP_fbk_harm
if (kC_fbk_harm == 1 || kInit == 1) then
  gkFBK_harm = (kP_fbk_harm - 1) * 0.25
endif
kP_fbk_bloom chnget "fbk_bloom"
kC_fbk_bloom changed2 kP_fbk_bloom
if (kC_fbk_bloom == 1 || kInit == 1) then
  gkFBK_bloom = kP_fbk_bloom
endif
kP_fbk_vib chnget "fbk_vib"
kC_fbk_vib changed2 kP_fbk_vib
if (kC_fbk_vib == 1 || kInit == 1) then
  gkFBK_vib = kP_fbk_vib
endif

; -- DIFFUSION (toggle 64)
kOn_dif chnget "on_dif"
kClk_dif changed2 kOn_dif
if (kClk_dif == 1) then
  kSel = 50
  kSelChg = 1
endif
kPv_dif init 0
if (kOn_dif > 0.5 && kPv_dif < 0.5) then
endif
kPv_dif = kOn_dif
gkDIF_bypass = 1 - kOn_dif
kP_dif_wet chnget "dif_wet"
kC_dif_wet changed2 kP_dif_wet
if (kC_dif_wet == 1 || kInit == 1) then
  gkDIF_wet = kP_dif_wet
endif
kP_dif_depth chnget "dif_depth"
kC_dif_depth changed2 kP_dif_depth
if (kC_dif_depth == 1 || kInit == 1) then
  gkDIF_depth = kP_dif_depth
endif
kP_dif_rate chnget "dif_rate"
kC_dif_rate changed2 kP_dif_rate
if (kC_dif_rate == 1 || kInit == 1) then
  gkDIF_rate = kP_dif_rate
endif
kP_dif_duck chnget "dif_duck"
kC_dif_duck changed2 kP_dif_duck
if (kC_dif_duck == 1 || kInit == 1) then
  gkDIF_duck = kP_dif_duck
endif

; -- CHAIN FB (toggle 60)
kOn_cfb chnget "on_cfb"
kClk_cfb changed2 kOn_cfb
if (kClk_cfb == 1) then
  kSel = 51
  kSelChg = 1
endif
kPv_cfb init 0
if (kOn_cfb > 0.5 && kPv_cfb < 0.5) then
endif
kPv_cfb = kOn_cfb
gkCFB_bypass = 1 - kOn_cfb
kP_cfb_ret chnget "cfb_ret"
kC_cfb_ret changed2 kP_cfb_ret
if (kC_cfb_ret == 1 || kInit == 1) then
  gkCFB_ret = kP_cfb_ret
endif
kP_cfb_gain chnget "cfb_gain"
kC_cfb_gain changed2 kP_cfb_gain
if (kC_cfb_gain == 1 || kInit == 1) then
  gkCFB_gain = kP_cfb_gain
endif
kP_cfb_tone chnget "cfb_tone"
kC_cfb_tone changed2 kP_cfb_tone
if (kC_cfb_tone == 1 || kInit == 1) then
  gkCFB_tone = kP_cfb_tone
endif
kP_cfb_ceil chnget "cfb_ceil"
kC_cfb_ceil changed2 kP_cfb_ceil
if (kC_cfb_ceil == 1 || kInit == 1) then
  gkCFB_ceil = kP_cfb_ceil
endif

; -- BLOOPER (toggle 56)
kOn_blp chnget "on_blp"
kClk_blp changed2 kOn_blp
if (kClk_blp == 1) then
  kSel = 52
  kSelChg = 1
endif
kPv_blp init 0
if (kOn_blp > 0.5 && kPv_blp < 0.5) then
  kExprTgt = 56
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BLOOPER\")"
endif
kPv_blp = kOn_blp
gkBLP_bypass = 1 - kOn_blp
kP_blp_motion chnget "blp_motion"
kC_blp_motion changed2 kP_blp_motion
if (kC_blp_motion == 1 || kInit == 1) then
  gkBLP_Motion = kP_blp_motion
endif
kP_blp_scat chnget "blp_scat"
kC_blp_scat changed2 kP_blp_scat
if (kC_blp_scat == 1 || kInit == 1) then
  gkBLP_Scat = kP_blp_scat
endif
kP_blp_size chnget "blp_size"
kC_blp_size changed2 kP_blp_size
if (kC_blp_size == 1 || kInit == 1) then
  gkBLP_Size = kP_blp_size
endif
kP_blp_char chnget "blp_char"
kC_blp_char changed2 kP_blp_char
if (kC_blp_char == 1 || kInit == 1) then
  gkBLP_Char = (kP_blp_char - 1) * 1
endif

; -- ONWARD (toggle 45)
kOn_onw chnget "on_onw"
kClk_onw changed2 kOn_onw
if (kClk_onw == 1) then
  kSel = 53
  kSelChg = 1
endif
kPv_onw init 0
if (kOn_onw > 0.5 && kPv_onw < 0.5) then
  kExprTgt = 45
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: ONWARD\")"
endif
kPv_onw = kOn_onw
gkONW_bypass = 1 - kOn_onw
kP_onw_mix chnget "onw_mix"
kC_onw_mix changed2 kP_onw_mix
if (kC_onw_mix == 1 || kInit == 1) then
  gkONW_mix = kP_onw_mix
endif
kP_onw_blend chnget "onw_blend"
kC_onw_blend changed2 kP_onw_blend
if (kC_onw_blend == 1 || kInit == 1) then
  gkONW_blend = kP_onw_blend
endif
kP_onw_size chnget "onw_size"
kC_onw_size changed2 kP_onw_size
if (kC_onw_size == 1 || kInit == 1) then
  gkONW_size = kP_onw_size
endif
kP_onw_follow chnget "onw_follow"
kC_onw_follow changed2 kP_onw_follow
if (kC_onw_follow == 1 || kInit == 1) then
  gkONW_follow = kP_onw_follow
endif
kP_onw_density chnget "onw_density"
kC_onw_density changed2 kP_onw_density
if (kC_onw_density == 1 || kInit == 1) then
  gkONW_density = kP_onw_density
endif
kP_onw_err chnget "onw_err"
kC_onw_err changed2 kP_onw_err
if (kC_onw_err == 1 || kInit == 1) then
  gkONW_err = kP_onw_err
endif
kP_onw_oct chnget "onw_oct"
kC_onw_oct changed2 kP_onw_oct
if (kC_onw_oct == 1 || kInit == 1) then
  gkONW_oct = kP_onw_oct
endif

; -- HABIT (toggle 36)
kOn_hab chnget "on_hab"
kClk_hab changed2 kOn_hab
if (kClk_hab == 1) then
  kSel = 54
  kSelChg = 1
endif
kPv_hab init 0
if (kOn_hab > 0.5 && kPv_hab < 0.5) then
  kExprTgt = 36
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: HABIT\")"
  gkHab_rec = 1
  cabbageSetValue "hab_rec", k(2), k(1)
endif
kPv_hab = kOn_hab
gkHabitBypass = 1 - kOn_hab
kP_hab_mix chnget "hab_mix"
kC_hab_mix changed2 kP_hab_mix
if (kC_hab_mix == 1 || kInit == 1) then
  gkHabitMix = kP_hab_mix
endif
kP_hab_size chnget "hab_size"
kC_hab_size changed2 kP_hab_size
if (kC_hab_size == 1 || kInit == 1) then
  gkHabitSize = kP_hab_size
endif
kP_hab_repeats chnget "hab_repeats"
kC_hab_repeats changed2 kP_hab_repeats
if (kC_hab_repeats == 1 || kInit == 1) then
  gkHabitRepeats = kP_hab_repeats
endif
kP_hab_blend chnget "hab_blend"
kC_hab_blend changed2 kP_hab_blend
if (kC_hab_blend == 1 || kInit == 1) then
  gkHabitBlend = kP_hab_blend
endif
kP_hab_rec chnget "hab_rec"
kC_hab_rec changed2 kP_hab_rec
if (kC_hab_rec == 1 || kInit == 1) then
  gkHab_rec = (kP_hab_rec - 1) * 1
endif

; -- MOOD II (toggle 66)
kOn_md2 chnget "on_md2"
kClk_md2 changed2 kOn_md2
if (kClk_md2 == 1) then
  kSel = 55
  kSelChg = 1
endif
kPv_md2 init 0
if (kOn_md2 > 0.5 && kPv_md2 < 0.5) then
  kExprTgt = 66
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: MOOD II\")"
endif
kPv_md2 = kOn_md2
gkMD2_bypass = 1 - kOn_md2
kP_md2_mix chnget "md2_mix"
kC_md2_mix changed2 kP_md2_mix
if (kC_md2_mix == 1 || kInit == 1) then
  gkMD2_Mix = kP_md2_mix
endif
kP_md2_clock chnget "md2_clock"
kC_md2_clock changed2 kP_md2_clock
if (kC_md2_clock == 1 || kInit == 1) then
  gkMD2_Clock = kP_md2_clock
endif
kP_md2_length chnget "md2_length"
kC_md2_length changed2 kP_md2_length
if (kC_md2_length == 1 || kInit == 1) then
  gkMD2_Length = kP_md2_length
endif
kP_md2_gen chnget "md2_gen"
kC_md2_gen changed2 kP_md2_gen
if (kC_md2_gen == 1 || kInit == 1) then
  gkMD2_Gen = kP_md2_gen
endif

; -- IMPULSE (toggle 48)
kOn_imp chnget "on_imp"
kClk_imp changed2 kOn_imp
if (kClk_imp == 1) then
  kSel = 56
  kSelChg = 1
endif
kPv_imp init 0
if (kOn_imp > 0.5 && kPv_imp < 0.5) then
  kExprTgt = 48
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: IMPULSE\")"
endif
kPv_imp = kOn_imp
gkIMP_bypass = 1 - kOn_imp
kP_imp_mix chnget "imp_mix"
kC_imp_mix changed2 kP_imp_mix
if (kC_imp_mix == 1 || kInit == 1) then
  gkIMP_mix = kP_imp_mix
endif
kP_imp_prob chnget "imp_prob"
kC_imp_prob changed2 kP_imp_prob
if (kC_imp_prob == 1 || kInit == 1) then
  gkIMP_prob = kP_imp_prob
endif
kP_imp_dec chnget "imp_dec"
kC_imp_dec changed2 kP_imp_dec
if (kC_imp_dec == 1 || kInit == 1) then
  gkIMP_dec = kP_imp_dec
endif
kP_imp_tone chnget "imp_tone"
kC_imp_tone changed2 kP_imp_tone
if (kC_imp_tone == 1 || kInit == 1) then
  gkIMP_tone = kP_imp_tone
endif

; -- BBCUT (toggle 20)
kOn_bbc chnget "on_bbc"
kClk_bbc changed2 kOn_bbc
if (kClk_bbc == 1) then
  kSel = 57
  kSelChg = 1
endif
kPv_bbc init 0
if (kOn_bbc > 0.5 && kPv_bbc < 0.5) then
  kExprTgt = 20
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: BBCUT\")"
endif
kPv_bbc = kOn_bbc
gkBBcut_bypass = 1 - kOn_bbc
kP_bbc_mix chnget "bbc_mix"
kC_bbc_mix changed2 kP_bbc_mix
if (kC_bbc_mix == 1 || kInit == 1) then
  gkBBcut_drywet = kP_bbc_mix
endif
kP_bbc_prob chnget "bbc_prob"
kC_bbc_prob changed2 kP_bbc_prob
if (kC_bbc_prob == 1 || kInit == 1) then
  gkBBcut_prob = kP_bbc_prob
endif
kP_bbc_flt chnget "bbc_flt"
kC_bbc_flt changed2 kP_bbc_flt
if (kC_bbc_flt == 1 || kInit == 1) then
  gkBBcut_fltMix = kP_bbc_flt
endif

; -- REPEAT (toggle 23)
kOn_rpt chnget "on_rpt"
kClk_rpt changed2 kOn_rpt
if (kClk_rpt == 1) then
  kSel = 58
  kSelChg = 1
endif
kPv_rpt init 0
if (kOn_rpt > 0.5 && kPv_rpt < 0.5) then
  kExprTgt = 23
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: REPEAT\")"
endif
kPv_rpt = kOn_rpt
gkWGuide_bypass = 1 - kOn_rpt
kP_rpt_mix chnget "rpt_mix"
kC_rpt_mix changed2 kP_rpt_mix
if (kC_rpt_mix == 1 || kInit == 1) then
  gkWGuide_drywet = kP_rpt_mix
endif
kP_rpt_prob chnget "rpt_prob"
kC_rpt_prob changed2 kP_rpt_prob
if (kC_rpt_prob == 1 || kInit == 1) then
  gkRPT_prob = kP_rpt_prob
endif
kP_rpt_char chnget "rpt_char"
kC_rpt_char changed2 kP_rpt_char
if (kC_rpt_char == 1 || kInit == 1) then
  gkRPT_char = kP_rpt_char
endif
kP_rpt_len chnget "rpt_len"
kC_rpt_len changed2 kP_rpt_len
if (kC_rpt_len == 1 || kInit == 1) then
  gkRPT_len = (kP_rpt_len - 1) * 1
endif

; -- CHOP (toggle 24)
kOn_chp chnget "on_chp"
kClk_chp changed2 kOn_chp
if (kClk_chp == 1) then
  kSel = 59
  kSelChg = 1
endif
kPv_chp init 0
if (kOn_chp > 0.5 && kPv_chp < 0.5) then
  kExprTgt = 24
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: CHOP\")"
endif
kPv_chp = kOn_chp
gkSqMod_bypass = 1 - kOn_chp
kP_chp_depth chnget "chp_depth"
kC_chp_depth changed2 kP_chp_depth
if (kC_chp_depth == 1 || kInit == 1) then
  gkSqMod_drywet = kP_chp_depth
endif
kP_chp_prob chnget "chp_prob"
kC_chp_prob changed2 kP_chp_prob
if (kC_chp_prob == 1 || kInit == 1) then
  gkCHP_prob = kP_chp_prob
endif
kP_chp_rate chnget "chp_rate"
kC_chp_rate changed2 kP_chp_rate
if (kC_chp_rate == 1 || kInit == 1) then
  gkCHP_rate = kP_chp_rate
endif
kP_chp_shape chnget "chp_shape"
kC_chp_shape changed2 kP_chp_shape
if (kC_chp_shape == 1 || kInit == 1) then
  gkCHP_shape = kP_chp_shape
endif

; -- RVRS (toggle 25)
kOn_rvs chnget "on_rvs"
kClk_rvs changed2 kOn_rvs
if (kClk_rvs == 1) then
  kSel = 60
  kSelChg = 1
endif
kPv_rvs init 0
if (kOn_rvs > 0.5 && kPv_rvs < 0.5) then
  kExprTgt = 25
  cabbageSet k(1), "exprLbl", "text(\"PEDAL TARGET: RVRS\")"
endif
kPv_rvs = kOn_rvs
gkFShift_bypass = 1 - kOn_rvs
kP_rvs_mix chnget "rvs_mix"
kC_rvs_mix changed2 kP_rvs_mix
if (kC_rvs_mix == 1 || kInit == 1) then
  gkFShift_drywet = kP_rvs_mix
endif
kP_rvs_prob chnget "rvs_prob"
kC_rvs_prob changed2 kP_rvs_prob
if (kC_rvs_prob == 1 || kInit == 1) then
  gkRVS_prob = kP_rvs_prob
endif
kP_rvs_len chnget "rvs_len"
kC_rvs_len changed2 kP_rvs_len
if (kC_rvs_len == 1 || kInit == 1) then
  gkRVS_len = kP_rvs_len
endif

; -- FREEZE clear (momentary) --
kB_frz_clear chnget "frz_clear"
kP_frz_clear init 0
if (kB_frz_clear > 0.5 && kP_frz_clear < 0.5) then
  gkFrzClear = 1
endif
kP_frz_clear = kB_frz_clear

; ---------- IR VERB rows ----------
kIrB0 chnget "ir_row0"
kIrC0 changed2 kIrB0
if (kIrC0 == 1) then
  kSel = 62
  kSelChg = 1
endif
kIrP0 init 0
if (kIrB0 > 0.5 && kIrP0 < 0.5) then
  gkIRActiveRow = 0
  gkIRBypass = 0
  cabbageSetValue "ir_row1", k(0), k(1)
  cabbageSetValue "ir_row2", k(0), k(1)
  cabbageSetValue "ir_row3", k(0), k(1)
  cabbageSetValue "ir_row4", k(0), k(1)
elseif (kIrB0 < 0.5 && kIrP0 > 0.5 && gkIRActiveRow == 0) then
  gkIRBypass = 1
endif
kIrP0 = kIrB0
kIrS0 chnget "ir_sel0"
kIrSc0 changed2 kIrS0
if (kIrSc0 == 1 || kInit == 1) then
  gkIRSel[0] = kIrS0 - 1
endif
kIrM0 chnget "ir_mix0"
kIrMc0 changed2 kIrM0
if (kIrMc0 == 1 || kInit == 1) then
  gkIRRowMix[0] = kIrM0
endif
kIrB1 chnget "ir_row1"
kIrC1 changed2 kIrB1
if (kIrC1 == 1) then
  kSel = 62
  kSelChg = 1
endif
kIrP1 init 0
if (kIrB1 > 0.5 && kIrP1 < 0.5) then
  gkIRActiveRow = 1
  gkIRBypass = 0
  cabbageSetValue "ir_row0", k(0), k(1)
  cabbageSetValue "ir_row2", k(0), k(1)
  cabbageSetValue "ir_row3", k(0), k(1)
  cabbageSetValue "ir_row4", k(0), k(1)
elseif (kIrB1 < 0.5 && kIrP1 > 0.5 && gkIRActiveRow == 1) then
  gkIRBypass = 1
endif
kIrP1 = kIrB1
kIrS1 chnget "ir_sel1"
kIrSc1 changed2 kIrS1
if (kIrSc1 == 1 || kInit == 1) then
  gkIRSel[1] = kIrS1 - 1
endif
kIrM1 chnget "ir_mix1"
kIrMc1 changed2 kIrM1
if (kIrMc1 == 1 || kInit == 1) then
  gkIRRowMix[1] = kIrM1
endif
kIrB2 chnget "ir_row2"
kIrC2 changed2 kIrB2
if (kIrC2 == 1) then
  kSel = 62
  kSelChg = 1
endif
kIrP2 init 0
if (kIrB2 > 0.5 && kIrP2 < 0.5) then
  gkIRActiveRow = 2
  gkIRBypass = 0
  cabbageSetValue "ir_row0", k(0), k(1)
  cabbageSetValue "ir_row1", k(0), k(1)
  cabbageSetValue "ir_row3", k(0), k(1)
  cabbageSetValue "ir_row4", k(0), k(1)
elseif (kIrB2 < 0.5 && kIrP2 > 0.5 && gkIRActiveRow == 2) then
  gkIRBypass = 1
endif
kIrP2 = kIrB2
kIrS2 chnget "ir_sel2"
kIrSc2 changed2 kIrS2
if (kIrSc2 == 1 || kInit == 1) then
  gkIRSel[2] = kIrS2 - 1
endif
kIrM2 chnget "ir_mix2"
kIrMc2 changed2 kIrM2
if (kIrMc2 == 1 || kInit == 1) then
  gkIRRowMix[2] = kIrM2
endif
kIrB3 chnget "ir_row3"
kIrC3 changed2 kIrB3
if (kIrC3 == 1) then
  kSel = 62
  kSelChg = 1
endif
kIrP3 init 0
if (kIrB3 > 0.5 && kIrP3 < 0.5) then
  gkIRActiveRow = 3
  gkIRBypass = 0
  cabbageSetValue "ir_row0", k(0), k(1)
  cabbageSetValue "ir_row1", k(0), k(1)
  cabbageSetValue "ir_row2", k(0), k(1)
  cabbageSetValue "ir_row4", k(0), k(1)
elseif (kIrB3 < 0.5 && kIrP3 > 0.5 && gkIRActiveRow == 3) then
  gkIRBypass = 1
endif
kIrP3 = kIrB3
kIrS3 chnget "ir_sel3"
kIrSc3 changed2 kIrS3
if (kIrSc3 == 1 || kInit == 1) then
  gkIRSel[3] = kIrS3 - 1
endif
kIrM3 chnget "ir_mix3"
kIrMc3 changed2 kIrM3
if (kIrMc3 == 1 || kInit == 1) then
  gkIRRowMix[3] = kIrM3
endif
kIrB4 chnget "ir_row4"
kIrC4 changed2 kIrB4
if (kIrC4 == 1) then
  kSel = 62
  kSelChg = 1
endif
kIrP4 init 0
if (kIrB4 > 0.5 && kIrP4 < 0.5) then
  gkIRActiveRow = 4
  gkIRBypass = 0
  cabbageSetValue "ir_row0", k(0), k(1)
  cabbageSetValue "ir_row1", k(0), k(1)
  cabbageSetValue "ir_row2", k(0), k(1)
  cabbageSetValue "ir_row3", k(0), k(1)
elseif (kIrB4 < 0.5 && kIrP4 > 0.5 && gkIRActiveRow == 4) then
  gkIRBypass = 1
endif
kIrP4 = kIrB4
kIrS4 chnget "ir_sel4"
kIrSc4 changed2 kIrS4
if (kIrSc4 == 1 || kInit == 1) then
  gkIRSel[4] = kIrS4 - 1
endif
kIrM4 chnget "ir_mix4"
kIrMc4 changed2 kIrM4
if (kIrMc4 == 1 || kInit == 1) then
  gkIRRowMix[4] = kIrM4
endif
kIrDamp chnget "ir_damp"
kIrDampC changed2 kIrDamp
if (kIrDampC == 1 || kInit == 1) then
  gkIRDamp = kIrDamp
endif
kIrLev chnget "ir_level"
kIrLevC changed2 kIrLev
if (kIrLevC == 1 || kInit == 1) then
  gkIRLevel = kIrLev
endif

; ---------- NAM rows ----------
kNmB0 chnget "nam_row0"
kNmC0 changed2 kNmB0
if (kNmC0 == 1) then
  kSel = 61
  kSelChg = 1
endif
kNmB1 chnget "nam_row1"
kNmC1 changed2 kNmB1
if (kNmC1 == 1) then
  kSel = 61
  kSelChg = 1
endif
kNmB2 chnget "nam_row2"
kNmC2 changed2 kNmB2
if (kNmC2 == 1) then
  kSel = 61
  kSelChg = 1
endif
kNmB3 chnget "nam_row3"
kNmC3 changed2 kNmB3
if (kNmC3 == 1) then
  kSel = 61
  kSelChg = 1
endif
kNmB4 chnget "nam_row4"
kNmC4 changed2 kNmB4
if (kNmC4 == 1) then
  kSel = 61
  kSelChg = 1
endif
#ifdef VST_NAM
kNmP0 init 0
if (kNmB0 > 0.5 && kNmP0 < 0.5) then
  gkNAMActiveRow = 0
  gkNAMBypass = 0
  gkNAMProfileIdx = giNAMRowBase[0] + gkNAMRowSel[0]
  cabbageSetValue "nam_row1", k(0), k(1)
  cabbageSetValue "nam_row2", k(0), k(1)
  cabbageSetValue "nam_row3", k(0), k(1)
  cabbageSetValue "nam_row4", k(0), k(1)
elseif (kNmB0 < 0.5 && kNmP0 > 0.5 && gkNAMActiveRow == 0) then
  gkNAMBypass = 1
endif
kNmP0 = kNmB0
kNmS0 chnget "nam_sel0"
kNmSc0 changed2 kNmS0
if (kNmSc0 == 1) then
  gkNAMRowSel[0] = kNmS0 - 1
  if (gkNAMActiveRow == 0) then
    gkNAMProfileIdx = giNAMRowBase[0] + gkNAMRowSel[0]
  endif
endif
kNmP1 init 0
if (kNmB1 > 0.5 && kNmP1 < 0.5) then
  gkNAMActiveRow = 1
  gkNAMBypass = 0
  gkNAMProfileIdx = giNAMRowBase[1] + gkNAMRowSel[1]
  cabbageSetValue "nam_row0", k(0), k(1)
  cabbageSetValue "nam_row2", k(0), k(1)
  cabbageSetValue "nam_row3", k(0), k(1)
  cabbageSetValue "nam_row4", k(0), k(1)
elseif (kNmB1 < 0.5 && kNmP1 > 0.5 && gkNAMActiveRow == 1) then
  gkNAMBypass = 1
endif
kNmP1 = kNmB1
kNmS1 chnget "nam_sel1"
kNmSc1 changed2 kNmS1
if (kNmSc1 == 1) then
  gkNAMRowSel[1] = kNmS1 - 1
  if (gkNAMActiveRow == 1) then
    gkNAMProfileIdx = giNAMRowBase[1] + gkNAMRowSel[1]
  endif
endif
kNmP2 init 0
if (kNmB2 > 0.5 && kNmP2 < 0.5) then
  gkNAMActiveRow = 2
  gkNAMBypass = 0
  gkNAMProfileIdx = giNAMRowBase[2] + gkNAMRowSel[2]
  cabbageSetValue "nam_row0", k(0), k(1)
  cabbageSetValue "nam_row1", k(0), k(1)
  cabbageSetValue "nam_row3", k(0), k(1)
  cabbageSetValue "nam_row4", k(0), k(1)
elseif (kNmB2 < 0.5 && kNmP2 > 0.5 && gkNAMActiveRow == 2) then
  gkNAMBypass = 1
endif
kNmP2 = kNmB2
kNmS2 chnget "nam_sel2"
kNmSc2 changed2 kNmS2
if (kNmSc2 == 1) then
  gkNAMRowSel[2] = kNmS2 - 1
  if (gkNAMActiveRow == 2) then
    gkNAMProfileIdx = giNAMRowBase[2] + gkNAMRowSel[2]
  endif
endif
kNmP3 init 0
if (kNmB3 > 0.5 && kNmP3 < 0.5) then
  gkNAMActiveRow = 3
  gkNAMBypass = 0
  gkNAMProfileIdx = giNAMRowBase[3] + gkNAMRowSel[3]
  cabbageSetValue "nam_row0", k(0), k(1)
  cabbageSetValue "nam_row1", k(0), k(1)
  cabbageSetValue "nam_row2", k(0), k(1)
  cabbageSetValue "nam_row4", k(0), k(1)
elseif (kNmB3 < 0.5 && kNmP3 > 0.5 && gkNAMActiveRow == 3) then
  gkNAMBypass = 1
endif
kNmP3 = kNmB3
kNmS3 chnget "nam_sel3"
kNmSc3 changed2 kNmS3
if (kNmSc3 == 1) then
  gkNAMRowSel[3] = kNmS3 - 1
  if (gkNAMActiveRow == 3) then
    gkNAMProfileIdx = giNAMRowBase[3] + gkNAMRowSel[3]
  endif
endif
kNmP4 init 0
if (kNmB4 > 0.5 && kNmP4 < 0.5) then
  gkNAMActiveRow = 4
  gkNAMBypass = 0
  gkNAMProfileIdx = giNAMRowBase[4] + gkNAMRowSel[4]
  cabbageSetValue "nam_row0", k(0), k(1)
  cabbageSetValue "nam_row1", k(0), k(1)
  cabbageSetValue "nam_row2", k(0), k(1)
  cabbageSetValue "nam_row3", k(0), k(1)
elseif (kNmB4 < 0.5 && kNmP4 > 0.5 && gkNAMActiveRow == 4) then
  gkNAMBypass = 1
endif
kNmP4 = kNmB4
kNmS4 chnget "nam_sel4"
kNmSc4 changed2 kNmS4
if (kNmSc4 == 1) then
  gkNAMRowSel[4] = kNmS4 - 1
  if (gkNAMActiveRow == 4) then
    gkNAMProfileIdx = giNAMRowBase[4] + gkNAMRowSel[4]
  endif
endif
kNamInG chnget "nam_in_gain"
kNamInGC changed2 kNamInG
if (kNamInGC == 1 || kInit == 1) then
  gkNAMInGain = kNamInG
endif
kNamOutG chnget "nam_out_gain"
kNamOutGC changed2 kNamOutG
if (kNamOutGC == 1 || kInit == 1) then
  gkNAMOutGain = kNamOutG
endif
#else
; NAM disabled in this build (see vst/README.md); keep it hard-bypassed.
gkNAMBypass = 1
#endif

; ---------- LOOPER (5-channel gesture surface) ----------
; Mirrors handler_looper.orc's $LP_CHn_PRESS / $LP_RELEASE_ALL /
; $LP_HOLD_WATCHER macros verbatim, but button-driven instead of
; MIDI-CC-driven (this build runs with MIDI disabled).
; -- LOOPER ch1 --
kLpBtn1 chnget "lp1_btn"
kLpPrv1 init 0
if (kLpBtn1 > 0.5 && kLpPrv1 < 0.5) then
  kLpNow1 times
  if (kLpNow1 - gkLpLastTap1 < 0.5) then
    ; double-tap -> clear
    gkLpDblFired1 = 1
    gkLpState1 = 0
    gkLpLen1 = 0
    gkLpWrPos1 = 0
    gkLpRdPos1 = 0
    gkLpSpIdx1 = 6
    if (gkDrumChan == 1) then
      gkDrumChan = -1
      gkDrumIdx = -1
    endif
    if (gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
      gkLpMasterLen = 0
    endif
  elseif (gkLpState1 == 0) then
    ; idle -> record (speed reset + transport anchor)
    gkLpState1 = 1
    gkLpWrPos1 = 0
    gkLpRdPos1 = 0
    gkLpSpIdx1 = 6
    gkLpAnchor1 = gkLpClock
    gkLpConsumed1 = 1
  elseif (gkLpState1 == 1) then
    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see
    ; handler/handler_looper.orc for the canonical macro — kept
    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)
    if (gkLpMasterLen == 0) then
      ; first loop closed anywhere = master transport
      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
        ; CLOCKED -- quantize to the nearest whole-beat grid line
        kLpBeats1 = round(gkLpWrPos1 / gkLpBeatSamples)
        if (kLpBeats1 < 1) then
          kLpBeats1 = 1
        endif
        kLpMTgt1 = kLpBeats1 * gkLpBeatSamples
        kLpMBufSz1 = ftlen(giLpBuf1L)
        if (kLpMTgt1 > kLpMBufSz1) then
          kLpMTgt1 = kLpMBufSz1
        endif
        if (gkLpWrPos1 >= kLpMTgt1) then
          ; late/exact -- round back, finalize now
          gkLpLen1      = kLpMTgt1
          gkLpMasterLen   = kLpMTgt1
          gkLpState1    = 2
          gkLpRdPos1    = gkLpWrPos1 % kLpMTgt1
        else
          ; early -- keep recording until the boundary
          gkLpTarget1 = kLpMTgt1
          gkLpState1  = 5
        endif
      else
        ; UNCLOCKED -- free capture, trim trailing silence
        gkLpTrimPos1 = gkLpWrPos1
        gkLpTarget1  = gkLpWrPos1   ; fallback if all-silent
        gkLpState1   = 6
      endif
    else
      kLpM1 = gkLpMasterLen
      kLpC0_1 = kLpM1 / 4
      kLpC1_1 = kLpM1 / 2
      kLpC2_1 = kLpM1
      kLpC3_1 = kLpM1 * 2
      kLpC4_1 = kLpM1 * 4
      kLpBst1 = kLpC0_1
      kLpBsD1 = abs(gkLpWrPos1 - kLpC0_1)
      if (abs(gkLpWrPos1 - kLpC1_1) < kLpBsD1) then
        kLpBst1 = kLpC1_1
        kLpBsD1 = abs(gkLpWrPos1 - kLpC1_1)
      endif
      if (abs(gkLpWrPos1 - kLpC2_1) < kLpBsD1) then
        kLpBst1 = kLpC2_1
        kLpBsD1 = abs(gkLpWrPos1 - kLpC2_1)
      endif
      if (abs(gkLpWrPos1 - kLpC3_1) < kLpBsD1) then
        kLpBst1 = kLpC3_1
        kLpBsD1 = abs(gkLpWrPos1 - kLpC3_1)
      endif
      if (abs(gkLpWrPos1 - kLpC4_1) < kLpBsD1) then
        kLpBst1 = kLpC4_1
      endif
      kLpBufSz1 = ftlen(giLpBuf1L)
      if (kLpBst1 > kLpBufSz1) then
        kLpBst1 = kLpBufSz1
      endif
      if (gkLpWrPos1 >= kLpBst1) then
        gkLpLen1   = kLpBst1
        gkLpState1 = 2
        gkLpRdPos1 = gkLpWrPos1 % kLpBst1
      else
        gkLpTarget1 = kLpBst1
        gkLpState1  = 5
      endif
    endif
    gkLpConsumed1 = 1
  elseif (gkLpState1 == 4) then
    ; stopped -> resume, transport relock
    gkLpState1 = 2
    gkLpRdPos1 = (gkLpClock - gkLpAnchor1) % gkLpLen1
    gkLpConsumed1 = 1
  endif
  gkLpLastTap1 = kLpNow1
  gkLpPressT1  = kLpNow1
elseif (kLpBtn1 < 0.5 && kLpPrv1 > 0.5) then
  ; release: classify tap-vs-hold for play/overdub only
  kLpRel1 times
  gkLpPressT1 = -1
  if (gkLpDblFired1 >= 0.5) then
    gkLpDblFired1 = 0
  elseif (gkLpConsumed1 >= 0.5) then
    gkLpConsumed1 = 0
  else
    if (gkLpState1 == 2) then
      gkLpState1 = 4     ; play -> stop
    elseif (gkLpState1 == 3) then
      gkLpState1 = 2     ; overdub -> play
    endif
  endif
endif
kLpPrv1 = kLpBtn1

; -- LOOPER ch2 --
kLpBtn2 chnget "lp2_btn"
kLpPrv2 init 0
if (kLpBtn2 > 0.5 && kLpPrv2 < 0.5) then
  kLpNow2 times
  if (kLpNow2 - gkLpLastTap2 < 0.5) then
    ; double-tap -> clear
    gkLpDblFired2 = 1
    gkLpState2 = 0
    gkLpLen2 = 0
    gkLpWrPos2 = 0
    gkLpRdPos2 = 0
    gkLpSpIdx2 = 6
    if (gkDrumChan == 2) then
      gkDrumChan = -1
      gkDrumIdx = -1
    endif
    if (gkLpLen1 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
      gkLpMasterLen = 0
    endif
  elseif (gkLpState2 == 0) then
    ; idle -> record (speed reset + transport anchor)
    gkLpState2 = 1
    gkLpWrPos2 = 0
    gkLpRdPos2 = 0
    gkLpSpIdx2 = 6
    gkLpAnchor2 = gkLpClock
    gkLpConsumed2 = 1
  elseif (gkLpState2 == 1) then
    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see
    ; handler/handler_looper.orc for the canonical macro — kept
    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)
    if (gkLpMasterLen == 0) then
      ; first loop closed anywhere = master transport
      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
        ; CLOCKED -- quantize to the nearest whole-beat grid line
        kLpBeats2 = round(gkLpWrPos2 / gkLpBeatSamples)
        if (kLpBeats2 < 1) then
          kLpBeats2 = 1
        endif
        kLpMTgt2 = kLpBeats2 * gkLpBeatSamples
        kLpMBufSz2 = ftlen(giLpBuf2L)
        if (kLpMTgt2 > kLpMBufSz2) then
          kLpMTgt2 = kLpMBufSz2
        endif
        if (gkLpWrPos2 >= kLpMTgt2) then
          ; late/exact -- round back, finalize now
          gkLpLen2      = kLpMTgt2
          gkLpMasterLen   = kLpMTgt2
          gkLpState2    = 2
          gkLpRdPos2    = gkLpWrPos2 % kLpMTgt2
        else
          ; early -- keep recording until the boundary
          gkLpTarget2 = kLpMTgt2
          gkLpState2  = 5
        endif
      else
        ; UNCLOCKED -- free capture, trim trailing silence
        gkLpTrimPos2 = gkLpWrPos2
        gkLpTarget2  = gkLpWrPos2   ; fallback if all-silent
        gkLpState2   = 6
      endif
    else
      kLpM2 = gkLpMasterLen
      kLpC0_2 = kLpM2 / 4
      kLpC1_2 = kLpM2 / 2
      kLpC2_2 = kLpM2
      kLpC3_2 = kLpM2 * 2
      kLpC4_2 = kLpM2 * 4
      kLpBst2 = kLpC0_2
      kLpBsD2 = abs(gkLpWrPos2 - kLpC0_2)
      if (abs(gkLpWrPos2 - kLpC1_2) < kLpBsD2) then
        kLpBst2 = kLpC1_2
        kLpBsD2 = abs(gkLpWrPos2 - kLpC1_2)
      endif
      if (abs(gkLpWrPos2 - kLpC2_2) < kLpBsD2) then
        kLpBst2 = kLpC2_2
        kLpBsD2 = abs(gkLpWrPos2 - kLpC2_2)
      endif
      if (abs(gkLpWrPos2 - kLpC3_2) < kLpBsD2) then
        kLpBst2 = kLpC3_2
        kLpBsD2 = abs(gkLpWrPos2 - kLpC3_2)
      endif
      if (abs(gkLpWrPos2 - kLpC4_2) < kLpBsD2) then
        kLpBst2 = kLpC4_2
      endif
      kLpBufSz2 = ftlen(giLpBuf2L)
      if (kLpBst2 > kLpBufSz2) then
        kLpBst2 = kLpBufSz2
      endif
      if (gkLpWrPos2 >= kLpBst2) then
        gkLpLen2   = kLpBst2
        gkLpState2 = 2
        gkLpRdPos2 = gkLpWrPos2 % kLpBst2
      else
        gkLpTarget2 = kLpBst2
        gkLpState2  = 5
      endif
    endif
    gkLpConsumed2 = 1
  elseif (gkLpState2 == 4) then
    ; stopped -> resume, transport relock
    gkLpState2 = 2
    gkLpRdPos2 = (gkLpClock - gkLpAnchor2) % gkLpLen2
    gkLpConsumed2 = 1
  endif
  gkLpLastTap2 = kLpNow2
  gkLpPressT2  = kLpNow2
elseif (kLpBtn2 < 0.5 && kLpPrv2 > 0.5) then
  ; release: classify tap-vs-hold for play/overdub only
  kLpRel2 times
  gkLpPressT2 = -1
  if (gkLpDblFired2 >= 0.5) then
    gkLpDblFired2 = 0
  elseif (gkLpConsumed2 >= 0.5) then
    gkLpConsumed2 = 0
  else
    if (gkLpState2 == 2) then
      gkLpState2 = 4     ; play -> stop
    elseif (gkLpState2 == 3) then
      gkLpState2 = 2     ; overdub -> play
    endif
  endif
endif
kLpPrv2 = kLpBtn2

; -- LOOPER ch3 --
kLpBtn3 chnget "lp3_btn"
kLpPrv3 init 0
if (kLpBtn3 > 0.5 && kLpPrv3 < 0.5) then
  kLpNow3 times
  if (kLpNow3 - gkLpLastTap3 < 0.5) then
    ; double-tap -> clear
    gkLpDblFired3 = 1
    gkLpState3 = 0
    gkLpLen3 = 0
    gkLpWrPos3 = 0
    gkLpRdPos3 = 0
    gkLpSpIdx3 = 6
    if (gkDrumChan == 3) then
      gkDrumChan = -1
      gkDrumIdx = -1
    endif
    if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
      gkLpMasterLen = 0
    endif
  elseif (gkLpState3 == 0) then
    ; idle -> record (speed reset + transport anchor)
    gkLpState3 = 1
    gkLpWrPos3 = 0
    gkLpRdPos3 = 0
    gkLpSpIdx3 = 6
    gkLpAnchor3 = gkLpClock
    gkLpConsumed3 = 1
  elseif (gkLpState3 == 1) then
    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see
    ; handler/handler_looper.orc for the canonical macro — kept
    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)
    if (gkLpMasterLen == 0) then
      ; first loop closed anywhere = master transport
      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
        ; CLOCKED -- quantize to the nearest whole-beat grid line
        kLpBeats3 = round(gkLpWrPos3 / gkLpBeatSamples)
        if (kLpBeats3 < 1) then
          kLpBeats3 = 1
        endif
        kLpMTgt3 = kLpBeats3 * gkLpBeatSamples
        kLpMBufSz3 = ftlen(giLpBuf3L)
        if (kLpMTgt3 > kLpMBufSz3) then
          kLpMTgt3 = kLpMBufSz3
        endif
        if (gkLpWrPos3 >= kLpMTgt3) then
          ; late/exact -- round back, finalize now
          gkLpLen3      = kLpMTgt3
          gkLpMasterLen   = kLpMTgt3
          gkLpState3    = 2
          gkLpRdPos3    = gkLpWrPos3 % kLpMTgt3
        else
          ; early -- keep recording until the boundary
          gkLpTarget3 = kLpMTgt3
          gkLpState3  = 5
        endif
      else
        ; UNCLOCKED -- free capture, trim trailing silence
        gkLpTrimPos3 = gkLpWrPos3
        gkLpTarget3  = gkLpWrPos3   ; fallback if all-silent
        gkLpState3   = 6
      endif
    else
      kLpM3 = gkLpMasterLen
      kLpC0_3 = kLpM3 / 4
      kLpC1_3 = kLpM3 / 2
      kLpC2_3 = kLpM3
      kLpC3_3 = kLpM3 * 2
      kLpC4_3 = kLpM3 * 4
      kLpBst3 = kLpC0_3
      kLpBsD3 = abs(gkLpWrPos3 - kLpC0_3)
      if (abs(gkLpWrPos3 - kLpC1_3) < kLpBsD3) then
        kLpBst3 = kLpC1_3
        kLpBsD3 = abs(gkLpWrPos3 - kLpC1_3)
      endif
      if (abs(gkLpWrPos3 - kLpC2_3) < kLpBsD3) then
        kLpBst3 = kLpC2_3
        kLpBsD3 = abs(gkLpWrPos3 - kLpC2_3)
      endif
      if (abs(gkLpWrPos3 - kLpC3_3) < kLpBsD3) then
        kLpBst3 = kLpC3_3
        kLpBsD3 = abs(gkLpWrPos3 - kLpC3_3)
      endif
      if (abs(gkLpWrPos3 - kLpC4_3) < kLpBsD3) then
        kLpBst3 = kLpC4_3
      endif
      kLpBufSz3 = ftlen(giLpBuf3L)
      if (kLpBst3 > kLpBufSz3) then
        kLpBst3 = kLpBufSz3
      endif
      if (gkLpWrPos3 >= kLpBst3) then
        gkLpLen3   = kLpBst3
        gkLpState3 = 2
        gkLpRdPos3 = gkLpWrPos3 % kLpBst3
      else
        gkLpTarget3 = kLpBst3
        gkLpState3  = 5
      endif
    endif
    gkLpConsumed3 = 1
  elseif (gkLpState3 == 4) then
    ; stopped -> resume, transport relock
    gkLpState3 = 2
    gkLpRdPos3 = (gkLpClock - gkLpAnchor3) % gkLpLen3
    gkLpConsumed3 = 1
  endif
  gkLpLastTap3 = kLpNow3
  gkLpPressT3  = kLpNow3
elseif (kLpBtn3 < 0.5 && kLpPrv3 > 0.5) then
  ; release: classify tap-vs-hold for play/overdub only
  kLpRel3 times
  gkLpPressT3 = -1
  if (gkLpDblFired3 >= 0.5) then
    gkLpDblFired3 = 0
  elseif (gkLpConsumed3 >= 0.5) then
    gkLpConsumed3 = 0
  else
    if (gkLpState3 == 2) then
      gkLpState3 = 4     ; play -> stop
    elseif (gkLpState3 == 3) then
      gkLpState3 = 2     ; overdub -> play
    endif
  endif
endif
kLpPrv3 = kLpBtn3

; -- LOOPER ch4 --
kLpBtn4 chnget "lp4_btn"
kLpPrv4 init 0
if (kLpBtn4 > 0.5 && kLpPrv4 < 0.5) then
  kLpNow4 times
  if (kLpNow4 - gkLpLastTap4 < 0.5) then
    ; double-tap -> clear
    gkLpDblFired4 = 1
    gkLpState4 = 0
    gkLpLen4 = 0
    gkLpWrPos4 = 0
    gkLpRdPos4 = 0
    gkLpSpIdx4 = 6
    if (gkDrumChan == 4) then
      gkDrumChan = -1
      gkDrumIdx = -1
    endif
    if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen5 == 0) then
      gkLpMasterLen = 0
    endif
  elseif (gkLpState4 == 0) then
    ; idle -> record (speed reset + transport anchor)
    gkLpState4 = 1
    gkLpWrPos4 = 0
    gkLpRdPos4 = 0
    gkLpSpIdx4 = 6
    gkLpAnchor4 = gkLpClock
    gkLpConsumed4 = 1
  elseif (gkLpState4 == 1) then
    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see
    ; handler/handler_looper.orc for the canonical macro — kept
    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)
    if (gkLpMasterLen == 0) then
      ; first loop closed anywhere = master transport
      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
        ; CLOCKED -- quantize to the nearest whole-beat grid line
        kLpBeats4 = round(gkLpWrPos4 / gkLpBeatSamples)
        if (kLpBeats4 < 1) then
          kLpBeats4 = 1
        endif
        kLpMTgt4 = kLpBeats4 * gkLpBeatSamples
        kLpMBufSz4 = ftlen(giLpBuf4L)
        if (kLpMTgt4 > kLpMBufSz4) then
          kLpMTgt4 = kLpMBufSz4
        endif
        if (gkLpWrPos4 >= kLpMTgt4) then
          ; late/exact -- round back, finalize now
          gkLpLen4      = kLpMTgt4
          gkLpMasterLen   = kLpMTgt4
          gkLpState4    = 2
          gkLpRdPos4    = gkLpWrPos4 % kLpMTgt4
        else
          ; early -- keep recording until the boundary
          gkLpTarget4 = kLpMTgt4
          gkLpState4  = 5
        endif
      else
        ; UNCLOCKED -- free capture, trim trailing silence
        gkLpTrimPos4 = gkLpWrPos4
        gkLpTarget4  = gkLpWrPos4   ; fallback if all-silent
        gkLpState4   = 6
      endif
    else
      kLpM4 = gkLpMasterLen
      kLpC0_4 = kLpM4 / 4
      kLpC1_4 = kLpM4 / 2
      kLpC2_4 = kLpM4
      kLpC3_4 = kLpM4 * 2
      kLpC4_4 = kLpM4 * 4
      kLpBst4 = kLpC0_4
      kLpBsD4 = abs(gkLpWrPos4 - kLpC0_4)
      if (abs(gkLpWrPos4 - kLpC1_4) < kLpBsD4) then
        kLpBst4 = kLpC1_4
        kLpBsD4 = abs(gkLpWrPos4 - kLpC1_4)
      endif
      if (abs(gkLpWrPos4 - kLpC2_4) < kLpBsD4) then
        kLpBst4 = kLpC2_4
        kLpBsD4 = abs(gkLpWrPos4 - kLpC2_4)
      endif
      if (abs(gkLpWrPos4 - kLpC3_4) < kLpBsD4) then
        kLpBst4 = kLpC3_4
        kLpBsD4 = abs(gkLpWrPos4 - kLpC3_4)
      endif
      if (abs(gkLpWrPos4 - kLpC4_4) < kLpBsD4) then
        kLpBst4 = kLpC4_4
      endif
      kLpBufSz4 = ftlen(giLpBuf4L)
      if (kLpBst4 > kLpBufSz4) then
        kLpBst4 = kLpBufSz4
      endif
      if (gkLpWrPos4 >= kLpBst4) then
        gkLpLen4   = kLpBst4
        gkLpState4 = 2
        gkLpRdPos4 = gkLpWrPos4 % kLpBst4
      else
        gkLpTarget4 = kLpBst4
        gkLpState4  = 5
      endif
    endif
    gkLpConsumed4 = 1
  elseif (gkLpState4 == 4) then
    ; stopped -> resume, transport relock
    gkLpState4 = 2
    gkLpRdPos4 = (gkLpClock - gkLpAnchor4) % gkLpLen4
    gkLpConsumed4 = 1
  endif
  gkLpLastTap4 = kLpNow4
  gkLpPressT4  = kLpNow4
elseif (kLpBtn4 < 0.5 && kLpPrv4 > 0.5) then
  ; release: classify tap-vs-hold for play/overdub only
  kLpRel4 times
  gkLpPressT4 = -1
  if (gkLpDblFired4 >= 0.5) then
    gkLpDblFired4 = 0
  elseif (gkLpConsumed4 >= 0.5) then
    gkLpConsumed4 = 0
  else
    if (gkLpState4 == 2) then
      gkLpState4 = 4     ; play -> stop
    elseif (gkLpState4 == 3) then
      gkLpState4 = 2     ; overdub -> play
    endif
  endif
endif
kLpPrv4 = kLpBtn4

; -- LOOPER ch5 --
kLpBtn5 chnget "lp5_btn"
kLpPrv5 init 0
if (kLpBtn5 > 0.5 && kLpPrv5 < 0.5) then
  kLpNow5 times
  if (kLpNow5 - gkLpLastTap5 < 0.5) then
    ; double-tap -> clear
    gkLpDblFired5 = 1
    gkLpState5 = 0
    gkLpLen5 = 0
    gkLpWrPos5 = 0
    gkLpRdPos5 = 0
    gkLpSpIdx5 = 6
    if (gkDrumChan == 5) then
      gkDrumChan = -1
      gkDrumIdx = -1
    endif
    if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0) then
      gkLpMasterLen = 0
    endif
  elseif (gkLpState5 == 0) then
    ; idle -> record (speed reset + transport anchor)
    gkLpState5 = 1
    gkLpWrPos5 = 0
    gkLpRdPos5 = 0
    gkLpSpIdx5 = 6
    gkLpAnchor5 = gkLpClock
    gkLpConsumed5 = 1
  elseif (gkLpState5 == 1) then
    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see
    ; handler/handler_looper.orc for the canonical macro — kept
    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)
    if (gkLpMasterLen == 0) then
      ; first loop closed anywhere = master transport
      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
        ; CLOCKED -- quantize to the nearest whole-beat grid line
        kLpBeats5 = round(gkLpWrPos5 / gkLpBeatSamples)
        if (kLpBeats5 < 1) then
          kLpBeats5 = 1
        endif
        kLpMTgt5 = kLpBeats5 * gkLpBeatSamples
        kLpMBufSz5 = ftlen(giLpBuf5L)
        if (kLpMTgt5 > kLpMBufSz5) then
          kLpMTgt5 = kLpMBufSz5
        endif
        if (gkLpWrPos5 >= kLpMTgt5) then
          ; late/exact -- round back, finalize now
          gkLpLen5      = kLpMTgt5
          gkLpMasterLen   = kLpMTgt5
          gkLpState5    = 2
          gkLpRdPos5    = gkLpWrPos5 % kLpMTgt5
        else
          ; early -- keep recording until the boundary
          gkLpTarget5 = kLpMTgt5
          gkLpState5  = 5
        endif
      else
        ; UNCLOCKED -- free capture, trim trailing silence
        gkLpTrimPos5 = gkLpWrPos5
        gkLpTarget5  = gkLpWrPos5   ; fallback if all-silent
        gkLpState5   = 6
      endif
    else
      kLpM5 = gkLpMasterLen
      kLpC0_5 = kLpM5 / 4
      kLpC1_5 = kLpM5 / 2
      kLpC2_5 = kLpM5
      kLpC3_5 = kLpM5 * 2
      kLpC4_5 = kLpM5 * 4
      kLpBst5 = kLpC0_5
      kLpBsD5 = abs(gkLpWrPos5 - kLpC0_5)
      if (abs(gkLpWrPos5 - kLpC1_5) < kLpBsD5) then
        kLpBst5 = kLpC1_5
        kLpBsD5 = abs(gkLpWrPos5 - kLpC1_5)
      endif
      if (abs(gkLpWrPos5 - kLpC2_5) < kLpBsD5) then
        kLpBst5 = kLpC2_5
        kLpBsD5 = abs(gkLpWrPos5 - kLpC2_5)
      endif
      if (abs(gkLpWrPos5 - kLpC3_5) < kLpBsD5) then
        kLpBst5 = kLpC3_5
        kLpBsD5 = abs(gkLpWrPos5 - kLpC3_5)
      endif
      if (abs(gkLpWrPos5 - kLpC4_5) < kLpBsD5) then
        kLpBst5 = kLpC4_5
      endif
      kLpBufSz5 = ftlen(giLpBuf5L)
      if (kLpBst5 > kLpBufSz5) then
        kLpBst5 = kLpBufSz5
      endif
      if (gkLpWrPos5 >= kLpBst5) then
        gkLpLen5   = kLpBst5
        gkLpState5 = 2
        gkLpRdPos5 = gkLpWrPos5 % kLpBst5
      else
        gkLpTarget5 = kLpBst5
        gkLpState5  = 5
      endif
    endif
    gkLpConsumed5 = 1
  elseif (gkLpState5 == 4) then
    ; stopped -> resume, transport relock
    gkLpState5 = 2
    gkLpRdPos5 = (gkLpClock - gkLpAnchor5) % gkLpLen5
    gkLpConsumed5 = 1
  endif
  gkLpLastTap5 = kLpNow5
  gkLpPressT5  = kLpNow5
elseif (kLpBtn5 < 0.5 && kLpPrv5 > 0.5) then
  ; release: classify tap-vs-hold for play/overdub only
  kLpRel5 times
  gkLpPressT5 = -1
  if (gkLpDblFired5 >= 0.5) then
    gkLpDblFired5 = 0
  elseif (gkLpConsumed5 >= 0.5) then
    gkLpConsumed5 = 0
  else
    if (gkLpState5 == 2) then
      gkLpState5 = 4     ; play -> stop
    elseif (gkLpState5 == 3) then
      gkLpState5 = 2     ; overdub -> play
    endif
  endif
endif
kLpPrv5 = kLpBtn5

; hold watcher: play->overdub / overdub->play at the 0.4s threshold,
; independent of button events (fires the instant the hold matures
; while the button is still down, same as the hardware).
kLpNowW times
if (gkLpPressT1 >= 0 && gkLpConsumed1 < 0.5 && gkLpDblFired1 < 0.5 && kLpNowW - gkLpPressT1 >= 0.4) then
  if (gkLpState1 == 2) then
    gkLpState1 = 3
    gkLpConsumed1 = 1
  elseif (gkLpState1 == 3) then
    gkLpState1 = 2
    gkLpConsumed1 = 1
  endif
endif
if (gkLpPressT2 >= 0 && gkLpConsumed2 < 0.5 && gkLpDblFired2 < 0.5 && kLpNowW - gkLpPressT2 >= 0.4) then
  if (gkLpState2 == 2) then
    gkLpState2 = 3
    gkLpConsumed2 = 1
  elseif (gkLpState2 == 3) then
    gkLpState2 = 2
    gkLpConsumed2 = 1
  endif
endif
if (gkLpPressT3 >= 0 && gkLpConsumed3 < 0.5 && gkLpDblFired3 < 0.5 && kLpNowW - gkLpPressT3 >= 0.4) then
  if (gkLpState3 == 2) then
    gkLpState3 = 3
    gkLpConsumed3 = 1
  elseif (gkLpState3 == 3) then
    gkLpState3 = 2
    gkLpConsumed3 = 1
  endif
endif
if (gkLpPressT4 >= 0 && gkLpConsumed4 < 0.5 && gkLpDblFired4 < 0.5 && kLpNowW - gkLpPressT4 >= 0.4) then
  if (gkLpState4 == 2) then
    gkLpState4 = 3
    gkLpConsumed4 = 1
  elseif (gkLpState4 == 3) then
    gkLpState4 = 2
    gkLpConsumed4 = 1
  endif
endif
if (gkLpPressT5 >= 0 && gkLpConsumed5 < 0.5 && gkLpDblFired5 < 0.5 && kLpNowW - gkLpPressT5 >= 0.4) then
  if (gkLpState5 == 2) then
    gkLpState5 = 3
    gkLpConsumed5 = 1
  elseif (gkLpState5 == 3) then
    gkLpState5 = 2
    gkLpConsumed5 = 1
  endif
endif

; speed combo <-> gkLpSpIdxN, bidirectional (widget writes the
; global; any global change — incl. the reset-to-6 gestures above —
; syncs the widget back so the combo never goes stale).
kLpSpW1 chnget "lp1_speed"
kLpSpWC1 changed2 kLpSpW1
if (kLpSpWC1 == 1) then
  gkLpSpIdx1 = kLpSpW1 - 1
endif
kLpSpGC1 changed2 gkLpSpIdx1
if (kLpSpGC1 == 1) then
  cabbageSetValue "lp1_speed", gkLpSpIdx1 + 1, k(1)
endif
kLpSpW2 chnget "lp2_speed"
kLpSpWC2 changed2 kLpSpW2
if (kLpSpWC2 == 1) then
  gkLpSpIdx2 = kLpSpW2 - 1
endif
kLpSpGC2 changed2 gkLpSpIdx2
if (kLpSpGC2 == 1) then
  cabbageSetValue "lp2_speed", gkLpSpIdx2 + 1, k(1)
endif
kLpSpW3 chnget "lp3_speed"
kLpSpWC3 changed2 kLpSpW3
if (kLpSpWC3 == 1) then
  gkLpSpIdx3 = kLpSpW3 - 1
endif
kLpSpGC3 changed2 gkLpSpIdx3
if (kLpSpGC3 == 1) then
  cabbageSetValue "lp3_speed", gkLpSpIdx3 + 1, k(1)
endif
kLpSpW4 chnget "lp4_speed"
kLpSpWC4 changed2 kLpSpW4
if (kLpSpWC4 == 1) then
  gkLpSpIdx4 = kLpSpW4 - 1
endif
kLpSpGC4 changed2 gkLpSpIdx4
if (kLpSpGC4 == 1) then
  cabbageSetValue "lp4_speed", gkLpSpIdx4 + 1, k(1)
endif
kLpSpW5 chnget "lp5_speed"
kLpSpWC5 changed2 kLpSpW5
if (kLpSpWC5 == 1) then
  gkLpSpIdx5 = kLpSpW5 - 1
endif
kLpSpGC5 changed2 gkLpSpIdx5
if (kLpSpGC5 == 1) then
  cabbageSetValue "lp5_speed", gkLpSpIdx5 + 1, k(1)
endif

; per-channel state+speed label, ~10 Hz
kLpUiTick metro 10
if (kLpUiTick == 1) then
  if (gkLpState1 == 0) then
    Slp1st strcpy "IDLE"
  elseif (gkLpState1 == 1) then
    Slp1st strcpy "REC"
  elseif (gkLpState1 == 2) then
    Slp1st strcpy "PLAY"
  elseif (gkLpState1 == 3) then
    Slp1st strcpy "OVD"
  elseif (gkLpState1 == 4) then
    Slp1st strcpy "STOP"
  elseif (gkLpState1 == 5) then
    Slp1st strcpy "SYNC"
  elseif (gkLpState1 == 6) then
    Slp1st strcpy "TRIM"
  endif
  if (gkLpSpIdx1 == 0) then
    Slp1sp strcpy "-2x"
  elseif (gkLpSpIdx1 == 1) then
    Slp1sp strcpy "-1x"
  elseif (gkLpSpIdx1 == 2) then
    Slp1sp strcpy "-1/2"
  elseif (gkLpSpIdx1 == 3) then
    Slp1sp strcpy "-1/4"
  elseif (gkLpSpIdx1 == 4) then
    Slp1sp strcpy "+1/4"
  elseif (gkLpSpIdx1 == 5) then
    Slp1sp strcpy "+1/2"
  elseif (gkLpSpIdx1 == 6) then
    Slp1sp strcpy "+1x"
  elseif (gkLpSpIdx1 == 7) then
    Slp1sp strcpy "+2x"
  endif
  cabbageSet k(1), "lp1_state", sprintfk("text(\"%s %s\")", Slp1st, Slp1sp)
  if (gkLpState2 == 0) then
    Slp2st strcpy "IDLE"
  elseif (gkLpState2 == 1) then
    Slp2st strcpy "REC"
  elseif (gkLpState2 == 2) then
    Slp2st strcpy "PLAY"
  elseif (gkLpState2 == 3) then
    Slp2st strcpy "OVD"
  elseif (gkLpState2 == 4) then
    Slp2st strcpy "STOP"
  elseif (gkLpState2 == 5) then
    Slp2st strcpy "SYNC"
  elseif (gkLpState2 == 6) then
    Slp2st strcpy "TRIM"
  endif
  if (gkLpSpIdx2 == 0) then
    Slp2sp strcpy "-2x"
  elseif (gkLpSpIdx2 == 1) then
    Slp2sp strcpy "-1x"
  elseif (gkLpSpIdx2 == 2) then
    Slp2sp strcpy "-1/2"
  elseif (gkLpSpIdx2 == 3) then
    Slp2sp strcpy "-1/4"
  elseif (gkLpSpIdx2 == 4) then
    Slp2sp strcpy "+1/4"
  elseif (gkLpSpIdx2 == 5) then
    Slp2sp strcpy "+1/2"
  elseif (gkLpSpIdx2 == 6) then
    Slp2sp strcpy "+1x"
  elseif (gkLpSpIdx2 == 7) then
    Slp2sp strcpy "+2x"
  endif
  cabbageSet k(1), "lp2_state", sprintfk("text(\"%s %s\")", Slp2st, Slp2sp)
  if (gkLpState3 == 0) then
    Slp3st strcpy "IDLE"
  elseif (gkLpState3 == 1) then
    Slp3st strcpy "REC"
  elseif (gkLpState3 == 2) then
    Slp3st strcpy "PLAY"
  elseif (gkLpState3 == 3) then
    Slp3st strcpy "OVD"
  elseif (gkLpState3 == 4) then
    Slp3st strcpy "STOP"
  elseif (gkLpState3 == 5) then
    Slp3st strcpy "SYNC"
  elseif (gkLpState3 == 6) then
    Slp3st strcpy "TRIM"
  endif
  if (gkLpSpIdx3 == 0) then
    Slp3sp strcpy "-2x"
  elseif (gkLpSpIdx3 == 1) then
    Slp3sp strcpy "-1x"
  elseif (gkLpSpIdx3 == 2) then
    Slp3sp strcpy "-1/2"
  elseif (gkLpSpIdx3 == 3) then
    Slp3sp strcpy "-1/4"
  elseif (gkLpSpIdx3 == 4) then
    Slp3sp strcpy "+1/4"
  elseif (gkLpSpIdx3 == 5) then
    Slp3sp strcpy "+1/2"
  elseif (gkLpSpIdx3 == 6) then
    Slp3sp strcpy "+1x"
  elseif (gkLpSpIdx3 == 7) then
    Slp3sp strcpy "+2x"
  endif
  cabbageSet k(1), "lp3_state", sprintfk("text(\"%s %s\")", Slp3st, Slp3sp)
  if (gkLpState4 == 0) then
    Slp4st strcpy "IDLE"
  elseif (gkLpState4 == 1) then
    Slp4st strcpy "REC"
  elseif (gkLpState4 == 2) then
    Slp4st strcpy "PLAY"
  elseif (gkLpState4 == 3) then
    Slp4st strcpy "OVD"
  elseif (gkLpState4 == 4) then
    Slp4st strcpy "STOP"
  elseif (gkLpState4 == 5) then
    Slp4st strcpy "SYNC"
  elseif (gkLpState4 == 6) then
    Slp4st strcpy "TRIM"
  endif
  if (gkLpSpIdx4 == 0) then
    Slp4sp strcpy "-2x"
  elseif (gkLpSpIdx4 == 1) then
    Slp4sp strcpy "-1x"
  elseif (gkLpSpIdx4 == 2) then
    Slp4sp strcpy "-1/2"
  elseif (gkLpSpIdx4 == 3) then
    Slp4sp strcpy "-1/4"
  elseif (gkLpSpIdx4 == 4) then
    Slp4sp strcpy "+1/4"
  elseif (gkLpSpIdx4 == 5) then
    Slp4sp strcpy "+1/2"
  elseif (gkLpSpIdx4 == 6) then
    Slp4sp strcpy "+1x"
  elseif (gkLpSpIdx4 == 7) then
    Slp4sp strcpy "+2x"
  endif
  cabbageSet k(1), "lp4_state", sprintfk("text(\"%s %s\")", Slp4st, Slp4sp)
  if (gkLpState5 == 0) then
    Slp5st strcpy "IDLE"
  elseif (gkLpState5 == 1) then
    Slp5st strcpy "REC"
  elseif (gkLpState5 == 2) then
    Slp5st strcpy "PLAY"
  elseif (gkLpState5 == 3) then
    Slp5st strcpy "OVD"
  elseif (gkLpState5 == 4) then
    Slp5st strcpy "STOP"
  elseif (gkLpState5 == 5) then
    Slp5st strcpy "SYNC"
  elseif (gkLpState5 == 6) then
    Slp5st strcpy "TRIM"
  endif
  if (gkLpSpIdx5 == 0) then
    Slp5sp strcpy "-2x"
  elseif (gkLpSpIdx5 == 1) then
    Slp5sp strcpy "-1x"
  elseif (gkLpSpIdx5 == 2) then
    Slp5sp strcpy "-1/2"
  elseif (gkLpSpIdx5 == 3) then
    Slp5sp strcpy "-1/4"
  elseif (gkLpSpIdx5 == 4) then
    Slp5sp strcpy "+1/4"
  elseif (gkLpSpIdx5 == 5) then
    Slp5sp strcpy "+1/2"
  elseif (gkLpSpIdx5 == 6) then
    Slp5sp strcpy "+1x"
  elseif (gkLpSpIdx5 == 7) then
    Slp5sp strcpy "+2x"
  endif
  cabbageSet k(1), "lp5_state", sprintfk("text(\"%s %s\")", Slp5st, Slp5sp)
endif

; ---------- drums (looper sample mode; browse targets the drum
; channel, or ch1 while none is loaded) ----------
gkDrumReqCh = (gkDrumChan >= 1 ? gkDrumChan : 1)
kB_drum_prev chnget "drum_prev"
kP_drum_prev init 0
if (kB_drum_prev > 0.5 && kP_drum_prev < 0.5) then
  gkDrumStep = gkDrumStep + 1
endif
kP_drum_prev = kB_drum_prev
kB_drum_next chnget "drum_next"
kP_drum_next init 0
if (kB_drum_next > 0.5 && kP_drum_next < 0.5) then
  gkDrumStep = gkDrumStep + 1
endif
kP_drum_next = kB_drum_next
kB_drum_play chnget "drum_play"
kP_drum_play init 0
if (kB_drum_play > 0.5 && kP_drum_play < 0.5) then
  gkDrumTap = 1
endif
kP_drum_play = kB_drum_play
kB_drum_clear chnget "drum_clear"
kP_drum_clear init 0
if (kB_drum_clear > 0.5 && kP_drum_clear < 0.5) then
  gkDrumClear = 1
endif
kP_drum_clear = kB_drum_clear
kDrTick metro 4
if (kDrTick == 1) then
  SDrNm chngetks "drum_file"
  if (gkDrumIdx >= 0) then
    cabbageSet k(1), "drum_lbl", sprintfk("text(\"%d/%d %s\")", gkDrumIdx + 1, gkDrumCount, SDrNm)
  else
    cabbageSet k(1), "drum_lbl", "text(\"-\")"
  endif
endif

; ---------- parameter panel switching ----------
if (kSelChg == 1) then
  cabbageSet k(1), "pnl_none", "visible(0)"
  cabbageSet k(1), "pnl_vol", sprintfk("visible(%d)", kSel == 0 ? 1 : 0)
  cabbageSet k(1), "pnl_atk", sprintfk("visible(%d)", kSel == 1 ? 1 : 0)
  cabbageSet k(1), "pnl_les", sprintfk("visible(%d)", kSel == 2 ? 1 : 0)
  cabbageSet k(1), "pnl_rm", sprintfk("visible(%d)", kSel == 3 ? 1 : 0)
  cabbageSet k(1), "pnl_tape", sprintfk("visible(%d)", kSel == 4 ? 1 : 0)
  cabbageSet k(1), "pnl_fms", sprintfk("visible(%d)", kSel == 5 ? 1 : 0)
  cabbageSet k(1), "pnl_fmt", sprintfk("visible(%d)", kSel == 6 ? 1 : 0)
  cabbageSet k(1), "pnl_sdl", sprintfk("visible(%d)", kSel == 7 ? 1 : 0)
  cabbageSet k(1), "pnl_bin", sprintfk("visible(%d)", kSel == 8 ? 1 : 0)
  cabbageSet k(1), "pnl_bc", sprintfk("visible(%d)", kSel == 9 ? 1 : 0)
  cabbageSet k(1), "pnl_bsc", sprintfk("visible(%d)", kSel == 10 ? 1 : 0)
  cabbageSet k(1), "pnl_h90", sprintfk("visible(%d)", kSel == 11 ? 1 : 0)
  cabbageSet k(1), "pnl_ps", sprintfk("visible(%d)", kSel == 12 ? 1 : 0)
  cabbageSet k(1), "pnl_gps", sprintfk("visible(%d)", kSel == 13 ? 1 : 0)
  cabbageSet k(1), "pnl_pf", sprintfk("visible(%d)", kSel == 14 ? 1 : 0)
  cabbageSet k(1), "pnl_lsy", sprintfk("visible(%d)", kSel == 15 ? 1 : 0)
  cabbageSet k(1), "pnl_res", sprintfk("visible(%d)", kSel == 16 ? 1 : 0)
  cabbageSet k(1), "pnl_wah", sprintfk("visible(%d)", kSel == 17 ? 1 : 0)
  cabbageSet k(1), "pnl_mog", sprintfk("visible(%d)", kSel == 18 ? 1 : 0)
  cabbageSet k(1), "pnl_vib", sprintfk("visible(%d)", kSel == 19 ? 1 : 0)
  cabbageSet k(1), "pnl_cho", sprintfk("visible(%d)", kSel == 20 ? 1 : 0)
  cabbageSet k(1), "pnl_pha", sprintfk("visible(%d)", kSel == 21 ? 1 : 0)
  cabbageSet k(1), "pnl_trm", sprintfk("visible(%d)", kSel == 22 ? 1 : 0)
  cabbageSet k(1), "pnl_fla", sprintfk("visible(%d)", kSel == 23 ? 1 : 0)
  cabbageSet k(1), "pnl_od", sprintfk("visible(%d)", kSel == 24 ? 1 : 0)
  cabbageSet k(1), "pnl_mar", sprintfk("visible(%d)", kSel == 25 ? 1 : 0)
  cabbageSet k(1), "pnl_rmz", sprintfk("visible(%d)", kSel == 26 ? 1 : 0)
  cabbageSet k(1), "pnl_dhg", sprintfk("visible(%d)", kSel == 27 ? 1 : 0)
  cabbageSet k(1), "pnl_muf", sprintfk("visible(%d)", kSel == 28 ? 1 : 0)
  cabbageSet k(1), "pnl_pre", sprintfk("visible(%d)", kSel == 29 ? 1 : 0)
  cabbageSet k(1), "pnl_bpl", sprintfk("visible(%d)", kSel == 30 ? 1 : 0)
  cabbageSet k(1), "pnl_bph", sprintfk("visible(%d)", kSel == 31 ? 1 : 0)
  cabbageSet k(1), "pnl_bds", sprintfk("visible(%d)", kSel == 32 ? 1 : 0)
  cabbageSet k(1), "pnl_cmp", sprintfk("visible(%d)", kSel == 33 ? 1 : 0)
  cabbageSet k(1), "pnl_dly", sprintfk("visible(%d)", kSel == 34 ? 1 : 0)
  cabbageSet k(1), "pnl_tdl", sprintfk("visible(%d)", kSel == 35 ? 1 : 0)
  cabbageSet k(1), "pnl_mvd", sprintfk("visible(%d)", kSel == 36 ? 1 : 0)
  cabbageSet k(1), "pnl_neb", sprintfk("visible(%d)", kSel == 37 ? 1 : 0)
  cabbageSet k(1), "pnl_shm", sprintfk("visible(%d)", kSel == 38 ? 1 : 0)
  cabbageSet k(1), "pnl_frz", sprintfk("visible(%d)", kSel == 39 ? 1 : 0)
  cabbageSet k(1), "pnl_gvb", sprintfk("visible(%d)", kSel == 40 ? 1 : 0)
  cabbageSet k(1), "pnl_vrb", sprintfk("visible(%d)", kSel == 41 ? 1 : 0)
  cabbageSet k(1), "pnl_grn", sprintfk("visible(%d)", kSel == 42 ? 1 : 0)
  cabbageSet k(1), "pnl_mod", sprintfk("visible(%d)", kSel == 43 ? 1 : 0)
  cabbageSet k(1), "pnl_oct", sprintfk("visible(%d)", kSel == 44 ? 1 : 0)
  cabbageSet k(1), "pnl_env", sprintfk("visible(%d)", kSel == 45 ? 1 : 0)
  cabbageSet k(1), "pnl_tsy", sprintfk("visible(%d)", kSel == 46 ? 1 : 0)
  cabbageSet k(1), "pnl_pll", sprintfk("visible(%d)", kSel == 47 ? 1 : 0)
  cabbageSet k(1), "pnl_teq", sprintfk("visible(%d)", kSel == 48 ? 1 : 0)
  cabbageSet k(1), "pnl_fbk", sprintfk("visible(%d)", kSel == 49 ? 1 : 0)
  cabbageSet k(1), "pnl_dif", sprintfk("visible(%d)", kSel == 50 ? 1 : 0)
  cabbageSet k(1), "pnl_cfb", sprintfk("visible(%d)", kSel == 51 ? 1 : 0)
  cabbageSet k(1), "pnl_blp", sprintfk("visible(%d)", kSel == 52 ? 1 : 0)
  cabbageSet k(1), "pnl_onw", sprintfk("visible(%d)", kSel == 53 ? 1 : 0)
  cabbageSet k(1), "pnl_hab", sprintfk("visible(%d)", kSel == 54 ? 1 : 0)
  cabbageSet k(1), "pnl_md2", sprintfk("visible(%d)", kSel == 55 ? 1 : 0)
  cabbageSet k(1), "pnl_imp", sprintfk("visible(%d)", kSel == 56 ? 1 : 0)
  cabbageSet k(1), "pnl_bbc", sprintfk("visible(%d)", kSel == 57 ? 1 : 0)
  cabbageSet k(1), "pnl_rpt", sprintfk("visible(%d)", kSel == 58 ? 1 : 0)
  cabbageSet k(1), "pnl_chp", sprintfk("visible(%d)", kSel == 59 ? 1 : 0)
  cabbageSet k(1), "pnl_rvs", sprintfk("visible(%d)", kSel == 60 ? 1 : 0)
  cabbageSet k(1), "pnl_nam", sprintfk("visible(%d)", kSel == 61 ? 1 : 0)
  cabbageSet k(1), "pnl_ir", sprintfk("visible(%d)", kSel == 62 ? 1 : 0)
endif

#ifdef STATE_RESTORE_WIDGETS
; ---------- settings restore -> widgets (standalone app) ----------
kWRst init 0
if (kWRst == 0 && kInit == 0 && gkNeptrStLoaded == 1) then
  kWRst = 1
  cabbageSetValue "on_vol", (tab:k(0, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "vol_level", tab:k(1, giNeptrState), k(1)
  cabbageSetValue "on_atk", (tab:k(2, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "atk_mix", tab:k(3, giNeptrState), k(1)
  cabbageSetValue "atk_time", tab:k(4, giNeptrState), k(1)
  cabbageSetValue "atk_sens", tab:k(5, giNeptrState), k(1)
  cabbageSetValue "atk_shape", tab:k(6, giNeptrState), k(1)
  cabbageSetValue "atk_range", tab:k(7, giNeptrState), k(1)
  cabbageSetValue "atk_retrig", tab:k(8, giNeptrState), k(1)
  cabbageSetValue "on_les", (tab:k(9, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "les_speed", tab:k(10, giNeptrState), k(1)
  cabbageSetValue "les_depth", tab:k(11, giNeptrState), k(1)
  cabbageSetValue "on_rm", (tab:k(12, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "rm_depth", tab:k(13, giNeptrState), k(1)
  cabbageSetValue "rm_freq", tab:k(14, giNeptrState), k(1)
  cabbageSetValue "on_tape", (tab:k(15, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "tape_amt", tab:k(16, giNeptrState), k(1)
  cabbageSetValue "tape_fail", tab:k(17, giNeptrState), k(1)
  cabbageSetValue "tape_model", tab:k(18, giNeptrState), k(1)
  cabbageSetValue "on_fms", (tab:k(19, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "fms_mix", tab:k(20, giNeptrState), k(1)
  cabbageSetValue "fms_shift", tab:k(21, giNeptrState), k(1)
  cabbageSetValue "on_fmt", (tab:k(22, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "fmt_mix", tab:k(23, giNeptrState), k(1)
  cabbageSetValue "fmt_shift", tab:k(24, giNeptrState), k(1)
  cabbageSetValue "on_sdl", (tab:k(25, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "sdl_mix", tab:k(26, giNeptrState), k(1)
  cabbageSetValue "sdl_maxdel", tab:k(27, giNeptrState), k(1)
  cabbageSetValue "sdl_fback", tab:k(28, giNeptrState), k(1)
  cabbageSetValue "on_bin", (tab:k(29, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bin_pedal", tab:k(30, giNeptrState), k(1)
  cabbageSetValue "on_bc", (tab:k(31, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bc_pedal", tab:k(32, giNeptrState), k(1)
  cabbageSetValue "on_bsc", (tab:k(33, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bsc_mix", tab:k(34, giNeptrState), k(1)
  cabbageSetValue "bsc_scatter", tab:k(35, giNeptrState), k(1)
  cabbageSetValue "bsc_fback", tab:k(36, giNeptrState), k(1)
  cabbageSetValue "bsc_window", tab:k(37, giNeptrState), k(1)
  cabbageSetValue "on_h90", (tab:k(38, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "h90_mix", tab:k(39, giNeptrState), k(1)
  cabbageSetValue "h90_semi", tab:k(40, giNeptrState), k(1)
  cabbageSetValue "on_ps", (tab:k(41, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ps_mix", tab:k(42, giNeptrState), k(1)
  cabbageSetValue "ps_semi", tab:k(43, giNeptrState), k(1)
  cabbageSetValue "on_gps", (tab:k(44, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "gps_mix", tab:k(45, giNeptrState), k(1)
  cabbageSetValue "gps_semi", tab:k(46, giNeptrState), k(1)
  cabbageSetValue "gps_fback", tab:k(47, giNeptrState), k(1)
  cabbageSetValue "gps_dly", tab:k(48, giNeptrState), k(1)
  cabbageSetValue "on_pf", (tab:k(49, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "pf_mix", tab:k(50, giNeptrState), k(1)
  cabbageSetValue "pf_semi", tab:k(51, giNeptrState), k(1)
  cabbageSetValue "on_lsy", (tab:k(52, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "lsy_mix", tab:k(53, giNeptrState), k(1)
  cabbageSetValue "lsy_loss", tab:k(54, giNeptrState), k(1)
  cabbageSetValue "lsy_bw", tab:k(55, giNeptrState), k(1)
  cabbageSetValue "lsy_pkt", tab:k(56, giNeptrState), k(1)
  cabbageSetValue "on_res", (tab:k(57, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "res_mix", tab:k(58, giNeptrState), k(1)
  cabbageSetValue "res_onset", tab:k(59, giNeptrState), k(1)
  cabbageSetValue "res_fb", tab:k(60, giNeptrState), k(1)
  cabbageSetValue "res_oct", tab:k(61, giNeptrState), k(1)
  cabbageSetValue "res_frz", tab:k(62, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "on_wah", (tab:k(63, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "wah_freq", tab:k(64, giNeptrState), k(1)
  cabbageSetValue "wah_res", tab:k(65, giNeptrState), k(1)
  cabbageSetValue "on_mog", (tab:k(66, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "mog_sens", tab:k(67, giNeptrState), k(1)
  cabbageSetValue "mog_res", tab:k(68, giNeptrState), k(1)
  cabbageSetValue "mog_type", (tab:k(69, giNeptrState) - 1) / 1 + 1, k(1)
  cabbageSetValue "mog_level", tab:k(70, giNeptrState), k(1)
  cabbageSetValue "on_vib", (tab:k(71, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "vib_depth", tab:k(72, giNeptrState) / 0.005, k(1)
  cabbageSetValue "vib_rate", tab:k(73, giNeptrState), k(1)
  cabbageSetValue "vib_fback", tab:k(74, giNeptrState), k(1)
  cabbageSetValue "on_cho", (tab:k(75, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "cho_mix", tab:k(76, giNeptrState), k(1)
  cabbageSetValue "cho_rate", tab:k(77, giNeptrState), k(1)
  cabbageSetValue "cho_depth", tab:k(78, giNeptrState), k(1)
  cabbageSetValue "on_pha", (tab:k(79, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "pha_depth", tab:k(80, giNeptrState), k(1)
  cabbageSetValue "pha_rate", tab:k(81, giNeptrState), k(1)
  cabbageSetValue "pha_fback", tab:k(82, giNeptrState), k(1)
  cabbageSetValue "pha_stages", tab:k(83, giNeptrState), k(1)
  cabbageSetValue "on_trm", (tab:k(84, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "trm_depth", tab:k(85, giNeptrState), k(1)
  cabbageSetValue "trm_mode", tab:k(86, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "trm_rate", tab:k(87, giNeptrState), k(1)
  cabbageSetValue "on_fla", (tab:k(88, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "fla_depth", tab:k(89, giNeptrState) / 0.01, k(1)
  cabbageSetValue "fla_rate", tab:k(90, giNeptrState), k(1)
  cabbageSetValue "fla_fback", tab:k(91, giNeptrState), k(1)
  cabbageSetValue "on_od", (tab:k(92, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "od_drive", tab:k(93, giNeptrState), k(1)
  cabbageSetValue "od_tone", tab:k(94, giNeptrState), k(1)
  cabbageSetValue "od_morph", tab:k(95, giNeptrState), k(1)
  cabbageSetValue "od_blend", tab:k(96, giNeptrState), k(1)
  cabbageSetValue "on_mar", (tab:k(97, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "mar_drive", tab:k(98, giNeptrState), k(1)
  cabbageSetValue "mar_tone", tab:k(99, giNeptrState), k(1)
  cabbageSetValue "mar_pres", tab:k(100, giNeptrState), k(1)
  cabbageSetValue "mar_tight", tab:k(101, giNeptrState), k(1)
  cabbageSetValue "on_rmz", (tab:k(102, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "rmz_drive", tab:k(103, giNeptrState), k(1)
  cabbageSetValue "rmz_tone", tab:k(104, giNeptrState), k(1)
  cabbageSetValue "rmz_midf", tab:k(105, giNeptrState), k(1)
  cabbageSetValue "rmz_midg", tab:k(106, giNeptrState), k(1)
  cabbageSetValue "on_dhg", (tab:k(107, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "dhg_drive", tab:k(108, giNeptrState), k(1)
  cabbageSetValue "dhg_tone", tab:k(109, giNeptrState), k(1)
  cabbageSetValue "dhg_blend", tab:k(110, giNeptrState), k(1)
  cabbageSetValue "dhg_tight", tab:k(111, giNeptrState), k(1)
  cabbageSetValue "on_muf", (tab:k(112, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "muf_drive", tab:k(113, giNeptrState), k(1)
  cabbageSetValue "muf_tone", tab:k(114, giNeptrState), k(1)
  cabbageSetValue "muf_mids", tab:k(115, giNeptrState), k(1)
  cabbageSetValue "muf_voice", tab:k(116, giNeptrState), k(1)
  cabbageSetValue "on_pre", (tab:k(117, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "on_bpl", (tab:k(118, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bpl_drive", tab:k(119, giNeptrState), k(1)
  cabbageSetValue "on_bph", (tab:k(120, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bph_drive", tab:k(121, giNeptrState), k(1)
  cabbageSetValue "on_bds", (tab:k(122, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bds_drive", tab:k(123, giNeptrState), k(1)
  cabbageSetValue "bds_blend", tab:k(124, giNeptrState), k(1)
  cabbageSetValue "bds_tone", tab:k(125, giNeptrState), k(1)
  cabbageSetValue "on_cmp", (tab:k(126, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "cmp_amt", tab:k(127, giNeptrState), k(1)
  cabbageSetValue "cmp_speed", tab:k(128, giNeptrState), k(1)
  cabbageSetValue "cmp_makeup", tab:k(129, giNeptrState), k(1)
  cabbageSetValue "cmp_blend", tab:k(130, giNeptrState), k(1)
  cabbageSetValue "cmp_gate", tab:k(131, giNeptrState), k(1)
  cabbageSetValue "on_dly", (tab:k(132, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "dly_mix", tab:k(133, giNeptrState), k(1)
  cabbageSetValue "dly_time", tab:k(134, giNeptrState) / 1000, k(1)
  cabbageSetValue "dly_fback", tab:k(135, giNeptrState), k(1)
  cabbageSetValue "dly_semi", tab:k(136, giNeptrState), k(1)
  cabbageSetValue "dly_mode", tab:k(137, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "on_tdl", (tab:k(138, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "tdl_mix", tab:k(139, giNeptrState), k(1)
  cabbageSetValue "tdl_time", tab:k(140, giNeptrState), k(1)
  cabbageSetValue "tdl_fback", tab:k(141, giNeptrState), k(1)
  cabbageSetValue "tdl_fctr", tab:k(142, giNeptrState), k(1)
  cabbageSetValue "tdl_fq", tab:k(143, giNeptrState), k(1)
  cabbageSetValue "tdl_head", (tab:k(144, giNeptrState) - 1) / 1 + 1, k(1)
  cabbageSetValue "on_mvd", (tab:k(145, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "mvd_numl", tab:k(146, giNeptrState), k(1)
  cabbageSetValue "mvd_denl", tab:k(147, giNeptrState), k(1)
  cabbageSetValue "mvd_numr", tab:k(148, giNeptrState), k(1)
  cabbageSetValue "mvd_denr", tab:k(149, giNeptrState), k(1)
  cabbageSetValue "mvd_att", tab:k(150, giNeptrState), k(1)
  cabbageSetValue "mvd_dec", tab:k(151, giNeptrState), k(1)
  cabbageSetValue "mvd_reps", tab:k(152, giNeptrState), k(1)
  cabbageSetValue "mvd_intv", tab:k(153, giNeptrState), k(1)
  cabbageSetValue "on_neb", (tab:k(154, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "neb_mix", tab:k(155, giNeptrState), k(1)
  cabbageSetValue "neb_dens", tab:k(156, giNeptrState), k(1)
  cabbageSetValue "neb_size", tab:k(157, giNeptrState), k(1)
  cabbageSetValue "on_shm", (tab:k(158, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "shm_mix", tab:k(159, giNeptrState), k(1)
  cabbageSetValue "shm_intv", tab:k(160, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "shm_bloom", tab:k(161, giNeptrState), k(1)
  cabbageSetValue "on_frz", (tab:k(162, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "frz_bed", tab:k(163, giNeptrState), k(1)
  cabbageSetValue "frz_damp", tab:k(164, giNeptrState), k(1)
  cabbageSetValue "frz_listen", tab:k(165, giNeptrState), k(1)
  cabbageSetValue "on_gvb", (tab:k(166, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "gvb_mix", tab:k(167, giNeptrState), k(1)
  cabbageSetValue "gvb_spacing", tab:k(168, giNeptrState), k(1)
  cabbageSetValue "gvb_decay", tab:k(169, giNeptrState), k(1)
  cabbageSetValue "on_vrb", (tab:k(170, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "vrb_mix", tab:k(171, giNeptrState), k(1)
  cabbageSetValue "vrb_room", tab:k(172, giNeptrState), k(1)
  cabbageSetValue "vrb_damp", tab:k(173, giNeptrState), k(1)
  cabbageSetValue "on_grn", (tab:k(174, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "grn_mix", tab:k(175, giNeptrState), k(1)
  cabbageSetValue "grn_dens", tab:k(176, giNeptrState), k(1)
  cabbageSetValue "grn_size", tab:k(177, giNeptrState), k(1)
  cabbageSetValue "grn_pitch", tab:k(178, giNeptrState), k(1)
  cabbageSetValue "on_mod", (tab:k(179, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "mod_mix", tab:k(180, giNeptrState), k(1)
  cabbageSetValue "mod_clock", tab:k(181, giNeptrState), k(1)
  cabbageSetValue "mod_dens", tab:k(182, giNeptrState), k(1)
  cabbageSetValue "mod_dly", tab:k(183, giNeptrState), k(1)
  cabbageSetValue "on_oct", (tab:k(184, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "oct_sub", tab:k(185, giNeptrState), k(1)
  cabbageSetValue "oct_tone", tab:k(186, giNeptrState), k(1)
  cabbageSetValue "oct_char", tab:k(187, giNeptrState), k(1)
  cabbageSetValue "on_env", (tab:k(188, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "env_wet", tab:k(189, giNeptrState), k(1)
  cabbageSetValue "env_sens", tab:k(190, giNeptrState), k(1)
  cabbageSetValue "env_mode", tab:k(191, giNeptrState), k(1)
  cabbageSetValue "env_att", tab:k(192, giNeptrState), k(1)
  cabbageSetValue "env_dec", tab:k(193, giNeptrState), k(1)
  cabbageSetValue "on_tsy", (tab:k(194, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "tsy_mix", tab:k(195, giNeptrState), k(1)
  cabbageSetValue "tsy_cut", tab:k(196, giNeptrState), k(1)
  cabbageSetValue "tsy_res", tab:k(197, giNeptrState), k(1)
  cabbageSetValue "tsy_glide", tab:k(198, giNeptrState), k(1)
  cabbageSetValue "tsy_sub", tab:k(199, giNeptrState), k(1)
  cabbageSetValue "tsy_det", tab:k(200, giNeptrState), k(1)
  cabbageSetValue "on_pll", (tab:k(201, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "pll_mix", tab:k(202, giNeptrState), k(1)
  cabbageSetValue "pll_intv", tab:k(203, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "pll_lock", tab:k(204, giNeptrState), k(1)
  cabbageSetValue "pll_vmix", tab:k(205, giNeptrState), k(1)
  cabbageSetValue "on_teq", (tab:k(206, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "teq_mid", tab:k(207, giNeptrState), k(1)
  cabbageSetValue "teq_freq", tab:k(208, giNeptrState), k(1)
  cabbageSetValue "teq_tilt", tab:k(209, giNeptrState), k(1)
  cabbageSetValue "teq_low", tab:k(210, giNeptrState), k(1)
  cabbageSetValue "teq_high", tab:k(211, giNeptrState), k(1)
  cabbageSetValue "on_fbk", (tab:k(212, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "fbk_level", tab:k(213, giNeptrState), k(1)
  cabbageSetValue "fbk_harm", tab:k(214, giNeptrState) / 0.25 + 1, k(1)
  cabbageSetValue "fbk_bloom", tab:k(215, giNeptrState), k(1)
  cabbageSetValue "fbk_vib", tab:k(216, giNeptrState), k(1)
  cabbageSetValue "on_dif", (tab:k(217, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "dif_wet", tab:k(218, giNeptrState), k(1)
  cabbageSetValue "dif_depth", tab:k(219, giNeptrState), k(1)
  cabbageSetValue "dif_rate", tab:k(220, giNeptrState), k(1)
  cabbageSetValue "dif_duck", tab:k(221, giNeptrState), k(1)
  cabbageSetValue "on_cfb", (tab:k(222, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "cfb_ret", tab:k(223, giNeptrState), k(1)
  cabbageSetValue "cfb_gain", tab:k(224, giNeptrState), k(1)
  cabbageSetValue "cfb_tone", tab:k(225, giNeptrState), k(1)
  cabbageSetValue "cfb_ceil", tab:k(226, giNeptrState), k(1)
  cabbageSetValue "on_blp", (tab:k(227, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "blp_motion", tab:k(228, giNeptrState), k(1)
  cabbageSetValue "blp_scat", tab:k(229, giNeptrState), k(1)
  cabbageSetValue "blp_size", tab:k(230, giNeptrState), k(1)
  cabbageSetValue "blp_char", tab:k(231, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "on_onw", (tab:k(232, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "onw_mix", tab:k(233, giNeptrState), k(1)
  cabbageSetValue "onw_blend", tab:k(234, giNeptrState), k(1)
  cabbageSetValue "onw_size", tab:k(235, giNeptrState), k(1)
  cabbageSetValue "onw_follow", tab:k(236, giNeptrState), k(1)
  cabbageSetValue "onw_density", tab:k(237, giNeptrState), k(1)
  cabbageSetValue "onw_err", tab:k(238, giNeptrState), k(1)
  cabbageSetValue "onw_oct", tab:k(239, giNeptrState), k(1)
  cabbageSetValue "on_hab", (tab:k(240, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "hab_mix", tab:k(241, giNeptrState), k(1)
  cabbageSetValue "hab_size", tab:k(242, giNeptrState), k(1)
  cabbageSetValue "hab_repeats", tab:k(243, giNeptrState), k(1)
  cabbageSetValue "hab_blend", tab:k(244, giNeptrState), k(1)
  cabbageSetValue "hab_rec", tab:k(245, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "on_md2", (tab:k(246, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "md2_mix", tab:k(247, giNeptrState), k(1)
  cabbageSetValue "md2_clock", tab:k(248, giNeptrState), k(1)
  cabbageSetValue "md2_length", tab:k(249, giNeptrState), k(1)
  cabbageSetValue "md2_gen", tab:k(250, giNeptrState), k(1)
  cabbageSetValue "on_imp", (tab:k(251, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "imp_mix", tab:k(252, giNeptrState), k(1)
  cabbageSetValue "imp_prob", tab:k(253, giNeptrState), k(1)
  cabbageSetValue "imp_dec", tab:k(254, giNeptrState), k(1)
  cabbageSetValue "imp_tone", tab:k(255, giNeptrState), k(1)
  cabbageSetValue "on_bbc", (tab:k(256, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "bbc_mix", tab:k(257, giNeptrState), k(1)
  cabbageSetValue "bbc_prob", tab:k(258, giNeptrState), k(1)
  cabbageSetValue "bbc_flt", tab:k(259, giNeptrState), k(1)
  cabbageSetValue "on_rpt", (tab:k(260, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "rpt_mix", tab:k(261, giNeptrState), k(1)
  cabbageSetValue "rpt_prob", tab:k(262, giNeptrState), k(1)
  cabbageSetValue "rpt_char", tab:k(263, giNeptrState), k(1)
  cabbageSetValue "rpt_len", tab:k(264, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "on_chp", (tab:k(265, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "chp_depth", tab:k(266, giNeptrState), k(1)
  cabbageSetValue "chp_prob", tab:k(267, giNeptrState), k(1)
  cabbageSetValue "chp_rate", tab:k(268, giNeptrState), k(1)
  cabbageSetValue "chp_shape", tab:k(269, giNeptrState), k(1)
  cabbageSetValue "on_rvs", (tab:k(270, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "rvs_mix", tab:k(271, giNeptrState), k(1)
  cabbageSetValue "rvs_prob", tab:k(272, giNeptrState), k(1)
  cabbageSetValue "rvs_len", tab:k(273, giNeptrState), k(1)
  cabbageSetValue "nam_in_gain", tab:k(276, giNeptrState), k(1)
  cabbageSetValue "nam_out_gain", tab:k(277, giNeptrState), k(1)
  cabbageSetValue "nam_sel0", tab:k(278, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "nam_sel1", tab:k(279, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "nam_sel2", tab:k(280, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "nam_sel3", tab:k(281, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "nam_sel4", tab:k(282, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_damp", tab:k(285, giNeptrState), k(1)
  cabbageSetValue "ir_level", tab:k(286, giNeptrState), k(1)
  cabbageSetValue "ir_sel0", tab:k(287, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_mix0", tab:k(288, giNeptrState), k(1)
  cabbageSetValue "ir_sel1", tab:k(289, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_mix1", tab:k(290, giNeptrState), k(1)
  cabbageSetValue "ir_sel2", tab:k(291, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_mix2", tab:k(292, giNeptrState), k(1)
  cabbageSetValue "ir_sel3", tab:k(293, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_mix3", tab:k(294, giNeptrState), k(1)
  cabbageSetValue "ir_sel4", tab:k(295, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "ir_mix4", tab:k(296, giNeptrState), k(1)
  cabbageSetValue "stereo_in", tab:k(297, giNeptrState), k(1)
  cabbageSetValue "lp1_speed", tab:k(299, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "lp2_speed", tab:k(300, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "lp3_speed", tab:k(301, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "lp4_speed", tab:k(302, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "lp5_speed", tab:k(303, giNeptrState) / 1 + 1, k(1)
  cabbageSetValue "nam_row0", (tab:k(274, giNeptrState) == 0 && tab:k(275, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ir_row0", (tab:k(283, giNeptrState) == 0 && tab:k(284, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "nam_row1", (tab:k(274, giNeptrState) == 1 && tab:k(275, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ir_row1", (tab:k(283, giNeptrState) == 1 && tab:k(284, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "nam_row2", (tab:k(274, giNeptrState) == 2 && tab:k(275, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ir_row2", (tab:k(283, giNeptrState) == 2 && tab:k(284, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "nam_row3", (tab:k(274, giNeptrState) == 3 && tab:k(275, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ir_row3", (tab:k(283, giNeptrState) == 3 && tab:k(284, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "nam_row4", (tab:k(274, giNeptrState) == 4 && tab:k(275, giNeptrState) < 0.5 ? 1 : 0), k(1)
  cabbageSetValue "ir_row4", (tab:k(283, giNeptrState) == 4 && tab:k(284, giNeptrState) < 0.5 ? 1 : 0), k(1)
endif
#endif

#ifdef STATE_RESTORE_WIDGETS
; ---------- hot-reload blue edge-glow ----------
SMark sprintf "%s/.updated", "$STATE_DIR"
iUpd filevalid SMark
if (iUpd == 1) then
  SRmk sprintf "rm -f \"%s\"", SMark
  iRmk system_i 1, SRmk
endif
kGlow init iUpd
kGlowFps metro 60
if (kGlow > 0) then
  kGlow = kGlow - 1.6/kr
  kGlow = kGlow < 0 ? 0 : kGlow
  if (kGlowFps == 1 || kGlow == 0) then
    cabbageSet 1, "updGlow", sprintfk("outlineColour(80, 180, 255, %d)", int(255*kGlow))
  endif
endif
#endif

; ---------- expression pedal (last-toggled-wins) ----------
kEx chnget "expr"
kExC changed2 kEx
if (kExC == 1) then
  if (kExprTgt == 1) then
    gkVOLpedal = kEx
  elseif (kExprTgt == 2) then
    gkATTK_sens = kEx
  elseif (kExprTgt == 3) then
    gkSemi = kEx*24 - 12
  elseif (kExprTgt == 4) then
    gkGPSsemis = kEx*24 - 12
  elseif (kExprTgt == 5) then
    gkFormantShift = kEx*24 - 12
  elseif (kExprTgt == 8) then
    gkPHA_fback = kEx*0.9
  elseif (kExprTgt == 10) then
    gkLeslieSpeed = 0.1 * exp(log(100) * kEx)
  elseif (kExprTgt == 11) then
    gkARModFreq = 0.05 * exp(log(5000) * kEx)
  elseif (kExprTgt == 15) then
    gkDelayMS = kEx*700000
  elseif (kExprTgt == 17) then
    gkMOOD_Clock = 1 - kEx
  elseif (kExprTgt == 20) then
    gkBBcut_expr = kEx
  elseif (kExprTgt == 21) then
    gkTapeExpr = kEx
  elseif (kExprTgt == 22) then
    gkWAH_freq = kEx
  elseif (kExprTgt == 23) then
    gkRPT_expr = kEx
  elseif (kExprTgt == 24) then
    gkCHP_expr = kEx
  elseif (kExprTgt == 25) then
    gkRVS_expr = kEx
  elseif (kExprTgt == 26) then
    gkSemiH90 = kEx*24 - 12
  elseif (kExprTgt == 27) then
    gkSemiPF = kEx*24 - 12
  elseif (kExprTgt == 28) then
    gkFormsShift = kEx*24 - 12
  elseif (kExprTgt == 30) then
    gkMasterVol = kEx
  elseif (kExprTgt == 31) then
    gkOD_drive = kEx
  elseif (kExprTgt == 32) then
    gkMAR_drive = kEx
  elseif (kExprTgt == 33) then
    gkDHG_drive = kEx
  elseif (kExprTgt == 34) then
    gkRMZ_drive = kEx
  elseif (kExprTgt == 35) then
    gkMUF_drive = kEx
  elseif (kExprTgt == 36) then
    gkHabitScan = kEx
  elseif (kExprTgt == 38) then
    gkBinPedal = kEx
  elseif (kExprTgt == 42) then
    gkBDS_drive = kEx
  elseif (kExprTgt == 44) then
    gkBcPedal = kEx
  elseif (kExprTgt == 45) then
    gkONW_follow = kEx
  elseif (kExprTgt == 48) then
    gkIMP_expr = kEx
  elseif (kExprTgt == 56) then
    gkBLP_expr = kEx
  elseif (kExprTgt == 61) then
    gkTSY_cut = kEx
  elseif (kExprTgt == 63) then
    gkFBK_expr = kEx
  elseif (kExprTgt == 66) then
    gkMD2_Clock = kEx
  elseif (kExprTgt == 69) then
    gkMvdExpr = kEx
  elseif (kExprTgt == 70) then
    gkNebWarp = kEx
  elseif (kExprTgt == 72) then
    gkFrzExpr = kEx
  endif
endif

kInit = 0
endin

</CsInstruments>
<CsScore>
i98  0 z
i1   0 z
i209 0 z
i210 0 z
i211 0 z
i212 0 z
i213 0 z
i214 0 z
i216 0 z
i190 0 z
i193 0 z
i194 0 z
i195 0 z
</CsScore>
</CsoundSynthesizer>
