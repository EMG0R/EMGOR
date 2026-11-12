# Game lineup & cores

All early-80s arcade titles run on a MAME-family core; the console games run on
their platform cores. RetroArch loads the right core per ROM. You supply ROMs.

| Game | Platform | Core (suggested) | Typical ROM | 4-way OK? |
|---|---|---|---|---|
| Galaga | Arcade | MAME / FBNeo | `galaga.zip` | yes (2-way + fire) |
| Ms. Pac-Man | Arcade | MAME / FBNeo | `mspacman.zip` | yes (4-way) |
| Dig Dug | Arcade | MAME / FBNeo | `digdug.zip` | yes (4-way) |
| Spy Hunter *(car + motorcycle attackers)* | Arcade | MAME | `spyhunt.zip` | drive (works on stick) |
| Donkey Kong | Arcade | MAME / FBNeo | `dkong.zip` | yes |
| Centipede | Arcade | MAME | `centiped.zip` | yes (built for trackball; stick works) |
| Frogger | Arcade | MAME / FBNeo | `frogger.zip` | yes (4-way) |
| Mario Bros. | Arcade | MAME / FBNeo | `mario.zip` | yes (2-way + jump) |
| Tetris | NES | Nestopia / FCEUmm | `Tetris.nes` | yes |
| Super Mario Bros. ("Mario") | NES | Nestopia / FCEUmm | `Super Mario Bros.nes` | yes |
| Flappy Bird | Homebrew | flappybird libretro | core-bundled | 1 button |

> "Mario" was ambiguous in the brief — listed both the **arcade Mario Bros.** and
> the **NES Super Mario Bros.** Keep whichever you meant (or both).

## Two controls, every game

Only the joystick + the arcade button are wired. That covers all of these:
movement on the stick, the single action (fire / jump / flap / drive-action) on
the arcade button. Spy Hunter's extra weapons are reachable but secondary — fine
for a one-button cabinet.

## Continue-on-death (free-play)

Arcade games don't have a universal "continue" — the real mechanism is
**free-play + auto-coin** so you can always start another credit on death:

1. Launch the game once. In RetroArch: **Quick Menu → Core Options** (or the
   game's in-emulator service/DIP menu) set **Free Play = On** where available.
2. The supervisor can pulse `BTN_START`/`BTN_SELECT` (coin/start) on launch and on
   game-over — wired in `gamepad.py` (`pulse()`); enabling the auto-pulse loop is
   a STATUS item.
3. Settings persist via NVRAM in `saves/`.

NES games (Tetris, SMB) just restart from their own menus — no coins.

## High scores

MAME-family games keep high scores in **NVRAM**, which RetroArch autosaves to
`saves/` (and reloads on launch) — so scores survive reboots for real. For cores
that need the explicit hiscore plugin, enable it in the core options and place
`hiscore.dat` in `system/`.
