"""Per-pad velocity when several pads are held.

Four sensors give four equations. Once the response surface is known, each
held pad contributes a KNOWN column to that system, so for up to four
simultaneous pads the individual forces can be solved for outright -- not
interpolated, decomposed. Beyond four it is underdetermined and the encoder
says so in sigma instead of inventing per-pad numbers.

This changes an earlier claim: uncalibrated, the hardware genuinely cannot say
which of two held pads is pressed harder. CALIBRATED, it can.
"""
import pytest

from septagraph.dimensional.pad_forces import solve_pad_forces

# response of each sensor to a unit press at that pad
PROFILE = {
    0:  (1000, 300, 100, 50),
    9:  (700, 500, 200, 120),
    54: (100, 200, 600, 900),
    63: (50, 100, 300, 1000),
    36: (200, 400, 500, 400),
}


def _mix(loads):
    """What the four sensors read when `loads` = {pad: force} are applied."""
    return tuple(
        sum(PROFILE[pad][i] * f for pad, f in loads.items())
        for i in range(4)
    )


class TestSinglePad:
    def test_a_reference_press_reads_one(self):
        f, _ = solve_pad_forces(_mix({0: 1.0}), [0], PROFILE)
        assert f[0] == pytest.approx(1.0, abs=0.01)

    def test_half_as_hard_reads_half(self):
        f, _ = solve_pad_forces(_mix({0: 0.5}), [0], PROFILE)
        assert f[0] == pytest.approx(0.5, abs=0.01)

    def test_harder_than_the_reference_goes_above_one(self):
        f, _ = solve_pad_forces(_mix({0: 1.8}), [0], PROFILE)
        assert f[0] > 1.5


class TestChords:
    def test_two_pads_pressed_differently_are_told_apart(self):
        f, _ = solve_pad_forces(_mix({0: 1.0, 63: 0.3}), [0, 63], PROFILE)
        assert f[0] == pytest.approx(1.0, abs=0.05)
        assert f[63] == pytest.approx(0.3, abs=0.05)

    def test_three_pads_are_decomposed(self):
        loads = {0: 0.8, 36: 0.4, 63: 1.2}
        f, _ = solve_pad_forces(_mix(loads), list(loads), PROFILE)
        for pad, want in loads.items():
            assert f[pad] == pytest.approx(want, abs=0.06)

    def test_four_pads_is_still_solvable(self):
        loads = {0: 0.5, 9: 0.9, 54: 0.3, 63: 0.7}
        f, _ = solve_pad_forces(_mix(loads), list(loads), PROFILE)
        for pad, want in loads.items():
            assert f[pad] == pytest.approx(want, abs=0.1)

    def test_an_unpressed_pad_is_not_given_phantom_force(self):
        f, _ = solve_pad_forces(_mix({0: 1.0}), [0, 63], PROFILE)
        assert f[63] < 0.1

    def test_force_is_never_negative(self):
        f, _ = solve_pad_forces(_mix({0: 1.0}), [0, 63, 54], PROFILE)
        assert all(v >= 0 for v in f.values())


class TestConfidence:
    def test_a_single_pad_is_reported_confidently(self):
        _, sigma = solve_pad_forces(_mix({0: 1.0}), [0], PROFILE)
        assert sigma < 0.2

    def test_more_pads_than_sensors_is_underdetermined_and_says_so(self):
        held = [0, 9, 36, 54, 63]
        f, sigma = solve_pad_forces(_mix({p: 0.5 for p in held}), held, PROFILE)
        assert sigma >= 0.5
        assert set(f) == set(held)  # still answers, just not confidently

    def test_two_pads_with_near_identical_response_are_flagged(self):
        # ill-conditioned: the sensors cannot separate them
        profile = {1: (500, 200, 60, 40), 2: (505, 202, 61, 40)}
        _, sigma = solve_pad_forces((1005, 402, 121, 80), [1, 2], profile)
        assert sigma > 0.3


class TestFallbacks:
    def test_an_uncalibrated_pad_yields_nothing(self):
        assert solve_pad_forces((100, 100, 100, 100), [5], {}) == ({}, 1.0)

    def test_no_pads_held_yields_nothing(self):
        assert solve_pad_forces((100, 100, 100, 100), [], PROFILE) == ({}, 1.0)

    def test_a_partially_known_chord_uses_the_pads_it_knows(self):
        f, _ = solve_pad_forces(_mix({0: 1.0}), [0, 999], PROFILE)
        assert 0 in f and 999 not in f


REST = (3300, 1400, 0, 0)


def _wire(sensor_vals):
    return (0, 0, 0, 0) + tuple(REST[i] + int(sensor_vals[i]) for i in range(4))


