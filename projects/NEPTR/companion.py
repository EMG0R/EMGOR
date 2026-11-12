"""Companion-mode state machine for the NEPTR Phase 4 UI.

Pure logic, no I/O. The UI feeds it timestamped input snapshots from the
Teensy MIDI bus (joystick button = CC77, joystick X/Y = CC27/CC28 scaled to
0..127, arcade button b1 = CC60) and acts on the events it returns.

Gestures (docs/companion.md):

  * Hold the joystick button HOLD_SECS (3s) OR the arcade button
    ARCADE_HOLD_SECS (1.2s) to toggle companion mode on/off. A held press is
    consumed by the hold detector, so it never doubles as a click.
  * A quick arcade TAP inside companion activates the current selection.
  * Five screens act as tabs: 0=DASHBOARD, 1=PATCHES, 2=AUDIO, 3=SYSTEM,
    4=NETWORK — grouped by concept, not by mechanism. (WAVE used to be tab 4;
    it is now a top-level destination reached from the main screen's top-left
    button, not a setting.) The joystick
    is a D-pad with dominant-axis selection + hysteresis: only the axis
    furthest from centre acts (no diagonal cross-talk), and after a step the
    stick must return near centre before it steps again. One push = one move.
  * The axes are rotation-corrected for the 270° panel mount (see below).
  * IDLE_SECS with no nav/activation exits and asks the caller to sleep.

The joystick is NOT captured: Csound keeps reacting to it over MIDI while the
menu is open (audio has priority). The companion only *reads* the bus.

update() returns a list of events:
  "enter"                      entered companion mode
  "exit"                       left via a hold gesture
  "sleep"                      left via the idle timeout; caller should sleep
  "nav"                        screen or selection changed; repaint
  ("activate", screen, sel)    arcade tapped on this screen/selection
"""

import os

# Tab order = grouping by concept: what's happening, what's loaded, how it
# sounds, how the box runs, how it's reachable.
DASHBOARD, PATCHES, AUDIO, SYSTEM, NETWORK = 0, 1, 2, 3, 4
SCREENS = ("DASHBOARD", "PATCHES", "AUDIO", "SYSTEM", "NETWORK")

# Kept so a stale persisted position or an old caller resolves to something
# sane rather than raising. POWER's contents live on SYSTEM now; WAVE left the
# menu entirely (top-level destination, see the module docstring).
POWER = SYSTEM
WAVE = DASHBOARD


def screen_name(index):
    """Stable NAME for a screen index — what gets persisted. Tab ORDER is a
    layout decision and changes between releases; the name is the contract."""
    try:
        return SCREENS[index]
    except (IndexError, TypeError):
        return SCREENS[DASHBOARD]


def screen_index(name):
    """Index for a persisted screen name. Anything unrecognised — a stale
    integer from before names were used, a hand-edited value, a tab that no
    longer exists — resolves to the DEFAULT tab, never to a wrong one."""
    try:
        return SCREENS.index(name)
    except (ValueError, TypeError):
        return DASHBOARD

# Joystick-as-D-pad tuning (axes are 0..127, centre ~64).
NAV_CENTER = 64
NAV_COMMIT = 26    # push this far from centre to register one step
NAV_RELEASE = 14   # must return within this of centre before stepping again


