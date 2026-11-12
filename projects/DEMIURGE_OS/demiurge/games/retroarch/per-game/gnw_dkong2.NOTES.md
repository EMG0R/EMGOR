# Game & Watch: Donkey Kong II (gnw_dkong2, model JR-55)

Nintendo Game & Watch **handheld** (Sharp SM510), 1983. Multi Screen LCD unit;
the sequel to DK-52, identical control scheme. The artwork pack renders the real
device. **No coins or credits** — turn it on, press GAME A.

## How to start

1. Launch the game with its code (see `games.conf`):
   `ARCADE, X, ARCADE, A, ARCADE, Y, ARCADE`.
2. The LCD device appears. Press **GAME A** to begin (GAME B = harder variant).

## Controls (device button -> what it does)

| Device button | Physical | RetroPad | Game & Watch function |
|---|---|---|---|
| Joystick left / right | stick | D-pad L/R | **Move** left / right |
| ARCADE button (b1) | center big | A | **Jump** |
| A face button (b9) | right cluster | A | **Jump** (same as ARCADE) |
| Start button (b3) | left cluster | START | **GAME A** (start a game) |
| Select button (b5) | left cluster | SELECT | unused (maps to MAME "Coin", ignored) |

Movement is left/right; jump is the single action. Up/down is bound but unused.

### The "extra" device functions (GAME B / Time / Alarm / ACL)

Same situation as gnw_dkong — not on a default RetroPad button and not reachable
through the cabinet's keyboard bridge:

- **GAME B** = `IPT_START2` · **Time** = `IPT_SELECT` ·
  **Alarm** = `IPT_SERVICE2` · **ACL** (reset) = `IPT_SERVICE1`

Reach them via the MAME menu (**TAB**, or RetroPad **R2**) -> *Input (this
machine)* if needed. Normal play only needs GAME A.

## Infinite-life cheat

**No external `.cht` file exists** (not in the libretro cheat database or
pugsy's collection), so no `cheats/MAME/gnw_dkong2.cht` was written.

The MAME hh_sm510 driver has a built-in toggle for this game, but note it is
**Invincibility, not infinite lives**:

- **"Invincibility (Cheat)"** — default Off.

Enable via the MAME menu: **TAB** (or RetroPad **R2**) -> *Machine
Configuration* (DIP switches) -> set **Invincibility (Cheat)** to **On**. It
persists in the machine's saved config and makes the game effectively
never-game-over. Nothing goes in RetroArch's cheat menu.
