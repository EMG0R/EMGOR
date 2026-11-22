#!/bin/bash
# build (if the wasm is missing or older than any source) and serve on http://127.0.0.1:8765
SIM="$(cd "$(dirname "$0")" && pwd)"
CORE="$SIM/../../HYPER_CORE/src"
W="$SIM/web/hxsim.wasm"
if [ "$1" != "--no-build" ]; then
  if [ ! -f "$W" ] || [ -n "$(find "$CORE" "$SIM/src" -type f \( -name '*.h' -o -name '*.cpp' \) -newer "$W" -print -quit)" ]; then
    echo "building wasm..."; bash "$SIM/build.sh" || exit 1
  else echo "wasm up to date"; fi
fi
exec node "$SIM/serve.mjs"
