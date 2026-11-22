; nam_stage.csd — DEMIURGE NAM mode CSound template.
; Loaded by demiurge-run-nam — do not edit for day-to-day use.
;
; Profile dir and start index are injected as CSound macros by demiurge-run-nam:
;   -D NAM_PROFILE_DIR=/absolute/path/to/profiles
;   -D NAM_PROFILE_IDX=0
;
; OSC control (port DEMIURGE_OSC_BASE, default 9000):
;   /nam/idx     i   — switch profile without audio interruption
;   /nam/bypass  i   — 1 = passthrough, 0 = NAM active
;
; Requires: csound-nam.so in /usr/local/lib/csound/plugins64/
;   (installed by setup/deploy-nam.sh or setup/build-csound-nam.sh)

; Defaults — overridden at launch by -D macros from demiurge-run-nam
#ifndef NAM_PROFILE_DIR
#define NAM_PROFILE_DIR # /home/gorpi7/demiurge/profiles/nam #
#endif
#ifndef NAM_PROFILE_IDX
#define NAM_PROFILE_IDX # 0 #
#endif
#ifndef DEMIURGE_OSC_BASE
#define DEMIURGE_OSC_BASE # 9000 #
#endif

<CsoundSynthesizer>
<CsOptions>
; demiurge-run-csound supplies the real flags (-odac -iadc -b -B -r --realtime).
; These are fallbacks for standalone testing:
;   NAM_PROFILE_DIR=/path/to/profiles csound nam_stage.csd
-n -d
</CsOptions>
<CsInstruments>
sr     = 48000
ksmps  = 128
nchnls = 2
0dbfs  = 1

gkNAMIdx    init $NAM_PROFILE_IDX.
gkNAMBypass init 0

; OSCinit at global scope — avoids i-time conflicts if instr restarts
giOSCIn OSCinit $DEMIURGE_OSC_BASE.

instr 1
  ; ---- i-rate: runs once when the instrument starts ----
  gSNAMLibDir  = "$NAM_PROFILE_DIR."

  ; ---- k/a-rate: runs every 128-sample block ----
  aL, aR ins

  ; Use local k-vars: CSound 6.18 OSClisten may not write directly to globals
  kRcvIdx  init $NAM_PROFILE_IDX.
  kRcvByp  init 0
  kIdx     OSClisten giOSCIn, "/nam/idx",    "i", kRcvIdx
  kByp     OSClisten giOSCIn, "/nam/bypass", "i", kRcvByp
  if kIdx == 1 then
    gkNAMIdx    = kRcvIdx
  endif
  if kByp == 1 then
    gkNAMBypass = kRcvByp
  endif

  aOutL, aOutR NAMProcess aL, aR, gkNAMIdx, gSNAMLibDir

  if (gkNAMBypass == 1) then
    aOutL = aL
    aOutR = aR
  endif

  outs aOutL, aOutR
endin

</CsInstruments>
<CsScore>
; Run until demiurge kills the process (86400 s = 24 h safety ceiling)
i1 0 86400
</CsScore>
</CsoundSynthesizer>
