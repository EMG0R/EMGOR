#!/usr/bin/env bash
# Pi boot cleanup: no pink Plymouth, no rainbow, no berry logo — just the code.
# Safe to re-run. Backs up cmdline.txt + config.txt with .bak.bootify suffix.
set -euo pipefail

CMDLINE=/boot/firmware/cmdline.txt
CONFIG=/boot/firmware/config.txt

[ -f "$CMDLINE" ] || { echo "ERR: $CMDLINE missing"; exit 1; }
[ -f "$CONFIG" ]  || { echo "ERR: $CONFIG missing"; exit 1; }

echo "[1/4] Backing up boot files..."
sudo cp -n "$CMDLINE" "${CMDLINE}.bak.bootify"
sudo cp -n "$CONFIG"  "${CONFIG}.bak.bootify"

echo "[2/4] Disabling rainbow splash in config.txt..."
if ! grep -qE '^disable_splash=1' "$CONFIG"; then
  echo 'disable_splash=1' | sudo tee -a "$CONFIG" >/dev/null
fi

echo "[3/4] Cleaning cmdline.txt (one line, remove quiet/splash/plymouth, add logo.nologo)..."
# cmdline.txt MUST remain a single line.
sudo sed -i -E '
  s/\bquiet\b//g;
  s/\bsplash\b//g;
  s/\bplymouth\.ignore-serial-consoles\b//g;
  s/\bvt\.global_cursor_default=[0-9]+\b//g;
  s/[[:space:]]+/ /g;
  s/^[[:space:]]+//;
  s/[[:space:]]+$//;
' "$CMDLINE"
if ! grep -q 'logo.nologo' "$CMDLINE"; then
  sudo sed -i 's/$/ logo.nologo/' "$CMDLINE"
fi

echo "[4/4] Masking Plymouth services (if present)..."
for svc in plymouth-start.service plymouth-quit.service plymouth-quit-wait.service plymouth-read-write.service; do
  sudo systemctl mask "$svc" 2>/dev/null || true
done

echo
echo "=== Result ==="
echo "cmdline.txt:"; cat "$CMDLINE"
echo
echo "config.txt splash lines:"; grep -E 'splash' "$CONFIG" || echo "(none)"
echo
echo "Plymouth service state:"
systemctl is-enabled plymouth-start.service 2>/dev/null || echo "plymouth-start: not present/masked"
echo
echo "Done. Reboot to see raw kernel boot text."
