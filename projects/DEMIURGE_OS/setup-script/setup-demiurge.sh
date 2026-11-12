#!/bin/bash
set -euo pipefail

# DEMIURGE Setup Script
# Run on a Pi 5 with Pi OS Lite (64-bit)
#
# PI_USER is THE single place this script gets the Pi's username from — every
# systemd unit this script installs (demiurge.service, demiurge-touch.service,
# demiurge-touch-poll.service, demiurge-web.service, rnbo-runner.service) has
# its User=/Group=/HOME= templated FROM this one variable, so renaming the rig
# (e.g. gorpi7 -> gorpi7v2) needs no edits anywhere: just run this script
# logged in as the new user and it picks it up automatically. Override only if
# you're installing AS a different account than the one that will run the
# instrument (rare): PI_USER=someoneelse ./setup-demiurge.sh
PI_USER="${PI_USER:-$USER}"
#
# demiurge.local is the hostname set in Raspberry Pi Imager — substitute your
# own host below if yours differs (ssh <pi-user>@demiurge.local is just the
# example this script's comments were written against).
#
# Usage: ssh <pi-user>@demiurge.local ./setup-demiurge.sh
#        ssh <pi-user>@demiurge.local ./setup-demiurge.sh 4    # resume from phase 4
#        ssh <pi-user>@demiurge.local ./setup-demiurge.sh 8    # boot config + reboot only
#        ssh <pi-user>@demiurge.local ./setup-demiurge.sh 10   # RNBO only (or resume there)
#
# RNBO (Cycling '74) support is Phase 10 — part of the standard run, no
# separate opt-in step anymore. It adds Cycling '74's apt repo (informational
# check only, nothing installed from it) and builds rnbo.oscquery.runner
# FROM SOURCE with on-device compile enabled, since the prebuilt apt package
# ships that disabled. Expect Phase 10 to take a while on a first run (a real
# compile of the runner + its conan dependencies) — see
# setup-script/rnbo-build-runner.sh for the researched details. See
# demiurge/docs/rnbo.md for why the installed service is gated fail-closed:
# rnbo-runner.service only actually runs when live.conf has `rnbo = on`
# or a *.rnbo file is in the chain.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Generic-install guards ───────────────────────────────────────────────────
# This script ships in two kinds of checkout: the owner's full tree (has neptr/,
# the launcher crate, the web UI, ...) and the public distro tree (which does not).
# Every phase that needs something from the full tree is conditional on that thing
# existing: present -> runs exactly as it always did; absent -> skipped with one
# "skip:" line. Nothing below changes what happens when the thing is present.
#   have <repo-relative-path>   true if that path exists in this checkout
#   skip_absent <what>          the one-line log for a skipped step
have() { [ -e "$SCRIPT_DIR/../$1" ]; }
skip_absent() { echo "  skip: $1 not present (generic install)"; }
HAVE_NEPTR=0
if [ -d "$SCRIPT_DIR/../neptr" ]; then HAVE_NEPTR=1; fi

FROM_PHASE="${1:-1}"
skip() { [[ "$FROM_PHASE" -gt "$1" ]]; }

echo "========================================"
echo "  DEMIURGE Setup"
[[ "$FROM_PHASE" -gt 1 ]] && echo "  (resuming from Phase $FROM_PHASE)"
echo "========================================"
echo ""

# cargo must be on PATH for Phase 6 regardless of start phase
export PATH="$HOME/.cargo/bin:$PATH"

# ── Phase 1: Foundation ──────────────────────

if ! skip 1; then
echo "=== Phase 1: Foundation ==="

sudo apt update && sudo apt upgrade -y

sudo apt install -y \
    git build-essential cmake pkg-config \
    alsa-utils \
    nano htop \
    sox ffmpeg libsox-fmt-all \
    liblo-dev liblo-tools \
    teensy-loader-cli \
    picotool \
    curl \
    libjack-jackd2-dev \
    nodejs npm \
    python3-mido \
    python3-rtmidi

# Ensure current user is in audio group
sudo usermod -aG audio "$PI_USER"

# Rust toolchain for the launcher build. We use rustup so the resulting
# binary is compatible with the system libc regardless of what the Pi OS
# ships. Idempotent — skips if a cargo is already on PATH.
if ! have src/demiurge-launcher-rs/Cargo.toml; then
    skip_absent "src/demiurge-launcher-rs/ (Rust toolchain not needed)"
elif ! command -v cargo >/dev/null 2>&1 && [ ! -x "$HOME/.cargo/bin/cargo" ]; then
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable --profile minimal
fi
export PATH="$HOME/.cargo/bin:$PATH"

echo "=== Phase 1 complete ==="
echo ""
fi

# ── Phase 2: PipeWire ────────────────────────

if ! skip 2; then
echo "=== Phase 2: PipeWire ==="

sudo apt install -y \
    pipewire pipewire-alsa pipewire-jack \
    wireplumber \
    pipewire-pulse

# Remove PulseAudio if present
sudo apt remove -y pulseaudio pulseaudio-module-bluetooth 2>/dev/null || true

# Enable PipeWire for user session
systemctl --user --now enable pipewire pipewire-pulse wireplumber

echo "=== Phase 2 complete ==="
echo ""
fi

# ── Phase 3: Virtual Audio Layer ─────────────

if ! skip 3; then
echo "=== Phase 3: Virtual Audio Layer ==="

sudo mkdir -p /etc/pipewire/pipewire.conf.d

# Virtual audio layer — 4-channel demiurge-sink / demiurge-source loopback
# pair, pinned at audio.rate = 48000 on every loopback side so a USB
# hot-plug exposing the graph to another rate cannot drag the virtual
# nodes with it. See gotchas.md Q7c (rate) and the header of
# config/pipewire/demiurge-virtual.conf for the full rationale.
# NOT a glob here, deliberately. config/pipewire/demiurge-midi.conf also lives
# in that directory and is NOT installed on purpose: it builds a PipeWire MIDI
# merge port, but this rig's MIDI hub is the ALSA-seq `Midi Through` (14:0) bus
# that Phase 4 sets up, and every consumer (csound -M 14:0, demiurge-clock,
# PMOR, the touch daemon) is wired to that. Installing both would be two
# mechanisms for one job and a second place for MIDI to go missing. Checked
# 2026-10-02 while auditing what the installer skips -- this one is a correct
# skip, not an oversight. If the PipeWire MIDI layer is ever revived, retire the
# ALSA-seq path in the same commit.
sudo install -m 0644 \
    "$SCRIPT_DIR/../config/pipewire/demiurge-virtual.conf" \
    /etc/pipewire/pipewire.conf.d/demiurge-virtual.conf

# WirePlumber policy: pin demiurge-sink (priority=0 + dont-reconnect),
# demote HDMI audio (enabled but zero priority + no autoconnect, so
# explicit HDMI selection works), prioritise USB audio class devices, pin
# every USB interface to 48000 via monitor.alsa.rules, ignore Bluetooth.
# See config/wireplumber/50-demiurge.conf header for full rationale.
sudo mkdir -p /etc/wireplumber/wireplumber.conf.d
# GLOB, not named files (2026-10-02). 99-demiurge-disable-bela-audio.conf had
# existed in the repo since 2026-09-25 and was NEVER installed, because this
# block named its files one by one. That rule hides the Bela's dead 44.1 kHz USB
# audio card; without it the launcher counts a second wired output, direct mode
# switches off, and the chain becomes csound(128) -> loopback(256) -> DAC(128) --
# a standing conversion that clicks with no xrun and no CPU load. See that
# file's own header: it is the "IR crackle that survived three different IR
# implementations". Anything dropped into config/wireplumber/ now installs.
for c in "$SCRIPT_DIR/../config/wireplumber/"*.conf; do
    [ -f "$c" ] || continue
    sudo install -m 0644 "$c" "/etc/wireplumber/wireplumber.conf.d/$(basename "$c")"
    echo "  wireplumber: $(basename "$c")"
done

# Pocket OpGorator (Daisy): never graph driver, force 48k. Numbered 95- so
# it loads after 50-demiurge.conf's generic USB rule and wins.
# (95-demiurge-opgorator.conf is covered by the glob above.)

systemctl --user restart pipewire wireplumber

echo "=== Phase 3 complete ==="
echo ""
fi

# ── Phase 4: Virtual MIDI Layer ──────────────

if ! skip 4; then
echo "=== Phase 4: Virtual MIDI Layer ==="

# Auto-connect all physical MIDI sources to Midi Through (demiurge-midi merge port)
sudo tee /usr/local/bin/demiurge-midi-connect.sh > /dev/null << 'SCRIPT'
#!/bin/bash
# Auto-connect all physical MIDI sources to demiurge-midi (Midi Through, client 14)
sleep 1
aconnect -l | grep -oP 'client \K\d+(?=.*\[type=kernel,card)' | while read client; do
    aconnect "$client":0 14:0 2>/dev/null || true
    # Mirror: also connect the bus BACK to the device so USB MIDI gear
    # (e.g. the Pocket OpGorator) hears 0xF8 clock ticks and the
    # CC116/CC119 broadcasts, not just sends into the bus. Idempotent —
    # a duplicate aconnect errors harmlessly.
    aconnect 14:0 "$client":0 2>/dev/null || true
done
SCRIPT
sudo chmod +x /usr/local/bin/demiurge-midi-connect.sh

# udev rule: fire on MIDI device hot-plug.
#
# ACTION=="add" AND ACTION=="remove" both run the connect script. The
# ADD case is the original reason this rule exists (wire fresh MIDI
# hardware onto the shared bus). The REMOVE case was added alongside
# device-presence clock-role auto-switching (see
# src/demiurge-launcher-rs/src/clockrole.rs and
# demiurge/docs/clock-role-auto.md): a Pocket OpGorator/Daisy connected
# MIDI-only (no audio interface) generates no PipeWire audio-node event
# at all, so pw-mon (events::spawn_pwmon, which only watches
# alsa_output.*/alsa_input.* nodes) never fires on it either way — ADD
# or REMOVE. Re-running the connect script on REMOVE is a harmless,
# idempotent no-op (aconnect against a vanished client just fails
# silently, same as any other race this script already tolerates), so
# it costs nothing and keeps both actions symmetric.
#
# The actual role-tracking fix for the MIDI-only case is NOT this udev
# rule, though — the launcher process doesn't subscribe to udev events
# at all. It's clockrole::pmor_present() being polled every heartbeat
# (~2s) regardless of what triggered the tick, checking BOTH the
# PipeWire audio-node graph and devices::scan_usb_midi()'s ALSA-seq
# client list. That poll is what actually catches a MIDI-only device
# appearing or disappearing; this udev rule is a companion/symmetry fix
# for demiurge-midi-connect.sh's own job (bus wiring), not the presence
# oracle. Fail-safe by construction: a missing/renamed device just means
# aconnect has nothing to do, never a hang, so this cannot wedge the
# launcher (which isn't even a party to this rule) or udev itself.
# STM32 DFU (the Daisy Seed in bootloader mode). Without this, dfu-util sees
# the device but fails with LIBUSB_ERROR_ACCESS and every flash needs sudo.
# Verified 2026-10-01: the rule takes effect immediately on udevadm trigger,
# no replug needed.
sudo tee /etc/udev/rules.d/60-dfuse.rules > /dev/null << 'DFURULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="df11", MODE="0666", TAG+="uaccess"
DFURULE

sudo tee /etc/udev/rules.d/99-demiurge-midi.rules > /dev/null << 'UDEV'
SUBSYSTEM=="sound", ACTION=="add", KERNEL=="midiC*D*", RUN+="/usr/local/bin/demiurge-midi-connect.sh"
SUBSYSTEM=="sound", ACTION=="remove", KERNEL=="midiC*D*", RUN+="/usr/local/bin/demiurge-midi-connect.sh"
UDEV
sudo udevadm control --reload-rules

# Run once now to connect any already-plugged devices
/usr/local/bin/demiurge-midi-connect.sh || true

# Microcontroller flashing — supplemental Teensy udev rule.
# The teensy-loader-cli apt package installs /lib/udev/rules.d/49-teensy.rules,
# which grants access via TAG+="uaccess". That only works for users logged in
# to a graphical seat; SSH sessions are NOT considered logged in, so over-SSH
# flashing fails with "Unable to claim interface" unless run as root. This
# supplemental rule grants plugdev access to every Teensy USB mode (firmware
# AND the HalfKay bootloader, PID 0478), so any plugdev user can run
# `demiurge-flash-teensy <hex>` over SSH without sudo. See
# demiurge/docs/microcontroller-flashing.md for the full workflow.
sudo install -m 0644 \
    "$SCRIPT_DIR/../config/udev/99-demiurge-teensy.rules" \
    /etc/udev/rules.d/99-demiurge-teensy.rules
