#!/bin/bash
# Run the on-screen control-mapping capture on the NEPTR panel, self-contained:
# frees the panel (stops UI + engine), runs, then ALWAYS brings both back.
#   bash ~/NEPTR_phase4/tools/map_controls_screen.sh
LOG=$HOME/NEPTR_phase4/docs/control_capture_last.log
restore() { sudo systemctl start demiurge.service neptr-ui.service; }
trap restore EXIT
sudo systemctl stop neptr-ui.service demiurge.service
SDL_VIDEODRIVER=kmsdrm SDL_AUDIODRIVER=dummy XDG_RUNTIME_DIR=/run/user/$(id -u) \
  MAP_START="${MAP_START:-}" "$HOME/kivy-venv/bin/python" "$HOME/NEPTR_phase4/tools/map_controls_screen.py" > "$LOG" 2>&1
