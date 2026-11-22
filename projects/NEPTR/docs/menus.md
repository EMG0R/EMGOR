# Menu architecture (phase 1: order is data)

## Data flow
```
neptr/menus.json  (source of truth: ordered list {id, name, slots, hidden?, conditional?})
   |-- csound/tools/gen_menus.py --> csound/handler/gen/menu_cycle.orc   (boot: giMenuCycle, giMenuCycleN, gkMenuCycleN)
   |        `-- gen_menus.py --check  (staleness/validity; run in tests)
   |-- web PUT /api/neptr/menus --> writes menus.json, runs the generator, pushes OSC
   |        /mc ii (position, menuId) per entry, then /mc_n i (count)  -> 127.0.0.1:7770
   |        -> handler/menu_reload.orc (instr 95) tablew's giMenuCycle and sets gkMenuCycleN   (live, no restart)
   `-- neptr/menus_source.py: load_menus(), menus_in_cycle_order()  (pygame UI)
```
The order of the `menus` array IS the nav cycle. Hidden menus are not in the cycle. A `conditional`
menu (HYPERGUITAR) is only walked while its device is present and must sit after the always-on menus
(nav.orc drops a trailing menu 21 while `gkHgPresent == 0`).

Current ids: 0..18 effects, **19 UTILITY**, **20 NORRIS** (touch-only SoundMagic rack page, no engine
slot branches; its five slots are `—` placeholders), **21 VOCAL** (touch-only vocal-chain page), **22 MIXER** (encoder page, see `mixer_page.md`), **23 HYPERGUITAR** (conditional, `$HG_MENU` in
hyperguitar.orc; dispatch branches use the macro, never a literal). Rule: new always-on menus go
BEFORE the conditional block; conditional ids shift (HYPERGUITAR went 20 -> 21 when NORRIS landed
2026-10-05, a one-menu renumber of dispatch via `$HG_MENU`).

**UTILITY NAM quality (2026-10-23):** tap row "NAM  FULL | MICRO" (default FULL). ch16 CC52
(127 = MICRO) -> chn_k `nam_size` (0/1, explicit `chnset 0` default in globals.orc) -> optional 5th arg of
`NAMProcess` -> `NamLoader::set_size()` (atomic store). The loader thread reloads the current profile
with `SetSlimmableSize(0.0)` (MICRO, smallest submodel) or `1.0` (FULL) and swaps the L/R pair, same path as a
profile change; the audio thread never calls SetSlimmableSize. Persisted in neptr_settings.ftab slot 340
(stored +100; absent/0 = FULL), echoed as `/nam_size`, excluded from preset recall. Calibrated
pregains (nam_pregain.conf) are for FULL; MICRO uses the same gains. Needs the rebuilt csound-nam.so installed.

Web page: `http://<demiurge-host>:8080/#/neptr/menus` (NEPTR index -> "Menu order"). Drag the grip
(mouse or touch), Hide/Show, Save. GET `/api/neptr/menus`, PUT `/api/neptr/menus` with
`{"menus":[{"id":n,"hidden":bool},...]}`: the client may only reorder/hide; ids must be unique and
the full known set; names/slots come from the file. Backend finds menus.json via `$NEPTR_DIR`, the
repo `neptr/` beside `web/`, else `~/NEPTR_phase4`.

menu_reload.orc is included after midi_handler.orc (not right after expr_registry) because it
listens on `giCtlOSC`, which midi_handler creates. After a deploy, new boot order needs the generated
file on the Pi; a live push alone does not survive an engine restart (menus.json does).

## Limitations
- Menu ids are never renumbered: dispatch branches test literal `gkCurrentMenu == N`
  (LOOPER = 14 is hardcoded in handler_looper.orc). Only order and visibility are data.
- Slot -> effect assignment is still in code (`slots` in menus.json mirrors the UI for display only).
- Only one trailing conditional menu is subtracted by nav.orc (menu 21). A second (HYPERTRUMPET)
  needs a per-menu presence flag.

## Pygame UI (do later, not done here)
```python
from menus_source import load_menus, menus_in_cycle_order
MENUS = [{"name": m["name"], "effects": ...} for m in load_menus()]   # id == list index
```
and sort by `menus_in_cycle_order()` where swipe direction/next-prev is drawn. The UI's swipe sends the
physical prev/next CC, so the ENGINE's cycle is authoritative; the `/menu` echo picks the page. The UI
never needs the order for correctness.

## Phase 2
Slot assignment registry + a generic ToggleById, so effects can move between menus from the same page
(no dispatch surgery). Target set: `docs/specs/2026-10-04-menu-families.md` (AMP, NAM, TONE, PITCH,
SPECTRAL, TIME, SPACE with one IR browser slot, TEXTURE, LOOPER, RIG, plus conditional HYPERGUITAR and
HYPERTRUMPET). That needs a per-menu presence flag generalising `gkHgPresent`.

## Current cycle order (2026-10-24)
PRESETS(0), UTILITY(19), VOLUME(1) .. AIR(13), NORRIS(20), IR VERB(18), NAM(17), CHASE BLISS(15),
GLITCH(16), LOOPER(14), MIXER(22), VOCAL(21), then HYPERGUITAR(23) (conditional, always last).
Ids unchanged; only the `menus.json` array order moved. PRESETS (0) is five user preset slots
(bank slots 1..5, shared with DUODECIMUS): release <5 s recall, 5..8 s capture, >=8 s delete,
timed in the engine (handler_menus.orc); echoes `/pocc` (saved bitmask), `/ploaded`, `/phold`.
