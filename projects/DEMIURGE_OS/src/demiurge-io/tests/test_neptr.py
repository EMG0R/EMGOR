"""NEPTR on the parameter pool: behaviour parity, arcade roles, USS slot.

Everything here is offline: loopback UDP on EPHEMERAL ports only (never 7770/8002/
9100/9102), no serial, no ALSA, no audio, no service. Plain unittest:

    python3 -m unittest discover -s tests -p 'test_neptr.py'

What is proven (docs/NEPTR_POOL_BENCH.md, "parity"):
  A. sensors.json maps  ==  the same maps as a patch-map file (pool names, param targets):
     identical bytes on the csound wire for the same readings.
  B. TODAY's real sensors.json (maps: []) + the NEPTR default patch map writes NOTHING new to
     csound from any sensor/button frame: exactly today's behaviour.
  C. The pedal: the pool path produces the same `/cc 1 29 <v>` the engine's own rewrite
     produces, for every wire message (the python twin is also checked against the .orc text).
  D. Tap tempo: arcade press -> OSC /transport/tap (1, age) with the age serial_bridge.py
     computes for the same dwell.
  E. Arcade roles (tap <-> USS trig, long hold), button priming, encoders never sources,
     the demiurge-uss slot tool.
"""
import importlib.util
import json
import os
import random
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

from demiurge_io.arcade import ArcadeRouter, HOLD_S
from demiurge_io.config import normalize_maps, save_patch_map
from demiurge_io.daemon import Daemon, source_aliases
from demiurge_io.model import Reading
from demiurge_io.osc import decode_packet
from demiurge_io.pool import ParamRegistry
from demiurge_io.sinks.csound import CsoundSink
from demiurge_io.sinks.osc import OscSink
from demiurge_io.sinks.transport import TransportSink, tap_age_ms
from demiurge_io.sources.midi import MidiSource, cc_to_reading, neptr_pedal_normalize
from demiurge_io.sources.teensy import (ControlState, SENSOR_DESCRIPTORS, parse_frame,
                                        POOL_BUTTONS, POOL_PREFIX)

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
DEFAULT_MAP = REPO / "neptr" / "pool" / "neptr_default.map.json"
LIVE_SENSORS = HERE / "fixtures" / "sensors.live-2026-10-07.json"
USS_MANIFEST = REPO / "uss" / "instruments" / "supersaw" / "params" / "uss-supersaw.json"
EXPR = {"targets": [
    {"id": 22, "label": "wah", "writes": ["gkWAH_freq"]},
    {"id": 30, "label": "vol", "writes": ["gkMasterVol"]},
    {"id": 31, "label": "od", "writes": ["gkOD_drive"]},
    {"id": 20, "label": "bbcut", "writes": ["gkBBcut_expr"]},
]}


class Wire:
    """A loopback UDP receiver standing in for an engine port. A reader thread keeps the
    kernel buffer empty (a few hundred datagrams in a burst would otherwise be dropped)."""
    def __init__(self):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.settimeout(0.05)
        self.port = self.s.getsockname()[1]
        self._got, self._lock, self._stop = [], threading.Lock(), False
        self._th = threading.Thread(target=self._run, daemon=True)
        self._th.start()

    def _run(self):
        while not self._stop:
            try:
                data, _ = self.s.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            with self._lock:
                self._got.extend(decode_packet(data))

    def drain(self):
        time.sleep(0.12)                      # loopback delivery + reader wake-up
        with self._lock:
            out, self._got = self._got, []
        return out

    def close(self):
        self._stop = True
        self._th.join(1)
        self.s.close()


