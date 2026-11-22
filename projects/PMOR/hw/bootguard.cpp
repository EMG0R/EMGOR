// bootguard.cpp -- see bootguard.h.
#include "bootguard.h"
#include "stm32h7xx.h"
#include "daisy_seed.h"

struct BootFlag { uint32_t magic, inv, state, count; };
#define BG_MAGIC 0x504D4F52u   // 'PMOR'
// .dtcmram_bss is NOLOAD and sits after .bss, outside the range startup zeroes: contents persist across resets.
static BootFlag s_bf __attribute__((section(".dtcmram_bss")));
static bool s_skip = false;

void bootguardStart(void) {
    bool valid = (s_bf.magic == BG_MAGIC) && (s_bf.inv == ~BG_MAGIC) && s_bf.state <= 3;
    if (!valid) { s_bf.magic = BG_MAGIC; s_bf.inv = ~BG_MAGIC; s_bf.state = 1; s_bf.count = 1; s_skip = false; }
    else {
        s_bf.count++;
        if (s_bf.state == 1 || s_bf.state == 3) { s_bf.state = 3; s_skip = true; }   // previous boot never finished storage (or already skipping)
        else { s_bf.state = 1; s_skip = false; }
    }
#ifdef PMOR_NO_STORE_BOOT
    s_skip = true;
#endif
#ifndef PMOR_NO_IWDG
    IWDG1->KR = 0xCCCC;                 // start (LSI ~32 kHz)
    IWDG1->KR = 0x5555;                 // unlock PR/RLR
    IWDG1->PR = 6;                      // /256 -> 125 Hz
    IWDG1->RLR = 2000;                  // 16 s
    uint32_t t = 100000; while (IWDG1->SR && --t) {}
    IWDG1->KR = 0xAAAA;
#endif
}
// BOOT BUDGET: until main() declares the boot finished (bootguardBootDone), kicks are honoured only for BOOT_KICK_BUDGET_MS after the
// first one. Service loops inside the boot (SD reads, usbDelay, OLED blits all call pmor_usb_service -> here) used to keep the dog fed
// forever, so a boot that wedged while still servicing USB never reset and never armed the skip-storage flag. Now it resets ~16 s after
// the budget runs out, the flag still says BOOTING, and the next boot skips storage. (docs: tools/fwcheck.sh rule 5 pins the callers.)
#define BOOT_KICK_BUDGET_MS 12000u
static volatile bool s_boot_done = false; static bool s_t0_set = false; static uint32_t s_t0 = 0;
void bootguardBootDone(void) { s_boot_done = true; }
void bootguardKick(void) {
#ifndef PMOR_NO_IWDG
    if (!s_boot_done) {
        uint32_t now = daisy::System::GetNow();
        if (!s_t0_set) { s_t0_set = true; s_t0 = now; }
        else if ((uint32_t)(now - s_t0) > BOOT_KICK_BUDGET_MS) return;     // the boot is overdue: let the watchdog have it
    }
    IWDG1->KR = 0xAAAA;
#endif
}
bool bootguardSkipStorage(void) { return s_skip; }
void bootguardStorageDone(void) { if (s_bf.state == 1) s_bf.state = 2; }
void bootguardRetry(void) { s_skip = false; if (s_bf.magic == BG_MAGIC && s_bf.inv == ~BG_MAGIC) s_bf.state = 1; }
uint8_t bootguardState(void) { return (uint8_t)s_bf.state; }
uint32_t bootguardBootCount(void) { return s_bf.count; }

// A HardFault (libDaisy's own handler spins forever) must not leave the board dead on the bus: remember it, skip storage next boot, reset.
// Wired with -Wl,--allow-multiple-definition (Makefile; --wrap cannot reach a same-object weak alias): our object precedes libdaisy.a. A fault BEFORE main() (e.g. a static constructor
// touching un-initialised SDRAM) now ends in a reset loop instead of a silent hang, so the Daisy bootloader window keeps recurring.
extern "C" void HardFault_Handler(void) {   // strong override of libDaisy system.cpp's (Makefile: first definition wins)
    if (s_bf.magic == BG_MAGIC && s_bf.inv == ~BG_MAGIC) { s_bf.state = 3; s_bf.count |= 0x80000000u; }
    for (volatile int i = 0; i < 2000000; i++) {}      // a moment for the serial/OLED world to settle
    NVIC_SystemReset();
}
