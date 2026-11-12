<Cabbage>
form caption("NAM DROP"), size(760, 330), colour(20, 16, 28), guiMode("queue"), pluginId("nmdr")

; ---- speaker grille = the drop zone (drops land anywhere on the window) ----
image bounds(20, 20, 720, 150), colour(26, 21, 36), outlineColour(74, 62, 102), outlineThickness(1), corners(8)
image bounds(21, 44, 718, 1), colour(48, 40, 66)
image bounds(21, 68, 718, 1), colour(48, 40, 66)
image bounds(21, 92, 718, 1), colour(48, 40, 66)
image bounds(21, 116, 718, 1), colour(48, 40, 66)
image bounds(21, 140, 718, 1), colour(48, 40, 66)

; nameplate, riveted to the grille
image bounds(36, 32, 96, 20), colour(223, 214, 240), corners(3)
label bounds(36, 35, 96, 14), text("NAM DROP"), fontColour(34, 26, 48), align("centre")

label bounds(20, 62, 720, 26), text("DROP A .NAM AMP HERE"), fontColour(233, 228, 244), align("centre")
label bounds(20, 96, 720, 18), channel("modelname"), text("no amp loaded - signal passes dry"), fontColour(183, 148, 246), align("centre")
label bounds(20, 124, 720, 13), channel("status"), text("starting engine ..."), fontColour(148, 138, 170), align("centre")

; ---- control plate ----
image bounds(20, 186, 720, 124), colour(33, 27, 46), outlineColour(104, 88, 140), outlineThickness(1), corners(8)
image bounds(28, 194, 5, 5), colour(120, 105, 150), corners(3)
image bounds(727, 194, 5, 5), colour(120, 105, 150), corners(3)
image bounds(28, 297, 5, 5), colour(120, 105, 150), corners(3)
image bounds(727, 297, 5, 5), colour(120, 105, 150), corners(3)

rslider bounds(36, 196, 64, 84), channel("ingain"), text("INPUT"), range(-24, 24, 0, 1, 0.1), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)
rslider bounds(134, 196, 64, 84), channel("gatethresh"), text("GATE"), range(-100, -20, -100, 1, 0.5), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)
rslider bounds(232, 196, 64, 84), channel("bass"), text("BASS"), range(-12, 12, 0, 1, 0.1), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)
rslider bounds(330, 196, 64, 84), channel("mid"), text("MID"), range(-12, 12, 0, 1, 0.1), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)
rslider bounds(428, 196, 64, 84), channel("treble"), text("TREBLE"), range(-12, 12, 0, 1, 0.1), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)
rslider bounds(526, 196, 64, 84), channel("outgain"), text("OUTPUT"), range(-36, 12, 0, 1, 0.1), colour(228, 222, 242), trackerColour(183, 148, 246), markerColour(44, 34, 66), outlineColour(20, 16, 28), fontColour(206, 198, 224), textColour(206, 198, 224)

; prev / next through the folder of the loaded model (arrow keys work too)
button bounds(618, 202, 30, 22), channel("prev"), text("<", "<"), colour:0(44, 36, 62), colour:1(86, 64, 132), fontColour:0(233, 228, 244), fontColour:1(196, 166, 255), corners(4), outlineColour(104, 88, 140)
button bounds(690, 202, 30, 22), channel("next"), text(">", ">"), colour:0(44, 36, 62), colour:1(86, 64, 132), fontColour:0(233, 228, 244), fontColour:1(196, 166, 255), corners(4), outlineColour(104, 88, 140)

; jewel pilot lamp - lights up when a model is actually loaded and running
image bounds(657, 201, 24, 24), channel("lamp"), colour(46, 30, 64), outlineColour(78, 66, 100), outlineThickness(2), corners(12)

; auto volume compensation - on by default, click off to hear the raw model level
button bounds(618, 240, 102, 22), channel("autovol"), text("AUTO VOL", "AUTO VOL"), value(1), colour:0(44, 36, 62), colour:1(86, 64, 132), fontColour:0(148, 138, 170), fontColour:1(196, 166, 255), corners(4), outlineColour(104, 88, 140)
label bounds(606, 268, 126, 11), text("matches your dry level"), fontColour(148, 138, 170), align("centre")
</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 --opcode-lib=/Users/emgor/Library/CsoundPlugins/csound-nam.so
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 64
nchnls = 2
0dbfs  = 1

; never let MIDI spawn extra instrument instances — the DSP chain is three
; always-on score-driven instruments (1 input → 50 engine → 99 output)
massign 0, 0

