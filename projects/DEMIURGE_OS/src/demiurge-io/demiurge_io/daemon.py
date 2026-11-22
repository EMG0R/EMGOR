"""daemon.py — wires sources -> conditioning -> router -> sinks together.

THREADING MODEL: each source's `run()` blocks forever reading its own kind
of I/O (a termios fd for the Teensy, a UDP socket for web). Rather than
unify termios-select and socket-select into one loop (which would need to
grow every time a new source with a different I/O shape shows up — the spec
explicitly wants video.py/game.py to drop in later without touching this
file's core logic), each source runs in its own daemon thread and pushes
into one shared queue.Queue. The main thread does nothing but drain that
queue and run the pure conditioning/router functions — the actual
sensor-to-OSC latency work is a handful of dict lookups and float ops per
item, so a single consumer thread is not a bottleneck at 100 Hz x a few
sources.

CONFIG HOT RELOAD: checked once per queue-drain pause (see MAIN_LOOP_IDLE_S)
rather than on every item — an os.stat() per queue item at 100+ Hz would be
wasteful for something that changes on human editing timescales.

PARAMETER POOL (docs/parameters.md): this daemon IS the pool — nothing new runs.
Added to the same loop: an `osc_in` source (direct `/p <path> <val>` parameter
writes, arriving as a third queue item type), optional `midi` and `serial_auto`
sources (Readings, exactly like teensy/web), `osc`/`midi` sinks next to the
csound sink behind the same one-method interface, a ParamRegistry so a map
target can be {"param": "/stage/name"}, and PatchMapWatcher so the active
patch's ~/demiurge/patches/<name>.map.json layers over sensors.json maps.
With no patch map file, no registry entries used and no new flags, behaviour
is identical to before.

--dry-run: swaps the real CsoundSink for DryRunSink; everything else in the
pipeline (conditioning, router, telemetry) runs identically, so this is a
genuine dry run of the whole decision pipeline, not a stub.
"""

from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from pathlib import Path

from . import conditioning, router
from .config import (ConfigWatcher, DEFAULT_PATH, DEFAULT_ROOT, PatchMapWatcher,
                     merge_maps, _SOURCE_DEFAULTS)
from .pool import Pool, ParamRegistry, PARAMS_DIR, expr_targets_candidates
from .sources.teensy import resolve_port
from .model import Reading
from .sinks.csound import CsoundSink, DryRunSink
from .sinks.osc import OscSink
from .sinks.midi import MidiSink
from .sinks.transport import TransportSink
from .arcade import ArcadeRouter, ARCADE_SOURCE, TAP, TRIG
from .sources.teensy import (TeensySource, SENSOR_DESCRIPTORS as TEENSY_DESCRIPTORS,
                            BUTTON_CCS, ENC_BTN_CCS, JOY_BTN_CC, POOL_PREFIX,
                            CHANNEL as CONTROL_CHANNEL)
from .sources.web import WebSource
from .sources.osc_in import OscInSource, PORT_DEFAULT as POOL_PORT_DEFAULT
from .sources.midi import (MidiSource, Snapshot, load_monitor_conf, DEFAULT_SNAPSHOT,
                           neptr_pedal_normalize)
from .sources.serial_auto import SerialAutoSource
from .telemetry import TelemetryBroadcaster, parse_destination

MAIN_LOOP_IDLE_S = 0.05   # queue.get timeout; also the config-reload check period

# Discrete controls that can arrive as a PUSHED reading rather than a serial
# frame — the web virtual panel sends `b9 1` on the same UDP line it uses for
# gyro and faders, because a browser has no serial port.
#
# They are re-published on the telemetry bus as the SAME `/cc` a real button
# produces, taken from the Teensy's own tables so there is exactly one
# name->CC map in the system. Without this a virtual press reaches csound (the
# web UI sends it /cc directly) and NOTHING else: the video engine's HOME
# selector listens for `cc:1:63`-style events, and a `/sensor b4` is not one.
# On a rig with no physical button board that difference is the difference
# between a navigable menu and a dead one.
#
# Encoders (e1-e7) are deliberately absent: an encoder reading is a relative
# delta, and turning one into a level-style CC would latch a phantom position.
WEB_CONTROL_CCS: dict = {}
WEB_CONTROL_CCS.update(BUTTON_CCS)
WEB_CONTROL_CCS.update(ENC_BTN_CCS)
WEB_CONTROL_CCS["jb"] = JOY_BTN_CC


