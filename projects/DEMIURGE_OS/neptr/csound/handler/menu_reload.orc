;==============================================================
; MENU_RELOAD.ORC - live menu-cycle order over OSC (no restart).
;==============================================================
; The web backend (PUT /api/neptr/menus) pushes a new nav order here:
;   /mc    "ii"  position, menuId   -> giMenuCycle[position] = menuId
;   /mc_n  "i"   count              -> gkMenuCycleN = count (clamped 1..32)
; Send the /mc entries first and /mc_n last. Hidden menus are simply not sent.
; Boot-time order comes from handler/gen/menu_cycle.orc (generated from
; neptr/menus.json); this only changes the running engine, so menus.json is
; what survives a restart. Menu ids are never renumbered.
;
; Included AFTER midi_handler.orc (like hyperguitar_link.orc) because it
; listens on giCtlOSC, which midi_handler.orc creates.
;==============================================================
instr 95
  kMcPos init 0
  kMcId init 0
  kMcN init 1
  kMcMore init 1
  kMcMore = 1
  while (kMcMore == 1) do
    kMcGot OSClisten giCtlOSC, "/mc", "ii", kMcPos, kMcId
    if (kMcGot == 1) then
      if (kMcPos >= 0 && kMcPos < ftlen(giMenuCycle) && kMcId >= 0) then
        tablew kMcId, kMcPos, giMenuCycle
      endif
    else
      kMcMore = 0
    endif
  od
  kMnMore init 1
  kMnMore = 1
  while (kMnMore == 1) do
    kMnGot OSClisten giCtlOSC, "/mc_n", "i", kMcN
    if (kMnGot == 1) then
      gkMenuCycleN = limit(kMcN, 1, ftlen(giMenuCycle))
    else
      kMnMore = 0
    endif
  od
endin
schedule 95, 0, -1
