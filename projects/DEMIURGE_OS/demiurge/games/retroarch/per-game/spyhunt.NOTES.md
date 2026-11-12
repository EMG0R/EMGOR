# Spy Hunter — how to play on the cabinet

Game: **Spy Hunter** (`spyhunt`, FBNeo core). It's a top-down DRIVING game —
you pilot a weaponized sports car down an endless road, shooting enemy vehicles
while avoiding civilians. The original cabinet had a steering yoke, a single gas
pedal, a lo/hi gear shifter, and weapon buttons.

## Start a game

1. **Insert coin** — press **LEFT arrow button (b5)** = Coin/Select.
   (Unless Free Play is enabled on-device — see bottom.)
2. **Start** — press **RIGHT arrow button (b3)** = Start. In Spy Hunter the
   Start input is the **"Weapons Van"** button; pressing it begins the game.
3. **DRIVE** — hold the **ACCELERATE** button to go. This is the bit that felt
   missing before: gas is on **b8** (and also fires from the **arcade button
   b1**). The car only moves when you hold accelerate.

## Driving

- **Steer** — push the **joystick LEFT / RIGHT**. (Up/Down are not used for
  driving; the road auto-scrolls and accelerate sets your speed.)
- **Accelerate (gas pedal)** — **b8** (RetroPad B). Hold it to go faster.
  The **arcade button b1** also accelerates (it broadcasts A+B+Y, so b1 = gas
  + gear + missiles all at once — handy panic button, but for clean driving use
  b8).
- **Gear shift (lo / hi)** — **b9** (RetroPad A). It's a TOGGLE: tap once for
  high gear (faster, less grip), tap again for low gear. No neutral.

## Weapons (device button -> function)

| Device button | RetroPad | Spy Hunter function |
|---------------|----------|---------------------|
| **b8**        | B        | **Accelerate** (gas pedal) |
| **b9**        | A        | **Gear shift** (tap = toggle lo/hi) |
| **b7**        | Y        | **Missiles** (when equipped) |
| **b6**        | X        | **Oil slick** (when equipped) |
| **b4**        | L        | **Smoke screen** (when equipped) |
| **b2**        | R        | **Machine guns** (front-firing, always available) |
| **b3**        | Start    | **Weapons Van / Start** |
| **b5**        | Select   | **Coin** (insert credit) |
| **b1 (arcade)** | A+B+Y  | broad "go + gear + missiles" (sends A, B, Y together) |
| joystick L/R  | D-pad    | **Steer** |

Notes on weapons:
- **Machine guns (b2)** are your default weapon — always available, fire freely.
- **Oil slick / smoke screen / missiles** are *special* weapons you only have
  after collecting them from the **weapons van** (the truck you drive into; the
  Weapons Van / Start button flashes when you enter a van zone). When you don't
  have a special weapon, its button does nothing.
- **Gear** matters: high gear = top speed for open road; drop to low gear for
  tight maneuvering and when enemies swarm.

## Goal / scoring

Survive as long as possible, rack up points killing enemy vehicles (with road
hazards, you LOSE points for hitting civilian cars). Spy Hunter is **time-/run-
based, not a fixed-lives game** in the usual sense — you start with a pool of
cars and earn a brief bonus pool early; the run ends when the game timer at the
bottom expires or you run out of cars. The infinite cheat (see below) keeps you
going indefinitely.

## Cheats (auto-applied)

The cabinet auto-applies enabled cheats in `cheats/FinalBurn Neo/spyhunt.cht`
on launch (`apply_cheats_after_load = true`). That file ships TWO enabled cheats:

- `Infinite Cars (lives)` — addr 57364, write value 5, 8-bit
- `Infinite Time (freeze game timer)` — addr 57365, write value 99, 8-bit

