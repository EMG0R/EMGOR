# Game & Watch: Mario Bros. (gnw_mario, model MW-56)

Nintendo Game & Watch **handheld** (Sharp SM510), 1983. This is **"Mario Bros."**
— the two-player package/conveyor handheld (NOT "Mario's Cement Factory"). The
artwork pack renders the real device. **No coins or credits** — turn it on,
press GAME A.

## How to start

1. Launch the game with its code (see `games.conf`):
   `ARCADE, A, ARCADE, B, ARCADE, Y, ARCADE`.
2. The LCD device appears. Press **GAME A** to begin (GAME B = harder variant).

## Controls (device button -> what it does)

| Device button | Physical | RetroPad | Game & Watch function |
|---|---|---|---|
| Joystick (all 4 dir) | stick | D-pad | **Move** along the conveyor (one side) |
| Start button (b3) | left cluster | START | **GAME A** (start a game) |
| Select button (b5) | left cluster | SELECT | unused (maps to MAME "Coin", ignored) |
| ARCADE / A / B / X / Y | center + right | — | **no effect** (this game has no action button) |

**This game has NO action/jump button** — play is movement only. It is the
two-player versus unit: each player works one side of the line. In MAME the two
sides are two independent dual-direction controls (P1 side / P2 side). On this
one-stick cabinet you drive a single side with the d-pad; both axes are used
(the control also moves Mario between the upper/lower levels).

### The "extra" device functions (GAME B / Time / Alarm / ACL)

Not on a default RetroPad button and not reachable through the cabinet's keyboard
bridge:

- **GAME B** = `IPT_START2` · **Time** = `IPT_SELECT` ·
  **Alarm** = `IPT_SERVICE2` · **ACL** (reset) = `IPT_SERVICE1`

Reach them via the MAME menu (**TAB**, or RetroPad **R2**) -> *Input (this
machine)* if needed. Normal play only needs GAME A.

## Infinite-life cheat

**No external `.cht` file exists** (not in the libretro cheat database or
pugsy's collection), so no `cheats/MAME/gnw_mario.cht` was written.

The MAME hh_sm510 driver has TWO built-in toggles for this game:

- **"Infinite Lives (Cheat)"** — default Off.
- **"Increase Score (Cheat)"** — default Off.

Enable via the MAME menu: **TAB** (or RetroPad **R2**) -> *Machine
Configuration* (DIP switches) -> set **Infinite Lives (Cheat)** to **On** (and
optionally Increase Score). It persists in the machine's saved config. Nothing
goes in RetroArch's cheat menu.
