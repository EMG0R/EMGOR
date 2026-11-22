"""Offline tests for the parameter-pool extension. Pure logic + temp dirs +
throwaway loopback ports ONLY: never the live sockets (7770/8002/8003/9102),
never serial, never ALSA. Plain unittest so it runs without pytest:

    python3 -m unittest discover -s tests -p 'test_pool.py'
"""
import json
import os
import socket
import struct
import tempfile
import time
import unittest
from pathlib import Path

from demiurge_io.config import (PatchMapWatcher, active_patch, load_patch_map,
                                merge_maps, normalize_maps, parse_config,
                                patch_map_path, save_patch_map)
from demiurge_io.osc import decode_osc, decode_packet, encode_osc
from demiurge_io.pool import ParamRegistry, Pool, write_manifest
from demiurge_io.sinks import Coalescer
from demiurge_io.sinks.midi import MidiSink, is_blocked
from demiurge_io.sinks.osc import OscSink, denormalize
from demiurge_io.sources.midi import (Snapshot, cc_to_reading, load_monitor_conf,
                                      pick_port)
from demiurge_io.sources.osc_in import parse_pool_packet
from demiurge_io.sources.serial_auto import SerialAutoSource, parse_hello
from demiurge_io.model import Reading

EXPR = {"targets": [
    {"id": 30, "label": "", "writes": ["gkMasterVol"]},
    {"id": 20, "label": "", "writes": ["gkBBcut_expr"]},
    {"id": 99, "label": "", "writes": ["gkMasterVol"]},     # duplicate slug
]}


class Rec:
    """Sink stand-in that records instead of sending."""
    def __init__(self):
        self.sent = []
    def send(self, target, value):
        self.sent.append((target, value))


class OscDecode(unittest.TestCase):
    def test_roundtrip_pool_write(self):
        pkt = encode_osc("/p", "/sc-pad/cutoff", 0.5)
        self.assertEqual(decode_osc(pkt), ("/p", ["/sc-pad/cutoff", 0.5]))
        self.assertEqual(parse_pool_packet(pkt), [("/sc-pad/cutoff", 0.5)])

    def test_int_value_accepted(self):
        self.assertEqual(parse_pool_packet(encode_osc("/p", "/x/y", 1)), [("/x/y", 1.0)])

    def test_csound_style_int_id_ignored(self):
        self.assertEqual(parse_pool_packet(encode_osc("/p", 30, 0.5)), [])

    def test_wrong_address_and_arity_ignored(self):
        self.assertEqual(parse_pool_packet(encode_osc("/q", "/a/b", 0.5)), [])
        self.assertEqual(parse_pool_packet(encode_osc("/p", "/a/b")), [])

    def test_nan_inf_rejected(self):
        self.assertEqual(parse_pool_packet(encode_osc("/p", "/a/b", float("nan"))), [])
        self.assertEqual(parse_pool_packet(encode_osc("/p", "/a/b", float("inf"))), [])

    def test_path_must_be_absolute(self):
        self.assertEqual(parse_pool_packet(encode_osc("/p", "cutoff", 0.1)), [])

    def test_garbage_never_raises(self):
        for junk in (b"", b"\x00\x01", b"/p\x00\x00,s\x00\x00", b"/p\x00\x00,sf\x00ab", b"#bundle\x00"):
            self.assertEqual(parse_pool_packet(junk), [])

    def test_bundle(self):
        a, b = encode_osc("/p", "/a/x", 0.1), encode_osc("/p", "/a/y", 0.2)
        bundle = b"#bundle\x00" + b"\x00" * 8
        for m in (a, b):
            bundle += struct.pack(">i", len(m)) + m
        self.assertEqual([p for p, _ in parse_pool_packet(bundle)], ["/a/x", "/a/y"])

    def test_double_and_bool_tags(self):
        pkt = (b"/p\x00\x00" + b",sd\x00" + b"/a/b\x00\x00\x00\x00" + struct.pack(">d", 0.25))
        self.assertEqual(parse_pool_packet(pkt), [("/a/b", 0.25)])

    def test_real_udp_loopback_nonlive_port(self):
        rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rx.bind(("127.0.0.1", 0))                 # ephemeral, never 9102
        rx.settimeout(2)
        tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        tx.sendto(encode_osc("/p", "/a/b", 0.75), rx.getsockname())
        data, _ = rx.recvfrom(4096)
        self.assertEqual(parse_pool_packet(data), [("/a/b", 0.75)])
        rx.close(); tx.close()


