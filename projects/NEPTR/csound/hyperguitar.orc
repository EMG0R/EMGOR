;==============================================================
; hyperguitar.orc — the hyperGuitar control link (2026-09-15)
;
; hyperGuitar is a separate instrument on a Bela board, plugged into this Pi
; over USB. Its own NeoTrellis control surface was removed and went into the
; PMOR v2 rebuild, so it has NO controls of its own: NEPTR's menu 21 IS its
; control surface now. Design doc:
;   docs/superpowers/specs/2026-09-15-hyperguitar-neptr-link-design.md
;
; This is a CONTROL link only. hyperGuitar's audio reaches this rig the way it
; already did, over the Bela's UAC2 USB audio bridge — nothing here touches the
; signal chain, which is also why this lives in its own file instead of in
; globals.orc/signal_chain.orc.
;
; ---------------- TRANSPORT ----------------
;   OUT  -> 192.168.7.2:7772   /hg/lane  iii  (lane, on, index)
;                              /hg/pedal f    (0..1 expression pedal)
;                              /hg/sync       (resend everything)
;   IN   <- on giCtlOSC (7770, the port /cc and /p already arrive on)
;                              /hg/state iii  (lane, on, index)
;                              /hg/hello i    (heartbeat, 1 Hz)
;
; Reusing 7770 for the inbound half is why this needs no new daemon and no new
; listener process: the engine is already bound there.
;
; ---------------- PRESENCE ----------------
; Menu 21 exists ONLY while hyperGuitar is alive. That is driven by the Bela's
; 1 Hz heartbeat, NOT by a USB or network check on this end, because what the
; menu needs to know is "is the instrument running and listening" — a Bela that
; is booting, or running some other project, would pass an ifconfig check and
; still give you a menu that sends into a void.
;
; ---------------- WHO OWNS THE STATE ----------------
; The Bela does. Every lane change there — from this menu today, from a
; re-attached Trellis tomorrow — ends in its hgLaneApply(), which echoes
; /hg/state back. gkHgLaneOn/gkHgLaneIdx here are a MIRROR that chases that
; echo; they are never the authority. That is the same discipline the touch UI
; already uses against this engine (sliders chase the OSC echo), and it is what
; makes the future Trellis sync for free instead of needing a second protocol.
;==============================================================

#ifndef HG_HOST
#define HG_HOST #"192.168.7.2"#
#endif
#ifndef HG_PORT
#define HG_PORT #7772#
#endif

; Menu index. nav.orc's wrap arithmetic reads this, so the menu can only ever
; be appended at the end of the cycle — which is what we want: adding it must
; not renumber any existing menu, because handler_looper.orc hardcodes 14 and
; several dispatch files branch on literal menu numbers.
; 2026-10-22: 21 -> 22 when VOCAL (always-on, touch-only) took menu 21, per the
; docs/menus.md rule that conditional ids shift and new always-on menus go before them.
; 2026-10-22: 22 -> 23 when MIXER (always-on encoder page) took menu 22.
#define HG_MENU #23#

#define HG_NLANES #5#

; Mirror of the Bela's lane state. Index 0 = leftmost on screen.
gkHgLaneOn[]  init 5
gkHgLaneIdx[] init 5

; Relay sequence counter for the UI hand-off below. Declared up here with the
; other globals because a global referenced inside an instrument must already
; exist when that instrument is compiled.
gkHgUiSeq init 0

; 1 while the heartbeat is alive. nav.orc adds this to the menu count, so the
; HYPERGUITAR page appears and disappears with the instrument.
gkHgPresent init 0

; The expression pedal's value WHEN THE GUITAR OWNS IT. Written only by
; ExprSet target 74 (handler/expr_targets.src.orc), relayed to the Bela by
; instr 98. Starts at 0.5 = centre of the lane's range, so engaging the lane
; before touching the pedal does not slam the pitch to the bottom of its span.
gkHgPedal init 0.5

; Toggle id the PS lane claims on engage, so the treadle comes OFF whatever it
; was driving (menu-0 presets park it on 30 = gkMasterVol) and onto the guitar,
; exactly the way every other pedal-using effect behaves.
#define HG_EXPR_ID #74#

