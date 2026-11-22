;==============================================================
; state_preset_exclude_restore.inc.orc  (HAND-MAINTAINED; no generator exists in the tree - was labelled GENERATED 2026-09-10)
; PRESET EXCLUSIONS — BLOOPER + every GLITCH-menu effect (IMPULSE, BBCUT,
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
            gkLastToggle29 = kEx_LastToggle29   ; slot 304 — expr pedal owner
            ; see the matching notes in _save.
            gkCurrentMenu = kEx_CurrentMenu   ; slot 302 — current menu
            gkHabitBypass = kEx_HabitBypass   ; slot 238 — HABIT bypass
            gkHabitMix = kEx_HabitMix   ; slot 239 — HABIT mix
            gkHabitSize = kEx_HabitSize   ; slot 240 — HABIT size
            gkHabitRepeats = kEx_HabitRepeats   ; slot 241 — HABIT repeats
            gkHabitBlend = kEx_HabitBlend   ; slot 242 — HABIT blend
            ; 2026-10-03 — master out, master volume and BPM are rig-level
            ; state, never preset state: a recall must not change loudness or
            ; tempo.
            gkMasterOut = kEx_MasterOut   ; slot 305 — master out volume
            gkMasterVol = kEx_MasterVol   ; slot 296 — master volume
            gkManualBpm = kEx_ManualBpm   ; slot 306 — manual BPM
