// store_hooks.h -- UI-side hooks into store/ that may not exist yet. WEAK: when store/ provides the symbol it is used,
// otherwise the pointer is null and the UI shows "N/A" instead of acting. (store/ is builder #1's; see report.)
#pragma once
#include <stdint.h>
// Move patch `from` into the EMPTY slot `to` (copy sound + audio sidecar, then retire `from` WITHOUT deleting its files:
// e.g. rename). Must refuse (return false) if `to` exists. Control plane, blocking is ok (user-initiated).
extern bool patchMoveSlot(uint8_t from, uint8_t to) __attribute__((weak));
