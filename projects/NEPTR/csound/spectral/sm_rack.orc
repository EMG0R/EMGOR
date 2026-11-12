;==============================================================
; SOUNDMAGIC SPECTRAL RACK  (engine; contract: spectral/CONTRACT.md)
; Re-implementation of Michael Norris's SoundMagic Spectral suite
; (michaelnorris.info). Algorithms/params are his; the code is ours.
;
; instr 241..248  rack group for FFT index 0..7 (512..65536): ONE pvsanal
;                 per channel, all active effects of that size run on the
;                 bin arrays in rack order (fx id ascending), ONE pvsynth.
; instr 249       rack control (ALWAYSON, score `i249 0 z`): resolves each
;                 on-effect's FFT index, starts/stops groups, sets gkSMActive.
; instr 251..254  TIME-DOMAIN stage, one instr per effect (fx 19 CHORUS,
;                 20 GRAINSTR, 21 IDEEFIX, 22 SUPERFB). Same chain-bus
;                 contract as the groups (read gaSMChain in place, limit
;                 +-0.99), run AFTER the groups (higher number) in ascending
;                 fx order. Started/stopped by instr 249 (hard off) so an
;                 OFF effect has no UDO instance and costs nothing.
;
; CHAIN BUS: signal_chain.orc writes gaSMChainL/R (input) and reads them back
; (output) in one SPECTRAL RACK block. Groups run in the SAME k-cycle after
; instr 1 (higher instr number), ascending size, each reading and rewriting
; the bus in place, so instr 1 reads the previous cycle's result: ONE ksmps
; (64 samples = 1.3 ms) of bus latency, on top of each group's own pvs latency.
; When no group is running the bus is a plain pass-through (same latency).
;
; Per frame: pvs2tab refreshes the arrays EVERY k-cycle, tab2pvs emits a frame
; only on its own hop boundary which lines up with a pvs2tab new-frame flag, so
; kernels run only when kNew==1 (measured on Csound 6.18: identical result to
; processing every cycle, 1/(hop/ksmps) of the work).
;==============================================================

; FFT index -> size
giSMsizes ftgen 0, 0, -8, -2, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536

; ---- run every active kernel of group iS on one channel's bin arrays ------
opcode SM_Run, k[]k[], k[]k[]iki
  kMag[], kPha[], iS, kNew, iN xin
#ifdef SM_HAVE_AVERAGE
  if (tab:k(0, giSMon) == 1 && tab:k(0, giSMres) == iS) then
    kMag, kPha SM_Averaging kMag, kPha, 0, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 0, kNew, iN
  endif
#end
#ifdef SM_HAVE_BLUR
  if (tab:k(1, giSMon) == 1 && tab:k(1, giSMres) == iS) then
    kMag, kPha SM_Blurring kMag, kPha, 1, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 1, kNew, iN
  endif
#end
#ifdef SM_HAVE_DRONE
  if (tab:k(2, giSMon) == 1 && tab:k(2, giSMres) == iS) then
    kMag, kPha SM_DroneMaker kMag, kPha, 2, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 2, kNew, iN
  endif
#end
#ifdef SM_HAVE_FREEZE
  if (tab:k(3, giSMon) == 1 && tab:k(3, giSMres) == iS) then
    kMag, kPha SM_Freezing kMag, kPha, 3, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 3, kNew, iN
  endif
#end
#ifdef SM_HAVE_GATEHOLD
  if (tab:k(4, giSMon) == 1 && tab:k(4, giSMres) == iS) then
    kMag, kPha SM_GateAndHold kMag, kPha, 4, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 4, kNew, iN
  endif
#end
#ifdef SM_HAVE_WEAVE
  if (tab:k(5, giSMon) == 1 && tab:k(5, giSMres) == iS) then
    kMag, kPha SM_Weave kMag, kPha, 5, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 5, kNew, iN
  endif
#end
#ifdef SM_HAVE_SHIMMER
  if (tab:k(6, giSMon) == 1 && tab:k(6, giSMres) == iS) then
    kMag, kPha SM_Shimmer kMag, kPha, 6, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 6, kNew, iN
  endif
#end
#ifdef SM_HAVE_SHUFFLE
  if (tab:k(7, giSMon) == 1 && tab:k(7, giSMres) == iS) then
    kMag, kPha SM_Shuffle kMag, kPha, 7, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 7, kNew, iN
  endif
#end
#ifdef SM_HAVE_PULSING
  if (tab:k(8, giSMon) == 1 && tab:k(8, giSMres) == iS) then
    kMag, kPha SM_Pulsing kMag, kPha, 8, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 8, kNew, iN
  endif
