# Status

Living checklist while this lives in the games folder. Update here; only promote
to the main Demiurge docs / setup-script when a piece is **proven on the Pi**.

## Done (built + unit-tested on Mac)
- [x] `lib/combo.py` — 1s arcade+joystick-click handshake, one-shot toggle. **tested**
- [x] `lib/config.py` — games.conf parser (live.conf-style). **tested**
- [x] `lib/teensy.py` — vendored serial reader + frame parser + synth MIDI emitter. **parser tested**
- [x] `lib/gamepad.py` — uinput virtual gamepad (Player 1) mapping.
- [x] `arcade_supervisor.py` — serial owner, combo→toggle, synth/console routing, freeze/thaw, RetroArch launch.
- [x] `bin/demiurge-freeze` / `demiurge-thaw` — SIGSTOP/SIGCONT demiurge.service.
- [x] `bin/games-install.sh` — RetroArch + deps + uinput + sudoers.
- [x] `retroarch/retroarch.cfg` — RGUI, PipeWire audio, persistent saves.
- [x] `games.conf`, docs.

## Verified ON THE PI (2026-06-15)
- [x] 9/9 unit tests pass on the Pi (python3-pytest).
- [x] RetroArch 1.20.0 installed; KMS video context available.
- [x] evdev installed; `/dev/uinput` set to 660 root:input (udev rule); virtual
      gamepad "Demiurge Arcade P1" creates successfully.
- [x] Supervisor opens /dev/ttyACM0, reads real Teensy frames, creates the
      'NEPTR Serial Bridge' ALSA-seq port (synth path live on hardware).
- [x] **Freeze/thaw pauses BOTH demiurge.service AND neptr-ui.service** — both
      MainPIDs go T (stopped) on freeze and back to S on thaw. UI process frozen
      in place = menu + face preserved in memory (a restart would lose them).
- [x] sudo is passwordless on the Pi; freeze/thaw work via `sudo -n`.

## Verified in the TESTING_GROUND sandbox (2026-06-15)
- [x] Switched Pi to the **TESTING_GROUND** patch (empty chain, `sync_layer = on`,
      PipeWire up, no csound, no Scarlett dependency, resets clean) as the test bed.
- [x] **Display handoff WORKS** via stop-the-UI (not freeze): `ui-release` stops
      neptr-ui → frees DRM master on card1 → RetroArch KMS modeset succeeds
      (`Found GL context "kms"`, `New FB 1280x720`, stays running). With the UI
      *frozen* it FAILED (`KMS: Error when switching mode`) — kivy keeps master.
- [x] **Game audio rides the sync layer**: RetroArch logs `[PipeWire]: Found Sink
      Node: demiurge-sink`, audio driver starts, `RetroArch` node appears in the
      graph. Launch sets `PIPEWIRE_NODE=demiurge-sink`.
- [x] Supervisor rewired: enter = ui-release + SIGSTOP audio + RetroArch; exit =
      close RetroArch + ui-restore + SIGCONT audio.

## Still needs YOUR eyes/hands (physical, not scriptable over SSH)
- [ ] Watch the panel during a real combo: face UI drops, RetroArch menu appears,
      combo again → UI restarts. (Mechanism proven; just needs a visual confirm.)
- [ ] Handshake fires from the PHYSICAL arcade button (b1) + joystick click (jb).
- [ ] Audible: synth freezes + resumes on the exact sample (test in a real patch,
      not the empty sandbox).
- [ ] Joystick maps 4-way; arcade button = fire/OK in the RGUI menu.
- [ ] High scores persist across reboot (NVRAM in saves/).

## Controls + portrait 2026-06-15
- [x] **Controls fixed**: the custom uinput *gamepad* had no RetroArch autoconfig
      → nothing responded. Switched `lib/gamepad.py` to a virtual **KEYBOARD**
      (arrows + SPACE + ENTER) with EXPLICIT `input_player1_*` binds in
      retroarch.cfg (no autoconfig needed). Verified the device emits the right
      keys (evdev read-back): joystick→arrows, arcade→space(A+B), joy-click→enter
      (Start). RetroArch reads it via udev. NEEDS visual confirm in-game.
- [x] **Portrait**: `video_rotation = "3"` (270°, matches the face-UI mount).
      Flip to "1" if upside-down. (`video_allow_rotate = true`.)

## Locked in 2026-06-15 (persistent service + CAR_MODE self-test)
- [x] Installed `config/demiurge-arcade.service` (system unit, User=gorpi7) to
      /etc/systemd/system, **enabled (autostarts on boot)**, active. It is the
      persistent joystick/arcade-button monitor + console toggle; sole owner of
      /dev/ttyACM0 (don't also run serial_bridge.py).
- [x] **Self-tested the full console cycle in CAR_MODE** (sync_layer=on here):
      ENTER → UI stopped + freeze (0 stages) + RetroArch renders (KMS FB, GLSL,
      gl driver, not stuck); EXIT → RetroArch closed + UI restored + chain
      SIGCONT'd; service stayed armed.
- [x] SSH gotcha: never `pkill -f arcade_supervisor.py` from an SSH one-liner —
      the pattern matches the remote shell's own cmdline and kills the session
      (exit 255). Use `systemctl stop` instead.

## Fixed 2026-06-15 (live debugging on the Pi)
- [x] **Black screen on combo** → root cause: the old whole-cgroup
      `systemctl kill --signal=SIGSTOP demiurge.service` froze the launcher (which
      manages the PipeWire graph), so RetroArch's audio init blocked forever.
      Fix: **stage-level freeze** — `kill -STOP` only the chain DSP programs,
      leaving the launcher + pipewire/wireplumber alive. Also sudo-free now
      (demiurge.service runs as gorpi7). RetroArch renders the menu again.
- [x] Game audio in TESTING_GROUND falls back to "continue without audio" (no
      real output without the Scarlett) — expected; the display/menu works. Real
      game audio to be verified with the Scarlett / a real patch.
- [x] `PIPEWIRE_NODE=demiurge-sink` REMOVED from the RetroArch launch (it pointed
      at the frozen sink). Audio driver switched pipewire → `alsathread`/default.

## Known limitation (by design, for now)
- The **UI is stop/restart, not a true freeze** — kivy holds DRM master via a
  direct card1 open with no VT/seat, so a frozen UI keeps the screen. Audio
  freezes for real; the UI's exact face is lost across the restart (menu/effect
  state rebuilds from OSC). Upgrade path: UI cooperatively `drmDropMaster` on a
  signal → then it could be SIGSTOP-paused too. See ARCHITECTURE.md.

## Open design items (do after first working run)
- [ ] Auto-flip `sync_layer` on/off around console enter/exit (so it works even
      from bypass mode). Today it assumes `sync_layer = on`.
- [ ] Continue-on-death auto-pulse loop (coin/start) — `gamepad.pulse()` exists,
      the trigger loop is not wired.
- [ ] Joystick Y polarity — confirm up=up on the cabinet; flip in gamepad.py if not.
- [ ] Per-game free-play DIPs documented per ROM.
- [ ] Boot service (`demiurge-arcade.service`) — drafted in INSTALL.md, not installed.

## Integration (only when proven)
- [ ] Fold `games.conf` keys into `live.conf`; add CLI wrappers (e.g. `demgame`).
- [ ] Replace the `serial_bridge.py` service with the supervisor cleanly.
- [ ] Add games-install steps to the main setup-script + a line in the main docs.
