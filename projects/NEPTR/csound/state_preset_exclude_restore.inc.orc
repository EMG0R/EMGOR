;==============================================================
; state_preset_exclude_restore.inc.orc  (HAND-MAINTAINED; no generator exists in the tree - was labelled GENERATED 2026-09-10)
; PRESET EXCLUSIONS — BLOOPER, LOOPER speeds + every GLITCH-menu effect (IMPULSE, BBCUT,
; REPEAT, CHOP, RVRS) are generative/performance effects: a preset RECALL must
; leave them EXACTLY as they currently are. This file is #included in the recall
; path in state_persist.orc: _save snapshots the live globals BEFORE the bank
; row overwrites giNeptrState + unpack; _restore writes them back AFTER unpack,
; so the excluded globals keep their pre-recall live values regardless of what
; the preset stored. Explicit, auditable list (global @ giNeptrState slot):
;==============================================================

            gkBLP_bypass = kEx_BLP_bypass   ; slot 225 — BLOOPER bypass
            gkBLP_Motion = kEx_BLP_Motion   ; slot 226 — BLOOPER motion
            gkBLP_Scat = kEx_BLP_Scat   ; slot 227 — BLOOPER scatter
            gkBLP_Size = kEx_BLP_Size   ; slot 228 — BLOOPER size
            gkBLP_Char = kEx_BLP_Char   ; slot 229 — BLOOPER character
            gkIMP_bypass = kEx_IMP_bypass   ; slot 249 — GLITCH IMPULSE bypass
            gkIMP_mix = kEx_IMP_mix   ; slot 250 — GLITCH IMPULSE mix
            gkIMP_prob = kEx_IMP_prob   ; slot 251 — GLITCH IMPULSE prob
            gkIMP_dec = kEx_IMP_dec   ; slot 252 — GLITCH IMPULSE decay
            gkIMP_tone = kEx_IMP_tone   ; slot 253 — GLITCH IMPULSE tone
            gkBBcut_bypass = kEx_BBcut_bypass   ; slot 254 — GLITCH BBCUT bypass
            gkBBcut_drywet = kEx_BBcut_drywet   ; slot 255 — GLITCH BBCUT drywet
            gkBBcut_prob = kEx_BBcut_prob   ; slot 256 — GLITCH BBCUT prob
            gkBBcut_fltMix = kEx_BBcut_fltMix   ; slot 257 — GLITCH BBCUT fltMix
            gkWGuide_bypass = kEx_WGuide_bypass   ; slot 258 — GLITCH REPEAT bypass (WGuide bus)
            gkWGuide_drywet = kEx_WGuide_drywet   ; slot 259 — GLITCH REPEAT drywet (WGuide bus)
            gkRPT_prob = kEx_RPT_prob   ; slot 260 — GLITCH REPEAT prob
            gkRPT_char = kEx_RPT_char   ; slot 261 — GLITCH REPEAT char
            gkRPT_len = kEx_RPT_len   ; slot 262 — GLITCH REPEAT len
            gkSqMod_bypass = kEx_SqMod_bypass   ; slot 263 — GLITCH CHOP bypass (SqMod bus)
            gkSqMod_drywet = kEx_SqMod_drywet   ; slot 264 — GLITCH CHOP drywet (SqMod bus)
            gkCHP_prob = kEx_CHP_prob   ; slot 265 — GLITCH CHOP prob
            gkCHP_rate = kEx_CHP_rate   ; slot 266 — GLITCH CHOP rate
            gkCHP_shape = kEx_CHP_shape   ; slot 267 — GLITCH CHOP shape
            gkFShift_bypass = kEx_FShift_bypass   ; slot 268 — GLITCH RVRS bypass (FShift bus)
            gkFShift_drywet = kEx_FShift_drywet   ; slot 269 — GLITCH RVRS drywet (FShift bus)
            gkRVS_prob = kEx_RVS_prob   ; slot 270 — GLITCH RVRS prob
            gkRVS_len = kEx_RVS_len   ; slot 271 — GLITCH RVRS len
            ; see the matching note in _save: the pedal's owner is live
            ; performance state and a recall must never re-point it.
            ; 2026-11-01: ONLY the onboard menu-0 slots (13..17) keep the live owner
            ; (the 09-15 protection). DUODECIMUS slots 1..12 hand the pedal to the
            ; owner stored in the preset (unpack already restored it). Show presets
            ; store VOLUME (target 1): put gkVOLpedal at the pedal's CURRENT position
            ; (no jump on the next move) and tell expr_cc29.orc not to disarm.
            ; 2026-11-01 (later): onboard 13..17 now its own bank, so every slot
            ; hands the pedal to its stored owner (Emory: volume must always map).
            if (gkLastToggle29 == 1 && gkExprPos >= 0) then
                gkVOLpedal = gkExprPos
            endif
            gkExprHandoff = gkLastToggle29
            ; see the matching notes in _save.
            gkCurrentMenu = kEx_CurrentMenu   ; slot 302 — current menu
            gkHabitBypass = kEx_HabitBypass   ; slot 238 — HABIT bypass
            gkHabitMix = kEx_HabitMix   ; slot 239 — HABIT mix
            gkHabitSize = kEx_HabitSize   ; slot 240 — HABIT size
            gkHabitRepeats = kEx_HabitRepeats   ; slot 241 — HABIT repeats
            gkHabitBlend = kEx_HabitBlend   ; slot 242 — HABIT blend
            gkLpSpIdx1 = kEx_LpSpIdx1   ; slot 297 — LOOPER ch1 speed
            gkLpSpIdx2 = kEx_LpSpIdx2   ; slot 298 — LOOPER ch2 speed
            gkLpSpIdx3 = kEx_LpSpIdx3   ; slot 299 — LOOPER ch3 speed
            gkLpSpIdx4 = kEx_LpSpIdx4   ; slot 300 — LOOPER ch4 speed
            gkLpSpIdx5 = kEx_LpSpIdx5   ; slot 301 — LOOPER ch5 speed
            ; 2026-10-03 — master out, master volume and BPM are rig-level
            ; state, never preset state: a recall must not change loudness or
            ; tempo.
            gkMasterOut = kEx_MasterOut   ; slot 305 — master out volume
            gkMasterVol = kEx_MasterVol   ; slot 296 — master volume
            gkManualBpm = kEx_ManualBpm   ; slot 306 — manual BPM
            ; see the matching note in _save: VOCAL chain is rig-level state.
            ; csound runs every opcode's INIT pass when this instr starts, even inside an
            ; untaken if-branch, so the chnsets below would write these k-vars' initial 0
            ; into the live channels at startup (silenced vocal + mixer, 2026-10-22).
            ; Give them the live channel value at init: the init-pass write is then a no-op.
            kEx_Voc_on init chnget:i("voc_on")
            kEx_Voc_hpf_hz init chnget:i("voc_hpf_hz")
            kEx_Voc_mid_db init chnget:i("voc_mid_db")
            kEx_Voc_mid_hz init chnget:i("voc_mid_hz")
            kEx_Voc_mid_q init chnget:i("voc_mid_q")
            kEx_Voc_dly_ms init chnget:i("voc_dly_ms")
            kEx_Voc_dly_fb init chnget:i("voc_dly_fb")
            kEx_Voc_dly_wet init chnget:i("voc_dly_wet")
            kEx_Voc_rev_fb init chnget:i("voc_rev_fb")
            kEx_Voc_rev_cut init chnget:i("voc_rev_cut")
            kEx_Voc_rev_wet init chnget:i("voc_rev_wet")
            kEx_Voc_level init chnget:i("voc_level")
            kEx_Mx0 init chnget:i("mx_gtr_vol")
            kEx_Mx1 init chnget:i("mx_gtr_pan")
            kEx_Mx2 init chnget:i("mx_gtr_wid")
            kEx_Mx3 init chnget:i("mx_tpt_vol")
            kEx_Mx4 init chnget:i("mx_tpt_pan")
            kEx_Mx5 init chnget:i("mx_tpt_wid")
            kEx_Mx6 init chnget:i("mx_pmr_vol")
            kEx_Mx7 init chnget:i("mx_pmr_pan")
            kEx_Mx8 init chnget:i("mx_pmr_wid")
            kEx_VocPre init chnget:i("voc_pre_db")
            kEx_Mx9 init chnget:i("mx_lp_vol")
            kEx_NamSize init chnget:i("nam_size")
            chnset kEx_Voc_on, "voc_on"   ; voc_on slot 319
            chnset kEx_Voc_hpf_hz, "voc_hpf_hz"   ; voc_hpf_hz slot 320
            chnset kEx_Voc_mid_db, "voc_mid_db"   ; voc_mid_db slot 321
            chnset kEx_Voc_mid_hz, "voc_mid_hz"   ; voc_mid_hz slot 322
            chnset kEx_Voc_mid_q, "voc_mid_q"   ; voc_mid_q slot 323
            chnset kEx_Voc_dly_ms, "voc_dly_ms"   ; voc_dly_ms slot 324
            chnset kEx_Voc_dly_fb, "voc_dly_fb"   ; voc_dly_fb slot 325
            chnset kEx_Voc_dly_wet, "voc_dly_wet"   ; voc_dly_wet slot 326
            chnset kEx_Voc_rev_fb, "voc_rev_fb"   ; voc_rev_fb slot 327
            chnset kEx_Voc_rev_cut, "voc_rev_cut"   ; voc_rev_cut slot 328
            chnset kEx_Voc_rev_wet, "voc_rev_wet"   ; voc_rev_wet slot 329
            chnset kEx_Voc_level, "voc_level"   ; voc_level slot 330
            ; see _save: MIXER page levels are rig-level state.
            chnset kEx_Mx0, "mx_gtr_vol"   ; mx_gtr_vol slot 331
            chnset kEx_Mx1, "mx_gtr_pan"   ; mx_gtr_pan slot 332
            chnset kEx_Mx2, "mx_gtr_wid"   ; mx_gtr_wid slot 333
            chnset kEx_Mx3, "mx_tpt_vol"   ; mx_tpt_vol slot 334
            chnset kEx_Mx4, "mx_tpt_pan"   ; mx_tpt_pan slot 335
            chnset kEx_Mx5, "mx_tpt_wid"   ; mx_tpt_wid slot 336
            chnset kEx_Mx6, "mx_pmr_vol"   ; mx_pmr_vol slot 337
            chnset kEx_Mx7, "mx_pmr_pan"   ; mx_pmr_pan slot 338
            chnset kEx_Mx8, "mx_pmr_wid"   ; mx_pmr_wid slot 339
            chnset kEx_VocPre, "voc_pre_db"   ; voc_pre_db slot 342
            chnset kEx_Mx9, "mx_lp_vol"   ; mx_lp_vol slot 341
            chnset kEx_NamSize, "nam_size"   ; nam_size slot 340

            ; 2026-11-01 - the AMP is its own layer (DUODECIMUS enc1): row, variant,
            ; bypass and gains are live performance state; an FX preset recall must
            ; never change the amp. gkNAMProfileIdx is derived but not persisted, so
            ; it is carried too (unpack recomputes it from the preset's row).
            gkNAMActiveRow = kEx_NAMRow   ; slot 272 - NAM active row
            gkNAMBypass = kEx_NAMByp   ; slot 273 - NAM bypass
            gkNAMInGain = kEx_NAMIn   ; slot 274 - NAM in gain
            gkNAMOutGain = kEx_NAMOut   ; slot 275 - NAM out gain
            gkNAMRowSel[0] = kEx_NAMSel0   ; slot 276
            gkNAMRowSel[1] = kEx_NAMSel1   ; slot 277
            gkNAMRowSel[2] = kEx_NAMSel2   ; slot 278
            gkNAMRowSel[3] = kEx_NAMSel3   ; slot 279
            gkNAMRowSel[4] = kEx_NAMSel4   ; slot 280
            gkNAMProfileIdx = kEx_NAMProf   ; derived (not persisted)