#end
#ifdef SM_HAVE_HARMONIZ
  if (tab:k(9, giSMon) == 1 && tab:k(9, giSMres) == iS) then
    kMag, kPha SM_Harmonizer kMag, kPha, 9, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 9, kNew, iN
  endif
#end
#ifdef SM_HAVE_BINSHIFT
  if (tab:k(10, giSMon) == 1 && tab:k(10, giSMres) == iS) then
    kMag, kPha SM_BinShift kMag, kPha, 10, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 10, kNew, iN
  endif
#end
#ifdef SM_HAVE_PITCH
  if (tab:k(11, giSMon) == 1 && tab:k(11, giSMres) == iS) then
    kMag, kPha SM_PitchShift kMag, kPha, 11, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 11, kNew, iN
  endif
#end
#ifdef SM_HAVE_STRETCH
  if (tab:k(12, giSMon) == 1 && tab:k(12, giSMres) == iS) then
    kMag, kPha SM_Stretch kMag, kPha, 12, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 12, kNew, iN
  endif
#end
#ifdef SM_HAVE_TRACING
  if (tab:k(13, giSMon) == 1 && tab:k(13, giSMres) == iS) then
    kMag, kPha SM_Tracing kMag, kPha, 13, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 13, kNew, iN
  endif
#end
#ifdef SM_HAVE_FILTBANK
  if (tab:k(14, giSMon) == 1 && tab:k(14, giSMres) == iS) then
    kMag, kPha SM_Filterbank kMag, kPha, 14, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 14, kNew, iN
  endif
#end
#ifdef SM_HAVE_GLIDEFLT
  if (tab:k(15, giSMon) == 1 && tab:k(15, giSMres) == iS) then
    kMag, kPha SM_GlidingFilters kMag, kPha, 15, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 15, kNew, iN
  endif
#end
#ifdef SM_HAVE_GRANUL
  if (tab:k(16, giSMon) == 1 && tab:k(16, giSMres) == iS) then
    kMag, kPha SM_Granulation kMag, kPha, 16, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 16, kNew, iN
  endif
#end
#ifdef SM_HAVE_EMERGE
  if (tab:k(17, giSMon) == 1 && tab:k(17, giSMres) == iS) then
    kMag, kPha SM_Emergence kMag, kPha, 17, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 17, kNew, iN
  endif
#end
#ifdef SM_HAVE_PARTGLD
  if (tab:k(18, giSMon) == 1 && tab:k(18, giSMres) == iS) then
    kMag, kPha SM_PartialGlide kMag, kPha, 18, kNew, iN
    kMag, kPha SM_Common kMag, kPha, 18, kNew, iN
  endif
#end
  xout kMag, kPha
endop

; ---- hero-parameter encoder step (called from the rot_*.orc generic branch) --
; kTog = gkLastToggle (75..98); kHero = 0..6 (P1..P7); kVal = encoder CC value
opcode SM_RotStep, 0, kkk
  kTog, kHero, kVal xin
  kFx = kTog - 75
  if (kFx < 0 || kFx > 22 || kVal == 64) then
    kgoto done
  endif
  kIdx = tab:k(kFx * 8 + kHero, giSMhero)
  if (kIdx < 0) then
    kgoto done
  endif
  kMn = tab:k(kIdx, giSMPmin)
  kMx = tab:k(kIdx, giSMPmax)
  kDf = tab:k(kIdx, giSMPdef)
  kCur = tab:k(kIdx, giSMP)
  kDir = (kVal > 64 ? 1 : -1)
  kRng = kMx - kMn
  if (kRng <= 8 && kMn == int(kMn) && kMx == int(kMx) && kDf == int(kDf)) then
    kNew = kCur + kDir              ; bool / enum / small integer: one step
  elseif (kMn > 0 && kMx / kMn > 100) then
    kNew = kCur * exp(kDir * log(kMx / kMn) / 100) ; wide Hz-like range: exponential
  else
    kNew = kCur + kDir * kRng / 100
  endif
  tablew limit(kNew, kMn, kMx), kIdx, giSMP
done:
endop

; ---- expression pedal: absolute write of the focused effect's pedal param ---
opcode SM_PedalSet, 0, kk
  kTog, kNorm xin
  kFx = kTog - 75
  if (kFx < 0 || kFx > 22) then
    kgoto done
  endif
  kIdx = tab:k(kFx, giSMpedal)
  if (kIdx < 0) then
    kgoto done
  endif
  kMn = tab:k(kIdx, giSMPmin)
  kMx = tab:k(kIdx, giSMPmax)
  kN = limit(kNorm, 0, 1)
  if (kMn > 0 && kMx / kMn > 100) then
    tablew kMn * exp(kN * log(kMx / kMn)), kIdx, giSMP  ; Hz-like: log taper
  else
    tablew kMn + kN * (kMx - kMn), kIdx, giSMP
  endif
