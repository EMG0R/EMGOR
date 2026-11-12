;==============================================================
; PRESET ACTIVATION SEQUENCES (2026-11-02)
;
; A preset = the 512-double state row (state_persist.orc bank) + an ORDERED
; sequence of up to 16 steps, replayed on recall as if pressed by hand, so the
; LAST step that claims the expression pedal owns it (normally VOLUME).
;
; Sidecar table giSeqBank, same size as the bank so the ftsave header is
; identical (tools write it by copying the bank's header):
;   slot s (1..17) step j (0..15) field f (0..3) -> ((s-1)*16 + j)*4 + f
;       field 0 = type : 0 empty   1 EFFECT ON (a = effect id, tools/effects.json)
;                        2 VOCAL   (a = 0/1)
;                        3 NAM AMP (a = flat profile idx 0..24, b = in dB, c = out dB)
;   SEQ_FLAG + s-1 : 1 = this slot's sequence is authoritative (even if empty:
;                    everything not listed is off); 0 = legacy slot (row only)
;   index 8730 generation, 8735 stamp (same positions as the bank)
; Files: <preset dir>/seq.ftab + seq.b.ftab (A/B, higher generation wins).
;
; Also here: the activation-order ring used by device capture. A edge scan of
; every effect's on-state (PresetScan, generated) pushes an id when it goes ON
; and drops it when it goes OFF, so at capture the ring is exactly "effects
; currently on, in the order they were switched on".
;==============================================================

giSeqBank   ftgen 0, 0, -8736, -2, 0
giSeqTmpBk  ftgen 0, 0, -8736, -2, 0     ; reload scratch (bank)
giSeqTmpSq  ftgen 0, 0, -8736, -2, 0     ; reload scratch (sequences)
giSeqRing   ftgen 0, 0, -32, -2, 0       ; [0] = count, [1..31] = ids, oldest first
giSeqPrev   ftgen 0, 0, -128, -2, 0      ; last scanned on-state per effect id
#define SEQ_FLAG   #1100#
#define SEQ_STAMP  #771218#
gkSeqMode init 0                         ; 0 follow edges, 1 resync, 2 rebuild (see PresetScan)

opcode PresetSeqClear, 0, 0
  tablew 0, 0, giSeqRing
endop

opcode PresetSeqDrop, 0, k
  kId xin
  kN = tab:k(0, giSeqRing)
  kI = 1
  kO = 1
  while (kI <= kN) do
    kV = tab:k(kI, giSeqRing)
    if (kV != kId) then
      tablew kV, kO, giSeqRing
      kO += 1
    endif
    kI += 1
  od
  tablew kO - 1, 0, giSeqRing
endop

opcode PresetSeqPush, 0, k
  kId xin
  PresetSeqDrop kId
  kN = tab:k(0, giSeqRing)
  if (kN >= 31) then
    kI = 2
    while (kI <= 31) do
      tablew tab:k(kI, giSeqRing), kI - 1, giSeqRing
      kI += 1
    od
    kN = 30
  endif
  tablew kId, kN + 1, giSeqRing
  tablew kN + 1, 0, giSeqRing
endop

; NAM amp step: same state a hand NAM press + encoders end up in. The settings
; change-scan persists it (slots 272..275), so it survives reboot.
opcode PresetApplyNam, 0, kkk
  kP, kIn, kOut xin
  kP = limit(int(kP), 0, giNAMProfileMax)
  kRow = int(kP / 5)
  gkNAMActiveRow = kRow
  gkNAMRowSel[kRow] = kP - kRow * 5
  gkNAMProfileIdx = kP
  gkNAMBypass = 0
  gkNAMInGain = limit(kIn, -12, 18)
  gkNAMOutGain = limit(kOut, -18, 18)
  gkLastToggle = 37
endop

#include "preset_apply.gen.orc"

; Replay slot kSlot's sequence: GUITAR effects off, then every step in order.
opcode PresetRecallSeq, 0, k
  kSlot xin
  PresetSweepOff
  PresetSeqClear
  kBase = (kSlot - 1) * 64
  kJ = 0
  while (kJ < 16) do
    kT = tab:k(kBase + kJ * 4, giSeqBank)
    kA = tab:k(kBase + kJ * 4 + 1, giSeqBank)
    kB = tab:k(kBase + kJ * 4 + 2, giSeqBank)
    kC = tab:k(kBase + kJ * 4 + 3, giSeqBank)
    if (kT == 1) then
      PresetApplyFx kA
      PresetSeqPush kA
    elseif (kT == 2) then
      chnset (kA >= 0.5 ? 1 : 0), "voc_gate"   ; VOCAL step = the x0/x1 gate
    elseif (kT == 3) then
      PresetApplyNam kA, kB, kC
    endif
    kJ += 1
  od
  gkSeqMode = 1
endop

; Device capture: ring (oldest first, newest 15) + VOLUME last if it is on.
; NO NAM / VOCAL step - the amp and vocal strip stay separate layers.
opcode PresetSeqCapture, 0, k
  kSlot xin
  kBase = (kSlot - 1) * 64
  kI = 0
  while (kI < 64) do
    tablew 0, kBase + kI, giSeqBank
    kI += 1
  od
  kN = tab:k(0, giSeqRing)
  kOut = 0
  kI = max(1, kN - 14)
  while (kI <= kN) do
    kId = tab:k(kI, giSeqRing)
    if (kId != 1 && kOut < 15) then
      tablew 1, kBase + kOut * 4, giSeqBank
      tablew kId, kBase + kOut * 4 + 1, giSeqBank
      kOut += 1
    endif
    kI += 1
  od
  if (gkVOLpedalBypass < 0.5) then
    tablew 1, kBase + kOut * 4, giSeqBank
    tablew 1, kBase + kOut * 4 + 1, giSeqBank
  endif
  tablew 1, $SEQ_FLAG + kSlot - 1, giSeqBank
endop

opcode PresetSeqClearSlot, 0, k
  kSlot xin
  kBase = (kSlot - 1) * 64
  kI = 0
  while (kI < 64) do
    tablew 0, kBase + kI, giSeqBank
    kI += 1
  od
  tablew 0, $SEQ_FLAG + kSlot - 1, giSeqBank
endop
