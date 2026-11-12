# PMOR Max for Live devices: UI audit and design system (2026-11-11, macbert/max)

Scope: the five generated devices (`PMOR core`, `PMOR lane drums / synth / sampler / control`) and their editor windows. Device names, grouping, varnames, wiring and behaviour are unchanged (checked: all 10 patchers, including editors, have identical patchcord graphs before and after; only presentation attributes and static text changed).

Method: `tests/render_devices.py` draws every presentation widget at 2x with Arial metrics and checks it against the design system below. The BEFORE column is that checker run on the committed patchers at 4568b0d (renders: `max/ui_renders/before/`); AFTER is the same checker on the regenerated patchers (renders: `max/ui_renders/`). The renders model Max's drawing (schematic widgets, real font metrics); they are not Live screenshots.

## 1. Findings (before)

659 presentation widgets over 10 patchers.

**Colour**
- One green palette used with no stable roles: LGRN was primary text, label text, the control engine, and meter fill; YEL was value readouts, status text, headings, hints, the sampler engine and the mix volumes; BLUE was the synth engine and also generic action buttons, the port label, preset engine menu, the "dist" label; PINK was the drums engine and also the delete / hold buttons, the port menu, the preset button. In the drums lane the group-depth dials were BLUE (synth's identity) and the volumes YEL (sampler's identity). FX engage cells alternated LGRN / YEL by bank parity with no meaning.
- Not the owner's taste: green ground / teal panels, not dark purple / violet.
- Low contrast (WCAG 2.x, computed): PINK on TEAL 4.7:1, BLUE on TEAL 5.4:1, LGRN on TEAL 7.6:1. 79 text widgets were below 7:1, including every dial label ("bl_*" in PINK / BLUE), the port label, the "dist" label, the SAVE USER PRESET heading. Disabled controls (firmware does not list the id) were painted [0.2,0.3,0.26] = 1.07:1 on TEAL, i.e. invisible instead of dimmed.
- Panel vs ground separation was 1.67:1 with 8-10 px of rounding mismatch (rr 8 / 10).
- live.* objects set only their INACTIVE-state colour attributes (`textcolor`, `bgcolor`, `bgoncolor`, `dialcolor`) plus the non-existent `textoncolor`. Per the Max 9 refpages Live paints the ACTIVE ones (`activebgcolor`, `activebgoncolor`, `activetextcolor`, `activetextoncolor`, `activedialcolor`, `activeneedlecolor`, `activefgdialcolor`), and live.menu has no `bgcolor` at all (`lcdbgcolor`, `activebgcolor`). 188 live.* objects (128 FX cells, 56 live.dials, toggles, menus) therefore showed Live's own theme colours (grey / orange) in the device chain, unrelated to the palette. Not detectable in the old renderer; now linted (`lint_devices.py`, refpage check) and checked (`render_devices.py`: LIVE_ACTIVE).
- Buttons: 5 different fills (LGRN, BLUE, YEL, PINK, engine colour) for the same widget class with no meaning (pull = BLUE, mark = YEL, preset = PINK, ...).

**Type**
- Mixed sizes 9 / 10 / 11 / 12 / 13 / 14 (337 of 525 text widgets, 64 %, at 9 pt; 84 at 10; 85 at 11; 7 at 12; 11 at 13; 1 at 14), no font family set anywhere (`fontname` absent: the font is whatever the Max default / Live theme gives, differing between comment, umenu, textbutton and live.*).
- 9 pt is below readable at Live's device height (169 px, 1x): all dial labels, readouts, FX cell names, hints, status lines.
- Dynamic strings were never sized: "pad 12 KICK | slot 3 | algo 2" in a 14 px high, 104 px wide box wraps to 2 lines and is clipped; "any ch -> ch 1 -> PMOR core" (130 px wide, 14 high) wraps and clips in all three single-channel lanes; "cells = engage (0/1) ... moves." wraps and clips in the control lane; "WIDTH PROB" wraps in its 60 px label; the MODE / ARPRATE menus (52 px) cannot hold "MANUAL" (needs 72); FX cell names PNGPG / DRONE / VOWEL / ISO-MD / SHMOD (32-34 px at 9 pt) leave 2 px in a 38 px cell, and at the readable 10 pt need 44 px cells.
- The unknown-params string "+N params this device does not know" (about 185 px) did not fit its 176 px column.

