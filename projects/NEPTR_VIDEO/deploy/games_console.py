#!/usr/bin/env python3
"""
games_console.py — HOME's front door into the EXISTING arcade console.

This is NOT a second arcade. It imports `arcade_supervisor.Supervisor` out of
~/demiurge/games and runs that exact object: the same virtual pad
(lib/gamepad.py), the same RetroArch launch, the same stage-level audio freeze,
the same jb-hold exit gesture, the same launch codes. Nothing about controls,
ROMs, cores or button roles is decided here, and no Teensy byte is parsed here.

WHY A DRIVER AT ALL, INSTEAD OF JUST RUNNING THE SUPERVISOR

The supervisor is a DAEMON. It boots into SYNTH mode and enters the console
only when the player types a game's launch CODE on the footswitches. HOME has
already made that decision -- the player selected GAMES on a menu -- so what is
needed is a way to say "start IN the console", which the daemon's command line
does not have. That is the whole of the first override below.

The second override is the one that matters for not hanging the rig. The
supervisor's console entry/exit calls `ui-release` / `ui-restore`, which STOP
and START neptr-ui.service. That is right when the supervisor is the thing that
owns the screen handoff. Under HOME it is actively wrong: HOME dropped DRM
master before running this and takes it back when this exits, so a `ui-restore`
on the way out starts the audio UI into a display HOME is about to reclaim --
two KMS clients racing for one master, which is precisely the state the rig
froze in. So under HOME the UI handoff is suppressed: ONE owner of the display,
and it is HOME.

THE THIRD OVERRIDE: WHERE THE CONTROLS COME FROM

The supervisor reads ONE input, a Teensy on a serial port. A rig with no board
attached therefore has no pad and, worse, no exit gesture -- which is a mode
that cannot be left, the one thing mode-games.sh exists to prevent. But the
board is not the control layer. DEMIURGE already HAS a control layer:
demiurge-io conditions every input to a float 0..1 and fans it out as
`/sensor <id> <norm>`, and the web UI's virtual panel feeds that same layer.
Real footswitch and on-screen button are the same event by the time they reach
it.

So with `NV_GAMES_CONTROL=bus` the supervisor's `reader` is swapped for
`control_bus.BusReader`, which listens on that bus and hands the supervisor
frames. That is the WHOLE integration: the reader is built in the supervisor's
`__init__` and only used in `run()`, so replacing the attribute needs no edit
to arcade_supervisor.py and changes nothing else about it. Button roles, the
per-game pad profiles, freeze/thaw, launch codes and the jb-hold exit are all
still the supervisor's, unmodified.

It is NOT a fake Teensy. Nothing here opens a pseudo-terminal, creates a device
node, or synthesises a serial byte -- an earlier attempt did propose exactly
that and it was rightly rejected. This subscribes to a bus.

    python3 games_console.py [games-dir]

Exits 0 when the player leaves the console (jb-hold, or RetroArch quitting on
its own). Never returns to synth mode: under HOME, leaving the console means
going back to the menu, and the menu is another process.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

# The arcade lives in DEMIURGE_OS, deployed to ~/demiurge/games. It is read,
# never modified, and never copied: a copy is the parallel system this stack
# forbids, and it would drift the moment a button role changed.
DEFAULT_GAMES_DIR = Path(os.environ.get("HOME", "/home/gorpi7")) / "demiurge" / "games"


def log(msg: str) -> None:
    print(f"[mode-games] {msg}", file=sys.stderr, flush=True)


def _control_source() -> str:
    """`bus` (the DEMIURGE control layer) or `serial` (a Teensy on a tty).

    Defaults to AUTO: the bus if there is no serial port to read, serial if
    there is. Auto rather than a flag because the honest default is "use the
    hardware when it is there", and because the failure this removes -- GAMES
    refusing to start on a rig with no board -- should not need a config edit
    to fix on a machine that has never had one.

    `NV_GAMES_CONTROL=bus|serial` forces it either way, which is what makes
    the bus path testable on a rig that DOES have a Teensy plugged in.
    """
    want = os.environ.get("NV_GAMES_CONTROL", "auto").strip().lower()
    if want in ("bus", "serial"):
        return want
    port = os.environ.get("NV_GAMES_SERIAL") or "/dev/ttyACM0"
    return "serial" if Path(port).exists() else "bus"


def _bus_port() -> int:
    # NOT 8002 and NOT 8004. 8002 is demiurge_web's telemetry socket and 8004
    # is the video engine's, and both are single-owner -- there is no
    # SO_REUSEPORT anywhere on this bus, deliberately, because Linux
    # load-balances rather than duplicates and the result is two consumers
    # that each silently receive half. A consumer gets its own destination.
    return int(os.environ.get("NV_GAMES_OSC_PORT", "8005"))


def build(games_dir: Path):
    """Import the real supervisor and subclass it. Returns (supervisor, cfg)."""
    # The supervisor does `sys.path.insert(0, <its own dir>)` at import time so
    # its `lib.*` imports resolve; we only have to make the module itself
    # importable, and put it FIRST so a stray same-named module cannot win.
    sys.path.insert(0, str(games_dir))
    from arcade_supervisor import Supervisor  # noqa: E402
    from lib.config import Config             # noqa: E402

    cfg = Config.load(str(games_dir / "games.conf"))

    # Point the arcade at a different serial port without editing games.conf.
    # This exists so the console path can be exercised off a pseudo-terminal
    # fed with recorded NEPTR2 frames -- pad creation, RetroArch, the jb-hold
    # exit and HOME reclaiming the display are all testable that way, and none
    # of them should have to wait on a physical stick being plugged in. The
    # arcade reads this key once, in its own __init__, so setting it here is
    # the whole of the override.
    port_override = os.environ.get("NV_GAMES_SERIAL")
    if port_override:
        cfg._raw["serial_port"] = port_override
        log(f"serial port overridden -> {port_override}")

    class HomeConsole(Supervisor):
        """The supervisor, entered at the console and answering to HOME."""

        def __init__(self, cfg):
            super().__init__(cfg)
            # Swap the input, and nothing else. See the module header: the
            # supervisor builds `reader` here and only calls `.lines()` in
            # run(), and SerialReader does not open its port until then --
            # so on the bus path no serial device is touched at all.
            if _control_source() == "bus":
                from control_bus import BusReader
                self.reader = BusReader(port=_bus_port())
                log(f"controls: DEMIURGE bus on :{_bus_port()} "
                    f"(virtual panel and/or Teensy via demiurge-io)")
            else:
                log(f"controls: serial {cfg['serial_port']}")

        # HOME owns the display. See the module header: letting the supervisor
        # start neptr-ui here is the two-masters race that froze the rig.
        def _run_helper(self, name: str):
            if name in ("ui-release", "ui-restore"):
                log(f"skipping {name} -- HOME owns the display handoff")
                return
            super()._run_helper(name)

        # Leaving the console is the END of GAMES mode, not a return to synth:
        # the thing the player goes back to is HOME, which is waiting on this
        # process to exit. `_stop` is the supervisor's own shutdown flag, so
        # this uses its existing exit path rather than bypassing it.
        def _exit_console(self):
            super()._exit_console()
            self._stop = True

    return HomeConsole(cfg), cfg


def main(argv: list[str]) -> int:
    games_dir = Path(argv[1]) if len(argv) > 1 else DEFAULT_GAMES_DIR
    if not (games_dir / "arcade_supervisor.py").is_file():
        log(f"no arcade_supervisor.py under {games_dir}")
        return 2

    sup, _cfg = build(games_dir)

    # Straight into the console with no game argument, which is exactly what
    # the supervisor already means by "no launch code": boot games.conf's
    # `default_game` on `default_core`. Which ROM GAMES opens is therefore
    # still a games.conf decision, not one made here.
    sup._enter_console(None)

    # A failed pad or a missing RetroArch makes _enter_console bail back to
    # SYNTH rather than raise. Running the serial loop then would block on the
    # Teensy forever with nothing on screen -- the hang HOME must never enter.
    if sup.mode != "console":
        log("supervisor refused the console (see its log above) -- returning to HOME")
        return 3

    # The supervisor's own loop from here: pad updates, jb-hold exit, coins,
    # boot-skip, and the RetroArch-died check. It returns when `_stop` is set,
    # which the override above does on leaving the console.
    sup.run()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
