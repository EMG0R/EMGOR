    ;==========================================================
    ; HANDLER_MENUS — CC dispatch chain (split 2026-08-12)
    ;==========================================================
    ; This file WAS 2521 lines of one flat if/elseif chain. It is now a
    ; manifest: the chain is unchanged, but each top-level `elseif (kCC == n)`
    ; branch lives in its own file under handler/dispatch/.
    ;
    ; The split is PURE TEXT MOTION. Concatenating the 14 files below in this
    ; order reproduces the pre-split file byte for byte (sha256
    ; 91372fe6c501b5a15dcd64f3b1dace54146031d7cee1725d338431966e8faf7b).
    ; Not one line of dispatch logic was edited. Re-verify at any time with:
    ;
    ;   cd handler/dispatch && cat nav.orc toggle_slot0.orc toggle_slot1.orc \
    ;     toggle_slot2.orc toggle_slot3.orc toggle_slot4.orc \
    ;     rot_slot0_cc24.orc rot_slot1_cc25.orc rot_slot2_cc26.orc \
    ;     rot_slot3_cc21.orc rot_slot4_cc22.orc rot_p2_cc20.orc \
    ;     rot_p3_cc23.orc expr_cc29.orc | shasum -a256
    ;
    ; WHY IT SPLITS WHERE IT DOES — the control model, for anyone editing here:
    ;
    ;   ENCODER BUTTONS are the SELECTOR. They are slot- and menu-keyed: a slot
    ;     button toggles that slot's effect in the current menu AND sets
    ;     gkLastToggle to that effect's id. The two menu-encoder buttons step
    ;     gkCurrentMenu. This is the only part that cares how many effects a
    ;     menu holds.  -> nav.orc, toggle_slot0..4.orc
    ;
    ;   ALL SEVEN ENCODER ROTATIONS are gkLastToggle-keyed — P1..P7 of the
    ;     FOCUSED effect, independent of menu and slot (the 2026-06-23
    ;     "full takeover"; see rot_slot0_cc24.orc's header). Rotations do not
    ;     belong to slots. An effect's parameter count is therefore not capped
    ;     by how many slots its menu has.
    ;     -> rot_slot0_cc24 / rot_slot1_cc25 / rot_slot2_cc26 /
    ;        rot_slot3_cc21 / rot_slot4_cc22 / rot_p2_cc20 / rot_p3_cc23
    ;
    ;   THE EXPRESSION PEDAL is gkLastToggle29-keyed — a SECOND, stickier
    ;     focus that only re-points when an effect explicitly claims the pedal
    ;     on toggle-ON, and never releases on toggle-OFF.  -> expr_cc29.orc
    ;
    ;   OVERRIDES are checked BEFORE the focus dispatch inside each rotation
    ;     file: menu 17 (NAM) and 18 (IR VERB) claim the five slot rotations
    ;     for variant browse, menu 0 claims CC20 for preset trim, the LOOPER
    ;     menu claims slot rotations via $LP_CH*_SPEED, and menu 0's slot
    ;     rotations are inert (`gkCurrentMenu != 0`).
    ;
    ; Each fragment BEGINS with `elseif (kCC == n) then` (nav.orc opens with
    ; the `if`) and none of them closes the chain — the terminating `endif` is
    ; in midi_handler.orc ("end CC dispatch"). They are therefore not
    ; standalone files and must stay in this order.
    ;
    ; Paths are relative to the .csd's directory, matching how midi_handler.orc
    ; includes this file.
    ;==========================================================

#include "handler/dispatch/nav.orc"
#include "handler/dispatch/toggle_slot0.orc"
#include "handler/dispatch/toggle_slot1.orc"
#include "handler/dispatch/toggle_slot2.orc"
#include "handler/dispatch/toggle_slot3.orc"
#include "handler/dispatch/toggle_slot4.orc"
#include "handler/dispatch/rot_slot0_cc24.orc"
#include "handler/dispatch/rot_slot1_cc25.orc"
#include "handler/dispatch/rot_slot2_cc26.orc"
#include "handler/dispatch/rot_slot3_cc21.orc"
#include "handler/dispatch/rot_slot4_cc22.orc"
#include "handler/dispatch/rot_p2_cc20.orc"
#include "handler/dispatch/rot_p3_cc23.orc"
#include "handler/dispatch/expr_cc29.orc"