**Layout**
- 623 of 659 presentation rects (94.5 %) off any common grid (positions like 124, 62-pitch columns, y 32 / 92 / 116 / 142, widths 130 / 110 / 56 / 38 / 170).
- Card insets differ per device: core 8 px left / right, 30 top; lanes 4 left / right, 28 top; drums / synth / sampler editors 6 / 40; core editor 8 / 8; the control editor 6 / 6. Different header strip heights, so the five do not align as a chain.
- Titles left-aligned at x 10-12, each a different width, no title in the editors, not centred. The editor button is 66 x 20 (lanes) vs 70 x 22 (core, inside the card, bottom-left). Header notes at x 170 (lanes) vs a status at x 150 (core).
- Control heights: umenu 20 / 22 / 24, textbutton 20 / 22, live.text 22 / 24, live.menu 20 / 22, textedit 22, flonum 22. Dial sizes: 24, 28, 40, 56, 64, 70, 72 (7 sizes; 4 of them within 8 px of each other).
- Label / readout: label at y + h + 1, readout at + lh, left-aligned at the control's x (so text sits off-centre under a dial); label widths 44 / 52 / 62 / 84 / 100 for the same role.
- Asymmetry: drums lane row 2 ends at x 704 of 720 vs a 10 px left margin; control lane: bank FX params dials end at x 898 of 900, 2 px from the device edge and 6 px outside the card; zones started at x 12 / 396 / 730 (no shared column grid); editors' dial rows left-aligned (x 24, 40, 14 start) instead of centred; seq rows used a 56 px pitch with 52 px menus.
- Editor pages show headings only on some pages (GROUP, SEQ), in YEL at 10 pt, hint text at 9 pt in LGRN.
- Control lane: 8 meters in a 188 px pitch with labels at 9 pt in a 56 px column, a 2-line caption clipped at the bottom.

**Checker totals on the old patchers** (`render_devices.py`, 2x): 79 contrast, 15 fit, 873 font, 648 grid, 10 layout, plus 2,108 palette findings (colour attributes outside the design-system palette, incl. the missing live.* active colours).

## 2. Design system

### Palette (defined once: `THEME_HEX` in `max/devices/make_devices.py`; mirrored into `pmor_ui.js` between `//@THEME-BEGIN/END`; `test_devices_schema.py` fails if they differ)

WCAG 2.x contrast ratios (computed from the hex values). Text targets: >= 7:1 on the surface it sits on (the type scale has no small print, so the 4.5:1 allowance for small labels is not used).

| token | hex | single role | on GROUND | on PANEL | on RAISED | GROUND text on it |
|---|---|---|---|---|---|---|
| GROUND | #0D0919 | device ground, header strip, input wells (menus, dials, toggles, fields) | - | 1.2 | 1.7 | - |
| PANEL | #221A3F | cards that group controls | 1.2 | - | 1.4 | - |
| RAISED | #3A2E73 | neutral button fill | 1.7 | 1.4 | - | - |
| DIVIDER | #5B4B9E | hairlines between zones (never text) | 2.8 | 2.3 | 1.6 | - |
| TEXT1 | #F3EEFF | primary text: titles, values, menu items, button labels | 17.3 | 14.4 | 10.2 | - |
| TEXT2 | #C9BCF0 | secondary text: control labels, captions, hints | 11.2 | 9.3 | 6.6 (not used there) | - |
| ACCENT | #BE9CFF | active / selected / primary action; identity of the core device | 8.8 | 7.3 | 5.2 | 8.8 |
| DRUMS | #FF8AD1 | drums identity: title, dial arcs, primary button | 9.2 | 7.6 | 5.4 | 9.2 |
| SYNTH | #7DB8FF | synth identity | 9.5 | 7.9 | 5.6 | 9.5 |
| SAMPLER | #FFA866 | sampler identity | 10.3 | 8.6 | 6.1 | 10.3 |
| CONTROL | #4FD9C4 | control / FX identity | 11.3 | 9.4 | 6.6 | 11.3 |
| WARN | #FFD84D | warning; pressed (armed) state of a hold button | 14.2 | 11.8 | 8.3 | 14.2 |
| DANGER | #FF7A8A | destructive action fill (delete, empty bin) | 7.9 | 6.5 | 4.6 | 7.9 |
| OK | #A6E85A | connected (CONNECT button on) | 13.4 | 11.1 | 7.9 | 13.4 |
| DISABLED | #6E60A0 | firmware-unknown / disabled control (pmor_ui.js); exempt from contrast, 3.0:1 on PANEL so it still reads as present | 3.6 | 3.0 | 2.1 | - |

