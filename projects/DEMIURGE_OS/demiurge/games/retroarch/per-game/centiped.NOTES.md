# Centipede (centiped) — controls & cheat notes

## Controls
Originally a trackball game; joystick + fire works fine. Move = joystick ->
arrows. **Fire = RetroPad B (FBNeo button 1).** This is covered by the global
binds: b1 (ARCADE, KEY_X+KEY_Z+KEY_S = A+B+Y broad fire) fires, and b8 (B,
KEY_Z = RetroPad B) is the dedicated fire. **Fire confirmed via default binds —
no per-game `.cfg` needed.**

## Cheat
`cheats/FinalBurn Neo/centiped.cht` — "Infinite Lives", ENABLED by default.
- Constant-write (cheat_type 1 = Set To Value), 1-byte (memory_search_size 0).
- Address 165 = 0x00A5, the Centipede (Atari 6502) zero-page lives counter,
  held at value 2.
- Source: standard libretro-database FBNeo `centiped.cht` address. The live DB
  was UNREACHABLE in this sandbox (no network), so the entry was constructed
  from the known FBNeo Centipede lives address. **Verify on hardware:** if lives
  don't pin, use RetroArch's in-game Cheat search on the lives byte to get the
  exact address for this build, then update `cheat0_address`.

## Free Play DIP (FBNeo)
RGUI -> Quick Menu -> Core Options -> "DIP Switches". Centipede's Atari driver
exposes coin/credit DIPs; set **"Coinage" / credit DIP to "Free Play"** (the
Atari Centipede coinage DIP includes a Free Play position). With Free Play on,
Start alone begins/continues.