class Registry(unittest.TestCase):
    def test_csound_synthesized_from_expr_targets(self):
        r = ParamRegistry(Path(tempfile.mkdtemp()))
        self.assertEqual(r.load_expr_targets(EXPR), 3)
        self.assertEqual(r.target("/csound/gkMasterVol"),
                         {"sink": "csound", "kind": "expr", "id": 30})
        self.assertEqual(r.target("/csound/gkMasterVol_99")["id"], 99)
        self.assertEqual(r.target("/csound/gkBBcut_expr")["id"], 20)

    def test_manifest_write_and_load(self):
        d = Path(tempfile.mkdtemp())
        params = [{"path": "/sc-pad/cutoff", "label": "Cutoff", "default": 0.5,
                   "address": "/cutoff",
                   "native": {"min": 40, "max": 12000, "curve": "exp", "unit": "Hz"}},
                  {"path": "/sc-pad/vol", "address": "/vol"},
                  {"path": "/other/bad", "address": "/bad"},       # wrong stage prefix
                  {"path": "/sc-pad/cc", "midi": {"ch": 2, "cc": 7}}]
        f = write_manifest("sc-pad", params, 9002, d)
        self.assertEqual(f, d / "sc-pad.json")
        doc = json.loads(f.read_text())
        self.assertEqual((doc["stage"], doc["port"], len(doc["params"])), ("sc-pad", 9002, 4))
        self.assertFalse(list(d.glob("*.tmp")))              # atomic: no stray tmp
        r = ParamRegistry(d)
        r.refresh(force=True)
        self.assertEqual(r.paths(), ["/sc-pad/cc", "/sc-pad/cutoff", "/sc-pad/vol"])
        t = r.target("/sc-pad/cutoff")
        self.assertEqual((t["sink"], t["port"], t["address"]), ("osc", 9002, "/cutoff"))
        self.assertEqual(t["native"]["unit"], "Hz")
        self.assertEqual(r.target("/sc-pad/cc"), {"sink": "midi", "ch": 2, "cc": 7})
        self.assertEqual(r.meta("/sc-pad/cutoff")["default"], 0.5)

    def test_no_csound_manifest(self):
        with self.assertRaises(ValueError):
            write_manifest("csound", [], 1, Path(tempfile.mkdtemp()))
        d = Path(tempfile.mkdtemp())
        (d / "csound.json").write_text(json.dumps(
            {"stage": "csound", "port": 1, "params": [{"path": "/csound/x", "address": "/x"}]}))
        r = ParamRegistry(d); r.refresh(force=True)
        self.assertEqual(r.paths(), [])                      # ignored: one csound list only

    def test_bad_manifest_ignored_and_hot_reload(self):
        d = Path(tempfile.mkdtemp())
        (d / "bad.json").write_text("{nope")
        et = d / "expr_targets.json"; et.write_text(json.dumps(EXPR))
        r = ParamRegistry(d / "params", et)
        (d / "params").mkdir()
        (d / "params" / "bad.json").write_text("{nope")
        self.assertTrue(r.refresh(force=True))
        self.assertEqual(len(r.paths()), 3)
        self.assertFalse(r.refresh())                        # unchanged -> no rebuild
        write_manifest("pd", [{"path": "/pd/a", "address": "/a"}], 9001, d / "params")
        self.assertTrue(r.refresh())
        self.assertIn("/pd/a", r.paths())


