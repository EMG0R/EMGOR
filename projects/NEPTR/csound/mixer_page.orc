;==============================================================
; MIXER_PAGE.ORC - the MIXER menu (menu 22): four stereo channel strips that
; sum into the MAIN outs. Encoder-swappable like every other menu page; nothing
; is hard-mapped to the touchscreen.
;
; CHANNELS (input pairs on the 8x8 HAT)      row / slot   primary   A (prev enc)  B (next enc)
;   0 GUITAR   in 1,2  rack output           row0 slot4   volume    pan           width
;   1 TRUMPET  in 7,8  (reserved, level)     row1 slot3   volume    pan           width
;   2 VOCAL    in 5,6  vocal chain           row2 slot2   volume    delay wet     reverb wet
;   3 PMOR     in 3,4  groovebox, dry        row3 slot1   volume    pan           width
; (2026-10-24: PMOR moved 7,8 -> 3,4, TRUMPET took 7,8. Keeps PMOR off the pair
;  the HAT watchdog pilot uses, and matches where Emory patched it.)
;   (4) LOOPER  looper bus   row4 slot0   volume    -             -
; Slot 0 (rightmost encoder) = LOOPER master volume (mx_lp_vol, applied to
; gaLpOutL/R in signal_chain.orc). No A/B, no select.
;
; CONTROL MODEL (same shape as every menu): the channel's encoder ROTATION is its
; volume; its BUTTON selects the channel (chn mx_sel). The two modulating
; encoders (CC20 = E7 "previous", CC23 = E5 "next") then drive the SELECTED
; channel's A / B params. Dispatch lives in handler/dispatch/*.orc as
; `gkCurrentMenu == $MX_MENU` branches that expand the macros below.
;
; STATE = chn_k channels (read/written with chnget/chnset, like voc_*):
;   mx_sel                       0..3   selected channel (not persisted)
;   mx_gtr_vol mx_tpt_vol mx_pmr_vol  0..1.5 linear (1 = unity)  defaults 1 / 0 / 1
;   mx_lp_vol                    0..1.5 looper master (default 0.85)
;   mx_{gtr,tpt,pmr}_pan         -1..1   balance (0 = centre, no gain added)
;   mx_{gtr,tpt,pmr}_wid          0..1   stereo width (1 = untouched, 0 = mono)
;   VOCAL volume IS voc_level; its A/B ARE voc_dly_wet / voc_rev_wet. There is no
;   second copy of those: the full VOCAL page (menu 21) keeps the rest of the
;   vocal chain (EQ, delay time/fb, reverb fb/cut, ON/DRY).
; Nothing here adds gain above unity-by-default; no AGC. Persisted in
; giNeptrState slots 331..339 + 341 (+100), recall-excluded (rig-level, not a preset).
;==============================================================

#define MX_MENU #22#

chn_k "mx_sel",     3, 2, 0,   0,  3
chn_k "mx_gtr_vol", 3, 2, 1,   0,  1.5
chn_k "mx_gtr_pan", 3, 2, 0,  -1,  1
chn_k "mx_gtr_wid", 3, 2, 1,   0,  1
chn_k "mx_tpt_vol", 3, 2, 0,   0,  1.5
chn_k "mx_tpt_pan", 3, 2, 0,  -1,  1
chn_k "mx_tpt_wid", 3, 2, 1,   0,  1
chn_k "mx_pmr_vol", 3, 2, 1,   0,  1.5
chn_k "mx_pmr_pan", 3, 2, 0,  -1,  1
chn_k "mx_pmr_wid", 3, 2, 1,   0,  1
chn_k "mx_lp_vol",  3, 2, 0.85, 0, 1.5
; chn_k's default is only a hint for hosts: the channel itself starts at 0.
; Set every default here, at load (state restore overrides it later).
chnset 0, "mx_sel"
chnset 1, "mx_gtr_vol"
chnset 0, "mx_gtr_pan"
chnset 1, "mx_gtr_wid"
chnset 0, "mx_tpt_vol"
chnset 0, "mx_tpt_pan"
chnset 1, "mx_tpt_wid"
chnset 1, "mx_pmr_vol"
chnset 0, "mx_pmr_pan"
chnset 1, "mx_pmr_wid"
chnset 0.85, "mx_lp_vol"

; ---- dispatch macros (expand inside instr 99; kVal = 127 CW / 0 CCW) ----
; one encoder tick on chn_k channel `chn`, clamped lo..hi, `st` per detent
#define MX_ROTC(uid'chn'lo'hi'st)#
  kMxR_$uid. chnget "$chn."
  if (kVal > 64) then
    kMxR_$uid. = min(kMxR_$uid. + $st, $hi)
  elseif (kVal < 64) then
    kMxR_$uid. = max(kMxR_$uid. - $st, $lo)
  endif
  chnset kMxR_$uid., "$chn."
#
; primary encoder: channel volume. ch = 0 GUITAR 1 TRUMPET 2 VOCAL 3 PMOR
#define MX_VOL_ROT(ch)#
  if ($ch == 0) then
    $MX_ROTC(gv'mx_gtr_vol'0'1.5'0.025)
  elseif ($ch == 1) then
    $MX_ROTC(tv'mx_tpt_vol'0'1.5'0.025)
  elseif ($ch == 2) then
    $MX_ROTC(vv'voc_level'0'4'0.05)
  else
    $MX_ROTC(pv'mx_pmr_vol'0'1.5'0.025)
  endif