class Rig:
    """A real Daemon (no sources started) whose csound / osc / transport sinks all point at
    throwaway loopback receivers. `t` is the fake clock the sinks and the daemon share."""

    def __init__(self, sensors, patch_maps=None, patch="neptr_gorcore", params=None):
        self.root = Path(tempfile.mkdtemp())
        (self.root / "live.conf").write_text(f"# set: {patch}\n")
        (self.root / "sensors.json").write_text(json.dumps(sensors))
        if patch_maps is not None:
            save_patch_map(patch, patch_maps["maps"], patch_maps.get("values"), self.root)
        (self.root / "params").mkdir()
        for name, doc in (params or {}).items():
            (self.root / "params" / name).write_text(json.dumps(doc))
        et = self.root / "expr_targets.json"
        et.write_text(json.dumps(EXPR))
        self.cs, self.uss, self.clock = Wire(), Wire(), Wire()
        self.t = [1000.0]
        telem = Wire()
        self.telem = telem
        self.d = Daemon(config_path=self.root / "sensors.json", dry_run=False,
                        telemetry_destinations=[("127.0.0.1", telem.port)],
                        enable_teensy=False, enable_web=False, root=self.root,
                        params_dir=self.root / "params", expr_targets=et,
                        csound_port=self.cs.port)
        self.d.pool.add_sink("osc", OscSink(clock=lambda: self.t[0]))
        self.d.pool.add_sink("transport", TransportSink(("127.0.0.1", self.clock.port),
                                                        clock=lambda: self.t[0]))

    def feed(self, source_id, raw, t=None):
        t = self.t[0] if t is None else t
        self.d._process_reading(Reading(source_id, raw, t))

    def reload(self):
        for p in (self.root / "params").glob("*.json"):
            os.utime(p, (self.t[0] + 5, self.t[0] + 5))
        self.d.registry.refresh(force=True)
        self.d._reload_maps()
        self.d._maps = self.d.pool.registry.enforce_hard(
            __import__("demiurge_io.config", fromlist=["merge_maps"]).merge_maps(
                self.d.watcher.config.maps, self.d.patchmaps.maps))
        self.d._arcade_event(self.d.arcade.set_uss_active(self.d._uss_active()))

    def close(self):
        for w in (self.cs, self.uss, self.clock, self.telem):
            w.close()
        shutil.rmtree(self.root, ignore_errors=True)


def random_frames(n=400, seed=7):
    """Sensor frames in raw units across each descriptor's range, LiDAR dropouts included."""
    rnd = random.Random(seed)
    frames = []
    for i in range(n):
        f = {}
        for sid, d in SENSOR_DESCRIPTORS.items():
            lo, hi = d.usable
            f[sid] = rnd.uniform(lo, hi)
            if d.dropout_value is not None and rnd.random() < 0.05:
                f[sid] = d.dropout_value
        frames.append((1000.0 + i * 0.01, f))
    return frames


# ---------------------------------------------------------------------------------------------
class A_SensorMapsEqualPatchMaps(unittest.TestCase):
    """Today's sensors.json maps vs the same maps in a patch-map file, pool names + param targets."""

    LEGACY = [
        {"id": "wah", "source": "tof1", "zone_lo": 0.1, "zone_hi": 0.9, "curve": "exp",
         "outside": "hold", "target": {"sink": "csound", "kind": "expr", "id": 22}},
        {"id": "vol", "source": "jx", "invert": True, "slew_ms": 40,
         "target": {"sink": "csound", "kind": "expr", "id": 30}},
        {"id": "ped", "source": "fsr1", "out_lo": 0, "out_hi": 127,
         "target": {"sink": "csound", "kind": "cc", "ch": 1, "cc": 29}},
        {"id": "od", "source": "fsr2", "mode": "gate",
         "target": {"sink": "csound", "kind": "expr", "id": 31}},
        {"id": "bb", "source": "ml", "mode": "trigger", "combine": "max",
         "target": {"sink": "csound", "kind": "expr", "id": 20}},
        {"id": "vol2", "source": "jy", "combine": "add", "out_hi": 0.5,
         "target": {"sink": "csound", "kind": "expr", "id": 30}},
    ]
    PARAM_TARGET = {22: "/csound/gkWAH_freq", 30: "/csound/gkMasterVol",
                    31: "/csound/gkOD_drive", 20: "/csound/gkBBcut_expr"}

    def pool_version(self):
        out = []
        for m in self.LEGACY:
            m = dict(m)
            m["source"] = POOL_PREFIX + m["source"]
            t = m["target"]
            if t["kind"] == "expr":
                m["target"] = {"param": self.PARAM_TARGET[t["id"]]}
            else:                                   # the pedal CC -> the registered pool param
                m["target"] = {"param": "/csound/neptr/expr"}
                m["out_lo"], m["out_hi"] = 0.0, 1.0
            out.append(m)
        return out

    def run_frames(self, rig):
        for t, f in random_frames():
            for sid, raw in f.items():
                rig.feed(sid, raw, t)
        return rig.cs.drain()

    def test_identical_csound_wire(self):
        live = json.loads(LIVE_SENSORS.read_text())
        a = Rig({"sources": live["sources"], "maps": self.LEGACY})
        b = Rig({"sources": live["sources"], "maps": []},
                {"maps": self.pool_version()})
        try:
            wa, wb = self.run_frames(a), self.run_frames(b)
        finally:
            a.close(); b.close()
        self.assertGreater(len(wa), 500)                      # the maps really did drive csound
        self.assertEqual(wa, wb)                              # ...and byte-identically
        verbs = {addr for addr, _ in wa}
        self.assertEqual(verbs, {"/p", "/cc"})                # both csound verbs were exercised

    def test_aliases_name_both_spellings(self):
        for sid in SENSOR_DESCRIPTORS:
            self.assertEqual(source_aliases(sid), (sid, POOL_PREFIX + sid))
        self.assertEqual(source_aliases("/hw/midi/1/29"), ("/hw/midi/1/29",))