; How many entries each lane has, so a rotation can wrap without asking the
; Bela. MUST match gHgLane[].nEntries in bela/hyperGuitar6chPan/hg_lanes.cpp —
; tests/test_hyperguitar_link.py asserts that by parsing both files, because a
; silent mismatch here would show the user a preset index the Bela does not
; have and the lane would appear to stick at the end of its list.
giHgLaneLen[] fillarray 6, 9, 7, 10, 10

; Heartbeat timeout: 3 missed beats. Derived from kr so it stays 3 seconds at
; any ksmps — this engine is launched at ksmps 64 today but the macro is a
; launch-time override.
giHgTimeout = kr * 3

opcode HgSendLane, 0, kkk
  kLane, kOn, kIdx xin
  ; The monotonic-counter trigger idiom from OSC3x_f in globals.orc: OSCsend
  ; fires when kwhen CHANGES, so a constant 1 inside a conditional would send
  ; once and then go quiet forever. The counter is UDO-local, so each call site
  ; gets its own and they cannot interfere.
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, $HG_HOST, $HG_PORT, "/hg/lane", "iii", kLane, kOn, kIdx
endop

opcode HgSendPedal, 0, k
  kNorm xin
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, $HG_HOST, $HG_PORT, "/hg/pedal", "f", kNorm
endop

opcode HgSendSync, 0, k
  kTrig xin
  ksend init 0
  ksend = ksend + 1
  ; carries a dummy int rather than an empty type string: an argument-less
  ; OSCsend is not reliably accepted, and the Bela ignores the payload
  OSCsend ksend, $HG_HOST, $HG_PORT, "/hg/sync", "i", kTrig
endop

;--------------------------------------------------------------
; A NEPTR encoder moved. Called from the menu-21 branches of the toggle and
; rotation dispatch files. We send the INTENT and do NOT update the mirror —
; the mirror only ever moves when the Bela's echo comes back. Updating it here
; too would make the UI lie for the ~2 ms before the echo lands, and would hide
; a dead link completely: with this discipline, if hyperGuitar stops answering
; the lane visibly stops responding, which is the truth.
;--------------------------------------------------------------
#define HG_TOGGLE(L) #
  if (gkHgPresent == 1) then
    HgSendLane $L, (gkHgLaneOn[$L] == 0 ? 1 : 0), gkHgLaneIdx[$L]
    ; LANE 0 CLAIMS THE EXPRESSION PEDAL ON ENGAGE. It is the 100%-wet pitch
    ; shifter whose whole point is being played from the treadle, so engaging
    ; it must take the pedal off volume — the standing last-toggled-wins rule.
    ; Only on the ON edge: a toggle-OFF leaves the claim alone, matching the
    ; documented behaviour of every other effect (expr_cc29.orc).
    if ($L == 0 && gkHgLaneOn[0] == 0) then
      gkLastToggle29 = $HG_EXPR_ID
    endif
  endif
#

#define HG_ROTATE(L'kDir) #
  if (gkHgPresent == 1) then
    kHgNew_$L. = gkHgLaneIdx[$L] + $kDir
    ; wrap rather than clamp: this is an accumulator the player spins, and
    ; stopping dead at the end of the list reads as a fault
    kHgNew_$L. = (kHgNew_$L. < 0 ? giHgLaneLen[$L] - 1 : kHgNew_$L. % giHgLaneLen[$L])
    ; a rotation also ENGAGES the lane — you turn the knob to hear the thing,
    ; and having to press first would make every lane a two-gesture control
    HgSendLane $L, 1, kHgNew_$L.
  endif
#

; NOTE ON SPLIT (2026-09-15): the presence instrument lives in
; hyperguitar_link.orc, included AFTER midi_handler.orc, because it listens on
; giCtlOSC which that file creates. Everything above must be here instead —
; BEFORE midi_handler.orc — because the menu-21 dispatch branches that use
; $HG_TOGGLE/$HG_ROTATE are included from inside it, and a csound macro has to
; be defined before the line that expands it.
