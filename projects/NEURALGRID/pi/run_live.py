#!/usr/bin/env python3
"""neuralGrid Phase 2 entry point: the Pi-side LIVE LAYER.

    python3 pi/run_live.py --port /dev/ttyUSB0 [--color-order grb] [--bpm 120]

Wires up the four softpot-selected menus:

    0 (softpot N) -> Instrument  (menu_instrument.MenuInstrument)
    1 (softpot S) -> Sequencer   (menu_sequencer.MenuSequencer)
    2 (softpot E) -> MIDI-fx     (menu_placeholder.MenuMidiFxPlaceholder, "coming soon")
    3 (softpot W) -> Reserved    (menu_placeholder.MenuReservedPlaceholder)

Runs until Ctrl-C, then does a clean shutdown (all MIDI notes off, LEDs
cleared, serial/MIDI ports closed).

    python3 pi/run_live.py --selftest

runs the whole engine headlessly against a fake link + fake clock (see
neuralgrid/live/fakes.py) -- no serial port, no real MIDI backend required
-- feeding synthetic KEY/ANALOG/clock events and asserting menu switching,
instrument note-on/off, sequencer toggle+playhead+note firing, and
framebuffer validity all behave. Prints PASS/FAIL and exits 0/1.
"""

from __future__ import annotations

import argparse
import sys
import time

sys.path.insert(0, __file__.rsplit("/", 1)[0])

from neuralgrid import codec
from neuralgrid.live.engine import LiveEngine, _apply_color_order
from neuralgrid.live.menu_instrument import MenuInstrument, note_for as instrument_note_for
from neuralgrid.live.menu_sequencer import MenuSequencer, note_for_row
from neuralgrid.live.menu_placeholder import MenuMidiFxPlaceholder, MenuReservedPlaceholder

MENU_CLASSES = {
    0: MenuInstrument,
    1: MenuSequencer,
    2: MenuMidiFxPlaceholder,
    3: MenuReservedPlaceholder,
}


# ---------------------------------------------------------------------------
# real hardware run
# ---------------------------------------------------------------------------


def run_real(
    port: str,
    color_order: str,
    bpm: float,
    brightness: int,
    stage_demo: bool = False,
    brain: str = "off",
) -> None:
    from neuralgrid.link import GridLink
    from neuralgrid.midiout import MidiOut
    from neuralgrid.clock import Clock

    link = GridLink(port)
    midi_out = MidiOut()
    clock = Clock(fallback_bpm=bpm)

    engine = LiveEngine(
        link=link,
        midi_out=midi_out,
        clock=clock,
        menu_classes=MENU_CLASSES,
        brightness=brightness,
        color_order=color_order,
    )

    # --stage-demo is a legacy alias for --brain manual.
    if stage_demo:
        brain = "manual"

    extra_tick = None
    if brain == "manual":
        from neuralgrid.live.stager_manual import ManualStager

        stager = ManualStager(engine)
        extra_tick = stager.tick
        print(
            f"--brain manual: staging a new config every {stager.interval_seconds:.0f}s "
            "(applied exactly on the next downbeat) -- proves the Phase 3 config "
            "boundary before any model exists."
        )
    elif brain == "model":
        from neuralgrid.live.stager_model import ModelStager

        stager = ModelStager(engine)
        extra_tick = stager.tick
        if stager.model is None:
            print(
                "--brain model: interface_vae.npz not found -- run "
                "pi/train_interface_vae.py first. Falling back to no-op (static config)."
            )
        else:
            print(
                f"--brain model: the Interface-VAE brain watches how you play and stages a "
                f"new config roughly every {stager.interval_seconds:.1f}s, swapped in exactly "
                "on the next downbeat -- this closes the adaptive loop."
            )
    else:
        print("--brain off: static config (the safety floor); no stager running.")

    print(f"neuralGrid live layer starting on {port} (color order={color_order}, "
          f"fallback bpm={bpm}). Ctrl-C to stop.")
    try:
        engine.run_forever(extra_tick=extra_tick)
    finally:
        midi_out.close()
        print("Shut down cleanly.")


