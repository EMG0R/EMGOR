#!/usr/bin/env python3
"""Headless render test for neptrPhase4_UI (pygame rewrite).

Runs with SDL's dummy video driver — no window, no Pi needed. Feeds OSC
messages straight into the UI state machine, renders each view, and writes
PNGs to --out (default /tmp/neptr_ui_shots) for eyeballing. Exits non-zero
if any view fails to render.

Usage:
    python scripts/ui_render_test.py [--out DIR]
"""

import argparse
import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import pygame  # noqa: E402

import neptrPhase4_UI as UI  # noqa: E402  (import is side-effect free; main() not called)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/neptr_ui_shots")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    pygame.display.init()
    pygame.font.init()
    pygame.display.set_mode((1, 1))

    ui = UI.NeptrUI()
    ui.load_faces()

    shots = 0

    def snap(name):
        nonlocal shots
        path = os.path.join(args.out, f"{shots:02d}_{name}.png")
        pygame.image.save(ui.canvas, path)
        shots += 1
        print(f"  saved {path}")

    # --- every menu view, nothing on -----------------------------------
    for i, menu in enumerate(UI.MENUS):
        UI.route_osc(ui, "/menu", i)
        assert ui.render(), f"menu {i} did not render"
        ui.trans_blocks = []  # not testing animation here
        ui.canvas.blit(ui.clean, (0, 0))
        snap(f"menu{i}_{menu['name']}")

    # --- NORRIS page (SoundMagic rack, menu 20) + deep sheet --------------------
    UI.route_osc(ui, "/menu", UI.NORRIS_MENU)
    for fx, on in ((0, 1), (3, 1), (7, 1), (10, 0)):
        UI.route_osc(ui, "/sm_state", (fx, on))
    ui.sm.focus = 3
    UI.route_osc(ui, "/sm_p", (3, 0, 72.0))
    ui._last_rendered_view = None
    assert ui.render(), "NORRIS page did not render"
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("menu20_NORRIS_soundmagic")

    ui.sm.open_sheet(3)
    assert ui.render(), "SoundMagic deep sheet did not render"
    snap("menu20_NORRIS_deep_sheet")
    ui.sm.close_sheet()
    ui._last_rendered_view = None

    # --- TIME menu (menu 7) with HABIT inverted (ON, during the dwell) ----------
    UI.route_osc(ui, "/menu", 7)
    ui.render()
    UI.route_osc(ui, "/hab_state", 0)
    assert ui.pending_switch_code == "hab", "invert dwell not armed"
    ui.render()
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("menu7_TIME_habit_inverted")

    # --- HABIT effect view, 4 sliders at distinct values ----------------
    ui.enter_effect_view("hab")
    for addr, v in (("/hab_mix", 0.8), ("/hab_size", 0.5),
                    ("/hab_repeats", 0.25), ("/hab_scan", 1.0)):
        UI.route_osc(ui, addr, v)
    assert ui.render(), "hab effect view did not render"
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("effect_hab_active")

    # --- same view bypassed (dimmed) ------------------------------------
    UI.route_osc(ui, "/hab_state", 1)   # view returns to TIME menu
    ui.current_view = "hab"             # force the dimmed param view
    ui._last_rendered_view = None
    ui.render()
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("effect_hab_bypassed")

    # --- single fat slider (VOLUME) and 2-col (NAM) ----------------------
    UI.route_osc(ui, "/vol_state", 0)
    UI.route_osc(ui, "/vol_level", 0.62)
    ui.render()
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("effect_vol_single")

    UI.route_osc(ui, "/nam2_state", 0)   # PEAVEY active
    UI.route_osc(ui, "/nam_wet", 0.9)
    ui.render()
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("menu8_NAM_peavey_lit")

    # --- presets menu with preset 2 lit ----------------------------------
    UI.route_osc(ui, "/nam2_state", 1)
    UI.route_osc(ui, "/menu", 0)
    UI.route_osc(ui, "/preset", 2)
    ui.render()
    ui.trans_blocks = []
    ui.canvas.blit(ui.clean, (0, 0))
    snap("menu0_PRESETS_solid_dist_lit")

    # --- one block-dissolve frame (deterministic-ish seed) ----------------
    import random
    random.seed(7)
    UI.route_osc(ui, "/menu", 5)
    ui.render()              # arms the dissolve (view changed), batch 1 drawn
    snap("transition_frame")

    # --- sleep faces -------------------------------------------------------
    ui.enter_sleep()
    ui.face_current = 0
    ui.render()
    snap("sleep_face1")
    ui.face_current = 4
    ui.render()
    snap("sleep_face5")

    print(f"render test OK — {shots} shots in {args.out}")


if __name__ == "__main__":
    main()
