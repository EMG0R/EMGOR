#!/usr/bin/env python3
"""
Pocket OpGorator host monitor.

Two independent jobs, run continuously and gracefully whether or not the
hardware is plugged in:
  1. Visualize the device's MIDI clock as pulses around a ring.
  2. Route the device's USB audio input to the Mac's speakers at low
     latency.

See README.md for measured latency, drift policy, and duplicate-tick
handling rationale.
"""
import argparse
import os
import sys
import time

import pygame

from state import SharedState
from audio_engine import AudioEngine
from midi_engine import MidiEngine
from cdc_engine import CdcEngine
import ui

DFU_CONFIRM_WINDOW_S = 3.0  # must press the DFU key twice within this window


def parse_args():
    p = argparse.ArgumentParser(description="Pocket OpGorator monitor")
    p.add_argument("--blocksize", type=int, default=64,
                    help="Audio callback block size in frames (default: 64)")
    p.add_argument("--latency", default="low", choices=["low", "high"],
                    help="PortAudio latency hint passed to both streams (default: low)")
    p.add_argument("--target-fill-ms", type=float, default=8.0,
                    help="Target ring-buffer fill level in ms; lower = less latency, "
                         "less headroom against jitter (default: 8.0)")
    p.add_argument("--input-device", default=None,
                    help="Override input device (name substring or index). "
                         "For testing without hardware, e.g. 'Scarlett' or 'MacBook Pro Microphone'.")
    p.add_argument("--output-device", default=None,
                    help="Override output device (name substring or index). Default: system default output.")
    p.add_argument("--aggregate", default=None,
                    help="Name substring or index of a macOS Aggregate Device to use for a single "
                         "low-latency duplex stream (no ring buffer, no resampling). Without this "
                         "flag, auto-detection looks for any device with both input and output "
                         "channels whose name contains 'opgorator'. Use this to force a specific "
                         "device, or to test the duplex path against ordinary duplex hardware, "
                         "e.g. --aggregate \"Scarlett 2i2 USB\".")
    p.add_argument("--no-aggregate-autocreate", action="store_true",
                    help="Disable automatic creation of a private macOS Aggregate Device. By "
                         "default the app creates one itself on startup (combining the PMOR's "
                         "input with the chosen output, no Audio MIDI Setup needed) whenever "
                         "none already exists. Pass this to force the dual-stream fallback "
                         "path instead, e.g. to measure/compare it.")
    p.add_argument("--remove-aggregate", action="store_true",
                    help="Destroy any lingering PMOR aggregate device (matched by our fixed "
                         "UID) and exit. Aggregates this app creates are private and "
                         "non-persistent -- they vanish on their own when the app quits or "
                         "crashes -- so this is only needed to force a clean slate, e.g. after "
                         "killing the app with SIGKILL. See aggregate_device.py.")
    p.add_argument("--create-aggregate", action="store_true",
                    help="Print step-by-step manual instructions for creating a macOS Aggregate "
                         "Device for the Pocket OpGorator, then exit. Not needed for normal use "
                         "any more -- the app creates one automatically on startup (see "
                         "--no-aggregate-autocreate) -- this remains for manual/diagnostic use, "
                         "e.g. building a *persistent* aggregate that survives outside this app.")
    p.add_argument("--midi-port", default=None,
                    help="Override MIDI port name substring to match (default: 'Pocket OpGorator'/'PMOR')")
    p.add_argument("--cdc-port", default=None,
                    help="Override the CDC serial console device path (e.g. /dev/cu.usbmodem14440001231 "
                         "or a pty path for testing). Default: auto-resolved via ioreg by USB product "
                         "name match.")
    p.add_argument("--fps", type=int, default=60, help="UI target frame rate (default: 60)")
    p.add_argument("--no-audio", action="store_true", help="Disable the audio engine entirely (MIDI viz only)")
    p.add_argument("--no-midi", action="store_true", help="Disable the MIDI engine entirely (audio monitor only)")
    p.add_argument("--no-cdc", action="store_true", help="Disable the CDC control console entirely")
    p.add_argument("--list-devices", action="store_true", help="Print audio devices and MIDI ports, then exit")
    return p.parse_args()


