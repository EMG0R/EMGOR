;==============================================================
; state_preset_exclude_save.inc.orc  (HAND-MAINTAINED; no generator exists in the tree - was labelled GENERATED 2026-09-10)
; PRESET EXCLUSIONS — BLOOPER + every GLITCH-menu effect (IMPULSE, BBCUT,
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
            ; 2026-10-03 — master out, master volume and BPM are rig-level
            ; state, never preset state: a recall must not change loudness or
            ; tempo.
            kEx_MasterOut = gkMasterOut   ; slot 305 — master out volume
            kEx_MasterVol = gkMasterVol   ; slot 296 — master volume
            kEx_ManualBpm = gkManualBpm   ; slot 306 — manual BPM