# Same story for the Pico (RP2040 / RP2350) — grants plugdev access to the
# 2e8a BOOTSEL endpoint so `demiurge-flash-pico` works over SSH without sudo.
sudo install -m 0644 \
    "$SCRIPT_DIR/../config/udev/99-demiurge-pico.rules" \
    /etc/udev/rules.d/99-demiurge-pico.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --action=change --subsystem-match=usb

# ── Bluetooth MIDI (BLE MIDI) ────────────────
echo "--- Bluetooth MIDI ---"

sudo apt install -y bluez pi-bluetooth libspa-0.2-bluetooth

sudo usermod -aG bluetooth "$PI_USER"

# Enable experimental features in bluetoothd — required for the BLE MIDI
# GATT profile (org.bluez.MIDI1). Without this flag bluetoothd ignores
# the MIDI service advertisement and the ble-midi kernel client never appears.
if grep -q "^Experimental" /etc/bluetooth/main.conf 2>/dev/null; then
    sudo sed -i 's/^Experimental.*/Experimental = true/' /etc/bluetooth/main.conf
else
    sudo sed -i '/^\[General\]/a Experimental = true' /etc/bluetooth/main.conf
fi

sudo systemctl daemon-reload
sudo systemctl enable --now bluetooth
sudo systemctl restart bluetooth

echo "=== Phase 4 complete ==="
echo ""
fi

# ── Phase 5: Audio Languages ─────────────────

if ! skip 5; then
echo "=== Phase 5: Audio Languages ==="

# dfu-util: flashing PMOR (the Daisy groovebox) FROM the Pi over USB. Added
# 2026-10-01 -- it was missing, so a bench flash needed a laptop in the loop.
# tcpdump: the only practical way to prove what is actually on the OSC/MIDI
# wire; diagnosing the 40-BPM clock bug needed it.
sudo apt install -y dfu-util tcpdump

sudo apt install -y \
    csound \
    puredata puredata-utils \
    supercollider supercollider-language supercollider-server supercollider-supernova \
    chuck \
    faust \
    python3-venv python3-pip

# Patched librtjack.so (config/csound/librtjack.so). Stock csound 6.18's rtjack
# plugin copies JACK port names into a 64-byte stack buffer; a PipeWire
# pro-audio port name such as
# `alsa_output.platform-soc_107c000000_sound.pro-output-0:playback_AUX0` is
# longer than that and smashes the stack (source: config/csound/rtjack.c.patched).
# The 2026-10-01 rebuild never installed it (deploy-rtjack.sh pointed at a path
# that did not exist), so the rebuilt rig ran the stock plugin. Only swap it on
# 6.18, the version the patch was built against: a different csound has a
# different plugin ABI and the swap could break csound outright. The original
# is kept once as .orig; apt upgrading csound will overwrite this, so re-run
# Phase 5 (or deploy-rtjack.sh) after a csound upgrade.
RTJ_DST=/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/librtjack.so
RTJ_SRC="$SCRIPT_DIR/../config/csound/librtjack.so"
if csound --version 2>&1 | grep -q '6\.18'; then
    if [ -f "$RTJ_SRC" ] && [ -f "$RTJ_DST" ] && ! cmp -s "$RTJ_SRC" "$RTJ_DST"; then
        [ -f "$RTJ_DST.orig" ] || sudo cp -a "$RTJ_DST" "$RTJ_DST.orig"
        sudo install -m 0644 "$RTJ_SRC" "$RTJ_DST"
        echo "  installed patched librtjack.so (original kept at $RTJ_DST.orig)"
    else
        echo "  patched librtjack.so already in place (or source/plugin missing -- skipped)"
    fi
else
    echo "  csound is not 6.18 -- patched librtjack.so NOT installed (built against 6.18)"
fi

# ── Phase 5b: NAM csound plugin ──
# The 2026-10-01 rebuild lost csound-nam.so, and the copy that came back was the
# stale single-instance opcode (one model for L then R): the audible "bitcrush".
# ONE source (neptr/csound/nam/src + vendored libs/) and ONE script
# (build-on-pi.sh, -j2: -j4 browned out a Pi 5) build it, straight into the dir
# csound really loads. get_active_pair exists only in the per-channel build.
NAM_DIR="$SCRIPT_DIR/../neptr/csound/nam"
NAM_SO=/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so
if [ -f "$NAM_DIR/libs/NeuralAmpModelerCore/NAM/dsp.h" ]; then
    echo "--- Phase 5b: building csound-nam.so"
    if bash "$NAM_DIR/build-on-pi.sh" \
       && nm -C "$NAM_SO" 2>/dev/null | grep -q get_active_pair; then
        echo "  csound-nam.so installed (per-channel build, get_active_pair present)"
    else
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo "!!! NAM PLUGIN WRONG OR MISSING: get_active_pair not in $NAM_SO"
        echo "!!! A single-instance build sounds like bitcrush. Re-run:"
        echo "!!!   bash $NAM_DIR/build-on-pi.sh"
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    fi
    # Post-install check, non-fatal (needs profiles in ~/demiurge/profiles/nam).
    bash "$SCRIPT_DIR/../testing/nam/test-nam.sh" \
        || echo "  WARNING: test-nam.sh reported failures (see above); setup continues"
else
    if [ -d "$NAM_DIR" ]; then
        echo "  NAM core not vendored at $NAM_DIR/libs -- csound-nam.so NOT built (run deploy-nam.sh)"
    else
        skip_absent "neptr/csound/nam (NAM plugin build)"
    fi
fi

echo "=== Phase 5 complete ==="
echo ""
fi

# ── Phase 6: DEMIURGE Launcher + Staging Tree ──

if ! skip 6; then
echo "=== Phase 6: DEMIURGE launcher + staging tree ==="

# 1) Build and install the Rust launcher.
#    The launcher is the *internal* engine that interprets ~/demiurge/live.conf.
#    Users never touch its sources — it is built once here from src/demiurge-launcher-rs
#    and installed to /opt/demiurge/bin. Rebuild by re-running this script.
sudo install -d /opt/demiurge/bin /opt/demiurge/lib
if have src/demiurge-launcher-rs/Cargo.toml; then
(
    cd "$SCRIPT_DIR/../src/demiurge-launcher-rs"
    cargo build --release
)
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/demiurge-launcher-rs/target/release/demiurge-launcher" \
    /opt/demiurge/bin/demiurge-launcher
else
    skip_absent "src/demiurge-launcher-rs/ (launcher build)"
fi

# 1a) Build and install demiurge-clock (ALSA seq clock daemon). Link support
#     requires ableton-link-dev AND -DLINK_PLATFORM_LINUX=1 (the Config.hpp
#     platform guard) — without that flag the headers compile to empty stubs
#     and the build silently loses all Link functionality.
if have src/demiurge-clock.cpp; then
sudo apt install -y libasound2-dev
# INSTALL it, don't just test for it. Until 2026-10-02 this script only ran the
# dpkg check below and never installed the package, so the `else` branch always
# won and demiurge-clock was built WITHOUT Link on every rig this script ever
# built -- silently, because "built without Link" is only a log line. Ableton
# Link is a first-class feature of this instrument (tempo from a Link peer is
# meant to flow Link -> daemon -> NEPTR -> PMOR), so it is a hard dependency now.
sudo apt install -y ableton-link-dev

if dpkg -l ableton-link-dev >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -Wall -pthread -DDEMIURGE_LINK -DLINK_PLATFORM_LINUX=1 \
        -o /tmp/demiurge-clock "$SCRIPT_DIR/../src/demiurge-clock.cpp" -lasound -lpthread
    echo "demiurge-clock built with Ableton Link support"
else
    g++ -O2 -Wall -pthread -o /tmp/demiurge-clock \
        "$SCRIPT_DIR/../src/demiurge-clock.cpp" -lasound -lpthread
    echo "demiurge-clock built without Link (ableton-link-dev not installed)"
fi
sudo install -m 0755 /tmp/demiurge-clock /opt/demiurge/bin/demiurge-clock
rm -f /tmp/demiurge-clock
else
    skip_absent "src/demiurge-clock.cpp (clock daemon build)"
fi

# 1a-ii) Build and install demiurge-csound-host — Csound computed INSIDE the
#     JACK callback (live.conf `csound_host = embedded`). Removes csound's
#     engine-thread ring (-B), which measured +2.83 ms at its lowest usable
#     setting and +23.6 ms at the value this project shipped for months; see
#     demiurge/docs/latency-measurements.md.
#
#     BUILDS ON THE PI, not on a dev Mac: it needs Csound's C API headers and
#     JACK's, which only exist on the target. -mcpu=native is deliberate for
#     the same reason (matches csound/nam's aarch64 flags). Skipped, loudly,
#     if either -dev package is missing — a rig without it simply keeps using
#     the standalone backend, which is the default anyway.
sudo apt install -y libcsound64-dev libjack-jackd2-dev
if [[ -f /usr/include/csound/csound.h && -f /usr/include/jack/jack.h ]]; then
    g++ -std=c++17 -O2 -mcpu=native -Wall -pthread \
        -o /tmp/demiurge-csound-host "$SCRIPT_DIR/../src/demiurge-csound-host.cpp" \
        -lcsound64 -ljack -lpthread
    sudo install -m 0755 /tmp/demiurge-csound-host /opt/demiurge/bin/demiurge-csound-host
    rm -f /tmp/demiurge-csound-host
    echo "demiurge-csound-host built (live.conf csound_host = embedded to use it)"
else
    echo "WARNING: csound and/or JACK headers missing — demiurge-csound-host NOT built."
    echo "         live.conf 'csound_host = embedded' will fail; standalone (default) is unaffected."
fi

# 1b) Install per-language wrappers and the shared env prelude.
#     demiurge-env.sh carries the LD_LIBRARY_PATH + PIPEWIRE_LATENCY
#     pins AND the DEMIURGE_OSC_BASE port reservation (see demiurge/docs/osc.md)
#     that every audio child must honour. The launcher and every
#     demiurge-run-* script sources it before exec. liblo-tools (Phase 1)
#     provides oscsend/oscdump for testing OSC by hand.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-env.sh" \
    /opt/demiurge/lib/demiurge-env.sh
for w in "$SCRIPT_DIR/../src/wrappers/demiurge-run-"*; do
    sudo install -m 0755 "$w" "/opt/demiurge/bin/$(basename "$w")"
done

# ---- EVERY OTHER TOOL IN src/wrappers (added 2026-10-02) -------------------
# The run-* loop above has always been a glob, so those never go missing. The
# STANDALONE tools were installed by individually-named `install` lines, and so
# whichever ones nobody remembered to add simply did not exist on the rig. That
# cost real debugging time on the 2026-10-02 rebuild:
#   demiurge-quantum-limits  missing -> the launcher could not verify the
#                            quantum, kept bouncing the card, and csound
#                            crash-looped for an hour looking like a PipeWire
#                            enumeration fault.
#   demiurge-interface       missing -> no way to see or pick the audio
#                            interface at all, which is the user-facing control
#                            for exactly the multi-interface problem that was
#                            corrupting audio that day.
#   demiurge-ringtest        missing -> the purpose-built "is the artifact on
#                            the input or generated in the chain" diagnostic
#                            was unavailable during an artifact hunt.
# So this is a GLOB on purpose: anything added to src/wrappers from now on gets
# installed without anyone having to remember. Skip the unit files (installed
# with their own templating), the env prelude (goes to lib/), and junk.
for w in "$SCRIPT_DIR/../src/wrappers/"*; do
    b="$(basename "$w")"
    case "$b" in
        *.service|*.timer|*.conf|*.sh|__pycache__|demiurge-env.sh) continue ;;
    esac
    [ -f "$w" ] || continue
    sudo install -m 0755 "$w" "/opt/demiurge/bin/$b"
done
echo "  installed $(ls -1 "$SCRIPT_DIR/../src/wrappers/" | grep -vcE "\\.(service|timer|conf|sh)$|__pycache__") wrapper tools"

# Microcontroller flashing helper. Goes in /usr/local/bin (not /opt/demiurge/bin)
# because it's user-facing — typed by humans over SSH — and /opt/demiurge/bin
# isn't on the default login PATH. Pairs with the supplemental udev rule
# installed in Phase 4.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-flash-teensy" \
    /usr/local/bin/demiurge-flash-teensy

# Same for the Pico (RP2040 / RP2350) — pairs with 99-demiurge-pico.rules.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-flash-pico" \
    /usr/local/bin/demiurge-flash-pico