class B_TodayStaysToday(unittest.TestCase):
    """The live sensors.json has maps: [] — nothing from any sensor/button reaches csound.
    The NEPTR default map must keep it that way."""

    def test_live_file_has_no_maps(self):
        self.assertEqual(json.loads(LIVE_SENSORS.read_text())["maps"], [])

    def test_default_map_adds_no_csound_write_from_sensors_or_buttons(self):
        live = json.loads(LIVE_SENSORS.read_text())
        dm = json.loads(DEFAULT_MAP.read_text())
        old = Rig(live)                                          # today: nothing mapped
        new = Rig(live, {"maps": dm["maps"]})                    # after: default map
        try:
            cs = ControlState()
            for t, f in random_frames(300, seed=11):
                frame = {k: 0 for k in POOL_BUTTONS}
                frame.update({k: int(v) for k, v in f.items()})
                frame["b1"] = int(random.Random(int(t * 1000)).random() < 0.2)
                for rig in (old, new):
                    for sid, raw in f.items():
                        rig.feed(sid, raw, t)
                for r in cs.frame_to_button_readings(frame, t):
                    for rig in (old, new):
                        rig.feed(r.source_id, r.raw, t)
            self.assertEqual(old.cs.drain(), [])
            self.assertEqual(new.cs.drain(), [])                # tap goes to the CLOCK, not csound
        finally:
            old.close(); new.close()

    def test_default_map_is_valid_and_every_target_resolves(self):
        dm = json.loads(DEFAULT_MAP.read_text())
        maps = normalize_maps(dm["maps"])
        self.assertEqual(len(maps), len(dm["maps"]))
        reg = ParamRegistry(Path(tempfile.mkdtemp()), builtins=True)
        reg.refresh(force=True)
        for m in maps:
            self.assertIsNotNone(reg.target(m["target"]["param"]), m["id"])
            self.assertNotRegex(m["source"], r"/e[b]?\d$")      # no encoder is ever a source