class CompanionController:
    HOLD_SECS = 3.0          # joystick-button hold to toggle
    ARCADE_HOLD_SECS = 1.2   # arcade-button hold to toggle
    IDLE_SECS = 15.0

    def __init__(self, item_counts, hold_secs=None, idle_secs=None,
                 default_sel=None):
        # item_counts() -> list[int]: navigable item count per screen index.
        self._item_counts = item_counts
        # default_sel(screen) -> int|None: where the cursor should land when a
        # screen is ENTERED with no better information. The PATCHES screen
        # returns the index of the currently-loaded patch, so entering it never
        # parks the cursor on the alphabetically-first patch (car_mode) as if
        # that were the live one. None / no callback keeps the old row 0.
        self._default_sel = default_sel
        if hold_secs is not None:
            self.HOLD_SECS = hold_secs
        if idle_secs is not None:
            self.IDLE_SECS = idle_secs

        # On-device axis inversion without code edits (e.g. a remount): set
        # NEPTR_NAV_INVERT_X / _Y = 1.
        self.invert_x = os.environ.get("NEPTR_NAV_INVERT_X", "0") == "1"
        self.invert_y = os.environ.get("NEPTR_NAV_INVERT_Y", "0") == "1"

        self.active = False
        self.screen = DASHBOARD
        self.sel = 0

        self._press_start = None         # joystick button
        self._hold_fired = False
        self._arcade_start = None        # arcade button
        self._arcade_hold_fired = False
        self._engaged = False            # D-pad latched until the stick re-centres
        self._last_activity = 0.0

    # ----------------------------------------------------------------- helpers

    def _counts(self):
        counts = list(self._item_counts())
        while len(counts) < len(SCREENS):
            counts.append(0)
        return counts

    def _clamp_sel(self):
        """Bound the cursor to the current item count.

        An EMPTY list leaves the cursor alone rather than zeroing it. Some
        lists arrive asynchronously — the AUDIO screen's interfaces are
        fetched on a worker thread because the CLI shells out — so a clamp
        that ran against the not-yet-loaded list would silently destroy a
        restored or user-set cursor. Nothing is selectable at count 0 anyway,
        and the clamp runs again (from _refresh_interfaces) once the items
        land. Found by the 2026-08-10 cold-boot test: a position restored on
        the AUDIO tab always came back as row 0."""
        n = self._counts()[self.screen]
        if n > 0:
            self.sel = max(0, min(self.sel, n - 1))

    def enter_screen(self, screen):
        """Park the cursor where this screen's owner says it belongs (the
        active patch / interface), falling back to row 0."""
        self.screen = screen
        sel = None
        if self._default_sel is not None:
            try:
                sel = self._default_sel(screen)
            except Exception:
                sel = None
        self.sel = 0 if sel is None else int(sel)
        self._clamp_sel()

    def restore(self, screen, sel, active=False, now=0.0):
        """Put the controller back where a previous run left it (persisted UI
        position). Bounds are clamped against the CURRENT item counts, so a
        patch disappearing between runs cannot strand the cursor."""
        if DASHBOARD <= screen < len(SCREENS):
            self.screen = screen
        self.sel = max(0, int(sel))
        self._clamp_sel()
        self.active = bool(active)
        self._last_activity = now      # a restored menu gets a full idle window

    def _toggle(self, now, events):
        if self.active:
            self.active = False
            events.append("exit")
        else:
            self.active = True
            self.enter_screen(DASHBOARD)
            self._engaged = False
            self._last_activity = now
            events.append("enter")

    # ------------------------------------------------------- touch entry points
    # The touchscreen drives the same state machine as the joystick/arcade
    # gestures — these are additive, the MIDI paths above are untouched.

    def toggle(self, now):
        """Touch request to enter/leave companion mode. Returns the same
        event list update() would ("enter"/"exit")."""
        events = []
        self._toggle(now, events)
        return events

    def set_screen(self, idx, now):
        """Touch tap on a tab. Returns ["nav"] if the screen changed."""
        if not self.active or not (DASHBOARD <= idx < len(SCREENS)):
            return []
        self._last_activity = now
        if idx == self.screen:
            return []
        self.enter_screen(idx)
        return ["nav"]

    def set_sel(self, idx, now):
        """Touch tap on a list item: move the cursor there. Returns ["nav"]
        if the selection changed (activation is the caller's decision)."""
        if not self.active:
            return []
        self._last_activity = now
        n = self._counts()[self.screen]
        if n <= 0 or not (0 <= idx < n) or idx == self.sel:
            return []
        self.sel = idx
        return ["nav"]

    def touch_activity(self, now):
        """Any touch inside companion mode counts as activity (defers the
        idle-timeout sleep, exactly like a joystick nudge)."""
        if self.active:
            self._last_activity = now

    def next_deadline(self, now):
        """When update() should next run even without new input: a hold
        completing, or the idle timeout. None when nothing is pending."""
        deadlines = []
        if self._press_start is not None and not self._hold_fired:
            deadlines.append(self._press_start + self.HOLD_SECS)
        if self._arcade_start is not None and not self._arcade_hold_fired:
            deadlines.append(self._arcade_start + self.ARCADE_HOLD_SECS)
        if self.active:
            deadlines.append(self._last_activity + self.IDLE_SECS)
        return min(deadlines) if deadlines else None

    # ------------------------------------------------------------------- entry

    def update(self, now, joy_btn, joy_x, joy_y, arcade):
        events = []

        # --- joystick-button hold: toggle companion ---
        if joy_btn:
            if self._press_start is None:
                self._press_start = now
                self._hold_fired = False
            elif (not self._hold_fired
                  and now - self._press_start >= self.HOLD_SECS):
                self._hold_fired = True
                self._toggle(now, events)
                return events
        else:
            self._press_start = None
            self._hold_fired = False

        # --- arcade-button: long-hold toggles, short tap activates ---
        if arcade:
            if self._arcade_start is None:
                self._arcade_start = now
                self._arcade_hold_fired = False
            elif (not self._arcade_hold_fired
                  and now - self._arcade_start >= self.ARCADE_HOLD_SECS):
                self._arcade_hold_fired = True
                self._toggle(now, events)
                return events
        else:
            if (self._arcade_start is not None
                    and not self._arcade_hold_fired and self.active):
                # released before the hold threshold while open -> a tap
                self._last_activity = now
                events.append(("activate", self.screen, self.sel))
            self._arcade_start = None
            self._arcade_hold_fired = False

        if not self.active:
            return events

        # --- joystick D-pad: dominant axis + hysteresis, rotation-corrected ---
        # The panel is mounted rotated 270°, so the stick's raw X/Y are 90° off
        # the screen axes. Map to screen space (sh = +right, sv = +up):
        #   screen-right = stick toward -Y(raw);  screen-up = stick toward +X(raw).
        dx = joy_x - NAV_CENTER
        dy = joy_y - NAV_CENTER
        sh = -dy
        sv = dx
        if self.invert_x:
            sh = -sh
        if self.invert_y:
            sv = -sv
        ah, av = abs(sh), abs(sv)
        if self._engaged:
            if ah <= NAV_RELEASE and av <= NAV_RELEASE:
                self._engaged = False
        elif ah >= NAV_COMMIT or av >= NAV_COMMIT:
            self._engaged = True
            self._last_activity = now
            if ah >= av:
                # horizontal -> switch tab (right = next)
                target = self.screen + (1 if sh > 0 else -1)
                if DASHBOARD <= target < len(SCREENS) and target != self.screen:
                    self.enter_screen(target)
                    events.append("nav")
            else:
                # vertical -> move selection (up = previous item)
                n = self._counts()[self.screen]
                target = self.sel + (-1 if sv > 0 else 1)
                if n > 0 and 0 <= target <= n - 1:
                    self.sel = target
                    events.append("nav")

        # --- idle timeout: leave and sleep ---
        if now - self._last_activity >= self.IDLE_SECS:
            self.active = False
            events.append("sleep")

        return events
