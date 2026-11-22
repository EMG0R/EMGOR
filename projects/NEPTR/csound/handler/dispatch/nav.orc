    ;==========================================================
    ; MENU NAVIGATION
    ;==========================================================

    ; 19 menus 0-18 (2026-07-26 delay/verb-expansion reorg: the old TIME menu
    ; (8: MOOD, GRANULAR, REVERB, DELAY) is dissolved into THREE menus —
    ; 8 GRANULAR (MOOD, GRANULAR, +3 free), 9 DIGIVERB (slot order per the UI's
    ; list-index->slot mapping, index0=slot4/CC72 ... index4=slot0/CC74:
    ; NEBULA(s4/CC72) SHIMMER(s3/CC71) FREEZE(s2/CC76, gesture) GRAINVERB
    ; (s1/CC75) REVERB(s0/CC74, LAST — user mandate)), 10 DELAY (same
    ; mapping: TAPE DELAY(s4/CC72) SPECDLY(s3/CC71) MOVE DELAY(s2/CC76) free
    ; (s1/CC75) DELAY(s0/CC74)). SPECDLY is menu-10 ONLY now (phase 2 —
    ; BINSCATTER took its old dual-homed SPECTRAL-menu-2 slot 2/CC76 instead).
    ; Everything from 9 BASS onward shifted +2: 11 BASS, 12 SYNTH, 13 AIR,
    ; 14 LOOPER, 15 CHASE BLISS, 16 GLITCH, 17 NAM, 18 IR VERB — menu-prev
    ; from PRESETS (wrapping) now lands on IR VERB, then NAM, GLITCH, CHASE
    ; BLISS, LOOPER.)
    ; MENU COUNT IS DYNAMIC (2026-09-15). 21 menus normally (18=IR VERB,
    ; 19=UTILITY, 20=NORRIS), 22 while hyperGuitar is plugged in and answering its
    ; heartbeat — menu 21 = HYPERGUITAR. gkHgPresent is 0 or 1 and is settled
    ; for this k-cycle by instr 98, which runs before this one.
    ;
    ; The extra menu is APPENDED, never inserted: handler_looper.orc hardcodes
    ; menu 14 and a dozen dispatch branches test literal menu numbers, so
    ; renumbering to keep hyperGuitar next to anything would break all of them.
    ; ---- MENU CYCLE ORDER (re-ordered 2026-10-24) ---------------------------
    ; ALL menus are walked, in giMenuCycle's order rather than numeric order
    ; (menus.json array order -> tools/gen_menus.py):
    ;   PRESETS(0) UTILITY(19) VOLUME(1) SPECTRAL(2) PITCH(3) FILTERING(4)
    ;   MOD(5) DRIVE(6) PREAMP(7) GRANULAR(8) DIGIVERB(9) DELAY(10) BASS(11)
    ;   SYNTH(12) AIR(13) NORRIS(20) IR VERB(18) NAM(17) CHASE BLISS(15)
    ;   GLITCH(16) LOOPER(14) MIXER(22) VOCAL(21) [HYPERGUITAR(23)]
    ; menu-PREV from PRESETS therefore lands on VOCAL (or HYPERGUITAR while plugged in).
    ; HYPERGUITAR (23) is the last entry and only counted while present.
    ; If gkCurrentMenu somehow sits outside the table, the next step enters at
    ; the first entry instead of getting stuck.
    ; Cycle length is data now: gkMenuCycleN comes from handler/gen/menu_cycle.orc
    ; (menus.json) and menu_reload.orc can change it live. A trailing
    ; HYPERGUITAR entry is only walked while the guitar is present.
    kMenuN = gkMenuCycleN
    if (tab:k(gkMenuCycleN - 1, giMenuCycle) == $HG_MENU && gkHgPresent == 0) then
      kMenuN = gkMenuCycleN - 1
    endif
    kCycPos = -1
    kCycI = 0
    while (kCycI < kMenuN) do
      if (gkCurrentMenu == tab:k(kCycI, giMenuCycle)) then
        kCycPos = kCycI
      endif
      kCycI = kCycI + 1
    od

    if (kCC == 70 && kVal == 127) then
      if (kCycPos < 0) then
        gkCurrentMenu = tab:k(0, giMenuCycle)
      else
        gkCurrentMenu = tab:k((kCycPos - 1 + kMenuN) % kMenuN, giMenuCycle)
      endif
      OSC3x_i "/menu", gkCurrentMenu

    elseif (kCC == 73 && kVal == 127) then
      if (kCycPos < 0) then
        gkCurrentMenu = tab:k(0, giMenuCycle)
      else
        gkCurrentMenu = tab:k((kCycPos + 1) % kMenuN, giMenuCycle)
      endif
      OSC3x_i "/menu", gkCurrentMenu

