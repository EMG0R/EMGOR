#!/usr/bin/env bash
# Pocket OpGorator monitor launcher: creates/uses a local venv, installs deps
# once, then runs main.py with whatever args are passed through.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

VENV_DIR="venv"

if [ ! -d "$VENV_DIR" ]; then
    echo "[run.sh] creating venv..."
    python3 -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"

STAMP="$VENV_DIR/.deps_installed"
if [ ! -f "$STAMP" ] || [ requirements.txt -nt "$STAMP" ]; then
    echo "[run.sh] installing dependencies..."
    pip install -q --upgrade pip
    pip install -q -r requirements.txt
    touch "$STAMP"
fi

exec python3 main.py "$@"