# Serial telemetry streamer — same rationale (user-facing, typed over SSH).
# `ssh demiurge.local demiurge-stream-teensy` tails the Teensy's serial print.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-stream-teensy" \
    /usr/local/bin/demiurge-stream-teensy

# Audio interface chooser — user-facing AND called by every UI (web, Mac TUI,
# NEPTR companion). Lists attached interfaces by product name and writes the
# global choice (~/demiurge/interface.conf), then restarts demiurge.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-interface" \
    /usr/local/bin/demiurge-interface

# Clock-role chooser (OpGorator/Demiurge master) — same rationale as
# demiurge-interface: user-facing AND called by every UI. Writes
# ~/demiurge/sync.conf and injects CC116 live on the MIDI bus; no
# service restart needed.
if have src/wrappers/demiurge-sync; then
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-sync" \
    /usr/local/bin/demiurge-sync
else
    skip_absent "src/wrappers/demiurge-sync (clock-role chooser)"
fi

# Engine settings (live.conf bits / quantum / rate / sync_layer) — the single
# write path for all four, same rationale as demiurge-interface. Nothing else
# may edit those keys: the CLI owns validation (notably that 24-bit is not a
# legal sample format for csound's realtime ALSA) and the mode-aware wording
# every UI shows for what `quantum` actually does in the current mode.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-audio" \
    /usr/local/bin/demiurge-audio

# Wi-Fi chooser — user-facing AND called by every UI. Wraps nmcli: lists
# visible SSIDs, joins saved/open/new networks and reports the current IP.
# The single write path for network selection; new profiles are created at
# autoconnect-priority 20 so the home network's 30 keeps winning.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-wifi" \
    /usr/local/bin/demiurge-wifi

# Agent runner and hardware mixer — user-facing and typed over SSH, which is
# the whole point of demiurge-agent (`ssh pi demiurge-agent ...` is its
# documented usage and it simply was not on the PATH: /opt/demiurge/bin is not
# in sshd's default PATH, so Phase 11's own printed instructions failed with
# "command not found"). Symlinked rather than copied so there is one file.
sudo ln -sf /opt/demiurge/bin/demiurge-agent /usr/local/bin/demiurge-agent
sudo ln -sf /opt/demiurge/bin/demiurge-mixer /usr/local/bin/demiurge-mixer
# Master-output recorder. The UI's REC button shells out to the bare name
# `demiurge-record` (neptrPhase4_UI.py RECORD_CMD) under systemd's default
# PATH, so without this link the button silently did nothing (2026-10-03
# rebuild: wrapper present in /opt/demiurge/bin, no link, "command not found").
sudo ln -sf /opt/demiurge/bin/demiurge-record /usr/local/bin/demiurge-record

# Touchscreen -> MIDI CC daemon + its fullscreen test/calibration visual.
# Daemon lives in /opt/demiurge/bin (the service ExecStart points there); the
# CLI face (`demiurge-touch on|off`, status) is user-facing AND called by the
# web UI, so it also goes to /usr/local/bin — same dual role as
# demiurge-interface. Gated by `touch =` in live.conf (default off; the daemon
# exits cleanly, ui-launch style). See demiurge/docs/config-reference.md.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-touch" \
    /opt/demiurge/bin/demiurge-touch
sudo ln -sf /opt/demiurge/bin/demiurge-touch /usr/local/bin/demiurge-touch
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-touch-test" \
    /opt/demiurge/bin/demiurge-touch-test
sudo ln -sf /opt/demiurge/bin/demiurge-touch-test /usr/local/bin/demiurge-touch-test
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s/^Group=.*/Group=$(id -gn)/" \
    -e "s#^Environment=HOME=.*#Environment=HOME=$HOME#" \
    -e "s/1000/$(id -u)/g" \
    "$SCRIPT_DIR/../config/demiurge-touch.service" | sudo tee /etc/systemd/system/demiurge-touch.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable demiurge-touch.service

# Polling touchscreen driver. The panel's ADS7846 has a DEAD PENIRQ output
# (stuck low after a screen came loose while powered), and the kernel ads7846
# driver is edge-interrupt-driven, so it is never woken — 0 interrupts since
# boot, which is why no reboot or reseat ever helped. The chip still measures
# correctly over SPI, so this daemon polls it and republishes the panel through
# uinput under the SAME device name, keeping every consumer (NEPTR UI,
# demiurge-touch, the calibrator, SDL) working with no code change.
sudo install -m 0755 \
    "$SCRIPT_DIR/../src/wrappers/demiurge-touch-poll" \
    /opt/demiurge/bin/demiurge-touch-poll
sudo ln -sf /opt/demiurge/bin/demiurge-touch-poll /usr/local/bin/demiurge-touch-poll
if have config/demiurge-touch-poll.service; then
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s/^Group=.*/Group=$(id -gn)/" \
    -e "s#^Environment=HOME=.*#Environment=HOME=$HOME#" \
    "$SCRIPT_DIR/../config/demiurge-touch-poll.service" | sudo tee /etc/systemd/system/demiurge-touch-poll.service >/dev/null
else
    skip_absent "config/demiurge-touch-poll.service"
fi

# Hand the SPI device to spidev at boot: drop the ads7846 overlay (which also
# disables the spidev@1 node it replaces) so /dev/spidev0.1 exists from boot
# and the useless kernel driver never loads. Idempotent — the daemon's runtime
# takeover covers the window before the next reboot, so this never has to be
# applied urgently. Only rewrites config.txt when the overlay is actually there.
BOOTCFG=/boot/firmware/config.txt
[ -f "$BOOTCFG" ] || BOOTCFG=/boot/config.txt
# SPI must be on regardless of whether an ads7846 overlay was ever present.
# The block below only fired when there WAS an overlay to comment out, so on a
# stock image (which has no ads7846 line at all) dtparam=spi=on never got added
# -- no /dev/spidev*, so demiurge-touch-poll span forever on "could not unbind
# driver" and the touchscreen simply did not exist. Found 2026-10-02.
# Only for the SPI touch panel: the NEPTR rig profile, an ads7846 line already in
# config.txt, or DEMIURGE_TOUCH=1. A generic Pi has no panel, and a stranger's
# config.txt is not ours to edit for hardware they don't have (2026-10-07).
WANT_SPI=0
if [ "$HAVE_NEPTR" = 1 ] || [ "${DEMIURGE_TOUCH:-0}" = 1 ] || { [ -f "$BOOTCFG" ] && grep -q 'dtoverlay=ads7846' "$BOOTCFG"; }; then WANT_SPI=1; fi
[ "$WANT_SPI" = 1 ] || echo "  skip: SPI touch panel not detected (set DEMIURGE_TOUCH=1 to force)"
if [ "$WANT_SPI" = 1 ] && [ -f "$BOOTCFG" ] && ! grep -q '^dtparam=spi=on' "$BOOTCFG"; then
    echo "  enabling SPI (dtparam=spi=on) — the polling touch driver needs /dev/spidev0.1"
    sudo cp "$BOOTCFG" "$BOOTCFG.pre-spi"
    sudo sed -i 's/^#dtparam=spi=on/dtparam=spi=on/' "$BOOTCFG"
    grep -q '^dtparam=spi=on' "$BOOTCFG" || echo 'dtparam=spi=on' | sudo tee -a "$BOOTCFG" >/dev/null
fi

if [ -f "$BOOTCFG" ] && grep -q '^dtoverlay=ads7846' "$BOOTCFG"; then
    echo "  ads7846 overlay -> spidev (dead PENIRQ; polling driver takes over)"
    sudo cp "$BOOTCFG" "$BOOTCFG.pre-touch-poll"
    sudo sed -i 's/^dtoverlay=ads7846/#dtoverlay=ads7846/' "$BOOTCFG"
    grep -q '^dtparam=spi=on' "$BOOTCFG" || echo 'dtparam=spi=on' | sudo tee -a "$BOOTCFG" >/dev/null
    echo "  (reboot needed for the boot-time path; runtime takeover works now)"
fi
sudo systemctl daemon-reload
if have config/demiurge-touch-poll.service; then
sudo systemctl enable demiurge-touch-poll.service
fi

# ---- LINGER: the single most important line in this script -----------------
# Without it the user systemd manager (and therefore PipeWire + WirePlumber)
# only exists while someone is LOGGED IN. On a headless instrument that means:
# PipeWire comes up when you ssh in, and DIES when you log out -- taking the
# interface's PipeWire node with it, which suspends the graph and kills csound.
#
# Diagnosed 2026-10-02 after hours of chasing symptoms: the journal showed two
# different `systemd --user` PIDs stopping/starting wireplumber, and
# `loginctl show-user` said `Linger=no`. It is why fixes appeared to work and
# then "stopped working", why restarting PipeWire helped only sometimes (the
# ssh login itself was what started it), and why the rig could never have run
# unattended. sync_layer = on CANNOT work without this, because that mode
# routes the chain through PipeWire nodes.
# ---- NEPTR UI python env ---------------------------------------------------
# pygame MUST come from apt, not pip. The pip wheel bundles its OWN SDL2 built
# WITHOUT the kmsdrm backend (SDL 2.28.4), so `pygame.display.init()` dies with
# "kmsdrm not available" on a headless KMS panel -- which is exactly how the UI
# failed for an hour on 2026-10-02 while it looked like a display-contention or
# DRM-master problem. Debian's python3-pygame links the SYSTEM SDL (2.32.4),
# which HAS kmsdrm. So: apt for pygame, --system-site-packages so the venv can
# see it, and pip ONLY for the small pure-python deps.
if [ "$HAVE_NEPTR" = 1 ]; then
sudo apt install -y python3-pygame
if [ ! -x "$HOME/kivy-venv/bin/python" ]; then
    python3 -m venv --system-site-packages "$HOME/kivy-venv"
fi
"$HOME/kivy-venv/bin/pip" install -q python-osc mido python-rtmidi pyserial || true
echo "  NEPTR UI venv ready (system pygame + kmsdrm)"
else
    skip_absent "neptr/ (NEPTR UI venv)"
fi

sudo loginctl enable-linger "$PI_USER"
echo "  linger enabled for $PI_USER (PipeWire now survives logout / runs at boot)"

# ---- HiFiBerry DAC+ADC Pro overlay (added 2026-10-02) ----------------------
# THIS SCRIPT USED TO NOT DO THIS, and it is the single highest-severity gap a
# rebuild can hit: with no card-specific dtoverlay the Pi boots with vc4hdmi
# only, there is no `card N: sndrpihifiberry`, and the instrument has NO audio
# path at all. Found while rebuilding after the 2026-10-01 SD card death; the
# overlay had to be added to config.txt by hand before the first boot.
# Idempotent, and it does NOT disable onboard audio -- the interface resolver
# already refuses to make HDMI the default, so leaving dtparam=audio alone is
# one less thing to undo. A reboot is required, which Phase 8 performs.
# Only when the HAT is actually there: its EEPROM says HiFiBerry, or the NEPTR rig
# profile (Emory's rebuild path), or DEMIURGE_HAT=hifiberry-dacplusadcpro. Writing
# a card overlay onto a board without that card is a config.txt edit for
# hardware that doesn't exist (2026-10-07: neuralgrid got one).
WANT_HIFIBERRY=0
if [ "$HAVE_NEPTR" = 1 ] || [ "${DEMIURGE_HAT:-}" = hifiberry-dacplusadcpro ] || \
   tr -d '\0' < /proc/device-tree/hat/vendor 2>/dev/null | grep -qi hifiberry; then WANT_HIFIBERRY=1; fi
[ "$WANT_HIFIBERRY" = 1 ] || echo "  skip: no HiFiBerry HAT detected (set DEMIURGE_HAT=hifiberry-dacplusadcpro to force)"
if [ "$WANT_HIFIBERRY" = 1 ] && [ -f "$BOOTCFG" ] && ! grep -q '^dtoverlay=hifiberry-dacplusadcpro' "$BOOTCFG"; then
    echo "  adding HiFiBerry DAC+ADC Pro overlay (no audio card without it)"
    sudo cp "$BOOTCFG" "$BOOTCFG.pre-hifiberry"
    printf '\n# HiFiBerry DAC+ADC Pro -- the instrument has no audio path without this.\ndtoverlay=hifiberry-dacplusadcpro\n' | sudo tee -a "$BOOTCFG" >/dev/null
fi
# NOTE: usb_max_current_enable=1 is deliberately NOT set here -- Phase 8 owns
# it, along with the rest of the config.txt performance block, behind its PSU
# confirmation prompt. Setting it in two places would be two mechanisms for one
# job, and the one that matters is the one gated on the user confirming they
# actually have the 5V/5A supply.

