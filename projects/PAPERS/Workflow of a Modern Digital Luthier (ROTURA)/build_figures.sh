#!/bin/bash
# Build the separate figure package ROTURA requires alongside the manuscript:
# TIFF at 300 dpi, numbered to match the captions, with metadata stripped so
# nothing in the attachments can identify the authors during blind review.
#
# Usage:  ./build_figures.sh

set -e
cd "$(dirname "$0")/figures"

rm -rf submission && mkdir submission

# Vector line art -> render at a DPI giving ~1800px wide, tagged 300 dpi so it
# prints at a usable column width.
pdftoppm -tiff -r 630 src/fig1_integrated.pdf    submission/Figure_01
pdftoppm -tiff -r 300 src/fig3_neptr_hw.pdf      submission/Figure_03
pdftoppm -tiff -r 630 src/fig4_neptr_signal.pdf  submission/Figure_04
pdftoppm -tiff -r 505 src/fig7_bouba_presets.pdf submission/Figure_07
pdftoppm -tiff -r 540 src/fig8_bouba_hw.pdf      submission/Figure_08
for f in submission/*-1.tif; do mv "$f" "${f%-1.tif}.tif"; done

# Photographs -> TIFF, converted via sips which drops EXIF in the process.
sips -s format tiff NEPTR1.jpeg     --out submission/Figure_02.tif >/dev/null
sips -s format tiff NEPTRshel2.jpeg --out submission/Figure_05.tif >/dev/null
sips -s format tiff BOUBA1.jpg      --out submission/Figure_06.tif >/dev/null
sips -s format tiff BOUBA2.png      --out submission/Figure_09.tif >/dev/null

for f in submission/*.tif; do
    sips -s dpiWidth 300 -s dpiHeight 300 "$f" >/dev/null
done

echo "=== FIGURE PACKAGE ==="
for f in submission/*.tif; do
    sips -g pixelWidth -g pixelHeight -g dpiWidth "$f" 2>/dev/null | \
    awk -F': ' -v n="$(basename "$f")" '
        /pixelWidth/{w=$2} /pixelHeight/{h=$2} /dpiWidth/{r=$2}
        END{printf "  %-16s %5sx%-5s  prints %5.1f cm%s\n", n,w,h,w/r*2.54,
            (w/r*2.54<10)?"   << undersized":""}'
done

echo
echo "=== blind-review metadata check ==="
# Read the declared TIFF tags rather than grepping the raw bytes: uncompressed
# pixel data throws constant false positives against any keyword list.
found=0
for f in submission/*.tif; do
    hits=$(sips -g all "$f" 2>/dev/null \
        | grep -iE "artist|copyright|owner|author|gps|make:|model:|software" \
        | grep -viE "software: *$" || true)
    if [ -n "$hits" ]; then
        echo "  $(basename "$f"):"
        echo "$hits" | sed 's/^/    /'
        found=1
    fi
done
[ "$found" -eq 0 ] && echo "  no identifying tags — clean for blind review" \
                   || echo "  ^ review these before submitting"