def source_aliases(source_id: str) -> tuple:
    """Every map `source` string that means this Reading. A Teensy sensor is
    `jx` on the wire and in sensors.json and `/hw/teensy/jx` in the pool; maps
    may use either, so today's maps keep working and new ones use pool names."""
    if source_id in TEENSY_DESCRIPTORS:
        return (source_id, POOL_PREFIX + source_id)
    return (source_id,)


class _ReadingItem:
    __slots__ = ("reading",)

    def __init__(self, reading: Reading):
        self.reading = reading


class _ParamItem:
    __slots__ = ("path", "value", "engine")

    def __init__(self, path: str, value: float, engine: bool = False):
        self.path = path
        self.value = value
        self.engine = engine        # True: engine-origin /pout, never sent back


class _ControlItem:
    __slots__ = ("ccs",)

    def __init__(self, ccs):
        self.ccs = ccs


class Daemon:
    def __init__(self, config_path: Path = DEFAULT_PATH, dry_run: bool = False,
                 teensy_port: str = None, web_host: str = None, web_port: int = None,
                 csound_host: str = "127.0.0.1", csound_port: int = 7770,
                 telemetry_host: str = "127.0.0.1", telemetry_port: int = 8002,
                 telemetry_destinations=None,
                 enable_teensy: bool = True, enable_web: bool = True,
                 forward_control: bool = False,
                 enable_osc_in: bool = False, pool_host: str = "127.0.0.1",
                 pool_port: int = POOL_PORT_DEFAULT,
                 enable_midi: bool = False, midi_port: str = "auto",
                 midi_snapshot: str = None, monitor_conf: Path = None,
                 serial_auto_ports=(), osc_allow_hosts=(),
                 midi_neptr: bool = False, midi_allow=(),
                 arcade_hold_s: float = None, clock_osc=None,
                 telemetry_params: bool = False,
                 root: Path = DEFAULT_ROOT, params_dir: Path = PARAMS_DIR,
                 expr_targets: Path = None):
        self.forward_control = forward_control
        self.q: "queue.Queue" = queue.Queue()
        self.stop_event = threading.Event()

        self.watcher = ConfigWatcher(config_path, TEENSY_DESCRIPTORS)

        self.sink = DryRunSink() if dry_run else CsoundSink(csound_host, csound_port)

        # ── parameter pool: registry + sinks + per-patch maps ──────────────
        if expr_targets is None:
            expr_targets = next((p for p in expr_targets_candidates() if p.exists()),
                                expr_targets_candidates()[0])
        self.registry = ParamRegistry(params_dir, expr_targets, builtins=True)
        self.registry.refresh(force=True)
        self.pool = Pool(self.registry)
        self.pool.add_sink("csound", self.sink)
        self.pool.add_sink("osc", DryRunSink("osc") if dry_run
                           else OscSink(allow_hosts=osc_allow_hosts))
        self.pool.add_sink("midi", DryRunSink("midi") if dry_run else MidiSink())
        self.pool.add_sink("transport", DryRunSink("transport") if dry_run
                           else (TransportSink(clock_osc) if clock_osc else TransportSink()))
        self.arcade = (ArcadeRouter(arcade_hold_s) if arcade_hold_s else ArcadeRouter())
        self._arcade_hold_default = self.arcade.hold_s
        self.patchmaps = PatchMapWatcher(root)
        self.telemetry_params = telemetry_params
        self._maps = self.pool.registry.enforce_hard(
                merge_maps(self.watcher.config.maps, self.patchmaps.maps))
        self._apply_patch_values()
        self.arcade.set_uss_active(self._uss_active())
        self.telemetry = TelemetryBroadcaster(
            telemetry_destinations or [(telemetry_host, telemetry_port)])

        self.sources = []
        if enable_teensy:
            # resolve_port() picks the body's stable /dev/serial/by-id name and
            # excludes the Bela and DUODECIMUS. A bare "/dev/ttyACM0" here is an
            # ENUMERATION INDEX, not a device: on 2026-09-15 it did not exist at
            # all (the body was ttyACM1, the Bela ttyACM2) and the sensors were
            # silently dead. See the comment above resolve_port in sources/teensy.py.
            self.sources.append(TeensySource(teensy_port or resolve_port()))
        if enable_web:
            kwargs = {}
            if web_host is not None:
                kwargs["host"] = web_host
            if web_port is not None:
                kwargs["port"] = web_port
            self.sources.append(WebSource(**kwargs))
        if enable_osc_in:
            self.sources.append(OscInSource(pool_host, pool_port))
        if enable_midi:
            snap = None
            if midi_snapshot:
                cmap, center, interval = load_monitor_conf(
                    monitor_conf or (root / "monitor.conf"))
                snap = Snapshot(cmap, center, midi_snapshot, interval)
            allow = set(midi_allow or ())
            if midi_neptr:
                allow.add((1, 29))          # the logical expression pedal, nothing else
            self.sources.append(MidiSource(
                midi_port, snap,
                normalize=neptr_pedal_normalize if midi_neptr else None,
                allow=allow or None))
        for sp in serial_auto_ports:
            self.sources.append(SerialAutoSource(sp))

        # Per-source-id conditioning state, per-map-id router state.
        self._cond_state: dict[str, conditioning.ConditionerState] = {}
        self._cond_last_t: dict[str, float] = {}
        self._map_state: dict[str, router.MapState] = {}
        self._map_last_t: dict[str, float] = {}
        # Last known up/down of each PUSHED discrete control, so a repeated
        # datagram is not a second press.
        self._pushed_ctl_state: dict[str, bool] = {}

        self._threads: list[threading.Thread] = []

        if telemetry_params:
            self.pool.subscribe(
                lambda path, v: self.telemetry.publish_param(path, v, time.monotonic()))

    # ── source thread plumbing ──────────────────────────────────────────
    def _on_reading(self, r: Reading) -> None:
        self.q.put(_ReadingItem(r))

    def _on_control(self, ccs) -> None:
        self.q.put(_ControlItem(ccs))

    def _on_param(self, path: str, value: float, engine: bool = False) -> None:
        self.q.put(_ParamItem(path, value, engine))

    def _start_sources(self) -> None:
        for src in self.sources:
            t = threading.Thread(target=self._run_source, args=(src,), daemon=True,
                                  name=f"demiurge-io-src-{getattr(src, 'id', '?')}")
            t.start()
            self._threads.append(t)

    def _run_source(self, src) -> None:
        try:
            if getattr(src, "accepts_param", False):
                src.run(self._on_reading, self._on_control, self._on_param)
            else:
                src.run(self._on_reading, self._on_control)
        except Exception as e:  # a wedged source must not take the daemon down
            print(f"[demiurge-io] source {getattr(src, 'id', src)!r} died: {e}",
                  file=sys.stderr, flush=True)

    # ── per-item processing ─────────────────────────────────────────────
    def _find_descriptor(self, source_id: str):
        # Only the Teensy source has a fixed, known sensor set today.
        return TEENSY_DESCRIPTORS.get(source_id)

    def _fallback_source_cfg(self, source_id: str):
        """Calibration for a source with NO sensors.json entry. Only the pool's
        own "/hw/..." sources qualify (they are born 0..1 or self-describing);
        anything else keeps the old telemetry-only behaviour."""
        if not source_id.startswith("/hw/"):
            return None
        cfg = dict(_SOURCE_DEFAULTS)
        for src in self.sources:
            cal = getattr(src, "calibration", None)
            if cal:
                over = cal(source_id)
                if over:
                    cfg.update(over)
                    break
        return cfg

    def _apply_patch_values(self) -> None:
        """A patch's baseline `values` are written once, on patch load."""
        for path, v in self.patchmaps.values.items():
            self.pool.publish(path, v)

    def _reload_maps(self) -> None:
        """Hot reload of everything the loop watches. Cheap: stats only."""
        changed = self.watcher.maybe_reload()
        if self.patchmaps.maybe_reload():
            changed = True
            if self.patchmaps.patch_changed:
                self._apply_patch_values()
        if self.registry.refresh():
            changed = True          # hard maps may have appeared/changed
        if changed:
            self._maps = self.pool.registry.enforce_hard(
                merge_maps(self.watcher.config.maps, self.patchmaps.maps))
            self._arcade_event(self.arcade.set_uss_active(self._uss_active()))

    # ── arcade role (arcade.py) ──────────────────────────────────────────
    def _uss_active(self) -> bool:
        """A USS instrument is active iff a live map claims the arcade as its
        trigger. Its manifest is only in ~/demiurge/params while it is the slot."""
        trig = [m for m in self._maps if m.get("arcade") == TRIG and m.get("enabled", True)]
        if trig:
            # An instrument that owns its own long hold (lockin: 5 s toggles its
            # sequencer) declares a longer pool hold in its hard map.
            self.arcade.hold_s = max([float(m.get("arcade_hold_s", self._arcade_hold_default))
                                      for m in trig])
        return bool(trig)

    def _arcade_event(self, new_mode) -> None:
        """The arcade role flipped (USS on/off, or a long hold). Release a held
        trigger gate so the instrument is never left gated, and re-arm the other
        role's map so the next press is a clean edge."""
        if new_mode is None:
            return
        print(f"[demiurge-io] arcade -> {new_mode}", file=sys.stderr, flush=True)
        for m in self._maps:
            if m.get("source") != ARCADE_SOURCE or "arcade" not in m:
                continue
            self._map_state.pop(m["id"], None)
            if m["arcade"] == TRIG and new_mode == TAP:
                self.pool.send_target(m["target"], m.get("out_lo", 0.0))

    def _arcade_tick(self) -> None:
        self._arcade_event(self.arcade.tick(time.monotonic()))

    def _publish_pushed_control(self, r: Reading) -> None:
        """A pushed discrete control (web virtual panel) -> the same `/cc` a
        real button makes. Edge-triggered, exactly like ControlState does for
        the serial frame: a repeated `b9 1` is one press, not a stream."""
        cc = WEB_CONTROL_CCS.get(r.source_id)
        if cc is None:
            return
        down = float(r.raw) >= 0.5
        if self._pushed_ctl_state.get(r.source_id) == down:
            return
        self._pushed_ctl_state[r.source_id] = down
        self.telemetry.publish_cc(CONTROL_CHANNEL, cc, 127 if down else 0)

    def _process_reading(self, r: Reading) -> None:
        cfg = self.watcher.config
        source_cfg = cfg.sources.get(r.source_id) or self._fallback_source_cfg(r.source_id)
        now = r.t

        self._publish_pushed_control(r)
        if r.source_id == ARCADE_SOURCE:
            self.arcade.on_edge(float(r.raw) >= 0.5, r.t)

        # Telemetry publishes raw-normalized-to-[0,1]-ish position for ANY
        # source, even one with no sensors.json entry yet (so a brand new
        # sensor shows up in a meters UI immediately) — falls back to a
        # trivial 0..1 clamp of the raw value when uncalibrated.
        if source_cfg is None:
            norm_for_telemetry = max(0.0, min(1.0, float(r.raw)))
            self.telemetry.publish(r.source_id, norm_for_telemetry, now)
            return
        if not source_cfg.get("enabled", True):
            return

        dt = now - self._cond_last_t.get(r.source_id, now)
        self._cond_last_t[r.source_id] = now
        state = self._cond_state.get(r.source_id, conditioning.ConditionerState())
        descriptor = self._find_descriptor(r.source_id)
        norm, valid, new_state = conditioning.condition(r.raw, source_cfg, descriptor,
                                                          state, dt)
        self._cond_state[r.source_id] = new_state

        # Pool-only sources (`/hw/teensy/b1`...) are not on the /sensor meters:
        # buttons already reach the telemetry bus as /cc (see _process_control).
        if not r.source_id.startswith(POOL_PREFIX):
            self.telemetry.publish(r.source_id, norm, now)

        # Every enabled map reading this source, evaluated independently,
        # then folded per-target by combine mode.
        contributions: dict[tuple, list[tuple[str, float]]] = {}
        aliases = source_aliases(r.source_id)
        for m in self._maps:
            if not m.get("enabled", True) or m.get("source") not in aliases:
                continue
            if not self.arcade.allows(m):
                continue
            map_id = m["id"]
            mdt = now - self._map_last_t.get(map_id, now)
            self._map_last_t[map_id] = now
            mstate = self._map_state.get(map_id, router.MapState())
            result = router.apply_map(m, norm, valid, mstate, mdt)
            self._map_state[map_id] = result.new_state
            if not result.send:
                continue
            target = m["target"]
            key = tuple(sorted(target.items()))
            contributions.setdefault(key, []).append((m.get("combine", "replace"), result.value))

        for key, contribs in contributions.items():
            target = dict(key)
            value = router.combine_values(contribs)
            if value is not None:
                self.pool.send_target(target, value, t0=now)

    def _process_param(self, item: "_ParamItem") -> None:
        if item.engine:
            self.pool.publish_out(item.path, item.value)
        else:
            self.pool.publish(item.path, item.value)

    def _process_control(self, ccs) -> None:
        """Encoders and buttons. OFF BY DEFAULT — see forward_control.

        The Teensy is built usb=serialmidi and emits BOTH a serial frame and
        real USB-MIDI, in parallel. csound is already subscribed to that MIDI
        (-Ma, via aconnect into Midi Through). So if we ALSO forward encoder
        clicks as /cc, csound sees every click TWICE and parameters move at
        double speed — the same double-feed neptr-bridge.service guards against
        with its `aconnect -d "Teensy MIDI":0 14:0`.

        Leaving these on the Teensy's own MIDI wire is also the conservative
        choice: encoders and toggles are NEPTR's central control mechanism and
        they keep the shortest possible path to the engine, unchanged and
        unaffected by anything this daemon does — if demiurge-io dies mid-set,
        the instrument still plays exactly as before. Sensors are additive.

        Turn this on only when the Teensy's direct MIDI feed has been torn
        down and demiurge-io is deliberately the single source.
        """
        # The telemetry bus ALWAYS sees these, regardless of forward_control.
        # That flag exists to stop csound being fed the same click twice (it
        # already has the Teensy's own USB-MIDI); it is not a statement that
        # other consumers should be kept in the dark. A boot-time mode selector
        # in the video engine is driven by exactly these events.
        for cc in ccs:
            self.telemetry.publish_cc(cc.channel, cc.cc, cc.value)
        if not self.forward_control:
            return
        for cc in ccs:
            self.sink.send_cc(cc.channel, cc.cc, cc.value)

    # ── main loop ────────────────────────────────────────────────────────
    def run_forever(self) -> None:
        self._start_sources()
        print(f"[demiurge-io] running "
              f"({'DRY RUN' if isinstance(self.sink, DryRunSink) else 'live'}); "
              f"sources={[getattr(s, 'id', '?') for s in self.sources]}",
              file=sys.stderr, flush=True)
        while not self.stop_event.is_set():
            try:
                item = self.q.get(timeout=MAIN_LOOP_IDLE_S)
            except queue.Empty:
                self._reload_maps()
                self._arcade_tick()
                self.pool.flush()
                continue
            if isinstance(item, _ReadingItem):
                self._process_reading(item.reading)
            elif isinstance(item, _ControlItem):
                self._process_control(item.ccs)
            elif isinstance(item, _ParamItem):
                self._process_param(item)
            self._reload_maps()
            self._arcade_tick()
            self.pool.flush()


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="demiurge-io",
                                 description="Sensor -> parameter control plane for NEPTR.")
    p.add_argument("--config", type=Path, default=DEFAULT_PATH,
                    help=f"sensors.json path (default {DEFAULT_PATH})")
    p.add_argument("--dry-run", action="store_true",
                    help="print what would be sent instead of sending OSC")
    # default None, NOT a path: an explicit flag still wins, but when it is
    # absent the resolver runs at construction time and picks the by-id name
    # that is actually the NEPTR body on this boot.
    p.add_argument("--teensy-port", default=None,
                   help="serial port for the NEPTR body; default = auto-detect "
                        "via /dev/serial/by-id (never a bare ttyACM index)")
    p.add_argument("--no-teensy", action="store_true", help="disable the Teensy source")
    p.add_argument("--no-web", action="store_true", help="disable the web/UDP source")
    p.add_argument("--forward-control", action="store_true",
                   help="also forward encoder/button CCs as OSC /cc. OFF by "
                        "default: the Teensy already sends these as real USB-MIDI "
                        "straight to csound, so forwarding them too makes every "
                        "encoder click register TWICE. Only enable if you have torn "
                        "down the Teensy's direct MIDI feed.")
    # ── parameter pool (all additive; with none of these, behaviour == before) ──
    p.add_argument("--osc-in", action="store_true",
                   help="enable the pool's OSC ingress: `/p <path> <val>` on "
                        "--pool-port (loopback)")
    p.add_argument("--pool-host", default="127.0.0.1")
    p.add_argument("--pool-port", type=int, default=POOL_PORT_DEFAULT)
    p.add_argument("--midi", action="store_true",
                   help="enable the ALSA-seq CC source (/hw/midi/<ch>/<cc>)")
    p.add_argument("--midi-port", default="auto")
    p.add_argument("--midi-snapshot", nargs="?", const=DEFAULT_SNAPSHOT, default=None,
                   metavar="FILE",
                   help="with --midi: also write demiurge-monitor's one-line "
                        "snapshot (default %s). Do NOT run alongside "
                        "demiurge-monitor.service." % DEFAULT_SNAPSHOT)
    p.add_argument("--monitor-conf", type=Path, default=None,
                   help="monitor.conf for snapshot labels (default ~/demiurge/monitor.conf)")
    p.add_argument("--midi-neptr", action="store_true",
                   help="with --midi: NEPTR pedal funnel at the source (ch7 CC14/15 -> "
                        "`/hw/midi/1/29`, the body's own ch1 CC29 parked) and publish ONLY "
                        "that pedal. Mirrors midi_handler.orc; see docs/NEPTR_POOL_BENCH.md.")
    p.add_argument("--midi-allow", action="append", default=[], metavar="CH:CC",
                   help="with --midi: only publish these (channel:cc, after any "
                        "--midi-neptr rewrite); repeatable. Default: every CC.")
    p.add_argument("--arcade-hold", type=float, default=None, metavar="SEC",
                   help="long-hold (seconds) that flips the arcade between tap tempo and "
                        "the active USS trigger (default 2.0)")
    p.add_argument("--serial-auto", action="append", default=[], metavar="PORT",
                   help="SCAFFOLD: generic serial->pool source on an EXPLICIT "
                        "port; repeatable. Never auto-discovered.")
    p.add_argument("--osc-allow-host", action="append", default=[], metavar="HOST",
                   help="allow the osc sink to send to this non-loopback host; repeatable")
    p.add_argument("--telemetry-params", action="store_true",
                   help="also publish /param <path> <val> on the telemetry bus")
    p.add_argument("--list-params", action="store_true",
                   help="print the merged parameter registry and exit (opens nothing)")
    p.add_argument("--web-host", default="127.0.0.1")
    p.add_argument("--web-port", type=int, default=8003)
    p.add_argument("--csound-host", default="127.0.0.1")
    p.add_argument("--csound-port", type=int, default=7770)
    p.add_argument("--telemetry-host", default="127.0.0.1")
    p.add_argument("--telemetry-port", type=int, default=8002)
    p.add_argument("--telemetry", action="append", metavar="HOST:PORT",
                   help="extra telemetry consumer; repeatable. Each gets the "
                        "full /sensor + /cc feed. Bare port means localhost. "
                        "Given at least once, this REPLACES the default "
                        "destination, so pass 8002 explicitly if the web UI "
                        "should still receive.")
    return p


