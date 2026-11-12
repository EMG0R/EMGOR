# UI Boot Autostart + TIMBRE Menu Sync

**Date:** 2026-06-08

**Goal:** (1) Make the Kivy face UI autostart on boot on the Pi, and (2) align the
csound engine with the UI's 8-menu layout so the TIMBRE menu (FORMANT + FORMS) is
activatable from the controller.

**Status:** Both done and confirmed working by ear after a csound engine restart.

---

## 1. UI autostart on boot

The UI was not running on boot because two pieces were missing on the Pi:

| Gap | Fix |
|-----|-----|
| Kivy never installed (the `~/kivy-venv` is `--system-site-packages` and relies on the system `python3-kivy`, which was absent — so `import kivy` failed) | `sudo apt install -y python3-kivy` (2.3.1 from trixie). `python-osc` was already in the venv. |
| `neptr-ui.service` never installed/enabled | `sudo install -m0644 ~/NEPTR_phase4/scripts/neptr-ui.service /etc/systemd/system/` → `daemon-reload` → `enable --now` |

Verify: `systemctl is-enabled neptr-ui.service` → `enabled`; `ss -ulpn | grep 8000`
shows python bound on `127.0.0.1:8000` (the OSC server binds only after Kivy fully
inits, so it's a good liveness signal).

Note: this Pi's sudoers allows passwordless sudo for general commands but **excludes
reboot/shutdown** — a cold-boot test must be run interactively (`ssh -t … 'sudo reboot'`).

## 2. TIMBRE menu — csound ↔ UI sync

**Problem:** the UI (`neptrPhase4_UI.py`) has 8 menus:

```
0=PRESETS 1=VOLUME 2=PITCH 3=TIMBRE 4=MOD 5=DRIVE 6=TIME 7=GLITCH
```

…but the engine (`csound/midi_handler.orc`) had only 7 (no TIMBRE), so every menu
index ≥ 3 was off-by-one between engine and UI. FORMS (`fms`) — fully implemented in
`effects/forms.orc` and wired into `signal_chain.orc:145` via `$FX_BYPASS(gkFormsBypass…)`
— had **no menu toggle**, so it could never be turned on. FORMANT lived only in PITCH.

**Change (`csound/midi_handler.orc`):** inserted TIMBRE at index 3 to match the UI.

- Menu-nav modulo bumped `7 → 8` (both prev/next).
- Renumbered MOD/DRIVE/TIME/GLITCH `gkCurrentMenu == 3..6` → `4..7` across every CC
  dispatch + rotation branch.
- Added TIMBRE (`gkCurrentMenu == 3`) branches wiring its two real effects to the same
  encoder slots the UI shows them in (UI TIMBRE = `[—, —, —, FORMANT, FORMS]`):

  | Slot | Effect | Toggle CC | Dry/wet CC | csound vars |
  |------|--------|-----------|------------|-------------|
  | s3   | FORMANT | 71 | 21 | `gkFormantBypass` / `gkFormantMix`, `$FX_TOGGLE(…'5)` |
  | s4   | FORMS   | 72 | 22 | `gkFormsBypass` / `gkFormsMix`, `$FX_TOGGLE(…'28)` |

  Shift params ride the existing secondary/CC29 knobs via `gkLastToggle == 5 / 28`
  (`$FX_TOGGLE` sets `gkLastToggle` on enable), so they needed no new wiring.
- FORMANT remains reachable from PITCH slot 0 as well (left as-is).

**Verification:** `csound --nosound` compile = 0 syntax errors; live UI accepted
`/menu 3`, `/fmt_state`, `/fms_state` with 0 restarts. Rollback on Pi:
`~/NEPTR_phase4/csound/midi_handler.orc.bak`.

**Important:** csound reads the orchestra only at launch — the running engine must be
restarted (`pkill -f 'csound.*neptrPhase4'` then relaunch) to pick up orc changes.

---

## Divergence note (Mac ↔ Pi)

Both `neptrPhase4_UI.py` and the `csound/` dir were **newer on the Pi** than the Mac
source when this work started (the Pi had been edited directly). The current Pi UI was
pulled back to the Mac as `neptrPhase4_UI_pi.py`, and the edited `midi_handler.orc` now
matches Pi byte-for-byte. Because `scripts/sync_to_pi.sh` uses `rsync --delete` with the
Mac as source of truth, reconcile Pi → Mac before any future sync or the newer Pi files
will be clobbered.

## Not done (separate task)

Within-menu **slot order** still differs between UI and csound in a few menus: VOLUME
(UI lists effects reversed vs csound slots), PITCH (csound has FORMANT at s0, UI does
not), and PRESETS order. The UI also has an orphan `EFFECT_PARAMS["drt"]` with no menu
or csound counterpart.
