"""Tests for the latency CLI: the report renderer and the selftest harness."""

from __future__ import annotations

import unittest

import numpy as np

import cli
import engine

SR = 48000


class TestRenderReport(unittest.TestCase):
    def _result(self):
        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(480), ref])[: len(ref)]
        return engine.analyze_shot(ref, out, SR)

    def test_report_shows_total_and_guaranteed_with_hw_offset(self):
        res = self._result()
        # measured latencies (>0.5 ms, high confidence) -> internal = measured
        agg = engine.aggregate_latency([10.0, 10.1, 9.9, 10.2])
        text = cli.render_report(res, agg, hw_offset_ms=1.7,
                                 interface="usb", mode="synclayer")
        # internal worst 10.2 + interface 1.7 = 11.9 ms guaranteed total
        self.assertIn("11.9", text)
        self.assertIn("GUARANTEED", text.upper())
        self.assertIn("usb", text)

    def test_falls_back_to_computed_internal_when_tap_zero(self):
        # measured ~0 (virtual tap) -> internal computed from quantum × mode.
        ref = engine.make_sweep(SR, dur=2.0)
        res = engine.analyze_shot(ref, ref, SR)  # zero-delay -> ~0 ms measured
        agg = engine.aggregate_latency([0.0, 0.0, 0.0])
        # synclayer @ 5.333 ms quantum -> 5 × 5.333 = 26.67 ms internal
        text = cli.render_report(res, agg, hw_offset_ms=35.0, interface="hdmi",
                                 offset_source="spec", quantum_ms=5.333,
                                 mode="synclayer")
        self.assertIn("26.6", text)            # computed internal
        self.assertIn("computed", text)
        self.assertIn("61.6", text)            # 26.67 + 35 total

    def test_report_marks_passthrough_fail(self):
        ref = engine.make_sweep(SR, dur=2.0)
        res = engine.analyze_shot(ref, np.zeros_like(ref), SR)
        agg = engine.aggregate_latency([])
        text = cli.render_report(res, agg, hw_offset_ms=0.0, interface="x")
        self.assertIn("FAIL", text.upper())


class TestInterfaceOffset(unittest.TestCase):
    def test_spec_estimate_when_no_calibration(self):
        # No calibration stored -> fall back to the spec-based estimate,
        # flagged as an estimate (not a measurement).
        ms, source = cli.resolve_offset({}, "hdmi", "synclayer")
        self.assertAlmostEqual(ms, cli.INTERFACE_SPECS["hdmi"], delta=0.001)
        self.assertEqual(source, "spec")

    def test_calibration_wins_over_spec(self):
        store = {"usb/bypass": {"hw_offset_ms": 1.73}}
        ms, source = cli.resolve_offset(store, "usb", "bypass")
        self.assertAlmostEqual(ms, 1.73, delta=0.001)
        self.assertEqual(source, "calibrated")

    def test_unknown_interface_has_a_default(self):
        ms, source = cli.resolve_offset({}, "weird-thing", "synclayer")
        self.assertGreater(ms, 0.0)
        self.assertEqual(source, "spec")


class TestInternalLatencyModel(unittest.TestCase):
    def test_bypass_is_two_quanta(self):
        # 256 frames @ 48k = 5.333 ms; bypass ~ 2x.
        self.assertAlmostEqual(cli.compute_internal_ms(5.333, "bypass"),
                               2 * 5.333, delta=0.01)

    def test_synclayer_is_five_quanta(self):
        # matches the measured ~28 ms at quantum 256 (5.333 ms).
        self.assertAlmostEqual(cli.compute_internal_ms(5.333, "synclayer"),
                               5 * 5.333, delta=0.01)

    def test_prefers_measured_when_real_device_path(self):
        # high confidence + a real (non-zero) measured latency -> use measured.
        val, src = cli.choose_internal(measured_typ=11.8, measured_worst=12.4,
                                       confidence=0.9, computed=26.6)
        self.assertEqual(src, "measured")
        self.assertAlmostEqual(val[0], 11.8, delta=0.01)
        self.assertAlmostEqual(val[1], 12.4, delta=0.01)

    def test_falls_back_to_computed_when_tap_is_zero(self):
        # virtual tap (0 ms) or low confidence -> computed.
        val, src = cli.choose_internal(measured_typ=0.0, measured_worst=0.0,
                                       confidence=1.0, computed=26.6)
        self.assertEqual(src, "computed")
        self.assertAlmostEqual(val[0], 26.6, delta=0.01)


class TestSelftest(unittest.TestCase):
    def test_selftest_passes_on_engine(self):
        # The testing-ground mode must return success on the real engine.
        self.assertEqual(cli.run_selftest(), 0)


if __name__ == "__main__":
    unittest.main()
