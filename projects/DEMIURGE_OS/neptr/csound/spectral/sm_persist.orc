;==============================================================
; SOUNDMAGIC rack persistence  (instr 250; contract: CONTRACT.md)
;
;   giSMP   (768 params)  -> $STATE_DIR/sm_params.ftab / sm_params.b.ftab
;   giSMon  (24 on flags) -> $STATE_DIR/sm_on.ftab     / sm_on.b.ftab
;   giSMPBank/giSMonBank  -> $STATE_DIR/sm_bank_<ctx>.ftab (+ .b, + sm_bankon_*)
;
; Same dual-file anti-truncation scheme as state_persist.orc (alternate A/B so
; a power cut mid-ftsavek never destroys the only good copy). Because the stamp
; sits at the END of the file, a truncated file can still pass filevalid; the
; stamp check (SM_STAMP at giSMP[767] / giSMon[23]) catches that, and a save
; GENERATION counter (giSMP[766]; giSMPBank[13056]) picks the fresher of two
; valid files (state_persist.orc learned that the hard way, 2026-09-25).
; The params and on-flags files of one letter are written in the same cycle, so
; the generation of the params file decides both.
;
; Defaults are applied at orchestra init, so the rack is sane even with
; STATE_PERSIST undefined (VST build) or no saved file. No layout change to
; giNeptrState: stamp 77000302 is untouched.
;==============================================================

#ifndef STATE_DIR
#define STATE_DIR #/home/gorpi7/demiurge/state#
#endif

; ---- defaults (orchestra init, before any instrument runs) ----
tableicopy giSMP, giSMPdef
tableiw $SM_STAMP, 767, giSMP        ; stamp slot (def table also carries it)
tableiw 2, 736, giSMP                ; rack default FFT index = 2048
tableiw $SM_STAMP, 23, giSMon        ; on-table stamp (fx 23 is spare)

instr 250
  ; ---- shadows start equal to the defaults so boot does not "dirty" ----
#ifdef STATE_PERSIST
  SDir = "$STATE_DIR"
  SCmd sprintf "mkdir -p \"%s\"", SDir
  iRet system_i 1, SCmd
  SPa sprintf "%s/sm_params.ftab", SDir
  SPb sprintf "%s/sm_params.b.ftab", SDir
  SOa sprintf "%s/sm_on.ftab", SDir
  SOb sprintf "%s/sm_on.b.ftab", SDir
  SBPa sprintf "%s/sm_bank_%d.ftab", SDir, $PRESET_BANK
  SBPb sprintf "%s/sm_bank_%d.b.ftab", SDir, $PRESET_BANK
  SBOa sprintf "%s/sm_bankon_%d.ftab", SDir, $PRESET_BANK
  SBOb sprintf "%s/sm_bankon_%d.b.ftab", SDir, $PRESET_BANK

  ; ---- load params: pick the valid file with the higher generation ----
  iGenA = -1
  iGenB = -1
  iOk filevalid SPa
  if (iOk == 1) then
    ftload SPa, 0, giSMP
    if (tab_i(767, giSMP) == $SM_STAMP) then
      iGenA = tab_i(766, giSMP)
    endif
  endif
  iOk filevalid SPb
  if (iOk == 1) then
    ftload SPb, 0, giSMP
    if (tab_i(767, giSMP) == $SM_STAMP) then
      iGenB = tab_i(766, giSMP)
    endif
  endif
  if (iGenA < 0 && iGenB < 0) then
    ; nothing valid: factory defaults (the table may hold a rejected file)
    tableicopy giSMP, giSMPdef
    tableiw $SM_STAMP, 767, giSMP
    tableiw 2, 736, giSMP
    tableiw 0, 766, giSMP
    prints "[sm] no valid saved params - defaults\n"
  else
    if (iGenA >= iGenB) then
      ftload SPa, 0, giSMP
      SOpick strcpy SOa
    else
      ftload SPb, 0, giSMP
      SOpick strcpy SOb
    endif
    ; ---- on flags from the SAME letter ----
    iOk filevalid SOpick
    if (iOk == 1) then
      ftload SOpick, 0, giSMon
    endif
    if (iOk != 1 || tab_i(23, giSMon) != $SM_STAMP) then
      iz = 0
      while (iz < 24) do
        tableiw 0, iz, giSMon
        iz += 1
      od
      tableiw $SM_STAMP, 23, giSMon
      prints "[sm] on-flag file missing/invalid - all rack effects off\n"
    endif
  endif

  ; ---- preset banks (per PRESET_BANK context) ----
  iBgA = -1
  iBgB = -1
  iOk filevalid SBPa
  if (iOk == 1) then
    ftload SBPa, 0, giSMPBank
    if (tab_i(13057, giSMPBank) == $SM_STAMP) then
      iBgA = tab_i(13056, giSMPBank)
    endif
  endif
  iOk filevalid SBPb
  if (iOk == 1) then
    ftload SBPb, 0, giSMPBank
    if (tab_i(13057, giSMPBank) == $SM_STAMP) then
      iBgB = tab_i(13056, giSMPBank)
    endif
  endif
  if (iBgA < 0 && iBgB < 0) then
    iz = 0
    while (iz < 13058) do
      tableiw 0, iz, giSMPBank
      iz += 1
    od
    tableiw $SM_STAMP, 13057, giSMPBank
  else
    if (iBgA >= iBgB) then
      ftload SBPa, 0, giSMPBank
      ftload SBOa, 0, giSMonBank
    else
      ftload SBPb, 0, giSMPBank
      ftload SBOb, 0, giSMonBank
    endif
  endif
