# Galaga (galaga) — controls & cheat notes

## Controls
Move L/R + fire. Joystick -> arrows (RetroPad d-pad), fire = RetroPad button 1
(FBNeo). Covered by the global binds in `retroarch/retroarch.cfg`: b1 (ARCADE,
KEY_X+KEY_Z+KEY_S = A+B+Y broad fire) fires, b8 (B, KEY_Z = RetroPad B) is a
dedicated fire. Start = b3 (KEY_ENTER), Coin = b5 (KEY_RIGHTSHIFT). Single-action
game — broad-fire arcade button covers fire. **No per-game `.cfg` needed.**

## Cheat
`cheats/FinalBurn Neo/galaga.cht` — "Infinite Lives", ENABLED by default.
- Constant-write (cheat_type 1 = Set To Value), 1-byte (memory_search_size 0).
- Address 39968 = 0x9820, value 6 — the FBNeo Galaga lives counter in Namco
  work RAM (0x9200-0x93FF / 0x9800 area).
- Source: **CONFIRMED** from the official FinalBurn Neo cheats repo,
  `cheats/galaga.ini` (finalburnneo/FBNeo-cheats):
  `cheat "Infinite Lives" ... 1 "Enabled", 0, 0x9820, 0x06`. This is the exact
  FBNeo core we run (fbneo_libretro.so), so the address/value match the core.

## Free Play DIP (FBNeo)
RGUI -> Quick Menu -> Core Options -> "DIP Switches". The FBNeo Galaga driver
exposes a dedicated **"Freeplay" = "On"** DIP option (separate from Coinage).
With Freeplay on, Start alone begins/continues — no coin needed.
