// ui/lava.h — animated "lava lamp" background for THE WATCH.
//
// Slow, flowing, morphing color blobs (metaballs) in the dark blue/violet
// palette that drift, merge and separate — the classic lava-lamp goo, kept
// deliberately DARK/DIM so UI text on every page stays perfectly readable.
//
// HOW IT STAYS CHEAP (the whole point — this chip overheats/corrupts easily):
//   * The goo is computed on a TINY low-resolution canvas (LAVA_W x LAVA_H,
//     a few thousand pixels) using a cheap metaball field — no per-screen-pixel
//     math. That's the only per-frame CPU cost and it's tiny (FPU on the S3).
//   * The small canvas is drawn UPSCALED (integer zoom) with anti-aliasing, so
//     LVGL's transform does the enlargement and the bilinear blur hides the low
//     resolution AND gives the soft, blurry blob look for free.
//   * It animates at a LOW, capped fps (lava is slow, so this looks smooth) to
//     bound the full-frame SPI flush cost and keep the panel cool.
//
// It lives on ONE shared full-screen background object (see ui.cpp), BEHIND the
// tileview, so the SAME animation shows through every transparent page. The
// wavy starfield (starfield.*) is layered ON TOP of this goo.
#pragma once

#include <lvgl.h>

// Attach the lava canvas as the lowest layer inside `parent` (a full-screen
// background container). Call BEFORE starfield_attach so the stars sit above it.
void lava_attach(lv_obj_t *parent);

// Start the single shared lava animation timer. Call once, after lava_attach.
void lava_start();
