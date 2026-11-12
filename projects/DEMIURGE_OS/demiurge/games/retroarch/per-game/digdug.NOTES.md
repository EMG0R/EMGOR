# Dig Dug (digdug) — controls & cheat notes

## Controls
4-way move + pump (fire). Joystick -> arrows (RetroPad d-pad), pump = RetroPad
button 1 (FBNeo). Covered by the global binds in `retroarch/retroarch.cfg`: b1
(ARCADE, KEY_X+KEY_Z+KEY_S = A+B+Y broad fire) pumps, b8 (B, KEY_Z = RetroPad B)
is a dedicated pump. Start = b3 (KEY_ENTER), Coin = b5 (KEY_RIGHTSHIFT).
Single-action game — broad-fire arcade button covers pump. **No per-game `.cfg`
needed.**

## Cheat
`cheats/FinalBurn Neo/digdug.cht` — "Infinite Lives", ENABLED by default.
- Constant-write (cheat_type 1 = Set To Value), 1-byte (memory_search_size 0).
- Address 37393 = 0x9211, held at value 2 — the Dig Dug lives byte in the Namco
  Galaga/Dig Dug hardware work RAM (0x9200-0x93FF, same region family as Galaga).
- Source: **CONSTRUCTED, NOT confirmed.** There is NO curated Dig Dug cheat in
  the libretro-database cht tree (cht/FBNeo and cht/MAME both 404) nor in the
  official FBNeo-cheats repo (digdug.ini does not exist). Galaga's confirmed
  lives byte sits at 0x9820 in this work-RAM family; this entry uses the
  long-documented MAME Dig Dug lives address (0x9211) as the best available.
  **VERIFY ON HARDWARE:** if lives don't pin, use RetroArch's in-game Cheat
  search on the lives byte (Quick Menu -> Cheats -> Start Search; lose a life to
  filter Decreased) to get the exact address for this build, then update
  `cheat0_address` (and `cheat0_value` if the start-lives count differs).

## Free Play DIP (FBNeo)
RGUI -> Quick Menu -> Core Options -> "DIP Switches". The FBNeo Dig Dug driver
exposes a dedicated **"Freeplay" = "On"** DIP option. With Freeplay on, Start
alone begins/continues — no coin needed.
