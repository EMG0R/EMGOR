"""Pure-function tests for router.py — the mapping matrix."""
import math

from demiurge_io.router import apply_map, combine_values, MapState


def base_map(**overrides):
    m = {
        "zone_lo": 0.0, "zone_hi": 1.0, "outside": "clamp", "invert": False,
        "curve": "lin", "out_lo": 0.0, "out_hi": 1.0, "mode": "continuous",
        "combine": "replace", "slew_ms": 0.0, "steps": 8,
    }
    m.update(overrides)
    return m


# ── zone rescaling: "the back half of a pedal must be a whole knob" ────────

def test_zone_rescale_back_half_becomes_full_range():
    m = base_map(zone_lo=0.5, zone_hi=1.0)
    r_low = apply_map(m, 0.5, True, MapState(), dt=0.0)
    assert math.isclose(r_low.value, 0.0, abs_tol=1e-9)
    r_high = apply_map(m, 1.0, True, MapState(), dt=0.0)
    assert math.isclose(r_high.value, 1.0, abs_tol=1e-9)
    r_mid = apply_map(m, 0.75, True, MapState(), dt=0.0)
    assert math.isclose(r_mid.value, 0.5, abs_tol=1e-9)


def test_zone_outside_clamp_pins_to_edge():
    m = base_map(zone_lo=0.5, zone_hi=1.0, outside="clamp")
    r = apply_map(m, 0.1, True, MapState(), dt=0.0)
    assert r.send is True
    assert r.value == 0.0


def test_zone_outside_idle_sends_nothing():
    m = base_map(zone_lo=0.5, zone_hi=1.0, outside="idle")
    r = apply_map(m, 0.1, True, MapState(), dt=0.0)
    assert r.send is False


def test_zone_outside_hold_freezes_at_last_inside_value():
    m = base_map(zone_lo=0.5, zone_hi=1.0, outside="hold")
    state = MapState()
    r1 = apply_map(m, 0.75, True, state, dt=0.0)      # inside -> 0.5
    assert math.isclose(r1.value, 0.5, abs_tol=1e-9)
    r2 = apply_map(m, 0.0, True, r1.new_state, dt=0.0)  # leaves zone
    assert math.isclose(r2.value, 0.5, abs_tol=1e-9), "hold must freeze at the last inside value"
    r3 = apply_map(m, 0.0, True, r2.new_state, dt=0.0)  # still outside
    assert math.isclose(r3.value, 0.5, abs_tol=1e-9)


# ── invert ───────────────────────────────────────────────────────────────

def test_invert_flips_direction():
    m = base_map(invert=True)
    r = apply_map(m, 0.25, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.75, abs_tol=1e-9)


# ── curves ───────────────────────────────────────────────────────────────

def test_curve_lin_is_identity():
    m = base_map(curve="lin")
    r = apply_map(m, 0.3, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.3, abs_tol=1e-9)


def test_curve_exp_biases_toward_low_end():
    m = base_map(curve="exp")
    r = apply_map(m, 0.5, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.25, abs_tol=1e-9)


def test_curve_log_biases_toward_high_end():
    m = base_map(curve="log")
    r = apply_map(m, 0.25, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.5, abs_tol=1e-9)


def test_curve_s_is_symmetric_smoothstep():
    m = base_map(curve="s")
    r = apply_map(m, 0.5, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.5, abs_tol=1e-9)
    r_low = apply_map(m, 0.1, True, MapState(), dt=0.0)
    assert r_low.value < 0.1   # gentle near the bottom


# ── out_lo/out_hi range mapping ─────────────────────────────────────────

def test_out_range_rescale():
    m = base_map(out_lo=-12.0, out_hi=12.0)
    r = apply_map(m, 0.5, True, MapState(), dt=0.0)
    assert math.isclose(r.value, 0.0, abs_tol=1e-9)
    r2 = apply_map(m, 1.0, True, MapState(), dt=0.0)
    assert math.isclose(r2.value, 12.0, abs_tol=1e-9)


# ── modes ────────────────────────────────────────────────────────────────

def test_mode_continuous_always_sends():
    m = base_map(mode="continuous")
    r = apply_map(m, 0.42, True, MapState(), dt=0.0)
    assert r.send is True
    assert math.isclose(r.value, 0.42, abs_tol=1e-9)


def test_mode_toggle_flips_on_rising_crossing_and_holds_through_release():
    m = base_map(mode="toggle", out_lo=0.0, out_hi=1.0)
    state = MapState()
    # Below threshold: nothing fires.
    r = apply_map(m, 0.2, True, state, dt=0.0)
    assert r.send is False
    state = r.new_state
    # Cross the high hysteresis threshold: toggles on.
    r = apply_map(m, 0.8, True, state, dt=0.0)
    assert r.send is True
    assert r.value == 1.0
    state = r.new_state
    # Coming back down through the middle (but not below the low
    # threshold) must NOT re-fire or flip again.
    r = apply_map(m, 0.5, True, state, dt=0.0)
    assert r.send is False
    assert r.value == 1.0
    state = r.new_state
    # Drop below the low threshold: re-arms, still doesn't fire yet.
    r = apply_map(m, 0.2, True, state, dt=0.0)
    assert r.send is False
    state = r.new_state
    # Cross high again: flips OFF this time.
    r = apply_map(m, 0.8, True, state, dt=0.0)
    assert r.send is True
    assert r.value == 0.0


