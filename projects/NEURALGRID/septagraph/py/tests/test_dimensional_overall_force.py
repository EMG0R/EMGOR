"""Overall velocity across however many pads are held.

Solving for each pad's individual force needs as many good sensors as pads.
On this instrument two of four FSRs are unreliable, so a four-pad chord makes
that system rank-deficient and the answer is noise -- observed as "it just does
not work at all with four pads down".

But the individual forces were never the goal. The goal is ONE overall value,
which is ONE unknown against FOUR equations:

    E = sum of the held pads' calibrated response columns
        ("what the sensors would read if every held pad were at reference")
    overall = (b . E) / (E . E)

Massively overdetermined, so it survives a dead sensor, and it is exactly the
question being asked: given these pads are down, how hard overall?
"""
import pytest

from septagraph.dimensional.pad_forces import overall_force

PROFILE = {
    0:  (1000, 300, 100, 50),
    9:  (700, 500, 200, 120),
    27: (400, 400, 400, 400),
    54: (100, 200, 600, 900),
    63: (50, 100, 300, 1000),
}


def _mix(loads):
    return tuple(sum(PROFILE[p][i] * f for p, f in loads.items()) for i in range(4))


class TestOnePad:
    def test_a_reference_press_reads_one(self):
        v, _ = overall_force(_mix({0: 1.0}), [0], PROFILE)
        assert v == pytest.approx(1.0, abs=0.01)

    def test_half_as_hard_reads_half(self):
        v, _ = overall_force(_mix({0: 0.5}), [0], PROFILE)
        assert v == pytest.approx(0.5, abs=0.01)


class TestManyPads:
    def test_four_pads_all_at_reference_read_one(self):
        held = [0, 9, 54, 63]
        v, _ = overall_force(_mix({p: 1.0 for p in held}), held, PROFILE)
        assert v == pytest.approx(1.0, abs=0.02)

    def test_four_pads_all_pressed_softly_read_soft(self):
        held = [0, 9, 54, 63]
        v, _ = overall_force(_mix({p: 0.3 for p in held}), held, PROFILE)
        assert v == pytest.approx(0.3, abs=0.03)

    def test_pressing_a_chord_harder_reads_harder(self):
        held = [0, 9, 27, 54]
        soft, _ = overall_force(_mix({p: 0.2 for p in held}), held, PROFILE)
        hard, _ = overall_force(_mix({p: 0.8 for p in held}), held, PROFILE)
        assert hard > soft * 3

    def test_five_pads_still_work(self):
        # more pads than sensors -- fatal for a per-pad solve, irrelevant here
        held = list(PROFILE)
        v, _ = overall_force(_mix({p: 0.6 for p in held}), held, PROFILE)
        assert v == pytest.approx(0.6, abs=0.05)

    def test_a_chord_is_not_louder_than_one_pad_at_the_same_strength(self):
        one, _ = overall_force(_mix({0: 0.7}), [0], PROFILE)
        four_held = [0, 9, 54, 63]
        four, _ = overall_force(_mix({p: 0.7 for p in four_held}), four_held, PROFILE)
        assert four == pytest.approx(one, abs=0.05)


class TestSurvivesBadHardware:
    def test_it_works_with_two_sensors_dead(self):
        # FSR2/FSR3 contributing nothing at all
        dead = {p: (v[0], v[1], 0, 0) for p, v in PROFILE.items()}
        held = [0, 9, 54, 63]
        b = tuple(sum(dead[p][i] * 0.5 for p in held) for i in range(4))
        v, _ = overall_force(b, held, dead)
        assert v == pytest.approx(0.5, abs=0.05)

    def test_confidence_falls_when_the_held_pads_barely_register(self):
        weak = {1: (12, 5, 0, 0)}
        _, sigma = overall_force((6, 2, 0, 0), [1], weak)
        strong_sigma = overall_force(_mix({0: 0.5}), [0], PROFILE)[1]
        assert sigma > strong_sigma

    def test_an_unknown_pad_contributes_nothing_rather_than_breaking(self):
        v, _ = overall_force(_mix({0: 1.0}), [0, 999], PROFILE)
        assert v == pytest.approx(1.0, abs=0.02)

    def test_no_profile_at_all_declines_to_answer(self):
        assert overall_force((100, 100, 100, 100), [5], {}) == (None, 1.0)


class TestHeadroom:
    def test_pressing_past_the_calibration_press_still_reads_higher(self):
        v, _ = overall_force(_mix({0: 1.6}), [0], PROFILE)
        assert v > 1.5


class TestInTheEncoder:
    REST = (0, 0, 0, 0)

    def _enc(self):
        from septagraph.dimensional.encoders.neuralgrid import (
            Calibration, NeuralGridEncoder,
        )
        return NeuralGridEncoder(
            calibration=Calibration(fsr_rest=self.REST, fsr_pads=PROFILE)
        )

    def _wire(self, sensors):
        return (0, 0, 0, 0) + tuple(int(v) for v in sensors)

    def _hold(self, e, pads):
        for pad in pads:
            e.on_key(pad % 8, pad // 8, True, t_mono=1.0)

    def _reference(self, e, t=1.0):
        """Let the note settle at nothing, so leaning in is what gets measured.
        Force is aftertouch: pressure ABOVE where the press landed."""
        for k in range(14):
            e.on_analog(self._wire((0, 0, 0, 0)), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        return t + 14 * 0.024

    def _settle(self, e, sensors, t=1.0, frames=220):
        """Let the auto-ranged scale settle before reading a level."""
        for k in range(frames):
            e.on_analog(self._wire(sensors), t_mono=t + k * 0.024)
            e.frame(t_mono=t + k * 0.024)
        return e.frame(t_mono=t + frames * 0.024).channel("force").points[0].values[0]

    def test_a_four_pad_chord_produces_a_sensible_value(self):
        held = [0, 9, 54, 63]
        e = self._enc()
        self._hold(e, held)
        t = self._reference(e)
        assert 0.1 < self._settle(e, _mix({p: 0.6 for p in held}), t=t) <= 1.0

    def test_a_chord_tracks_how_hard_it_is_pressed(self):
        held = [0, 9, 54, 63]
        e = self._enc(); self._hold(e, held)
        t = self._reference(e)
        self._settle(e, _mix({p: 0.9 for p in held}), t=t)   # settle on firm
        soft = self._settle(e, _mix({p: 0.2 for p in held}), t=8.0, frames=30)
        hard = self._settle(e, _mix({p: 0.9 for p in held}), t=9.0, frames=30)
        assert hard > soft + 0.15

    def test_you_can_always_press_above_what_you_have_been_doing(self):
        e = self._enc()
        self._hold(e, [0])
        settled = self._settle(e, _mix({0: 1.0}))
        harder = self._settle(e, _mix({0: 2.5}), t=5.0, frames=3)
        assert harder > settled

    def test_a_light_press_reads_light_against_a_settled_scale(self):
        e = self._enc()
        self._hold(e, [0])
        self._settle(e, _mix({0: 1.0}))                      # settle on firm
        # more than a few frames: the smoother releases SLOWLY on purpose, so
        # sustained pressure does not shimmer
        assert self._settle(e, _mix({0: 0.15}), t=5.0, frames=30) < 0.55