# 2) Stage ~/demiurge/ for the current user (docs, examples/, bridges/, …).
#    NEVER overwrite the user's live.conf — it's the single user-facing
#    config file and is sacred. We sync everything else and re-add a
#    starter live.conf only if one doesn't already exist.
mkdir -p "$HOME/demiurge"
# --exclude=/live.conf is anchored to the staging root so only the
# top-level ~/demiurge/live.conf is preserved from overwrite — example
# show-files like demiurge/examples/plucky_ambient/live.conf still sync.
#
# The other excludes (2026-10-02) protect USER-OWNED state that is NOT in the
# repo's demiurge/ tree. With --delete, rsync deletes every destination file
# the source lacks, so before this a re-run of Phase 6 would have wiped
# ~/demiurge/sets, presets, state, interface.conf and asound.state (the mixer
# snapshot) -- exactly the files a card death already cost once. An --exclude
# also protects a path from --delete.
# 2026-10-07: NO --delete, ever. On a stranger's machine ~/demiurge may be
# something of theirs that shares the name; a destructive default silently
# destroys it. Files the repo dropped stay behind as harmless leftovers.
if have demiurge; then
rsync -a \
    --exclude '.DS_Store' \
    --exclude=/live.conf \
    --exclude=/sets/ \
    --exclude=/presets/ \
    --exclude=/state/ \
    --exclude=/backups/ \
    --exclude=/interface.conf \
    --exclude=/graph.conf \
    --exclude=/sync.conf \
    --exclude=/touch-cal.conf \
    --exclude=/ui-state.conf \
    --exclude=/asound.state \
    "$SCRIPT_DIR/../demiurge/" "$HOME/demiurge/"
else
    skip_absent "demiurge/ (docs/examples staging tree)"
fi

# Seeding order for the user-owned files, on a rig that does not have them yet:
#   1. the Mac-side backup kit: rig-backup/latest/<host>/home/demiurge/...
#      (setup-script/backup-rig.sh pulls it; it holds the config he last
#      verified by ear, which is what a card death must restore)
#   2. the repo default (live.conf and the neptr sets only)
# NEVER overwrite anything that already exists: rsync --ignore-existing for
# directories, `[ -f ]` guards for single files.
RIG_BACKUP=""
for cand in "$SCRIPT_DIR/../rig-backup/latest/$(hostname).local" \
            "$SCRIPT_DIR/../rig-backup/latest/$(hostname)" \
            "$SCRIPT_DIR/../rig-backup/latest/demiurge.local" \
            "$SCRIPT_DIR/../rig-backup/latest/"*; do
    if [ -d "$cand/home/demiurge" ]; then RIG_BACKUP="$cand/home/demiurge"; break; fi
done
if [ -n "$RIG_BACKUP" ]; then
    echo "  seeding user files from rig backup: $RIG_BACKUP"
    for d in sets presets state; do
        if [ -d "$RIG_BACKUP/$d" ]; then
            mkdir -p "$HOME/demiurge/$d"
            rsync -a --ignore-existing "$RIG_BACKUP/$d/" "$HOME/demiurge/$d/"
        fi
    done
    for f in live.conf asound.state interface.conf graph.conf touch-cal.conf ui-state.conf sync.conf; do
        if [ -f "$RIG_BACKUP/$f" ] && [ ! -f "$HOME/demiurge/$f" ]; then
            install -m 0644 "$RIG_BACKUP/$f" "$HOME/demiurge/$f"
            echo "    restored $f"
        fi
    done
else
    echo "  no rig backup in the repo (rig-backup/latest/<host>/home/demiurge) -- repo defaults only"
fi

if [ ! -f "$HOME/demiurge/live.conf" ]; then
  if have demiurge/live.conf; then
    install -m 0644 "$SCRIPT_DIR/../demiurge/live.conf" "$HOME/demiurge/live.conf"
    echo "    live.conf: repo default (no backup copy)"
  else
    # Generic checkout: there is no repo live.conf to copy (demiurge/ is not part of
    # the public tree), so write a minimal, valid starter. Never overwrites: this
    # whole branch only runs when ~/demiurge/live.conf does not exist.
    cat > "$HOME/demiurge/live.conf" <<'LIVECONF'
# ~/demiurge/live.conf -- the one user-facing DEMIURGE config.
# Generic starter written by setup-demiurge.sh. Edit freely; the installer never
# overwrites this file. Unknown keys are ignored; '#' starts a comment.

# ---------- globals ----------
link    = off
bpm     = 120
rate    = 48000
quantum = 128

# ---------- sync layer ----------
# on  = the chain runs as PipeWire stages wired into demiurge-sink
# off = the first program in the chain talks to the interface directly (bypass)
sync_layer = off

# ---------- rnbo ----------
rnbo = off

# ---------- touch ----------
touch = off

# ---------- chain ----------
# One program per indented line (absolute path or ~/...). Language is taken from
# the file extension (.csd .ck .pd .scd .dsp .strudel .cpp .rnbo).
chain =
LIVECONF
    echo "    live.conf: generic starter (no repo default, no backup copy)"
  fi
fi
# A live.conf restored from another install can carry an absolute /home/<user>/
# path for a user that no longer exists (that crash-looped the UI 157 times on
# the 2026-10-02 rebuild). Warn, never edit: live.conf is his file.
if grep -nE '^[^#]*/home/[A-Za-z0-9_-]+/' "$HOME/demiurge/live.conf" | grep -v "/home/$PI_USER/" >/dev/null 2>&1; then
    echo "  !!! live.conf has /home/<other-user>/ paths -- use \$HOME instead:"
    grep -nE '^[^#]*/home/[A-Za-z0-9_-]+/' "$HOME/demiurge/live.conf" | grep -v "/home/$PI_USER/" || true
fi

# The NEPTR sets (neptr_gorcore is his active one). Added WITHOUT overwriting,
# after the backup seed above so a set he edited on the rig wins over the repo
# copy. The repo's demiurge/ tree has no sets/, so before this the active set
# existed only on the SD card and died with it.
mkdir -p "$HOME/demiurge/sets"
if [ -d "$SCRIPT_DIR/../neptr/scripts/sets" ]; then
rsync -a --ignore-existing "$SCRIPT_DIR/../neptr/scripts/sets/" "$HOME/demiurge/sets/"
else
    skip_absent "neptr/scripts/sets/"
fi

# Strudel runner dependencies
if [ -f "$HOME/demiurge/strudel/package.json" ]; then
    npm install --prefix "$HOME/demiurge/strudel" --silent
fi

# 3) Install /boot/firmware/demiurge.conf if missing
if [ ! -f /boot/firmware/demiurge.conf ]; then
    sudo install -m 0644 "$SCRIPT_DIR/../config/demiurge.conf.default" /boot/firmware/demiurge.conf
fi

# 4) Install + enable systemd service (+ every drop-in present in the repo)
# The repo ships the unit with gorpi7 / UID 1000 as placeholders. Render it
# for whoever is actually running the install so the launcher starts as this
# user (avoids status=217/USER when the Pi user isn't named gorpi7).
_UID="$(id -u)"
_GRP="$(id -gn)"
if have config/demiurge.service; then
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s/^Group=.*/Group=$_GRP/" \
    -e "s#^Environment=HOME=.*#Environment=HOME=$HOME#" \
    -e "s/1000/$_UID/g" \
    "$SCRIPT_DIR/../config/demiurge.service" | sudo tee /etc/systemd/system/demiurge.service >/dev/null
else
    skip_absent "config/demiurge.service (launcher unit)"
fi
sudo install -d /etc/systemd/system/demiurge.service.d
# Drop-ins in config/systemd/demiurge.service.d/ (glob, so a new one can never
# go missing again): 10-clock-lockstep (rate/quantum + mixer restore before the
# engine starts), 20-state-ram (RAM state seed/flush), cpu-affinity.
# 60-scrub-stray-mics.conf is RETIRED (moved to config/_attic): it hardcoded a
# specific Focusrite serial, brand-named and dead -- the launcher scrubs stray
# mic links generically. Remove the stale copy a previous install left behind.
sudo rm -f /etc/systemd/system/demiurge.service.d/60-scrub-stray-mics.conf
for dropin in "$SCRIPT_DIR/../config/systemd/demiurge.service.d/"*.conf; do
    [ -e "$dropin" ] || continue
    sudo install -m 0644 "$dropin" "/etc/systemd/system/demiurge.service.d/$(basename "$dropin")"
done
sudo systemctl daemon-reload
if have config/demiurge.service; then
sudo systemctl enable demiurge.service
fi

echo "=== Phase 6 complete ==="
echo ""
fi

# ── Phase 7: M8 Performance ──────────────────

if ! skip 7; then
echo "=== Phase 7: M8 performance config ==="

PERF_DIR="$SCRIPT_DIR/../config/pi5-performance"

# RT limits for @audio (takes effect at next login)
sudo install -m 0644 "$PERF_DIR/demiurge-audio.limits.conf" /etc/security/limits.d/demiurge-audio.conf

# PipeWire low-latency quantum
sudo install -m 0644 "$PERF_DIR/demiurge-pipewire-lowlatency.conf" /etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf

# Power level (live.conf `power = low|medium|high`): ONE root oneshot applies
# governor/freq/core-count at boot and on `demiurge-power <level>`. It replaces
# the old demiurge-cpu-governor unit and neptr's demiurge-power-restore (retired).
# `high` relies on the overclock block Phase 8 writes (config.txt.snippet).
# The user-facing wrapper + demlow/demmed/demhigh come from the src/wrappers glob
# (Phase 3) into /opt/demiurge/bin; /usr/local/bin gets symlinks so SSH and the
# UIs find them.
sudo install -m 0755 "$PERF_DIR/demiurge-power.sh" /usr/local/bin/demiurge-power.sh
sudo install -m 0644 "$PERF_DIR/demiurge-power.service" /etc/systemd/system/demiurge-power.service
sudo install -m 0755 "$SCRIPT_DIR/../src/wrappers/demiurge-power" /opt/demiurge/bin/demiurge-power
for n in demiurge-power demlow demmed demhigh; do
    sudo ln -sf /opt/demiurge/bin/demiurge-power "/usr/local/bin/$n"
done
sudo systemctl disable --now demiurge-cpu-governor.service 2>/dev/null || true
sudo systemctl disable --now demiurge-power-restore.service 2>/dev/null || true
sudo rm -f /usr/local/bin/demiurge-cpu-governor.sh \
           /etc/systemd/system/demiurge-cpu-governor.service \
           /usr/local/bin/demiurge-power-restore \
           /etc/systemd/system/demiurge-power-restore.service

# UI launcher — config-driven on-device UI. Reads `ui` / `ui_cmd` / `ui_also`
# from live.conf and launches the face UI + any aux commands (central control
# from the one user-facing config file). neptr-ui.service (which calls this)
# ships with NEPTR_phase4 and is installed/enabled by its sync_to_pi.sh — here
# we just make sure the launcher binary exists so the unit's ExecStart resolves.
if have config/pi5-performance/demiurge-ui-launch.sh; then
sudo install -m 0755 "$PERF_DIR/demiurge-ui-launch.sh" /usr/local/bin/demiurge-ui-launch.sh
else
    skip_absent "config/pi5-performance/demiurge-ui-launch.sh"
fi

# Thermal watchdog
sudo install -m 0755 "$PERF_DIR/demiurge-thermal-watchdog.sh" /usr/local/bin/demiurge-thermal-watchdog.sh
sudo install -m 0644 "$PERF_DIR/demiurge-thermal-watchdog.service" /etc/systemd/system/demiurge-thermal-watchdog.service
sudo install -m 0644 "$PERF_DIR/demiurge-thermal-watchdog.timer" /etc/systemd/system/demiurge-thermal-watchdog.timer

# ── Network stability pack ───────────────────────────────────────────────
# Every tool in this project addresses the Pi as `demiurge.local`, so link
# stability is a precondition for everything else, not a nicety. See
# config/network/README.md for the three separate faults this fixes.
NET_DIR="$SCRIPT_DIR/../config/network"

# Persistent journal FIRST. Raspberry Pi OS ships Storage=volatile, so every
# reboot erased the evidence and no outage could ever be post-mortemed. The
# 99- prefix is load-bearing: drop-ins merge by filename across /etc and /usr,
# so it has to sort after the vendor's 40-rpi-volatile-storage.conf.
sudo mkdir -p /etc/systemd/journald.conf.d
sudo install -m 0644 "$NET_DIR/99-demiurge-persistent-journal.conf" \
    /etc/systemd/journald.conf.d/99-demiurge-persistent-journal.conf
