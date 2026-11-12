// bootguard.h -- a storage hang must never brick USB again.
//  * IWDG (independent watchdog, ~16 s): started at the top of main(); refreshed by pmor_usb_service() and every main-loop pass,
//    so a hang anywhere resets the board instead of leaving it dead on the bus. (A system reset, e.g. the CDC "dfu" path, clears it.)
//  * Persistent boot flag in .dtcmram_bss (DTCM, NOT zeroed by startup, survives a watchdog/software reset, random after a power cycle):
//      state BOOTING at start; set to OK when the background storage tasks reach "done". A boot that finds BOOTING means the previous boot
//      never finished its storage tasks -> this boot SKIPS the storage boot tasks (SD migration / PRESETS export+install / pool scan) and
//      stays in SKIP until a power cycle. USB/CDC/protocol/audio/sequencer do not depend on them.
//  * Compile-time switches (build bisecting): -DPMOR_NO_STORE_BOOT (no migration, no presets, no pool), -DPMOR_NO_IWDG.
#pragma once
#include <stdint.h>
void bootguardStart(void);              // first thing in main()
void bootguardBootDone(void);           // main(): hwBoot/navInit/studioInit returned -> the boot kick budget no longer applies
void bootguardKick(void);               // refresh the watchdog (cheap; call from any service loop)
bool bootguardSkipStorage(void);        // true: do not run storage boot tasks this boot
void bootguardStorageDone(void);        // storage tasks reached done: the next boot may run them again
void bootguardRetry(void);              // clear a sticky skip: storage tasks may run again (state back to BOOTING until they finish)
uint8_t bootguardState(void);           // 0 unknown 1 booting 2 ok 3 skip (for STATUS)
uint32_t bootguardBootCount(void);
