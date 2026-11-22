"""Pure-function tests for conditioning.py — no hardware, no threads."""
from demiurge_io.conditioning import condition, ConditionerState
from demiurge_io.model import SourceDescriptor

TOF_DESC = SourceDescriptor("tof1", "LiDAR 1", "mm", 0, 4000, 100.0, dropout_value=-1)
PLAIN_CFG = {"in_lo": 0.0, "in_hi": 100.0, "deadzone": 0.0, "smooth_ms": 0.0}


def test_linear_calibration_midpoint():
    state = ConditionerState()
    norm, valid, _ = condition(50.0, PLAIN_CFG, None, state, dt=0.0)
    assert valid is True
    assert norm == 0.5


def test_linear_calibration_clamps_out_of_range():
    state = ConditionerState()
    norm, valid, _ = condition(500.0, PLAIN_CFG, None, state, dt=0.0)
    assert norm == 1.0
    norm2, _, _ = condition(-500.0, PLAIN_CFG, None, ConditionerState(), dt=0.0)
    assert norm2 == 0.0


def test_deadzone_snaps_edges_to_hard_0_and_1():
    cfg = {"in_lo": 0.0, "in_hi": 100.0, "deadzone": 0.05, "smooth_ms": 0.0}
    norm_low, _, _ = condition(3.0, cfg, None, ConditionerState(), dt=0.0)   # 0.03 < deadzone
    assert norm_low == 0.0
    norm_high, _, _ = condition(97.0, cfg, None, ConditionerState(), dt=0.0)  # 0.97 > 1-deadzone
    assert norm_high == 1.0
    norm_mid, _, _ = condition(50.0, cfg, None, ConditionerState(), dt=0.0)
    assert norm_mid == 0.5


def test_smoothing_moves_toward_target_but_does_not_jump_instantly():
    cfg = {"in_lo": 0.0, "in_hi": 100.0, "deadzone": 0.0, "smooth_ms": 100.0}
    state = ConditionerState(held_norm=0.0, smoothed=0.0, initialized=True, valid=True)
    norm, valid, new_state = condition(100.0, cfg, None, state, dt=0.01)
    assert 0.0 < norm < 1.0, "one small dt step must not reach the target instantly"
    # Keep feeding the same input; it should monotonically approach 1.0.
    prev = norm
    for _ in range(50):
        norm, _, new_state = condition(100.0, cfg, None, new_state, dt=0.01)
        assert norm >= prev - 1e-9
        prev = norm
    assert norm > 0.99


def test_dropout_holds_last_good_value_never_snaps_to_zero():
    cfg = {"in_lo": 20.0, "in_hi": 460.0, "deadzone": 0.0, "smooth_ms": 0.0, "dropout": "hold"}
    state = ConditionerState()
    # First a good reading near the top of range.
    norm1, valid1, state = condition(460.0, cfg, TOF_DESC, state, dt=0.0)
    assert valid1 is True
    assert norm1 == 1.0
    # Then a dropout (-1 sentinel): must hold at 1.0, not fall to 0.
    norm2, valid2, state = condition(-1, cfg, TOF_DESC, state, dt=0.01)
    assert valid2 is False
    assert norm2 == 1.0, "dropout must hold the last good value, never snap to 0"
    # Another dropout: still holding.
    norm3, valid3, state = condition(-1, cfg, TOF_DESC, state, dt=0.01)
    assert norm3 == 1.0
    # Recovery: a real reading resumes normal tracking.
    norm4, valid4, state = condition(20.0, cfg, TOF_DESC, state, dt=0.01)
    assert valid4 is True
    assert norm4 < 1.0


def test_dropout_on_very_first_sample_defaults_to_zero_not_undefined():
    cfg = {"in_lo": 20.0, "in_hi": 460.0, "deadzone": 0.0, "smooth_ms": 0.0}
    norm, valid, state = condition(-1, cfg, TOF_DESC, ConditionerState(), dt=0.0)
    assert valid is False
    assert norm == 0.0
