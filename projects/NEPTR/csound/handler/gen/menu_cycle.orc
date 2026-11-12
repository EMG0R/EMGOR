;==============================================================
; MENU_CYCLE.ORC - GENERATED. DO NOT HAND-EDIT.
;==============================================================
; Source:    menus.json  (sha256 ed16adc683cd2ef1)
; Generator: tools/gen_menus.py
;
; giMenuCycle holds the menu ids nav.orc walks, in order. Hidden menus are
; simply absent. The table has room for 32 entries; only the first
; giMenuCycleN are live. gkMenuCycleN is the live (k-rate) length, so
; menu_reload.orc can change the order over OSC without a restart.
; Menu ids are never renumbered - dispatch branches test literal ids.
; Order: 0 PRESETS, 19 UTILITY, 1 VOLUME, 2 SPECTRAL, 3 PITCH, 4 FILTERING, 5 MOD, 6 DRIVE, 7 PREAMP, 8 GRANULAR, 9 DIGIVERB, 10 DELAY, 11 BASS, 12 SYNTH, 13 AIR, 20 NORRIS, 18 IR VERB, 17 NAM, 15 CHASE BLISS, 16 GLITCH, 14 LOOPER, 22 MIXER, 21 VOCAL, 23 HYPERGUITAR
;==============================================================
giMenuCycle ftgen 0, 0, -32, -2, 0, 19, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 18, 17, 15, 16, 14, 22, 21, 23, 0, 0, 0, 0, 0, 0, 0, 0
giMenuCycleN = 24
gkMenuCycleN init 24
#define MENU_CYCLE_N #24#