done:
endop

; ---- the eight rack groups --------------------------------------------------
#define SM_GROUP(INUM'SIDX)#
instr $INUM
  iS = $SIDX
  iN = table:i(iS, giSMsizes)
  iHop = iN / 4
  aL = gaSMChainL
  aR = gaSMChainR
  fL pvsanal aL, iN, iHop, iN, 1, 1
  fR pvsanal aR, iN, iHop, iN, 1, 1
  kML[] init iN / 2 + 1
  kPL[] init iN / 2 + 1
  kMR[] init iN / 2 + 1
  kPR[] init iN / 2 + 1
  kfrL pvs2tab kML, kPL, fL
  kfrR pvs2tab kMR, kPR, fR
  kNew changed kfrL
  if (kNew == 1) then
    kML, kPL SM_Run kML, kPL, iS, kNew, iN
    kMR, kPR SM_Run kMR, kPR, iS, kNew, iN
  endif
  fOL tab2pvs kML, kPL, iHop, iN, 1
  fOR tab2pvs kMR, kPR, iHop, iN, 1
  aOL pvsynth fOL
  aOR pvsynth fOR
  aOL dcblock aOL
  aOR dcblock aOR
  gaSMChainL = limit(aOL, -0.99, 0.99)
  gaSMChainR = limit(aOR, -0.99, 0.99)
