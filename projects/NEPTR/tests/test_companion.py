"""Tests for the companion-mode state machine (companion.CompanionController).

Pure logic, headless. Screens: 0=DASHBOARD, 1=PATCHES, 2=POWER.

Gestures:
  * joystick button held HOLD_SECS (3s)      -> toggle companion
  * arcade button held ARCADE_HOLD_SECS (1.2s) -> toggle companion
  * arcade quick tap inside companion        -> activate current selection
  * joystick = D-pad (dominant axis + hysteresis), rotation-corrected for the
    270° panel mount.

Raw joystick mapping on the device (calibrated from the panel mount):
  physical up=jx HIGH, down=jx LOW, left=jy HIGH, right=jy LOW.
So the screen-direction helpers below push the right raw values.
"""

import companion
from companion import CompanionController

CENTER = 64
LOW = 8       # well below the deadband
HIGH = 120    # well above the deadband

# screen-direction -> raw (joy_x, joy_y), calibrated for the 270° mount
_DIRS = {
    "up":     (HIGH,   CENTER),
    "down":   (LOW,    CENTER),
    "left":   (CENTER, HIGH),
    "right":  (CENTER, LOW),
    "center": (CENTER, CENTER),
}


def make(counts=(0, 3, 4)):
    return CompanionController(item_counts=lambda: list(counts))


def step(c, t, btn=False, x=CENTER, y=CENTER, arcade=False):
    return c.update(t, joy_btn=btn, joy_x=x, joy_y=y, arcade=arcade)


def push(c, t, direction, arcade=False):
    x, y = _DIRS[direction]
    return c.update(t, joy_btn=False, joy_x=x, joy_y=y, arcade=arcade)


def _enter(c, t=3.0):
    """Enter via the joystick-button 3s hold. Returns the entry time."""
    step(c, t - 3.0, btn=True)
    step(c, t, btn=True)
    step(c, t + 0.1, btn=False)
    return t


# ---------------------------------------------------------------- enter / exit

def test_joystick_hold_enters_and_exits():
    c = make()
    assert step(c, 0.0, btn=True) == []
    assert step(c, 3.0, btn=True) == ["enter"]
    assert c.active and c.screen == 0
    step(c, 3.1, btn=False)
    step(c, 4.0, btn=True)
    assert step(c, 7.0, btn=True) == ["exit"]
    assert not c.active


def test_arcade_hold_enters_after_1_2s():
    c = make()
    assert step(c, 0.0, arcade=True) == []
    assert step(c, 1.1, arcade=True) == []          # not yet
    assert step(c, 1.2, arcade=True) == ["enter"]   # crosses 1.2s
    assert c.active


def test_arcade_hold_exits():
    c = make()
    _enter(c)
    step(c, 4.0, arcade=True)
    assert step(c, 5.2, arcade=True) == ["exit"]
    assert not c.active


def test_arcade_hold_does_not_also_activate_on_release():
    c = make()
    step(c, 0.0, arcade=True)
    step(c, 1.2, arcade=True)                        # enter
    ev = step(c, 1.3, arcade=False)                  # release after the hold
    assert ev == []                                  # no stray activate
    assert c.active


def test_inputs_ignored_when_not_active():
    c = make()
    assert push(c, 0.0, "up") == []
    assert push(c, 0.1, "right") == []
    assert not c.active


# ---------------------------------------------------------------- navigation

def test_right_advances_tab_left_goes_back():
    c = make()
    t = _enter(c)
    assert push(c, t + 0.1, "right") == ["nav"]
    assert c.screen == 1
    push(c, t + 0.2, "center")
    assert push(c, t + 0.3, "right") == ["nav"]
    assert c.screen == 2
    push(c, t + 0.4, "center")
    assert push(c, t + 0.5, "left") == ["nav"]
    assert c.screen == 1