def list_devices():
    import sounddevice as sd
    import rtmidi
    print("=== Audio devices (sounddevice / PortAudio) ===")
    print(sd.query_devices())
    print()
    print("=== MIDI input ports (python-rtmidi) ===")
    m = rtmidi.MidiIn()
    ports = m.get_ports()
    if not ports:
        print("  (none)")
    for i, name in enumerate(ports):
        print(f"  {i}: {name}")
    m.close_port()


def print_create_aggregate_instructions():
    print("""
NOTE: you almost certainly don't need this. As of this version, the app
creates a private macOS Aggregate Device for you automatically on startup
(see aggregate_device.py) -- no Audio MIDI Setup, no manual steps. It's
private and non-persistent: invisible to Audio MIDI Setup and every other
app, and cleaned up automatically when this app quits (even on a crash),
so it can never leave a stray device behind or disturb Loopback/NDI/other
virtual audio routing. Pass --no-aggregate-autocreate to disable this and
fall back to the old dual-stream (ring buffer + resampler) path, and
--remove-aggregate to force-clean a lingering one (rarely needed).

The manual steps below are kept only for building a *persistent* aggregate
that survives outside this app (e.g. for use by a different program too),
which the automatic private one deliberately does not do.

How to create it by hand (takes about a minute):

  1. Open Audio MIDI Setup (Applications > Utilities, or Spotlight).
  2. Click the "+" button in the bottom-left corner, choose
     "Create Aggregate Device".
  3. In the device list on the right, tick the checkbox for
     "Pocket OpGorator Audio" AND the checkbox for the output you want to
     monitor through (e.g. "MacBook Pro Speakers").
  4. Click the "Clock Source" / master column and set the OUTPUT device
     (e.g. MacBook Pro Speakers) as the clock master -- click the radio
     button in the "Master Device" column next to it.
  5. Tick "Drift Correction" for the Pocket OpGorator row (the
     non-master device). Drift correction goes on the non-master device
     because the master's clock is the timing reference everything else
     is measured against -- correcting the master against itself is
     meaningless.
  6. Rename the aggregate to something containing "OpGorator" (e.g.
     "OpGorator Aggregate") so this tool's auto-detection finds it, or
     just pass its exact name with --aggregate "Your Aggregate Name".
  7. Run this tool again -- it will report "AGGREGATE MODE" and a real
     measured latency instead of "DUAL-STREAM FALLBACK".

See README.md "Aggregate device mode" for more detail.
""")


