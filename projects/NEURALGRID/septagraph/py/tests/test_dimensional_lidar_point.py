"""One smoothed point in space, fused from whichever rangefinders can see.

The four sensors are the input; the output is a single blurry point. Blurry is
the honest part -- sigma widens when fewer sensors contribute -- but it still
has to be steady enough to play against, and steady cannot cost latency.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder


def _enc():
    return NeuralGridEncoder(calibration=Calibration(lidar_max_mm=1000, grid_rotation=0))


def _ranges(top=None, bottom=None, left=None, right=None):
    """Build a hardware frame from EDGE positions.

    The firmware's index order is not the position order -- top is index 1 --
    so writing raw tuples in tests silently tests the wrong axis.
    """
    from septagraph.dimensional.encoders.neuralgrid import DEFAULT_LIDAR_MAP
    out = [0xFFFF] * 4
    for key, mm in (("n", top), ("s", bottom), ("w", left), ("e", right)):
        if mm is not None:
            out[DEFAULT_LIDAR_MAP[key]] = mm
    return tuple(out)


from septagraph.dimensional.encoders.neuralgrid import GRID_MM
SCALE = GRID_MM   # the instrument's real width -- what trilateration works in


def _hand(x, y, z=0.15, which=("n", "s", "e", "w"), jitter=None):
    """Distances a hand at (x, y, z) would actually produce.

    Arbitrary distance tuples are not realizable by any hand, and a real solve
    answers the geometry rather than a per-axis ratio -- so test data has to be
    geometry too.
    """
    from septagraph.dimensional.trilaterate import SENSOR_AT
    out = {}
    for pos in which:
        sx, sy = SENSOR_AT[pos]
        d = ((x - sx) ** 2 + (y - sy) ** 2 + z * z) ** 0.5
        out[pos] = int(d * SCALE + (jitter or {}).get(pos, 0))
    return _ranges(top=out.get("n"), bottom=out.get("s"),
                   left=out.get("w"), right=out.get("e"))


def _point(e, t):
    ch = e.frame(t_mono=t).channel("lidar")
    return ch.points[0] if ch.points else None


class TestSmoothing:
    def test_a_jittering_hand_produces_a_steadier_point(self):
        import random
        rnd = random.Random(3)
        e = _enc()
        raw, out = [], []
        t = 0.0
        for _ in range(60):
            t += 0.05
            wob = rnd.uniform(-0.03, 0.03)
            e.on_lidar(_hand(0.5, 0.5 + wob), t_mono=t)
            raw.append(0.5 + wob)
            p = _point(e, t)
            if p:
                out.append(p.values[1])
        # Standard deviation, not max-minus-min: for a first-order filter the
        # noise gain is sqrt(a/(2-a)) ~= 0.69 at this coefficient, and spread is
        # far too jumpy a statistic to test that against.
        def _sd(xs):
            m = sum(xs) / len(xs)
            return (sum((v - m) ** 2 for v in xs) / len(xs)) ** 0.5
        assert _sd(out[20:]) < _sd(raw[20:]) * 0.8

    def test_it_follows_a_real_move_within_a_couple_of_frames(self):
        # A flat solve pulls a raised hand toward the middle -- height inflates
        # every distance and there is nowhere to put it -- so a 0.6 move reads
        # as less than 0.6. That is the accepted cost of the steadier answer;
        # what matters is that it MOVES, promptly.
        e = _enc()
        for k in range(10):
            e.on_lidar(_hand(0.5, 0.8), t_mono=k * 0.05)
        before = _point(e, 0.5).values[1]
        for k in range(4):
            e.on_lidar(_hand(0.5, 0.2), t_mono=0.55 + k * 0.05)
        assert abs(_point(e, 0.75).values[1] - before) > 0.2

    def test_it_starts_on_the_first_reading_rather_than_easing_in(self):
        e = _enc()
        e.on_lidar(_hand(0.5, 0.25), t_mono=0.0)
        assert _point(e, 0.0).values[1] == pytest.approx(0.25, abs=0.08)

    def test_losing_the_hand_and_regaining_it_does_not_drag(self):
        e = _enc()
        for k in range(10):
            e.on_lidar(_hand(0.5, 0.8), t_mono=k * 0.05)
        e.on_lidar((0xFFFF,) * 4, t_mono=0.55)
        e.frame(t_mono=0.55)
        e.on_lidar(_hand(0.5, 0.25), t_mono=0.6)
        assert _point(e, 0.6).values[1] == pytest.approx(0.25, abs=0.1)


class TestStillBlurry:
    def test_fewer_contributing_sensors_still_widens_sigma(self):
        e = _enc()
        e.on_lidar(_hand(0.5, 0.5), t_mono=0.0)
        all_four = max(_point(e, 0.0).sigma)
        e2 = _enc()
        e2.on_lidar(_hand(0.5, 0.5, which=("n",)), t_mono=0.0)
        assert max(_point(e2, 0.0).sigma) > all_four


# TestSensorSetChanges removed: it asserted that a sensor joining or leaving
# should SNAP straight to the new answer. Superseded by TestNaturalBlending --
# a sensor's contribution now ramps, so the point drifts to the better estimate
# instead of stepping. Both are defensible; blending is what the instrument
# actually wants to play.


class TestNaturalBlending:
    """A sensor acquiring or losing the hand should fade, not pop. Its
    contribution ramps, so the point drifts to the better answer instead of
    stepping to it."""

    def test_a_sensor_appearing_moves_the_point_gradually(self):
        e = _enc()
        t = 0.0
        for _ in range(30):
            t += 0.05
            e.on_lidar(_hand(0.8, 0.4, which=("n", "s")), t_mono=t)
        before = _point(e, t).values[0]
        t += 0.05
        e.on_lidar(_hand(0.8, 0.4), t_mono=t)
        assert abs(_point(e, t).values[0] - before) < 0.4

    def test_and_it_arrives_within_a_few_more_frames(self):
        e = _enc()
        t = 0.0
        for _ in range(30):
            t += 0.05
            e.on_lidar(_hand(0.8, 0.4, which=("n", "s")), t_mono=t)
        for _ in range(25):
            t += 0.05
            e.on_lidar(_hand(0.8, 0.4), t_mono=t)
        assert _point(e, t).values[0] > 0.6

    def test_losing_a_sensor_fades_it_out_rather_than_snapping(self):
        e = _enc()
        t = 0.0
        for _ in range(40):
            t += 0.05
            e.on_lidar(_hand(0.8, 0.4), t_mono=t)
        before = _point(e, t).values[0]
        t += 0.05
        e.on_lidar(_hand(0.8, 0.4, which=("n", "s")), t_mono=t)
        assert abs(_point(e, t).values[0] - before) < 0.4

    def test_the_hand_vanishing_entirely_still_stops_at_once(self):
        # fading is for a sensor changing its mind, not for a hand that has gone
        e = _enc()
        t = 0.0
        for _ in range(20):
            t += 0.05
            e.on_lidar(_hand(0.5, 0.4), t_mono=t)
        t += 0.05
        e.on_lidar((0xFFFF,) * 4, t_mono=t)
        assert not e.frame(t_mono=t).channel("lidar").active