def test_down_advances_selection_up_goes_back():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")   # to PATCHES
    assert c.screen == 1 and c.sel == 0
    assert push(c, t + 0.3, "down") == ["nav"]
    assert c.sel == 1
    push(c, t + 0.4, "center")
    assert push(c, t + 0.5, "up") == ["nav"]
    assert c.sel == 0


def test_rotation_calibration():
    # Pin the mount calibration: physical right (jy LOW) = next tab;
    # physical up (jx HIGH) = selection up. Guards against an axis regression.
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    c.update(t + 0.1, joy_btn=False, joy_x=CENTER, joy_y=LOW, arcade=False)
    assert c.screen == 1                              # jy LOW -> next tab (right)
    c.update(t + 0.2, joy_btn=False, joy_x=CENTER, joy_y=CENTER, arcade=False)
    c.update(t + 0.3, joy_btn=False, joy_x=LOW, joy_y=CENTER, arcade=False)
    assert c.sel == 1                                 # jx LOW -> selection down


def test_dominant_axis_only_strongest_acts():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")   # to PATCHES
    # jx LOW (down, strong dx) + jy mildly off: vertical dominates -> only sel
    ev = c.update(t + 0.3, joy_btn=False, joy_x=LOW, joy_y=95, arcade=False)
    assert ev == ["nav"]
    assert c.screen == 1          # tab untouched
    assert c.sel == 1             # selection moved


def test_hysteresis_holds_without_retrigger():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")   # to PATCHES
    assert push(c, t + 0.3, "down") == ["nav"]               # sel 0 -> 1
    # ease jx into the hysteresis gap (|Δ|=20: past release 14, below commit 26)
    assert c.update(t + 0.4, joy_btn=False, joy_x=44, joy_y=CENTER, arcade=False) == []
    assert push(c, t + 0.5, "down") == []                    # still engaged
    assert c.sel == 1
    push(c, t + 0.6, "center")
    assert push(c, t + 0.7, "down") == ["nav"]
    assert c.sel == 2