def test_mode_gate_opens_and_closes_with_hysteresis_no_latch():
    m = base_map(mode="gate", out_lo=0.0, out_hi=1.0)
    state = MapState()
    r = apply_map(m, 0.8, True, state, dt=0.0)
    assert r.value == 1.0 and r.send is True
    state = r.new_state
    r = apply_map(m, 0.5, True, state, dt=0.0)   # inside hysteresis band: stays open
    assert r.value == 1.0 and r.send is False
    state = r.new_state
    r = apply_map(m, 0.2, True, state, dt=0.0)   # below low threshold: closes
    assert r.value == 0.0 and r.send is True


def test_mode_trigger_fires_once_per_rising_edge():
    m = base_map(mode="trigger", out_lo=0.0, out_hi=1.0)
    state = MapState()
    r = apply_map(m, 0.8, True, state, dt=0.0)
    assert r.send is True
    state = r.new_state
    # Staying high must not re-fire.
    r = apply_map(m, 0.9, True, state, dt=0.0)
    assert r.send is False
    state = r.new_state
    # Dropping and rising again fires a second trigger.
    r = apply_map(m, 0.1, True, state, dt=0.0)
    state = r.new_state
    r = apply_map(m, 0.8, True, state, dt=0.0)
    assert r.send is True


def test_mode_step_quantizes_and_only_sends_on_change():
    m = base_map(mode="step", steps=4, out_lo=0.0, out_hi=1.0)
    state = MapState()
    r = apply_map(m, 0.05, True, state, dt=0.0)   # -> step 0
    assert r.send is True
    assert math.isclose(r.value, 0.0, abs_tol=1e-9)
    state = r.new_state
    r2 = apply_map(m, 0.07, True, state, dt=0.0)  # still step 0: no send
    assert r2.send is False
    state = r2.new_state
    r3 = apply_map(m, 0.30, True, state, dt=0.0)  # -> step 1 (0.25)
    assert r3.send is True
    assert math.isclose(r3.value, 0.25, abs_tol=1e-9)


# ── combine ──────────────────────────────────────────────────────────────

def test_combine_replace_last_wins():
    assert combine_values([("replace", 0.2), ("replace", 0.9)]) == 0.9


def test_combine_add_sums():
    assert combine_values([("add", 0.2), ("add", 0.3)]) == 0.5 or \
        abs(combine_values([("add", 0.2), ("add", 0.3)]) - 0.5) < 1e-9


def test_combine_max_takes_largest():
    assert combine_values([("max", 0.2), ("max", 0.9), ("max", 0.5)]) == 0.9


def test_combine_empty_is_none():
    assert combine_values([]) is None


# ── the spec's headline example: three maps sharing one source ─────────────

def test_three_maps_one_source_volume_toggle_rate_split():
    """One sensor (norm in 0..1) drives three independent maps:
       - volume on the front half   [0.0, 0.5]
       - a toggle on the back half  [0.5, 1.0]
       - a rate control also on the back half [0.5, 1.0]
    All three must evaluate correctly and independently off the SAME norm."""
    volume_map = base_map(zone_lo=0.0, zone_hi=0.5, outside="hold", mode="continuous")
    toggle_map = base_map(zone_lo=0.5, zone_hi=1.0, outside="idle", mode="toggle")
    rate_map = base_map(zone_lo=0.5, zone_hi=1.0, outside="hold", mode="continuous",
                         out_lo=1.0, out_hi=10.0)

    vol_state = MapState()
    tog_state = MapState()
    rate_state = MapState()

    # Position 1: front half, norm=0.25 -> volume should be at 0.5, toggle/rate idle-ish.
    v = apply_map(volume_map, 0.25, True, vol_state, dt=0.0)
    assert math.isclose(v.value, 0.5, abs_tol=1e-9)
    vol_state = v.new_state

    t = apply_map(toggle_map, 0.25, True, tog_state, dt=0.0)
    assert t.send is False   # outside its zone, outside="idle"
    tog_state = t.new_state

    r = apply_map(rate_map, 0.25, True, rate_state, dt=0.0)
    assert r.send is True    # outside="hold" still emits (frozen) each call
    rate_state = r.new_state

    # Position 2: back half, norm=0.9 -> volume holds at last value (0.5),
    # toggle fires on and goes to out_hi, rate tracks continuously.
    v2 = apply_map(volume_map, 0.9, True, vol_state, dt=0.0)
    assert math.isclose(v2.value, 0.5, abs_tol=1e-9), "volume must hold, not jump, once out of its zone"

    t2 = apply_map(toggle_map, 0.9, True, tog_state, dt=0.0)
    assert t2.send is True
    assert t2.value == 1.0

    r2 = apply_map(rate_map, 0.9, True, rate_state, dt=0.0)
    # frac within [0.5,1.0] zone at norm=0.9 -> (0.9-0.5)/0.5 = 0.8 -> out 1..10 -> 8.2
    assert math.isclose(r2.value, 8.2, abs_tol=1e-9)