Rules: every text colour is TEXT1 or TEXT2, except the engine / ACCENT colour used for the device title and section heading, which are all >= 7.6:1 on their surface (title on GROUND >= 8.8). Fills that carry dark text (engine, ACCENT, DANGER, WARN, OK) carry GROUND text, 7.9:1 or better. Engine colours appear only as: title, section heading, dial arc / toggle fill, the one primary button, FX cell fill. A device never uses another engine's colour. Pressed state: neutral -> ACCENT, primary -> TEXT1, danger -> WARN.

### Surfaces and widgets
- GROUND under everything; PANEL cards (corner radius 8); controls are GROUND wells inside cards, PANEL wells in the GROUND header; DIVIDER hairlines 1 px.
- Buttons: neutral (RAISED / TEXT1), primary (engine or ACCENT fill / GROUND text), danger (DANGER fill / GROUND text). Corner radius 4 on every button, menu, field.
- live.* objects set every colour attribute they paint (active and inactive): live.text off = GROUND with TEXT1, on = engine fill with GROUND text; live.dial value arc = engine colour, face GROUND, needle TEXT1; live.menu GROUND with TEXT1, highlight ACCENT.

### Grid, sizes, type
- 4 px grid: every presentation x / y / w / h is a multiple of 4 (hairlines and the full-device ground excepted). Enforced by `render_devices.py`.
- Device: 169 px high (Live's device view) = 32 px header strip (GROUND) + 128 px card (y 32..160, 8 px side margins) + 9 px ground. Header: routing note left at x 16 (lanes / core), centred title (24 px box at y 4, x multiple of 4, centred +-0), editor button right (72 x 24 at x W - 88, y 4). The same strip in all five devices and in every editor (editor: selector left / title / page menu right, 168 px each side).
- Content margin 16 (card 8 + pad 8), gutter 8, control row 24 (all buttons, menus, fields, toggles), label and readout rows 16.
- Perform-view dials: 28 px, columns 60 px on a shared grid from x 140 (sidebar x 16..128, hairline x 132), two rows at y 36 and y 96 (dial 28 + label 16 + readout 16 = 60 px). Labels and readouts are centred on the control.
- Editor dials: 64 px (primary), 40 px (secondary / sequencer / global); pitch 112 px (pages) and 96 px (sequencer rows); every row is centred on the window (x = 360 +- n * pitch / 2) with one label baseline per row.
- Font: Arial everywhere (`FONT`; Live substitutes its UI font). Type scale: 14 bold (title), 12 bold (section heading), 11 (body: menus, buttons, values), 10 (labels, captions, readouts). Nothing below 10.

## 3. Fixes applied

Generator (`max/devices/make_devices.py`): palette + type + grid constants; helpers `label` / `title` / `divider` / `menu` / `button` (kinds) / `hold`; every device rebuilt on them; `bank_ctl`, `ctl`, `seq_page`, `selector`, `pager`, `save_row`, `livectl` (+ `live_text_attrs`, `live_dial_attrs`, `live_menu_attrs`), `lane_common`, all five devices and five editors.
- Header strip and card identical in all five devices; titles centred, engine-coloured; editor button in the header, right.
- Core: status line, mode / CONNECT / port, progress, three centred text lines; CONNECT on = OK.
- Lanes: shared sidebar + 60 px column grid; one colour per device; group depth dials no longer in another engine's colour; toggles 52 x 28 aligned to the dial row; captions rewritten to fit two lines.
- Control lane: three zones split by hairlines (meters 16..280, FX browser 296..676, bank params 688..884), FX cells 44 x 24 at 10 pt (were 38 x 22 at 9 pt, names clipped), all engage cells in CONTROL (were alternating), bank params fully inside the card, shorter caption.
- Editors: header (selector / title / page menu), selection info right-aligned in the card top row, per-page headings on every page, centred rows, seq pages 96 px pitch with 80 px menus, synth GEN / sampler SEQ in rows of 6 / 7; core editor in two equal cards with shared row positions (y 64 / 96 / 128 / 160), 80 px global pitch, symmetric recycle bin row.
- pmor_ui.js: DIM = THEME.DISABLED (was 1.07:1), "+N unknown params" (fits its column).
- Tooling: `render_devices.py` rewritten (Pillow text render at 2x, per-page editor renders, 8 check classes), `lint_devices.py` checks live.* colour attribute names against the Max refpages, `test_devices_schema.py` checks the JS palette mirror, `tests/test_compat_ui.js` and `tests/max_dials/run_gate.sh` expect the DISABLED colour.

## 4. Results (after)

`render_devices.py`: 0 problems over 10 patchers (19 render pages). Before: 3,733 findings (see above). All device tests and `check.sh --host` pass except `tests/max_dials/run.sh`'s synth-dial step (`s8` / `s9`), which also failed at 4568b0d (it expects the pre-bank synth dials `s<k>` that pmor_ui.js no longer serves).

## 5. Not verifiable without Live (open)

- Real Live drawing: the renders model Max. live.* active-state attribute names come from the Max 9 refpages; whether Live's device theme overrides them or draws the panel-appearance dial differently is unconfirmed until the rebuilt devices are looked at in Live.
- Font: `Arial` is set explicitly; Live may substitute Ableton Sans (different metrics: widths here use Arial, text in Live may be a few px narrower or wider). Fit checks have 2-4 px slack per widget.
- The umenu / live.menu drop-down popups are drawn by the OS / Live, not themeable from the patcher.
- Runtime strings (status, pool, store, saveinfo, selection info) can exceed their boxes for unusual values; boxes are sized for the longest known strings (SAMPLES in `render_devices.py`); the core editor's pool-info comment is 2 lines high.
- Bank 1 has no MIX param (firmware), so the third column of "bank FX params" is empty on bank 1 by design.
- `usebgoncolor` is set on textbuttons (refpage: "Use Background On Color"); the CONNECT on-colour (OK green) needs this to show.

## 4. Layout pass 2 (2026-11-12): strips hold the essentials, the window holds everything

Verdict on pass 1: "feels vibecoded". Pass 2 changes structure, not the palette (`THEME_HEX` kept; four dark group tints `GRP_KICK / GRP_SNARE / GRP_HAT / GRP_PERC` added as surfaces behind TEXT1 for the pad grid).

One component set, every element has one reason:
- **Strip** (169 px): header (routing note left, centred title) + PANEL card + the **EXPAND column**: 96 x 112 engine-colour button at the right edge (14 bold, `EXPAND` <-> `MINIMIZE`), the same place in all five devices. Core: status, SET SYNC lines, CONNECT, (adopt / load on PMOR only on `patchdiff`). Drums: pad selector + KICK SNARE HAT PERC MASTER. Synth: ALGO + VOLUME CUTOFF RESO RELEASE. Sampler: slot selector + VOLUME PITCH FORMANT CUTOFF (the schema has no per-slot volume). Control: FX bank / effect / distortion menus, the bank's 16 engage cells, per-bank AMOUNT / LIDAR / MIX. Perform controls no longer drawn (drum sound / length / pitch / tone / volume / reverb, group depths, algo knobs, amp env, filter env amount, sampler width / prob / poly / reverse) still exist as Live parameters (automation list) and are edited in the window.
- **Window** (`p editor`, native resizable window, remembers size + position in a pattr): title, a `tab` bar, one PANEL card. Pages are shown / hidden by pmor_bank.js (`page n`); every presentation object belongs to one page (`bh<page>_` names or `PAGEOF` in pmor_bank.js, `pages.json`). Drums: PAD GROUP MIX SEQ DEEP SAVE. Synth: SOUND OSC AMP FILTER GEN LFO SAVE. Sampler: SLOT MIX SEQ ENV POOL. Control: METERS FX. Core: LINK SAMPLES PRESETS GLOBAL BIN.
- **Grid cell** (textbutton, 72 x 48 or 84 x 28): the drum PAD page is a 4 x 4 grid in PMOR's physical layout (TL PERC, TR HAT, BL KICK, BR SNARE), each cell shows its pad's sound in the group tint, the selected cell is ACCENT; beside it the 24 sound chips of the selected pad's group (header names the group; the current sound is ACCENT; click sets it). Sampler SLOT = 4 x 4 slot cells. Synth SOUND = 8 algorithm cards (header = algorithm, line = the four knobs it gives), selected ACCENT.
- Selected = ACCENT everywhere (tab, cell, chip, card). Section heading = engine colour 12 bold. Dials, menus, buttons unchanged.
Renders: `max/ui_renders/` (one PNG per page). Checked by `render_devices.py` (0 problems): contrast, grid, fit, overlap per page.

