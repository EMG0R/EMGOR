// ws2812_dma.h -- register-level SPI2 TX DMA for the WS2812B strip on D29 (PB14, SPI2 with IOSWP). Owner: main session.
// Why: with the FX running, the audio callback preempts a blocking SPI send for longer than the WS2812 reset time (~50 us). The
// strip latches mid-frame and the rest of the stream restarts at pixel 0, so the first ~12-30 px show shuffled data and the rest
// freeze. libDaisy's SpiHandle::DmaTransmit sent one frame and never signalled done. This bypasses it: DMA2 Stream6 (unused by
// libDaisy: it uses DMA2 S0/S1 DAC, S2/S3 SPI, S4 UART) -> DMAMUX1 ch14 -> SPI2 TXDR, SPI2 in simplex transmit with TSIZE = n,
// completion = SPI_SR_EOT, polled from the main loop. No IRQs, so nothing in here can disturb audio.
// SpiHandle::Init still does the pin mux, clocks and the base SPI2 config (master, 8 bit, prescaler, soft NSS); IOSWP is set after.
#pragma once
#include "daisy_seed.h"

static bool g_ws_active = false;
static uint32_t g_ws_err = 0;                                          // transfers that ended with a DMA error flag

static void wsDmaInit() {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; (void)RCC->AHB1ENR;
    RCC->AHB2ENR |= RCC_AHB2ENR_SRAM1EN | RCC_AHB2ENR_SRAM2EN | RCC_AHB2ENR_SRAM3EN; (void)RCC->AHB2ENR;   // D2 SRAM clocks (the buffer lives there)
    DMA2_Stream6->CR = 0; while(DMA2_Stream6->CR & DMA_SxCR_EN) {}
    DMAMUX1_Channel14->CCR = DMA_REQUEST_SPI2_TX;                      // DMA2 stream k <-> DMAMUX1 channel 8+k
    DMA2_Stream6->PAR = (uint32_t)&SPI2->TXDR;
    DMA2_Stream6->CR  = DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_PL_1 | DMA_SxCR_PL_0;   // mem -> periph, byte/byte, very high priority
    DMA2_Stream6->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH_0;             // FIFO on, half threshold
    g_ws_active = false;
}
static void wsClose() {
    SPI2->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_UDRC | SPI_IFCR_MODFC | SPI_IFCR_SUSPC;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CFG1 &= ~SPI_CFG1_TXDMAEN;
    DMA2_Stream6->CR &= ~DMA_SxCR_EN; while(DMA2_Stream6->CR & DMA_SxCR_EN) {}
    if(DMA2->HISR & (DMA_HISR_TEIF6 | DMA_HISR_DMEIF6)) g_ws_err++;
    DMA2->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6;
    g_ws_active = false;
}
// Bench 2026-11-16: EOT never came (dma 0, done 0, fb 1); libDaisy's DmaTransmit, which also waits on EOT, failed the same way.
// So EOT isn't required: once the DMA has handed over every byte (NDTR == 0), the SPI FIFO (<= 16 bytes, ~41 us) has drained
// within 2 ms. Close then. g_ws_dbg snapshots the registers at the first poll after the start and at any abort, for telemetry.
static uint32_t g_ws_nd0_ms = 0, g_ws_eot = 0, g_ws_ndz = 0;
static uint32_t g_ws_dbg[6] = {0};                                     // SPI2 SR, CR1, CFG2, CR2, DMA NDTR, DMA2 HISR
static uint32_t g_ws_dbgabort[6] = {0};
static void wsSnap(uint32_t* d) { d[0] = SPI2->SR; d[1] = SPI2->CR1; d[2] = SPI2->CFG2; d[3] = SPI2->CR2; d[4] = DMA2_Stream6->NDTR; d[5] = DMA2->HISR; }
static bool g_ws_first = false;
// true = frame finished (or nothing in flight). Call from the main loop.
static bool wsDmaPoll() {
    if(!g_ws_active) return true;
    if(g_ws_first) { g_ws_first = false; wsSnap(g_ws_dbg); }
    if(SPI2->SR & SPI_SR_EOT) { g_ws_eot++; wsClose(); return true; }
    const uint32_t now = daisy::System::GetNow();
    if(DMA2_Stream6->NDTR == 0) {
        if(SPI2->SR & SPI_SR_TXC) { g_ws_ndz++; g_ws_nd0_ms = 0; wsClose(); return true; }   // DMA handed every byte over and the shift register is empty: done now
        if(!g_ws_nd0_ms) g_ws_nd0_ms = now ? now : 1;
        else if(now - g_ws_nd0_ms >= 2) { g_ws_ndz++; g_ws_nd0_ms = 0; wsClose(); return true; }
    }
    return false;
}
static void wsDmaAbort() { if(g_ws_active) { wsSnap(g_ws_dbgabort); g_ws_nd0_ms = 0; wsClose(); } }
// Polled (blocking) send without HAL. HAL_SPI_Transmit waits for EOT, which never sets in this SPI2/IOSWP setup (measured
// 2026-11-17: every frame took ~20 ms = 5.2 ms of data + the 14.7 ms HAL timeout, stalling the main loop 60 % of the time).
// Here: push bytes on TXP, then wait for TXC (max 200 us), then close. Returns the longest gap between two byte writes in
// cycles: a gap longer than the FIFO time (16 bytes = ~41 us at 3.125 MHz) means an IRQ let the FIFO run dry, which can
// latch the WS2812s mid-frame (a real stretch).
static uint32_t wsPolledSend(const uint8_t* buf, uint16_t n) {
    if(g_ws_active) wsClose();
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_UDRC | SPI_IFCR_MODFC | SPI_IFCR_SUSPC;
    SPI2->CFG1 &= ~SPI_CFG1_TXDMAEN;
    SPI2->CFG2 = (SPI2->CFG2 & ~SPI_CFG2_COMM) | SPI_CFG2_COMM_0 | SPI_CFG2_IOSWP;
    SPI2->CR2 = n;
    SPI2->CR1 |= SPI_CR1_SPE; SPI2->CR1 |= SPI_CR1_CSTART;
    volatile uint8_t* tx = (volatile uint8_t*)&SPI2->TXDR;
    uint32_t last = DWT->CYCCNT, maxgap = 0;
    for(uint16_t i = 0; i < n; i++) {
        uint32_t spin = 0; while(!(SPI2->SR & SPI_SR_TXP)) { if(++spin > 2000000u) { wsClose(); return 0xFFFFFFFFu; } }
        const uint32_t now = DWT->CYCCNT; if(i && now - last > maxgap) maxgap = now - last; last = now;
        *tx = buf[i];
    }
    const uint32_t t0 = DWT->CYCCNT; while(!(SPI2->SR & (SPI_SR_TXC | SPI_SR_EOT)) && DWT->CYCCNT - t0 < 96000u) {}   // <= 200 us at 480 MHz
    wsClose();
    return maxgap;
}
static bool wsDmaStart(const uint8_t* buf, uint16_t n) {
    if(g_ws_active) return false;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_UDRC | SPI_IFCR_MODFC | SPI_IFCR_SUSPC;
    SPI2->CFG2 = (SPI2->CFG2 & ~SPI_CFG2_COMM) | SPI_CFG2_COMM_0 | SPI_CFG2_IOSWP;   // simplex transmitter, keep the MISO/MOSI swap (SPE = 0 here)
    SPI2->CR2 = n;                                                      // TSIZE: EOT fires after n bytes have left the shift register
    DMA2->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6;
    DMA2_Stream6->M0AR = (uint32_t)buf; DMA2_Stream6->NDTR = n;
    SPI2->CFG1 |= SPI_CFG1_TXDMAEN;                                     // RM0433: TXDMAEN, then DMA enable, then SPE, then CSTART
    DMA2_Stream6->CR |= DMA_SxCR_EN;
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_CSTART;
    g_ws_active = true; g_ws_first = true; g_ws_nd0_ms = 0;
    return true;
}