sudo systemctl restart systemd-journald
sudo journalctl --flush

# NetworkManager-wide defaults: wifi powersave off, permanent (non-random) MAC
# so DHCP reservations hold, ethernet route-metric 100 vs wifi 600, and above
# all connection.autoconnect-retries=0 — NM's default of 4 turns a brief AP
# reboot into an indefinite outage.
sudo install -o root -g root -m 0644 "$NET_DIR/10-demiurge-network.conf" \
    /etc/NetworkManager/conf.d/10-demiurge-network.conf

# The guard + its config. Enforces "one link at a time" (ethernet primary,
# wifi parked while the cable works) so `demiurge.local` can only ever resolve
# to one address, and recovers a dead wifi link on a ladder. Never exits
# non-zero — it must not poison `systemctl --failed` when the network is down.
sudo install -m 0755 "$NET_DIR/demiurge-netguard.sh" /usr/local/bin/demiurge-netguard.sh
sudo install -m 0644 "$NET_DIR/demiurge-netguard.service" /etc/systemd/system/demiurge-netguard.service
sudo install -m 0644 "$NET_DIR/demiurge-netguard.timer" /etc/systemd/system/demiurge-netguard.timer
sudo mkdir -p /etc/demiurge
# Never clobber a tuned config on an existing box — HOME_SSID is site-specific.
if [ ! -f /etc/demiurge/network.conf ] && [ ! -f "$NET_DIR/demiurge-network.conf.default" ]; then
    skip_absent "config/network/demiurge-network.conf.default (netguard runs on its built-in defaults)"
elif [ ! -f /etc/demiurge/network.conf ]; then
    sudo install -m 0644 "$NET_DIR/demiurge-network.conf.default" /etc/demiurge/network.conf
    echo "--- Installed /etc/demiurge/network.conf — set HOME_SSID to this site's network."
else
    echo "--- /etc/demiurge/network.conf exists, left alone."
fi

# Dispatcher hook: fires the guard the instant the wired link changes, so a
# cable pull fails over in ~2s instead of waiting up to 20s for the timer.
# MUST be root-owned and non-group/world-writable or NM silently refuses it.
sudo install -o root -g root -m 0755 "$NET_DIR/90-demiurge-failover" \
    /etc/NetworkManager/dispatcher.d/90-demiurge-failover

# Polkit rule: without it, every wifi join/forget/scan run by demiurge-wifi
# (from demiurge-web or over ssh — neither has an active local session) is
# silently denied by NM's stock "local && active" policy. This is what made
# the web UI's network picker look unresponsive.
sudo mkdir -p /etc/polkit-1/rules.d
sudo install -o root -g root -m 0644 "$NET_DIR/50-demiurge-netdev.rules" \
    /etc/polkit-1/rules.d/50-demiurge-netdev.rules
sudo systemctl restart polkit

# brcmfmac driver workaround — documented Pi 5 + Trixie bug (association
# failures joining any network other than whichever one is already
# associated; dmesg shows "brcmf_set_channel: set chanspec ... fail, reason
# -52"). roamoff=1 is already shipped by Raspberry Pi OS in
# /lib/modprobe.d/rpi-brcmfmac.conf; feature_disable is a separate, additional
# workaround this box also needs. REQUIRES A REBOOT — modprobe.d changes are
# not runtime-applicable to an already-loaded module.
sudo install -o root -g root -m 0644 "$NET_DIR/50-demiurge-brcmfmac.conf" \
    /etc/modprobe.d/50-demiurge-brcmfmac.conf

# IRQ isolation — mask CPU 3 from userspace interrupts so audio DSP
# runs uninterrupted. Required to eliminate intermittent xruns at
# quantum = 128 even when CPU load is low.
sudo install -m 0755 "$PERF_DIR/demiurge-irq-isolate.sh" /usr/local/bin/demiurge-irq-isolate.sh
sudo install -m 0644 "$PERF_DIR/demiurge-irq-isolate.service" /etc/systemd/system/demiurge-irq-isolate.service

# Audio IRQ-thread RT priority — the threaded handlers that actually pace
# audio go to FIFO 85 (above the engine's 74, above the default 50) so the
# interrupt that wakes the engine each block can't queue behind other irq
# threads. Matters at 128 frames and below. Which IRQ that is depends
# entirely on the attached interface (USB host controller / I2S DMA engine /
# something else), so the script derives it at runtime — see its header.
sudo install -m 0755 "$PERF_DIR/demiurge-audio-irq-rt.sh" /usr/local/bin/demiurge-audio-irq-rt.sh
sudo install -m 0644 "$PERF_DIR/demiurge-audio-irq-rt.service" /etc/systemd/system/demiurge-audio-irq-rt.service

# Audio-interface bounce — on-demand sysfs unbind/bind that makes WirePlumber
# re-enumerate a hot-pluggable interface whose node negotiated the wrong rate.
# Script only, NO boot unit: it is invoked by the launcher's
# ensure_usb_rates_stable() when (and only when) a USB audio node exists.
sudo install -m 0755 "$PERF_DIR/demiurge-audio-bounce.sh" /usr/local/bin/demiurge-audio-bounce.sh

# --- Migration: retire the interface-specific units -----------------------
# demiurge-usb-irq-rt      → superseded by demiurge-audio-irq-rt (it matched
#                            only xhci, so it boosted USB controllers carrying
#                            no audio while an I2S HAT's DMA thread stayed at
#                            FIFO 50 — below the engine).
# demiurge-scarlett-*      → matched USB vendor ID 1235 and were a no-op for
#                            every other interface; the rebounce timer also
#                            failed at every boot when WirePlumber was slow,
#                            costing 15s and poisoning `systemctl --failed`.
#                            The launcher does this condition-driven now.
for stale in demiurge-usb-irq-rt.service \
             demiurge-scarlett-bounce.service \
             demiurge-scarlett-rebounce.timer \
             demiurge-scarlett-rebounce.service; do
    sudo systemctl disable --now "$stale" 2>/dev/null || true
    sudo rm -f "/etc/systemd/system/$stale"
done
sudo rm -f /usr/local/bin/demiurge-usb-irq-rt.sh \
           /usr/local/bin/demiurge-scarlett-bounce.sh \
           /usr/local/bin/demiurge-scarlett-rebounce.sh

# The launcher calls the bounce script through sudo, so the NOPASSWD rule has
# to name the new path. Warn rather than edit: sudoers is hand-maintained. The
# file is named after the Pi user (<user>-demiurge), so glob for it instead of
# hardcoding one user's name.
for sf in /etc/sudoers.d/*-demiurge; do
    [ -f "$sf" ] || continue
    if ! sudo grep -q "demiurge-audio-bounce.sh" "$sf" 2>/dev/null; then
        echo "!!! $sf still points at demiurge-scarlett-bounce.sh."
        echo "!!! Update it (sudo visudo -f $sf) to allow"
        echo "!!! /usr/local/bin/demiurge-audio-bounce.sh, or the launcher's rate"
        echo "!!! recovery will silently do nothing."
    fi
done

sudo systemctl daemon-reload
sudo systemctl enable --now demiurge-power.service
sudo systemctl enable --now demiurge-thermal-watchdog.timer
sudo systemctl enable --now demiurge-netguard.timer
sudo nmcli general reload conf || true
sudo systemctl enable --now demiurge-irq-isolate.service
sudo systemctl enable --now demiurge-audio-irq-rt.service

systemctl --user restart pipewire wireplumber || true

# Quantum-experiment safety net. demiurge-quantum-restore.service is a NO-OP
# unless ~/demiurge/backups/quantum-experiment/ACTIVE exists, so enabling it
# permanently costs nothing. Before=demiurge.service: a too-small period can
# livelock the RT thread and the hardware watchdog turns that into a reboot with
# the bad config still on disk -- this puts the known-good config back first.
# It was in the repo but never installed.
sudo install -m 0755 "$SCRIPT_DIR/demiurge-qrestore" /usr/local/sbin/demiurge-qrestore
sudo install -m 0644 "$SCRIPT_DIR/../config/demiurge-quantum-restore.service" /etc/systemd/system/demiurge-quantum-restore.service
sudo systemctl daemon-reload
sudo systemctl enable demiurge-quantum-restore.service

# Retire the /etc copy of 60-demiurge-period.conf. It used to be GENERATED by
# `demiurge-quantum-limits apply-period` (so a fresh install never had it, and
# the device silently ran 50-demiurge.conf's 128x4 whatever live.conf said).
# `demiurge-quantum-limits lockstep` now owns it, and writes it to
# ~/.config/wireplumber/wireplumber.conf.d/ where it shadows /etc. Leaving a
# stale /etc copy would be two mechanisms for one job.
sudo rm -f /etc/wireplumber/wireplumber.conf.d/60-demiurge-period.conf

# Fast-boot safe cuts. Idempotent; masks only services that never participate
# in the audio path. See config/pi5-performance/demiurge-fastboot.sh for the
# full list and rationale.
bash "$PERF_DIR/demiurge-fastboot.sh"

# ── Phase 7d: the four services that previously had NO installer ──────────
#
# Added 2026-10-02, during the rebuild after the SD card died. These four units
# existed in the repo but nothing installed them -- they had been hand-placed on
# the old card, so a card death silently lost them. The one that hurt most is
# demiurge-midi-connect: it is what wires hardware MIDI to the hub IN BOTH
# DIRECTIONS, and without it PMOR is write-only on the bus and never receives a
# single clock tick (diagnosed the hard way on 2026-10-01 -- `aconnect -l` showed
# "Pocket OpGorator ... Connecting To: 14:0" with no "Connected From:" line at
# all, and PMOR's own diag confirmed it had never heard anything).
echo "--- installing the previously hand-placed services ---"

# binaries first
# (each one is conditional on its source existing; a generic checkout lacks some)
if have src/wrappers/demiurge-midi-connect; then
sudo install -m 0755 "$SCRIPT_DIR/../src/wrappers/demiurge-midi-connect"  /usr/local/bin/demiurge-midi-connect
else skip_absent "src/wrappers/demiurge-midi-connect"; fi
sudo install -m 0755 "$SCRIPT_DIR/../src/wrappers/demiurge-capture-guard" /usr/local/bin/demiurge-capture-guard
if have src/wrappers/demiurge-state-ram; then
sudo install -m 0755 "$SCRIPT_DIR/../src/wrappers/demiurge-state-ram"     /usr/local/bin/demiurge-state-ram
else skip_absent "src/wrappers/demiurge-state-ram"; fi
if have neptr/scripts/demiurge-monitor; then
sudo install -m 0755 "$SCRIPT_DIR/../neptr/scripts/demiurge-monitor"      /usr/local/bin/demiurge-monitor
else skip_absent "neptr/scripts/demiurge-monitor"; fi

# units, User=/Group= templated from PI_USER like every other unit here.
# demiurge-capture-guard runs as root by design (it pokes the ALSA DMA), so it
# is installed verbatim rather than templated (and left DISABLED, see below).
for u in src/wrappers/demiurge-midi-connect.service \
         src/wrappers/demiurge-state-flush.service \
         neptr/scripts/demiurge-monitor.service; do
    if ! have "$u"; then skip_absent "$u"; continue; fi
    sed -e "s/^User=.*/User=$PI_USER/" \
        -e "s/^Group=.*/Group=$(id -gn "$PI_USER")/" \
        "$SCRIPT_DIR/../$u" | sudo tee "/etc/systemd/system/$(basename "$u")" > /dev/null
done
sudo install -m 0644 "$SCRIPT_DIR/../config/demiurge-capture-guard.service" /etc/systemd/system/demiurge-capture-guard.service
if have src/wrappers/demiurge-state-flush.timer && have src/wrappers/demiurge-state-ram; then
sudo install -m 0644 "$SCRIPT_DIR/../src/wrappers/demiurge-state-flush.timer" /etc/systemd/system/demiurge-state-flush.timer
else skip_absent "src/wrappers/demiurge-state-flush.timer"; fi

sudo systemctl daemon-reload
# demiurge-capture-guard is installed but DISABLED (2026-10-02). It runs as
# root and on a stall detection stops demiurge, stops PipeWire and rebinds the
# codec; its premise (a capture DMA stall) was disproved -- see memory
# adc-capture-dma-stall -- so a false positive would take the instrument down
# for nothing. The unit and script stay in the repo for the next real stall
# investigation: `sudo systemctl enable --now demiurge-capture-guard`.
sudo systemctl disable --now demiurge-capture-guard.service 2>/dev/null || true
_NOW_UNITS=()
if have src/wrappers/demiurge-midi-connect.service; then _NOW_UNITS+=(demiurge-midi-connect.service); fi
if have neptr/scripts/demiurge-monitor.service; then _NOW_UNITS+=(demiurge-monitor.service); fi
if [ "${#_NOW_UNITS[@]}" -gt 0 ]; then sudo systemctl enable --now "${_NOW_UNITS[@]}"; fi
if have src/wrappers/demiurge-state-flush.timer && have src/wrappers/demiurge-state-ram; then
sudo systemctl enable --now demiurge-state-flush.timer
fi
echo "--- services installed and enabled ---"