class C_PedalParity(unittest.TestCase):
    ORC = REPO / "neptr" / "csound" / "midi_handler.orc"

    @staticmethod
    def engine_cc29(ch, cc, val):
        """What midi_handler.orc does to a hardware MIDI CC, in its order, as far as CC29 goes:
        demote ch1 CC29 to CC30 (OSC-origin exempt), THEN funnel ch7 CC14/15 into ch1 CC29."""
        if ch == 1 and cc == 29:
            cc = 30
        if ch == 7 and cc in (14, 15):
            ch, cc = 1, 29
        return val if (ch == 1 and cc == 29) else None

    def test_python_twin_matches_the_orc_text(self):
        s = self.ORC.read_text()
        self.assertRegex(s, r"kFromOsc == 0 && kSt >= 176 && kSt < 192 && kCh == 1 && kCC == 29\) then\s*\n\s*kCC = 30")
        self.assertRegex(s, r"kCh == 7 && \(kCC == 14 \|\| kCC == 15\)\) then\s*\n\s*kCh = 1\s*\n\s*kCC = 29")
        self.assertIn("#ifndef PEDAL_VIA_POOL", s)               # the single-mechanism switch exists
        # the demotion is textually BEFORE the funnel (order is load-bearing)
        self.assertLess(s.index("kCC = 30"), s.index("#ifndef PEDAL_VIA_POOL"))

    def test_every_wire_message_gives_the_same_cc29_write(self):
        dm = json.loads(DEFAULT_MAP.read_text())
        rig = Rig({"sources": [], "maps": []}, {"maps": dm["maps"]})
        src = MidiSource("Through", normalize=neptr_pedal_normalize, allow={(1, 29)})
        want = []
        try:
            for ch in (1, 2, 7, 10, 16):
                for cc in (13, 14, 15, 16, 20, 29, 30, 60, 102):
                    for val in range(128):
                        w = self.engine_cc29(ch, cc, val)       # the engine's own rewrite
                        if w is not None:
                            want.append([1, 29, w])
                        pair = src.accepts(ch, cc)               # the pool's source side
                        if pair:
                            r = cc_to_reading(*pair, val, 1.0)
                            if r:
                                rig.feed(r.source_id, r.raw)
            got = [a for _, a in rig.cs.drain()]
            self.assertEqual(len(want), 256)                      # ch7 CC14 + CC15, 128 values each
            self.assertEqual(got, want)                          # same writes, same order, every value
        finally:
            rig.close()

    def test_soft_takeover_inputs_unchanged_by_osc_origin(self):
        """The pool delivers the pedal as OSC /cc (kFromOsc=1): exempt from the demotion, same
        downstream dispatch as the funnel's own output. Anything else would fight the treadle."""
        self.assertIn("kFromOsc EXEMPTS the OSC path", self.ORC.read_text())


class D_TapParity(unittest.TestCase):
    def test_age_formula_equals_serial_bridge(self):
        spec = importlib.util.spec_from_file_location(
            "serial_bridge_ref", REPO / "neptr" / "csound" / "serial_bridge.py")
        try:
            bridge = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(bridge)
        except ImportError:
            self.skipTest("serial_bridge needs mido")
        self.assertEqual((bridge.TAP_BUTTON, bridge.TAP_CC, bridge.TAP_CHANNEL), ("b1", 113, 15))
        for dwell_ms in (0, 1, 2, 4.4, 6, 12, 40, 120, 500):
            t_frame, now = 100.0, 100.0 + dwell_ms / 1000.0
            with mock.patch.object(bridge.time, "monotonic", lambda: now):
                want = bridge.tap_age_ms(t_frame)
            self.assertEqual(tap_age_ms(t_frame, now), want, dwell_ms)

    def test_arcade_press_sends_one_neptr_tap_to_the_clock(self):
        dm = json.loads(DEFAULT_MAP.read_text())
        rig = Rig({"sources": [], "maps": []}, {"maps": dm["maps"]})
        try:
            cs = ControlState()
            frames = [{"b1": 0}, {"b1": 0}, {"b1": 1}, {"b1": 1}, {"b1": 1}, {"b1": 0}]
            rig.t[0] = 50.0
            for i, fr in enumerate(frames):
                for r in cs.frame_to_button_readings(fr, 50.0 + i * 0.01 - 0.007 * (i == 2)):
                    rig.t[0] = 50.0 + i * 0.01 + 0.002          # dispatch 2 ms after the frame
                    rig.feed(r.source_id, r.raw, r.t)
            msgs = rig.clock.drain()
            self.assertEqual(len(msgs), 1)                       # press edge only, never release/hold
            addr, args = msgs[0]
            self.assertEqual(addr, "/transport/tap")
            self.assertEqual(args[0], 1)                         # source = NEPTR
            self.assertEqual(args[1], float(tap_age_ms(50.0 + 2 * 0.01 - 0.007, 50.0 + 2 * 0.01 + 0.002)))
            self.assertEqual(rig.cs.drain(), [])                 # csound never sees it
        finally:
            rig.close()


