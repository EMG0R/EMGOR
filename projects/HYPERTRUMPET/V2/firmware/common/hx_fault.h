// hx_fault.h -- HardFault handler for the HYPERTRUMPET V2 firmware. NEW 2026-11-15. Include ONCE (defines HardFault_Handler, overriding libDaisy's weak one).
// On a fault: stacked registers + fault status go to backup SRAM (0x38800000, survives NVIC_SystemReset), the user LED (PC7) blinks fast for ~4 s,
// then the chip resets. The next boot calls hxFaultBoot() and reports the record in every hello ("fault":{...}). Resolve the PC with:
//   arm-none-eabi-addr2line -f -C -e build/<target>.elf <pc>
#pragma once
#include <stdint.h>
#include "daisy_seed.h"
#include "hx_seed.h"

#define HX_FAULT_MAGIC 0xFA17C0DEu
inline volatile uint32_t g_hxFaultCtx = 0;       // app writes "what was running" (bench: case index); stored in the record
struct HxFaultRec { uint32_t n, pc, lr, sp, xpsr, cfsr, hfsr, bfar, mmfar, r0, r1, r2, r3, r12, ctx; };
static HxFaultRec g_hxFault; static bool g_hxFaultValid = false;
#define HX_BKP ((volatile uint32_t*)0x38800000)

static inline void hxBkpEnable() { RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN; (void)RCC->AHB4ENR; PWR->CR1 |= PWR_CR1_DBP; }
static inline void hxFaultBoot() {               // call early in main() (after hw.Init)
    hxBkpEnable();
    volatile uint32_t* b = HX_BKP;
    if (b[0] == HX_FAULT_MAGIC) {
        uint32_t* d = (uint32_t*)&g_hxFault; for (unsigned i = 0; i < sizeof(HxFaultRec) / 4; i++) d[i] = b[1 + i];
        g_hxFaultValid = true; b[0] = 0;       // record consumed; the fault counter word after it is kept
    }
}
static inline void hxFaultJson(HxOut& o) {
    if (!g_hxFaultValid) return;
    const HxFaultRec& f = g_hxFault;
    o.s(",\"fault\":{\"n\":"); o.u(f.n); o.s(",\"pc\":"); o.hex(f.pc); o.s(",\"lr\":"); o.hex(f.lr); o.s(",\"sp\":"); o.hex(f.sp); o.s(",\"xpsr\":"); o.hex(f.xpsr);
    o.s(",\"cfsr\":"); o.hex(f.cfsr); o.s(",\"hfsr\":"); o.hex(f.hfsr); o.s(",\"bfar\":"); o.hex(f.bfar); o.s(",\"mmfar\":"); o.hex(f.mmfar);
    o.s(",\"r0\":"); o.hex(f.r0); o.s(",\"r1\":"); o.hex(f.r1); o.s(",\"r2\":"); o.hex(f.r2); o.s(",\"r3\":"); o.hex(f.r3); o.s(",\"r12\":"); o.hex(f.r12);
    o.s(",\"ctx\":"); o.u(f.ctx); o.c('}');
}
extern "C" void hxFaultC(uint32_t* sp) {
    hxBkpEnable();
    volatile uint32_t* b = HX_BKP;
    const unsigned ci = 1 + sizeof(HxFaultRec) / 4; uint32_t n = b[ci]; n = n < 1000u ? n + 1 : 1;   // persistent counter (garbage after a power loss restarts at 1)
    HxFaultRec r = { n, sp[6], sp[5], (uint32_t)sp, sp[7], SCB->CFSR, SCB->HFSR, SCB->BFAR, SCB->MMFAR, sp[0], sp[1], sp[2], sp[3], sp[4], g_hxFaultCtx };
    uint32_t* d = (uint32_t*)&r; for (unsigned i = 0; i < sizeof r / 4; i++) b[1 + i] = d[i];
    b[ci] = n; b[0] = HX_FAULT_MAGIC;
    for (int k = 0; k < 40; k++) {                // ~4 s of fast blinking on the user LED (PC7), busy-wait: nothing else is trustworthy now
        GPIOC->ODR ^= (1u << 7);
        for (volatile uint32_t i = 0; i < 3000000u; i++) {}
    }
    NVIC_SystemReset();
}
extern "C" __attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n b hxFaultC\n");
}
