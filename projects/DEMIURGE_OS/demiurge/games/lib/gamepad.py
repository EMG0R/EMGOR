"""
gamepad.py — console-mode input: Teensy joystick/button -> virtual KEYBOARD.

We emit a uinput KEYBOARD (not a gamepad) because RetroArch binds keyboard keys
deterministically with no per-device "autoconfig" profile — a custom uinput
*gamepad* needs an autoconfig matching its name + exact button indices, which is
fragile and was why the controls did nothing. A keyboard + explicit
`input_player1_*` binds in retroarch.cfg always works, for the menu AND in-game.

EVERY physical button is mapped (see docs/BUTTONS.md). Paired with
retroarch/retroarch.cfg binds:

    joystick X/Y (12-bit)  ->  arrows (UP/DOWN/LEFT/RIGHT)      = RetroPad d-pad
    b1 (ARCADE)            ->  KEY_X + KEY_Z + KEY_S = RetroPad A+B+Y (BROAD FIRE
                               so the main button fires in ANY game, whichever
                               RetroPad button that core uses for fire)
    b9 (A)                 ->  KEY_X       = RetroPad A
    b8 (B)                 ->  KEY_Z       = RetroPad B
    b6 (X)                 ->  KEY_A       = RetroPad X   (fighters)
    b7 (Y)                 ->  KEY_S       = RetroPad Y   (fighters)
    b3 (RIGHT, left side)  ->  KEY_ENTER      = RetroPad Start  (distinct button)
    b5 (LEFT, left side)   ->  KEY_RIGHTSHIFT = RetroPad Select / Coin (distinct)
                               (per-game FREE PLAY is enabled so Start alone
                                begins + continues after death; Select inserts a
                                coin manually if ever needed.)
    b4 (UP, left side)     ->  KEY_LEFTBRACE  = RetroPad L
    b2 (DOWN, left side)   ->  KEY_RIGHTBRACE = RetroPad R

RetroArch reads evdev directly via its udev input driver, so it sees this device
regardless of KMS focus.

The joystick click (jb) is NOT a game control — the supervisor uses jb-hold to
exit a game, and reads the launch-code buttons from the SERIAL frame
(SequenceDetector), so gameplay never trips the launcher.

Requires python3-evdev + writable /dev/uinput (input group + udev rule).
"""

from __future__ import annotations