def test_selection_clamped_to_item_count():
    c = make(counts=(0, 2, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")    # PATCHES, 2 items
    push(c, t + 0.3, "down"); push(c, t + 0.4, "center")     # sel 0->1
    assert c.sel == 1
    assert push(c, t + 0.5, "down") == []                    # already last
    assert c.sel == 1


def test_dashboard_has_no_navigable_items():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    assert c.screen == 0
    assert push(c, t + 0.3, "down") == []
    assert c.sel == 0


def test_switching_tab_reseats_the_cursor():
    """Entering a screen seats the cursor at that screen's own default (row 0
    with no default_sel callback) instead of carrying the previous tab's row
    across. Cross-tab carryover is what made row 0 vs "the live item"
    ambiguous on the PATCHES screen."""
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")    # PATCHES
    push(c, t + 0.3, "down"); push(c, t + 0.4, "center")
    push(c, t + 0.5, "down"); push(c, t + 0.6, "center")     # sel 2
    assert c.sel == 2
    push(c, t + 0.7, "right"); push(c, t + 0.8, "center")    # POWER (4 items)
    assert c.screen == 2 and c.sel == 0


def test_entering_screen_uses_default_sel():
    """default_sel(screen) decides where the cursor lands — the UI points it
    at the patch that is actually loaded, so the alphabetically-first patch
    (car_mode) is never sitting under the cursor by accident."""
    c = make(counts=(0, 4, 4))
    c._default_sel = lambda screen: 2 if screen == 1 else None
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")    # PATCHES
    assert c.screen == 1 and c.sel == 2
    push(c, t + 0.3, "right"); push(c, t + 0.4, "center")    # POWER
    assert c.sel == 0                                        # no default there


def test_default_sel_failure_is_not_fatal():
    c = make(counts=(0, 3))
    c._default_sel = lambda screen: 1 / 0
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")
    assert c.screen == 1 and c.sel == 0


def test_default_sel_out_of_range_is_clamped():
    c = make(counts=(0, 3))
    c._default_sel = lambda screen: 99
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")
    assert c.sel == 2


def test_restore_puts_the_cursor_back():
    c = make(counts=(0, 5, 4))
    c.restore(1, 3, active=True, now=100.0)
    assert c.active and c.screen == 1 and c.sel == 3
    # and the restored menu gets a full idle window, not an instant timeout
    assert c.update(100.1, joy_btn=0, joy_x=64, joy_y=64, arcade=0) == []


def test_restore_clamps_against_a_shrunken_list():
    c = make(counts=(0, 2))
    c.restore(1, 9, active=True, now=0.0)
    assert c.sel == 1


def test_restore_rejects_a_bogus_screen():
    c = make(counts=(0, 3))
    c.restore(99, 0, active=True, now=0.0)
    assert c.screen == 0


# ---------------------------------------------------------------- activate

def test_arcade_tap_activates_current_selection():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")    # PATCHES
    push(c, t + 0.3, "down"); push(c, t + 0.4, "center")     # sel 1
    step(c, t + 0.5, arcade=True)                            # press
    assert step(c, t + 0.6, arcade=False) == [("activate", 1, 1)]   # release = tap


def test_arcade_tap_is_one_shot():
    c = make()
    t = _enter(c)
    step(c, t + 0.1, arcade=True)
    assert step(c, t + 0.2, arcade=False) == [("activate", 0, 0)]
    assert step(c, t + 0.3, arcade=False) == []             # no repeat


# ---------------------------------------------------------------- idle timeout

def test_idle_fifteen_seconds_exits_and_sleeps():
    c = make()
    t = _enter(c)
    assert step(c, t + 14.9) == []
    assert step(c, t + 15.0) == ["sleep"]
    assert not c.active


def test_navigation_resets_idle_timer():
    c = make(counts=(0, 3, 4))
    t = _enter(c)
    push(c, t + 0.1, "right"); push(c, t + 0.2, "center")
    push(c, t + 10.0, "down"); push(c, t + 10.1, "center")
    assert step(c, t + 24.9) == []
    assert c.active
    assert step(c, t + 25.1) == ["sleep"]
    assert not c.active


# ---------------------------------------------------------------- misc

def test_default_thresholds():
    c = make()
    assert c.HOLD_SECS == 3.0
    assert c.ARCADE_HOLD_SECS == 1.2
    assert c.IDLE_SECS == 15.0


def test_next_deadline_none_when_idle_and_inactive():
    c = make()
    assert c.next_deadline(5.0) is None


def test_next_deadline_joystick_hold():
    c = make()
    step(c, 0.0, btn=True)
    assert c.next_deadline(0.0) == 3.0


def test_next_deadline_arcade_hold():
    c = make()
    step(c, 0.0, arcade=True)
    assert c.next_deadline(0.0) == 1.2


def test_next_deadline_idle_when_active():
    c = make()
    t = _enter(c)
    assert c.next_deadline(t) == t + 15.0


def test_restore_survives_a_list_that_has_not_loaded_yet():
    """Regression (cold boot, 2026-08-10): the AUDIO screen's interface list
    is fetched on a worker thread, so at restore time it is still empty. A
    clamp against the empty list used to zero the restored cursor — the
    position survived the power cycle and was then thrown away microseconds
    later."""
    counts = [0, 3, 4, 0]                 # AUDIO (3) not loaded yet
    c = CompanionController(item_counts=lambda: counts)
    c.restore(3, 2, active=True, now=0.0)
    assert c.sel == 2                     # kept, not zeroed
    counts[3] = 5                         # the list lands
    c._clamp_sel()
    assert c.sel == 2                     # still where the user left it


def test_clamp_still_bounds_a_stale_cursor_once_items_exist():
    counts = [0, 3, 4, 0]
    c = CompanionController(item_counts=lambda: counts)
    c.restore(3, 9, active=True, now=0.0)
    counts[3] = 2
    c._clamp_sel()
    assert c.sel == 1
