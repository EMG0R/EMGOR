;==============================================================
; state_preset_exclude_save.inc.orc  (HAND-MAINTAINED; no generator exists in the tree - was labelled GENERATED 2026-09-10)
; PRESET EXCLUSIONS — BLOOPER, LOOPER speeds + every GLITCH-menu effect (IMPULSE, BBCUT,
; REPEAT, CHOP, RVRS) are generative/performance effects: a preset RECALL must
; leave them EXACTLY as they currently are. This file is #included in the recall
; path in state_persist.orc: _save snapshots the live globals BEFORE the bank
; row overwrites giNeptrState + unpack; _restore writes them back AFTER unpack,
; so the excluded globals keep their pre-recall live values regardless of what
; the preset stored. Explicit, auditable list (global @ giNeptrState slot):
;==============================================================

            kEx_BLP_bypass = gkBLP_bypass   ; slot 225 — BLOOPER bypass
            kEx_BLP_Motion = gkBLP_Motion   ; slot 226 — BLOOPER motion
            kEx_BLP_Scat = gkBLP_Scat   ; slot 227 — BLOOPER scatter
            kEx_BLP_Size = gkBLP_Size   ; slot 228 — BLOOPER size
            kEx_BLP_Char = gkBLP_Char   ; slot 229 — BLOOPER character
            kEx_IMP_bypass = gkIMP_bypass   ; slot 249 — GLITCH IMPULSE bypass
            kEx_IMP_mix = gkIMP_mix   ; slot 250 — GLITCH IMPULSE mix
            kEx_IMP_prob = gkIMP_prob   ; slot 251 — GLITCH IMPULSE prob
            kEx_IMP_dec = gkIMP_dec   ; slot 252 — GLITCH IMPULSE decay
            kEx_IMP_tone = gkIMP_tone   ; slot 253 — GLITCH IMPULSE tone
            kEx_BBcut_bypass = gkBBcut_bypass   ; slot 254 — GLITCH BBCUT bypass
            kEx_BBcut_drywet = gkBBcut_drywet   ; slot 255 — GLITCH BBCUT drywet
            kEx_BBcut_prob = gkBBcut_prob   ; slot 256 — GLITCH BBCUT prob
            kEx_BBcut_fltMix = gkBBcut_fltMix   ; slot 257 — GLITCH BBCUT fltMix
            kEx_WGuide_bypass = gkWGuide_bypass   ; slot 258 — GLITCH REPEAT bypass (WGuide bus)
            kEx_WGuide_drywet = gkWGuide_drywet   ; slot 259 — GLITCH REPEAT drywet (WGuide bus)
            kEx_RPT_prob = gkRPT_prob   ; slot 260 — GLITCH REPEAT prob
            kEx_RPT_char = gkRPT_char   ; slot 261 — GLITCH REPEAT char
            kEx_RPT_len = gkRPT_len   ; slot 262 — GLITCH REPEAT len
            kEx_SqMod_bypass = gkSqMod_bypass   ; slot 263 — GLITCH CHOP bypass (SqMod bus)
            kEx_SqMod_drywet = gkSqMod_drywet   ; slot 264 — GLITCH CHOP drywet (SqMod bus)
            kEx_CHP_prob = gkCHP_prob   ; slot 265 — GLITCH CHOP prob
            kEx_CHP_rate = gkCHP_rate   ; slot 266 — GLITCH CHOP rate
            kEx_CHP_shape = gkCHP_shape   ; slot 267 — GLITCH CHOP shape
            kEx_FShift_bypass = gkFShift_bypass   ; slot 268 — GLITCH RVRS bypass (FShift bus)
            kEx_FShift_drywet = gkFShift_drywet   ; slot 269 — GLITCH RVRS drywet (FShift bus)
            kEx_RVS_prob = gkRVS_prob   ; slot 270 — GLITCH RVRS prob
            kEx_RVS_len = gkRVS_len   ; slot 271 — GLITCH RVRS len
            ; 2026-09-15 — NOT a generative effect; a DIFFERENT reason. This is
            ; the expression pedal's sticky owner. expr_cc29.orc's documented
            ; rule is LAST-TOGGLED-WINS, and it explicitly survives an effect
            ; being switched OFF — the pedal is live performance state, not
            ; preset state. The full-state bank broke that by restoring
            ; whatever owner was live at capture: every occupied gorcore slot
            ; had this at target 1 (VOLUME PEDAL), so recalling any foot preset
            ; silently stole the pedal from the menu-0 presets' declared
            ; target 30 (gkMasterVol, the reverb-swell stage).
            kEx_LastToggle29 = gkLastToggle29   ; slot 304 — expr pedal owner
            ; 2026-09-25 — WHICH MENU YOU ARE ON is live performance state, not
            ; preset state. state_unpack.inc.orc restores slot 302 into
            ; gkCurrentMenu, so every foot preset silently moved the rig to
            ; whatever menu was showing when the preset was captured — the UI
            ; could hide the page change but the engine (and the foot cycle)
            ; had genuinely moved. A recall must leave the menu where it is.
            kEx_CurrentMenu = gkCurrentMenu   ; slot 302 — current menu
            ; 2026-09-25 — HABIT joins BLOOPER and the GLITCH menu as a
            ; generative/performance effect: a recall leaves it exactly as it
            ; is. It sits BEFORE the looper stage in signal_chain.orc, so it is
            ; still recorded into loops normally — that is unaffected by this.
            kEx_HabitBypass = gkHabitBypass   ; slot 238 — HABIT bypass
            kEx_HabitMix = gkHabitMix   ; slot 239 — HABIT mix
            kEx_HabitSize = gkHabitSize   ; slot 240 — HABIT size
            kEx_HabitRepeats = gkHabitRepeats   ; slot 241 — HABIT repeats
            kEx_HabitBlend = gkHabitBlend   ; slot 242 — HABIT blend
            ; 2026-10-24 — LOOPER varispeed per channel is live loop state, not a
            ; guitar sound: a recall must never change how a running loop plays.
            kEx_LpSpIdx1 = gkLpSpIdx1   ; slot 297 — LOOPER ch1 speed
            kEx_LpSpIdx2 = gkLpSpIdx2   ; slot 298 — LOOPER ch2 speed
            kEx_LpSpIdx3 = gkLpSpIdx3   ; slot 299 — LOOPER ch3 speed
            kEx_LpSpIdx4 = gkLpSpIdx4   ; slot 300 — LOOPER ch4 speed
            kEx_LpSpIdx5 = gkLpSpIdx5   ; slot 301 — LOOPER ch5 speed
            ; 2026-10-03 — master out, master volume and BPM are rig-level
            ; state, never preset state: a recall must not change loudness or
            ; tempo.
            kEx_MasterOut = gkMasterOut   ; slot 305 — master out volume
            kEx_MasterVol = gkMasterVol   ; slot 296 — master volume
            kEx_ManualBpm = gkManualBpm   ; slot 306 — manual BPM
            ; 2026-10-22 — VOCAL chain (mic strip on inputs 5,6) is rig-level, not
            ; a guitar preset: a recall must not retune the voice. chn_k
            ; channels (no gk global), listed as `voc_*`.
            kEx_Voc_on chnget "voc_on"   ; voc_on slot 319
            kEx_Voc_hpf_hz chnget "voc_hpf_hz"   ; voc_hpf_hz slot 320
            kEx_Voc_mid_db chnget "voc_mid_db"   ; voc_mid_db slot 321
            kEx_Voc_mid_hz chnget "voc_mid_hz"   ; voc_mid_hz slot 322
            kEx_Voc_mid_q chnget "voc_mid_q"   ; voc_mid_q slot 323
            kEx_Voc_dly_ms chnget "voc_dly_ms"   ; voc_dly_ms slot 324
            kEx_Voc_dly_fb chnget "voc_dly_fb"   ; voc_dly_fb slot 325
            kEx_Voc_dly_wet chnget "voc_dly_wet"   ; voc_dly_wet slot 326
            kEx_Voc_rev_fb chnget "voc_rev_fb"   ; voc_rev_fb slot 327
            kEx_Voc_rev_cut chnget "voc_rev_cut"   ; voc_rev_cut slot 328
            kEx_Voc_rev_wet chnget "voc_rev_wet"   ; voc_rev_wet slot 329
            kEx_Voc_level chnget "voc_level"   ; voc_level slot 330
            ; MIXER page (menu 22): channel levels/pans/widths are rig-level too.
            kEx_Mx0 chnget "mx_gtr_vol"   ; mx_gtr_vol slot 331
            kEx_Mx1 chnget "mx_gtr_pan"   ; mx_gtr_pan slot 332
            kEx_Mx2 chnget "mx_gtr_wid"   ; mx_gtr_wid slot 333
            kEx_Mx3 chnget "mx_tpt_vol"   ; mx_tpt_vol slot 334
            kEx_Mx4 chnget "mx_tpt_pan"   ; mx_tpt_pan slot 335
            kEx_Mx5 chnget "mx_tpt_wid"   ; mx_tpt_wid slot 336
            kEx_Mx6 chnget "mx_pmr_vol"   ; mx_pmr_vol slot 337
            kEx_Mx7 chnget "mx_pmr_pan"   ; mx_pmr_pan slot 338
            kEx_Mx8 chnget "mx_pmr_wid"   ; mx_pmr_wid slot 339
            ; NAM quality (UTILITY) is rig-level too.
            kEx_VocPre chnget "voc_pre_db"   ; voc_pre_db slot 342
            kEx_Mx9 chnget "mx_lp_vol"   ; mx_lp_vol slot 341
            kEx_NamSize chnget "nam_size"   ; nam_size slot 340

            ; 2026-11-01 - the AMP is its own layer (DUODECIMUS enc1): row, variant,
            ; bypass and gains are live performance state; an FX preset recall must
            ; never change the amp. gkNAMProfileIdx is derived but not persisted, so
            ; it is carried too (unpack recomputes it from the preset's row).
            kEx_NAMRow = gkNAMActiveRow   ; slot 272 - NAM active row
            kEx_NAMByp = gkNAMBypass   ; slot 273 - NAM bypass
            kEx_NAMIn = gkNAMInGain   ; slot 274 - NAM in gain
            kEx_NAMOut = gkNAMOutGain   ; slot 275 - NAM out gain
            kEx_NAMSel0 = gkNAMRowSel[0]   ; slot 276
            kEx_NAMSel1 = gkNAMRowSel[1]   ; slot 277
            kEx_NAMSel2 = gkNAMRowSel[2]   ; slot 278
            kEx_NAMSel3 = gkNAMRowSel[3]   ; slot 279
            kEx_NAMSel4 = gkNAMRowSel[4]   ; slot 280
            kEx_NAMProf = gkNAMProfileIdx   ; derived (not persisted)
