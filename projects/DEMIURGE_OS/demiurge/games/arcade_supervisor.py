#!/usr/bin/env python3
"""
arcade_supervisor.py — the always-on Demiurge arcade brain.

ONE long-lived process that owns the Teensy serial port (/dev/ttyACM0) and does
three jobs at once:

  1. SYNTH mode: watches for per-game launch CODES (Konami-style button
     sequences, e.g. ARCADE,Y,ARCADE,X,ARCADE,A,ARCADE = Galaga) and launches
     that game on a match. Also re-emits the Teensy CCs exactly as
     serial_bridge.py would, so csound / the synth keep working unchanged.
  2. CONSOLE mode: feeds the joystick + buttons to a virtual gamepad that
     RetroArch reads as player 1. Hold the joystick click (jb) ~1.5s to exit the
     game back to synth (jb is reserved — never a gameplay control).
  3. UI SAFETY NET (synth mode): if the face UI is DOWN (crashed, lost the boot
     DRM race, bare Linux terminal), hold jb ~1.5s to REVIVE it — ui-restore
     (re)starts neptr-ui and re-takes the panel. Gated on the UI being dead, so
     a live UI keeps owning the jb-hold (its companion menu) — no conflict. So
     the joystick always gets you back to the panel / demiurge control.

On entering the console it FREEZES the whole Demiurge audio chain with SIGSTOP
(via bin/demiurge-freeze) — every audio language pauses mid-sample, exactly as it
was, regardless of language. The sync layer (PipeWire) keeps running, so RetroArch
plays out the same interface as a Demiurge node. Toggling back THAWS (SIGCONT) and
audio resumes precisely where it stopped.

Run it as the user (needs: dialout for serial, input/uinput for the gamepad, and
passwordless `systemctl kill` for freeze/thaw — see docs/INSTALL.md). It REPLACES
serial_bridge.py as the serial owner (only one process can read the port); in
synth mode it is a behavioural superset of that bridge.

    python3 arcade_supervisor.py [games.conf]
"""

from __future__ import annotations

import os
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lib.combo import ComboDetector            # noqa: E402
from lib.config import Config                   # noqa: E402
from lib.sequence import SequenceDetector       # noqa: E402
from lib.teensy import SerialReader, parse_frame  # noqa: E402

HERE = Path(__file__).resolve().parent
BIN = HERE / "bin"

SYNTH = "synth"
CONSOLE = "console"


def log(msg: str) -> None:
    print(f"[arcade] {msg}", file=sys.stderr, flush=True)


