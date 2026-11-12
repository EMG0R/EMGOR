  ;==============================================================
  ; OSC FEEDBACK — change-triggered for all parameters
  ;==============================================================

  ; Menu & last toggle
  $OSC_I(gkCurrentMenu'/menu'menu)
  $OSC_I(gkLastToggle'/last_toggle'lastTog)

  ; UTILITY page (2026-09-01): master-out volume, manual master BPM, rec state
  $OSC_F(gkMasterOut'/master_out'mout)
  $OSC_F(gkManualBpm'/master_bpm'mbpm)
  $OSC_I(gkRecState'/rec_state'recSt)
  ; the authoritative live tempo (loop/external/manual resolved) so the utility
  ; page can show what is ACTUALLY running, not just the manual request.
  ; 2026-10-05: was gkChaseBlissBPM (the LOOP-IMPLIED tempo, or the default 120 with
  ; no loop) -- a local guess that never tracked the received transport tempo. It is
  ; now gkGLITCH_bpm, the arbitrated tempo the looper/FX actually run on: the received
  ; transport number (CC120/121, else CC119) whenever it is authoritative.
  $OSC_F(gkGLITCH_bpm'/clock_bpm'clkBpm)

  ; Preset system
  $OSC_I(gkCurrentPreset'/preset'presId)

  ; Toggle 1: Volume Pedal
  $OSC_I(gkVOLpedalBypass'/vol_state'volByp)
  $OSC_F(gkVOLpedal'/vol_level'volLvl)

  ; Toggle 2: Attack Shaper (v2 POG swell)
  $OSC_I(gkATTK_bypass'/atk_state'atkByp)
  $OSC_F(gkATTK_mix'/atk_mix'atkMix)
  $OSC_F(gkATTK_atkMs'/atk_time'atkTime)
  $OSC_F(gkATTK_sens'/atk_sens'atkSens)
  $OSC_F(gkATTK_shape'/atk_shape'atkShape)
  $OSC_F(gkATTK_range'/atk_range'atkRange)
  $OSC_F(gkATTK_retrigMs'/atk_retrig'atkRetrig)

  ; Toggle 3: Pitch Shifter (old)
  $OSC_I(gkBypassPS'/ps_state'psByp)
  $OSC_F(gkSemi'/ps_semi'psSemi)
  $OSC_F(gkDryPctPS'/ps_mix'psMix)

  ; Toggle 26: H90 Pitch (sinusoidal model)
  $OSC_I(gkBypassH90'/h90_state'h90Byp)
  $OSC_F(gkSemiH90'/h90_semi'h90Semi)
  $OSC_F(gkH90mix'/h90_mix'h90Mix)

  ; Toggle 27: Pitch Fork (mincer phase-locked PV)
  $OSC_I(gkBypassPF'/pf_state'pfByp)
  $OSC_F(gkSemiPF'/pf_semi'pfSemi)
  $OSC_F(gkPFmix'/pf_mix'pfMix)

  ; Toggle 4: GPS
  $OSC_I(gkBypassGPS'/gps_state'gpsByp)
  $OSC_F(gkGPSsemis'/gps_semi'gpsSemi)
  $OSC_F(gkGPSmix'/gps_mix'gpsMix)
  $OSC_F(gkGPSfback'/gps_fback'gpsFb)
  $OSC_F(gkGPSdly'/gps_dly'gpsDly)

  ; Toggle 5: Formant Shifter
  $OSC_I(gkFormantBypass'/fmt_state'fmtByp)
  $OSC_F(gkFormantShift'/fmt_shift'fmtShift)
  $OSC_F(gkFormantMix'/fmt_mix'fmtMix)

  ; Toggle 28: FORMS — clean formant shifter (pvswarp)
  $OSC_I(gkFormsBypass'/fms_state'fmsByp)
  $OSC_F(gkFormsShift'/fms_shift'fmsShift)
  $OSC_F(gkFormsMix'/fms_mix'fmsMix)

  ; Toggle 38: BIN REDUCTION — spectral contrast exponentiation
  $OSC_I(gkBinBypass'/bin_state'binByp)
  $OSC_F(gkBinPedal'/bin_pedal'binPed)

  ; Toggle 44: BINCRUSHER — spectral bin coarsener
  $OSC_I(gkBcBypass'/bc_state'bcByp)
  $OSC_F(gkBcPedal'/bc_pedal'bcPed)

  ; Toggle 6: Vibrato
  $OSC_I(gkVIB_bypass'/vib_state'vibByp)
  $OSC_F(gkVIB_freq'/vib_rate'vibRate)
  kVibDepNorm = gkVIB_depth * 200   ; raw 0-0.005 → display 0-1
  $OSC_F(kVibDepNorm'/vib_depth'vibDepth)
  $OSC_F(gkVIB_fback'/vib_fback'vibFb)

  ; Toggle 7: Chorus
  $OSC_I(gkBypassChorus'/cho_state'choByp)
  $OSC_F(gkChorusRate'/cho_rate'choRate)
  $OSC_F(gkChorusDepth'/cho_depth'choDepth)
  $OSC_F(gkChorusMix'/cho_mix'choMix)

  ; Toggle 8: Phaser
  $OSC_I(gkPHA_bypass'/pha_state'phaByp)
  $OSC_F(gkPHA_rate'/pha_rate'phaRate)
  $OSC_F(gkPHA_fback'/pha_fback'phaFb)
  $OSC_F(gkPHA_depth'/pha_depth'phaDep)
  $OSC_F(gkPHA_stages'/pha_stages'phaStg)

  ; Toggle 9: Flanger
  $OSC_I(gkFLA_bypass'/fla_state'flaByp)
  $OSC_F(gkFLA_rate'/fla_rate'flaRate)
  kFlaDepNorm = gkFLA_depth * 100   ; raw 0-0.01 → display 0-1
  $OSC_F(kFlaDepNorm'/fla_depth'flaDepth)
  $OSC_F(gkFLA_fback'/fla_fback'flaFb)

  ; Toggle 10: Leslie
  $OSC_I(gkLeslieBypass'/les_state'lesByp)
  $OSC_F(gkLeslieSpeed'/les_speed'lesSpd)
  $OSC_F(gkLeslieDepth'/les_depth'lesDep)

  ; Toggle 11: Ring Modulator
  $OSC_I(gkBypassRM'/rm_state'rmByp)
  $OSC_F(gkARModFreq'/rm_freq'rmFreq)
  $OSC_F(gkARModDepth'/rm_depth'rmDepth)

  ; Toggle 31: Overdrive (Klon ⊕ TS, instr 130)
  $OSC_I(gkOD_bypass'/od_state'odByp)
  $OSC_F(gkOD_drive'/od_drive'odDrv)
  $OSC_F(gkOD_tone'/od_tone'odTone)
  $OSC_F(gkOD_wc1'/od_morph'odWc1)
  $OSC_F(gkOD_wc2'/od_blend'odWc2)

  ; Toggle 32: Marshall crunch preamp (instr 131)
  $OSC_I(gkMAR_bypass'/mar_state'marByp)
  $OSC_F(gkMAR_drive'/mar_drive'marDrv)
  $OSC_F(gkMAR_tone'/mar_tone'marTone)
  $OSC_F(gkMAR_wc1'/mar_pres'marWc1)
  $OSC_F(gkMAR_wc2'/mar_tight'marWc2)

  ; Toggle 33: Dual High-Gain preamp (instr 132)
  $OSC_I(gkDHG_bypass'/dhg_state'dhgByp)
  $OSC_F(gkDHG_drive'/dhg_drive'dhgDrv)
  $OSC_F(gkDHG_tone'/dhg_tone'dhgTone)
  $OSC_F(gkDHG_wc1'/dhg_blend'dhgWc1)
  $OSC_F(gkDHG_wc2'/dhg_tight'dhgWc2)

  ; Toggle 34: RAT × MZ (instr 133)
  $OSC_I(gkRMZ_bypass'/rmz_state'rmzByp)
  $OSC_F(gkRMZ_drive'/rmz_drive'rmzDrv)
  $OSC_F(gkRMZ_tone'/rmz_tone'rmzTone)
  $OSC_F(gkRMZ_wc1'/rmz_midf'rmzWc1)
  $OSC_F(gkRMZ_wc2'/rmz_midg'rmzWc2)

  ; Toggle 35: Big Muff (instr 134)
  $OSC_I(gkMUF_bypass'/muf_state'mufByp)
  $OSC_F(gkMUF_drive'/muf_drive'mufDrv)
  $OSC_F(gkMUF_tone'/muf_tone'mufTone)
  $OSC_F(gkMUF_wc1'/muf_mids'mufWc1)
  $OSC_F(gkMUF_wc2'/muf_voice'mufWc2)

  ; Toggle 14: AUTOMOOG
  $OSC_I(gkAUTOMOOG_bypass'/mog_state'mogByp)
  $OSC_F(gkAUTOMOOG_sens'/mog_sens'mogSens)
  $OSC_F(gkAUTOMOOG_res'/mog_res'mogRes)
  $OSC_F(gkAUTOMOOG_type'/mog_type'mogType)
  $OSC_F(gkAUTOMOOG_level'/mog_level'mogLvl)
  $OSC_F(gkAUTOMOOG_dist'/mog_dist'mogDist)

  ; Toggle 15: Delay
  $OSC_I(gkBypassDelay'/dly_state'dlyByp)
  $OSC_F(gkDelayMS'/dly_time'dlyTime)
  $OSC_F(gkDelMix'/dly_mix'dlyMix)
  $OSC_F(gkFeedback'/dly_fback'dlyFb)
  $OSC_F(gkSemiDel'/dly_semi'dlySemi)
  $OSC_I(gkDlyMode'/dly_mode'dlyMode)   ; 0 = normal, 1 = reverse repeats

  ; Toggle 16: Spectral Delay
  $OSC_I(gkSpecDelBypass'/sdl_state'sdlByp)
  $OSC_F(gkSpecDelMix'/sdl_mix'sdlMix)
  $OSC_F(gkSpecDelMaxDel'/sdl_maxdel'sdlMdl)
  $OSC_F(gkSpecDelFB'/sdl_fback'sdlFb)

  ; Toggle 36: HABIT — echo collector
  $OSC_I(gkHabitBypass'/hab_state'habByp)
  $OSC_F(gkHabitMix'/hab_mix'habMix)
  $OSC_F(gkHabitSize'/hab_size'habSiz)
  $OSC_F(gkHabitRepeats'/hab_repeats'habRep)
  $OSC_F(gkHabitScan'/hab_scan'habScn)
  $OSC_F(gkHabitBlend'/hab_blend'habBlnd)
  ; record gate: 1 = ring recording, 0 = FROZEN (CC76 click flip-flops it)
  $OSC_I(gkHab_rec'/hab_rec'habRec)

  ; Toggle 17: MOOD
  $OSC_I(gkMOOD_Bypass'/mod_state'modByp)
  $OSC_F(gkMOOD_Clock'/mod_clock'modClk)
  $OSC_F(gkMOOD_Mix'/mod_mix'modMix)
  $OSC_F(gkMOOD_Dens'/mod_dens'modDens)
  $OSC_F(gkMOOD_Dly'/mod_dly'modDly)

  ; Toggle 18: Granular
  $OSC_I(gkGranBypass'/grn_state'grnByp)
  $OSC_F(gkGranMix'/grn_mix'grnMix)
  $OSC_F(gkGranDens'/grn_dens'grnDens)
  $OSC_F(gkGranSize'/grn_size'grnSize)
  $OSC_F(gkGranPitch'/grn_pitch'grnPitch)

  ; Toggle 19: Reverb
  $OSC_I(gkverbBypass'/vrb_state'vrbByp)
  $OSC_F(gkReverbFB'/vrb_room'vrbRoom)
  $OSC_F(gkverbMix'/vrb_mix'vrbMix)
  $OSC_F(gkReverbDamp'/vrb_damp'vrbDamp)

  ; Toggle 20: BBCUT v2 (stutter.orc INTEGRATION NOTES item H)
  $OSC_I(gkBBcut_bypass'/bbc_state'bbcByp)
  $OSC_F(gkBBcut_drywet'/bbc_mix'bbcMix)
  $OSC_F(gkBBcut_prob'/bbc_prob'bbcProb)
  $OSC_F(gkBBcut_fltMix'/bbc_flt'bbcFlt)

  ; Toggle 21: TAPE
  $OSC_I(gkTapeBypass'/tape_state'tapeByp)
  $OSC_F(gkTapeAmount'/tape_amt'tapeAmt)
  $OSC_F(gkTapeFail'/tape_fail'tapeFail)
  $OSC_F(gkTapeModel'/tape_model'tapeMdl)

  ; Toggle 22: WAH
  $OSC_I(gkWAH_bypass'/wah_state'wahByp)
  $OSC_F(gkWAH_freq'/wah_freq'wahFreq)
  $OSC_F(gkWAH_res'/wah_res'wahRes)

  ; Toggle 23: REPEAT v2 (reuses WGuide bypass/mix globals, but publishes
  ; under the "rpt" UI code — matches EFFECT_PARAMS["rpt"] / MENUS so the
  ; UI's generic route_osc() dispatch finds it)
  $OSC_I(gkWGuide_bypass'/rpt_state'rptByp)
  $OSC_F(gkWGuide_drywet'/rpt_mix'rptMix)
  $OSC_F(gkRPT_prob'/rpt_prob'rptProb)
  $OSC_F(gkRPT_char'/rpt_char'rptChar)
  $OSC_F(gkRPT_len'/rpt_len'rptLen)

  ; Toggle 24: CHOP v2 (reuses SqMod bypass/mix globals, "chp" UI code)
  $OSC_I(gkSqMod_bypass'/chp_state'chpByp)
  $OSC_F(gkSqMod_drywet'/chp_depth'chpDepth)
  $OSC_F(gkCHP_prob'/chp_prob'chpProb)
  $OSC_F(gkCHP_rate'/chp_rate'chpRate)
  $OSC_F(gkCHP_shape'/chp_shape'chpShape)

  ; Toggle 25: RVRS v2 (reuses FShift bypass/mix globals, "rvs" UI code)
  $OSC_I(gkFShift_bypass'/rvs_state'rvsByp)
  $OSC_F(gkFShift_drywet'/rvs_mix'rvsMix)
  $OSC_F(gkRVS_prob'/rvs_prob'rvsProb)
  $OSC_F(gkRVS_len'/rvs_len'rvsLen)

  ; Toggle 37: NAM Amp Sim (Menu 11) — per-ROW slot state for UI.
  ; A slot is lit when its amp row is the active one and not bypassed;
  ; the variant within the row doesn't change which slot is lit.
  knam0st = (gkNAMBypass < 0.5 && gkNAMActiveRow == 0) ? 0 : 1
  knam1st = (gkNAMBypass < 0.5 && gkNAMActiveRow == 1) ? 0 : 1
  knam2st = (gkNAMBypass < 0.5 && gkNAMActiveRow == 2) ? 0 : 1
  knam3st = (gkNAMBypass < 0.5 && gkNAMActiveRow == 3) ? 0 : 1
  knam4st = (gkNAMBypass < 0.5 && gkNAMActiveRow == 4) ? 0 : 1
  $OSC_I(knam0st'/nam0_state'nam0st)
  $OSC_I(knam1st'/nam1_state'nam1st)
  $OSC_I(knam2st'/nam2_state'nam2st)
  $OSC_I(knam3st'/nam3_state'nam3st)
  $OSC_I(knam4st'/nam4_state'nam4st)
  $OSC_F(gkNAMOutGain'/nam_out_gain'namOutGain)
  $OSC_F(gkNAMInGain'/nam_in_gain'namInGain)
  ; Per-row variant selection — UI renders the selected amp's name live.
  knamsel0 = gkNAMRowSel[0]
  knamsel1 = gkNAMRowSel[1]
  knamsel2 = gkNAMRowSel[2]
  knamsel3 = gkNAMRowSel[3]
  knamsel4 = gkNAMRowSel[4]
  $OSC_I(knamsel0'/nam_sel0'namSel0)
  $OSC_I(knamsel1'/nam_sel1'namSel1)
  $OSC_I(knamsel2'/nam_sel2'namSel2)
  $OSC_I(knamsel3'/nam_sel3'namSel3)
  $OSC_I(knamsel4'/nam_sel4'namSel4)

  ; MAIN PREAMP (toggle 39): the surf-bank presets are voiced WITH the Hudson
  ; Broadcast preamp, which is no longer inherent. Whenever a surf preset activates
  ; (surf bank only), enable the preamp once so the gig presets keep their sound.
  ; Fires only on preset change, so manual PREAMP-menu toggling afterward still works.
  kPreChg changed gkCurrentPreset
  if (kPreChg == 1 && gkPresetBank == 0 && gkCurrentPreset >= 0) then
    gkPreamp_bypass = 0
  endif
  $OSC_I(gkPreamp_bypass'/preamp_state'preByp)

  ; Bass preamps (radio), bass distortion, tremolo — slot state for the UI.
  $OSC_I(gkBPL_bypass'/bpl_state'bplByp)
  $OSC_I(gkBPH_bypass'/bph_state'bphByp)
  $OSC_I(gkBDS_bypass'/bds_state'bdsByp)
  $OSC_I(gkTotalGlitch'/tgl_state'tglSt)
  $OSC_I(gkTremBypass'/trm_state'trmByp)
  $OSC_F(gkBPL_drive'/bpl_drive'bplDrv)
  $OSC_F(gkBPH_drive'/bph_drive'bphDrv)
  $OSC_F(gkBDS_drive'/bds_drive'bdsDrv)
  $OSC_F(gkBDS_blend'/bds_blend'bdsBlnd)
  $OSC_F(gkBDS_tone'/bds_tone'bdsTone)
  $OSC_F(gkTremRate'/trm_rate'trmRate)
  $OSC_F(gkTremDepth'/trm_depth'trmDepth)
  $OSC_I(gkTremMode'/trm_mode'trmMode)

  ; INPUT MODE — true-stereo switch (signal_chain.orc input stage)
  $OSC_I(gkStereoIn'/stereo_in'stIn)

  ; Toggle 45: ONWARD — auto-recapturing spectral freeze (instr 155)
  ; ONWARD v2 = time-domain DYNAMIC SAMPLER. gkONW_gate is GONE (v1's
  ; FOLLOW/HOLD capture probability); blend/size/err/type are the new params.
  $OSC_I(gkONW_bypass'/onw_state'onwByp)
  $OSC_F(gkONW_mix'/onw_mix'onwMix)
  $OSC_F(gkONW_blend'/onw_blend'onwBlend)
  $OSC_F(gkONW_size'/onw_size'onwSize)
  $OSC_F(gkONW_follow'/onw_follow'onwFollow)
  $OSC_F(gkONW_density'/onw_density'onwDensity)
  $OSC_F(gkONW_err'/onw_err'onwErr)
  $OSC_F(gkONW_oct'/onw_oct'onwOct)

  ; Toggle 66: MOOD II — SLIP-only dynamic sampler (live input + looper), instr 189
  $OSC_I(gkMD2_bypass'/md2_state'md2Byp)
  $OSC_F(gkMD2_Mix'/md2_mix'md2Mix)
  $OSC_F(gkMD2_Clock'/md2_clock'md2Clock)
  $OSC_F(gkMD2_Length'/md2_length'md2Length)
  $OSC_F(gkMD2_Gen'/md2_gen'md2Gen)

  ; Toggle 46: LOSSY — codec degradation (instr 123)
  $OSC_I(gkLsyBypass'/lsy_state'lsyByp)
  $OSC_F(gkLSY_mix'/lsy_mix'lsyMix)
  $OSC_F(gkLSY_loss'/lsy_loss'lsyLoss)
  $OSC_F(gkLSY_bw'/lsy_bw'lsyBw)
  $OSC_F(gkLSY_pkt'/lsy_pkt'lsyPkt)

  ; Toggle 47: RESONATOR — sympathetic pipe bank (instr 157)
  $OSC_I(gkRES_bypass'/res_state'resByp)
  $OSC_F(gkRES_mix'/res_mix'resMix)
  $OSC_F(gkRES_onset'/res_onset'resOnset)
  $OSC_F(gkRES_fb'/res_fb'resFb)
  $OSC_F(gkRES_oct'/res_oct'resOct)
  $OSC_F(gkRES_frz'/res_frz'resFrz)

  ; Toggle 48: IMPULSE SYNTH — per-pick synth voice (instr 156)
  $OSC_I(gkIMP_bypass'/imp_state'impByp)
  $OSC_F(gkIMP_mix'/imp_mix'impMix)
  $OSC_F(gkIMP_tone'/imp_tone'impTone)
  $OSC_F(gkIMP_dec'/imp_dec'impDec)
  $OSC_F(gkIMP_prob'/imp_prob'impProb)

  ; Toggle 54: OCTAVER (instr 170)
  $OSC_I(gkOCT_bypass'/oct_state'octByp)
  $OSC_F(gkOCT_sub'/oct_sub'octSub)
  $OSC_F(gkOCT_tone'/oct_tone'octTone)
  $OSC_F(gkOCT_char'/oct_char'octChar)

  ; Toggle 55: ENV FILTER (instr 171)
  $OSC_I(gkENV_bypass'/env_state'envByp)
  $OSC_F(gkENV_wet'/env_wet'envWet)
  $OSC_F(gkENV_sens'/env_sens'envSens)
  $OSC_F(gkENV_mode'/env_mode'envMode)
  $OSC_F(gkENV_att'/env_att'envAtt)
  $OSC_F(gkENV_dec'/env_dec'envDec)

  ; Toggles 49-53: IR VERB (Menu 12, instr 240) — NAM-style per-row slot
  ; state + per-row variant selection + per-row dry/wet.
  kir0st = (gkIRBypass < 0.5 && gkIRActiveRow == 0) ? 0 : 1
  kir1st = (gkIRBypass < 0.5 && gkIRActiveRow == 1) ? 0 : 1
  kir2st = (gkIRBypass < 0.5 && gkIRActiveRow == 2) ? 0 : 1
  kir3st = (gkIRBypass < 0.5 && gkIRActiveRow == 3) ? 0 : 1
  kir4st = (gkIRBypass < 0.5 && gkIRActiveRow == 4) ? 0 : 1
  $OSC_I(kir0st'/ir0_state'ir0st)
  $OSC_I(kir1st'/ir1_state'ir1st)
  $OSC_I(kir2st'/ir2_state'ir2st)
  $OSC_I(kir3st'/ir3_state'ir3st)
  $OSC_I(kir4st'/ir4_state'ir4st)
  kirsel0 = gkIRSel[0]
  kirsel1 = gkIRSel[1]
  kirsel2 = gkIRSel[2]
  kirsel3 = gkIRSel[3]
  kirsel4 = gkIRSel[4]
  $OSC_I(kirsel0'/ir_sel0'irSel0)
  $OSC_I(kirsel1'/ir_sel1'irSel1)
  $OSC_I(kirsel2'/ir_sel2'irSel2)
  $OSC_I(kirsel3'/ir_sel3'irSel3)
  $OSC_I(kirsel4'/ir_sel4'irSel4)
  kirmix0 = gkIRRowMix[0]
  kirmix1 = gkIRRowMix[1]
  kirmix2 = gkIRRowMix[2]
  kirmix3 = gkIRRowMix[3]
  kirmix4 = gkIRRowMix[4]
  $OSC_F(kirmix0'/ir0_mix'ir0mx)
  $OSC_F(kirmix1'/ir1_mix'ir1mx)
  $OSC_F(kirmix2'/ir2_mix'ir2mx)
  $OSC_F(kirmix3'/ir3_mix'ir3mx)
  $OSC_F(kirmix4'/ir4_mix'ir4mx)
  $OSC_F(gkIRDamp'/ir_damp'irDamp)
  $OSC_F(gkIRLevel'/ir_level'irLevel)

  ;==============================================================
  ; 2026-07-13 BATCH — toggles 57-58, 60-65; instrs 180-181, 183-188
  ;==============================================================

  ; Toggle 57: COMPRESSOR (instr 180, PREAMP menu s4)
  $OSC_I(gkCMP_bypass'/cmp_state'cmpByp)
  $OSC_F(gkCMP_amt'/cmp_amt'cmpAmt)
  $OSC_F(gkCMP_speed'/cmp_speed'cmpSpd)
  $OSC_F(gkCMP_makeup'/cmp_makeup'cmpMkp)
  $OSC_F(gkCMP_blend'/cmp_blend'cmpBlnd)
  $OSC_F(gkCMP_gate'/cmp_gate'cmpGate)

  ; Toggle 58: TONE EQ (instr 181, SYNTH menu s2)
  $OSC_I(gkTEQ_bypass'/teq_state'teqByp)
  $OSC_F(gkTEQ_mid'/teq_mid'teqMid)
  $OSC_F(gkTEQ_freq'/teq_freq'teqFreq)
  $OSC_F(gkTEQ_tilt'/teq_tilt'teqTilt)
  $OSC_F(gkTEQ_low'/teq_low'teqLow)
  $OSC_F(gkTEQ_high'/teq_high'teqHigh)

  ; Toggle 60: CHAIN FB (instr 183, AIR menu s4)
  $OSC_I(gkCFB_bypass'/cfb_state'cfbByp)
  $OSC_F(gkCFB_ret'/cfb_ret'cfbRet)
  $OSC_F(gkCFB_gain'/cfb_gain'cfbGain)
  $OSC_F(gkCFB_tone'/cfb_tone'cfbTone)
  $OSC_F(gkCFB_ceil'/cfb_ceil'cfbCeil)

  ; Toggle 61: TRACK SYNTH (instr 184, SYNTH menu s0)
  $OSC_I(gkTSY_bypass'/tsy_state'tsyByp)
  $OSC_F(gkTSY_mix'/tsy_mix'tsyMix)
  $OSC_F(gkTSY_cut'/tsy_cut'tsyCut)
  $OSC_F(gkTSY_res'/tsy_res'tsyRes)
  $OSC_F(gkTSY_glide'/tsy_glide'tsyGld)
  $OSC_F(gkTSY_sub'/tsy_sub'tsySub)
  $OSC_F(gkTSY_det'/tsy_det'tsyDet)

  ; Toggle 62: PLL FUZZ (instr 185, SYNTH menu s1)
  $OSC_I(gkPLL_bypass'/pll_state'pllByp)
  $OSC_F(gkPLL_mix'/pll_mix'pllMix)
  $OSC_F(gkPLL_intv'/pll_intv'pllIntv)
  $OSC_F(gkPLL_lock'/pll_lock'pllLock)
  $OSC_F(gkPLL_vmix'/pll_vmix'pllVmix)

  ; Toggle 63: FEEDBACKER (instr 186, AIR menu s0)
  $OSC_I(gkFBK_bypass'/fbk_state'fbkByp)
  $OSC_F(gkFBK_level'/fbk_level'fbkLvl)
  $OSC_F(gkFBK_harm'/fbk_harm'fbkHarm)
  $OSC_F(gkFBK_bloom'/fbk_bloom'fbkBlm)
  $OSC_F(gkFBK_vib'/fbk_vib'fbkVib)

  ; Toggle 64: DIFFUSION (instr 187, AIR menu s1)
  $OSC_I(gkDIF_bypass'/dif_state'difByp)
  $OSC_F(gkDIF_wet'/dif_wet'difWet)
  $OSC_F(gkDIF_depth'/dif_depth'difDep)
  $OSC_F(gkDIF_rate'/dif_rate'difRate)
  $OSC_F(gkDIF_duck'/dif_duck'difDuck)

  ; Toggle 65 / old instr 188 (SHIMMER) REMOVED 2026-07-13 — that code point
  ; is dead. The NEW SHIMMER below (2026-07-26, toggle 71, instr 194) is a
  ; different effect entirely (shimmerverb.orc, not the retired shimmer.orc).

  ; 2026-07-26 delay/verb expansion (docs/specs/2026-07-26-delay-verb-
  ; expansion.md) — toggles 67-73. GRAINVERB (188/toggle 73) publishes its
  ; own OSC locally (spectraldelay.orc-style local OSCsend in grainverb.orc
  ; itself — /gvb_state, /gvb_mix, /gvb_spacing, /gvb_decay); no entry here.

  ; Toggle 67: BINSCATTER (instr 201, spectraldelay.orc — SPECTRAL menu s2)
  $OSC_I(gkBscBypass'/bsc_state'bscByp)
  $OSC_F(gkBscMix'/bsc_mix'bscMix)
  $OSC_F(gkBscScatter'/bsc_scatter'bscScat)
  $OSC_F(gkBscFB'/bsc_fback'bscFb)
  $OSC_F(gkBscMaxDel'/bsc_window'bscWin)

  ; Toggle 68: TAPE DELAY (instr 190, tapedelay.orc — DELAY menu s4)
  $OSC_I(gkTdlBypass'/tdl_state'tdlByp)
  $OSC_F(gkTdlMix'/tdl_mix'tdlMix)
  $OSC_F(gkTdlTimeMs'/tdl_time'tdlTime)
  $OSC_F(gkTdlFeedback'/tdl_fback'tdlFb)
  $OSC_F(gkTdlFiltCtr'/tdl_center'tdlCtr)
  $OSC_F(gkTdlFiltQ'/tdl_width'tdlQ)
  ; UI's head-mode STEP_LABELS are 0-based (0..6); the engine global
  ; (gkTdlHeadMode) is 1-based (1..7, stepped mask index) — send UI-space.
  kTdlHeadUI = gkTdlHeadMode - 1
  $OSC_I(kTdlHeadUI'/tdl_head'tdlHead)

  ; Toggle 69: POLY DLY v5 (instr 191, movedelay.orc — DELAY menu s2)
  $OSC_I(gkMvdBypass'/mvd_state'mvdByp)
  $OSC_I(gkMvdNumL'/mvd_numl'mvdNumL)
  $OSC_I(gkMvdDenL'/mvd_denl'mvdDenL)
  $OSC_I(gkMvdNumR'/mvd_numr'mvdNumR)
  $OSC_I(gkMvdDenR'/mvd_denr'mvdDenR)
  $OSC_F(gkMvdAtt'/mvd_att'mvdAtt)
  $OSC_F(gkMvdDec'/mvd_dec'mvdDec)
  $OSC_I(gkMvdReps'/mvd_reps'mvdReps)
  $OSC_I(gkMvdIntv'/mvd_intv'mvdIntv)

  ; Toggle 70: NEBULA (instr 193, nebula.orc — DIGIVERB menu s4)
  $OSC_I(gkNebBypass'/neb_state'nebByp)
  $OSC_F(gkNebMix'/neb_mix'nebMix)
  $OSC_F(gkNebDensity'/neb_dens'nebDens)
  $OSC_F(gkNebSize'/neb_size'nebSize)

  ; Toggle 71: SHIMMER (instr 194, shimmerverb.orc — DIGIVERB menu s3)
  $OSC_I(gkShmBypass'/shm_state'shmByp)
  $OSC_F(gkShmMix'/shm_mix'shmMix)
  $OSC_F(gkShmInterval'/shm_intv'shmIntv)
  $OSC_F(gkShmFbAmt'/shm_bloom'shmBlm)

  ; Toggle 72: FREEZE (instr 195, freezeverb.orc — DIGIVERB menu s2)
  $OSC_I(gkFrzBypass'/frz_state'frzByp)
  $OSC_F(gkFrzBedLevel'/frz_level'frzLvl)
  $OSC_F(gkFrzDamp'/frz_damp'frzDamp)
  $OSC_F(gkFrzListenGain'/frz_listen'frzLstn)

  ; PER-PRESET OUTPUT TRIM — the ACTIVE preset's stored gain (giPresetTrim).
  ; gkPresetTrim is refreshed every k-cycle by signal_chain.orc's master stage,
  ; so this mirrors both encoder edits (menu 0 / CC20) and preset switches.
  $OSC_F(gkPresetTrim'/preset_trim'presTrim)

  ; PRESET BANK (mode) — one-shot at startup so the UI picks surf/bass labels,
  ; plus change-triggered footer for robustness.
  kBankInit init 0
  if (kBankInit == 0) then
    OSC3x_i "/preset_bank", gkPresetBank
    ; MAIN PREAMP voice: giPreVoice is an i-time constant, so $OSC_F's `changed`
    ; would never fire and the UI's Voice slider would sit dead at 0 forever
    ; (it is really 1 = MEDIUM). One-shot it at boot instead.
    OSC3x_f "/pre_voice", giPreVoice
    ; Bass boot: default to the LIGHT preamp (radio pair), HEAVY off.
    if (gkPresetBank == 1) then
      gkBPL_bypass = 0
      gkBPH_bypass = 1
    endif
    kBankInit = 1
  endif
  $OSC_I(gkPresetBank'/preset_bank'pbnk)


  ;==============================================================
  ; SOUNDMAGIC rack -> UI (spectral/CONTRACT.md). EXACTLY two OSCsend sites in
  ; total (2 sockets) regardless of ~310 params — never one socket per param
  ; (the 2026-09-01 fd-limit incident). OSCsend fires on a change of its kwhen
  ; argument, i.e. at most ONE message per k-cycle per site, so this is a
  ; drained change-scan: each cycle send the first changed slot found, and
  ; update its shadow. Full resend on gkOscRefresh = poison every shadow so the
  ; scan walks all of them (about a second at 750 k/s). Burst (preset recall)
  ; drains at one message per k-cycle.
  ;==============================================================
  kSMcntS init 0
  kSMcntP init 0
  kSMptr  init 0
  if (gkOscRefresh == 1) then
    kSMi = 0
    while (kSMi < 768) do
      tablew 1e30, kSMi, giSMshadow
      kSMi += 1
    od
    kSMi = 0
    while (kSMi < 24) do
      tablew -1, kSMi, giSMonSh
      kSMi += 1
    od
  endif
  ; /sm_state fx on
  kSMi = 0
  kSMhit = -1
  while (kSMi < 23 && kSMhit < 0) do
    if (tab:k(kSMi, giSMon) != tab:k(kSMi, giSMonSh)) then
      kSMhit = kSMi
    endif
    kSMi += 1
  od
  if (kSMhit >= 0) then
    kSMv = tab:k(kSMhit, giSMon)
    tablew kSMv, kSMhit, giSMonSh
    kSMcntS += 1
    OSCsend kSMcntS, "127.0.0.1", 8000, "/sm_state", "ii", kSMhit, int(kSMv)
    OSCsend kSMcntS, "127.0.0.1", 8005, "/sm_state", "ii", kSMhit, int(kSMv)   ; web mirror (CONTRACT.md "Web mirror")
  endif
  ; /sm_p fx p val  (scan 48 slots per cycle from a rotating pointer)
  kSMi = 0
  kSMhit = -1
  while (kSMi < 48 && kSMhit < 0) do
    kSMx = kSMptr + kSMi
    if (kSMx >= 736) then
      kSMx -= 736
    endif
    if (tab:k(kSMx, giSMP) != tab:k(kSMx, giSMshadow)) then
      kSMhit = kSMx
    endif
    kSMi += 1
  od
  if (kSMhit >= 0) then
    kSMv = tab:k(kSMhit, giSMP)
    tablew kSMv, kSMhit, giSMshadow
    kSMcntP += 1
    OSCsend kSMcntP, "127.0.0.1", 8000, "/sm_p", "iif", int(kSMhit / 32), kSMhit % 32, kSMv
    OSCsend kSMcntP, "127.0.0.1", 8005, "/sm_p", "iif", int(kSMhit / 32), kSMhit % 32, kSMv   ; web mirror
    kSMptr = kSMhit + 1
  else
    kSMptr = kSMptr + 48
  endif
  if (kSMptr >= 736) then
    kSMptr -= 736
  endif
  ; /sm_hero fx k idx and /sm_pedal fx idx : echo of giSMhero / giSMpedal edits
  ; (/sh /shp). Same shadow + one-send-per-k-cycle pattern; shadows start at -99
  ; so the first cycle (and every gkOscRefresh) re-sends the whole mapping.
  giSMheroSh  ftgen 0, 0, -192, -2, -99
  giSMpedalSh ftgen 0, 0, -24, -2, -99
  kSMhptr init 0
  if (gkOscRefresh == 1) then
    kSMi = 0
    while (kSMi < 192) do
      tablew -99, kSMi, giSMheroSh
      kSMi += 1
    od
    kSMi = 0
    while (kSMi < 24) do
      tablew -99, kSMi, giSMpedalSh
      kSMi += 1
    od
  endif
  kSMi = 0
  kSMhit = -1
  while (kSMi < 48 && kSMhit < 0) do
    kSMx = kSMhptr + kSMi
    if (kSMx >= 192) then
      kSMx -= 192
    endif
    if (tab:k(kSMx, giSMhero) != tab:k(kSMx, giSMheroSh)) then
      kSMhit = kSMx
    endif
    kSMi += 1
  od
  if (kSMhit >= 0) then
    kSMv = tab:k(kSMhit, giSMhero)
    tablew kSMv, kSMhit, giSMheroSh
    kSMcntH init 0
    kSMcntH += 1
    OSCsend kSMcntH, "127.0.0.1", 8000, "/sm_hero", "iii", int(kSMhit / 8), kSMhit % 8, int(kSMv)
    OSCsend kSMcntH, "127.0.0.1", 8005, "/sm_hero", "iii", int(kSMhit / 8), kSMhit % 8, int(kSMv)   ; web mirror
    kSMhptr = kSMhit + 1
  else
    kSMhptr = kSMhptr + 48
  endif
  if (kSMhptr >= 192) then
    kSMhptr -= 192
  endif
  kSMi = 0
  kSMhit = -1
  while (kSMi < 24 && kSMhit < 0) do
    if (tab:k(kSMi, giSMpedal) != tab:k(kSMi, giSMpedalSh)) then
      kSMhit = kSMi
    endif
    kSMi += 1
  od
  if (kSMhit >= 0) then
    kSMv = tab:k(kSMhit, giSMpedal)
    tablew kSMv, kSMhit, giSMpedalSh
    kSMcntD init 0
    kSMcntD += 1
    OSCsend kSMcntD, "127.0.0.1", 8000, "/sm_pedal", "ii", kSMhit, int(kSMv)
    OSCsend kSMcntD, "127.0.0.1", 8005, "/sm_pedal", "ii", kSMhit, int(kSMv)   ; web mirror
  endif
