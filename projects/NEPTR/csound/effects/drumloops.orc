;==============================================================
; DRUM LOOPS — sample mode on any looper channel (menu 12)
;
; Turn an empty channel's encoder while the looper has nothing else
; on it and a RANDOM file from $DRUMLOOP_DIR loads onto that channel
; and plays. Each further detent picks another random file. The
; file's length becomes gkLpMasterLen, so live loops recorded on the
; other channels grid-snap and transport-lock to the drum loop.
; Tap/hold/double-tap on the drum channel keep their normal meanings
; (stop/resume, overdub, clear/exit).
;
; There is only ever ONE drum loop, and browsing is only honored
; while it would be the only content on the looper — once anything
; else is recorded (or recording), browse requests are ignored so a
; stray encoder turn can't yank the master mid-performance.
;
; Shared verbatim between the hardware csd and the VST/APP builds.
; Each wrapper defines $DRUMLOOP_DIR (absolute path) and writes only
; the pending-intent globals below; all logic lives here.
;
;   gkDrumStep  — browse trigger (any nonzero = pick a new random
;                 file); hardware encoder detents and VST prev/next
;                 both add here. Consumed (zeroed) by instr 216.
;   gkDrumReqCh — channel (1-5) the browse request targets (set
;                 alongside gkDrumStep by whichever encoder turned)
;   gkDrumTap   — VST play/stop button intent (hardware uses the
;                 normal tap path instead)
;   gkDrumClear — VST clear-button intent (hardware double-taps)
;   gkDrumChan  — channel holding the drum loop (1-5, -1 = none)
;   gkDrumIdx   — loaded file index (-1 = none), gkDrumCount = #files
;
; Loading (instr 215) is i-time: GEN01 deferred temp tables, then a
; linear-interp copy into that channel's giLpBuf resampling filesr ->
; sr. The i-pass briefly blocks the audio thread; it only runs on an
; explicit browse, and a click while switching drum loops is fine.
;==============================================================

#ifndef DRUMLOOP_DIR
#define DRUMLOOP_DIR #samples/drumloops#
#endif

giDrumGain  init 0.6   ; drum loops land at ~60% level system-wide (baked in
                       ; at load so live overdubs on top keep their own gain)
gkDrumStep  init 0
gkDrumReqCh init 1
gkDrumTap   init 0
gkDrumClear init 0
gkDrumChan  init -1
gkDrumIdx   init -1
gkDrumCount init 0
gSDrumName  init ""

giDrumBufsL[] fillarray giLpBuf1L, giLpBuf2L, giLpBuf3L, giLpBuf4L, giLpBuf5L
giDrumBufsR[] fillarray giLpBuf1R, giLpBuf2R, giLpBuf3R, giLpBuf4R, giLpBuf5R

; commit a loaded drum loop to channel $N (i-vars iCh/iDstN in scope)
#define DRUM_COMMIT_CH(N) #
    if (iCh == $N) then
      gkLpLen$N    = iDstN
      gkLpWrPos$N  = 0
      gkLpTarget$N = 0
      gkLpSpIdx$N  = 6
      gkLpAnchor$N = gkLpClock
      gkLpRdPos$N  = 0
      gkLpState$N  = 2
    endif
#