class NormToParam(unittest.TestCase):
    def setUp(self):
        self.reg = ParamRegistry(Path(tempfile.mkdtemp()))
        self.reg.load_expr_targets(EXPR)
        self.reg.register("/sc/cut", {"sink": "osc", "port": 9002, "address": "/cut"})
        self.pool = Pool(self.reg)
        self.cs, self.osc = Rec(), Rec()
        self.pool.add_sink("csound", self.cs)
        self.pool.add_sink("osc", self.osc)

    def test_param_target_resolves_to_csound_expr(self):
        self.assertTrue(self.pool.send_target({"param": "/csound/gkMasterVol"}, 0.4))
        self.assertEqual(self.cs.sent, [({"sink": "csound", "kind": "expr", "id": 30}, 0.4)])

    def test_param_value_clamped_to_unit(self):
        self.pool.publish("/sc/cut", 1.7)
        self.pool.publish("/sc/cut", -3)
        self.assertEqual([v for _, v in self.osc.sent], [1.0, 0.0])
        self.assertEqual(self.pool.values["/sc/cut"], 0.0)

    def test_legacy_target_passes_through_unclamped(self):
        t = {"sink": "csound", "kind": "cc", "ch": 1, "cc": 20}
        self.pool.send_target(t, 100.0)                      # kind=cc is 0..127 space
        self.assertEqual(self.cs.sent, [(t, 100.0)])

    def test_legacy_target_without_sink_key_defaults_csound(self):
        self.pool.send_target({"kind": "expr", "id": 5}, 0.1)
        self.assertEqual(len(self.cs.sent), 1)

    def test_unknown_param_and_sink_dropped_not_raised(self):
        self.assertFalse(self.pool.publish("/nope/x", 0.5))
        self.assertFalse(self.pool.send_target({"sink": "video", "x": 1}, 0.5))
        self.assertEqual(self.cs.sent + self.osc.sent, [])

    def test_subscribers_see_param_writes(self):
        seen = []
        self.pool.subscribe(lambda p, v: seen.append((p, v)))
        self.pool.publish("/sc/cut", 0.3)
        self.assertEqual(seen, [("/sc/cut", 0.3)])

    def test_router_to_param_end_to_end(self):
        """A real router.apply_map output drives a param target (zone+curve)."""
        from demiurge_io import router
        m = normalize_maps([{"id": "m", "source": "tof1", "zone_lo": 0.5, "zone_hi": 1.0,
                             "curve": "lin", "target": {"param": "/csound/gkBBcut_expr"}}])[0]
        res = router.apply_map(m, 0.75, True, router.MapState(), 0.0)
        self.pool.send_target(m["target"], res.value)
        self.assertAlmostEqual(self.cs.sent[0][1], 0.5)
        self.assertEqual(self.cs.sent[0][0]["id"], 20)


class PatchMaps(unittest.TestCase):
    def mkroot(self, header="neptr_gorcore"):
        root = Path(tempfile.mkdtemp())
        (root / "live.conf").write_text(f"# DEMIURGE\n# set: {header}\n[x]\n")
        return root

    MAP = {"id": "tof1-wah", "source": "/hw/teensy/tof1",
           "target": {"param": "/csound/gkMasterVol"}, "invert": True}

    def test_active_patch_header(self):
        root = self.mkroot("abc")
        self.assertEqual(active_patch(root / "live.conf"), "abc")
        self.assertIsNone(active_patch(root / "missing.conf"))

    def test_save_load_roundtrip(self):
        root = self.mkroot()
        maps = normalize_maps([self.MAP])
        p = save_patch_map("neptr_gorcore", maps, {"/csound/gkMasterVol": 0.8}, root)
        self.assertEqual(p, root / "patches" / "neptr_gorcore.map.json")
        self.assertFalse(list(p.parent.glob("*.tmp")))
        doc = json.loads(p.read_text())
        self.assertEqual((doc["version"], doc["patch"]), (1, "neptr_gorcore"))
        maps2, values = load_patch_map(p)
        self.assertEqual(maps2, maps)
        self.assertEqual(values, {"/csound/gkMasterVol": 0.8})
        self.assertTrue(maps2[0]["invert"])
        self.assertEqual(maps2[0]["curve"], "lin")           # existing router defaults applied

    def test_old_and_new_target_shapes_both_valid(self):
        maps = normalize_maps([
            {"id": "a", "source": "s", "target": {"sink": "csound", "kind": "expr", "id": 30}},
            {"id": "b", "source": "s", "target": {"param": "/x/y"}},
            {"id": "c", "source": "s"},                      # no target -> dropped
            {"id": "d", "source": "s", "target": "str"}])    # bad target -> dropped
        self.assertEqual([m["id"] for m in maps], ["a", "b"])
        cfg = parse_config({"sources": [], "maps": [{"id": "a", "source": "s",
                            "target": {"param": "/x/y"}}]})
        self.assertEqual(cfg.maps[0]["target"], {"param": "/x/y"})

    def test_merge_patch_overrides_by_id_in_place(self):
        g = normalize_maps([{"id": "a", "source": "s", "target": {"param": "/g/a"}},
                            {"id": "b", "source": "s", "target": {"param": "/g/b"}}])
        p = normalize_maps([{"id": "a", "source": "s", "target": {"param": "/p/a"}},
                            {"id": "c", "source": "s", "target": {"param": "/p/c"}}])
        out = merge_maps(g, p)
        self.assertEqual([(m["id"], m["target"]["param"]) for m in out],
                         [("a", "/p/a"), ("b", "/g/b"), ("c", "/p/c")])

    def test_watcher_follows_patch_and_falls_back(self):
        root = self.mkroot("one")
        w = PatchMapWatcher(root)
        self.assertEqual((w.patch, w.maps), ("one", []))     # no file -> fallback (empty)
        save_patch_map("one", normalize_maps([self.MAP]), {"/a/b": 0.2}, root)
        self.assertTrue(w.maybe_reload())
        self.assertEqual([m["id"] for m in w.maps], ["tof1-wah"])
        self.assertFalse(w.maybe_reload())
        # switch patch: other song's maps must not linger
        time.sleep(0.01)
        (root / "live.conf").write_text("# set: two\n")
        os.utime(root / "live.conf", (time.time() + 5, time.time() + 5))
        self.assertTrue(w.maybe_reload())
        self.assertEqual((w.patch, w.maps, w.patch_changed), ("two", [], True))
        self.assertFalse(w.maybe_reload())
        self.assertFalse(w.patch_changed)

    def test_watcher_bad_json_keeps_previous_same_patch(self):
        root = self.mkroot("one")
        save_patch_map("one", normalize_maps([self.MAP]), None, root)
        w = PatchMapWatcher(root)
        self.assertEqual(len(w.maps), 1)
        pm = patch_map_path("one", root)
        pm.write_text("{half")
        os.utime(pm, (time.time() + 5, time.time() + 5))
        self.assertFalse(w.maybe_reload())
        self.assertEqual(len(w.maps), 1)

    def test_patch_name_cannot_escape(self):
        with self.assertRaises(ValueError):
            patch_map_path("../evil", Path("/tmp/x"))


