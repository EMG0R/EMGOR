"""MIDI OUT for the neuralGrid live layer, over python-rtmidi.

Opens an rtmidi output stream named "neuralGrid". If a destination port
whose name contains "Midi Through" is already visible on the system (true
on the Pi, where ALSA's "Midi Through" (14:0) is always present), we
auto-connect straight into it -- that's the simplest way to get our notes
onto the same ALSA graph "demiurge_clock" and other synths already live on,
without requiring a human to run `aconnect` by hand. If no such port
exists we fall back to opening our own virtual port named "neuralGrid" so
other software (or `aconnect`) can find and patch into us later.

python-rtmidi (or a usable MIDI backend/port) may simply not be present --
on the Mac dev machine, for instance. In that case every method becomes a
silent no-op after a single warning log line at construction time, so the
rest of the live layer (grid, menus, sequencer) runs completely normally
with MIDI simply not going anywhere.
"""

from __future__ import annotations

import logging
from typing import Optional, Set, Tuple

log = logging.getLogger("neuralgrid.midiout")

CC_ALL_NOTES_OFF = 123
CC_ALL_SOUND_OFF = 120


class MidiOut:
    def __init__(self, port_name: str = "neuralGrid", auto_connect_substring: str = "Midi Through"):
        self.port_name = port_name
        self._midiout = None
        self._available = False
        # Notes we believe are currently sounding, so all_notes_off() can
        # send explicit note-offs as well as the CC (some synths ignore
        # CC 123 in poly mode).
        self._active_notes: Set[Tuple[int, int]] = set()

        try:
            import rtmidi  # type: ignore
        except ImportError:
            log.warning(
                "python-rtmidi not installed -- MIDI OUT disabled, grid/menus still run"
            )
            return

        try:
            midiout = rtmidi.MidiOut()
            connected = False

            if auto_connect_substring:
                ports = midiout.get_ports()
                for idx, name in enumerate(ports):
                    if auto_connect_substring.lower() in name.lower():
                        midiout.open_port(idx)
                        connected = True
                        log.info("MIDI OUT auto-connected to existing port %r", name)
                        break

            if not connected:
                try:
                    midiout.open_virtual_port(port_name)
                    connected = True
                    log.info("MIDI OUT opened virtual port %r", port_name)
                except (NotImplementedError, RuntimeError):
                    # Virtual ports aren't supported on this backend (e.g.
                    # Windows/MMSystem). Fall back to the first real port.
                    ports = midiout.get_ports()
                    if ports:
                        midiout.open_port(0)
                        connected = True
                        log.info("MIDI OUT opened first available port %r", ports[0])

            if connected:
                self._midiout = midiout
                self._available = True
            else:
                log.warning("MIDI OUT: no usable port -- disabled, grid/menus still run")
        except Exception as exc:  # noqa: BLE001 - never let MIDI setup crash the app
            log.warning("MIDI OUT init failed (%s) -- disabled, grid/menus still run", exc)
            self._midiout = None
            self._available = False

    @property
    def available(self) -> bool:
        return self._available

    def _send(self, message) -> None:
        if not self._available or self._midiout is None:
            return
        try:
            self._midiout.send_message(message)
        except Exception as exc:  # noqa: BLE001
            log.warning("MIDI OUT send failed (%s)", exc)

    def note_on(self, note: int, vel: int = 100, ch: int = 0) -> None:
        note = max(0, min(127, int(note)))
        vel = max(0, min(127, int(vel)))
        ch = max(0, min(15, int(ch)))
        self._active_notes.add((note, ch))
        self._send([0x90 | ch, note, vel])

    def note_off(self, note: int, ch: int = 0) -> None:
        note = max(0, min(127, int(note)))
        ch = max(0, min(15, int(ch)))
        self._active_notes.discard((note, ch))
        self._send([0x80 | ch, note, 0])

    def cc(self, controller: int, value: int, ch: int = 0) -> None:
        """Send a Control Change message. Additive method (neuralGrid MIDI
        mode: softpots/lidar -> CC). No-op when MIDI OUT is unavailable,
        same as every other method here."""
        controller = max(0, min(127, int(controller)))
        value = max(0, min(127, int(value)))
        ch = max(0, min(15, int(ch)))
        self._send([0xB0 | ch, controller, value])

    def channel_aftertouch(self, value: int, ch: int = 0) -> None:
        """Send Channel Pressure (aftertouch). Additive method (neuralGrid
        MIDI mode: held FSR pressure -> channel aftertouch, off by default).
        No-op when MIDI OUT is unavailable, same as every other method here."""
        value = max(0, min(127, int(value)))
        ch = max(0, min(15, int(ch)))
        self._send([0xD0 | ch, value])

    def list_ports(self) -> list:
        """List available MIDI output port names, for the UI's port picker
        (neuralGrid MIDI mode task). Additive, read-only: probes a throwaway
        rtmidi.MidiOut() so it never disturbs the connection already open on
        ``self``. Returns [] if python-rtmidi isn't installed."""
        try:
            import rtmidi  # type: ignore
        except ImportError:
            return []
        try:
            probe = rtmidi.MidiOut()
            return list(probe.get_ports())
        except Exception:  # noqa: BLE001
            return []

    def select_port(self, name: str) -> bool:
        """Reopen the output on a specific named port (additive; used by the
        MIDI mode panel's port picker). Closes whatever was open on
        ``self`` first. Returns False (no-op) when python-rtmidi isn't
        installed or ``name`` isn't among the currently visible ports."""
        try:
            import rtmidi  # type: ignore
        except ImportError:
            return False
        try:
            new = rtmidi.MidiOut()
            ports = new.get_ports()
            for idx, pname in enumerate(ports):
                if pname == name:
                    new.open_port(idx)
                    if self._midiout is not None:
                        try:
                            self._midiout.close_port()
                        except Exception:  # noqa: BLE001
                            pass
                    self._midiout = new
                    self._available = True
                    self.port_name = name
                    return True
            return False
        except Exception as exc:  # noqa: BLE001
            log.warning("MIDI OUT select_port failed (%s)", exc)
            return False

    def all_notes_off(self) -> None:
        # Explicit note-offs for anything we think is sounding...
        for note, ch in list(self._active_notes):
            self._send([0x80 | ch, note, 0])
        self._active_notes.clear()
        # ...plus the channel-mode CCs on every channel, as a backstop.
        for ch in range(16):
            self._send([0xB0 | ch, CC_ALL_NOTES_OFF, 0])
            self._send([0xB0 | ch, CC_ALL_SOUND_OFF, 0])

    def close(self) -> None:
        if self._midiout is not None:
            try:
                self.all_notes_off()
                self._midiout.close_port()
            except Exception:  # noqa: BLE001
                pass
