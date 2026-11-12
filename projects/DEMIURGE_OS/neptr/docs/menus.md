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
slot branches; its five slots are `—` placeholders), **21 HYPERGUITAR** (conditional, `$HG_MENU` in
hyperguitar.orc; dispatch branches use the macro, never a literal). Rule: new always-on menus go
BEFORE the conditional block; conditional ids shift (HYPERGUITAR went 20 -> 21 when NORRIS landed
2026-10-05, a one-menu renumber of dispatch via `$HG_MENU`).

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
