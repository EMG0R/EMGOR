;==============================================================
; MIXER.ORC - N-in / N-out FULL routing matrix, INSIDE Csound. N = nchnls_i / nchnls
; (the card's channel count; capped at MX_MAXCH by the 256-slot pool table).
;
; Compiled only with --omacro:MIXER=1 (neptrPhase4.csd; set by demiurge-run-csound
; from live.conf csound_extra, or automatically when ~/.config/neptr/routing exists).
; Without the macro none of this is seen and the patch is byte-for-byte unchanged.
; It is the sync_layer=off path: Csound owns the card, so the matrix IS its input
; and output section. (sync_layer=on: same pool params, see docs/MIXER.md.)
;
; BUSES: the NEPTR rack takes two inputs (rackL, rackR) and produces two outputs
; (main L / main R). Any physical input may feed rackL, rackR and/or ANY output
; directly; any output may take main L and/or main R. Many-to-many, all cells are
; independent gains. "uss" is a separate engine process: unreachable from in here.
;
; POOL PARAMS (0..1; unity = 0.75; dB curve below), n/k are 1-based:
;   /mixer/in/<n>/gain  /mixer/in/<n>/mute          per input channel
;   /mixer/in/<n>/to/rackL  /to/rackR  /to/out/<k>  matrix cell: input n -> bus/output
;   /mixer/out/<k>/gain /mixer/out/<k>/mute         per output channel
;   /mixer/out/<k>/from/mainL  /from/mainR          matrix cell: main bus -> output k
; METERS (two-way, /pout, change-only ~12 Hz; value = (dBFS+60)/60 in 0..1):
;   /mixer/in/<n>/peak|rms (pre-fader)   /mixer/out/<k>/peak|rms (post-fader)
; GAIN CURVE: 0 = silence, 0..0.75 = -60..0 dB, 0.75..1 = 0..+12 dB. ampdb(0) = 1.0,
; so every default multiply is exactly 1.0: defaults are bit-identical to the plain
; patch. MUTE: v >= 0.5.
; DEFAULTS (identity): in1->rackL, in2->rackR, mainL->out1, mainR->out2, all else 0.
;
; BOOT MAP (macros; the wrapper builds them from ~/.config/neptr/routing), 1-based
; comma lists, a present macro REPLACES that default:
;   MIXER_MAIN_L / MIXER_MAIN_R  outputs that carry main L / R
;   MIXER_RACK_L / MIXER_RACK_R  physical inputs that feed the rack's L / R
;   MIXER_DIRECT                 flat pairs in,out,in,out,...  (input n straight to output k)
;   MIXER_IN_GAIN/_MUTE, MIXER_OUT_GAIN/_MUTE  per-channel pool values
;==============================================================
#include "params/csound/demiurge_params.udo"

#ifndef MIXER_PORT
#define MIXER_PORT #9030#
#end
#define MX_MAXCH #12#

giMxNi    init min($NEPTR_NCHNLS_I, $MX_MAXCH)
giMxNo    init min($NEPTR_NCHNLS, $MX_MAXCH)
giMxUnity init 0.75
giMxSI    init giMxNo + 4                     ; slots per input: gain, mute, rackL, rackR, N outs
giMxSO    init 4                              ; slots per output: gain, mute, mainL, mainR

; ---- boot state (pool values), then macro overrides ----
giMxCIG[] init $MX_MAXCH                      ; per-input gain
giMxCIM[] init $MX_MAXCH
giMxCOG[] init $MX_MAXCH
giMxCOM[] init $MX_MAXCH
giMxCI[]  init $MX_MAXCH * ($MX_MAXCH + 2)    ; input cells [n*(No+2) + d], d: 0 rackL 1 rackR 2+k out k
giMxCO[]  init $MX_MAXCH * 2                  ; output cells [k*2 + {0 mainL, 1 mainR}]
giMxj init 0
giMxk init 0
while giMxj < $MX_MAXCH do
  giMxCIG[giMxj] = giMxUnity
  giMxCOG[giMxj] = giMxUnity
  giMxj += 1
od
if giMxNi >= 1 then
  giMxCI[0] = giMxUnity
endif
if giMxNi >= 2 then
  giMxCI[giMxNo + 2 + 1] = giMxUnity
endif
giMxCO[0] = giMxUnity
if giMxNo >= 2 then
  giMxCO[3] = giMxUnity
endif
#ifdef MIXER_MAIN_L
giMxT[] fillarray $MIXER_MAIN_L
giMxj = 0
while giMxj < $MX_MAXCH do
  giMxCO[giMxj*2] = 0
  giMxj += 1
od
giMxj = 0
while giMxj < lenarray(giMxT) do
  if giMxT[giMxj] >= 1 && giMxT[giMxj] <= giMxNo then
    giMxCO[(giMxT[giMxj] - 1) * 2] = giMxUnity
  endif
  giMxj += 1
od
#end
#ifdef MIXER_MAIN_R
giMxT[] fillarray $MIXER_MAIN_R
giMxj = 0
while giMxj < $MX_MAXCH do
  giMxCO[giMxj*2 + 1] = 0
  giMxj += 1
od
giMxj = 0
while giMxj < lenarray(giMxT) do
  if giMxT[giMxj] >= 1 && giMxT[giMxj] <= giMxNo then
    giMxCO[(giMxT[giMxj] - 1) * 2 + 1] = giMxUnity
  endif
  giMxj += 1
od
#end
#ifdef MIXER_RACK_L
giMxT[] fillarray $MIXER_RACK_L
giMxj = 0
while giMxj < $MX_MAXCH do
  giMxCI[giMxj*(giMxNo+2)] = 0
  giMxj += 1
od
giMxj = 0
while giMxj < lenarray(giMxT) do
  if giMxT[giMxj] >= 1 && giMxT[giMxj] <= giMxNi then
    giMxCI[(giMxT[giMxj] - 1) * (giMxNo + 2)] = giMxUnity
  endif
  giMxj += 1
od
#end
#ifdef MIXER_RACK_R
giMxT[] fillarray $MIXER_RACK_R
giMxj = 0
while giMxj < $MX_MAXCH do
  giMxCI[giMxj*(giMxNo+2) + 1] = 0
  giMxj += 1
od
giMxj = 0
while giMxj < lenarray(giMxT) do
  if giMxT[giMxj] >= 1 && giMxT[giMxj] <= giMxNi then
    giMxCI[(giMxT[giMxj] - 1) * (giMxNo + 2) + 1] = giMxUnity
  endif
  giMxj += 1
od
#end
#ifdef MIXER_DIRECT
giMxT[] fillarray $MIXER_DIRECT
giMxj = 0
while giMxj + 1 < lenarray(giMxT) do
  if giMxT[giMxj] >= 1 && giMxT[giMxj] <= giMxNi && giMxT[giMxj+1] >= 1 && giMxT[giMxj+1] <= giMxNo then
    giMxCI[(giMxT[giMxj] - 1) * (giMxNo + 2) + 1 + giMxT[giMxj+1]] = giMxUnity
  endif
  giMxj += 2
od
#end
#ifdef MIXER_IN_GAIN
giMxT[] fillarray $MIXER_IN_GAIN
giMxj = 0
while giMxj < min(lenarray(giMxT), $MX_MAXCH) do
  giMxCIG[giMxj] = giMxT[giMxj]
  giMxj += 1
od
#end
#ifdef MIXER_IN_MUTE
giMxT[] fillarray $MIXER_IN_MUTE
giMxj = 0
while giMxj < min(lenarray(giMxT), $MX_MAXCH) do
  giMxCIM[giMxj] = giMxT[giMxj]
  giMxj += 1
od
#end
#ifdef MIXER_OUT_GAIN
giMxT[] fillarray $MIXER_OUT_GAIN
giMxj = 0
while giMxj < min(lenarray(giMxT), $MX_MAXCH) do
  giMxCOG[giMxj] = giMxT[giMxj]
  giMxj += 1
od
#end
#ifdef MIXER_OUT_MUTE
giMxT[] fillarray $MIXER_OUT_MUTE
giMxj = 0
while giMxj < min(lenarray(giMxT), $MX_MAXCH) do
  giMxCOM[giMxj] = giMxT[giMxj]
  giMxj += 1
od
#end

; ---- register the pool slots (same registry the dparam opcode uses) ----
gSMxMtr[] init 4 * $MX_MAXCH      ; meter paths: [dir*2*MAX + ch*2 + {0 peak,1 rms}]
giMxBase  init giDParamN
giMxj = 0
while giMxj < giMxNi do
  gSDParamPaths[giDParamN] sprintf "/mixer/in/%d/gain", giMxj + 1
  tabw_i giMxCIG[giMxj], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/in/%d/mute", giMxj + 1
  tabw_i giMxCIM[giMxj], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/in/%d/to/rackL", giMxj + 1
  tabw_i giMxCI[giMxj*(giMxNo+2)], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/in/%d/to/rackR", giMxj + 1
  tabw_i giMxCI[giMxj*(giMxNo+2)+1], giDParamN, giDParamTab
  giDParamN += 1
  giMxk = 0
  while giMxk < giMxNo do
    gSDParamPaths[giDParamN] sprintf "/mixer/in/%d/to/out/%d", giMxj + 1, giMxk + 1
    tabw_i giMxCI[giMxj*(giMxNo+2)+2+giMxk], giDParamN, giDParamTab
    giDParamN += 1
    giMxk += 1
  od
  gSMxMtr[giMxj*2]   sprintf "/mixer/in/%d/peak", giMxj + 1
  gSMxMtr[giMxj*2+1] sprintf "/mixer/in/%d/rms", giMxj + 1
  giMxj += 1
od
giMxBaseO init giDParamN
giMxj = 0
while giMxj < giMxNo do
  gSDParamPaths[giDParamN] sprintf "/mixer/out/%d/gain", giMxj + 1
  tabw_i giMxCOG[giMxj], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/out/%d/mute", giMxj + 1
  tabw_i giMxCOM[giMxj], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/out/%d/from/mainL", giMxj + 1
  tabw_i giMxCO[giMxj*2], giDParamN, giDParamTab
  giDParamN += 1
  gSDParamPaths[giDParamN] sprintf "/mixer/out/%d/from/mainR", giMxj + 1
  tabw_i giMxCO[giMxj*2+1], giDParamN, giDParamTab
  giDParamN += 1
  gSMxMtr[2*$MX_MAXCH + giMxj*2]   sprintf "/mixer/out/%d/peak", giMxj + 1
  gSMxMtr[2*$MX_MAXCH + giMxj*2+1] sprintf "/mixer/out/%d/rms", giMxj + 1
  giMxj += 1
od
if giDParamN > giDParamMax then
  prints "mixer: %d slots exceed the pool table (%d)\n", giDParamN, giDParamMax
endif

; ---- audio-rate state ----
gaMxDir[]   init $MX_MAXCH        ; inputs routed direct, per output
gaMxMtrIn[] init $MX_MAXCH        ; meter feeds (copied; read by instr 281)
gaMxMtrOut[] init $MX_MAXCH
gkMxSmIn[]  init $MX_MAXCH        ; smoothed per-channel gains (previous block)
gkMxSmOut[] init $MX_MAXCH
gkMxSmC[]   init $MX_MAXCH * ($MX_MAXCH + 2)   ; smoothed input cell gains
gkMxSmO[]   init $MX_MAXCH * 2                 ; smoothed output cell gains
gkMxFirst   init 1

; v (0..1 pool value) -> linear amplitude
opcode MixAmp, k, k
  kv xin
  kA = 0
  if kv > 0.0005 then
    if kv <= 0.75 then
      kA = ampdb(-60 + 80 * kv)
    else
      kA = ampdb(48 * (kv - 0.75))
    endif
  endif
  xout kA
endop

; Rack input stage. Returns what the NEPTR rack sees as in1 / in2.
; Idle-CPU gate (2026-10-22): MixAmp is a pure function of the raw pool value,
; and the pool values only change when a fader/route is moved, yet MixIn/MixOut
; called it ~110 times EVERY k-cycle (a UDO call each) for values that had not
; moved. Cache last raw value + its amp per slot; recompute only on change.
; Cache seeds are raw 0 / amp 0 == MixAmp(0), so a never-touched slot is exact.
; Result is bit-identical to calling MixAmp every cycle.
gkMxRawG[]  init $MX_MAXCH                       ; input/output channel gain raw
gkMxAmpG[]  init $MX_MAXCH
gkMxRawC[]  init $MX_MAXCH * ($MX_MAXCH + 2)     ; input cell raw
gkMxAmpC[]  init $MX_MAXCH * ($MX_MAXCH + 2)
gkMxRawOG[] init $MX_MAXCH
gkMxAmpOG[] init $MX_MAXCH
gkMxRawO[]  init $MX_MAXCH * 2                   ; output cell raw
gkMxAmpO[]  init $MX_MAXCH * 2

#define MX_AMPC(kV'kOut'aRaw'aAmp'idx)#
  if ($kV != $aRaw[$idx]) then
    $aRaw[$idx] = $kV
    $aAmp[$idx] = MixAmp($kV)
  endif
  $kOut = $aAmp[$idx]
#

opcode MixIn, aa, 0
  aRL = a(0)
  aRR = a(0)
  aZ  = a(0)
  kc = 0
  while kc < giMxNo do
    gaMxDir[kc] = aZ
    kc += 1
  od
  aRamp line 0, ksmps / sr, 1          ; one 0..1 ramp per block: click-free gain steps
  kj = 0
  while kj < giMxNi do
    kb = giMxBase + kj * giMxSI
    kG tab kb, giDParamTab
    kM tab kb + 1, giDParamTab
    $MX_AMPC(kG'kGA'gkMxRawG'gkMxAmpG'kj)
    kT = (kM >= 0.5 ? 0 : kGA)
    if gkMxFirst == 1 then
      gkMxSmIn[kj] = kT
    endif
    kP = gkMxSmIn[kj]
    kN = kP + (kT - kP) * 0.2            ; ~5 blocks to settle
    gkMxSmIn[kj] = kN
    aIn inch kj + 1
    gaMxMtrIn[kj] = aIn
    aS = aIn * (kP + (kN - kP) * aRamp)
    kd = 0
    while kd < giMxNo + 2 do
      kci = kj * (giMxNo + 2) + kd
      kV tab kb + 2 + kd, giDParamTab
      $MX_AMPC(kV'kCT'gkMxRawC'gkMxAmpC'kci)
      if gkMxFirst == 1 then
        gkMxSmC[kci] = kCT
      endif
      kCP = gkMxSmC[kci]
      if kCT > 0 || kCP > 0 then
        kCN = kCP + (kCT - kCP) * 0.2
        gkMxSmC[kci] = kCN
        aTerm = aS * (kCP + (kCN - kCP) * aRamp)
        if kd == 0 then
          aRL = aRL + aTerm
        elseif kd == 1 then
          aRR = aRR + aTerm
        else
          gaMxDir[kd - 2] = gaMxDir[kd - 2] + aTerm
        endif
      endif
      kd += 1
    od
    kj += 1
  od
  xout aRL, aRR
endop

; Output stage: replaces `outs`. Every output k = direct inputs + main cells.
opcode MixOut, 0, aa
  aL, aR xin
  aRamp line 0, ksmps / sr, 1
  kj = 0
  while kj < giMxNo do
    kb = giMxBaseO + kj * giMxSO
    kG tab kb, giDParamTab
    kM tab kb + 1, giDParamTab
    $MX_AMPC(kG'kGA'gkMxRawOG'gkMxAmpOG'kj)
    kT = (kM >= 0.5 ? 0 : kGA)
    if gkMxFirst == 1 then
      gkMxSmOut[kj] = kT
    endif
    kP = gkMxSmOut[kj]
    kN = kP + (kT - kP) * 0.2
    gkMxSmOut[kj] = kN
    aSrc = gaMxDir[kj]
    ks = 0
    while ks < 2 do
      kV tab kb + 2 + ks, giDParamTab
      kOi = kj*2+ks
      $MX_AMPC(kV'kCT'gkMxRawO'gkMxAmpO'kOi)
      if gkMxFirst == 1 then
        gkMxSmO[kj*2+ks] = kCT
      endif
      kCP = gkMxSmO[kj*2+ks]
      if kCT > 0 || kCP > 0 then
        kCN = kCP + (kCT - kCP) * 0.2
        gkMxSmO[kj*2+ks] = kCN
        aCg = kCP + (kCN - kCP) * aRamp
        if ks == 0 then
          aSrc = aSrc + aL * aCg
        else
          aSrc = aSrc + aR * aCg
        endif
      endif
      ks += 1
    od
    aO = aSrc * (kP + (kN - kP) * aRamp)
    gaMxMtrOut[kj] = aO
    outch kj + 1, aO
    kj += 1
  od
  gkMxFirst = 0
endop

; one meter instance per channel: p4 = 0 input / 1 output, p5 = 0-based channel
instr 281
  idir = p4
  ich  = p5
  iix  = idir * 2 * $MX_MAXCH + ich * 2
  if idir == 0 then
    a1 = gaMxMtrIn[ich]
  else
    a1 = gaMxMtrOut[ich]
  endif
  kT metro 12.5
  kPk max_k a1, kT, 1
  kRm rms a1, 6
  kLastP init -1
  kLastR init -1
  if kT == 1 then
    kdP = (kPk > 0.000001 ? 20 * log10(kPk) : -120)
    kdR = (kRm > 0.000001 ? 20 * log10(kRm) : -120)
    kvP = limit((kdP + 60) / 60, 0, 1)
    kvR = limit((kdR + 60) / 60, 0, 1)
    kSendP = (abs(kvP - kLastP) > 0.004 ? 1 : 0)
    kSendR = (abs(kvR - kLastR) > 0.004 ? 1 : 0)
    if kSendP == 1 then
      kLastP = kvP
    endif
    if kSendR == 1 then
      kLastR = kvR
    endif
  else
    kSendP = 0
    kSendR = 0
    kvP = kLastP
    kvR = kLastR
  endif
  SpP = gSMxMtr[iix]
  SpR = gSMxMtr[iix + 1]
  ; OSCsend fires when its trigger rises: call every cycle, trigger = change flag
  OSCsend kSendP, gSDParamHost, giDParamPool, "/pout", "sf", SpP, kvP
  OSCsend kSendR, gSDParamHost, giDParamPool, "/pout", "sf", SpR, kvR
endin

; start the pool receiver + the meters (always-on, runs once)
instr 282
  dparam_init "mixer", $MIXER_PORT
  ich = 0
  while ich < giMxNi do
    schedule 281, 0, -1, 0, ich
    ich += 1
  od
  ich = 0
  while ich < giMxNo do
    schedule 281, 0, -1, 1, ich
    ich += 1
  od
  turnoff
endin