# ── Phase 7e: boot face ────────────────────────────────────────────────────
# Draws the NEPTR X_X face (neptr/protoFACES/FACE3.png, already the "off"
# expression -- no new art needed) + "DemiurgeOS" into /dev/fb0 as early in
# boot as a oneshot unit can run. It is a single raw write and exit: it never
# touches the console driver, never takes DRM master, never blocks anything,
# and kernel/systemd boot text keeps scrolling over/through it exactly like a
# classic boot logo -- this is deliberate, not a bug. Cosmetic only; the
# script fails open (any problem --> logs and exits 0, boot unaffected).
#
# Orthogonal to disable_splash=1 / logo.nologo (Phase 8): those two tokens
# only suppress the kernel's OWN built-in CONFIG_LOGO path, which this Pi's
# kernel doesn't even use, and they exist specifically to keep boot text
# visible -- exactly what this unit depends on. Nothing to reconcile; leave
# both as Phase 8 already has them.
#
# Asset is pre-rendered on the Mac (needs Pillow there) via
# config/boot-face/build-boot-face.py -- the Pi needs nothing beyond dd.
# Geometry (480x800x16bpp RGB565, 960-byte stride) was confirmed live via
# `fbset -i` on 2026-10-02; the installed script re-checks it every boot and
# skips the write rather than scribble into a changed panel mode.
#
# Revert in one command:
#   sudo systemctl disable --now demiurge-boot-face.service
if have config/boot-face/boot-face.raw && have config/boot-face/demiurge-boot-face.sh \
   && have config/boot-face/demiurge-boot-face.service; then
echo "--- installing boot face ---"
sudo mkdir -p /usr/local/share/demiurge
sudo install -m 0644 "$SCRIPT_DIR/../config/boot-face/boot-face.raw" /usr/local/share/demiurge/boot-face.raw
sudo install -m 0755 "$SCRIPT_DIR/../config/boot-face/demiurge-boot-face.sh" /usr/local/bin/demiurge-boot-face.sh
sudo install -m 0644 "$SCRIPT_DIR/../config/boot-face/demiurge-boot-face.service" /etc/systemd/system/demiurge-boot-face.service
sudo systemctl daemon-reload
sudo systemctl enable --now demiurge-boot-face.service
echo "--- boot face installed ---"
else
    skip_absent "config/boot-face/ (boot splash)"
fi

# ── Phase 7f: NEPTR VIDEO + HOME mode selector + arcade console ───────────
#
# Added 2026-10-02, same rebuild as Phase 7d: the Pi got audio back but lost
# video and games entirely (NEPTR_VIDEO had never been installed on this card
# at all, and demiurge-arcade.service had NEVER had an installer anywhere —
# games-install.sh explicitly deferred it, "install a user service later").
#
# NEPTR_VIDEO is its own repo/deploy, NOT part of this checkout, so this phase
# does not build it — that's NEPTR_VIDEO/deploy/deploy-video.sh's job, run
# from the Mac (it rsyncs source + cmake builds + runs ctest). This phase:
#   1. installs the apt build deps deploy-video.sh needs, so that script can
#      ever succeed here
#   2. installs + enables neptr-video.service IF the binary already exists
#      (i.e. deploy-video.sh has been run at least once) — templated for
#      PI_USER like every other unit in this script
#   3. installs the arcade's build deps + uinput/groups/sudoers (the part of
#      demiurge/games/bin/games-install.sh that is host setup, not source
#      deploy) and installs + enables demiurge-arcade.service, templated
#
# BOOT ARBITRATION: neptr-video.service becomes the thing WantedBy=multi-user
# at boot (it boots into HOME, NEPTR's mode selector — AUDIO/VIDEO/GAMES).
# neptr-ui.service must stay DISABLED (not masked -- HOME starts it on demand
# via deploy/mode-audio.sh) or the two race for drmSetMaster at boot. This
# script does not touch demiurge.service's own enablement -- the audio ENGINE
# (csound etc., no display) keeps autostarting either way, only the FACE UI
# arbitration changes. See NEPTR_VIDEO/deploy/README.md section "One display,
# one master" before touching any of this by hand.
echo "--- NEPTR VIDEO + HOME mode selector + arcade console ---"

if [ "$HAVE_NEPTR" = 1 ]; then
echo "build deps for NEPTR_VIDEO (deploy-video.sh builds the binary itself)"
sudo apt-get install -y libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev
else
    skip_absent "neptr/ (NEPTR_VIDEO build deps)"
fi

NV_DIR="/home/$PI_USER/NEPTR_VIDEO"
NV_BIN="$NV_DIR/build/neptr-video"
if [ -x "$NV_BIN" ]; then
    sed -e "s/^User=.*/User=$PI_USER/" \
        -e "s/^Group=.*/Group=$(id -gn "$PI_USER")/" \
        -e "s|^Environment=HOME=.*|Environment=HOME=/home/$PI_USER|" \
        -e "s|^WorkingDirectory=.*|WorkingDirectory=$NV_DIR|" \
        -e "s|^ExecStart=.*neptr-video\$|ExecStart=/usr/bin/stdbuf -oL -eL $NV_BIN|" \
        "$NV_DIR/deploy/neptr-video.service" | sudo tee /etc/systemd/system/neptr-video.service > /dev/null
    sudo systemctl daemon-reload
    sudo systemctl enable neptr-video.service
    sudo systemctl disable neptr-ui.service 2>/dev/null || true
    echo "--- neptr-video.service installed + enabled (HOME owns the display at boot) ---"
else
    echo "  (skipping neptr-video.service: $NV_BIN not built yet -- run"
    echo "   NEPTR_VIDEO/deploy/deploy-video.sh $PI_USER@<host> from the Mac first,"
    echo "   then re-run this phase to install the unit)"
fi

if have demiurge/games; then
echo "arcade console deps (retroarch, python3-evdev, uinput, groups, sudoers)"
sudo apt-get install -y retroarch python3-evdev python3-mido python3-rtmidi
sudo apt-get install -y libretro-fceumm libretro-nestopia 2>/dev/null || \
    echo "  (some libretro cores not in apt -- RetroArch Online Updater > Core Downloader)"
sudo modprobe uinput || true
echo "uinput" | sudo tee /etc/modules-load.d/uinput.conf > /dev/null
echo 'KERNEL=="uinput", MODE="0660", GROUP="input", OPTIONS+="static_node=uinput"' \
    | sudo tee /etc/udev/rules.d/99-demiurge-uinput.rules > /dev/null
sudo udevadm control --reload-rules && sudo udevadm trigger || true
sudo usermod -aG input,dialout "$PI_USER"
if [ "$HAVE_NEPTR" = 1 ]; then
ARCADE_SUDOERS=/etc/sudoers.d/demiurge-arcade
sudo tee "$ARCADE_SUDOERS" > /dev/null <<EOF
$PI_USER ALL=(root) NOPASSWD: /usr/bin/systemctl stop neptr-ui.service
$PI_USER ALL=(root) NOPASSWD: /usr/bin/systemctl start neptr-ui.service
EOF
sudo chmod 0440 "$ARCADE_SUDOERS"
sudo visudo -cf "$ARCADE_SUDOERS"
else
    skip_absent "neptr/ (neptr-ui sudoers for the arcade)"
fi
else
    skip_absent "demiurge/games (arcade console deps)"
fi

GAMES_DIR="/home/$PI_USER/demiurge/games"
if [ -f "$GAMES_DIR/config/demiurge-arcade.service" ]; then
    sed -e "s/^User=.*/User=$PI_USER/" \
        -e "s/^Group=.*/Group=$(id -gn "$PI_USER")/" \
        -e "s|^Environment=HOME=.*|Environment=HOME=/home/$PI_USER|" \
        -e "s|^WorkingDirectory=.*|WorkingDirectory=$GAMES_DIR|" \
        -e "s|^ExecStart=.*|ExecStart=/usr/bin/python3 $GAMES_DIR/arcade_supervisor.py|" \
        "$GAMES_DIR/config/demiurge-arcade.service" | sudo tee /etc/systemd/system/demiurge-arcade.service > /dev/null
    sudo systemctl daemon-reload
    sudo systemctl enable demiurge-arcade.service
    echo "--- demiurge-arcade.service installed + enabled (ConditionPathExists gates it on the Teensy being attached) ---"
else
    echo "  (skipping demiurge-arcade.service: $GAMES_DIR not deployed yet)"
fi
echo "--- NEPTR VIDEO + arcade console phase done ---"

# Generate the device-side clock config from live.conf AS THE USER, so the
# period / rate / clock fragments exist before the first boot of the engine
# (demiurge.service runs the same command as ExecStartPre from then on). They
# land in ~/.config/{wireplumber,pipewire}, which is why no sudo: this must NOT
# run as root or the fragments would be root-owned in the user's config dirs.
# Needs the user's runtime dir + bus to reach wireplumber/pipewire. Non-fatal:
# lockstep never fails the service and neither may the installer.
echo "--- generating clock fragments from live.conf (demiurge-quantum-limits lockstep) ---"
_RT="/run/user/$(id -u "$PI_USER")"
if [ "$(id -u)" -eq 0 ] && [ "$PI_USER" != root ]; then
    sudo -u "$PI_USER" env HOME="$HOME" XDG_RUNTIME_DIR="$_RT" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$_RT/bus" \
        /opt/demiurge/bin/demiurge-quantum-limits lockstep || echo "  (lockstep reported a problem; it re-runs at every engine start)"
else
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$_RT}" \
        DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$_RT/bus}" \
        /opt/demiurge/bin/demiurge-quantum-limits lockstep || echo "  (lockstep reported a problem; it re-runs at every engine start)"
fi

echo "=== Phase 7 complete ==="
echo ""
fi

echo "========================================"
echo "  DEMIURGE Setup complete"
echo "========================================"
echo ""
echo "Verify:"
echo "  pw-cli ls Node | grep demiurge      # virtual audio nodes"
echo "  aconnect -l                          # MIDI Through"
echo "  ls ~/demiurge/                       # docs/ examples/ live.conf"
echo "  source /opt/demiurge/lib/demiurge-env.sh; echo \$DEMIURGE_OSC_BASE  # OSC base port (9000)"
echo "  cat /boot/firmware/demiurge.conf     # launch = ... path"
echo "  sudo systemctl start demiurge        # run the launcher"
echo "  journalctl -u demiurge -f            # watch it"
echo "  journalctl -u demiurge-thermal-watchdog -f   # live temps"
echo "  journalctl -u demiurge-netguard -f           # live link state"
echo "  ip route | grep default                      # wired should be metric 100"
echo ""
echo "OPTIONAL — RNBO support was NOT installed by this script. It's a"
echo "separate opt-in chain (external apt repo + from-source build). If you"
echo "want it, run in order:"
echo "  sudo bash setup-script/rnbo-add-repo.sh"
echo "  bash setup-script/rnbo-build-deps.sh"
echo "  sudo bash setup-script/rnbo-install-runner.sh"
echo "  bash setup-script/deploy-rnbo-wave1.sh"
echo "See demiurge/docs/rnbo.md for why it's gated this way."
echo ""

# ── Phase 8: Boot config (overclock + kernel cmdline) ────────────────────────
# WARNING: edits /boot/firmware/config.txt and /boot/firmware/cmdline.txt then
# reboots. A bad write here can make the Pi unbootable. Both files are backed
# up before any change and all tokens are applied idempotently.