; audio buses between the always-safe shell and the NAM engine
gaInL  init 0
gaInR  init 0
gaOutL init 0
gaOutR init 0

; engine heartbeat: instr 50 asserts 1 every k-cycle, instr 99 consumes it.
; if the engine isn't running for any reason, output falls back to dry.
gkEngine init 0
; latched "engine has run at least once" — for the UI health check only
gkEngineSeen init 0

gSshownName init ""
gSshownDrop init ""
gSsavedPath init ""

; ---------------------------------------------------------------------------
; instr 2 (Boot) — the NAM engine is compiled at runtime so the base
; orchestra NEVER references the NAM opcodes: if csound-nam.so failed to
; load, everything else (dry passthrough, knobs, UI) still works and the UI
; reports it.  Chain order mirrors the official NAM plugin: input trim ->
; gate detection (pre-model) -> model (with loudness auto-vol) -> gate gain
; (post-model) -> tone stack -> DC blocker -> output trim.
; instr 51 writes the last-loaded model path to a sidecar file.
; ---------------------------------------------------------------------------
instr 2
  iRes compilestr {{
instr 50
  SPath chngetks "nam_path"
  kAuto chnget "autovol"
  aoL, aoR NAMProcess gaInL, gaInR, 0, SPath, kAuto, 0
  gkEngine = 1
  gkEngineSeen = 1

  ; noise gate: detect on the input, apply on the model output so the amp's
  ; own hiss gets gated too (same sidechain layout as the official plugin)
  kThr chnget "gatethresh"
  kgs  init 1
  if kThr <= -99.5 then
    kgs = 1
  else
    krms rms 0.5 * (gaInL + gaInR)
    kdb  = dbamp(krms + 0.000001)
    kopen = (kdb > kThr ? 1 : 0)
    ktime = (kopen > kgs ? 0.003 : 0.08)
    kgs  portk kopen, ktime
  endif
  agate interp kgs
  aoL = aoL * agate
  aoR = aoR * agate

  ; tone stack (post-model): bass shelf 100 Hz, mid peak 800 Hz, treble
  ; shelf 3.2 kHz, each +/-12 dB
  kB chnget "bass"
  kM chnget "mid"
  kT chnget "treble"
  aoL pareq aoL, 100, ampdb(kB), 0.71, 1
  aoR pareq aoR, 100, ampdb(kB), 0.71, 1
  aoL pareq aoL, 800, ampdb(kM), 0.71, 0
  aoR pareq aoR, 800, ampdb(kM), 0.71, 0
  aoL pareq aoL, 3200, ampdb(kT), 0.71, 2
  aoR pareq aoR, 3200, ampdb(kT), 0.71, 2

  ; DC blocker (reference chain has a 5 Hz high-pass here)
  aoL dcblock2 aoL
  aoR dcblock2 aoR

  gaOutL = aoL
  gaOutR = aoR

  SName NAMActiveName
  chnsetks SName, "active_name"
  SFull NAMActivePath
  chnsetks SFull, "active_path"

  kNav chnget "nav"
  NAMStep kNav, kNav
endin

instr 51
  SNow chnget "active_path"
  NAMWriteFile "/Users/emgor/Library/CsoundPlugins/namdrop_last.txt", SNow
endin
  }}
  ; boot diagnostics: written from inside whatever host loads the plugin
  iok filevalid "/Users/emgor/Library/CsoundPlugins/csound-nam.so"
  fprints "/tmp/namdrop_boot.txt", "sr=%d ksmps=%d compilestr=%d sofile_exists=%d\n", sr, ksmps, iRes, iok
  if iRes == 0 then
    scoreline_i "i 50 0 604800"
  endif
  turnoff
endin