# ---------------------------------------------------------------------------
# --selftest : headless engine proof, no hardware, no pyserial/rtmidi needed
# ---------------------------------------------------------------------------


class SelfTestFailure(AssertionError):
    pass


def _check(label: str, condition: bool, detail: str = "") -> None:
    status = "PASS" if condition else "FAIL"
    print(f"  [{status}] {label}" + (f" -- {detail}" if detail and not condition else ""))
    if not condition:
        raise SelfTestFailure(f"{label}: {detail}")


def _valid_framebuffer(fb) -> bool:
    if len(fb) != 64:
        return False
    for px in fb:
        if len(px) != 3:
            return False
        if not all(isinstance(v, int) and 0 <= v <= 255 for v in px):
            return False
    return True


_NOISE_RNG = __import__("random").Random(0xC0FFEE)


def _touch_channel(link, channel: int, value: int, ticks: int = 14) -> None:
    """Feed `ticks` synthetic ANALOG frames with `channel` held ~stable at
    `value` (a real finger: tiny jitter) while the OTHER channels get the
    violent wideband noise a floating, untouched SoftPot actually produces on
    hardware (~700+ counts of jump per sample). This is what the stability-
    based TouchDetector must discriminate, so it makes `channel` the sole
    last_touched_channel. (A constant baseline would itself look perfectly
    stable and register as a touch, which is not how the real strips behave.)"""
    for _ in range(ticks):
        values = [_NOISE_RNG.randint(0, 1800) for _ in range(4)]
        values[channel] = value + _NOISE_RNG.randint(-8, 8)
        link.inject_analog(values)


