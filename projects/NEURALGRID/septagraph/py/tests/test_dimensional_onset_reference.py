"""Aftertouch measured from the note's own onset.

The reference is captured milliseconds before it is used, so sensor drift --
the dominant failure on this hardware, 2133 -> 240 over twenty minutes -- is
structurally irrelevant rather than worked around. It also cancels whatever the
plate was already carrying: a hand leaning on the grid, another pad held.

One continuous value: pressure above the level the note settled at.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder

PROFILE = {0: (1000, 300, 100, 50), 63: (50, 100, 300, 1000)}


def _enc(**kw):
    return NeuralGridEncoder(
        calibration=Calibration(fsr_pads=PROFILE, grid_rotation=0, **kw)
    )


def _analog(fsr):
    return (0, 0, 0, 0) + tuple(int(v) for v in fsr)


def _force(e, t):
    ch = e.frame(t_mono=t).channel("force")
    return ch.points[0].values[0] if ch.points else None


def _press(e, pad, resting, strike, hold, t0=1.0):
    """Rest, then strike, then settle into a hold -- as a real press goes."""
    t = t0
    for _ in range(12):                       # the plate before anything
        t += 0.024
        e.on_analog(_analog(resting), t_mono=t)
    e.on_key(pad % 8, pad // 8, True, t_mono=t)
    for _ in range(2):                        # the strike transient
        t += 0.024
        e.on_analog(_analog(strike), t_mono=t)
    for _ in range(8):                        # settles into the hold
        t += 0.024
        e.on_analog(_analog(hold), t_mono=t)
    return t


class TestDriftCannotReachIt:
    def test_a_wildly_drifted_baseline_does_not_matter(self):
        quiet = _enc()
        t = _press(quiet, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        a = _force(quiet, t)

        drifted = _enc()
        off = (2000, 600, 200, 100)            # everything sitting far higher
        t = _press(drifted, 0,
                   tuple(v + o for v, o in zip((50, 15, 5, 2), off)),
                   tuple(v + o for v, o in zip((900, 270, 90, 45), off)),
                   tuple(v + o for v, o in zip((400, 120, 40, 20), off)))
        assert _force(drifted, t) == pytest.approx(a, abs=0.06)

    def test_a_hand_already_resting_on_the_grid_is_cancelled(self):
        alone = _enc()
        t = _press(alone, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        a = _force(alone, t)

        leaning = _enc()
        lean = (600, 180, 60, 30)
        t = _press(leaning, 0,
                   tuple(v + l for v, l in zip((50, 15, 5, 2), lean)),
                   tuple(v + l for v, l in zip((900, 270, 90, 45), lean)),
                   tuple(v + l for v, l in zip((400, 120, 40, 20), lean)))
        assert _force(leaning, t) == pytest.approx(a, abs=0.06)


class TestAftertouchIsPressureBeyondTheHold:
    def test_merely_holding_reads_near_nothing(self):
        e = _enc()
        t = _press(e, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        assert _force(e, t) < 0.25

    def test_leaning_in_after_the_note_raises_it(self):
        e = _enc()
        t = _press(e, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        held = _force(e, t)
        for _ in range(10):
            t += 0.024
            e.on_analog(_analog((1600, 480, 160, 80)), t_mono=t)
        assert _force(e, t) > held + 0.2

    def test_easing_back_off_lowers_it_again(self):
        e = _enc()
        t = _press(e, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        for _ in range(10):
            t += 0.024
            e.on_analog(_analog((1600, 480, 160, 80)), t_mono=t)
        leaned = _force(e, t)
        for _ in range(30):
            t += 0.024
            e.on_analog(_analog((400, 120, 40, 20)), t_mono=t)
        assert _force(e, t) < leaned

    def test_releasing_ends_it(self):
        e = _enc()
        t = _press(e, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        e.on_key(0, 0, False, t_mono=t)
        assert not e.frame(t_mono=t).channel("force").active

    def test_a_second_press_gets_its_own_reference(self):
        e = _enc()
        t = _press(e, 0, (50, 15, 5, 2), (900, 270, 90, 45), (400, 120, 40, 20))
        e.on_key(0, 0, False, t_mono=t)
        e.frame(t_mono=t)
        t = _press(e, 0, (400, 120, 40, 20), (1200, 360, 120, 60),
                   (700, 210, 70, 35), t0=t)
        assert _force(e, t) < 0.25          # holding again, not leaning


# TestStrikeVelocity removed: velocity and aftertouch are the same quantity
# here -- one continuous pressure value -- not a transient at note-start plus a
# sustained level. Splitting them was an assumption, not a requirement.