; ---------------------------------------------------------------------------
; instr 1 — input stage + all UI logic.  Uses no NAM opcodes: guaranteed to
; compile and pass audio even if the engine is unavailable.
; ---------------------------------------------------------------------------
instr 1
  ; one-time UI truth about the engine, judged on evidence: has the engine
  ; instrument actually run?  checked once, half a second after startup.
  kChecked init 0
  kT timeinsts
  if kChecked == 0 && kT >= 0.5 then
    kChecked = 1
    if gkEngineSeen == 1 then
      cabbageSet 1, "status", "text", "works with any Neural Amp Modeler capture"
    else
      cabbageSet 1, "modelname", "text", "dry passthrough only"
      cabbageSet 1, "status", "text", "ENGINE OFFLINE - csound-nam.so failed to load"
    endif
  endif

  ; --- dropped file path; a live drop wins over the session-restored path
  SDrop chngetks "LAST_FILE_DROPPED"
  SPath strcpyk SDrop
  if strlenk(SDrop) == 0 then
    SPath chngetks "restorepath"
  endif
  chnsetks SPath, "nam_path"

  ; --- trim gains (smoothed); the host saves the knob values as parameters
  kOutDb cabbageGetValue "outgain"
  gkOut  = ampdb(portk(kOutDb, 0.05))

  ; clean input volume, applied before the engine; the opcode's loudness
  ; reference is its own input, so auto vol tracks the post-gain level
  kInDb cabbageGetValue "ingain"
  kIn   = ampdb(portk(kInDb, 0.05))
  aL inch 1
  aR inch 2
  gaInL = aL * kIn
  gaInR = aR * kIn

  ; --- folder navigation: buttons + arrow keys -> one-cycle pulse on "nav"
  kNav = 0
  kPrevBtn cabbageGetValue "prev"
  kNextBtn cabbageGetValue "next"
  if changed2(kPrevBtn) == 1 then
    kNav = -1
  elseif changed2(kNextBtn) == 1 then
    kNav = 1
  endif
  kKey  chnget "KEY_PRESSED"
  kDown chnget "KEY_DOWN"
  if changed2(kDown) == 1 && kDown == 1 then
    if kKey == 65554 || kKey == 65556 || kKey == 63234 || kKey == 63232 then
      kNav = -1    ; left / up arrow = previous file
    elseif kKey == 65555 || kKey == 65557 || kKey == 63235 || kKey == 63233 then
      kNav = 1     ; right / down arrow = next file
    endif
  endif
  chnset kNav, "nav"

  ; --- "loading ..." feedback the moment a file lands
  SBase strcpyk SDrop
  if strcmpk(SDrop, gSshownDrop) != 0 then
    gSshownDrop strcpyk SDrop
    kpos strrindexk SBase, "/"
    if kpos >= 0 then
      SBase strsubk SBase, kpos + 1, strlenk(SBase)
    endif
    klen strlenk SBase
    if klen > 4 then
      SBase strsubk SBase, 0, klen - 4
    endif
    SMsg sprintfk "loading %s ...", SBase
    cabbageSet 1, "modelname", "text", SMsg
  endif

  ; --- true engine state -> name label, jewel lamp, sidecar save
  SActive chngetks "active_name"
  if strcmpk(SActive, gSshownName) != 0 then
    gSshownName strcpyk SActive
    if strlenk(SActive) > 0 then
      cabbageSet 1, "modelname", "text", SActive
      cabbageSet 1, "status", "text", "running - drop another .nam or use < > to browse"
      cabbageSet 1, "lamp", "colour", 196, 166, 255
      cabbageSet 1, "lamp", "outlineColour", 130, 104, 180
    endif
  endif

  ; save whichever model is ACTUALLY loaded (drop or navigation) so the
  ; plugin reopens with it (instr 51 lives in the engine's compiled block)
  SNowPath chngetks "active_path"
  if strcmpk(SNowPath, gSsavedPath) != 0 then
    gSsavedPath strcpyk SNowPath
    if strlenk(SNowPath) > 0 then
      event "i", 51, 0, 0.01
    endif
  endif
endin

; ---------------------------------------------------------------------------
; instr 3 (Restore) — read the last-used model path from the sidecar file.
; No Cabbage state opcodes (broken in Cabbage 2.9 plugin binaries — verified).
; ---------------------------------------------------------------------------
instr 3
  iexists filevalid "/Users/emgor/Library/CsoundPlugins/namdrop_last.txt"
  if iexists == 1 then
    Sres, iline readfi "/Users/emgor/Library/CsoundPlugins/namdrop_last.txt"
    if strlen(Sres) > 4 then
      chnset Sres, "restorepath"
    endif
  endif
endin

; ---------------------------------------------------------------------------
; instr 99 — output stage.  Dry if the engine heartbeat is absent.
; ---------------------------------------------------------------------------
instr 99
  if gkEngine == 1 then
    aL = gaOutL
    aR = gaOutR
  else
    aL = gaInL
    aR = gaInR
  endif
  outs aL * gkOut, aR * gkOut
  gkEngine = 0
endin

</CsInstruments>
<CsScore>
i 2 0 0.1
i 1 0 [3600*24*7]
i 99 0 [3600*24*7]
i 3 0.2 0.1
</CsScore>
</CsoundSynthesizer>
