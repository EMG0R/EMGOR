# STREET FIGHTER (sf) - cabinet notes

ROM: `sf.zip` | Core: FBNeo (`fbneo_libretro.so`) | Label: `sf`
Game: **Street Fighter (World, Analog buttons)** - the original 1987 Capcom title.

## Which game is this?
`sf` is the *first* Street Fighter, not SF2. The original cabinet shipped in two
forms: a deluxe cabinet with two pressure-sensitive pads (one punch pad, one
kick pad, each reading light/medium/heavy by how hard you hit it) and a
"regular" cabinet with the same **6-button** layout later used by SF2.
The FBNeo `sf` parent set presents the 6-button layout: **3 punches + 3 kicks**.

## Starting the game
1. Launch via the arcade supervisor / games.conf entry for label `sf`
   (FBNeo core + `sf.zip`). The per-game override is applied automatically with:
   `--appendconfig demiurge/games/retroarch/per-game/sf.cfg`
2. Insert a credit: cabinet button **b5** (Select/Coin -> keyboard `rshift`).
   - If you set the Free Play DIP (below), no coin is needed.
3. Press **b3** (Start -> `enter`) to begin.

## Attack mapping (device button -> attack)
The override forces FBNeo device type **769** = "RetroPad, 6 buttons, Classic",
which puts all six attacks on RetroPad Y/X/L/B/A/R.

| Cabinet button | RetroPad | Keyboard | Street Fighter attack       |
|----------------|----------|----------|-----------------------------|
| b7             | Y        | s        | Light Punch  (Jab)          |
| b6             | X        | a        | Medium Punch (Strong)       |
| b4             | L        | [        | Heavy Punch  (Fierce)       |
| b8             | B        | z        | Light Kick   (Short)        |
| b9             | A        | x        | Medium Kick  (Forward)      |
| b2             | R        | ]        | Heavy Kick   (Roundhouse)   |
| b3             | Start    | enter    | Start                       |
| b5             | Coin     | rshift   | Insert credit               |
| b1             | A+B+Y    | -        | broad "ARCADE" (MK+LK+LP)   |
| joystick       | d-pad    | -        | move / jump / crouch        |

Mental model: **top row = punches, bottom row = kicks; left=light -> right=heavy.**
Note: on the real machine the panel ran heavy->light left-to-right, but the
mapping above follows the modern SF2 convention (light->heavy) for muscle memory.

## Cheats
File: `cheats/FinalBurn Neo/sf.cht` (RetroArch `.cht`, 3 cheats).
Enabled by default:
- P1 Infinite Energy (Health)
- P2 Infinite Energy (Health)
- Infinite Time (round timer frozen)

To toggle at runtime: Quick Menu -> Cheats -> Load Cheat File, then enable/
disable individual entries.

### Verify the cheat addresses on first run
The energy/timer addresses are taken from the MAME/FBNeo cheat record for the
`[sf]` parent set and entered as flat work-RAM byte writes. CPS-1 work-RAM
offsets can shift between core revisions. If health/time do NOT stay pinned on
this build, re-derive them with Quick Menu -> Cheats -> Start/Continue Search
(search "energy" while taking damage, "time" while the clock ticks) and update
the `*_address` / `*_code` fields. Addresses used:
- P1 energy: 16745961 (0xFF83E9)
- P2 energy: 16746363 (0xFF857B)
- Round timer: 16745670 (0xFF82C6)

## Free Play (skip coins)
The original Street Fighter exposes a **"Free Play"** DIP option (standard
Capcom DIP, set ON for free play). Set it via Quick Menu -> Core Options /
DIP Switches -> look for the entry named **"Free Play"** and set it ON. With
Free Play ON the cabinet starts on b3 (Start) without inserting a credit.
