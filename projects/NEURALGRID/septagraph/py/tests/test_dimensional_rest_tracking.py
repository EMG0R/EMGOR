"""The zero point has to follow the sensors down.

Measured on hardware: rest was captured with FSR1 at 2133, and twenty minutes
later it sat at 1596 untouched -- 537 counts BELOW its own zero. above_rest
clamps at zero, so that sensor contributed nothing at all until pressed hard
enough to climb back past a stale number. That is "I have to press very hard to
see change", and it is the zero point, not the sensitivity.

A rolling minimum is the honest floor: whatever the sensor has actually reached
recently is, by definition, at least as low as resting.
"""
import pytest

from septagraph.dimensional.encoders.neuralgrid import Calibration, NeuralGridEncoder

REST = (87, 2133, 11, 13)


def _enc(**kw):
    return NeuralGridEncoder(calibration=Calibration(fsr_rest=REST, **kw))


def _wire(fsr):
    return (0, 0, 0, 0) + tuple(fsr)


def _hold(e):
    e.on_key(0, 0, True, t_mono=0.0)
    return e


class TestFollowsDrift:
    def test_a_sensor_drifting_below_its_rest_is_not_stranded(self):
        e = _hold(_enc())
        t = 0.0
        for _ in range(40):                     # settles at a lower level
            t += 0.05
            e.on_analog(_wire((80, 1596, 10, 12)), t_mono=t)
        t += 0.05
        e.on_analog(_wire((80, 1900, 10, 12)), t_mono=t)   # a light press
        assert e.raw()["fsr_total"] > 200

    def test_without_tracking_that_press_would_read_nothing(self):
        # the same press against the stale zero
        assert max(0, 1900 - 2133) == 0

    def test_the_floor_follows_each_sensor_separately(self):
        e = _hold(_enc())
        t = 0.0
        for _ in range(40):
            t += 0.05
            e.on_analog(_wire((80, 1596, 10, 12)), t_mono=t)
        assert e.raw()["fsr_floor_now"][1] <= 1596
        assert e.raw()["fsr_floor_now"][0] <= 87

    def test_it_never_rises_above_the_calibrated_rest(self):
        # a long press must not drag the zero UP and erase the sensor's range
        e = _hold(_enc())
        t = 0.0
        for _ in range(60):
            t += 0.05
            e.on_analog(_wire((3000, 3800, 900, 900)), t_mono=t)
        assert e.raw()["fsr_floor_now"] == list(REST)

    def test_a_press_still_reads_above_a_tracked_floor(self):
        e = _hold(_enc())
        t = 0.0
        for _ in range(40):
            t += 0.05
            e.on_analog(_wire((80, 1596, 10, 12)), t_mono=t)
        rest_total = e.raw()["fsr_total"]
        for _ in range(6):
            t += 0.05
            e.on_analog(_wire((600, 2400, 300, 200)), t_mono=t)
        assert e.raw()["fsr_total"] > rest_total + 1000

    def test_an_old_dip_stops_counting_once_the_window_passes(self):
        e = _hold(_enc())
        t = 0.0
        e.on_analog(_wire((10, 900, 0, 0)), t_mono=t)      # a momentary dip
        for _ in range(400):                               # well past the window
            t += 0.05
            e.on_analog(_wire((80, 1596, 10, 12)), t_mono=t)
        assert e.raw()["fsr_floor_now"][1] > 900