#endif

  ; shadows = current (post-load) state
  iz = 0
  while (iz < 768) do
    tableiw tab_i(iz, giSMP), iz, giSMpsh
    iz += 1
  od
  iz = 0
  while (iz < 24) do
    tableiw tab_i(iz, giSMon), iz, giSMonPsh
    iz += 1
  od

#ifdef STATE_PERSIST
  kAB init 0
  kBAB init 0
  kMet metro 0.5
  kDirty = 0
  if (kMet == 1) then
    ki = 0
    while (ki < 766 && kDirty == 0) do
      if (tab:k(ki, giSMP) != tab:k(ki, giSMpsh)) then
        kDirty = 1
      endif
      ki += 1
    od
    ki = 0
    while (ki < 24 && kDirty == 0) do
      if (tab:k(ki, giSMon) != tab:k(ki, giSMonPsh)) then
        kDirty = 1
      endif
      ki += 1
    od
  endif
  kSaveA = 0
  kSaveB = 0
  if (kDirty == 1) then
    ki = 0
    while (ki < 768) do
      tablew tab:k(ki, giSMP), ki, giSMpsh
      ki += 1
    od
    ki = 0
    while (ki < 24) do
      tablew tab:k(ki, giSMon), ki, giSMonPsh
      ki += 1
    od
    tablew tab:k(766, giSMP) + 1, 766, giSMP      ; bump generation
    tablew tab:k(766, giSMP), 766, giSMpsh
    kAB = 1 - kAB
    kSaveA = (kAB == 0 ? 1 : 0)
    kSaveB = kAB
  endif
  ftsavek SPa, kSaveA, 0, giSMP
  ftsavek SPb, kSaveB, 0, giSMP
  ftsavek SOa, kSaveA, 0, giSMon
  ftsavek SOb, kSaveB, 0, giSMon

  ; preset bank: saved immediately after a capture/delete (flag from state_persist.orc)
  kBSaveA = 0
  kBSaveB = 0
  if (gkSMBankDirty == 1) then
    gkSMBankDirty = 0
    tablew $SM_STAMP, 13057, giSMPBank
    tablew tab:k(13056, giSMPBank) + 1, 13056, giSMPBank
    kBAB = 1 - kBAB
    kBSaveA = (kBAB == 0 ? 1 : 0)
    kBSaveB = kBAB
  endif
  ftsavek SBPa, kBSaveA, 0, giSMPBank
  ftsavek SBPb, kBSaveB, 0, giSMPBank
  ftsavek SBOa, kBSaveA, 0, giSMonBank
  ftsavek SBOb, kBSaveB, 0, giSMonBank
#endif
endin
