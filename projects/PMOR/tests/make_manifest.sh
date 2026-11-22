#!/usr/bin/env bash
# make_manifest.sh RENDER_DIR > MANIFEST : "sha256 case samples peak rms nonfinite" per render
set -euo pipefail
d="$1"; stat="${F32STAT:-$(cd "$(dirname "$0")/.." && pwd)/build_host/f32stat}"
for f in $(ls "$d"/*.f32 | sort); do
  h=$(sha256sum "$f" | cut -d' ' -f1); s=$("$stat" "$f")
  echo "$h $s"
done
