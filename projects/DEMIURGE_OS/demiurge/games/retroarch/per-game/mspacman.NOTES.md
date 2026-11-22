# Ms. Pac-Man (mspacman) — controls & cheat notes

## Controls
Move only, no action button. Joystick -> arrows (RetroPad d-pad) works with the
global binds in `retroarch/retroarch.cfg`. Start = b3 (KEY_ENTER). Coin = b5
(KEY_RIGHTSHIFT) if Free Play is off. **No per-game `.cfg` needed.**

## Cheat
`cheats/FinalBurn Neo/mspacman.cht` — "Infinite Lives", ENABLED by default.
- Constant-write (cheat_type 1 = Set To Value), 1-byte (memory_search_size 0).
- Address 20020 = 0x4E14, the Pac-Man-hardware work-RAM lives counter
  (work RAM 0x4C00-0x4FFF), held at value 2.
- Source: standard libretro-database FBNeo `mspacman.cht` address. The live DB
  was UNREACHABLE in this sandbox (no network), so the entry was constructed
  from the known FBNeo Ms. Pac-Man lives address. **Verify on hardware:** if
  lives don't pin, RetroArch's in-game Cheat search at the lives byte will give
  the exact address for this build, then update `cheat0_address`.

## Free Play DIP (FBNeo)
RGUI -> Quick Menu -> Core Options -> "DIP Switches" -> **"Coinage" = "Free Play"**.
(FBNeo Pac-Man-hardware driver exposes free play under the Coinage DIP, not a
separate "Free Play" toggle.) With Free Play on, Start alone begins/continues.
