"""Force must be silent unless a pad is actually held.

Observed on hardware: force sat at 0.299 with nothing touching the grid. Three
causes stacked --

  1. No gate. Aftertouch with no note held is not a thing: the resting sensors
     were being read as playing.
  2. No deadzone. The rest point drifts hundreds of counts, so "above rest"
     is never really zero.
  3. The compressive curve then multiplied that residue: 0.35 maps a tenth of
     an input to nearly half an output, so a little drift became a lot of force.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder

REST = (76, 2423, 0, 161)


def _wire(above):
    return (0, 0, 0, 0) + tuple(REST[i] + above[i] for i in range(4))


def _enc(**kw):
    return NeuralGridEncoder(calibration=Calibration(fsr_rest=REST, **kw))


class TestGatedToPresses:
    def test_no_pad_held_means_no_force_at_all(self):
        e = _enc()
        e.on_analog(_wire((400, 300, 0, 200)), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("force").active

    def test_drift_with_nothing_held_stays_silent(self):
        e = _enc()
        for t in range(10):
            e.on_analog(_wire((120, 400, 0, 90)), t_mono=float(t))
        assert not e.frame(t_mono=10.0).channel("force").active

    def test_holding_a_pad_opens_it(self):
        e = _enc()
        e.on_key(3, 3, True, t_mono=1.0)
        e.on_analog(_wire((900, 400, 0, 300)), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").active

    def test_releasing_closes_it_again(self):
        e = _enc()
        e.on_key(3, 3, True, t_mono=1.0)
        e.on_analog(_wire((900, 400, 0, 300)), t_mono=1.0)
        e.frame(t_mono=1.0)
        e.on_key(3, 3, False, t_mono=2.0)
        assert not e.frame(t_mono=2.0).channel("force").active

    def test_the_smoother_starts_clean_on_the_next_press(self):
        e = _enc()
        e.on_key(0, 0, True, t_mono=1.0)
        e.on_analog(_wire((2000, 900, 0, 600)), t_mono=1.0)
        e.frame(t_mono=1.0)
        e.on_key(0, 0, False, t_mono=2.0)
        e.frame(t_mono=2.0)
        e.on_key(1, 1, True, t_mono=3.0)
        e.on_analog(_wire((200, 90, 0, 60)), t_mono=3.0)
        v = e.frame(t_mono=3.0).channel("force").points[0].values[0]
        assert v < 0.5      # must not carry the old hard press across


class TestDeadzone:
    def test_a_press_barely_above_the_drift_reads_zero(self):
        # a held pad still REPORTS -- the note is down, the pressure is simply
        # at its bottom -- it just must not report drift as force
        e = _enc()
        e.on_key(0, 0, True, t_mono=1.0)
        e.on_analog(_wire((30, 20, 0, 15)), t_mono=1.0)
        ch = e.frame(t_mono=1.0).channel("force")
        assert ch.active and ch.points[0].values[0] == 0.0

    def test_a_real_press_clears_it(self):
        e = _enc()
        e.on_key(0, 0, True, t_mono=1.0)
        # establish a ceiling first, or the first sample defines full scale
        e.on_analog(_wire((3000, 1200, 0, 900)), t_mono=0.9)
        e.on_analog(_wire((1200, 500, 0, 400)), t_mono=1.0)
        assert e.frame(t_mono=1.0).channel("force").points[0].values[0] > 0.3

    def test_just_past_the_deadzone_reads_below_a_hard_press(self):
        # The compressive curve is deliberately steep at the bottom, so the
        # property that matters is ORDER, not an absolute number: a whisper
        # must still sit clearly under a real press.
        e = _enc()
        e.on_key(0, 0, True, t_mono=0.0)
        e.on_analog(_wire((3000, 1200, 0, 900)), t_mono=0.0)   # set full scale
        for _ in range(8):
            e.on_analog(_wire((130, 60, 0, 40)), t_mono=1.0)
        light = e.frame(t_mono=1.0).channel("force").points[0].values[0]
        for _ in range(8):
            e.on_analog(_wire((2400, 1000, 0, 700)), t_mono=2.0)
        hard = e.frame(t_mono=2.0).channel("force").points[0].values[0]
        assert light < hard * 0.6

    def test_it_is_still_monotonic_through_the_range(self):
        e = _enc()
        e.on_key(0, 0, True, t_mono=0.0)
        seen = []
        for n, mag in enumerate((200, 600, 1200, 2400)):
            for _ in range(8):   # let the smoother settle at each level
                e.on_analog(_wire((mag, mag // 3, 0, mag // 4)), t_mono=float(n))
            seen.append(e.frame(t_mono=float(n)).channel("force").points[0].values[0])
        assert all(b >= a for a, b in zip(seen, seen[1:]))


class TestCalibratedPathIsGatedToo:
    def test_a_calibrated_profile_is_also_silent_when_nothing_is_held(self):
        e = _enc(fsr_pads={0: (500, 200, 60, 40)})
        e.on_analog(_wire((400, 300, 0, 200)), t_mono=1.0)
        assert not e.frame(t_mono=1.0).channel("force").active
