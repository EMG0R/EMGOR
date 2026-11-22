# Game & Watch: Donkey Kong (gnw_dkong, model DK-52)

Nintendo Game & Watch **handheld** (Sharp SM510), 1982. Two-screen (Multi
Screen) LCD unit. The artwork pack renders the real device. **There are no
coins or credits** — this is a handheld, you just turn it on and press GAME A.

## How to start

1. Launch the game with its code (see `games.conf`):
   `ARCADE, X, ARCADE, A, ARCADE, B, ARCADE`.
2. The LCD device appears. Press **GAME A** to begin a normal game
   (GAME B is the faster, harder variant).

## Controls (device button -> what it does)

| Device button | Physical | RetroPad | Game & Watch function |
|---|---|---|---|
| Joystick left / right | stick | D-pad L/R | **Move** Mario left / right |
| ARCADE button (b1) | center big | A | **Jump** |
| A face button (b9) | right cluster | A | **Jump** (same as ARCADE) |
| Start button (b3) | left cluster | START | **GAME A** (start a game) |
| Select button (b5) | left cluster | SELECT | unused (maps to MAME "Coin", which this handheld ignores) |

Movement is left/right only; the jump button is the single action. Up/down on
the stick is bound but the game does not use vertical movement.

### The "extra" device functions (GAME B / Time / Alarm / ACL)

MAME exposes the real device's other buttons, but the **default RetroPad layout
does not place them on a button**, and the cabinet's fixed keyboard bridge
(`lib/gamepad.py`) can't reach them through `input_player1_*`:

- **GAME B** = MAME `IPT_START2` (no default RetroPad button)
- **Time**   = MAME `IPT_SELECT`
- **Alarm**  = MAME `IPT_SERVICE2`
- **ACL** (reset) = MAME `IPT_SERVICE1`

To use them, open the MAME on-screen menu (**TAB** on a keyboard, or RetroPad
**R2** in the MAME core) -> *Input (this machine)* and bind them, or just play
GAME A (the only mode the cabinet needs). They are not required for normal play.

## Infinite-life cheat

**No external `.cht` cheat file exists** for this game (it is not in the
libretro cheat database or pugsy's MAME cheat collection — Game & Watch SM510
handhelds have none), so no `cheats/MAME/gnw_dkong.cht` was written.

**BUT** MAME's driver has a built-in toggle. The hh_sm510 driver exposes
Nintendo's unpopulated factory-test feature as a dip-switch-style option:

- **"Infinite Lives (Cheat)"** — default Off.

Turn it on via the MAME menu: **TAB** (or RetroPad **R2**) ->
*Machine Configuration* (a.k.a. DIP switches) -> set **Infinite Lives (Cheat)**
to **On**. It persists in the machine's saved config. This is the correct way
to get never-game-over here; there is nothing to put in RetroArch's cheat menu.