;--------------------------------------------------------------
; instr 215 — DrumLoad. p4 = target channel (1-5). Rescans the
; folder every call so files dropped in while running are found.
;--------------------------------------------------------------
instr 215
  iCh  = p4
  SDir = "$DRUMLOOP_DIR"

  SW[] directory SDir, ".wav"
  SA[] directory SDir, ".aif"
  SB[] directory SDir, ".aiff"
  SC[] directory SDir, ".flac"
  iNW lenarray SW
  iNA lenarray SA
  iNB lenarray SB
  iNC lenarray SC
  iTot = iNW + iNA + iNB + iNC

  if (iTot == 0) then
    prints "DRUMS: no loop files in %s\n", SDir
    gkDrumCount = 0
    turnoff
  else
    SAll[] init iTot
    iX = 0
    iI = 0
    while iI < iNW do
      SAll[iX] = SW[iI]
      iX += 1
      iI += 1
    od
    iI = 0
    while iI < iNA do
      SAll[iX] = SA[iI]
      iX += 1
      iI += 1
    od
    iI = 0
    while iI < iNB do
      SAll[iX] = SB[iI]
      iX += 1
      iI += 1
    od
    iI = 0
    while iI < iNC do
      SAll[iX] = SC[iI]
      iX += 1
      iI += 1
    od

    ; stable alphabetical order (directory returns arbitrary order) so
    ; gkDrumIdx means the same file across rescans / on the UI side
    iI = 0
    while iI < iTot - 1 do
      iJ = 0
      while iJ < iTot - 1 - iI do
        iCmp strcmp SAll[iJ], SAll[iJ+1]
        if (iCmp > 0) then
          STmp = SAll[iJ]
          SAll[iJ] = SAll[iJ+1]
          SAll[iJ+1] = STmp
        endif
        iJ += 1
      od
      iI += 1
    od

    ; totally random pick each detent (transport-clock hash — no
    ; global RNG reseed), never repeating the current file
    iCur = i(gkDrumIdx)
    iClk = i(gkLpClock)
    iNew = int(frac(iClk * 0.00073331 + iCur * 0.4171) * iTot)
    if (iNew < 0 || iNew >= iTot) then
      iNew = 0
    endif
    if (iTot > 1 && iNew == iCur) then
      iNew = (iNew + 1) % iTot
    endif

    ; directory returns full paths; strip to basename for display
    SPath = SAll[iNew]
    iSlash strrindex SPath, "/"
    SName strsub SPath, iSlash + 1, -1
    iChn  filenchnls SPath
    iSrF  filesr SPath
    iTmpL ftgen 0, 0, 0, -1, SPath, 0, 0, 1
    if (iChn >= 2) then
      iTmpR ftgen 0, 0, 0, -1, SPath, 0, 0, 2
    else
      iTmpR = iTmpL
    endif

    iDstL  = giDrumBufsL[iCh - 1]
    iDstR  = giDrumBufsR[iCh - 1]
    iSrcN  = ftlen(iTmpL)
    iRatio = iSrF / sr
    iDstN  = int(iSrcN * sr / iSrF)
    iBufSz = ftlen(iDstL)
    if (iDstN > iBufSz) then
      prints "DRUMS: %s longer than looper buffer, truncating\n", SName
      iDstN = iBufSz
    endif

    iX = 0
    while iX < iDstN do
      iSPos = iX * iRatio
      i0  = int(iSPos)
      iFr = iSPos - i0
      i1  = (i0 + 1 < iSrcN ? i0 + 1 : iSrcN - 1)
      iv0L tab_i i0, iTmpL
      iv1L tab_i i1, iTmpL
      iv0R tab_i i0, iTmpR
      iv1R tab_i i1, iTmpR
      tableiw (iv0L + (iv1L - iv0L) * iFr) * giDrumGain, iX, iDstL
      tableiw (iv0R + (iv1R - iv0R) * iFr) * giDrumGain, iX, iDstR
      iX += 1
    od

    ftfree iTmpL, 0
    if (iChn >= 2) then
      ftfree iTmpR, 0
    endif

    gSDrumName = SName
    prints "DRUMS: ch%d <- %d/%d %s (%d samples)\n", iCh, iNew + 1, iTot, SName, iDstN

    ; k-pass commit: hand the channel to the drum loop and re-anchor
    ; the transport so live loops lock to the new master length
    gkDrumCount = iTot
    gkDrumIdx   = iNew
    gkDrumChan  = iCh
    $DRUM_COMMIT_CH(1)
    $DRUM_COMMIT_CH(2)
    $DRUM_COMMIT_CH(3)
    $DRUM_COMMIT_CH(4)
    $DRUM_COMMIT_CH(5)
    gkLpMasterLen = iDstN
    turnoff
  endif
endin