class E_ButtonSources(unittest.TestCase):
    def test_held_at_connect_is_blocked_until_released(self):
        cs = ControlState()
        first = cs.frame_to_button_readings({"b1": 1, "b2": 0}, 1.0)
        self.assertNotIn(POOL_PREFIX + "b1", [r.source_id for r in first])
        self.assertIn(POOL_PREFIX + "b2", [r.source_id for r in first])    # baseline 0 for released
        self.assertEqual(cs.frame_to_button_readings({"b1": 1}, 1.01), [])  # still held: silent
        self.assertEqual([(r.source_id, r.raw) for r in cs.frame_to_button_readings({"b1": 0}, 1.02)
                          if r.source_id.endswith("b1")], [(POOL_PREFIX + "b1", 0)])
        self.assertEqual([(r.source_id, r.raw) for r in cs.frame_to_button_readings({"b1": 1}, 1.03)
                          if r.source_id.endswith("b1")], [(POOL_PREFIX + "b1", 1)])

    def test_edge_only(self):
        cs = ControlState()
        cs.frame_to_button_readings({}, 0.0)
        for _ in range(50):
            self.assertEqual(cs.frame_to_button_readings({"b3": 0}, 0.1), [])

    def test_encoders_and_encoder_buttons_are_never_pool_sources(self):
        cs = ControlState()
        every = {f"e{i}": 3 for i in range(1, 8)}
        every.update({f"eb{i}": 1 for i in range(1, 8)})
        every.update({f"b{i}": 1 for i in range(1, 10)})
        every["jb"] = 1
        cs.frame_to_button_readings({}, 0.0)
        ids = [r.source_id for r in cs.frame_to_button_readings(every, 0.01)]
        self.assertEqual(sorted(ids), sorted(POOL_PREFIX + b for b in POOL_BUTTONS))
        self.assertFalse([i for i in ids if re.search(r"/e\d|/eb\d", i)])


