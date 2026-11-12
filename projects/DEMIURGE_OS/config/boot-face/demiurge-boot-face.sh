#!/bin/bash
# DEMIURGE boot face -- writes a pre-rendered NEPTR X_X face + "DemiurgeOS"
# wordmark directly into /dev/fb0 as early in boot as systemd allows, then
# exits. It does NOT touch the console driver, does NOT take DRM master, and
# does NOT block or own the display -- it is a single raw write, then gone.
# Kernel/systemd boot text keeps scrolling on the same console exactly as it
# always has; it simply scrolls over/through the image over the next several
# seconds, the same way a classic Linux boot logo behaves.
#
# Fails open: any problem here (no /dev/fb0 yet, wrong geometry, asset
# missing) is logged and the script exits 0. Nothing downstream depends on
# this unit, so a failure here never blocks or delays booting into a working
# instrument.
set -u

ASSET=/usr/local/share/demiurge/boot-face.raw
FB=/dev/fb0

log() { echo "demiurge-boot-face: $*"; }

if [ ! -r "$ASSET" ]; then
    log "asset missing at $ASSET -- skipping (cosmetic only)"
    exit 0
fi

# /dev/fb0 is created by udev once the vc4 KMS driver binds. By the time any
# systemd unit can run this is already true on every boot observed on this
# rig, but retry briefly rather than assume it.
tries=0
while [ ! -e "$FB" ] && [ "$tries" -lt 20 ]; do
    sleep 0.1
    tries=$((tries + 1))
done
if [ ! -e "$FB" ]; then
    log "$FB never appeared -- skipping"
    exit 0
fi

# Sanity-check geometry before writing a single byte. This asset was built
# for the confirmed panel mode (480x800x16, RGB565, 960-byte stride). If a
# future card/panel swap changes any of that, writing the raw blob blind
# would just scribble garbage into part of the frame -- harmless to boot, but
# pointless. Skip instead.
BPP_FILE=/sys/class/graphics/fb0/bits_per_pixel
STRIDE_FILE=/sys/class/graphics/fb0/stride
VSIZE_FILE=/sys/class/graphics/fb0/virtual_size
if [ -r "$BPP_FILE" ] && [ -r "$STRIDE_FILE" ] && [ -r "$VSIZE_FILE" ]; then
    bpp="$(cat "$BPP_FILE")"
    stride="$(cat "$STRIDE_FILE")"
    vsize="$(cat "$VSIZE_FILE")"
    if [ "$bpp" != "16" ] || [ "$stride" != "960" ] || [ "$vsize" != "480,800" ]; then
        log "unexpected fb geometry (bpp=$bpp stride=$stride size=$vsize) -- skipping"
        exit 0
    fi
fi

# The asset is 480x490 at 16bpp/960-byte stride == exactly 490 full rows, so
# a single sequential write starting at row 0 is correct with no seek math.
if ! dd if="$ASSET" of="$FB" bs=960 conv=notrunc status=none 2>/tmp/demiurge-boot-face.err; then
    log "dd failed: $(cat /tmp/demiurge-boot-face.err 2>/dev/null)"
    exit 0
fi

log "drew $(wc -c < "$ASSET") bytes to $FB"
exit 0
