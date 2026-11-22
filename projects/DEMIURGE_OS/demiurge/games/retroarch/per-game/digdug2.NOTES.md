# Dig Dug II (digdug2) — controls & cheat notes

## Controls
4-way move + pump (fire). Joystick -> arrows (RetroPad d-pad), pump = RetroPad
button 1 (FBNeo). Covered by the global binds in `retroarch/retroarch.cfg`: b1
(ARCADE, KEY_X+KEY_Z+KEY_S = A+B+Y broad fire) pumps, b8 (B, KEY_Z = RetroPad B)
is a dedicated pump. Start = b3 (KEY_ENTER), Coin = b5 (KEY_RIGHTSHIFT).
Single-action game — broad-fire arcade button covers pump. **No per-game `.cfg`
needed.**

## Cheat
`cheats/FinalBurn Neo/digdug2.cht` — "Infinite Lives", ENABLED by default.
- Constant-write (cheat_type 1 = Set To Value), 1-byte (memory_search_size 0).
- Address 70 = 0x0046, held at value 2 — the Dig Dug II lives byte. Dig Dug II
  runs on different (M6809) hardware than Dig Dug, so the lives address is in
  low/zero-page work RAM, not the 0x9200 region.
- Source: **CONSTRUCTED, NOT confirmed.** There is NO curated Dig Dug II cheat in
  the libretro-database cht tree (cht/FBNeo and cht/MAME both 404) nor in the
  official FBNeo-cheats repo (digdug2.ini does not exist). This entry uses the
  documented low-RAM Dig Dug II lives address as the best available.
  **VERIFY ON HARDWARE:** if lives don't pin, use RetroArch's in-game Cheat
  search on the lives byte (Quick Menu -> Cheats -> Start Search; lose a life to
  filter Decreased) to get the exact address for this build, then update
  `cheat0_address` (and `cheat0_value` if the start-lives count differs).

## Free Play DIP (FBNeo)
RGUI -> Quick Menu -> Core Options -> "DIP Switches". The FBNeo Dig Dug II
driver exposes credit/coin DIPs; set **"Coinage" / credit DIP to "Free Play"**
(Dig Dug II's coinage DIP includes a Free Play position; it does not expose a
separate "Freeplay" toggle like Galaga/Dig Dug). With Free Play on, Start alone
begins/continues.
