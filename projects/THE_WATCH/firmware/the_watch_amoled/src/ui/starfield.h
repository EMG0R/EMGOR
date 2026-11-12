// ui/starfield.h — animated "space" starfield background for THE WATCH.
//
// A field of small, twinkling, slowly-drifting stars (with a gentle sine sway)
// drawn BEHIND the clock faces on the dark blue-violet background. Implemented
// as lightweight LVGL objects so only the tiny changed regions are repainted —
// no full-screen redraw, no extra SPI pressure, no tearing.
//
// Usage (see ui.cpp):
//   lv_obj_t *pg = make_page(...);
//   starfield_attach(pg);      // add the field BEFORE the page's own widgets
//   build_<face>(pg);          // widgets render on top of the stars
//   ... after all attaches ...
//   starfield_start();         // one shared animation timer
#pragma once

#include <lvgl.h>

// Attach an animated starfield behind the content of `page`. Call BEFORE adding
// the page's widgets so the stars sit at the bottom of the z-order.
void starfield_attach(lv_obj_t *page);

// Start the single shared animation timer. Call once, after all attaches.
void starfield_start();
