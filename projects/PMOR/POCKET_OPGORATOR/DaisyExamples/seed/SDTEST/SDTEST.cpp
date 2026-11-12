// SDTEST — SD ONLY. Driver extracted from the CURRENT PMOR_V2, so this
// exercises exactly the hardened init that is on the board.
// Reports which SD command actually fails, every 2s, over CDC.
#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "ff_gen_drv.h"
#include "../_bringup_dfu.h"
using namespace daisy;
static DaisySeed hw;
extern "C" void pmor_usb_service(void) {}
static inline void tud_task(void) {}

static dsy_gpio ssd_clk, ssd_mosi, ssd_miso, ssd_cs;
static bool     ssd_sdhc = false;
static DSTATUS  ssd_stat = STA_NOINIT;
static bool     ssd_fast = false;
volatile uint8_t g_sd_stage=0;
volatile uint8_t g_sd_r1=0xEE;   // after init, drop the 5µs delays (~80x faster
                                    // for bulk audio I/O; init must stay slow per spec)

static void sclk(int v)  { dsy_gpio_write(&ssd_clk,  v); }
static void smosi(int v) { dsy_gpio_write(&ssd_mosi, v); }
static void scs(int v)   { dsy_gpio_write(&ssd_cs,   v); }
static int  smiso()      { return (int)dsy_gpio_read(&ssd_miso); }

