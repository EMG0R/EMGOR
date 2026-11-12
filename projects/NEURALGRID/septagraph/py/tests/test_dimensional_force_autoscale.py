"""The profile gives direction; the scale has to be learned live.

Measured: the pad profile's reference totals sit around 2885 counts, captured
when the FSRs were reading in the thousands. Twenty minutes later the same
sensors rest near zero and a press produces a few hundred. Against a fixed
reference that is force ~0.15 -- "I have to press very hard to see change."

The profile's VALUE is position-independence: which sensors a press at that pad
should load, and in what ratio. That ratio survives drift. The magnitude does
not. So the projection is kept and its output is auto-ranged.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder

PROFILE = {0: (1000, 300, 100, 50), 63: (50, 100, 300, 1000)}
REST = (0, 0, 0, 0)


def _enc():
    return NeuralGridEncoder(
        calibration=Calibration(fsr_rest=REST, fsr_pads=PROFILE, grid_rotation=0)
    )



# Force is now AFTERTOUCH: pressure above where the note settled. Holding at the
# level you pressed at reads zero by definition, so these press, let the note
# settle, and then lean in -- which is what the value actually measures.

def _play(e, pad, sensors, frames=220, t=1.0, settle=(0, 0, 0, 0)):
    """Press at `settle`, let it become the reference, then lean to `sensors`."""
    e.on_key(pad % 8, pad // 8, True, t_mono=t)
    k = 0
    for _ in range(14):                       # the note settles -> reference
        e.on_analog((0, 0, 0, 0) + tuple(int(v) for v in settle),
                    t_mono=t + k * 0.024)
        e.frame(t_mono=t + k * 0.024)
        k += 1
    for _ in range(frames):                   # then lean in
        e.on_analog((0, 0, 0, 0) + tuple(int(v) for v in sensors),
                    t_mono=t + k * 0.024)
        e.frame(t_mono=t + k * 0.024)
        k += 1
    return e.frame(t_mono=t + k * 0.024).channel("force").points[0].values[0]


class TestDriftedSensorsStillReach:
    def test_a_press_a_tenth_of_the_calibrated_size_still_reads_strong(self):
        # the whole complaint: everything got quieter, so the top of the range
        # has to come with it. Not 1.0 -- headroom deliberately leaves room to
        # press harder than your recent hardest.
        e = _enc()
        assert _play(e, 0, (100, 30, 10, 5)) > 0.7

    def test_pressing_past_your_recent_hardest_goes_higher_still(self):
        e = _enc()
        at_max = _play(e, 0, (100, 30, 10, 5), frames=260)
        harder = _play(e, 0, (260, 78, 26, 13), frames=3, t=5.0)
        assert harder > at_max

    def test_it_gets_there_within_a_second_of_playing(self):
        e = _enc()
        assert _play(e, 0, (100, 30, 10, 5), frames=260) > 0.7

    def test_soft_still_reads_softer_than_hard(self):
        e = _enc()
        _play(e, 0, (100, 30, 10, 5))                    # establish the range
        soft = _play(e, 0, (25, 8, 3, 1), t=2.0)
        hard = _play(e, 0, (100, 30, 10, 5), t=3.0)
        assert hard > soft + 0.25

    def test_a_harder_press_rescales_the_softer_one_down(self):
        e = _enc()
        _play(e, 0, (100, 30, 10, 5))
        _play(e, 0, (400, 120, 40, 20), t=2.0)           # a much harder press
        assert _play(e, 0, (100, 30, 10, 5), t=3.0) < 0.75

    def test_the_learned_scale_is_reported(self):
        e = _enc()
        _play(e, 0, (100, 30, 10, 5))
        assert e.raw()["force_scale"] > 0


class TestStillPositionIndependent:
    def test_the_same_press_at_two_corners_reads_the_same(self):
        e = _enc()
        near = _play(e, 0, (100, 30, 10, 5))
        e2 = _enc()
        far = _play(e2, 63, (5, 10, 30, 100))
        assert near == pytest.approx(far, abs=0.05)