class F_ArcadeRoles(unittest.TestCase):
    def test_state_machine(self):
        a = ArcadeRouter()
        self.assertEqual(a.mode, "tap")
        a.on_edge(True, 0.0)
        self.assertIsNone(a.tick(10.0))                        # no USS: a long hold is just a long tap
        a.on_edge(False, 10.0)
        self.assertEqual(a.set_uss_active(True), "trig")
        a.on_edge(True, 20.0)
        self.assertIsNone(a.tick(21.9))
        self.assertEqual(a.tick(22.0), "tap")                  # long hold: trig -> tap
        self.assertIsNone(a.tick(30.0))                        # one flip per hold
        a.on_edge(False, 30.1)
        a.on_edge(True, 40.0)
        self.assertEqual(a.tick(42.5), "trig")                 # and back
        self.assertEqual(a.set_uss_active(False), "tap")       # USS leaves: always tap
        self.assertEqual(HOLD_S, 2.0)

    def test_full_scenario_with_the_real_manifest(self):
        dm = json.loads(DEFAULT_MAP.read_text())
        man = json.loads(USS_MANIFEST.read_text())
        man["port"] = None                                      # patched below to the receiver port
        rig = Rig({"sources": [], "maps": []}, {"maps": dm["maps"]})
        man["port"] = rig.uss.port
        trig = "/uss-supersaw/trig"
        try:
            d = rig.d
            B = POOL_PREFIX + "b1"

            def press(t, down):
                rig.t[0] = t
                rig.feed(B, 1 if down else 0, t)

            def tick(t):
                rig.t[0] = t
                d._arcade_event(d.arcade.tick(t))

            def uss():
                return [(a, g) for a, g in rig.uss.drain()]

            press(0.0, False)
            # 1. no USS: press = tap, nothing to the instrument
            press(1.0, True); press(1.1, False)
            self.assertEqual([a for a, _ in rig.clock.drain()], ["/transport/tap"])
            self.assertEqual(uss(), [])
            # 2. USS appears (manifest installed): press = trig gate, no tap
            (rig.root / "params" / "uss-supersaw.json").write_text(json.dumps(man))
            rig.t[0] = 1.5; rig.reload()
            self.assertEqual(d.arcade.mode, "trig")
            press(2.0, True); press(2.2, False)
            self.assertEqual(uss(), [("/p", [trig, 1.0]), ("/p", [trig, 0.0])])
            self.assertEqual(rig.clock.drain(), [])
            # 3. long hold: gate released, role back to tap; release is silent
            press(3.0, True)
            self.assertEqual(uss(), [("/p", [trig, 1.0])])
            tick(4.9); self.assertEqual(d.arcade.mode, "trig")
            tick(5.0)
            self.assertEqual((d.arcade.mode, uss()), ("tap", [("/p", [trig, 0.0])]))
            press(5.2, False)
            self.assertEqual((uss(), rig.clock.drain()), ([], []))
            press(6.0, True); press(6.1, False)                 # now a tap again, USS still active
            self.assertEqual([a for a, _ in rig.clock.drain()], ["/transport/tap"])
            self.assertEqual(uss(), [])
            # 4. USS leaves: hard maps gone, tap forever
            (rig.root / "params" / "uss-supersaw.json").unlink()
            rig.t[0] = 7.0; rig.reload()
            self.assertEqual(d.arcade.mode, "tap")
            self.assertFalse(d._uss_active())
        finally:
            rig.close()

    def test_lockin_declares_a_longer_hold_than_its_own_5s_gesture(self):
        man = json.loads((REPO / "uss/instruments/lockin/params/uss-lockin.json").read_text())
        b1 = [m for m in man["hard_maps"] if m["source"] == POOL_PREFIX + "b1"][0]
        self.assertEqual(b1["arcade"], "trig")
        self.assertGreater(b1["arcade_hold_s"], 5.0)

    def test_every_uss_manifest_tags_b1_as_the_trig_role(self):
        for f in (REPO / "uss" / "instruments").glob("*/params/uss-*.json"):
            man = json.loads(f.read_text())
            b1 = [m for m in man["hard_maps"] if m["source"] == POOL_PREFIX + "b1"]
            self.assertEqual([m.get("arcade") for m in b1], ["trig"], f.name)


class G_CsoundNormAdapter(unittest.TestCase):
    def test_pool_param_cc_is_scaled_last_inch_legacy_untouched(self):
        w = Wire()
        try:
            sink = CsoundSink("127.0.0.1", w.port)
            for v in range(128):
                sink.send({"sink": "csound", "kind": "cc", "ch": 1, "cc": 29, "norm": True}, v / 127.0)
                sink.send({"sink": "csound", "kind": "cc", "ch": 1, "cc": 29}, float(v))
            got = [a for _, a in w.drain()]
            self.assertEqual(got, [[1, 29, v] for v in range(128) for _ in (0, 1)])
        finally:
            w.close()