#
; encoder click: select the channel the two modulating encoders now drive
#define MX_SELECT(ch)#
  chnset $ch, "mx_sel"
  printks "MIXER: channel %d selected\n", 0, $ch
#
; CC20 (previous encoder) -> selected channel's A: pan / (vocal) delay wet
#define MX_A_ROT #
  kMxSelA chnget "mx_sel"
  if (kMxSelA < 0.5) then
    $MX_ROTC(ga'mx_gtr_pan'-1'1'0.05)
  elseif (kMxSelA < 1.5) then
    $MX_ROTC(ta'mx_tpt_pan'-1'1'0.05)
  elseif (kMxSelA < 2.5) then
    $MX_ROTC(va'voc_dly_wet'0'1'0.02)
  else
    $MX_ROTC(pa'mx_pmr_pan'-1'1'0.05)
  endif
#
; CC23 (next encoder) -> selected channel's B: width / (vocal) reverb wet
#define MX_B_ROT #
  kMxSelB chnget "mx_sel"
  if (kMxSelB < 0.5) then
    $MX_ROTC(gb'mx_gtr_wid'0'1'0.05)
  elseif (kMxSelB < 1.5) then
    $MX_ROTC(tb'mx_tpt_wid'0'1'0.05)
  elseif (kMxSelB < 2.5) then
    $MX_ROTC(vb'voc_rev_wet'0'1'0.02)
  else
    $MX_ROTC(pb'mx_pmr_wid'0'1'0.05)
  endif
#

#ifdef MIXER
; One stereo strip: balance + width + volume, all portk-smoothed (no zipper).
; Defaults are exactly transparent (pan 0, width 1) so unity volume = bit-identical.
opcode MxStrip, aa, aakkk
  aL, aR, kVol, kPan, kWid xin
  kV  portk limit(kVol, 0, 1.5), 0.03
  kP  portk limit(kPan, -1, 1), 0.03
  kW  portk limit(kWid, 0, 1), 0.03
  aM  = (aL + aR) * 0.5
  aS  = (aL - aR) * 0.5 * kW
  kGL = kV * (1 - max(kP, 0))
  kGR = kV * (1 + min(kP, 0))
  xout (aM + aS) * kGL, (aM - aS) * kGR
endop

; The page's audio stage, called from instr 1 once per k-cycle.
;   aGL/aGR = rack output (GUITAR), already through the whole FX chain.
; Returns main L/R = guitar strip + trumpet + vocal + PMOR strips. Vocal is also
; put on direct buses 5,6 (as before), at its mixer volume (voc_level).
opcode MixerPage, aa, aa
  aGL, aGR xin
  kGv chnget "mx_gtr_vol"
  kGp chnget "mx_gtr_pan"
  kGw chnget "mx_gtr_wid"
  aML, aMR MxStrip aGL, aGR, kGv, kGp, kGw
  if (giMxNi >= 8 && giMxNo >= 2) then
    kTv chnget "mx_tpt_vol"
    kTp chnget "mx_tpt_pan"
    kTw chnget "mx_tpt_wid"
    aTL inch 7
    aTR inch 8
    aTL, aTR MxStrip aTL, aTR, kTv, kTp, kTw
    aML = aML + aTL
    aMR = aMR + aTR
  endif
  if (giMxNi >= 6 && giMxNo >= 2) then
    aVIL inch 5
    aVIR inch 6
    aVOL, aVOR VocalChain aVIL, aVIR        ; voc_level applied inside
    ; vocal meters: input (pre-chain) and output (post level) peaks, 25 Hz
    kVmTrig metro 25
    kVmIL max_k aVIL, kVmTrig, 1
    kVmIR max_k aVIR, kVmTrig, 1
    kVmOL max_k aVOL, kVmTrig, 1
    kVmOR max_k aVOR, kVmTrig, 1
    OSCsend kVmTrig, "127.0.0.1", 8000, "/voc_meter", "ffff", kVmIL, kVmIR, kVmOL, kVmOR
    aML = aML + aVOL
    aMR = aMR + aVOR
    ; VOCALS INTO THE LOOPER (2026-11-01): gaLpIn is latched at signal_chain.orc
    ; "gaLpInL = aPrevL" BEFORE this runs, so the voice never reached the loops.
    ; Add the finished vocal strip (post chain + voc_level) to the looper input;
    ; the looper instrs read gaLpIn after instr 1 finishes this cycle. The loops'
    ; own playback joins aPrevL later, so they never re-record themselves.
    gaLpInL = gaLpInL + aVOL
    gaLpInR = gaLpInR + aVOR
    if (giMxNo >= 6) then
      gaMxDir[4] = gaMxDir[4] + aVOL
      gaMxDir[5] = gaMxDir[5] + aVOR
    endif
  endif
  if (giMxNi >= 4 && giMxNo >= 2) then
    kPv chnget "mx_pmr_vol"
    kPp chnget "mx_pmr_pan"
    kPw chnget "mx_pmr_wid"
    aPL inch 3
    aPR inch 4
    aPL, aPR MxStrip aPL, aPR, kPv, kPp, kPw
    aML = aML + aPL
    aMR = aMR + aPR
  endif
  xout aML, aMR
endop
#end
