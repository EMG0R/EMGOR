import pytest
from septagraph.types import Knot, Septagraph, Hit, vec, ZERO, HALF
from septagraph.axes import K


def test_vec_validates_and_clamps():
    v = vec(*([1.5] * K))
    assert v == tuple([1.0] * K)
    with pytest.raises(ValueError):
        vec(0.1, 0.2)


def test_septagraph_interp_two_knots():
    s = Septagraph("s", 0, 1.0, (Knot(0.0, ZERO, ZERO), Knot(1.0, tuple([1.0] * K), tuple([0.2] * K))))
    mu, sig = s.at(0.25)
    assert mu == pytest.approx(tuple([0.25] * K))
    assert sig == pytest.approx(tuple([0.05] * K))
    assert s.at(-1)[0] == ZERO and s.at(2)[0] == tuple([1.0] * K)


def test_septagraph_requires_knot_invariants():
    with pytest.raises(ValueError):
        Septagraph("s", 0, 1.0, (Knot(0.5, ZERO, ZERO),))  # t0 != 0
    with pytest.raises(ValueError):
        Septagraph("s", 0, 1.0, (Knot(0.0, ZERO, ZERO), Knot(0.5, ZERO, ZERO)))  # last != 1 with n>1
    Septagraph("s", 0, 1.0, (Knot(0.0, HALF, ZERO),))  # single knot ok


def test_hit_clamps_velocity():
    h = Hit("c:1", 0, True, 1.4, HALF, ZERO)
    assert h.velocity == 1.0
