# NEPTR Phase 4 UI rewrite — "squares" edition (2026-06-12)

Custom replacement for the stock-widget Kivy UI in `neptrPhase4_UI.py`.
**Framework: pygame (SDL2) — Kivy is fully removed** (revised mid-implementation:
the user wants full pixel-level control, no widget toolkit). Every rect, font,
and frame is drawn by hand. The main loop blocks on the OSC queue, so idle CPU
is ~0% — there is no framework tick at all; the screen repaints only when OSC
changes visible state (plus glitch frames and sleep faces).

## What stays (same behavior, ported)

- OSC contract and every address (`/menu`, `/preset`, `/<code>_state`, `/<code>_<param>`)
- View-state model, burst coalescing, value dedupe, render signatures
- Sleep-after-10s, NEPTR_ROT / NEPTR_WINDOWED env config
- Background `(28, 0, 30)`, menu title + divider at top
- protoFACES sleep screen logic and integration
- Same systemd unit and Pi venv (`~/kivy-venv`, gains `pygame-ce` via pip)

## Menu view

Title at top (existing state label). Below: **5 equidistant rectangles**, one
per effect slot, always 5 so the grid never jumps (menus with fewer effects or
"—" placeholders show faint empty outlines).

- OFF: outlined rect, dim text
- ON: **inverted** — solid white fill, background-purple text
- Turning an effect on while its menu is visible: the slot inverts first,
  then ~0.35 s later the view glitches into that effect's submenu

## Effect submenu — vertical Max/MSP-style sliders

Params as side-by-side vertical columns (1–4, equally dividing the width).
Each column: tall outlined rect filling **bottom-up** with solid color
proportional to value; param name + numeric value beneath. Fixed palette:

1. magenta `(0.85, 0.20, 0.75)`
2. violet `(0.55, 0.30, 0.95)`
3. hot pink `(1.00, 0.30, 0.55)`
4. lavender `(0.72, 0.60, 1.00)`

Bypassed effect = same layout with fills/text dimmed.

## Faces — 25% faster

Same `NeutralScreen` logic and PNGs; all timings ×0.75: check interval
1.0 → 0.75 s, FACE2 flash 0.4 → 0.3 s, other-face hold 2.5 → 1.875 s.
Probabilities unchanged.

## Glitch transition — slice displacement

On every view change (not on param updates): the new view is built
immediately, then for 3 frames (~200 ms at 15 fps) each slot/slider gets a
random horizontal `Translate` offset (re-randomized per frame, one random
slice gets a magenta tint flash), then everything snaps to zero. Pure matrix
transforms — no textures, no shaders, zero idle cost.

## Files

- `neptrPhase4_UI.py` — rewritten (this is what `neptr-ui.service` runs)
- `neptrPhase4_UI_pi.py` — stale duplicate, retired to `backups/`
- `backups/` — dated copies of both pre-rewrite files
- `scripts/osc_smoketest.py` — mirror updated to current menus/effects

## Testing

Local: `NEPTR_WINDOWED=1 NEPTR_ROT=0` run on the Mac, drive with
`scripts/osc_smoketest.py`, eyeball screenshots. Pi: `./scripts/sync_to_pi.sh`,
journal check, smoketest over ssh, CPU check next to Csound.