class H_UssSlotTool(unittest.TestCase):
    """src/wrappers/demiurge-uss against a temp ~/demiurge: live.conf edit, manifest, link, guards."""
    TOOL = REPO / "src" / "wrappers" / "demiurge-uss"

    def setUp(self):
        self.r = Path(tempfile.mkdtemp())
        shutil.copytree(REPO / "uss" / "instruments", self.r / "uss" / "instruments")
        self.live = self.r / "live.conf"
        self.live.write_text("# set: neptr_gorcore\nsync_layer = on\ncsound_extra = --omacro:PRESET_BANK=2\n"
                             "chain =\n  ~/NEPTR_phase4/csound/neptrPhase4.csd\n")
        self.orig = self.live.read_text()
        fake = self.r / "launcher"
        fake.write_bytes(b"...DEMIURGE_PARAM_PORT...")
        self.env = dict(os.environ, DEMIURGE_ROOT=str(self.r), DEMIURGE_LAUNCHER_BIN=str(fake),
                        PYTHONDONTWRITEBYTECODE="1")

    def tearDown(self):
        shutil.rmtree(self.r, ignore_errors=True)

    def run_tool(self, *a):
        return subprocess.run([sys.executable, str(self.TOOL), *a], env=self.env,
                              capture_output=True, text=True)

    def test_switch_one_at_a_time_and_off_restores_the_file(self):
        p = self.run_tool("supersaw")
        self.assertEqual(p.returncode, 0, p.stderr)
        t = self.live.read_text()
        self.assertIn("# USS-SLOT-BEGIN", t)
        self.assertEqual(t.count("chain ="), 2)                  # NEPTR chain + the USS chain
        self.assertIn("neptrPhase4.csd", t)                      # NEPTR chain untouched
        self.assertEqual([x.name for x in (self.r / "params").glob("uss-*.json")], ["uss-supersaw.json"])
        link = self.r / "uss" / "active" / "uss-supersaw.ck"
        self.assertTrue(link.is_symlink() and link.resolve().name == "supersaw.ck")
        p = self.run_tool("lockin")                              # switch: replaces, never accumulates
        self.assertEqual(p.returncode, 0, p.stderr)
        t = self.live.read_text()
        self.assertEqual(t.count("chain ="), 2)
        self.assertIn("uss-lockin.ck", t); self.assertNotIn("uss-supersaw", t)
        self.assertEqual([x.name for x in (self.r / "params").glob("uss-*.json")], ["uss-lockin.json"])
        self.assertEqual([x.name for x in (self.r / "uss" / "active").glob("uss-*")], ["uss-lockin.ck"])
        p = self.run_tool("off")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(self.live.read_text().strip(), self.orig.strip())
        self.assertEqual(list((self.r / "params").glob("uss-*.json")), [])
        self.assertTrue(list(self.r.glob("live.conf.bak-uss-*")))     # backup-first

    def test_dry_run_changes_nothing(self):
        p = self.run_tool("supersaw", "--dry-run")
        self.assertEqual(p.returncode, 0)
        self.assertEqual(self.live.read_text(), self.orig)
        self.assertFalse((self.r / "params").exists() and list((self.r / "params").glob("*")))

    def test_refuses_sync_layer_off_and_old_launcher_and_unknown(self):
        self.live.write_text(self.orig.replace("sync_layer = on", "sync_layer = off"))
        self.assertEqual(self.run_tool("supersaw").returncode, 2)
        self.live.write_text(self.orig)
        Path(self.env["DEMIURGE_LAUNCHER_BIN"]).write_bytes(b"old launcher")
        self.assertEqual(self.run_tool("supersaw").returncode, 2)
        self.assertEqual(self.run_tool("supersaw", "--force").returncode, 0)
        self.assertEqual(self.run_tool("nosuch").returncode, 2)

    def test_launcher_parses_the_block_as_a_second_parallel_chain(self):
        """The same live.conf text, through the launcher's own parser rules (live.rs): comment
        lines never end/open a chain, `chain =` opens a new parallel one. Checked textually
        here and by `cargo test` for the parser itself."""
        self.run_tool("supersaw")
        chains, cur = [], None
        for raw in self.live.read_text().splitlines():
            line = raw.split("#", 1)[0]
            if not line.strip():
                continue
            if not line[0].isspace():
                cur = [] if line.split("=")[0].strip() == "chain" else None
                if cur is not None:
                    chains.append(cur)
            elif cur is not None:
                cur.append(line.strip())
        self.assertEqual(chains, [["~/NEPTR_phase4/csound/neptrPhase4.csd"],
                                  [str(self.r / "uss/active/uss-supersaw.ck")]])


if __name__ == "__main__":
    unittest.main(verbosity=1)