if ! skip 8; then
echo "========================================================"
echo "  Phase 8: Boot config — DANGEROUS"
echo "========================================================"
echo ""
echo "  This phase edits /boot/firmware/config.txt and"
echo "  /boot/firmware/cmdline.txt, then REBOOTS the Pi."
echo ""
echo "  A bad edit can make the Pi unbootable."
echo "  Both files are backed up before any change is made."
echo "  All tokens are applied idempotently (safe to re-run)."
echo ""
echo "  config.txt will ALWAYS gain (base, no prompt — safe without a 5A PSU):"
echo "    usb_max_current_enable=1  disable_splash=1  boot_delay=0"
echo ""
echo "  config.txt overclock block is OPT-IN and asked for separately:"
echo "    arm_freq=2800  over_voltage_delta=50000  force_turbo=0"
echo "    gpu_freq=500   arm_boost=1"
echo ""
echo "  cmdline.txt will gain:"
echo "    threadirqs  usbcore.autosuspend=-1  isolcpus=3"
echo "    nohz_full=3  rcu_nocbs=3  fsck.mode=skip  logo.nologo"
echo ""
echo "  REQUIREMENTS (or don't proceed):"
echo "    - Official 5V/5A USB-C PSU (NOT a 3A phone charger)"
echo "    - Active cooling (official Pi 5 Active Cooler or equivalent)"
echo ""
read -r -p "  Apply boot config and reboot? [yes/no]: " _BOOT_CONFIRM
echo ""

if [[ "$_BOOT_CONFIRM" != "yes" ]]; then
    echo "Skipped. Re-run with: ./setup-demiurge.sh 8"
    exit 0
fi

_PERF_DIR="$SCRIPT_DIR/../config/pi5-performance"
_CONFIG_TXT="/boot/firmware/config.txt"
_CMDLINE_TXT="/boot/firmware/cmdline.txt"
_TS="$(date +%Y%m%d-%H%M%S)"

# Backup both files before touching anything
sudo cp "$_CONFIG_TXT"  "${_CONFIG_TXT}.bak-${_TS}"
sudo cp "$_CMDLINE_TXT" "${_CMDLINE_TXT}.bak-${_TS}"
echo "Backed up:"
echo "  ${_CONFIG_TXT}.bak-${_TS}"
echo "  ${_CMDLINE_TXT}.bak-${_TS}"
echo ""

# ── config.txt ──────────────────────────────────────────────────────────────
# --- BASE block: always applied, never prompted (EMGOR 2026-10-21) ----------
# usb_max_current_enable=1 is a standing rule, not a per-install choice, and it
# must not be hostage to the overclock decision the way it used to be.
if grep -q "^usb_max_current_enable=" "$_CONFIG_TXT" 2>/dev/null; then
    echo "config.txt: base block already present — skipping (already applied)"
else
    echo "config.txt: appending BASE block (usb_max_current_enable, fast boot)..."
    echo "" | sudo tee -a "$_CONFIG_TXT" > /dev/null
    sudo tee -a "$_CONFIG_TXT" < "$_PERF_DIR/config.txt.base.snippet" > /dev/null
    echo "config.txt: base done"
fi
echo ""

# --- OVERCLOCK block: opt-in, its own prompt, default NO --------------------
# Default power mode is `medium` (stock 2400), so an unconfigured rig never
# silently runs overclocked. Answer yes only on the official 5V/5A PSU with
# active cooling.
if grep -q "^arm_freq=" "$_CONFIG_TXT" 2>/dev/null; then
    echo "config.txt: arm_freq already present — skipping (already applied)"
else
    read -r -p "  Also apply the 2800 MHz OVERCLOCK? Needs 5V/5A PSU + active cooling [yes/NO]: " _OC_CONFIRM
    echo ""
    if [[ "$_OC_CONFIRM" == "yes" ]]; then
        echo "config.txt: appending overclock block..."
        echo "" | sudo tee -a "$_CONFIG_TXT" > /dev/null
        sudo tee -a "$_CONFIG_TXT" < "$_PERF_DIR/config.txt.snippet" > /dev/null
        echo "config.txt: overclock done — set 'power = high' in live.conf to use it"
    else
        echo "config.txt: overclock SKIPPED (stock 2400 MHz). Re-run phase 8 to add it later."
    fi
fi
echo ""

# ── cmdline.txt ─────────────────────────────────────────────────────────────
# cmdline.txt MUST be a single line — never add newlines.
# No quiet / loglevel=3 / fastboot / systemd.show_status=false — boot text
# stays visible on HDMI by design. fsck.mode=skip already covers what
# fastboot would do, and the visible-log slowdown is ~50-200 ms (<2%).
_CMDLINE_TOKENS=(
    "threadirqs"
    "usbcore.autosuspend=-1"
    "isolcpus=3"
    "nohz_full=3"
    "rcu_nocbs=3"
    "fsck.mode=skip"
    "logo.nologo"
)

_CMDLINE_NEW="$(cat "$_CMDLINE_TXT")"

for _token in "${_CMDLINE_TOKENS[@]}"; do
    # Space-pad both sides so we match whole tokens only
    case " $_CMDLINE_NEW " in
        *" $_token "*) echo "cmdline.txt: $_token already present — skipping" ;;
        *) _CMDLINE_NEW="$_CMDLINE_NEW $_token"
           echo "cmdline.txt: adding $_token" ;;
    esac
done

# Hard safety check: result must still be exactly one line
_LINECOUNT="$(printf '%s' "$_CMDLINE_NEW" | wc -l)"
if [[ "$_LINECOUNT" -gt 0 ]]; then
    echo ""
    echo "ERROR: cmdline.txt result has multiple lines ($_LINECOUNT) — aborting"
    echo "Backups preserved at ${_CONFIG_TXT}.bak-${_TS} and ${_CMDLINE_TXT}.bak-${_TS}"
    echo "Restore with: sudo cp ${_CMDLINE_TXT}.bak-${_TS} $_CMDLINE_TXT"
    exit 1
fi

_TMPFILE="$(mktemp)"
printf '%s\n' "$_CMDLINE_NEW" > "$_TMPFILE"
sudo cp "$_TMPFILE" "$_CMDLINE_TXT"
rm -f "$_TMPFILE"
echo "cmdline.txt: written"
echo ""

echo "Final cmdline.txt:"
cat "$_CMDLINE_TXT"
echo ""

echo "=== Phase 8 complete — rebooting in 5 seconds ==="
echo "    Ctrl-C now to cancel."
sleep 5
sudo reboot
fi


# ── Phase 9: Web UI (demiurge-web) ────────────────────────────────────────────
# NOTE: Phase 8 ends in a reboot, so on a fresh install run this phase
# separately afterwards:  ./setup-demiurge.sh 9

if ! skip 9; then
echo "=== Phase 9: Web UI (demiurge-web) ==="

if ! have web/demiurge_web.py || ! have config/demiurge-web.service; then
    skip_absent "web/ + config/demiurge-web.service (web UI)"
else

# Status daemon + browser mirror of the demiurge companion (stdlib-only Python).
# Serves http://demiurge.local:8080; writes ~/.demiurge/status every 2 s.
sudo install -d /opt/demiurge/web/static
sudo install -m 0755 "$SCRIPT_DIR/../web/demiurge_web.py" /opt/demiurge/web/demiurge_web.py
sudo install -m 0644 "$SCRIPT_DIR/../web/static/index.html" \
                     "$SCRIPT_DIR/../web/static/app.js" \
                     "$SCRIPT_DIR/../web/static/style.css" /opt/demiurge/web/static/

# Unit: pinned off the audio core (CPUAffinity=0-2), Nice=10, deliberately
# NOT PartOf=demiurge.service (UI must survive audio stop). The repo ships
# User=gorpi7 as a placeholder — render it for whoever is actually running
# the install, same templating as demiurge.service in Phase 6 (avoids
# status=217/USER when the Pi user isn't named gorpi7).
# Confirm the Pi-side power helper name (`which demiurge-power`) and adjust
# Environment=DEMIURGE_POWER_CMD in the unit if it differs.
sed -e "s/^User=.*/User=$PI_USER/" \
    "$SCRIPT_DIR/../config/demiurge-web.service" | sudo tee /etc/systemd/system/demiurge-web.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable --now demiurge-web
fi

echo "=== Phase 9 complete — open http://demiurge.local:8080 ==="
echo ""
fi

# ── Phase 10: RNBO (Cycling '74 OSCQuery runner) ────────────────────────────
# NOTE: this does a real from-source compile (RNBO + libossia + boost, etc.
# built via conan when not already cached) — on a Pi 5 the first run can take
# tens of minutes. Re-running (e.g. `./setup-demiurge.sh 10`) is much faster
# once conan's local cache is warm and the runner repo is already cloned.

if ! skip 10; then
echo "=== Phase 10: RNBO ==="

export PATH="$HOME/.local/bin:$PATH"

# 1) Add Cycling '74's apt repo (trixie/arm64) + signing key. Installs
#    nothing by itself — this step exists so `apt-cache policy rnbooscquery`
#    can confirm the repo is reachable and has a candidate for this
#    arch/suite before we sink time into a from-source build. (We do NOT
#    install the prebuilt rnbooscquery package itself — see step 3 below for
#    why: it ships on-device compile DISABLED, and installing it alongside
#    our source-built /opt/rnbo/bin binary would add nothing but a second,
#    weaker copy of the runner to keep straight.)
RNBO_KEY_URL="https://raw.githubusercontent.com/Cycling74/rnbo.oscquery.runner/main/config/apt-cycling74-pubkey.asc"
RNBO_KEY_DST="/usr/share/keyrings/apt-cycling74-pubkey.asc"
RNBO_LIST_DST="/etc/apt/sources.list.d/cycling74.list"

sudo curl -fsSL "$RNBO_KEY_URL" -o "$RNBO_KEY_DST"
sudo chmod 0644 "$RNBO_KEY_DST"
cat <<EOF | sudo tee "$RNBO_LIST_DST" >/dev/null
deb [signed-by=$RNBO_KEY_DST arch=arm64] https://c74-apt.nyc3.digitaloceanspaces.com/raspbian/ trixie main extra
EOF
sudo apt-get update

RNBO_CAND="$(apt-cache policy rnbooscquery 2>/dev/null | awk '/Candidate:/{print $2; exit}')"
if [[ -n "$RNBO_CAND" && "$RNBO_CAND" != "(none)" ]]; then
    echo "rnbooscquery visible via apt: candidate $RNBO_CAND (informational only — not installed)"
else
    echo "WARNING: rnbooscquery has no apt candidate for this arch/suite — repo may be unreachable." >&2
fi

# 2) Build toolchain: apt build deps + pinned conan (user-level pip --user)
#    + a default conan profile pinned to c++20 / libstdc++11 (matches what
#    the runner's own CMakeLists expects on Linux).
sudo apt-get install -y \
    libavahi-compat-libdnssd-dev build-essential libssl-dev libjack-jackd2-dev \
    libdbus-1-dev libxml2-dev libgmock-dev libsdbus-c++-dev libsndfile1-dev \
    cmake ccache git python3-pip

pip3 install --break-system-packages --user "conan==1.66.0"
hash -r
conan --version

conan profile new default --detect --force || true
conan profile update settings.compiler.libcxx=libstdc++11 default || true
conan profile update settings.compiler.cppstd=20 default || true

# 3) Clone + build rnbo.oscquery.runner FROM SOURCE with on-device compile
#    enabled. The prebuilt apt package (checked in step 1) ships compile
#    disabled, so this is the only path that gets DEMIURGE the real
#    on-device-compile runner. See rnbo-build-runner.sh's header comment for
#    the researched details (pinned tag, and how RNBO_DIR gets resolved
#    headlessly via Cycling '74's public conan remote).
bash "$SCRIPT_DIR/rnbo-build-runner.sh"

# 4) Install the compile-enabled runner binary -> /opt/rnbo/bin.
RNBO_RUNNER_BIN="$HOME/rnbo.oscquery.runner/build/bin/rnbooscquery"
sudo install -d /opt/rnbo/bin
sudo install -m0755 "$RNBO_RUNNER_BIN" /opt/rnbo/bin/rnbooscquery

# 5) Runner config (user-level).
install -d "$HOME/.config/rnbo"
install -m0644 "$SCRIPT_DIR/../config/rnbo/demiurge-runner.json" "$HOME/.config/rnbo/demiurge.json"

# 6) RNBO auto-reload watcher (--user: reload slot 0 whenever Max pushes a
#    new compile of the patch in the chain).
install -d "$HOME/.local/bin" "$HOME/.config/systemd/user"
install -m0755 "$SCRIPT_DIR/../config/rnbo/demiurge-rnbo-autoreload.sh" \
    "$HOME/.local/bin/demiurge-rnbo-autoreload.sh"
install -m0644 "$SCRIPT_DIR/../config/rnbo/demiurge-rnbo-autoreload.service" \
    "$HOME/.config/systemd/user/demiurge-rnbo-autoreload.service"
systemctl --user daemon-reload
systemctl --user enable --now demiurge-rnbo-autoreload.service

