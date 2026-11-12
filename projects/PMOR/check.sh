#!/usr/bin/env bash
# check.sh -- the merge gate. Firmware build + host build + all tests. Exit non-zero on ANY failure.
# Nothing reaches PMOR unless this passes.
#
#   ./check.sh            everything
#   ./check.sh --host     host build + tests only (no ARM toolchain needed)
#   ./check.sh --fw       firmware build only
#
# Env (defaults in brackets):
#   PMOR_DEPS     [~/pmor-deps]   holds libDaisy/ DaisySP/ tinyusb/ (pinned: see DEPS.txt) and tc/ (ARM GCC 10.3)
# Builds are niced onto cores 0-2 with make -j2: core 3 is the live audio core on twink.
set -uo pipefail
root="$(cd "$(dirname "$0")" && pwd)"; cd "$root"
PMOR_DEPS="${PMOR_DEPS:-$HOME/pmor-deps}"
mode="${1:-all}"
LIM="nice -n19"; command -v taskset >/dev/null 2>&1 && LIM="nice -n19 taskset -c 0-2"
fails=0; results=()
step() { # step "name" cmd...
  local name="$1"; shift
  echo "=== $name"
  if "$@"; then results+=("PASS  $name"); else results+=("FAIL  $name"); fails=$((fails+1)); fi
}

host() {
  step "core purity (no hardware includes)" tests/test_core_pure.sh
  step "device lint (no port/network object in saved patchers)" python3 tests/lint_devices.py .
  step "device layout render (clipping / overlap / contrast)" python3 tests/render_devices.py build_host/ui
  step "host configure" $LIM cmake -S . -B build_host -DCMAKE_BUILD_TYPE=Release > build_host.cfg.log 2>&1 \
    || { echo "cmake failed:"; tail -20 build_host.cfg.log; }
  step "host build (pmor_core + render + tests)" $LIM cmake --build build_host -j2
  step "param schema complete"            build_host/test_schema
  step "patch format round trip"          build_host/test_patch_roundtrip
  step "clock/sequencer host measurements" bash tools/host_clock_test.sh
  step "golden renders bit-identical"     tests/test_golden.sh build_host/pmor_render build_host/golden_out
  step "drum engines (presets, depth knobs, idle cost)" build_host/test_engines
  step "new drum preset goldens bit-identical" tests/test_golden_engines.sh build_host/pmor_render build_host/golden_eng_out
  step "drum synth engine vs the original voice: 96 presets x 8 scenarios, SNR table, preset values inside schema ranges" build_host/test_drumsynth
  step "synth filter stability (raw SVF + engine sweep)" build_host/test_synth_filter
  step "SD layout + additive migration (real-card copy; legacy files untouched)" tests/test_storage_migration.sh build_host/test_storage_layout
  step "ui state machine (browser, pool, reorder, holds, clock mode)" build_host/test_ui
  step "FX over MIDI: loop -> CC stream, CC in -> FX, echo / override rules, rate limit" build_host/test_fxmidi
  step "sampler audio restore is a bounded background job (boot no longer blocks)" build_host/test_audload
  step "sampler START/END/LOOP/ATTACK/RELEASE: defaults bit-identical, extras act" build_host/test_sampler_ext
  step "sampler stutter only chops: post-release playback identical to never stuttering" build_host/test_sampler_stutter
  step "synth LFO + drum deep recipe knobs: off/neutral bit-identical, each knob acts, finite" build_host/test_deep
  step "LED push: a pad press/release is written ahead of the background frame push (press -> LED latency)" build_host/test_ledpush
  step "first-hit recording: loop starts at the first hit (own clock: transport restart; followed: snapped to the 16th + lane offset)" build_host/test_firsthit
  step "MIDI out: no extra note-off before a note-on (retrigger only when held)" build_host/test_noteout
  step "follower: real clock_engine.c vs synthetic 124 BPM stream (jitter, bunching, offset)" build_host/test_follower
  step "protocol client vs simulated PMOR (every command)" build_host/pmorctl sim
  step "protocol client vs simulated PMOR, new-layout manifest hooks" build_host/pmorctl simnew
  step "negative control: firmware that clears the CDC TX FIFO on a session reset (0c48eaa) is caught by the sim" build_host/pmorctl simbad
  step "Max host client (max/pmor_host.h) vs simulated PMOR" build_host/test_hostclient
  step "Max shared core + resampler (max/pmor_share.h)"   build_host/test_share
  step "Max host reverse-compat (older / newer / very old fake firmware)" build_host/test_compat
  step "Max core synth notes make sound (pad-nearest path)" build_host/test_core_synth
  step "Max sampler slot load (.aud -> slot, ch3 note audible, bad files silent)" build_host/test_slotload
  step "Max preset files as the firmware writes them" build_host/test_presets_real
  step "Max sample cache (session dir first, fallback ~/Music/PMOR_Cache)" build_host/test_cache
  step "Max synth note -> pad mapping (max/pmor_notepad.h)" build_host/test_notepad
  step "Max parity: same performance, firmware path vs Live notes-only path (max/pmor_hostctl.h)" build_host/test_parity
  step "schema.json == firmware schema" bash -c 'build_host/dump_schema | diff -q - max/devices/schema.json'
  if command -v node >/dev/null 2>&1; then for t in test_bank test_where test_join test_lane test_compat_ui test_studio_sim test_session test_fx test_browser test_snap; do step "device js: $t (node)" node tests/$t.js; done; fi
  [ "$(uname -s)" = Darwin ] && [ -d /Applications/Max.app ] && { step "dial/umenu value conversion in headless Max" bash tests/max_dials/run.sh; step "editor idle when closed (headless Max)" bash tests/max_dials/run_gate.sh; }
  step "device schema (patcher param ids/ranges vs firmware schema)" python3 tests/test_devices_schema.py
}

