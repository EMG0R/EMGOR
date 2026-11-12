import math
import pytest
from septagraph.axes import AXES, K, clamp01, to_physical, from_physical


def test_axes_names_and_count():
    assert AXES == ("freq", "loud", "harm", "rate", "scale", "az", "spread", "dist", "el")
    assert K == 9


def test_clamp():
    assert clamp01(-0.1) == 0.0 and clamp01(1.7) == 1.0 and clamp01(0.3) == 0.3


def test_freq_map_endpoints_and_mid():
    assert to_physical(0, 0.0) == pytest.approx(20.0)
    assert to_physical(0, 1.0) == pytest.approx(20000.0)
    assert to_physical(0, 0.5) == pytest.approx(20 * math.sqrt(1000), rel=1e-6)


def test_loud_rate_scale_az():
    assert to_physical(1, 1.0) == pytest.approx(0.0)  # dBFS
    assert to_physical(1, 0.0) == pytest.approx(-60.0)
    assert to_physical(3, 0.0) == pytest.approx(0.5)  # Hz
    assert to_physical(3, 1.0) == pytest.approx(64.0)
    assert to_physical(4, 1.0) == pytest.approx(8.0)  # cyc/oct
    assert to_physical(5, 0.5) == pytest.approx(0.0)  # deg
    assert to_physical(5, 0.0) == pytest.approx(-180.0)


@pytest.mark.parametrize("idx", range(8))
@pytest.mark.parametrize("k", [0.0, 0.25, 0.5, 0.9, 1.0])
def test_roundtrip(idx, k):
    assert from_physical(idx, to_physical(idx, k)) == pytest.approx(k, abs=1e-6)
