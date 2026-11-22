;==============================================================
; state_unpack.inc.orc  (GENERATED-BY-EXTRACTION 2026-09-08)
; The giNeptrState table -> live gk* globals unpack, factored out of
; state_persist.orc so it can run in TWO places with identical, correct
; behaviour (incl. the +1 menu/toggle offsets, NAM profile recompute, and
; the masterout/bpm guards):
;   1. BOOT restore   (instr 97, inside `if (iGood == 1)`).
;   2. PRESET RECALL  (instr 96, on a /cc 16 106 pulse -> ftloadk + this).
; Pure k-rate: only `gk* = tab:k(n,giNeptrState)`, array writes, guarded kVar
; assignments and comments — NO i-time-only opcodes — so it is legal to
; #include mid-instr at k-rate. Slots 0..339; layout stamp 77000302 @ 511.
; Edit the slot map in state_persist.orc's WRITE side and re-extract; keep
; the two in lockstep.
;==============================================================
            gkVOLpedalBypass = tab:k(0, giNeptrState)
            gkVOLpedal = tab:k(1, giNeptrState)
            gkATTK_bypass = tab:k(2, giNeptrState)
            gkATTK_mix = tab:k(3, giNeptrState)
            gkATTK_atkMs = tab:k(4, giNeptrState)
            gkATTK_sens = tab:k(5, giNeptrState)
            gkATTK_shape = tab:k(6, giNeptrState)
            gkATTK_range = tab:k(7, giNeptrState)
            gkATTK_retrigMs = tab:k(8, giNeptrState)
            gkLeslieBypass = tab:k(9, giNeptrState)
            gkLeslieSpeed = tab:k(10, giNeptrState)
            gkLeslieDepth = tab:k(11, giNeptrState)
            gkBypassRM = tab:k(12, giNeptrState)
            gkARModDepth = tab:k(13, giNeptrState)
            gkARModFreq = tab:k(14, giNeptrState)
            gkTapeBypass = tab:k(15, giNeptrState)
            gkTapeAmount = tab:k(16, giNeptrState)
            gkTapeFail = tab:k(17, giNeptrState)
            gkTapeModel = tab:k(18, giNeptrState)
            gkFormsBypass = tab:k(19, giNeptrState)
            gkFormsMix = tab:k(20, giNeptrState)
            gkFormsShift = tab:k(21, giNeptrState)
            gkFormantBypass = tab:k(22, giNeptrState)
            gkFormantMix = tab:k(23, giNeptrState)
            gkFormantShift = tab:k(24, giNeptrState)
            gkSpecDelBypass = tab:k(25, giNeptrState)
            gkSpecDelMix = tab:k(26, giNeptrState)
            gkSpecDelMaxDel = tab:k(27, giNeptrState)
            gkSpecDelFB = tab:k(28, giNeptrState)
            gkBinBypass = tab:k(29, giNeptrState)
            gkBinPedal = tab:k(30, giNeptrState)
            gkBcBypass = tab:k(31, giNeptrState)
            gkBcPedal = tab:k(32, giNeptrState)
            gkBscBypass = tab:k(33, giNeptrState)
            gkBscMix = tab:k(34, giNeptrState)
            gkBscScatter = tab:k(35, giNeptrState)
            gkBscFB = tab:k(36, giNeptrState)
            gkBscMaxDel = tab:k(37, giNeptrState)
            gkBypassH90 = tab:k(38, giNeptrState)
            gkH90mix = tab:k(39, giNeptrState)
            gkSemiH90 = tab:k(40, giNeptrState)
            gkBypassPS = tab:k(41, giNeptrState)
            gkDryPctPS = tab:k(42, giNeptrState)
            gkSemi = tab:k(43, giNeptrState)
            gkBypassGPS = tab:k(44, giNeptrState)
            gkGPSmix = tab:k(45, giNeptrState)
            gkGPSsemis = tab:k(46, giNeptrState)
            gkGPSfback = tab:k(47, giNeptrState)
            gkGPSdly = tab:k(48, giNeptrState)
            gkBypassPF = tab:k(49, giNeptrState)
            gkPFmix = tab:k(50, giNeptrState)
            gkSemiPF = tab:k(51, giNeptrState)
            gkLsyBypass = tab:k(52, giNeptrState)
            gkLSY_mix = tab:k(53, giNeptrState)
            gkLSY_loss = tab:k(54, giNeptrState)
            gkLSY_bw = tab:k(55, giNeptrState)
            gkLSY_pkt = tab:k(56, giNeptrState)
            gkRES_bypass = tab:k(57, giNeptrState)
            gkRES_mix = tab:k(58, giNeptrState)
            gkRES_onset = tab:k(59, giNeptrState)
            gkRES_fb = tab:k(60, giNeptrState)
            gkRES_oct = tab:k(61, giNeptrState)
            gkRES_frz = tab:k(62, giNeptrState)
            gkWAH_bypass = tab:k(63, giNeptrState)
            gkWAH_freq = tab:k(64, giNeptrState)
            gkWAH_res = tab:k(65, giNeptrState)
            gkAUTOMOOG_bypass = tab:k(66, giNeptrState)
            gkAUTOMOOG_sens = tab:k(67, giNeptrState)
            gkAUTOMOOG_res = tab:k(68, giNeptrState)
            gkAUTOMOOG_type = tab:k(69, giNeptrState)
            gkAUTOMOOG_level = tab:k(70, giNeptrState)
            gkVIB_bypass = tab:k(71, giNeptrState)
            gkVIB_depth = tab:k(72, giNeptrState)
            gkVIB_freq = tab:k(73, giNeptrState)
            gkVIB_fback = tab:k(74, giNeptrState)
            gkBypassChorus = tab:k(75, giNeptrState)
            gkChorusMix = tab:k(76, giNeptrState)
            gkChorusRate = tab:k(77, giNeptrState)
            gkChorusDepth = tab:k(78, giNeptrState)
            gkPHA_bypass = tab:k(79, giNeptrState)
            gkPHA_depth = tab:k(80, giNeptrState)
            gkPHA_rate = tab:k(81, giNeptrState)
            gkPHA_fback = tab:k(82, giNeptrState)
            gkPHA_stages = tab:k(83, giNeptrState)
            gkTremBypass = tab:k(84, giNeptrState)
            gkTremDepth = tab:k(85, giNeptrState)
            gkTremMode = tab:k(86, giNeptrState)
            gkTremRate = tab:k(87, giNeptrState)
            gkFLA_bypass = tab:k(88, giNeptrState)
            gkFLA_depth = tab:k(89, giNeptrState)
            gkFLA_rate = tab:k(90, giNeptrState)
            gkFLA_fback = tab:k(91, giNeptrState)
            gkOD_bypass = tab:k(92, giNeptrState)
            gkOD_drive = tab:k(93, giNeptrState)
            gkOD_tone = tab:k(94, giNeptrState)
            gkOD_wc1 = tab:k(95, giNeptrState)
            gkOD_wc2 = tab:k(96, giNeptrState)
            gkMAR_bypass = tab:k(97, giNeptrState)
            gkMAR_drive = tab:k(98, giNeptrState)
            gkMAR_tone = tab:k(99, giNeptrState)
            gkMAR_wc1 = tab:k(100, giNeptrState)
            gkMAR_wc2 = tab:k(101, giNeptrState)
            gkRMZ_bypass = tab:k(102, giNeptrState)
            gkRMZ_drive = tab:k(103, giNeptrState)
            gkRMZ_tone = tab:k(104, giNeptrState)
            gkRMZ_wc1 = tab:k(105, giNeptrState)
            gkRMZ_wc2 = tab:k(106, giNeptrState)
            gkDHG_bypass = tab:k(107, giNeptrState)
            gkDHG_drive = tab:k(108, giNeptrState)
            gkDHG_tone = tab:k(109, giNeptrState)
            gkDHG_wc1 = tab:k(110, giNeptrState)
            gkDHG_wc2 = tab:k(111, giNeptrState)
            gkMUF_bypass = tab:k(112, giNeptrState)
            gkMUF_drive = tab:k(113, giNeptrState)
            gkMUF_tone = tab:k(114, giNeptrState)
            gkMUF_wc1 = tab:k(115, giNeptrState)
            gkMUF_wc2 = tab:k(116, giNeptrState)
            gkPreamp_bypass = tab:k(117, giNeptrState)
            gkBPL_bypass = tab:k(118, giNeptrState)
            gkBPL_drive = tab:k(119, giNeptrState)
            gkBPH_bypass = tab:k(120, giNeptrState)
            gkBPH_drive = tab:k(121, giNeptrState)
            gkBDS_bypass = tab:k(122, giNeptrState)
            gkBDS_drive = tab:k(123, giNeptrState)
            gkBDS_blend = tab:k(124, giNeptrState)
            gkBDS_tone = tab:k(125, giNeptrState)
            gkCMP_bypass = tab:k(126, giNeptrState)
            gkCMP_amt = tab:k(127, giNeptrState)
            gkCMP_speed = tab:k(128, giNeptrState)
            gkCMP_makeup = tab:k(129, giNeptrState)
            gkCMP_blend = tab:k(130, giNeptrState)
            gkCMP_gate = tab:k(131, giNeptrState)
            gkBypassDelay = tab:k(132, giNeptrState)
            gkDelMix = tab:k(133, giNeptrState)
            gkDelayMS = tab:k(134, giNeptrState)
            gkFeedback = tab:k(135, giNeptrState)
            gkSemiDel = tab:k(136, giNeptrState)
            gkDlyMode = tab:k(137, giNeptrState)
            gkTdlBypass = tab:k(138, giNeptrState)
            gkTdlMix = tab:k(139, giNeptrState)
            gkTdlTimeMs = tab:k(140, giNeptrState)
            gkTdlFeedback = tab:k(141, giNeptrState)
            gkTdlFiltCtr = tab:k(142, giNeptrState)
            gkTdlFiltQ = tab:k(143, giNeptrState)
            gkTdlHeadMode = tab:k(144, giNeptrState)
            gkMvdBypass = tab:k(145, giNeptrState)
            ; POLY DLY v5: every slot clamped to its range — old presets hold v4
            ; values here (146 was mix 0..1, 151 was mode 0..15, ...).
            gkMvdNumL = limit(int(tab:k(146, giNeptrState) + 0.5), 1, 16)
            gkMvdDenL = limit(int(tab:k(147, giNeptrState) + 0.5), 1, 16)
            gkMvdNumR = limit(int(tab:k(148, giNeptrState) + 0.5), 1, 16)
            gkMvdDenR = limit(int(tab:k(149, giNeptrState) + 0.5), 1, 16)
            gkMvdIntv = limit(int(tab:k(150, giNeptrState) + 0.5), -12, 12)
            gkMvdAtt  = limit(tab:k(151, giNeptrState), 0, 1)
            gkNebBypass = tab:k(152, giNeptrState)
            gkNebMix = tab:k(153, giNeptrState)
            gkNebDensity = tab:k(154, giNeptrState)
            gkNebSize = tab:k(155, giNeptrState)
            gkShmBypass = tab:k(156, giNeptrState)
            gkShmMix = tab:k(157, giNeptrState)
            gkShmInterval = tab:k(158, giNeptrState)
            gkShmFbAmt = tab:k(159, giNeptrState)
            gkFrzBypass = tab:k(160, giNeptrState)
            gkFrzBedLevel = tab:k(161, giNeptrState)
            gkFrzDamp = tab:k(162, giNeptrState)
            gkFrzListenGain = tab:k(163, giNeptrState)
            gkGvbBypass = tab:k(164, giNeptrState)
            gkGvbMix = tab:k(165, giNeptrState)
            gkGvbSpacing = tab:k(166, giNeptrState)
            gkGvbDecay = tab:k(167, giNeptrState)
            gkverbBypass = tab:k(168, giNeptrState)
            gkverbMix = tab:k(169, giNeptrState)
            gkReverbFB = tab:k(170, giNeptrState)
            gkReverbDamp = tab:k(171, giNeptrState)
            gkGranBypass = tab:k(172, giNeptrState)
            gkGranMix = tab:k(173, giNeptrState)
            gkGranDens = tab:k(174, giNeptrState)
            gkGranSize = tab:k(175, giNeptrState)
            gkGranPitch = tab:k(176, giNeptrState)
            gkMOOD_Bypass = tab:k(177, giNeptrState)
            gkMOOD_Mix = tab:k(178, giNeptrState)
            gkMOOD_Clock = tab:k(179, giNeptrState)
            gkMOOD_Dens = tab:k(180, giNeptrState)
            gkMOOD_Dly = tab:k(181, giNeptrState)
            gkOCT_bypass = tab:k(182, giNeptrState)
            gkOCT_sub = tab:k(183, giNeptrState)
            gkOCT_tone = tab:k(184, giNeptrState)
            gkOCT_char = tab:k(185, giNeptrState)
            gkENV_bypass = tab:k(186, giNeptrState)
            gkENV_wet = tab:k(187, giNeptrState)
            gkENV_sens = tab:k(188, giNeptrState)
            gkENV_mode = tab:k(189, giNeptrState)
            gkENV_att = tab:k(190, giNeptrState)
            gkENV_dec = tab:k(191, giNeptrState)
            gkTSY_bypass = tab:k(192, giNeptrState)
            gkTSY_mix = tab:k(193, giNeptrState)
            gkTSY_cut = tab:k(194, giNeptrState)
            gkTSY_res = tab:k(195, giNeptrState)
            gkTSY_glide = tab:k(196, giNeptrState)
            gkTSY_sub = tab:k(197, giNeptrState)
            gkTSY_det = tab:k(198, giNeptrState)
            gkPLL_bypass = tab:k(199, giNeptrState)
            gkPLL_mix = tab:k(200, giNeptrState)
            gkPLL_intv = tab:k(201, giNeptrState)
            gkPLL_lock = tab:k(202, giNeptrState)
            gkPLL_vmix = tab:k(203, giNeptrState)
            gkTEQ_bypass = tab:k(204, giNeptrState)
            gkTEQ_mid = tab:k(205, giNeptrState)
            gkTEQ_freq = tab:k(206, giNeptrState)
            gkTEQ_tilt = tab:k(207, giNeptrState)
            gkTEQ_low = tab:k(208, giNeptrState)
            gkTEQ_high = tab:k(209, giNeptrState)
            gkFBK_bypass = tab:k(210, giNeptrState)
            gkFBK_level = tab:k(211, giNeptrState)
            gkFBK_harm = tab:k(212, giNeptrState)
            gkFBK_bloom = tab:k(213, giNeptrState)
            gkFBK_vib = tab:k(214, giNeptrState)
            gkDIF_bypass = tab:k(215, giNeptrState)
            gkDIF_wet = tab:k(216, giNeptrState)
            gkDIF_depth = tab:k(217, giNeptrState)
            gkDIF_rate = tab:k(218, giNeptrState)
            gkDIF_duck = tab:k(219, giNeptrState)
            gkCFB_bypass = tab:k(220, giNeptrState)
            gkCFB_ret = tab:k(221, giNeptrState)
            gkCFB_gain = tab:k(222, giNeptrState)
            gkCFB_tone = tab:k(223, giNeptrState)
            gkCFB_ceil = tab:k(224, giNeptrState)
            gkBLP_bypass = tab:k(225, giNeptrState)
            gkBLP_Motion = tab:k(226, giNeptrState)
            gkBLP_Scat = tab:k(227, giNeptrState)
            gkBLP_Size = tab:k(228, giNeptrState)
            gkBLP_Char = tab:k(229, giNeptrState)
            gkONW_bypass = tab:k(230, giNeptrState)
            gkONW_mix = tab:k(231, giNeptrState)
            gkONW_blend = tab:k(232, giNeptrState)
            gkONW_size = tab:k(233, giNeptrState)
            gkONW_follow = tab:k(234, giNeptrState)
            gkONW_density = tab:k(235, giNeptrState)
            gkONW_err = tab:k(236, giNeptrState)
            gkONW_oct = tab:k(237, giNeptrState)
            gkHabitBypass = tab:k(238, giNeptrState)
            gkHabitMix = tab:k(239, giNeptrState)
            gkHabitSize = tab:k(240, giNeptrState)
            gkHabitRepeats = tab:k(241, giNeptrState)
            gkHabitBlend = tab:k(242, giNeptrState)
            gkHab_rec = tab:k(243, giNeptrState)
            gkMD2_bypass = tab:k(244, giNeptrState)
            gkMD2_Mix = tab:k(245, giNeptrState)
            gkMD2_Clock = tab:k(246, giNeptrState)
            gkMD2_Length = tab:k(247, giNeptrState)
            gkMD2_Gen = tab:k(248, giNeptrState)
            gkIMP_bypass = tab:k(249, giNeptrState)
            gkIMP_mix = tab:k(250, giNeptrState)
            gkIMP_prob = tab:k(251, giNeptrState)
            gkIMP_dec = tab:k(252, giNeptrState)
            gkIMP_tone = tab:k(253, giNeptrState)
            gkBBcut_bypass = tab:k(254, giNeptrState)
            gkBBcut_drywet = tab:k(255, giNeptrState)
            gkBBcut_prob = tab:k(256, giNeptrState)
            gkBBcut_fltMix = tab:k(257, giNeptrState)
            gkWGuide_bypass = tab:k(258, giNeptrState)
            gkWGuide_drywet = tab:k(259, giNeptrState)
            gkRPT_prob = tab:k(260, giNeptrState)
            gkRPT_char = tab:k(261, giNeptrState)
            gkRPT_len = tab:k(262, giNeptrState)
            gkSqMod_bypass = tab:k(263, giNeptrState)
            gkSqMod_drywet = tab:k(264, giNeptrState)
            gkCHP_prob = tab:k(265, giNeptrState)
            gkCHP_rate = tab:k(266, giNeptrState)
            gkCHP_shape = tab:k(267, giNeptrState)
            gkFShift_bypass = tab:k(268, giNeptrState)
            gkFShift_drywet = tab:k(269, giNeptrState)
            gkRVS_prob = tab:k(270, giNeptrState)
            gkRVS_len = tab:k(271, giNeptrState)
            gkNAMActiveRow = tab:k(272, giNeptrState)
            gkNAMBypass = tab:k(273, giNeptrState)
            gkNAMInGain = tab:k(274, giNeptrState)
            gkNAMOutGain = tab:k(275, giNeptrState)
            gkNAMRowSel[0] = tab:k(276, giNeptrState)
            gkNAMRowSel[1] = tab:k(277, giNeptrState)
            gkNAMRowSel[2] = tab:k(278, giNeptrState)
            gkNAMRowSel[3] = tab:k(279, giNeptrState)
            gkNAMRowSel[4] = tab:k(280, giNeptrState)
            ; 2026-09-01: gkNAMProfileIdx is NOT a persisted slot, so after
            ; every boot the row/variant SELECTION was restored (UI showed
            ; your amp) while the actually-loaded model silently stayed at
            ; the orchestra default (profile 5, deluxe clean). Pressing the
            ; already-active row's button only toggles bypass, so the amp
            ; could never change: "on/off works, switching does nothing".
            ; Recompute the profile from the restored row+variant instead.
            kNamRowR = gkNAMActiveRow
            if (kNamRowR >= 0 && kNamRowR <= 4) then
                gkNAMProfileIdx = giNAMRowBase[kNamRowR] + gkNAMRowSel[kNamRowR]
            endif
            gkIRActiveRow = tab:k(281, giNeptrState)
            gkIRBypass = tab:k(282, giNeptrState)
            gkIRDamp = tab:k(283, giNeptrState)
            gkIRLevel = tab:k(284, giNeptrState)
            gkIRSel[0] = tab:k(285, giNeptrState)
            gkIRRowMix[0] = tab:k(286, giNeptrState)
            gkIRSel[1] = tab:k(287, giNeptrState)
            gkIRRowMix[1] = tab:k(288, giNeptrState)
            gkIRSel[2] = tab:k(289, giNeptrState)
            gkIRRowMix[2] = tab:k(290, giNeptrState)
            gkIRSel[3] = tab:k(291, giNeptrState)
            gkIRRowMix[3] = tab:k(292, giNeptrState)
            gkIRSel[4] = tab:k(293, giNeptrState)
            gkIRRowMix[4] = tab:k(294, giNeptrState)
            gkStereoIn = tab:k(295, giNeptrState)
            gkMasterVol = tab:k(296, giNeptrState)
            gkLpSpIdx1 = tab:k(297, giNeptrState)
            gkLpSpIdx2 = tab:k(298, giNeptrState)
            gkLpSpIdx3 = tab:k(299, giNeptrState)
            gkLpSpIdx4 = tab:k(300, giNeptrState)
            gkLpSpIdx5 = tab:k(301, giNeptrState)
            ; 2026-09-01: UI ROUTING state — menu, encoder target, pedal
            ; target. These were never persisted, so after every boot the
            ; encoders and pedal were routed nowhere until an effect was
            ; re-toggled ("it forgot my settings" even when the ftab
            ; restored). Stored +1 so 0 means "absent" (old files have 0
            ; here from the table init and are skipped cleanly).
            kStMenu = tab:k(302, giNeptrState)
            if (kStMenu > 0.5) then
                gkCurrentMenu = kStMenu - 1
            endif
            kStLt = tab:k(303, giNeptrState)
            if (kStLt > 0.5) then
                gkLastToggle = kStLt - 1
            endif
            kStLt29 = tab:k(304, giNeptrState)
            if (kStLt29 > 0.5) then
                gkLastToggle29 = kStLt29 - 1
            endif
            ; UTILITY page (2026-09-01): master-out volume + manual BPM. Stored
            ; +epsilon-free (plain values); old files have 0 here from the table
            ; init, so guard against a bogus 0 restore that would mute output /
            ; zero the tempo.
            kStMOut = tab:k(305, giNeptrState)
            if (kStMOut > 0.001) then
                gkMasterOut = kStMOut
            endif
            kStBpm = tab:k(306, giNeptrState)
            if (kStBpm >= 40) then
                gkManualBpm = kStBpm
            endif
            ; POLY DLY v5 Decay + Repeats (clamped; slots held v4 preset/decay-mode)
            gkMvdDec  = limit(tab:k(307, giNeptrState), 0, 1)
            gkMvdReps = limit(int(tab:k(308, giNeptrState) + 0.5), 1, 8)
            ; hyperGuitar lane mirror (2026-10-03), stored +1 (0 = absent, so
            ; older files/presets are skipped and keep the live mirror)
            kStHgOn0 = tab:k(309, giNeptrState)
            if (kStHgOn0 > 0.5) then
                gkHgLaneOn[0] = kStHgOn0 - 1
            endif
            kStHgOn1 = tab:k(310, giNeptrState)
            if (kStHgOn1 > 0.5) then
                gkHgLaneOn[1] = kStHgOn1 - 1
            endif
            kStHgOn2 = tab:k(311, giNeptrState)
            if (kStHgOn2 > 0.5) then
                gkHgLaneOn[2] = kStHgOn2 - 1
            endif
            kStHgOn3 = tab:k(312, giNeptrState)
            if (kStHgOn3 > 0.5) then
                gkHgLaneOn[3] = kStHgOn3 - 1
            endif
            kStHgOn4 = tab:k(313, giNeptrState)
            if (kStHgOn4 > 0.5) then
                gkHgLaneOn[4] = kStHgOn4 - 1
            endif
            kStHgIx0 = tab:k(314, giNeptrState)
            if (kStHgIx0 > 0.5) then
                gkHgLaneIdx[0] = kStHgIx0 - 1
            endif
            kStHgIx1 = tab:k(315, giNeptrState)
            if (kStHgIx1 > 0.5) then
                gkHgLaneIdx[1] = kStHgIx1 - 1
            endif
            kStHgIx2 = tab:k(316, giNeptrState)
            if (kStHgIx2 > 0.5) then
                gkHgLaneIdx[2] = kStHgIx2 - 1
            endif
            kStHgIx3 = tab:k(317, giNeptrState)
            if (kStHgIx3 > 0.5) then
                gkHgLaneIdx[3] = kStHgIx3 - 1
            endif
            kStHgIx4 = tab:k(318, giNeptrState)
            if (kStHgIx4 > 0.5) then
                gkHgLaneIdx[4] = kStHgIx4 - 1
            endif
            ; VOCAL chain (2026-10-22), 319..330 (+342 voc_pre_db), stored +100 (0 = absent; older
            ; files/presets skip). Channels: see effects/vocal_chain.orc.
            kStV0 = tab:k(319, giNeptrState)
            if (kStV0 > 50) then
                chnset kStV0 - 100, "voc_on"
            endif
            kStV1 = tab:k(320, giNeptrState)
            if (kStV1 > 50) then
                chnset kStV1 - 100, "voc_hpf_hz"
            endif
            kStV2 = tab:k(321, giNeptrState)
            if (kStV2 > 50) then
                chnset kStV2 - 100, "voc_mid_db"
            endif
            kStV3 = tab:k(322, giNeptrState)
            if (kStV3 > 50) then
                chnset kStV3 - 100, "voc_mid_hz"
            endif
            kStV4 = tab:k(323, giNeptrState)
            if (kStV4 > 50) then
                chnset kStV4 - 100, "voc_mid_q"
            endif
            kStV5 = tab:k(324, giNeptrState)
            if (kStV5 > 50) then
                chnset kStV5 - 100, "voc_dly_ms"
            endif
            kStV6 = tab:k(325, giNeptrState)
            if (kStV6 > 50) then
                chnset kStV6 - 100, "voc_dly_fb"
            endif
            kStV7 = tab:k(326, giNeptrState)
            if (kStV7 > 50) then
                chnset kStV7 - 100, "voc_dly_wet"
            endif
            kStV8 = tab:k(327, giNeptrState)
            if (kStV8 > 50) then
                chnset kStV8 - 100, "voc_rev_fb"
            endif
            kStV9 = tab:k(328, giNeptrState)
            if (kStV9 > 50) then
                chnset kStV9 - 100, "voc_rev_cut"
            endif
            kStV10 = tab:k(329, giNeptrState)
            if (kStV10 > 50) then
                chnset kStV10 - 100, "voc_rev_wet"
            endif
            kStV11 = tab:k(330, giNeptrState)
            if (kStV11 > 50) then
                chnset kStV11 - 100, "voc_level"
            endif
            ; MIXER page (2026-10-22), 331..339, stored +100 (0 = absent; skipped).
            kStM0 = tab:k(331, giNeptrState)
            if (kStM0 > 50) then
                chnset kStM0 - 100, "mx_gtr_vol"
            endif
            kStM1 = tab:k(332, giNeptrState)
            if (kStM1 > 50) then
                chnset kStM1 - 100, "mx_gtr_pan"
            endif
            kStM2 = tab:k(333, giNeptrState)
            if (kStM2 > 50) then
                chnset kStM2 - 100, "mx_gtr_wid"
            endif
            kStM3 = tab:k(334, giNeptrState)
            if (kStM3 > 50) then
                chnset kStM3 - 100, "mx_tpt_vol"
            endif
            kStM4 = tab:k(335, giNeptrState)
            if (kStM4 > 50) then
                chnset kStM4 - 100, "mx_tpt_pan"
            endif
            kStM5 = tab:k(336, giNeptrState)
            if (kStM5 > 50) then
                chnset kStM5 - 100, "mx_tpt_wid"
            endif
            kStM6 = tab:k(337, giNeptrState)
            if (kStM6 > 50) then
                chnset kStM6 - 100, "mx_pmr_vol"
            endif
            kStM7 = tab:k(338, giNeptrState)
            if (kStM7 > 50) then
                chnset kStM7 - 100, "mx_pmr_pan"
            endif
            kStM8 = tab:k(339, giNeptrState)
            if (kStM8 > 50) then
                chnset kStM8 - 100, "mx_pmr_wid"
            endif
            kStPre = tab:k(342, giNeptrState)
            if (kStPre > 50) then
                chnset kStPre - 100, "voc_pre_db"
            endif
            kStLp = tab:k(341, giNeptrState)
            if (kStLp > 50) then
                chnset kStLp - 100, "mx_lp_vol"
            endif
            ; NAM quality (UTILITY, 2026-10-23), 340, stored +100 (0 = absent = FULL).
            kStNs = tab:k(340, giNeptrState)
            if (kStNs > 50) then
                chnset kStNs - 100, "nam_size"
            endif