# 7) RNBO runner gate (ExecCondition: only run when RNBO is actually wanted —
#    rnbo = on in live.conf, or a *.rnbo token is in the chain) + the
#    rnbo-runner.service unit itself. The repo ships both User=gorpi7 and
#    UID 1000 as placeholders — render them for whoever is actually running
#    the install, same templating as demiurge.service (Phase 6) and
#    demiurge-web.service (Phase 9).
sudo install -m0755 "$SCRIPT_DIR/../config/rnbo/demiurge-rnbo-gate.sh" /usr/local/bin/demiurge-rnbo-gate.sh

_UID="$(id -u)"
_GRP="$(id -gn)"
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s/^Group=.*/Group=$_GRP/" \
    -e "s#^Environment=HOME=.*#Environment=HOME=$HOME#" \
    -e "s#/home/gorpi7#$HOME#g" \
    -e "s/1000/$_UID/g" \
    "$SCRIPT_DIR/../config/rnbo/rnbo-runner.service" | sudo tee /etc/systemd/system/rnbo-runner.service >/dev/null

sudo systemctl daemon-reload
# Still enabled at boot, but ExecCondition keeps it DOWN unless live.conf
# opts in (rnbo = on) or a *.rnbo is in the chain. enable --now is safe: it
# condition-skips cleanly ("condition failed", not "failed") when gated off.
sudo systemctl enable --now rnbo-runner.service
sleep 3

echo ""
echo "--- Phase 10 status ---"
echo -n "rnbo-runner.service: "; systemctl is-active rnbo-runner.service || true
if systemctl is-active --quiet rnbo-runner.service; then
    curl -sf -o /dev/null http://localhost:5678/rnbo/info/version \
        && echo "  OSCQuery up on :5678" \
        || echo "  WARNING: runner active but :5678 not answering yet"
else
    echo "  (inactive/condition-skipped is expected unless live.conf has rnbo = on or a *.rnbo is in the chain)"
fi

echo "=== Phase 10 complete ==="
echo ""
fi

# ──────────────────────────────────────────────────────────────────────────────
# ── Phase 11: Agent toolchain (run agents ON the Pi) ─────────────────────────
# ──────────────────────────────────────────────────────────────────────────────
#
# WHY: debugging this rig from the Mac means every edit is an SSH round trip and
# the agent doing the work cannot see the hardware it is reasoning about. It
# cannot read `aplay -l`, cannot watch `journalctl -f` while a fault happens,
# cannot run demiurge-ringtest while an artifact is actually audible. An agent
# running HERE can. The 2026-10-02 rebuild made that painfully clear.
#
# Installs three backends so they can be compared / used interchangeably, plus
# git + gh so an agent on the Pi can actually commit and push its own work, and
# tmux so a long investigation survives the SSH connection dropping.
#
# AUTH IS INTERACTIVE AND IS NOT DONE HERE. No credentials are baked into this
# repo or this script — see the printed instructions at the end of the phase.

if ! skip 11; then
echo "=== Phase 11: Agent toolchain ==="

sudo apt install -y git tmux curl ca-certificates

# gh (GitHub CLI) — not in Debian's default set on every image. Added from
# GitHub's own apt repo, keyring-verified. Skipped quietly if already present.
if ! command -v gh >/dev/null 2>&1; then
    curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
        | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg status=none
    sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
    sudo apt update -qq && sudo apt install -y gh
fi

# Node: these CLIs want a modern Node. Phase 1 installs Debian's nodejs; if that
# is older than 20 the agent CLIs will install but misbehave at runtime, so say
# so loudly rather than leaving a confusing failure for later.
NODE_MAJOR="$(node -p 'process.versions.node.split(".")[0]' 2>/dev/null || echo 0)"
if [ "$NODE_MAJOR" -lt 20 ]; then
    echo "  WARNING: node $NODE_MAJOR is older than 20. The agent CLIs need >= 20."
    echo "           Install a newer Node (e.g. NodeSource) and re-run: setup-demiurge.sh 11"
fi

# Global npm installs WITHOUT sudo. Writing to /usr/lib/node_modules as root
# leaves root-owned files in the user's toolchain and then `npm update` fails
# confusingly; a per-user prefix keeps the agent able to update its own tools.
NPM_PREFIX="$HOME/.npm-global"
mkdir -p "$NPM_PREFIX"
npm config set prefix "$NPM_PREFIX" >/dev/null 2>&1 || true
case ":$PATH:" in
    *":$NPM_PREFIX/bin:"*) ;;
    *)  for rc in "$HOME/.bashrc" "$HOME/.profile"; do
            [ -f "$rc" ] || continue
            grep -q 'npm-global/bin' "$rc" || \
                printf '\n# agent CLIs (demiurge Phase 11)\nexport PATH="$HOME/.npm-global/bin:$PATH"\n' >> "$rc"
        done ;;
esac
export PATH="$NPM_PREFIX/bin:$PATH"

# The three backends. Each is independent — a failure to fetch one must not
# abort the phase, so they are attempted separately and reported individually.
for pkg in "@anthropic-ai/claude-code:claude" "@google/gemini-cli:gemini" "@openai/codex:codex"; do
    name="${pkg%%:*}"; bin="${pkg##*:}"
    if command -v "$bin" >/dev/null 2>&1; then
        echo "  $bin already installed ($(command -v "$bin"))"
    elif npm install -g --silent "$name" >/dev/null 2>&1; then
        echo "  installed $name -> $bin"
    else
        echo "  WARNING: could not install $name (network? node version?) — retry: npm install -g $name"
    fi
done

# git identity: an agent that commits on the Pi must not produce commits
# attributed to nobody. Defaults to the rig's OWNER, not the machine — a
# hostname-derived identity produced commits by "demiurge <gorpi7v2@demiurge.local>",
# which is noise in a history where every commit is his. The machine is already
# recorded in the commit message when it matters.
# Generic checkout (no neptr/): never invent an identity for someone else's
# machine; set one only if DEMIURGE_GIT_NAME / DEMIURGE_GIT_EMAIL are given.
if [ "$HAVE_NEPTR" = 1 ]; then
git config --global user.name  "${DEMIURGE_GIT_NAME:-EMG0R}" 2>/dev/null || true
git config --global user.email "${DEMIURGE_GIT_EMAIL:-maintainer@example.invalid}" 2>/dev/null || true
else
    [ -z "${DEMIURGE_GIT_NAME:-}" ]  || git config --global user.name  "$DEMIURGE_GIT_NAME"  2>/dev/null || true
    [ -z "${DEMIURGE_GIT_EMAIL:-}" ] || git config --global user.email "$DEMIURGE_GIT_EMAIL" 2>/dev/null || true
fi
git config --global init.defaultBranch main 2>/dev/null || true
# The repo lives in the user's home and is operated on by the same user; mark it
# safe so git does not refuse with "dubious ownership" when a tool runs as root.
git config --global --add safe.directory "$HOME/_______DEMIURGE" 2>/dev/null || true

# Let the MAC push straight into this repo's checked-out branch, updating the
# working tree with it. Without this, git refuses to push to a non-bare repo's
# current branch and the handoff degrades into `git bundle` passed by hand —
# which is exactly what happened on 2026-10-03. The Mac drives in both
# directions (it can reach the Pi; the Pi cannot reach it), via
# setup-script/pi-agent.sh --push / --pull. `updateInstead` refuses rather than
# clobbers when the Pi's tree is dirty, so an agent's uncommitted work is safe.
if [ -d "$HOME/_______DEMIURGE/.git" ]; then
    git -C "$HOME/_______DEMIURGE" config receive.denyCurrentBranch updateInstead 2>/dev/null || true
fi

# ---- shared memory repo (optional, owner's private layer) -------------------
# An agent should know who it works for and keep what it learns. If the owner
# keeps a private memory repo, name it OUTSIDE the public tree: DEMIURGE_MEMORY_REPO
# (env) or MEMORY_REPO= in /demiurge/local/memory.conf (node-local, never synced),
# and optionally MEMORY_DIR= (default ~/memory-repo). The public installer names
# no repo. Cloned over HTTPS with the gh credential helper; a repo that ships
# scripts/install-sync.sh gets its per-machine wiring run (idempotent).
MEMORY_REPO="${DEMIURGE_MEMORY_REPO:-}"; MEMORY_DIR=""
if [ -z "$MEMORY_REPO" ] && [ -f /demiurge/local/memory.conf ]; then
    MEMORY_REPO="$(sed -n 's/^MEMORY_REPO=//p' /demiurge/local/memory.conf | head -1)"
    MEMORY_DIR="$(sed -n 's/^MEMORY_DIR=//p' /demiurge/local/memory.conf | head -1)"
fi
MEMORY_DIR="${MEMORY_DIR:-$HOME/memory-repo}"; MEMORY_DIR="${MEMORY_DIR/#\~/$HOME}"
if [ -z "$MEMORY_REPO" ]; then
    echo "  skip: no memory repo configured (set MEMORY_REPO= in /demiurge/local/memory.conf)"
elif [ ! -d "$MEMORY_DIR/.git" ]; then
    if git clone -q "$MEMORY_REPO" "$MEMORY_DIR" 2>/dev/null; then
        echo "  cloned memory repo -> $MEMORY_DIR"
    else
        echo "  NOTE: could not clone the memory repo (run 'gh auth login' first, then re-run phase 11)"
    fi
fi
if [ -n "$MEMORY_REPO" ] && [ -f "$MEMORY_DIR/scripts/install-sync.sh" ]; then
    bash "$MEMORY_DIR/scripts/install-sync.sh" 2>&1 | sed 's/^/  /'
fi

echo ""
echo "=== Phase 11 complete ==="
echo ""
echo "  AUTHENTICATION IS INTERACTIVE — nothing was logged in for you, and no"
echo "  credentials live in this repo. Run these ON THE PI, once each:"
echo ""
echo "    claude                 # then /login, follow the browser/code flow"
echo "    gemini                 # first run prompts for auth"
echo "    codex                  # first run prompts for auth"
echo "    gh auth login          # choose SSH or HTTPS; enables push from the Pi"
echo ""
if have setup-script/pi-agent.sh; then
echo "  From the MAC, one command (knows the rig from pi-rig.conf):"
echo "    setup-script/pi-agent.sh --auth      # walks the logins above"
echo "    setup-script/pi-agent.sh --list"
echo "    setup-script/pi-agent.sh 'why is csound crash-looping?'"
echo "    setup-script/pi-agent.sh --bg 'long investigation...'"
echo "    setup-script/pi-agent.sh --pull      # bring the Pi agent's commits back"
echo "    setup-script/pi-agent.sh --push      # send Mac commits to the Pi"
echo ""
fi
echo "  Or directly over ssh:"
echo "    ssh $(whoami)@$(hostname) demiurge-agent --list"
echo "    ssh $(whoami)@$(hostname) demiurge-agent 'why is csound crash-looping?'"
echo "    ssh $(whoami)@$(hostname) demiurge-agent --bg 'long investigation...'"
echo "    ssh $(whoami)@$(hostname) demiurge-agent --attach     # watch a --bg run"
echo ""
if have MD/DEMIURGE.md; then
echo "  Agent orientation doc (all three backends read the same one):"
echo "    MD/DEMIURGE.md   <- the map, invariants, and USER MODIFICATIONS"
echo "    CLAUDE.md / GEMINI.md / AGENTS.md are thin pointers at it"
echo ""
elif have MD/DEMIURGE.public.md; then
echo "  System overview (what an agent or a human reads first):"
echo "    MD/DEMIURGE.public.md"
echo ""
fi
echo "  git identity on this Pi:"
echo "    $(git config --global user.name) <$(git config --global user.email)>"
echo "    override with DEMIURGE_GIT_NAME / DEMIURGE_GIT_EMAIL and re-run phase 11"
echo ""
fi

# ── Phase 12: COD (agent persistence + tank client, DORMANT) ─────────────────
# Downloads public EMG0R/cod, keeps it current via a decoupled updater instance
# (cod-update.timer; never touches the audio engine), installs cod's agent
# persistence + bus client + units. Nothing joins a tank, nothing logs in to
# Tailscale: cod-bus stays disabled and guarded until the COD app writes
# ~/.cod-bus.conf. Re-run safe: never clobbers ~/.cod-bus.conf or ~/.claude-persist.
# Starts/stops/restarts nothing. See docs/cod.md.
if ! skip 12; then
echo "=== Phase 12: COD (dormant) ==="
if have setup-script/cod/install-cod.sh; then
    bash "$SCRIPT_DIR/cod/install-cod.sh" 2>&1 | sed 's/^/  /' || echo "  (COD hook reported a problem; re-run: bash setup-script/cod/install-cod.sh)"
else
    skip_absent "setup-script/cod/install-cod.sh (COD hook)"
fi
echo "=== Phase 12 complete ==="
echo ""
fi