endin
#
$SM_GROUP(241'0)
$SM_GROUP(242'1)
$SM_GROUP(243'2)
$SM_GROUP(244'3)
$SM_GROUP(245'4)
$SM_GROUP(246'5)
$SM_GROUP(247'6)
$SM_GROUP(248'7)

; ---- time-domain stage (fx 19..22), one instr each ----------------------------
#ifdef SM_HAVE_CHORUS
instr 251
  aL, aR SM_Chorus gaSMChainL, gaSMChainR, 19
  gaSMChainL = limit(aL, -0.99, 0.99)
  gaSMChainR = limit(aR, -0.99, 0.99)
endin
#end
#ifdef SM_HAVE_GRAINSTR
instr 252
  aL, aR SM_GrainStreamer gaSMChainL, gaSMChainR, 20
  gaSMChainL = limit(aL, -0.99, 0.99)
  gaSMChainR = limit(aR, -0.99, 0.99)
endin
#end
#ifdef SM_HAVE_IDEEFIX
instr 253
  aL, aR SM_IdeeFixer gaSMChainL, gaSMChainR, 21
  gaSMChainL = limit(aL, -0.99, 0.99)
  gaSMChainR = limit(aR, -0.99, 0.99)
endin
#end
#ifdef SM_HAVE_SUPERFB
instr 254
  aL, aR SM_SuperFilterbank gaSMChainL, gaSMChainR, 22
  gaSMChainL = limit(aL, -0.99, 0.99)
  gaSMChainR = limit(aR, -0.99, 0.99)
endin
#end

; ---- rack control -----------------------------------------------------------
instr 249
  ; Only effects whose kernel is compiled in count (SM_HAVE_<CODE>): the rest
  ; are inert, so with no kernels the rack never starts a group and the chain
  ; is bit-exact dry.
  kWant[] init 8
  kHave[] init 24
  kTdWant[] init 4
  kTdPrv[] init 4
  kIsOn[] init 24
  kGrpNext[] init 8
  kTdNext[] init 4
  kClk init 0
  kClk += ksmps / sr
  kDefFFT = limit(tab:k(736, giSMP), 0, 7)
  gkSMDefaultFFT = kDefFFT
  kk = 0
  while (kk < 8) do
    kWant[kk] = 0
    kk += 1
  od
  kk = 0
  while (kk < 19) do
    kHave[kk] = 0
    kk += 1
  od
#ifdef SM_HAVE_AVERAGE
  kHave[0] = 1
#end
#ifdef SM_HAVE_BLUR
  kHave[1] = 1
#end
#ifdef SM_HAVE_DRONE
  kHave[2] = 1
#end
#ifdef SM_HAVE_FREEZE
  kHave[3] = 1
#end
#ifdef SM_HAVE_GATEHOLD
  kHave[4] = 1
#end
#ifdef SM_HAVE_WEAVE
  kHave[5] = 1
#end
#ifdef SM_HAVE_SHIMMER
  kHave[6] = 1
#end
#ifdef SM_HAVE_SHUFFLE
  kHave[7] = 1
#end
#ifdef SM_HAVE_PULSING
  kHave[8] = 1
#end
#ifdef SM_HAVE_HARMONIZ
  kHave[9] = 1
#end
#ifdef SM_HAVE_BINSHIFT
  kHave[10] = 1
#end
#ifdef SM_HAVE_PITCH
  kHave[11] = 1
#end
#ifdef SM_HAVE_STRETCH
  kHave[12] = 1
#end
#ifdef SM_HAVE_TRACING
  kHave[13] = 1
#end
#ifdef SM_HAVE_FILTBANK
  kHave[14] = 1
#end
#ifdef SM_HAVE_GLIDEFLT
  kHave[15] = 1
#end
#ifdef SM_HAVE_GRANUL
  kHave[16] = 1
#end
#ifdef SM_HAVE_EMERGE
  kHave[17] = 1
#end
#ifdef SM_HAVE_PARTGLD
  kHave[18] = 1
#end
  kAct = 0
  kk = 0
  while (kk < 19) do
    if (kHave[kk] == 1 && tab:k(kk, giSMon) == 1) then
      kFp = tab:k(kk, giSMfftp)
      kRes = (kFp < 0 ? kDefFFT : limit(round(tab:k(kFp, giSMP)), 0, 7))
      tablew kRes, kk, giSMres
      kWant[int(kRes)] = 1
      kAct = 1
    endif
    kk += 1
  od
  ; time-domain effects (fx 19..22 -> instr 251..254): count toward gkSMActive
  kTdWant[0] = 0
  kTdWant[1] = 0
  kTdWant[2] = 0
  kTdWant[3] = 0
#ifdef SM_HAVE_CHORUS
  kTdWant[0] = tab:k(19, giSMon) == 1 ? 1 : 0
#end
#ifdef SM_HAVE_GRAINSTR
  kTdWant[1] = tab:k(20, giSMon) == 1 ? 1 : 0
#end
#ifdef SM_HAVE_IDEEFIX
  kTdWant[2] = tab:k(21, giSMon) == 1 ? 1 : 0
#end
#ifdef SM_HAVE_SUPERFB
  kTdWant[3] = tab:k(22, giSMon) == 1 ? 1 : 0
#end
  kt = 0
  while (kt < 4) do
    if (kTdWant[kt] == 1) then
      kAct = 1
    endif
    if (kTdWant[kt] == 1 && kTdPrv[kt] == 0) then
      event "i", 251 + kt, 0, -1
      kTdNext[kt] = kClk + 1
    elseif (kTdWant[kt] == 0 && kTdPrv[kt] == 1) then
      turnoff2 251 + kt, 0, 0
    elseif (kTdWant[kt] == 1 && kClk >= kTdNext[kt] && active:k(251 + kt) < 0.5) then
      ; wanted, was running, but gone: it died on a performance error. Restart (max 1/s).
      event "i", 251 + kt, 0, -1
      kTdNext[kt] = kClk + 1
      printks "SM rack: time-domain instr %d died, restarting\n", 0.001, 251 + kt
    endif
    kTdPrv[kt] = kTdWant[kt]
    kt += 1
  od
  gkSMActive = kAct
  kPrv[] init 8
  kg = 0
  while (kg < 8) do
    if (kWant[kg] == 1 && kPrv[kg] == 0) then
      event "i", 241 + kg, 0, -1
      kGrpNext[kg] = kClk + 1
    elseif (kWant[kg] == 0 && kPrv[kg] == 1) then
      turnoff2 241 + kg, 0, 0
    elseif (kWant[kg] == 1 && kClk >= kGrpNext[kg] && active:k(241 + kg) < 0.5) then
      ; wanted, was running, but gone: it died on a performance error. Restart (max 1/s).
      event "i", 241 + kg, 0, -1
      kGrpNext[kg] = kClk + 1
      printks "SM rack: group instr %d died, restarting\n", 0.001, 241 + kg
    endif
    kPrv[kg] = kWant[kg]
    kg += 1
  od
  ; Rack-owned final limiter. Runs after every group (249 > 248), before the time-domain stages
  ; (which limit themselves), so a dead group or stage can never leave an unlimited signal on the
  ; chain bus. Identity on already-limited audio; only while the rack is active (idle = bit-exact dry).
  if (kAct == 1) then
    gaSMChainL = limit(gaSMChainL, -0.99, 0.99)
    gaSMChainR = limit(gaSMChainR, -0.99, 0.99)
  endif
endin
