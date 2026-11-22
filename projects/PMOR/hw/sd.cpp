// sd.cpp — SD card: bit-bang SPI + FatFS diskio shim + mount.
// VERBATIM from PMOR_V2.cpp L3380–3580 (driver) and L4149–4161 (mount).
// DO NOT change DelayUs(5), the 250 ms settle, the 160-clock preamble, the
// 5 s ACMD41 deadline or the 3 retries — all measured on this wiring.
#include "hw.h"
#include "hw_priv.h"
#include "../config.h"
#include "daisy_seed.h"
#include "fatfs.h"
#include "ff_gen_drv.h"
extern "C" {
#include "util/hal_map.h"
}
extern "C" {
#include "tusb.h"
}
using namespace daisy;

// ============================================================
// SPI SD card — bit-bang on D26/D27/D25/D28
// D26(PD11)=CLK  D27(PG9)=MOSI  D25(PA0)=MISO  D28(PA2)=CS
// ============================================================
static dsy_gpio ssd_clk, ssd_mosi, ssd_miso, ssd_cs;
static bool     ssd_sdhc = false;
static DSTATUS  ssd_stat = STA_NOINIT;
static bool     ssd_fast = false;   // after init, drop the 5µs delays (~80x faster
                                    // for bulk audio I/O; init must stay slow per spec)

static void sclk(int v)  { dsy_gpio_write(&ssd_clk,  v); }
static void smosi(int v) { dsy_gpio_write(&ssd_mosi, v); }
static void scs(int v)   { dsy_gpio_write(&ssd_cs,   v); }
static int  smiso()      { return (int)dsy_gpio_read(&ssd_miso); }

static GPIO_TypeDef *g_sd_clk_port, *g_sd_mosi_port, *g_sd_miso_port; static uint16_t g_sd_clk_pin, g_sd_mosi_pin, g_sd_miso_pin;
static uint8_t spiXfer(uint8_t b) {
    uint8_t rx = 0;
    if(ssd_fast){
        // fast path: direct BSRR writes (was dsy_gpio_write -> HAL_GPIO_WritePin per edge: ~2.5 ms per 512 B sector).
        // Same pins, same edge order (data set, clock high, sample, clock low); the card limit is 25 MHz and this runs far below it.
        for (int i = 7; i >= 0; i--) {
            g_sd_mosi_port->BSRR = ((b >> i) & 1) ? (uint32_t)g_sd_mosi_pin : ((uint32_t)g_sd_mosi_pin << 16);
            g_sd_clk_port->BSRR = g_sd_clk_pin;
            if (g_sd_miso_port->IDR & g_sd_miso_pin) rx |= (1 << i);
            g_sd_clk_port->BSRR = (uint32_t)g_sd_clk_pin << 16;
        }
        return rx;
    }
    // 5us half-period. REVERTED from a 2us "optimisation" 2026-09-13: the
    // faster init clock made the card fail to mount (SD:-- on the dash).
    // This value is what the working Aug-18 build used. DO NOT speed it up
    // again -- the spec headroom is theoretical, this wiring is not.
    for (int i = 7; i >= 0; i--) {
        smosi((b >> i) & 1);
        System::DelayUs(5);
        sclk(1);
        System::DelayUs(5);
        if (smiso()) rx |= (1 << i);
        sclk(0);
    }
    return rx;
}

static uint8_t sdCmd(uint8_t cmd, uint32_t arg) {
    uint8_t crc = (cmd == 0) ? 0x95 : (cmd == 8) ? 0x87 : 0xFF;
    spiXfer(0x40 | cmd);
    spiXfer((uint8_t)(arg >> 24));
    spiXfer((uint8_t)(arg >> 16));
    spiXfer((uint8_t)(arg >>  8));
    spiXfer((uint8_t) arg);
    spiXfer(crc);
    uint8_t r = 0xFF;
    for (int i = 0; i < 8 && (r & 0x80); i++) r = spiXfer(0xFF);
    return r;
}

static DSTATUS ssd_initialize_once(BYTE);

// Retry the whole bring-up. A card that is slow to come up often fails the
// first attempt and succeeds on the second, once its internal regulator has
// charged. Cheap insurance -- this runs once at boot.
static DSTATUS ssd_initialize(BYTE drv) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (ssd_initialize_once(drv) == 0) return 0;
        ssd_fast = false;
        System::Delay(150);
        tud_task();
    }
    return STA_NOINIT;
}

