import math
import pytest
from looper_ref.model import nearest_pow2_ratio, quantize_length


def test_nearest_pow2_ratio_snaps_to_octave_grid():
    assert nearest_pow2_ratio(1.05) == 1.0
    assert nearest_pow2_ratio(0.6) == 0.5      # cut just past half -> half
    assert nearest_pow2_ratio(0.3) == 0.25     # quarter region
    assert nearest_pow2_ratio(1.5) == 2.0      # round-half-up -> double
    assert nearest_pow2_ratio(3.0) == 4.0


def test_nearest_pow2_ratio_clamps_to_exp_bounds():
    assert nearest_pow2_ratio(0.01) == 0.25    # clamp low (min_exp=-2)
    assert nearest_pow2_ratio(50.0) == 8.0     # clamp high (max_exp=3)


def test_nearest_pow2_ratio_rejects_nonpositive():
    with pytest.raises(ValueError):
        nearest_pow2_ratio(0.0)


def test_quantize_length_rounds_against_master():
    # master cycle = 2.0 s
    assert quantize_length(1.1, 2.0) == 1.0    # cut just past halfway -> half
    assert quantize_length(0.9, 2.0) == 1.0    # clearly short -> half
    assert quantize_length(3.9, 2.0) == 4.0    # "two rotations" -> x2 longer