def main():
    args = parse_args()

    if args.create_aggregate:
        print_create_aggregate_instructions()
        return 0

    if args.remove_aggregate:
        import aggregate_device as agg
        ok, msg = agg.destroy_aggregate_by_uid()
        print(f"[remove-aggregate] {'ok' if ok else 'FAILED'}: {msg}")
        return 0 if ok else 1

    if args.list_devices:
        list_devices()
        return 0

    state = SharedState()

    audio_engine = None
    midi_engine = None
    cdc_engine = None

    if not args.no_audio:
        audio_engine = AudioEngine(
            state,
            blocksize=args.blocksize,
            latency=args.latency,
            input_device_override=args.input_device,
            output_device_override=args.output_device,
            target_fill_ms=args.target_fill_ms,
            aggregate_override=args.aggregate,
            aggregate_autocreate=not args.no_aggregate_autocreate,
        )
        audio_engine.start()

    if not args.no_midi:
        midi_engine = MidiEngine(state, port_name_override=args.midi_port)
        midi_engine.start()

    if not args.no_cdc:
        cdc_engine = CdcEngine(state, port_override=args.cdc_port)
        cdc_engine.start()

    # Init ONLY the subsystems we use. Plain pygame.init() also brings up
    # SDL's audio backend, which enumerates CoreAudio devices — and that
    # blocks indefinitely while the Pocket OpGorator is attached (measured:
    # main thread stuck inside pygame.init(), window never appears, looks
    # like "the app won't open"). All our audio goes through sounddevice, so
    # SDL has no business touching an audio device at all.
    pygame.display.init()
    pygame.font.init()
    pygame.display.set_caption("PMOR")

    # Dock icon. Without this, SDL hands macOS the Python framework's own
    # icon, so the Dock tile visibly flips from the PMOR artwork to the
    # Python snake the moment the window opens (and back on quit). Setting
    # it here — BEFORE set_mode(), which is when SDL creates the NSWindow
    # and claims the tile — makes the app keep its own identity.
    try:
        _icon_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  "PMOR-1024.png")
        if os.path.exists(_icon_path):
            pygame.display.set_icon(pygame.image.load(_icon_path))
    except Exception:
        pass  # cosmetic only — never let an icon problem stop the app

    screen = pygame.display.set_mode((ui.WIDTH, ui.HEIGHT))
    clock = pygame.time.Clock()

    font_big = pygame.font.SysFont("Menlo,Consolas,monospace", 40, bold=True)
    font = pygame.font.SysFont("Menlo,Consolas,monospace", 20)
    font_small = pygame.font.SysFont("Menlo,Consolas,monospace", 15)
    fonts = (font_big, font, font_small)

    # dfu_armed_until is owned entirely by this (UI) thread -- it's not
    # shared with any engine, just gates whether the next 'D' keypress is
    # allowed to actually fire the DFU command. See ui.py KEYMAP / the
    # README "Guarded DFU" section for the two-step rationale.
    dfu_armed_until = 0.0

    # SY0-5 map to the top-row number keys.
    SY_KEYS = {
        pygame.K_0: 0, pygame.K_1: 1, pygame.K_2: 2,
        pygame.K_3: 3, pygame.K_4: 4, pygame.K_5: 5,
    }

    running = True
    try:
        while running:
            now = time.time()
            if dfu_armed_until and now > dfu_armed_until:
                dfu_armed_until = 0.0

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    key = event.key
                    if key == pygame.K_ESCAPE:
                        running = False
                        continue

                    if key == pygame.K_d:
                        if dfu_armed_until and now <= dfu_armed_until:
                            # second press within the window: actually fire
                            if cdc_engine is not None:
                                cdc_engine.send_dfu()
                            dfu_armed_until = 0.0
                        else:
                            dfu_armed_until = now + DFU_CONFIRM_WINDOW_S
                        continue

                    # Any other key cancels a pending DFU confirmation --
                    # the confirm has to be the VERY next keypress.
                    dfu_armed_until = 0.0

                    if key in SY_KEYS and cdc_engine is not None:
                        cdc_engine.set_sy_mode(SY_KEYS[key])
                    elif key == pygame.K_b and cdc_engine is not None:
                        currently_on = bool(state.cdc.beat_on)
                        cdc_engine.set_beat(not currently_on)
                    elif key == pygame.K_LEFTBRACKET and cdc_engine is not None:
                        lvl = state.cdc.beat_level if state.cdc.beat_level is not None else 64
                        cdc_engine.set_beat_level(lvl - 8)
                    elif key == pygame.K_RIGHTBRACKET and cdc_engine is not None:
                        lvl = state.cdc.beat_level if state.cdc.beat_level is not None else 64
                        cdc_engine.set_beat_level(lvl + 8)
                    elif key == pygame.K_m and midi_engine is not None:
                        # 'M' = tell the device WE (host) are taking master.
                        # CC116 = 2 means "other side (host) is master".
                        if midi_engine.send_role(2):
                            state.midi.role_sent = 2
                            state.midi.role_sent_at = now
                    elif key == pygame.K_p and midi_engine is not None:
                        # 'P' = hand clock master back to the PMOR/OpGorator
                        # itself. CC116 = 1 means "OpGorator/PMOR is master".
                        if midi_engine.send_role(1):
                            state.midi.role_sent = 1
                            state.midi.role_sent_at = now

            ui.draw(screen, state, fonts, clock.get_fps(), dfu_armed_until)
            pygame.display.flip()
            clock.tick(args.fps)
    finally:
        pygame.quit()
        if audio_engine is not None:
            audio_engine.stop()
        if midi_engine is not None:
            midi_engine.stop()
        if cdc_engine is not None:
            cdc_engine.stop()

    return 0


if __name__ == "__main__":
    sys.exit(main())
