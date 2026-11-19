#!/bin/bash
# Builds web/hxsim.wasm from HYPER_CORE/src (FX engine, registry, effects, mapping) + sim/src (api, synthetic trumpet).
# Standalone wasm (no JS glue): the AudioWorklet instantiates it from bytes. Needs emcc (brew install emscripten).
# Env: HX_FASTMATH=0 builds without -ffast-math; HX_OPT=-O2 to override optimisation.
set -e
SIM="$(cd "$(dirname "$0")" && pwd)"
CORE="$SIM/../../HYPER_CORE/src"
FX="$CORE/dsp/fx"
OUT="$SIM/web/hxsim.wasm"
command -v emcc >/dev/null || { echo "emcc not found: brew install emscripten"; exit 1; }
FM="-ffast-math -fno-finite-math-only"   # keep NaN/Inf checks (hx::finite) alive; they are the engine's output guard
[ "${HX_FASTMATH:-1}" = "0" ] && FM=""
EXPORTS=$(grep "^API" "$SIM/src/sim_api.cpp" | grep -o "sim_[a-z0-9_]*(" | tr -d "(" | sed "s/^/_/" | paste -sd, -)
em++ ${HX_OPT:--O3} -std=c++17 $FM -fno-exceptions -fno-rtti \
  -I"$FX" -I"$CORE" \
  "$SIM/src/sim_api.cpp" "$SIM/src/trumpet_synth.cpp" "$FX/fx_engine.cpp" "$FX/fx_registry.cpp" "$FX/mapping/fx_mapping.cpp" \
  -sSTANDALONE_WASM=1 --no-entry -sINITIAL_MEMORY=100663296 -sALLOW_MEMORY_GROWTH=0 -sSTACK_SIZE=1048576 \
  -sFILESYSTEM=0 -sERROR_ON_UNDEFINED_SYMBOLS=1 -sEXPORTED_FUNCTIONS="$EXPORTS" \
  -Wno-unused-parameter -Wno-unused-function \
  -o "$OUT.tmp.wasm"
mv "$OUT.tmp.wasm" "$OUT"
ls -l "$OUT"