; channel $N blocks a browse unless it is the request target itself
; (kReq/kBusy k-vars in scope)
#define DRUM_BUSY_CH(N) #
  if (kReq != $N && (gkLpLen$N > 0 || gkLpState$N != 0)) then
    kBusy = 1
  endif
#

#define DRUM_TAP_CH(N) #
  if (gkDrumChan == $N) then
    if (gkLpState$N == 2) then
      gkLpState$N = 4
      printks "DRUMS: STOP\n", 0
    elseif (gkLpState$N == 4 && gkLpLen$N > 0) then
      gkLpState$N = 2
      gkLpRdPos$N = (gkLpClock - gkLpAnchor$N) % gkLpLen$N
      printks "DRUMS: RESUME\n", 0
    endif
  endif
#

#define DRUM_CLEAR_CH(N) #
  if (gkDrumChan == $N) then
    gkLpState$N = 0
    gkLpLen$N   = 0
    gkLpWrPos$N = 0
    gkLpRdPos$N = 0
    gkLpSpIdx$N = 6
    kDrumLive = gkLpLen1 + gkLpLen2 + gkLpLen3 + gkLpLen4 + gkLpLen5
    if (kDrumLive == 0) then
      gkLpMasterLen = 0
    endif
    gkDrumChan = -1
    gkDrumIdx  = -1
    printks "DRUMS: CLEAR\n", 0
  endif
#

;--------------------------------------------------------------
; instr 216 — watcher (alwayson). Consumes control-surface intents;
; runs after the looper instrs so state edits land next cycle.
;--------------------------------------------------------------
instr 216
  ; browse request — honored only when the drum loop would be the
  ; only content on the looper: every other channel must be fully
  ; idle, and the target must be empty or already the drum channel
  if (gkDrumStep != 0) then
    kReq = gkDrumReqCh
    kBusy = 0
    $DRUM_BUSY_CH(1)
    $DRUM_BUSY_CH(2)
    $DRUM_BUSY_CH(3)
    $DRUM_BUSY_CH(4)
    $DRUM_BUSY_CH(5)
    kSelfOk = 0
    if (gkDrumChan == kReq) then
      kSelfOk = 1
    elseif (gkDrumChan == -1) then
      if (kReq == 1 && gkLpState1 == 0 && gkLpLen1 == 0) then
        kSelfOk = 1
      elseif (kReq == 2 && gkLpState2 == 0 && gkLpLen2 == 0) then
        kSelfOk = 1
      elseif (kReq == 3 && gkLpState3 == 0 && gkLpLen3 == 0) then
        kSelfOk = 1
      elseif (kReq == 4 && gkLpState4 == 0 && gkLpLen4 == 0) then
        kSelfOk = 1
      elseif (kReq == 5 && gkLpState5 == 0 && gkLpLen5 == 0) then
        kSelfOk = 1
      endif
    endif
    if (kBusy == 0 && kSelfOk == 1) then
      event "i", 215, 0, 0.1, kReq
    endif
    gkDrumStep = 0
  endif

  ; VST play/stop button — same semantics as a hardware tap on the
  ; drum channel
  if (gkDrumTap == 1) then
    $DRUM_TAP_CH(1)
    $DRUM_TAP_CH(2)
    $DRUM_TAP_CH(3)
    $DRUM_TAP_CH(4)
    $DRUM_TAP_CH(5)
    gkDrumTap = 0
  endif

  ; VST clear button — same semantics as a hardware double-tap on the
  ; drum channel
  if (gkDrumClear == 1) then
    $DRUM_CLEAR_CH(1)
    $DRUM_CLEAR_CH(2)
    $DRUM_CLEAR_CH(3)
    $DRUM_CLEAR_CH(4)
    $DRUM_CLEAR_CH(5)
    gkDrumClear = 0
  endif

  ; filename for the VST label (unbound channel on hardware, harmless)
  kNmTick metro 4
  if (kNmTick == 1) then
    chnsetks gSDrumName, "drum_file"
  endif
endin
