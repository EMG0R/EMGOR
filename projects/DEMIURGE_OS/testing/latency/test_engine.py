"""Ground-truth tests for the DEMIURGE latency engine.

The whole point of this file IS the verification the user asked for:
synthesize a capture whose latency / gain / colouring / glitches are
KNOWN exactly, then prove the engine recovers them within tolerance.
No hardware required — this is the "testing-ground mode".

Run:  python3 -m unittest -v testing.latency.test_engine
  or: cd testing/latency && python3 -m unittest -v test_engine
"""

from __future__ import annotations

import unittest

import numpy as np

import engine


SR = 48000


class TestLatencyRecovery(unittest.TestCase):
    def test_recovers_known_integer_sample_delay(self):
        # A sweep delayed by exactly 480 samples = 10.000 ms at 48 kHz.
        ref = engine.make_sweep(SR, dur=2.0)
        delay = 480
        out = np.concatenate([np.zeros(delay), ref])[: len(ref)]

        res = engine.measure_latency(ref, out, SR)

        self.assertIsNotNone(res.latency_ms)
        self.assertAlmostEqual(res.latency_ms, 1000.0 * delay / SR, delta=0.05)
        self.assertGreater(res.confidence, 0.5)


class TestLatencyDriftRobustness(unittest.TestCase):
    def test_confident_despite_clock_drift(self):
        # Real captures cross independent device clocks: the output drifts
        # slightly in time vs the reference (resampling). Latency must still
        # be recovered with HIGH confidence (the old full-length dot product
        # collapsed to ~0 here).
        from scipy import signal
        ref = engine.make_sweep(SR, dur=6.0)
        delay = 480  # 10 ms
        drifted = signal.resample(ref, int(len(ref) * 1.0002))  # +200 ppm
        out = np.concatenate([np.zeros(delay), drifted])[: len(ref)]

        res = engine.measure_latency(ref, out, SR)

        self.assertIsNotNone(res.latency_ms)
        self.assertAlmostEqual(res.latency_ms, 10.0, delta=0.3)
        self.assertGreater(res.confidence, 0.5)


class TestShotGainAndPassthrough(unittest.TestCase):
    def _aligned_capture(self, delay=480, gain_db=0.0, color=None, noise=0.0):
        """Build a stereo capture: ch0 = ref, ch1 = delayed/coloured copy."""
        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(delay), ref])[: len(ref)]
        out = out * (10.0 ** (gain_db / 20.0))
        if color is not None:
            out = color(out)
        if noise > 0:
            rng = np.random.default_rng(0)
            out = out + noise * rng.standard_normal(out.size)
        return ref, out

    def test_passthrough_ok_when_signal_present(self):
        ref, out = self._aligned_capture()
        res = engine.analyze_shot(ref, out, SR)
        self.assertTrue(res.passthrough)

    def test_passthrough_fails_on_silence(self):
        ref, _ = self._aligned_capture()
        res = engine.analyze_shot(ref, np.zeros_like(ref), SR)
        self.assertFalse(res.passthrough)

    def test_passthrough_ok_when_signal_present_even_if_uncorrelated(self):
        # Audio IS getting through (full-scale output) but it doesn't correlate
        # with the reference (e.g. free-running clock domains async-resample).
        # Passthrough must still be OK — the inability to LOCK latency is
        # reported separately (low confidence -> computed fallback).
        ref = engine.make_sweep(SR, dur=2.0)
        rng = np.random.default_rng(0)
        out = 0.4 * rng.standard_normal(ref.size)   # signal present, uncorrelated
        res = engine.analyze_shot(ref, out, SR)
        self.assertTrue(res.passthrough)
        self.assertLess(res.latency.confidence, 0.3)   # latency NOT locked

    def test_recovers_known_gain(self):
        ref, out = self._aligned_capture(gain_db=-6.0)
        res = engine.analyze_shot(ref, out, SR)
        self.assertAlmostEqual(res.gain_db, -6.0, delta=0.5)


class TestFrequencyResponse(unittest.TestCase):
    def test_detects_lowpass_coloring(self):
        from scipy import signal as sg

        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(480), ref])[: len(ref)]
        # Hard 1.5 kHz low-pass: highs should be crushed, lows intact.
        sos = sg.butter(6, 1500.0, fs=SR, output="sos")
        out = sg.sosfiltfilt(sos, out)

        res = engine.analyze_shot(ref, out, SR)

        self.assertTrue(res.passthrough)
        # high band must be far more attenuated than the low band
        self.assertLess(res.freq_response.high_db, res.freq_response.low_db - 12.0)


class TestGlitchDetection(unittest.TestCase):
    def test_clean_capture_has_no_glitches(self):
        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(480), ref])[: len(ref)]
        res = engine.analyze_shot(ref, out, SR)
        self.assertEqual(res.glitches.clicks, 0)

    def test_injected_clicks_are_detected(self):
        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(480), ref])[: len(ref)].copy()
        # Slam in 5 full-scale single-sample clicks at known spots.
        for pos in (10000, 25000, 40000, 60000, 80000):
            out[pos] += 0.9
        res = engine.analyze_shot(ref, out, SR)
        self.assertGreaterEqual(res.glitches.clicks, 4)


class TestLatencySeries(unittest.TestCase):
    def test_series_of_constant_delay_capture(self):
        # A looped sweep delayed by a constant 480 samples; every segment
        # should report ~10 ms, so worst ~= typical and jitter ~ 0.
        one = engine.make_sweep(SR, dur=1.0)
        loop = np.tile(one, 4)
        out = np.concatenate([np.zeros(480), loop])[: len(loop)]
        series = engine.measure_latency_series(loop, out, SR, n_segments=4)
        self.assertEqual(len(series), 4)
        for v in series:
            self.assertAlmostEqual(v, 10.0, delta=0.2)


class TestSilentLeadIn(unittest.TestCase):
    def test_leading_silence_is_not_a_dropout(self):
        # Every real capture starts with silence before the signal arrives
        # (the latency itself). That lead-in must not count as a dropout.
        ref = engine.make_sweep(SR, dur=2.0)
        out = np.concatenate([np.zeros(2400), ref])[: len(ref)]  # 50 ms lead-in
        res = engine.analyze_shot(ref, out, SR)
        self.assertEqual(res.glitches.dropout_windows, 0)


class TestRobustness(unittest.TestCase):
    def test_short_or_empty_capture_fails_cleanly(self):
        # A near-empty capture (wiring/recording failure) must report a
        # passthrough FAIL, not crash the analysis.
        ref = engine.make_sweep(SR, dur=2.0)
        tiny = np.zeros(20)
        res = engine.analyze_shot(ref[:20], tiny, SR)
        self.assertFalse(res.passthrough)

    def test_silent_full_length_capture_fails_cleanly(self):
        ref = engine.make_sweep(SR, dur=2.0)
        res = engine.analyze_shot(ref, np.zeros_like(ref), SR)
        self.assertFalse(res.passthrough)


class TestLatencyAggregation(unittest.TestCase):
    def test_typical_jitter_and_worst_case(self):
        samples = [10.0, 10.2, 9.8, 10.1, 12.0]   # one outlier = worst case
        agg = engine.aggregate_latency(samples)
        self.assertAlmostEqual(agg.typical_ms, 10.1, delta=0.2)   # median
        self.assertAlmostEqual(agg.worst_ms, 12.0, delta=0.01)
        self.assertGreater(agg.jitter_ms, 0.0)


if __name__ == "__main__":
    unittest.main()