class Sinks(unittest.TestCase):
    def test_denormalize(self):
        self.assertEqual(denormalize(0.5, None), 0.5)
        self.assertAlmostEqual(denormalize(0.5, {"min": 0, "max": 10}), 5.0)
        self.assertAlmostEqual(denormalize(0.0, {"min": 40, "max": 12000, "curve": "exp"}), 40.0)
        self.assertAlmostEqual(denormalize(1.0, {"min": 40, "max": 12000, "curve": "exp"}), 12000.0)
        self.assertAlmostEqual(denormalize(2.0, {"min": 0, "max": 10}), 10.0)   # clamped

    def test_coalescer_keeps_latest(self):
        c = Coalescer(100.0)
        self.assertTrue(c.offer("k", 0.1, 0.000))
        self.assertFalse(c.offer("k", 0.2, 0.002))
        self.assertFalse(c.offer("k", 0.3, 0.004))
        self.assertEqual(c.drain(0.005), [])
        self.assertEqual(c.drain(0.011), [("k", 0.3)])
        self.assertEqual(c.drain(0.5), [])

    def test_osc_sink_sends_real_datagram_to_throwaway_port(self):
        rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rx.bind(("127.0.0.1", 0)); rx.settimeout(2)
        port = rx.getsockname()[1]
        s = OscSink(clock=lambda: 0.0)
        s.send({"sink": "osc", "port": port, "address": "/cutoff",
                "native": {"min": 0, "max": 100}}, 0.25)
        self.assertEqual(decode_osc(rx.recvfrom(4096)[0]), ("/cutoff", [25.0]))
        rx.close()

    def test_osc_sink_refuses_remote_unless_allowlisted(self):
        s = OscSink(clock=lambda: 0.0)
        sent = []
        s.sock = type("S", (), {"sendto": lambda self, p, a: sent.append(a)})()
        s.send({"host": "192.168.0.103", "port": 1, "address": "/x"}, 0.5)
        self.assertEqual(sent, [])
        s2 = OscSink(allow_hosts=["192.168.0.103"], clock=lambda: 0.0)
        s2.sock = s.sock
        s2.send({"host": "192.168.0.103", "port": 1, "address": "/x"}, 0.5)
        self.assertEqual(sent, [("192.168.0.103", 1)])

    def test_osc_sink_coalesces_then_flushes_last(self):
        now = [0.0]
        s = OscSink(max_hz=100, clock=lambda: now[0])
        sent = []
        s.sock = type("S", (), {"sendto": lambda self, p, a: sent.append(decode_osc(p))})()
        t = {"port": 1, "address": "/a"}
        s.send(t, 0.1); now[0] = 0.002
        s.send(t, 0.9)
        self.assertEqual(len(sent), 1)
        now[0] = 0.02; s.flush()
        self.assertEqual(sent[-1][1][0], struct.unpack(">f", struct.pack(">f", 0.9))[0])

    def test_midi_blocklist(self):
        self.assertTrue(is_blocked(16, 104) and is_blocked(16, 119) and is_blocked(1, 29))
        self.assertFalse(is_blocked(16, 120) or is_blocked(2, 29) or is_blocked(1, 30))

    def test_midi_sink_scales_and_blocks(self):
        class Port:
            def __init__(self): self.msgs = []
            def send(self, m): self.msgs.append(m)
        port = Port()
        s = MidiSink(opener=lambda hint: port, clock=lambda: 0.0)
        s.send({"sink": "midi", "ch": 3, "cc": 20}, 0.5)
        s.send({"sink": "midi", "ch": 1, "cc": 29}, 0.5)     # pedal: blocked
        s.send({"sink": "midi", "ch": 16, "cc": 110}, 1.0)   # clock: blocked
        self.assertEqual(len(port.msgs), 1)
        m = port.msgs[0]
        self.assertEqual((m.channel, m.control, m.value), (2, 20, 64))

    def test_midi_sink_open_failure_is_silent(self):
        def boom(h): raise OSError("no port")
        s = MidiSink(opener=boom, clock=lambda: 0.0)
        s.send({"ch": 1, "cc": 20}, 0.5)                     # must not raise