class Supervisor:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.mode = SYNTH
        # SYNTH mode: per-game launch CODES (Konami-style button sequences).
        self.games_by_label = {g["label"]: g for g in cfg.games()}
        self.sequence = SequenceDetector(
            {g["label"]: g["code"] for g in cfg.games()},
            gap_seconds=cfg.code_gap_seconds,
            logger=lambda tok, buf: log(f"code press: {tok}  buffer={buf}"),
        )
        # CONSOLE mode: hold the joystick click (jb) to exit a game -> synth.
        # jb is reserved (not a gameplay control), so it never false-fires.
        self.exit_combo = ComboDetector(cfg.exit_hold_seconds)
        # SYNTH-mode UI safety net: hold jb while the face UI is DOWN to revive
        # it, so the joystick always brings the panel / demiurge control back
        # from ANY state (crashed UI, bare Linux terminal). The live UI owns the
        # jb-hold (its companion menu), so this only fires when the UI is dead —
        # gated on `_ui_active()` below, so there's never a conflict.
        self.revive_combo = ComboDetector(cfg.exit_hold_seconds)
        self.current_game = None   # the {label,rom,code} currently launched
        self._ff_until = 0.0       # hold fast-forward (boot-skip) until this time
        # Games without an infinite-lives cheat get a one-time coin burst on launch.
        self._autocoin_games = set(str(cfg.get("autocoin_games", "")).split())
        self._coins_left = 0
        self._next_coin_t = 0.0
        self.reader = SerialReader(str(cfg["serial_port"]))

        # Lazily-created, mode-specific I/O.
        self.midi = None      # MidiEmitter (synth mode)
        self.pad = None       # VirtualGamepad (console mode)
        self.retro = None     # RetroArch subprocess (console mode)

        self._stop = False
        signal.signal(signal.SIGTERM, self._on_signal)
        signal.signal(signal.SIGINT, self._on_signal)

    def _on_signal(self, *_):
        self._stop = True

    # ── synth mode plumbing ────────────────────────────────────────────────
    def _ensure_midi(self):
        if self.midi is None:
            from lib.teensy import MidiEmitter
            self.midi = MidiEmitter()

    # ── freeze / thaw ──────────────────────────────────────────────────────
    def _freeze(self):
        if not self.cfg["freeze_synth"]:
            log("freeze_synth=off — leaving the chain running")
            return
        log("freezing Demiurge chain (SIGSTOP)")
        self._run_helper("demiurge-freeze")

    def _thaw(self):
        if not self.cfg["freeze_synth"]:
            return
        log("thawing Demiurge chain (SIGCONT)")
        self._run_helper("demiurge-thaw")

    def _run_helper(self, name: str):
        path = BIN / name
        try:
            subprocess.run([str(path)], check=False, timeout=10)
        except Exception as e:  # never let a freeze hiccup kill the supervisor
            log(f"{name} failed: {e}")

    def _schedule_coins(self):
        """Queue a coin burst if the current game has no infinite-lives cheat."""
        label = self.current_game["label"] if self.current_game else ""
        if label in self._autocoin_games:
            self._coins_left = int(self.cfg["autocoin_burst"])
            self._next_coin_t = (time.monotonic()
                                 + int(self.cfg["autocoin_delay_ms"]) / 1000.0)
            log(f"coin burst queued for {label}: {self._coins_left}")
        else:
            self._coins_left = 0

    def _ui_active(self) -> bool:
        """True if neptr-ui.service is running. Gates the synth-mode revive so
        the supervisor never fights a live UI for the panel."""
        try:
            r = subprocess.run(["systemctl", "is-active", "neptr-ui"],
                               capture_output=True, text=True, timeout=3)
            return r.stdout.strip() == "active"
        except Exception:
            return True   # on error, assume up — don't fight a possibly-live UI

    # ── RetroArch ──────────────────────────────────────────────────────────
    def _launch_retroarch(self):
        cfg_path = HERE / "retroarch" / "retroarch.cfg"
        cmd = ["retroarch", "--config", str(cfg_path)]
        default_core = str(self.cfg["default_core"]).strip()
        # Which ROM/core: the game chosen by its launch code, else the
        # default_game fallback. Boots straight in — no menu.
        if self.current_game:
            rom = self.current_game["rom"]
            core = self.current_game.get("core") or default_core
        else:
            rom = str(self.cfg["default_game"]).strip()
            core = default_core
        if rom and core:
            cmd += ["-L", core, str((HERE / "roms" / rom))]
        elif rom:
            cmd.append(str((HERE / "roms" / rom)))
        # Per-game control override (layered on top of the main --config) so each
        # game can have a tailored layout/labels. Loads reliably via --appendconfig
        # (RetroArch's own per-game override dir doesn't resolve under our --config).
        if self.current_game:
            pg = HERE / "retroarch" / "per-game" / f"{self.current_game['label']}.cfg"
            if pg.exists():
                cmd += ["--appendconfig", str(pg)]
        log(f"launching: {' '.join(cmd)}")
        env = dict(os.environ)
        env.setdefault("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
        # Force the framebuffer (KMSDRM) path: the face UI is SDL2-KMSDRM with no
        # X/Wayland, so RetroArch must take the DRM device directly. Stripping
        # DISPLAY/WAYLAND_DISPLAY makes RetroArch pick its KMS context. The UI is
        # frozen (SIGSTOP) by now, so the panel is free.
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        # Game audio goes to the DEFAULT PipeWire sink. Do NOT target
        # demiurge-sink: the freeze SIGSTOPs the whole demiurge.service cgroup,
        # which includes the demiurge-sink loopback, so a stream pointed at it
        # blocks forever (RetroArch hangs at 0% CPU on a black screen — exactly
        # this bug). Routing game audio THROUGH demiurge-sink for DSP needs a
        # stage-level freeze (SIGSTOP only the chain programs, leaving the sink
        # loopback alive) — see docs/STATUS.md. For now: default sink.
        env.pop("PIPEWIRE_NODE", None)
        try:
            self.retro = subprocess.Popen(cmd, env=env)
        except FileNotFoundError:
            log("retroarch not found — install it (see docs/INSTALL.md)")
            self.retro = None

    def _kill_retroarch(self):
        # Close RetroArch CLEANLY so it drops DRM master / closes its
        # /dev/dri/card1 fd before ui-restore tries to grab KMSDRM. SIGTERM lets
        # RetroArch run its shutdown (save state, drmDropMaster, close fd); a
        # hard SIGKILL leaves the kernel to reap the fd asynchronously, which is
        # the worst case for the reclaim race. So give SIGTERM a generous window
        # (RetroArch can take a few seconds to flush saves) and only escalate to
        # SIGKILL as a last resort.
        if self.retro and self.retro.poll() is None:
            log("closing RetroArch (SIGTERM)")
            self.retro.terminate()
            try:
                self.retro.wait(timeout=10)
            except subprocess.TimeoutExpired:
                log("RetroArch did not exit on SIGTERM — escalating to SIGKILL")
                self.retro.kill()
                try:
                    self.retro.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    log("RetroArch still not reaped after SIGKILL")
        self.retro = None
        # Belt-and-suspenders: a stray detached RetroArch (or one we lost the
        # handle to) would still hold the panel. ui-restore polls for this too,
        # but reap any leftover here so the poll converges fast.
        try:
            subprocess.run(["pkill", "-TERM", "-x", "retroarch"],
                           check=False, timeout=5)
        except Exception:
            pass

    # ── mode toggle ────────────────────────────────────────────────────────
    def _enter_console(self, game=None):
        self.current_game = game
        label = game["label"] if game else "default"
        log(f"=== entering CONSOLE ({label}) ===")
        # Require a FRESH jb-hold to exit (arm only after a release).
        self.exit_combo.reset()
        self.revive_combo.reset()
        # 1) release the screen: STOP the UI (it holds DRM master; can't be frozen
        #    off the panel). 2) freeze the audio chain in place (SIGSTOP).
        self._run_helper("ui-release")
        self._freeze()
        from lib.gamepad import VirtualGamepad
        try:
            self.pad = VirtualGamepad(deadzone=int(self.cfg["joy_deadzone"]),
                                      rotation=int(self.cfg["joy_rotation"]),
                                      invert_x=bool(self.cfg["joy_invert_x"]),
                                      invert_y=bool(self.cfg["joy_invert_y"]),
                                      game=(game["label"] if game else ""))
        except Exception as e:
            log(f"could not create virtual gamepad: {e} — aborting console")
            self._thaw()
            self._run_helper("ui-restore")
            return
        self._launch_retroarch()
        self._ff_until = time.monotonic() + int(self.cfg["boot_skip_ms"]) / 1000.0
        self._schedule_coins()
        self.mode = CONSOLE

    def _exit_console(self):
        log("=== leaving CONSOLE ===")
        # reverse order: close RetroArch (frees the CRTC), restore the UI (re-takes
        # the panel), then thaw the audio chain (resumes on the exact sample).
        self._kill_retroarch()
        if self.pad:
            self.pad.close()
            self.pad = None
        self._run_helper("ui-restore")
        self._thaw()
        self.current_game = None
        self._ff_until = 0.0
        self._coins_left = 0
        # Drop any stray presses accumulated during the game so the next launch
        # code starts clean (the detector keeps listening in synth too).
        self.sequence.reset()
        # Fresh jb-hold required before the synth revive can fire (the player was
        # just holding jb to exit — don't let that roll straight into a revive).
        self.revive_combo.reset()
        self.mode = SYNTH

    def _switch_game(self, game):
        """Switch to another game WITHOUT leaving the console — UI stays down,
        audio stays frozen; just swap the running RetroArch. Same game = no-op."""
        if not game:
            return
        if self.current_game and game["label"] == self.current_game["label"]:
            return  # already playing it
        log(f"=== switching game -> {game['label']} ===")
        self._kill_retroarch()
        time.sleep(0.5)              # let the old RetroArch drop DRM master
        self.current_game = game
        if self.pad and game:
            self.pad.set_game(game["label"])   # switch to the new game's control profile
        self._launch_retroarch()
        self._ff_until = time.monotonic() + int(self.cfg["boot_skip_ms"]) / 1000.0
        self._schedule_coins()
        self.exit_combo.reset()     # fresh jb-hold arming for the new game

    # ── main loop ──────────────────────────────────────────────────────────
    def run(self):
        if not self.cfg["enabled"]:
            log("enabled=off in games.conf — supervisor idle (synth only)")
        log(f"up. serial={self.cfg['serial_port']} mode={self.mode} "
            f"games={list(self.games_by_label)} "
            f"exit_hold={self.cfg.exit_hold_seconds}s")
        # NOTE: the supervisor does NOT start the face UI. The UI is owned by the
        # demiurge UI loader (neptr-ui.service via demiurge-ui-launch.sh), which
        # launches it on boot, config-driven from live.conf. The old startup
        # `ui-restore` workaround here was removed once neptr-ui.service was made
        # boot-reliable on its own (DRM-wait ExecStartPre + no self-ordering
        # cycle + no one-shot DRM condition). The combo flow still owns the UI at
        # RUNTIME: _enter_console stops it for a game, _exit_console restarts it.
        for line in self.reader.lines():
            if self._stop:
                break
            frame = parse_frame(line)
            if frame is None:
                continue

            now = time.monotonic()
            # Launch CODES are ALWAYS listening (same for every game). From synth
            # a code OPENS its game; from INSIDE a game a code SWITCHES to that
            # game without leaving the console (chiller-style).
            label = (self.sequence.update(frame, now)
                     if self.cfg["enabled"] else None)
            if label:
                game = self.games_by_label.get(label)
                if self.mode == SYNTH:
                    self._enter_console(game)
                else:
                    self._switch_game(game)
                continue

            if self.mode == SYNTH:
                self._ensure_midi()
                self.midi.emit(frame)
                # UI safety net: hold jb ~exit_hold_seconds while the face UI is
                # DOWN to revive it — ui-restore (re)starts neptr-ui and re-takes
                # the panel over a crashed UI or bare terminal. One-shot (combo
                # requires a release before re-firing) and gated on the UI being
                # dead, so it never conflicts with the live UI's own jb-hold
                # companion menu. "Hold the joystick -> get the panel back."
                if self.revive_combo.update(bool(frame.get("jb", 0)), now):
                    if not self._ui_active():
                        log("jb-hold in synth + UI down -> reviving UI (ui-restore)")
                        self._run_helper("ui-restore")
            else:
                # Boot-skip: hold fast-forward for the first seconds so the game
                # reaches its start screen fast, then back to normal speed.
                if self.pad:
                    self.pad.set_ff(now < self._ff_until)
                # Coin burst (no-cheat games): drip the queued coins in once the
                # game is up = tons of credits, continue forever without coin-dealing.
                if self._coins_left > 0 and self.pad and now >= self._next_coin_t:
                    self.pad.insert_coin()
                    self._coins_left -= 1
                    self._next_coin_t = now + 0.1
                # In a game: hold the joystick click (jb) ~1.5s to exit -> synth.
                # (jb isn't a gameplay control, so this never trips during play.)
                if self.exit_combo.update(bool(frame.get("jb", 0)), now):
                    self._exit_console()
                    continue
                # If RetroArch exited on its own, fall back to synth.
                if self.retro is not None and self.retro.poll() is not None:
                    log("RetroArch exited on its own — returning to synth")
                    self._exit_console()
                    continue
                if self.pad:
                    self.pad.update(frame)

        self._shutdown()

    def _shutdown(self):
        log("shutting down")
        if self.mode == CONSOLE:
            self._exit_console()
        if self.midi:
            self.midi.close()


def main() -> int:
    cfg_path = sys.argv[1] if len(sys.argv) > 1 else None
    cfg = Config.load(cfg_path)
    Supervisor(cfg).run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