static DSTATUS ssd_initialize_once(BYTE) {
    ssd_clk  = {hw.GetPin(PIN_SD_CLK),  DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};   // D26
    ssd_mosi = {hw.GetPin(PIN_SD_MOSI), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};   // D27
    ssd_miso = {hw.GetPin(PIN_SD_MISO), DSY_GPIO_MODE_INPUT,     DSY_GPIO_PULLUP};   // D25
    ssd_cs   = {hw.GetPin(PIN_SD_CS),   DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};   // D28
    dsy_gpio_init(&ssd_clk);  dsy_gpio_init(&ssd_mosi);
    dsy_gpio_init(&ssd_miso); dsy_gpio_init(&ssd_cs);
    g_sd_clk_port = dsy_hal_map_get_port(&ssd_clk.pin); g_sd_clk_pin = dsy_hal_map_get_pin(&ssd_clk.pin);
    g_sd_mosi_port = dsy_hal_map_get_port(&ssd_mosi.pin); g_sd_mosi_pin = dsy_hal_map_get_pin(&ssd_mosi.pin);
    g_sd_miso_port = dsy_hal_map_get_port(&ssd_miso.pin); g_sd_miso_pin = dsy_hal_map_get_pin(&ssd_miso.pin);
    sclk(0); smosi(1); scs(1);
    // v2 2026-09-13: 10ms -> 250ms. This is a PASSIVE 3.3V breakout (no
    // regulator, no level shifter, just 4x 10k pull-ups and two small caps),
    // so the card's supply has almost no bulk behind it. A card that answers
    // CMD0 but times out at ACMD41 is one that has not finished its internal
    // power-up: CMD0 draws almost nothing, ACMD41 starts the real current
    // burst. Give the rail time to settle before we ask for that.
    System::Delay(250);

    // Power-up clock preamble. Spec minimum is 74 clocks; send 160 for
    // margin (10 -> 20 bytes) since these leads are long and unterminated.
    for (int i = 0; i < 20; i++) spiXfer(0xFF);

    // CMD0 — reset into SPI mode
    scs(0);
    uint8_t r = sdCmd(0, 0);
    scs(1); spiXfer(0xFF);
    if (r != 0x01) return STA_NOINIT;

    // CMD8 — check voltage / detect SDHC capability
    scs(0);
    r = sdCmd(8, 0x000001AA);
    bool v2 = (r == 0x01);
    if (v2) { for (int i = 0; i < 4; i++) spiXfer(0xFF); } // skip R7
    scs(1); spiXfer(0xFF);

    // ACMD41 — activate init; HCS=1 to negotiate SDHC
    // Deadline 2s -> 5s. The SD spec allows up to 1s, but a marginal supply
    // stretches the card's internal power-up well past that, and giving up
    // early is indistinguishable from a dead card.
    uint32_t hcs = v2 ? 0x40000000u : 0u;
    uint32_t deadline = System::GetNow() + 5000;
    do {
        scs(0); sdCmd(55, 0); scs(1); spiXfer(0xFF);
        scs(0); r = sdCmd(41, hcs); scs(1); spiXfer(0xFF);
        // This negotiation can spin for up to 2 s. Starving tud_task() that
        // long makes the host time the device out and drop it off the bus —
        // the same failure ntInit() and the intro animation had. Runs in
        // thread context (main), so pumping here is safe.
        tud_task();
    } while (r == 0x01 && System::GetNow() < deadline);
    if (r != 0x00) return STA_NOINIT;

    // CMD58 — read OCR, determine SDHC (CCS bit)
    ssd_sdhc = false;
    if (v2) {
        scs(0);
        r = sdCmd(58, 0);
        if (r == 0x00) {
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = spiXfer(0xFF);
            ssd_sdhc = (ocr[0] & 0x40) != 0;
        }
        scs(1); spiXfer(0xFF);
    }

    // CMD16 — force 512-byte blocks for SDSC cards
    if (!ssd_sdhc) {
        scs(0); r = sdCmd(16, 512); scs(1); spiXfer(0xFF);
        if (r != 0x00) return STA_NOINIT;
    }

    ssd_stat = 0;
    ssd_fast = true;   // init done — switch to fast bulk transfers
    return 0;
}