class Sources(unittest.TestCase):
    def test_midi_cc_to_reading(self):
        r = cc_to_reading(2, 30, 127, 1.0)
        self.assertEqual((r.source_id, r.raw), ("/hw/midi/2/30", 1.0))
        self.assertIsNone(cc_to_reading(16, 110, 5, 1.0))    # clock CC not a value

    def test_snapshot_matches_monitor_format(self):
        d = tempfile.mkdtemp()
        conf = os.path.join(d, "monitor.conf")
        open(conf, "w").write("interval = 0.1\ncenter64 = 27\n[map]\njstX: 27\nexp: 29\nenc1: 20\n")
        cmap, center, interval = load_monitor_conf(conf)
        self.assertEqual((cmap, center, interval), ([("jstX", 27), ("exp", 29), ("enc1", 20)], {27: 64}, 0.1))
        out = os.path.join(d, "snap")
        s = Snapshot(cmap, center, out, interval)
        s.update(29, 100)
        self.assertTrue(s.maybe_publish(1.0))
        self.assertEqual(open(out).read(), "jstX:64 exp:100 enc1:-\n")
        self.assertFalse(s.maybe_publish(1.05))
        self.assertFalse(os.path.exists(out + ".tmp"))

    def test_monitor_default_map_when_conf_missing(self):
        cmap, center, interval = load_monitor_conf("/nonexistent/monitor.conf")
        self.assertEqual(cmap[0], ("jstX", 27))
        self.assertEqual(center, {27: 64, 28: 64})

    def test_pick_port_order(self):
        self.assertEqual(pick_port(["Midi Through:0", "Teensy MIDI:0"]), "Teensy MIDI:0")
        self.assertIsNone(pick_port([]))

    def test_serial_hello_and_stream(self):
        self.assertEqual(parse_hello("@hello pad k1:knob:0:1023 b:button:0:1 bad"),
                         ("pad", {"k1": ("knob", 0.0, 1023.0), "b": ("button", 0.0, 1.0)}))
        self.assertIsNone(parse_hello("k1 5"))
        s = SerialAutoSource("/dev/null-test")
        self.assertIsNone(s.handle_line("@hello pad k1:knob:0:1023", 0.0))
        r = s.handle_line("k1 512", 1.0)
        self.assertEqual((r.source_id, r.raw), ("/hw/pad/k1", 512.0))
        self.assertEqual(s.calibration("/hw/pad/k1"), {"in_lo": 0.0, "in_hi": 1023.0})
        self.assertEqual(s.uncalibrated(), [])

    def test_serial_no_handshake_autoranges(self):
        s = SerialAutoSource("/dev/ttyTEST9")
        s.handle_line("a 10", 0.0); s.handle_line("a 30", 0.1); s.handle_line("junk", 0.2)
        self.assertEqual(s.calibration("/hw/ttyTEST9/a"), {"in_lo": 10.0, "in_hi": 30.0})
        self.assertEqual(s.uncalibrated(), ["/hw/ttyTEST9/a"])