static uint8_t spiXfer(uint8_t b) {
    uint8_t rx = 0;
    if(ssd_fast){
        // fast path: raw GPIO toggling (~10 MHz), no explicit delay — the
        // function-call overhead alone keeps it inside the card's SPI limit
        for (int i = 7; i >= 0; i--) {
            smosi((b >> i) & 1);
            sclk(1);
            if (smiso()) rx |= (1 << i);
            sclk(0);
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

// SD INIT — REVERTED 2026-09-14 to the EXACT code from the working Aug-18
// build (POCKETGORATOR_CODE.cpp), byte for byte. The "hardening" tried on
// 2026-09-13 (250ms settle, 160-clock preamble, 5s ACMD41, 3 retries) did NOT
// fix the ACMD41 timeout, so it was change without benefit. Known-good beats
// clever. Do not re-apply it without evidence it helps.
static DSTATUS ssd_initialize(BYTE) {
    ssd_clk  = {hw.GetPin(26), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    ssd_mosi = {hw.GetPin(27), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    ssd_miso = {hw.GetPin(25), DSY_GPIO_MODE_INPUT,     DSY_GPIO_PULLUP};
    ssd_cs   = {hw.GetPin(28), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    dsy_gpio_init(&ssd_clk);  dsy_gpio_init(&ssd_mosi);
    dsy_gpio_init(&ssd_miso); dsy_gpio_init(&ssd_cs);
    sclk(0); smosi(1); scs(1);
    System::Delay(10);

    // 80 clocks with CS high to put card into SPI idle
    for (int i = 0; i < 10; i++) spiXfer(0xFF);

    // CMD0 — reset into SPI mode
    scs(0);
    uint8_t r = sdCmd(0, 0);
    scs(1); spiXfer(0xFF);
    g_sd_r1 = r;
    if (r != 0x01) { g_sd_stage = 1; return STA_NOINIT; }

    // CMD8 — check voltage / detect SDHC capability
    scs(0);
    r = sdCmd(8, 0x000001AA);
    bool v2 = (r == 0x01);
    if (v2) { for (int i = 0; i < 4; i++) spiXfer(0xFF); } // skip R7
    scs(1); spiXfer(0xFF);

    // ACMD41 — activate init; HCS=1 to negotiate SDHC
    uint32_t hcs = v2 ? 0x40000000u : 0u;
    uint32_t deadline = System::GetNow() + 2000;
    do {
        scs(0); sdCmd(55, 0); scs(1); spiXfer(0xFF);
        scs(0); r = sdCmd(41, hcs); scs(1); spiXfer(0xFF);
        // This negotiation can spin for up to 2 s. Starving tud_task() that
        // long makes the host time the device out and drop it off the bus —
        // the same failure ntInit() and the intro animation had. Runs in
        // thread context (main), so pumping here is safe.
        tud_task();
    } while (r == 0x01 && System::GetNow() < deadline);
    g_sd_r1 = r;
    if (r != 0x00) { g_sd_stage = 2; return STA_NOINIT; }

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
        g_sd_r1 = r;
        if (r != 0x00) { g_sd_stage = 3; return STA_NOINIT; }
    }

    ssd_stat = 0;
    ssd_fast = true;   // init done — switch to fast bulk transfers
    return 0;
}

static DSTATUS ssd_status(BYTE) { return ssd_stat; }

static DRESULT ssd_read(BYTE, BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
        // Multi-second file operations (patch/audio autosave, sample load)
        // run through here sector by sector; without this pump they starve
        // the USB plane whole seconds at a time — measured as a 3.2s hole
        // in the MIDI clock (host BPM display drops out, 2026-08-16).
        pmor_usb_service();
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

static char  sd_path[4] = {};
static FATFS sd_fs;

static int sdTest(char *msg, size_t msgsz)
{
    static bool linked = false;
    if(!linked) {
        if(FATFS_LinkDriver(&SPISD_Driver, sd_path) != 0) {
            snprintf(msg, msgsz, "LinkDriver FAILED"); return 1; }
        linked = true;
    }
    f_mount(nullptr, sd_path, 0);
    ssd_stat = STA_NOINIT; ssd_fast = false;
    g_sd_stage = 0; g_sd_r1 = 0xEE;

    if(ssd_initialize(0) & STA_NOINIT) {
        snprintf(msg, msgsz, "INIT FAIL stage=%d R1=0x%02X%s",
            (int)g_sd_stage,(unsigned)g_sd_r1,
            g_sd_stage==1 ? " CMD0 no reply: no card / no power / broken wire"
          : g_sd_stage==2 ? " ACMD41 timeout: supply sag under init current"
          : g_sd_stage==3 ? " CMD58/OCR failed"
          : " (no stage)");
        return 2;
    }

    FRESULT fr = f_mount(&sd_fs, sd_path, 1);
    if(fr != FR_OK) {
        snprintf(msg,msgsz,"INIT OK (sdhc=%d) but f_mount fr=%d%s",
            ssd_sdhc?1:0,(int)fr,
            fr==13?" NO FILESYSTEM - reformat FAT32":" DATA PATH failure");
        return 3;
    }

    static const char pl[] = "PMOR SD OK";
    char back[sizeof(pl)] = {}; FIL f; UINT n=0;
    if(f_open(&f,"SDTEST.TXT",FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK){
        snprintf(msg,msgsz,"mounted but f_open(write) FAILED"); return 4; }
    fr=f_write(&f,pl,sizeof(pl),&n); f_close(&f);
    if(fr!=FR_OK||n!=sizeof(pl)){ snprintf(msg,msgsz,"f_write FAILED fr=%d",(int)fr); return 4; }
    if(f_open(&f,"SDTEST.TXT",FA_READ)!=FR_OK){ snprintf(msg,msgsz,"f_open(read) FAILED"); return 5; }
    fr=f_read(&f,back,sizeof(back),&n); f_close(&f);
    if(fr!=FR_OK||memcmp(back,pl,sizeof(pl))){ snprintf(msg,msgsz,"read/verify FAILED"); return 6; }

    snprintf(msg,msgsz,"SD FULLY OK  sdhc=%d  wrote+read+verified",
              ssd_sdhc?1:0);
    return 0;
}

int main(void)
{
    hw.Configure(); hw.Init(true);
    bringup_dfu_init(hw);
    char msg[128]={}; int code=sdTest(msg,sizeof(msg));
    uint32_t nextTest=0, nextPrint=0;
    while(1){
        bringup_dfu_poll();
        uint32_t now=System::GetNow();
        if(now>=nextTest){ nextTest=now+2000; code=sdTest(msg,sizeof(msg)); }
        if(now>=nextPrint){ nextPrint=now+1000;
            hw.PrintLine("SDTEST code=%d %s", code, msg);
            hw.SetLed(code==0); }
        System::Delay(1);
    }
}