class TestEncoderEmitsOneForce:
    """ONE 1D force value, whatever is held.

    The per-pad decomposition is the internal machinery, not the output: it is
    what makes the single number independent of WHERE and HOW MANY pads are
    pressed. Solve each held pad's force, then combine.
    """

    def _enc(self):
        from septagraph.dimensional.encoders.neuralgrid import (
            Calibration, NeuralGridEncoder,
        )
        return NeuralGridEncoder(
            calibration=Calibration(fsr_rest=REST, fsr_pads=PROFILE)
        )

    def _hold(self, e, pads):
        for pad in pads:
            e.on_key(pad % 8, pad // 8, True, t_mono=1.0)

    def test_always_exactly_one_force_value(self):
        e = self._enc()
        self._hold(e, [0, 9, 63])
        e.on_analog(_wire(_mix({0: 1.0, 9: 0.5, 63: 0.3})), t_mono=1.0)
        assert len(e.frame(t_mono=1.0).channel("force").points) == 1

    def test_the_same_press_at_two_corners_reads_the_same(self):
        # the whole point of the profile: position drops out
        e = self._enc()
        self._hold(e, [0])
        e.on_analog(_wire(_mix({0: 0.8})), t_mono=1.0)
        near = e.frame(t_mono=1.0).channel("force").points[0].values[0]

        e2 = self._enc()
        self._hold(e2, [63])
        e2.on_analog(_wire(_mix({63: 0.8})), t_mono=1.0)
        far = e2.frame(t_mono=1.0).channel("force").points[0].values[0]

        assert near == pytest.approx(far, abs=0.05)

    def test_pressing_more_pads_equally_hard_is_not_louder(self):
        # four gentle pads must not read as one hard one
        e = self._enc()
        self._hold(e, [0])
        e.on_analog(_wire(_mix({0: 0.6})), t_mono=1.0)
        one = e.frame(t_mono=1.0).channel("force").points[0].values[0]

        e2 = self._enc()
        held = [0, 9, 54, 63]
        self._hold(e2, held)
        e2.on_analog(_wire(_mix({p: 0.6 for p in held})), t_mono=1.0)
        four = e2.frame(t_mono=1.0).channel("force").points[0].values[0]

        assert four == pytest.approx(one, abs=0.08)

    def test_pressing_harder_reads_harder(self):
        # Force auto-ranges now, so absolute values depend on what has
        # been played recently. What must hold is the RELATIVE dynamics
        # within a phrase -- that is what playing depends on.
        e = self._enc()
        self._hold(e, [0])
        t = 1.0
        for _ in range(220):                     # the scale creeps at 2%/frame
            t += 0.024
            e.on_analog(_wire(_mix({0: 0.3})), t_mono=t)
            e.frame(t_mono=t)
        soft = e.frame(t_mono=t).channel("force").points[0].values[0]
        for _ in range(4):
            t += 0.024
            e.on_analog(_wire(_mix({0: 0.9})), t_mono=t)
            e.frame(t_mono=t)
        hard = e.frame(t_mono=t).channel("force").points[0].values[0]
        assert hard > soft

    def _settled(self, e, sensors, t=1.0):
        # force is aftertouch, so let the note settle at nothing first and let
        # the lean-in be what is measured
        for k in range(14):
            e.on_analog(_wire((0, 0, 0, 0)), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        for k in range(14, 40):
            e.on_analog(_wire(sensors), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        return e.frame(t_mono=t + 40 * 0.024).channel("force").points[0].values[0]

    def test_a_lightly_held_pad_does_not_drag_the_chord_down(self):
        # The cosine gate decides which held pads are actually being PLAYED, so
        # a finger merely resting is left out of the expectation rather than
        # averaged in. Resting on the grid while playing is normal, and this is
        # the behaviour that makes it survivable.
        e = self._enc()
        self._hold(e, [0, 63])
        t = 1.0
        def hold(loads, n):
            nonlocal t
            for _ in range(n):
                t += 0.024
                e.on_analog(_wire(_mix(loads)), t_mono=t)
                e.frame(t_mono=t)
            return e.frame(t_mono=t).channel("force").points[0].values[0]
        hold({0: 1.0, 63: 1.0}, 220)                    # set the reference
        both = hold({0: 1.0, 63: 1.0}, 4)
        mixed = hold({0: 1.0, 63: 0.05}, 4)             # one finger just resting
        assert mixed == pytest.approx(both, abs=0.1)

    def test_a_resting_finger_dilutes_only_modestly(self):
        # The cost of the robust projection, measured rather than assumed: a
        # light finger on a chord pulls the overall down, but not by much.
        e = self._enc()
        self._hold(e, [0])
        alone = self._settled(e, _mix({0: 0.9}))

        e2 = self._enc()
        self._hold(e2, [0, 54, 63])
        resting = self._settled(e2, _mix({0: 0.9, 54: 0.05, 63: 0.05}))

        assert resting > alone * 0.45

    def test_five_pads_are_reported_confidently_now(self):
        # Five pads used to raise sigma because a per-pad solve of five
        # unknowns against four sensors is genuinely underdetermined. The
        # projection has one unknown regardless, so the count no longer costs
        # any confidence -- that was the point of changing it.
        e = self._enc()
        held = [0, 9, 36, 54, 63]
        self._hold(e, held)
        e.on_analog(_wire(_mix({p: 0.5 for p in held})), t_mono=1.0)
        (p,) = e.frame(t_mono=1.0).channel("force").points
        assert p.sigma[0] < 0.2

    def test_the_per_pad_solve_is_still_visible_for_debugging(self):
        # settle at nothing, then lean unevenly -- the per-pad breakdown is of
        # the AFTERTOUCH, so a flat hold has nothing to break down
        e = self._enc()
        self._hold(e, [0, 63])
        t = 1.0
        for k in range(14):
            e.on_analog(_wire((0, 0, 0, 0)), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        for k in range(14, 34):
            e.on_analog(_wire(_mix({0: 1.0, 63: 0.3})), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        per_pad = e.raw()["pad_forces"]
        assert per_pad[0] > per_pad[63]

    def test_with_no_pads_held_there_is_no_force_at_all(self):
        # gated: aftertouch with no note held is not a thing
        e = self._enc()
        e.on_analog(_wire((500, 200, 60, 40)), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("force").active
