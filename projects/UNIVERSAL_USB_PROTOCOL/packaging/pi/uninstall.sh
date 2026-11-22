#!/bin/bash
set -euo pipefail

echo "==> Stopping and removing omniplex.service (if present)"
sudo systemctl stop omniplex.service 2>/dev/null || true
sudo systemctl disable omniplex.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/omniplex.service
sudo systemctl daemon-reload 2>/dev/null || true

echo "==> Removing /usr/local/bin/omniplex"
sudo rm -f /usr/local/bin/omniplex

echo "==> Done."