def list_params() -> int:
    reg = ParamRegistry(PARAMS_DIR, next((p for p in expr_targets_candidates()
                                          if p.exists()), None), builtins=True)
    reg.refresh(force=True)
    for path in reg.paths():
        print(path, reg.target(path))
    return 0


def main(argv=None) -> int:
    args = build_arg_parser().parse_args(argv)
    dests = ([parse_destination(x) for x in args.telemetry]
             if args.telemetry else None)
    if args.list_params:
        return list_params()
    d = Daemon(config_path=args.config, dry_run=args.dry_run,
               telemetry_destinations=dests,
               forward_control=args.forward_control,
               teensy_port=args.teensy_port, web_host=args.web_host,
               web_port=args.web_port, csound_host=args.csound_host,
               csound_port=args.csound_port, telemetry_host=args.telemetry_host,
               telemetry_port=args.telemetry_port,
               enable_teensy=not args.no_teensy, enable_web=not args.no_web,
               enable_osc_in=args.osc_in, pool_host=args.pool_host,
               pool_port=args.pool_port, enable_midi=args.midi,
               midi_port=args.midi_port, midi_snapshot=args.midi_snapshot,
               monitor_conf=args.monitor_conf, serial_auto_ports=args.serial_auto,
               osc_allow_hosts=args.osc_allow_host,
               midi_neptr=args.midi_neptr,
               midi_allow=[tuple(int(x) for x in a.split(":", 1)) for a in args.midi_allow],
               arcade_hold_s=args.arcade_hold,
               telemetry_params=args.telemetry_params)
    try:
        d.run_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