class DaemonWiring(unittest.TestCase):
    """The real Daemon, in-process: no sources started, telemetry aimed at a
    throwaway port, csound/osc/midi sinks replaced by recorders."""

    def build(self, sensors, patch_maps=None, patch="song"):
        from demiurge_io.daemon import Daemon
        root = Path(tempfile.mkdtemp())
        (root / "live.conf").write_text(f"# set: {patch}\n")
        (root / "sensors.json").write_text(json.dumps(sensors))
        if patch_maps is not None:
            save_patch_map(patch, patch_maps["maps"], patch_maps.get("values"), root)
        et = root / "expr_targets.json"; et.write_text(json.dumps(EXPR))
        d = Daemon(config_path=root / "sensors.json", dry_run=True,
                   telemetry_destinations=[("127.0.0.1", 59999)],
                   enable_teensy=False, enable_web=False, root=root,
                   params_dir=root / "params", expr_targets=et)
        d.cs, d.osc = Rec(), Rec()
        d.pool.add_sink("csound", d.cs); d.pool.add_sink("osc", d.osc)
        d.sink = d.cs
        return d, root

    SRC = {"id": "/hw/midi/1/30", "in_lo": 0.0, "in_hi": 1.0}

    def test_hw_source_without_calibration_entry_routes_via_patch_map(self):
        d, _ = self.build({"sources": [], "maps": []},
                          {"maps": [{"id": "m", "source": "/hw/midi/1/30",
                                     "target": {"param": "/csound/gkMasterVol"}}]})
        d._process_reading(Reading("/hw/midi/1/30", 0.5, 1.0))
        self.assertEqual(d.cs.sent, [({"sink": "csound", "kind": "expr", "id": 30}, 0.5)])

    def test_unknown_non_hw_source_still_telemetry_only(self):
        d, _ = self.build({"sources": [], "maps": [{"id": "m", "source": "weird",
                           "target": {"param": "/csound/gkMasterVol"}}]})
        d._process_reading(Reading("weird", 0.5, 1.0))
        self.assertEqual(d.cs.sent, [])                      # old behaviour preserved

    def test_legacy_sensors_json_map_unchanged(self):
        d, _ = self.build({"sources": [{"id": "tof1"}], "maps": [
            {"id": "old", "source": "tof1",
             "target": {"sink": "csound", "kind": "expr", "id": 30}}]})
        d._process_reading(Reading("tof1", 0.25, 1.0))
        self.assertEqual(d.cs.sent, [({"sink": "csound", "kind": "expr", "id": 30}, 0.25)])

    def test_patch_map_overrides_global_by_id(self):
        d, _ = self.build({"sources": [{"id": "tof1"}], "maps": [
            {"id": "m", "source": "tof1", "target": {"sink": "csound", "kind": "expr", "id": 30}}]},
            {"maps": [{"id": "m", "source": "tof1", "invert": True,
                       "target": {"param": "/csound/gkBBcut_expr"}}]})
        d._process_reading(Reading("tof1", 0.25, 1.0))
        self.assertEqual(d.cs.sent, [({"sink": "csound", "kind": "expr", "id": 20}, 0.75)])

    def test_param_item_and_baseline_values(self):
        d, _ = self.build({"sources": [], "maps": []},
                          {"maps": [], "values": {"/csound/gkMasterVol": 0.8}})
        d._apply_patch_values()
        self.assertEqual(d.cs.sent[-1][1], 0.8)
        from demiurge_io.daemon import _ParamItem
        d._process_param(_ParamItem("/csound/gkBBcut_expr", 0.3))
        self.assertEqual(d.cs.sent[-1], ({"sink": "csound", "kind": "expr", "id": 20}, 0.3))

    def test_patch_switch_hot_reload(self):
        d, root = self.build({"sources": [{"id": "tof1"}], "maps": []},
                             {"maps": [{"id": "m", "source": "tof1",
                                        "target": {"param": "/csound/gkMasterVol"}}]})
        self.assertEqual(len(d._maps), 1)
        (root / "live.conf").write_text("# set: other\n")
        os.utime(root / "live.conf", (time.time() + 5, time.time() + 5))
        d._reload_maps()
        self.assertEqual(d._maps, [])                        # back to global-only fallback


