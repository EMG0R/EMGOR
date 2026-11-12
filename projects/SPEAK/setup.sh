#!/usr/bin/env bash
# SPEAK companion setup — run once on Mac or Pi
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Creating virtual environment..."
python3 -m venv .venv

echo "==> Installing dependencies..."
.venv/bin/pip install --upgrade pip -q
.venv/bin/pip install -r requirements.txt -q

MODEL_DIR="$SCRIPT_DIR/models"
MODEL_FILE="$MODEL_DIR/Hermes-3-Llama-3.1-8B.Q4_K_M.gguf"

if [ -f "$MODEL_FILE" ]; then
    echo "==> Model already present, skipping download."
else
    echo "==> Downloading Hermes-3-Llama-3.1-8B Q4_K_M (~4.9GB)..."
    mkdir -p "$MODEL_DIR"
    .venv/bin/python3 -c "
from huggingface_hub import hf_hub_download
hf_hub_download(
    repo_id='NousResearch/Nous-Hermes-2-Mistral-7B-DPO-GGUF',
    filename='Nous-Hermes-2-Mistral-7B-DPO.Q4_K_M.gguf',
    local_dir='$MODEL_DIR',
)
print('Model downloaded.')
"
fi

echo ""
echo "==> Setup complete."
echo "    Run with: .venv/bin/python3 companion.py"