static DSTATUS ssd_status(BYTE) { return ssd_stat; }

volatile uint32_t g_sd_rd_calls = 0, g_sd_rd_sector0 = 0;
volatile uint32_t g_sd_rd_sectors = 0, g_sd_rd_spi_us = 0, g_sd_rd_svc_us = 0;   // STATUS sd= : where a card read spends its time
static inline uint32_t tickUs(uint32_t d) { return (uint32_t)(((uint64_t)d * 1000000ull) / System::GetTickFreq()); }
static DRESULT ssd_read(BYTE, BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    g_sd_rd_calls++; if (count == 1) g_sd_rd_sector0++;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
        // Multi-second file operations (patch/audio autosave, sample load)
        // run through here sector by sector; without this pump they starve
        // the USB plane whole seconds at a time — measured as a 3.2s hole
        // in the MIDI clock (host BPM display drops out, 2026-08-16).
        uint32_t t0 = System::GetTick();
        pmor_usb_service();
        uint32_t t1 = System::GetTick();
        scs(0);
        if (sdCmd(17, sector) != 0x00) { scs(1); return RES_ERROR; }
        uint32_t t = System::GetNow() + 200;
        uint8_t tok;
        do { tok = spiXfer(0xFF); } while (tok == 0xFF && System::GetNow() < t);
        if (tok != 0xFE) { scs(1); return RES_ERROR; }
        for (int i = 0; i < 512; i++) buf[i] = spiXfer(0xFF);
        spiXfer(0xFF); spiXfer(0xFF); // CRC
        scs(1); spiXfer(0xFF);
        if (!ssd_sdhc) sector += 512; else sector++;
        g_sd_rd_sectors++; g_sd_rd_svc_us += tickUs(t1 - t0); g_sd_rd_spi_us += tickUs(System::GetTick() - t1);
    }
    return RES_OK;
}

static DRESULT ssd_write(BYTE, const BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
        pmor_usb_service();   // see ssd_read — keeps MIDI clock + USB audio
                              // alive through multi-second autosaves
        scs(0);
        if (sdCmd(24, sector) != 0x00) { scs(1); return RES_ERROR; }
        spiXfer(0xFF);
        spiXfer(0xFE); // data token
        for (int i = 0; i < 512; i++) spiXfer(buf[i]);
        spiXfer(0xFF); spiXfer(0xFF); // dummy CRC
        if ((spiXfer(0xFF) & 0x1F) != 0x05) { scs(1); return RES_ERROR; }
        uint32_t t = System::GetNow() + 500;
        while (spiXfer(0xFF) == 0x00 && System::GetNow() < t) {}
        scs(1); spiXfer(0xFF);
        if (!ssd_sdhc) sector += 512; else sector++;
    }
    return RES_OK;
}

static DRESULT ssd_ioctl(BYTE, BYTE cmd, void* buf) {
    if (cmd == CTRL_SYNC)       return RES_OK;
    if (cmd == GET_SECTOR_SIZE) { *(WORD*)buf  = 512; return RES_OK; }
    if (cmd == GET_BLOCK_SIZE)  { *(DWORD*)buf = 1;   return RES_OK; }
    return RES_PARERR;
}

static const Diskio_drvTypeDef SPISD_Driver = {
    ssd_initialize, ssd_status, ssd_read, ssd_write, ssd_ioctl
};

// ============================================================
// Mount (PMOR_V2.cpp L4149–4161)
// ============================================================
char         sd_path[4] = {};       // FatFS logical drive path (hw.h)
static FATFS sdFS;
static bool  g_sd_ok = false;

bool sdMount() {
    g_sd_ok = (FATFS_LinkDriver(&SPISD_Driver, sd_path) == 0) &&
              (f_mount(&sdFS, sd_path, 1) == FR_OK);
    return g_sd_ok;
}

bool sdOk() { return g_sd_ok; }

// Bulk-transfer speed. Reads are proven at the fast (no-delay) clock, but a
// write that fails read-back verification (store/files.cpp) drops the card
// to the slow 5 us clock for the rest of the session — never the other way.
void sdSetFast(bool fast) { ssd_fast = fast; }
bool sdIsFast()           { return ssd_fast; }