if __name__ == "__main__":
    unittest.main(verbosity=1)


class HardSoftAndPout(unittest.TestCase):
    def _reg(self):
        r = ParamRegistry(Path(tempfile.mkdtemp()))
        r.load_manifest({"stage": "sc", "port": 9002,
            "params": [{"path": "/sc/a", "address": "/a", "map": "hard"},
                       {"path": "/sc/b", "address": "/b"}],
            "hard_maps": [{"id": "h1", "source": "/hw/x/y", "target": {"param": "/sc/a"}}]})
        return r

    def test_flag_default_soft(self):
        r = self._reg()
        self.assertTrue(r.is_hard("/sc/a"))
        self.assertFalse(r.is_hard("/sc/b"))
        self.assertEqual(r.meta("/sc/b")["map"], "soft")

    def test_enforce_hard_drops_user_remap_and_appends_hard(self):
        r = self._reg()
        user = [{"id": "u1", "source": "/hw/q", "target": {"param": "/sc/a"}},
                {"id": "u2", "source": "/hw/q", "target": {"param": "/sc/b"}},
                {"id": "h1", "source": "/hw/z", "target": {"param": "/sc/b"}}]
        out = r.enforce_hard(user)
        self.assertEqual([m["id"] for m in out], ["u2", "h1"])
        self.assertEqual(out[-1]["source"], "/hw/x/y")

    def test_pout_echoes_but_never_hits_sink(self):
        r = self._reg()
        pool = Pool(r)
        rec = Rec(); pool.add_sink("osc", rec)
        seen = []
        pool.subscribe(lambda p, v: seen.append((p, v)))
        self.assertTrue(pool.publish_out("/sc/b", 1.5))
        self.assertEqual(rec.sent, [])
        self.assertEqual(seen, [("/sc/b", 1.0)])
        self.assertFalse(pool.publish_out("/nope/x", 0.1))
        self.assertFalse(pool.can_remap("/sc/a"))

    def test_pout_parse(self):
        pkt = encode_osc("/pout", "/sc/b", 0.25)
        self.assertEqual(parse_pool_packet(pkt), [])
        self.assertEqual(parse_pool_packet(pkt, "/pout"), [("/sc/b", 0.25)])


class HookProto(unittest.TestCase):
    """`"proto":"p"` params: no address, sink sends /p <path> <0..1>."""
    DOC = {"stage": "hk", "port": 18599, "params": [
        {"path": "/hk/cut", "proto": "p", "map": "hard", "native": {"min": 40, "max": 9000}},
        {"path": "/hk/old", "address": "/old", "native": {"min": 0, "max": 10}},
        {"path": "/hk/none"}]}

    def test_manifest_loads_hook_params(self):
        r = ParamRegistry(Path("/nonexistent"))
        self.assertEqual(r.load_manifest(self.DOC), 2)
        self.assertEqual(r.target("/hk/cut"),
                         {"sink": "osc", "host": "127.0.0.1", "port": 18599,
                          "proto": "p", "path": "/hk/cut"})
        self.assertEqual(r.target("/hk/old")["address"], "/old")
        self.assertIsNone(r.target("/hk/none"))

    def test_sink_sends_p_path_val_undenormalized(self):
        rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rx.bind(("127.0.0.1", 0)); rx.settimeout(2)
        port = rx.getsockname()[1]
        s = OscSink(clock=lambda: 0.0)
        s.send({"sink": "osc", "port": port, "proto": "p", "path": "/hk/cut",
                "native": {"min": 40, "max": 9000}}, 0.25)
        data, _ = rx.recvfrom(1024)
        rx.close()
        self.assertEqual(decode_osc(data), ("/p", ["/hk/cut", 0.25]))

    def test_sink_address_mode_unchanged(self):
        rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        rx.bind(("127.0.0.1", 0)); rx.settimeout(2)
        port = rx.getsockname()[1]
        s = OscSink(clock=lambda: 0.0)
        s.send({"sink": "osc", "port": port, "address": "/old",
                "native": {"min": 0, "max": 10}}, 0.5)
        data, _ = rx.recvfrom(1024)
        rx.close()
        self.assertEqual(decode_osc(data), ("/old", [5.0]))