class VirtualGamepad:
    """Virtual keyboard 'Demiurge Arcade P1' driven from Teensy frames."""

    # Rotate the joystick direction (degrees clockwise) before mapping to arrows,
    # so the stick lines up with a rotated (portrait) screen. Screen coords, y-down.
    _ROT = {
        0:   lambda x, y: (x, y),
        90:  lambda x, y: (-y, x),
        180: lambda x, y: (-x, -y),
        270: lambda x, y: (y, -x),
    }

    def __init__(self, deadzone: int = 900, center: int = 2048, rotation: int = 0,
                 invert_x: bool = False, invert_y: bool = False, game: str = ""):
        from evdev import UInput, ecodes as e  # lazy import
        self._e = e
        self.deadzone = deadzone
        self.center = center
        self.rotation = rotation if rotation in self._ROT else 0
        # Applied AFTER rotation. A pure up/down (or L/R) swap is a reflection,
        # not a rotation, so these are needed to match any physical stick mount.
        self.invert_x = invert_x
        self.invert_y = invert_y
        self.game = game

        # All button keys. KEY_SEMICOLON = RetroPad R2 (analog accelerate, e.g. Spy
        # Hunter gas). NOTE: do NOT use KEY_P for R2 — 'p' is RetroArch's pause
        # hotkey, so the gas button would pause the game. Arrows = d-pad (joystick).
        self._btn_keys = [e.KEY_X, e.KEY_Z, e.KEY_A, e.KEY_S,
                          e.KEY_LEFTBRACE, e.KEY_RIGHTBRACE, e.KEY_SEMICOLON,
                          e.KEY_ENTER, e.KEY_RIGHTSHIFT, e.KEY_GRAVE]
        self._ff = False   # hold fast-forward (KEY_GRAVE) — boot-skip on launch
        self._spy_gear = "low"   # Spy Hunter auto-gear state (low=medium, high=fast)
        self._gear_pulse = 0     # frames left to hold the gear-shift toggle
        keys = [e.KEY_UP, e.KEY_DOWN, e.KEY_LEFT, e.KEY_RIGHT] + self._btn_keys
        self._all_keys = keys
        self.ui = UInput({e.EV_KEY: keys}, name="Demiurge Arcade P1",
                         vendor=0xDEEE, product=0x0002, version=1)
        self._state = {k: 0 for k in keys}

        # Per-game physical-button -> RetroPad-key profiles. Pick the game's
        # profile if it has one, else the default. KEY meanings (via retroarch.cfg
        # binds): X=A, Z=B, A=X, S=Y, [=L, ]=R, P=R2, enter=Start, rshift=Select.
        D = {  # default (most games): broad-fire arcade + A/B/X/Y/L/R + start/select
            "b1": [e.KEY_X, e.KEY_Z, e.KEY_S],  # ARCADE = broad fire (A+B+Y)
            "b9": [e.KEY_X], "b8": [e.KEY_Z], "b7": [e.KEY_S], "b6": [e.KEY_A],
            "b4": [e.KEY_LEFTBRACE], "b2": [e.KEY_RIGHTBRACE],
            "b3": [e.KEY_ENTER], "b5": [e.KEY_RIGHTSHIFT],
        }
        # Spy Hunter: GAS is the JOYSTICK (push up = accelerate/fast, handled in
        # update()). SHOOT (machine guns) = the A button (b9) + arcade. FBNeo
        # digital order B/A/Y/X/L/R = Van/Gear/Missiles/Oil/Smoke/Guns.
        # Gear is AUTO (managed by the speed zones in update), so no manual gear
        # button — the right cluster is all SHOOT/weapons.
        SPYHUNT = {
            "b9": [e.KEY_RIGHTBRACE], # SHOOT machine guns (R) = A button
            "b1": [e.KEY_RIGHTBRACE], # arcade also shoots
            "b8": [e.KEY_RIGHTBRACE], # also shoot (right cluster)
            "b7": [e.KEY_S],          # MISSILES (Y)
            "b6": [e.KEY_A],          # OIL SLICK (X)
            "b4": [e.KEY_LEFTBRACE],  # SMOKE SCREEN (L)
            "b2": [e.KEY_Z],          # WEAPONS VAN (B)
            "b3": [e.KEY_ENTER],      # Start
            "b5": [e.KEY_RIGHTSHIFT], # Coin
        }
        self._profiles = {"spyhunt": SPYHUNT, "_default": D}
        self._btnmap = self._profiles.get(game, D)

    def set_ff(self, on: bool) -> None:
        """Hold/release fast-forward (boot-skip). Applied on the next update()."""
        self._ff = bool(on)

    def set_game(self, game: str) -> None:
        """Switch to another game's control profile (used on game-switch)."""
        self.game = game
        self._btnmap = self._profiles.get(game, self._profiles["_default"])
        self._spy_gear = "low"   # assume low gear at game start
        self._gear_pulse = 0
        self.release_all()

    def _set(self, code, pressed: int) -> bool:
        """Emit a key edge only on change. Returns True if it changed."""
        if self._state.get(code, 0) == pressed:
            return False
        self.ui.write(self._e.EV_KEY, code, pressed)
        self._state[code] = pressed
        return True

    def _axis(self, raw: int):
        """12-bit reading -> -1 / 0 / +1 about center with deadzone."""
        d = raw - self.center
        if d <= -self.deadzone:
            return -1
        if d >= self.deadzone:
            return 1
        return 0

    def update(self, frame: dict) -> None:
        """Compute every desired key into one `want` set, then apply once."""
        e = self._e
        want = set()

        # Raw axes in screen coords: x+ = right, y+ = down. Rotate + reflect.
        x = self._axis(jx) if (jx := frame.get("jx")) is not None else 0
        y = self._axis(jy) if (jy := frame.get("jy")) is not None else 0
        x, y = self._ROT[self.rotation](x, y)
        if self.invert_x:
            x = -x
        if self.invert_y:
            y = -y
        if x < 0:
            want.add(e.KEY_LEFT)
        if x > 0:
            want.add(e.KEY_RIGHT)
        if self.game == "spyhunt":
            # Driving with 3 ANALOG speed zones from the stick's Y position
            # (steer stays on L/R; d-pad up/down not sent):
            #   bottom 20% of travel  -> STOP (no gas)
            #   middle 60%            -> MEDIUM (gas + LOW gear)
            #   top 20%               -> HIGH (gas + HIGH gear)
            # gas = R2 (semicolon); gear (RetroPad A = KEY_X) is auto-toggled when
            # the zone crosses medium<->high (Spy Hunter's gear is a toggle).
            rawx, rawy = frame.get("jx"), frame.get("jy")
            ny = (rawy - self.center) / self.center if rawy is not None else 0.0
            nx0 = (rawx - self.center) / self.center if rawx is not None else 0.0
            nx0, ny = self._ROT[self.rotation](nx0, ny)
            if self.invert_y:
                ny = -ny
            p = max(0.0, min(1.0, (-ny + 1.0) / 2.0))   # 0 = full down, 1 = full up
            if p >= 0.2:
                want.add(e.KEY_SEMICOLON)               # gas on (medium/high)
                desired = "high" if p >= 0.8 else "low"
                if desired != self._spy_gear:
                    self._gear_pulse = 3                 # hold the toggle a few frames
                    self._spy_gear = desired
            if self._gear_pulse > 0:
                want.add(e.KEY_X)                        # pulse gear shift (RetroPad A)
                self._gear_pulse -= 1
        else:
            if y < 0:
                want.add(e.KEY_UP)
            if y > 0:
                want.add(e.KEY_DOWN)

        # Buttons via the active per-game profile.
        for field, codes in self._btnmap.items():
            if frame.get(field, 0):
                want.update(codes)

        # Boot-skip fast-forward (held by the supervisor for the first seconds).
        if self._ff:
            want.add(e.KEY_GRAVE)

        # Apply to every managed key (emit-on-change only).
        changed = False
        for code in self._all_keys:
            changed |= self._set(code, 1 if code in want else 0)
        if changed:
            self.ui.syn()

    def insert_coin(self) -> None:
        """Tap Coin/Select once (used for the no-cheat games' coin burst)."""
        self.pulse(self._e.KEY_RIGHTSHIFT, hold=0.04)

    def pulse(self, code, hold: float = 0.06) -> None:
        import time
        self._set(code, 1)
        self.ui.syn()
        time.sleep(hold)
        self._set(code, 0)
        self.ui.syn()

    def release_all(self) -> None:
        for code in list(self._state):
            self._set(code, 0)
        self.ui.syn()

    def close(self) -> None:
        try:
            self.release_all()
            self.ui.close()
        except Exception:
            pass