def run_selftest() -> bool:
    from neuralgrid.live.fakes import FakeLink, FakeClock, StubMidiOut

    print("=" * 70)
    print("neuralGrid live layer self-test (no hardware, no pyserial/rtmidi required)")
    print("=" * 70)

    try:
        link = FakeLink()
        clock = FakeClock()
        midi_out = StubMidiOut()
        engine = LiveEngine(
            link=link,
            midi_out=midi_out,
            clock=clock,
            menu_classes=MENU_CLASSES,
            color_order="GRB",
        )

        # -- startup ---------------------------------------------------
        print("-- startup: ping -> HELLO -> brightness --")
        engine.start()
        _check("link started", link.started)
        _check("clock started", clock.started)
        _check("ping was sent", ("ping",) in link.sent_frames)
        _check("HELLO was received", engine.hello is not None)
        _check(
            "brightness was set after HELLO",
            any(f[0] == "brightness" for f in link.sent_frames),
        )
        _check("default active menu is Instrument (0)", engine.active_menu_idx == 0)

        # -- color order applied only at send time --------------------
        print("-- color order applied once, at packing time --")
        packed = _apply_color_order([(10, 20, 30)], "GRB")
        _check("GRB packing swaps R and G", packed == [(20, 10, 30)], str(packed))
        packed_rgb = _apply_color_order([(10, 20, 30)], "RGB")
        _check("RGB packing is identity", packed_rgb == [(10, 20, 30)], str(packed_rgb))

        # -- instrument menu: press lights cell + emits note -----------
        print("-- instrument menu: press/release --")
        link.inject_key(2, 3, True)
        engine.tick()
        expected_note = instrument_note_for(2, 3)
        held_color = engine.framebuffer[3 * 8 + 2]
        _check(
            "framebuffer lit (bright) at pressed cell",
            held_color != (0, 0, 0),
            str(held_color),
        )
        _check(
            "note_on fired for pressed cell",
            ("on", expected_note, 100, 0) in midi_out.events,
            str(midi_out.events),
        )
        link.inject_key(2, 3, False)
        engine.tick()
        _check(
            "note_off fired on release",
            ("off", expected_note, 0) in midi_out.events,
            str(midi_out.events),
        )
        released_color = engine.framebuffer[3 * 8 + 2]
        _check(
            "pressed cell dims to faint idle glow after release (not full-bright)",
            sum(released_color) < sum(held_color),
            f"held={held_color} released={released_color}",
        )
        _check("framebuffer valid 64xRGB (instrument)", _valid_framebuffer(engine.framebuffer))

        # -- menu switching on softpot touch -----------------------------
        print("-- menu switching follows last-touched softpot --")
        _touch_channel(link, channel=1, value=2000)
        engine.tick()
        _check("channel 1 touch switches to Sequencer (1)", engine.active_menu_idx == 1,
               f"got {engine.active_menu_idx}")

        _touch_channel(link, channel=2, value=2500)
        engine.tick()
        _check("channel 2 touch switches to MIDI-fx placeholder (2)", engine.active_menu_idx == 2)
        # Placeholder must never crash on input.
        link.inject_key(5, 5, True)
        link.inject_key(5, 5, False)
        engine.tick()
        _check("placeholder framebuffer valid", _valid_framebuffer(engine.framebuffer))

        _touch_channel(link, channel=3, value=3000)
        engine.tick()
        _check("channel 3 touch switches to Reserved placeholder (3)", engine.active_menu_idx == 3)
        engine.tick()
        _check("reserved placeholder framebuffer valid", _valid_framebuffer(engine.framebuffer))

        _touch_channel(link, channel=0, value=1500)
        engine.tick()
        _check("channel 0 touch switches back to Instrument (0)", engine.active_menu_idx == 0)

        # Switch to sequencer to test it, and confirm instrument's held
        # note (none currently held, but exercise on_exit cleanup path is
        # implicitly covered by earlier press/release already being clean).
        _touch_channel(link, channel=1, value=2000)
        engine.tick()
        _check("back on Sequencer (1)", engine.active_menu_idx == 1)

        # -- sequencer: toggle a cell, playhead advances, lit cells fire --
        print("-- sequencer: toggle + playhead + clock-driven note firing --")
        seq: MenuSequencer = engine.menus[1]  # type: ignore[assignment]
        _check("sequencer starts empty", not any(any(row) for row in seq.grid))

        link.inject_key(3, 2, True)  # step=3, track(row)=2
        link.inject_key(3, 2, False)  # release must NOT untoggle
        engine.tick()
        _check("press toggled cell (3,2) on", seq.grid[2][3] is True)
        _check("release did not untoggle", seq.grid[2][3] is True)

        midi_out.events.clear()
        for step in range(8):
            clock.fire_step(step)
            engine.tick()
        _check("playhead reached column matching last fired step", seq.playhead == 7)
        expected_seq_note = note_for_row(2)
        _check(
            "lit cell fired a note_on when playhead entered its column",
            any(e[0] == "on" and e[1] == expected_seq_note for e in midi_out.events),
            str(midi_out.events),
        )

        time.sleep(0.2)
        engine.tick()
        _check(
            "gated note releases itself via tick() after its gate time",
            any(e[0] == "off" and e[1] == expected_seq_note for e in midi_out.events),
            str(midi_out.events),
        )
        _check("framebuffer valid 64xRGB (sequencer)", _valid_framebuffer(engine.framebuffer))

        # -- config boundary (Phase 3): codec round-trip + downbeat swap --
        print("-- config boundary: codec round-trip + downbeat-gated atomic swap --")
        from neuralgrid import config as cfgmod
        import numpy as np

        default_cfg = cfgmod.default_config()
        vec = cfgmod.to_vector(default_cfg)
        _check(
            "to_vector produces a VECTOR_LEN-length float32 vector",
            vec.shape == (cfgmod.VECTOR_LEN,) and vec.dtype == np.float32,
            f"shape={vec.shape} dtype={vec.dtype}",
        )

        round_tripped = cfgmod.from_vector(vec)
        _roundtrip_ok = True
        _roundtrip_detail = ""
        for key, val in default_cfg.items():
            rt_val = round_tripped.get(key)
            if isinstance(val, float):
                ok = rt_val is not None and abs(val - rt_val) < 1e-5
            else:
                ok = rt_val == val
            if not ok:
                _roundtrip_ok = False
                _roundtrip_detail = f"{key}: {val!r} != {rt_val!r}"
                break
        _check(
            "default_config round-trips dict<->vector losslessly (float tolerance)",
            _roundtrip_ok, _roundtrip_detail,
        )

        _check(
            "engine boots with the default active config",
            engine.config["note_layout"] == default_cfg["note_layout"]
            and engine.config["scale_id"] == default_cfg["scale_id"]
            and engine.config["led_palette"] == default_cfg["led_palette"],
        )

        # -- valid staged config applies EXACTLY on the downbeat, not before
        new_cfg = dict(default_cfg)
        new_cfg["note_layout"] = 2  # scale_degrees
        new_cfg["scale_id"] = 6  # minor_pentatonic
        new_cfg["led_palette"] = 3  # ice
        staged_ok = engine.stage_config(new_cfg, source="selftest")
        _check("valid config accepted for staging", staged_ok is True)

        prev = dict(engine.config)
        engine.tick()
        engine.tick()
        _check(
            "staged config NOT applied before a downbeat (still ticking)",
            engine.config["note_layout"] == prev["note_layout"]
            and engine.config["scale_id"] == prev["scale_id"]
            and engine.config["led_palette"] == prev["led_palette"],
        )

        clock.fire_downbeat()
        _check(
            "staged config applied exactly on the downbeat, after it fires",
            engine.config["note_layout"] == 2
            and engine.config["scale_id"] == 6
            and engine.config["led_palette"] == 3,
        )

        # -- invalid / wrong-length staged config is rejected, active persists
        active_after_swap = dict(engine.config)
        bad_vector = np.zeros(5, dtype=np.float32)  # wrong length
        rejected = engine.stage_config(bad_vector, source="bad-vector-length")
        _check("wrong-length vector rejected at stage time", rejected is False)

        bad_dict = dict(default_cfg)
        bad_dict["schema_version"] = 999  # wrong schema
        rejected2 = engine.stage_config(bad_dict, source="bad-schema-version")
        _check("wrong schema_version dict rejected at stage time", rejected2 is False)

        bad_dict2 = dict(default_cfg)
        bad_dict2["root_note"] = 99  # out of range enum
        rejected3 = engine.stage_config(bad_dict2, source="bad-enum-range")
        _check("out-of-range enum field dict rejected at stage time", rejected3 is False)

        clock.fire_downbeat()
        _check(
            "active config unchanged after rejecting invalid staged configs",
            engine.config["note_layout"] == active_after_swap["note_layout"]
            and engine.config["scale_id"] == active_after_swap["scale_id"]
            and engine.config["led_palette"] == active_after_swap["led_palette"],
        )

        # -- model layer (Phase 4): Interface-VAE + ModelStager -------------
        print("-- model layer: Interface-VAE forward pass + ModelStager off the hot path --")
        from neuralgrid.models.interface_vae import InterfaceVAE, LATENT_DIM
        from neuralgrid.models.features import FeatureWindow, FEATURE_DIM
        from neuralgrid.live.stager_model import ModelStager

        vae = InterfaceVAE(seed=0)
        _check(
            "InterfaceVAE is tiny (sub-1M params)",
            0 < vae.param_count() < 1_000_000,
            str(vae.param_count()),
        )

        zero_latent = np.zeros(LATENT_DIM, dtype=np.float32)
        decoded = vae.decode(zero_latent)
        _check(
            "decode(latent) returns a VECTOR_LEN-length float vector",
            decoded.shape == (cfgmod.VECTOR_LEN,),
            str(decoded.shape),
        )
        decoded_cfg = cfgmod.from_vector(decoded)
        _decode_valid = True
        try:
            cfgmod.validate(decoded_cfg)
        except ValueError:
            _decode_valid = False
        _check("config.validate() accepts a decoded vector", _decode_valid)

        # encode -> decode of a known preset stays reasonably close (this
        # is an untrained, freshly-initialized model, so "reasonably
        # close" just means: doesn't crash, stays in-range, and isn't
        # wildly far in raw vector distance from something plausible).
        preset_vec = cfgmod.to_vector(default_cfg)
        fake_features = np.zeros(FEATURE_DIM, dtype=np.float32)
        mu, logvar = vae.encode(fake_features)
        _check("encode() returns latent-shaped mu/logvar", mu.shape == (LATENT_DIM,) and logvar.shape == (LATENT_DIM,))
        round_trip_vec = vae.infer(fake_features, temperature=0.0)
        _check(
            "encode->decode output is in [0,1] (sigmoid range)",
            bool(np.all(round_trip_vec >= 0.0) and np.all(round_trip_vec <= 1.0)),
        )
        rt_cfg = cfgmod.from_vector(round_trip_vec)
        _rt_valid = True
        try:
            cfgmod.validate(rt_cfg)
        except ValueError:
            _rt_valid = False
        _check("encode->decode of a preset's features decodes to a valid config", _rt_valid)

        # -- save/load reproduces outputs ---------------------------------
        import tempfile, os

        with tempfile.TemporaryDirectory() as tmpdir:
            npz_path = os.path.join(tmpdir, "interface_vae_selftest.npz")
            vae.save(npz_path)
            loaded_vae = InterfaceVAE.load(npz_path)
            out_before = vae.decode(zero_latent)
            out_after = loaded_vae.decode(zero_latent)
            _check(
                "loading a saved .npz reproduces identical outputs",
                bool(np.allclose(out_before, out_after)),
            )

        # -- ModelStager: synthetic feature stream stages a valid config,
        # swapped by the engine's existing downbeat mechanism -----------
        stager_engine = LiveEngine(
            link=FakeLink(),
            midi_out=StubMidiOut(),
            clock=FakeClock(),
            menu_classes=MENU_CLASSES,
            color_order="GRB",
        )
        stager_engine.start()
        # Feed a synthetic "hammering pads" interaction stream directly
        # into the engine's rolling feature window, exactly as _on_key
        # would from real KEY events.
        for i in range(20):
            stager_engine.interaction_features.on_key_event(i % 8, (i * 3) % 8, True, t=time.monotonic())
        stager = ModelStager(stager_engine, model=vae, interval_seconds=0.0)
        _check("ModelStager loaded a model (constructor arg, no .npz needed for this check)", stager.model is not None)

        staged_now = stager.stage_now()
        _check("ModelStager.stage_now() stages a config the engine accepts", staged_now is True)

        downbeats_before = stager_engine._downbeat_count
        stager_engine.clock.fire_downbeat()  # type: ignore[attr-defined]
        _check(
            "engine's downbeat counter advanced (Phase 3 swap mechanism fired, unchanged)",
            stager_engine._downbeat_count == downbeats_before + 1,
        )
        _swap_valid = True
        try:
            cfgmod.validate(stager_engine.config)
        except ValueError:
            _swap_valid = False
        _check("post-swap active config is valid", _swap_valid)
        stager_engine.stop()

        # -- ModelStager gracefully no-ops with no model loaded -----------
        noop_engine = LiveEngine(
            link=FakeLink(), midi_out=StubMidiOut(), clock=FakeClock(),
            menu_classes=MENU_CLASSES, color_order="GRB",
        )
        noop_engine.start()
        noop_stager = ModelStager(noop_engine, model=None, interval_seconds=0.0)
        _check("ModelStager with no model set is a documented no-op state", noop_stager.model is None)
        noop_result = noop_stager.stage_now()
        _check("ModelStager.stage_now() with no model returns False, doesn't raise", noop_result is False)
        noop_engine.stop()

        # -- FeatureWindow: rolling window is O(1)-ish and bounded, doesn't
        # block/grow unbounded, feeding events is safe from the hot path --
        fw = FeatureWindow(window_seconds=1.0)
        t0 = time.monotonic()
        for i in range(500):
            fw.on_key_event(i % 8, i % 8, True, t=t0 + i * 0.001)
            fw.on_analog_event([100, 200, 300, 400], t=t0 + i * 0.001)
        feats = fw.compute_features(now=t0 + 0.5)
        _check("FeatureWindow.compute_features returns FEATURE_DIM floats", feats.shape == (FEATURE_DIM,))
        _check(
            "FeatureWindow.compute_features stays within [0,1]",
            bool(np.all(feats >= 0.0) and np.all(feats <= 1.0)),
        )

        # -- --brain off leaves the config static (no stager attached) ----
        static_engine = LiveEngine(
            link=FakeLink(), midi_out=StubMidiOut(), clock=FakeClock(),
            menu_classes=MENU_CLASSES, color_order="GRB",
        )
        static_engine.start()
        static_before = dict(static_engine.config)
        for _ in range(5):
            static_engine.tick()  # no extra_tick, mirroring --brain off
        static_engine.clock.fire_downbeat()  # type: ignore[attr-defined]
        static_after = dict(static_engine.config)
        _check(
            "--brain off: config stays static across ticks and downbeats (no stager attached)",
            static_before == static_after,
        )
        static_engine.stop()

        # -- the live loop is not blocked by the stager --------------------
        blocking_engine = LiveEngine(
            link=FakeLink(), midi_out=StubMidiOut(), clock=FakeClock(),
            menu_classes=MENU_CLASSES, color_order="GRB",
        )
        blocking_engine.start()
        blocking_stager = ModelStager(blocking_engine, model=vae, interval_seconds=0.0)
        t_start = time.monotonic()
        for _ in range(20):
            blocking_engine.tick()
            blocking_stager.tick()
        elapsed = time.monotonic() - t_start
        _check(
            "20 ticks with the model stager attached complete fast (stager never blocks the loop)",
            elapsed < 2.0,
            f"{elapsed:.3f}s",
        )
        blocking_engine.stop()

        # -- shutdown ---------------------------------------------------
        print("-- shutdown: all notes off + LEDs cleared --")
        engine.stop()
        _check("all_notes_off called on shutdown", ("all_off",) in midi_out.events)
        _check("clear frame sent on shutdown", ("clear",) in link.sent_frames)
        _check("link stopped", link.started is False)
        _check("clock stopped", clock.started is False)

    except SelfTestFailure as exc:
        print()
        print(f"OVERALL: FAIL -- {exc}")
        return False
    except Exception as exc:  # noqa: BLE001 - surface any unexpected crash too
        print()
        print(f"OVERALL: FAIL -- unexpected exception: {exc!r}")
        return False

    print()
    print("OVERALL: PASS -- live layer engine behaves per spec")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default="/dev/ttyUSB0", help="ESP32 serial port")
    parser.add_argument("--color-order", default="GRB", help="LED panel wire channel order, e.g. GRB or RGB")
    parser.add_argument("--bpm", type=float, default=120.0, help="fallback tempo (bpm) when no external MIDI clock is present")
    parser.add_argument("--brightness", type=int, default=180, help="global LED brightness (0..255)")
    parser.add_argument("--selftest", action="store_true", help="run the headless engine self-test (no hardware)")
    parser.add_argument(
        "--stage-demo", action="store_true",
        help="deprecated alias for --brain manual",
    )
    parser.add_argument(
        "--brain", choices=["off", "model", "manual"], default="off",
        help="off (default): static config, the safety floor. model: the Interface-VAE "
             "(Phase 4) watches how you play and stages new configs, closing the adaptive "
             "loop. manual: the Phase 3 hand-authored demo sequence (proves the config "
             "boundary without any model).",
    )
    args = parser.parse_args()

    if args.selftest:
        ok = run_selftest()
        sys.exit(0 if ok else 1)

    run_real(
        args.port, args.color_order, args.bpm, args.brightness,
        stage_demo=args.stage_demo, brain=args.brain,
    )


if __name__ == "__main__":
    main()