**These two addresses are NOT verified and are almost certainly placeholders.**
Why: there is **no spyhunt cheat in the libretro cheat database** (both
`cht/FBNeo/spyhunt.cht` and `cht/MAME/spyhunt.cht` return 404 upstream), and in
this build environment the large legacy MAME `cheat.dat` databases could not be
fetched/grepped (curl is sandbox-denied; the web fetcher truncates the file
before reaching the "s" section). So I could **not** confirm the real RAM
addresses for Spy Hunter's car-count or game-timer counters. The .cht is written
in correct RetroArch format with the enable flags ON as requested, but the
addresses MUST be re-derived on-device.

### How to find the real addresses on-device (do this once, then lock them)

1. Launch Spy Hunter, coin up, start a game so cars + timer are showing.
2. RetroArch -> Quick Menu -> Cheats -> Start/Continue Cheat Search.
3. Search size = 1 byte. Note your current car count, search for that value.
   Lose/gain a car, search for the new value. Repeat until one address remains
   = the **cars** address. Add it as a cheat, value = your desired car count.
4. Repeat the search for the on-screen **game timer** number to find the timer
   address; set its cheat value high (or use a "set to constant each frame"
   type) to freeze time.
5. Quick Menu -> Cheats -> "Save Game-Specific Cheats" — RetroArch writes a
   correct `spyhunt.cht` into `cheats/FinalBurn Neo/` with the right addresses.
   Then re-enable both entries and re-save so they auto-apply on next launch.

Source / confidence: best-known behavior (Spy Hunter is time-based with a car
pool) is well-documented, but the exact RAM offsets are **unverified** here.
Closest authoritative source would be Pugsy's MAME cheats
(https://www.mamecheat.co.uk/) or the mame2003-plus `metadata/cheat.dat` — fetch
either on a machine with network access and grep `:spyhunt:` to drop in the real
address/value, replacing the placeholders above.

## Free Play (enable on-device later)

FBNeo's exposed DIP list for spyhunt (`SpyhuntDIPList[]` in
`src/burn/drv/pre90s/d_mcr3.cpp`) contains only **Game Timer** (1:00 / 1:30),
**Demo Sounds** (Off/On), and **Service Mode** (Off/On) — **there is NO "Free
Play" DIP setting exposed by FBNeo for Spy Hunter.** (Spy Hunter's coinage was
controlled by other DIP bytes that FBNeo leaves at defaults / unnamed.)

So to make it "always playable without coining up", the reliable path on this
cabinet is **not** a Free Play DIP — it's the **infinite cheat** above plus the
b5 = Coin button, OR (best) auto-credit at launch. If you want true free-play
behavior, the cleanest options are:

1. Rely on the infinite-time/cars cheat so a single coin+start lasts forever.
2. Set **Game Timer = 1:30** (the longer of the two) via the FBNeo DIP menu for
   extra runway.
3. If a later FBNeo build exposes a Free Play DIP for spyhunt, enable it in
   RetroArch -> Quick Menu -> Core Options / DIP Switches. (MAME's spyhunt
   driver may expose a "Free Play" DIP under a different name, but the MAME
   driver source could not be retrieved to confirm the exact string in this
   environment — check the on-device DIP menu.)

## Reference: source of the mapping

FBNeo `SpyhuntInputList[]` (verbatim, `src/burn/drv/pre90s/d_mcr3.cpp`):

```
"Coin"        -> p1 coin
"Weapons Van" -> p1 start
"Gear Shift"  -> p1 fire 2
"Missiles"    -> p1 fire 3
"Oil Slick"   -> p1 fire 4
"Smoke Screen"-> p1 fire 5
"Machine Gun" -> p1 fire 6
"P1 Wheel"    -> p1 x-axis  (analog, clamp 0x34..0xb4)
"P1 Accelerator" -> p1 fire 1 (analog in arcade; digital button here)
```

FBNeo libretro maps generic `p1 fire 1..6` to RetroPad **B, A, Y, X, L, R**, and
`p1 start`/`p1 coin` to Start/Select — which is exactly the table above.