fw() {
  local tc="$PMOR_DEPS/tc/gcc-arm-none-eabi-10.3-2021.10/bin"
  [ -d "$tc" ] && export PATH="$tc:$PATH"
  command -v arm-none-eabi-gcc >/dev/null || { echo "no arm-none-eabi-gcc"; results+=("FAIL  firmware toolchain"); fails=$((fails+1)); return; }
  echo "toolchain: $(arm-none-eabi-gcc --version | head -1)"
  touch hw/studio.cpp   # PMOR_FW_VERSION (git rev) is a -D: force it to be re-evaluated
  step "firmware build" $LIM make -j2 LIBDAISY_DIR="$PMOR_DEPS/libDaisy" DAISYSP_DIR="$PMOR_DEPS/DaisySP" \
        TINYUSB_DIR="$PMOR_DEPS/tinyusb" > build_fw.log 2>&1 \
    || { echo "firmware build failed:"; grep -E "error|Error" build_fw.log | head -20; }
  step "Daisy Seed model (regions, stack depth, SDRAM-before-init, pre-USB boot): tools/fwcheck.sh" tools/fwcheck.sh build/PMOR_V3.elf
  step "firmware image present + sized" bash -c '[ -s build/PMOR_V3.bin ] && arm-none-eabi-size build/PMOR_V3.elf | tail -1'
}

case "$mode" in
  --host) host ;;
  --fw)   fw ;;
  *)      host; fw ;;
esac

echo; echo "================ check.sh summary"
printf '%s\n' "${results[@]}"
if [ $fails -ne 0 ]; then echo "check.sh: $fails FAILED"; exit 1; fi
# Merge-gate stamp: only a CLEAN tree whose full run (host + firmware) passed gets one. tools/flash_pi.sh requires it
# to match HEAD and the exact image on disk, so nothing un-checked can reach PMOR.
if [ "$mode" = all ] && [ -z "$(git status --porcelain)" ] && [ -s build/PMOR_V3.bin ]; then
  echo "$(git rev-parse HEAD) $(sha256sum build/PMOR_V3.bin | cut -d' ' -f1)" > .check_stamp
  echo "check.sh: stamp written for $(git rev-parse --short HEAD)"
else
  rm -f .check_stamp; echo "check.sh: no stamp (partial run or dirty tree) - not flashable"
fi
echo "check.sh: ALL PASS"
