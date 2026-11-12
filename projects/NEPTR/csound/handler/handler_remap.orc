    ;==========================================================
    ; PHYSICAL ENCODER REMAP  (DEMIURGE 2026 — user pedalboard layout)
    ;==========================================================
    ; Hardware enc -> CC (button / rotation), per the Teensy firmware (SENSOR_MAP):
    ;   E1 70/20  E2 71/21  E3 72/22  E4 73/23  E5 74/24  E6 75/25  E7 76/26
    ;   rotation: CW=127, CCW=0    button: press=127, release=0
    ;
    ; ROLES (this layout):
    ;   E7 = PREVIOUS-MENU encoder: BUTTON press = prev menu; ROTATION = secondary param
    ;   E5 = NEXT-MENU encoder:     BUTTON press = next menu; ROTATION = tertiary param
    ;   Effect slots: each encoder's BUTTON toggles its effect and the SAME encoder's
    ;     ROTATION is that effect's dry/wet (linked). Physical RIGHT -> LEFT =
    ;     E1, E4, E3, E2, E6 = slot0..slot4.
    ;
    ; Pure permutation of the PHYSICAL cc -> the LOGICAL cc the dispatch below
    ; already uses (menu prev=70 next=73; slot toggles 74/75/76/71/72; slot rot
    ; 24/25/26/21/22; secondary param=20, tertiary=23; pedal=29). Button presses
    ; and rotation events pass their value (127/0) through unchanged — menu nav
    ; is a BUTTON press, NOT rotation.
    kCCx = kCC
    if (kCC == 76) then
      kCCx = 70           ; E7 BUTTON  -> menu PREV
    elseif (kCC == 74) then
      kCCx = 73           ; E5 BUTTON  -> menu NEXT
    elseif (kCC == 26) then
      kCCx = 20           ; E7 rotation -> secondary param (other aspect of effect)
    elseif (kCC == 24) then
      kCCx = 23           ; E5 rotation -> tertiary param  (other aspect of effect)
    elseif (kCC == 70) then
      kCCx = 74           ; E1 button  -> slot0 toggle
    elseif (kCC == 20) then
      kCCx = 24           ; E1 rotation -> slot0 dry/wet
    elseif (kCC == 73) then
      kCCx = 75           ; E4 button  -> slot1 toggle
    elseif (kCC == 23) then
      kCCx = 25           ; E4 rotation -> slot1 dry/wet
    elseif (kCC == 72) then
      kCCx = 76           ; E3 button  -> slot2 toggle
    elseif (kCC == 22) then
      kCCx = 26           ; E3 rotation -> slot2 dry/wet
    ; E2 button 71 -> 71 and E2 rotation 21 -> 21 are unchanged (slot3)
    elseif (kCC == 75) then
      kCCx = 72           ; E6 button  -> slot4 toggle
    elseif (kCC == 25) then
      kCCx = 22           ; E6 rotation -> slot4 dry/wet
    endif
    kCC = kCCx
