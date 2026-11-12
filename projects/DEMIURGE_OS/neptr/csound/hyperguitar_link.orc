
;==============================================================
; hyperguitar_link.orc — the hyperGuitar presence + state-mirror instrument.
;
; Split out of hyperguitar.orc so it can be included AFTER midi_handler.orc:
; it listens on giCtlOSC, which that file creates. See hyperguitar.orc for the
; protocol, the presence rationale and who owns the state.
;==============================================================

instr 98  ; HYPERGUITAR LINK — presence + inbound state mirror
  ; Runs before instr 99 (the MIDI/CC handler) so gkHgPresent is already
  ; settled for this k-cycle when the menu dispatch reads it.

  ;---------------- heartbeat ----------------
  kHello init 0
  kAge   init 100000        ; start "long dead" so boot does not flash present
  kGotHello = 0
  kMoreH init 1
  kMoreH = 1
  while (kMoreH == 1) do
    kH OSClisten giCtlOSC, "/hg/hello", "i", kHello
    if (kH == 1) then
      kGotHello = 1
    else
      kMoreH = 0
    endif
  od

  if (kGotHello == 1) then
    kAge = 0
  else
    kAge = kAge + 1
  endif

  kPrevPresent = gkHgPresent
  gkHgPresent = (kAge < giHgTimeout ? 1 : 0)

  ; On every 0->1 transition ask for a full resend. This covers the two cases
  ; that would otherwise leave the menu showing stale lanes: hyperGuitar
  ; restarting (its lanes reset to off, ours would still show them on) and the
  ; USB link dropping and coming back.
  if (gkHgPresent == 1 && kPrevPresent == 0) then
    HgSendSync 1
  endif

  ; Presence lost while the user is sitting on the page — don't strand them on
  ; a menu that no longer exists. nav.orc's modulo would also leave
  ; gkCurrentMenu out of range, which would index the UI's menu list off the end.
  if (gkHgPresent == 0 && kPrevPresent == 1 && gkCurrentMenu == $HG_MENU) then
    gkCurrentMenu = 0
    OSC3x_i "/menu", gkCurrentMenu
  endif

  ;---------------- inbound state echo ----------------
  ; Drained in a loop, same reason midi_handler.orc drains /cc: OSClisten
  ; yields at most ONE message per call, and a sync answer is five of them.
  kLane init 0
  kOn   init 0
  kIdx  init 0
  kMoreS init 1
  kMoreS = 1
  while (kMoreS == 1) do
    kS OSClisten giCtlOSC, "/hg/state", "iii", kLane, kOn, kIdx
    if (kS == 1) then
      if (kLane >= 0 && kLane < $HG_NLANES) then
        gkHgLaneOn[kLane]  = kOn
        gkHgLaneIdx[kLane] = kIdx
        ; straight through to the UI — one relay, no interpretation
        OSCsend kS + gkHgUiSeq, "127.0.0.1", 8000, "/hg_lane", "iii", kLane, kOn, kIdx
        gkHgUiSeq = gkHgUiSeq + 1
      endif
    else
      kMoreS = 0
    endif
  od

  ; ---- pedal -> guitar, ONLY while the guitar owns the pedal ----
  ; gkHgPedal is written exclusively by ExprSet target 74, which only runs when
  ; gkLastToggle29 == 74. So when the player points the treadle at something
  ; else, this simply stops changing and the guitar holds its last interval
  ; instead of being driven by a pedal that is now doing another job.
  if (gkHgPresent == 1) then
    kPedChg changed gkHgPedal
    if (kPedChg == 1) then
      HgSendPedal gkHgPedal
    endif
  endif

  ; ---- presence to the UI ----
  ;
  ; ON CHANGE **AND** ON A 2 Hz HEARTBEAT OF ITS OWN, not on change alone.
  ;
  ; Change-only is what every other broadcast in this engine does, and for a
  ; parameter that is correct: the UI asks for a full snapshot at startup
  ; (ch16 CC102 -> gkOscRefresh) and catches up. Presence is different, and it
  ; bit us on 2026-09-15: the engine restarted at 18:07:22, the guitar's
  ; heartbeat arrived about a second later, gkHgPresent went 0->1 and the one
  ; and only /hg_present broadcast fired — and the UI did not start until
  ; 18:10:57, three and a half minutes later. It missed the edge, never learned
  ; the guitar was there, and clamped every menu-21 echo back to IR VERB. The
  ; symptom is "my hyperGuitar menu is not showing even though it is plugged
  ; in", with nothing wrong anywhere in the link.
  ;
  ; Any ordering of engine and UI startup must converge, and a UI can also be
  ; restarted on its own at any time. Re-stating presence twice a second costs
  ; two localhost datagrams and removes the whole class of failure, so it is
  ; not worth being clever about. The change-triggered send is kept as well so
  ; a real transition is still reported immediately rather than up to 500 ms
  ; late.
  $OSC_I(gkHgPresent'/hg_present'hgPres)
  kPresTick init 0
  kPresTick = kPresTick + 1
  if (kPresTick >= kr / 2) then
    kPresTick = 0
    OSC3x_i "/hg_present", gkHgPresent
  endif

  ; ---- lane mirror on a sync request ----
  ; A late-joining UI needs the LANES too, not just presence. gkOscRefresh is
  ; the engine's existing "resend everything" pulse; answer it by re-stating
  ; the mirror we hold AND asking the guitar to re-send, so the mirror itself
  ; cannot be stale. instr 98 is numbered below 99, so it sees the pulse on the
  ; cycle after instr 99 raises it and before instr 99 clears it — see the
  ; gkOscRefresh note in globals.orc.
  if (gkOscRefresh == 1) then
    kRi = 0
    while (kRi < $HG_NLANES) do
      OSCsend gkHgUiSeq + kRi, "127.0.0.1", 8000, "/hg_lane", "iii", kRi, gkHgLaneOn[kRi], gkHgLaneIdx[kRi]
      kRi = kRi + 1
    od
    gkHgUiSeq = gkHgUiSeq + $HG_NLANES
    if (gkHgPresent == 1) then
      HgSendSync 1
    endif
  endif
endin

schedule 98, 0, -1
