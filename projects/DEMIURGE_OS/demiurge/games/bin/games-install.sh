#!/usr/bin/env bash
# games-install.sh — one-shot installer for the Demiurge arcade console.
#
# SELF-CONTAINED to the games folder on purpose (NOT wired into the main
# setup-script yet — that happens once this is proven on-device). Run ON THE PI:
#
#     cd ~/demiurge/games && bash bin/games-install.sh
#
# Idempotent: safe to re-run. It installs RetroArch + the Python deps the
# supervisor needs, sets up uinput + serial permissions, and drops the
# NOPASSWD sudoers line that lets freeze/thaw SIGSTOP demiurge.service.
set -euo pipefail

GAMES_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
USER_NAME="${SUDO_USER:-$USER}"

say() { printf '\n\033[1;36m== %s ==\033[0m\n' "$*"; }

say "APT packages (RetroArch + Python serial/MIDI/uinput)"
sudo apt-get update
sudo apt-get install -y \
  retroarch \
  python3-evdev python3-mido python3-rtmidi

# Libretro cores: Debian ships some as libretro-*; MAME/NES cores are most
# reliably pulled with RetroArch's own online updater. We try apt first.
say "Libretro cores (best-effort via apt; use RetroArch Online Updater for the rest)"
sudo apt-get install -y \
  libretro-fceumm libretro-nestopia 2>/dev/null || \
  echo "  (some cores not in apt — get MAME/NES cores via RetroArch > Online Updater > Core Downloader)"

say "uinput (virtual gamepad for player 1)"
sudo modprobe uinput || true
echo "uinput" | sudo tee /etc/modules-load.d/uinput.conf >/dev/null
# udev rule so the supervisor (in the 'input' group) can create the pad
echo 'KERNEL=="uinput", MODE="0660", GROUP="input", OPTIONS+="static_node=uinput"' \
  | sudo tee /etc/udev/rules.d/99-demiurge-uinput.rules >/dev/null
sudo udevadm control --reload-rules && sudo udevadm trigger || true

say "Groups for $USER_NAME (input = uinput, dialout = serial)"
sudo usermod -aG input,dialout "$USER_NAME"
echo "  (log out/in or reboot for group changes to take effect)"

say "NOPASSWD sudoers for freeze/thaw (SIGSTOP/SIGCONT demiurge.service)"
SUDOERS=/etc/sudoers.d/demiurge-arcade
sudo tee "$SUDOERS" >/dev/null <<EOF
# Arcade supervisor privileges (passwordless): stop/start the face UI to
# release/retake the screen (neptr-ui is a SYSTEM unit). The audio freeze needs
# NO sudo — demiurge.service runs as $USER_NAME, so the supervisor kill -STOPs
# its own stage processes directly.
$USER_NAME ALL=(root) NOPASSWD: /usr/bin/systemctl stop neptr-ui.service
$USER_NAME ALL=(root) NOPASSWD: /usr/bin/systemctl start neptr-ui.service
EOF
sudo chmod 0440 "$SUDOERS"
sudo visudo -cf "$SUDOERS"

chmod +x "$GAMES_DIR"/bin/demiurge-freeze "$GAMES_DIR"/bin/demiurge-thaw \
         "$GAMES_DIR"/bin/ui-release "$GAMES_DIR"/bin/ui-restore

say "Done."
cat <<EOF

Next:
  1. Drop ROMs into:        $GAMES_DIR/roms/    (see roms/README.md)
  2. Make sure live.conf has 'sync_layer = on' so game audio rides the sync layer.
  3. Stop the old bridge:   pkill -f serial_bridge.py   (the supervisor replaces it)
  4. Run the supervisor:    python3 $GAMES_DIR/arcade_supervisor.py
  5. Hold the arcade button + click the joystick for 1s  ->  console.

To run it on boot, install a user service later (docs/INSTALL.md, step 6).
EOF
