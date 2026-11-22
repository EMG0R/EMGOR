// oled.cpp — MyOled instance + oledInit() (VERBATIM config from PMOR_V2.cpp
// L104, L3927, L4041–4099) + the drawing primitives declared in hw.h, implemented
// over libDaisy's OledDisplay<> graphics wrapper on top of pmor's bit-bang driver.
#include "hw.h"
#include "hw_priv.h"
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
// PMOR's own bit-banged SSD1309 driver — see oled_pmor.h for why. The SPI1
// hardware peripheral kills USB the moment it transmits; bit-banging the same
// pins does not. This keeps libDaisy's OledDisplay<> graphics wrapper, so every
// existing Fill/SetCursor/WriteString/DrawRect/Update call is unchanged.
#include "oled_pmor.h"
using namespace daisy;
using MyOled = OledDisplay<pmor::BitBangSSD130x128x64>;

static MyOled display;

void oledInit() {
    // Install the OLED per-page yield BEFORE anything can draw: Update()
    // blits the framebuffer as eight back-to-back blocking SPI transfers,
    // and without yielding between them the host never finishes enumerating.
    // DO NOT re-add an OLED page-yield that calls tud_task(). It was added
    // to fight what looked like task starvation, but the real defect is a
    // FIFO race between the priority-0 audio ISR and the priority-6 OTG ISR
    // -- and calling tud_task() from inside display.Update() just adds a
    // THIRD concurrent toucher of ep_in_ff from thread context, widening the
    // race instead of closing it.
    dsy_oled_page_yield_hook = nullptr;

    MyOled::Config oc;
    // VERIFIED 2026-08-15 against libDaisy daisy_seed.h:204-208 —
    //   D7=PG10  D8=PG11  D9=PB4  D10=PB5  D11=PB8
    // and against the SSD130x 4-wire SPI transport this replaces, whose
    // defaults are sclk=PG11, mosi=PB5, nss=PG10. The previous values here
    // (D9/D11/D8) resolved to PB4/PB8/PG11: the MISO pin as clock, data on a
    // pin the panel isn't wired to, and the real clock used as chip-select.
    // The panel could not have lit in ANY electrical configuration.
    oc.driver_config.sclk  = seed::D8;    // PG11
    oc.driver_config.mosi  = seed::D10;   // PB5  (shared with NeoTrellis SCL)
    oc.driver_config.cs    = seed::D7;    // PG10 (was SPI1 NSS/HARD_OUTPUT)
    oc.driver_config.dc    = seed::D3;
    oc.driver_config.reset = seed::D4;
    // Bit-bang half-period padding. 0 toggles as fast as a 480MHz core can,
    // which overruns the SSD1309 (its SPI ceiling is a few MHz) and leaves the
    // panel dark. A blank screen with correct wiring is nearly always "too
    // fast"; garbage on screen would be the opposite problem.
    // Push-pull during transactions means edges are fast again, so the clock
    // no longer has to wait on an RC rise. 8 NOPs ~= 16ns per half-period is
    // still far inside the SSD1309's tolerance and keeps a full frame ~1ms
    // instead of the 65ms that made the UI unusable.
    // 24 NOPs/half-period ≈ 2-3MHz — comfortably inside the SSD1309's spec
    // (8 ran right at the ~10MHz ceiling and produced sporadic miswritten
    // bytes on screen, 2026-08-16). Full frame is still ~2-3ms.
    // v2 2026-09-13: 24 -> 48. At 24 this panel corrupted command bytes at
    // runtime (mirrored, then split-and-swapped halves after minutes) --
    // the same "miswritten bytes" failure the 8->24 change fixed before,
    // just at a lower threshold on these flying leads. 48 halves the clock
    // again. Ladder if it recurs: 48 -> 64 -> 96.
    oc.driver_config.nop_delay = 48;
    display.Init(oc);

    // ── DISABLE SPI1's INTERRUPT ─────────────────────────────────────────
    // libDaisy's SpiHandle::Init() unconditionally does
    //     HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
    //     HAL_NVIC_EnableIRQ(SPI1_IRQn);
    // (per/spi.cpp:999) -- preempt priority ZERO, the highest on the chip.
    // The OLED transport only ever uses HAL_SPI_Transmit (polling), so that
    // interrupt serves no purpose here, but once enabled it outranks the OTG
    // ISR and can preempt it mid-TXFIFO-copy, which wedges the DWC2 packet
    // state machine permanently (device off the bus until a power cycle).
    //
    // This is why: raising USB from priority 6 to 1 changed nothing (0 still
    // wins); tud_task() yields changed nothing (thread mode cannot outrank an
    // ISR); removing the audio-ISR FIFO write changed nothing (wrong writer);
    // and soft SPI "fixed" USB only because it never touches the peripheral
    // at all. The single common factor across every failing build was SPI1
    // actually transmitting.
    NVIC_DisableIRQ(SPI1_IRQn);
    // POWER TEST: the OLED refresh kills USB *instantly* (reproduced live over
    // CDC: "TOled 1" -> device off the bus mid-reply). Instant, not gradual,
    // and unaffected by tud_task() yields — that points at current draw, not
    // timing, matching PocketPassthru-FLASHING.md §6 (the panel's onboard 12V
    // boost pushing total draw past what the port supplies).
    // Drive it at minimum contrast to cut that draw and see if USB survives.
    // (contrast set inside the driver init — SendCommand is private here)
    display.Fill(false);
    g_t_oled = System::GetNow();
    display.Update();   // dark during hardware init — intro animation follows
}

void oledFlush() { display.Update(); }
// Stepped push (main loop): oledFlushBegin() after the frame is drawn, then oledFlushStep() once per pass until it returns false.
void oledFlushBegin()   { pmor::BitBangSSD130x128x64::inst->PrepareFrame(); }
bool oledFlushStep()    { return pmor::BitBangSSD130x128x64::inst->StepChunk(); }
bool oledFlushPending() { return pmor::BitBangSSD130x128x64::inst->Pending(); }

// ---------------------------------------------------------------- primitives
static inline bool inb(int x, int y) { return x >= 0 && x < 128 && y >= 0 && y < 64; }

void oledClear() { display.Fill(false); }

void oledPixel(int x, int y, bool on) {
    if(inb(x, y)) display.DrawPixel((uint_fast8_t)x, (uint_fast8_t)y, on);
}

// Bresenham with per-pixel clipping (libDaisy's DrawLine takes uint8 and would
// wrap negative coordinates).
void oledLine(int x0, int y0, int x1, int y1, bool on) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;   // dy negative
    int err = dx + dy;
    for(;;) {
        oledPixel(x0, y0, on);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

void oledRect(int x0, int y0, int x1, int y1, bool on, bool fill) {
    if(x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if(y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if(fill) {
        for(int y = y0; y <= y1; y++)
            for(int x = x0; x <= x1; x++) oledPixel(x, y, on);
    } else {
        oledLine(x0, y0, x1, y0, on);
        oledLine(x0, y1, x1, y1, on);
        oledLine(x0, y0, x0, y1, on);
        oledLine(x1, y0, x1, y1, on);
    }
}

// Rounded rectangle: 4 straight edges + 4 quarter-circle corners of radius r
// (midpoint circle, one octant pair per corner). fill = solid, using the same
// corner geometry so the filled shape matches the outline exactly.
void oledRoundRect(int x0, int y0, int x1, int y1, int r, bool on, bool fill) {
    if(x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if(y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if(r < 0) r = 0;
    if(2 * r > w - 1) r = (w - 1) / 2;
    if(2 * r > h - 1) r = (h - 1) / 2;
    if(r == 0) { oledRect(x0, y0, x1, y1, on, fill); return; }

    // corner centres
    int cxl = x0 + r, cxr = x1 - r, cyt = y0 + r, cyb = y1 - r;

    if(fill) {
        // middle band (full width) + top/bottom bands via the circle spans
        oledRect(x0, cyt, x1, cyb, on, true);
        int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
        while(x < y) {
            if(f >= 0) { y--; ddy += 2; f += ddy; }
            x++; ddx += 2; f += ddx;
            // horizontal spans at vertical offset y (near) and x (far)
            oledLine(cxl - x, cyt - y, cxr + x, cyt - y, on);
            oledLine(cxl - x, cyb + y, cxr + x, cyb + y, on);
            oledLine(cxl - y, cyt - x, cxr + y, cyt - x, on);
            oledLine(cxl - y, cyb + x, cxr + y, cyb + x, on);
        }
        return;
    }

    // 4 straight edges (between the corner arcs)
    oledLine(cxl, y0, cxr, y0, on);
    oledLine(cxl, y1, cxr, y1, on);
    oledLine(x0, cyt, x0, cyb, on);
    oledLine(x1, cyt, x1, cyb, on);

    // 4 corner arcs
    int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    oledPixel(cxl - r, cyt, on); oledPixel(cxr + r, cyt, on);
    oledPixel(cxl, cyt - r, on); oledPixel(cxr, cyt - r, on);
    oledPixel(cxl - r, cyb, on); oledPixel(cxr + r, cyb, on);
    oledPixel(cxl, cyb + r, on); oledPixel(cxr, cyb + r, on);
    while(x < y) {
        if(f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        oledPixel(cxr + x, cyt - y, on); oledPixel(cxr + y, cyt - x, on);   // top-right
        oledPixel(cxl - x, cyt - y, on); oledPixel(cxl - y, cyt - x, on);   // top-left
        oledPixel(cxr + x, cyb + y, on); oledPixel(cxr + y, cyb + x, on);   // bottom-right
        oledPixel(cxl - x, cyb + y, on); oledPixel(cxl - y, cyb + x, on);   // bottom-left
    }
}

void oledText(int x, int y, const char* s, bool on) {
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    display.SetCursor((uint16_t)x, (uint16_t)y);
    display.WriteString(s, Font_6x8, on);
}

void oledTextBig(int x, int y, const char* s, bool on) {
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    display.SetCursor((uint16_t)x, (uint16_t)y);
    display.WriteString(s, Font_7x10, on);
}

// ============================================================
// CDC test hooks called from usb/clock_engine.c (OB1/OBF/OBB/OBW/OBD/OBC/OBH/
// OBS, OPH/OPL, OC <n>). VERBATIM from PMOR_V2.cpp L3730–3816. They need the
// display instance, so they live here.
// ============================================================
extern "C" {
#include "tusb.h"
#include "usb/usb_audio_stream.h"
}
// Raw 2-byte command straight to the panel, bypassing OledDisplay (whose
// driver_ member is private). Same pins, same timing as the bit-bang driver.
static void oledCmdRaw(uint8_t c1, uint8_t c2) {
    static GPIO rsclk, rmosi, rcs, rdc;
    static bool rinit = false;
    if(!rinit) {
        rsclk.Init(seed::D8,  GPIO::Mode::OUTPUT);
        rmosi.Init(seed::D10, GPIO::Mode::OUTPUT);
        rcs.Init(seed::D7,   GPIO::Mode::OUTPUT);
        rdc.Init(seed::D3,   GPIO::Mode::OUTPUT);
        rinit = true;
    }
    rdc.Write(false); rcs.Write(false);
    uint8_t bytes[2] = {c1, c2};
    for(int b = 0; b < 2; b++)
        for(int bit = 7; bit >= 0; bit--) {
            rmosi.Write((bytes[b] >> bit) & 1);
            for(volatile int k = 0; k < 8; k++) {}
            rsclk.Write(true);
            for(volatile int k = 0; k < 8; k++) {}
            rsclk.Write(false);
        }
    rcs.Write(true); rmosi.Write(true); rsclk.Write(false);
}

extern "C" void pmor_oled_test(int what) {
    if(what == 1) {                       // one dirty page (~131 bytes bit-banged)
        display.DrawPixel(0, 0, true);
        display.DrawPixel(0, 0, false);   // net no visual change, marks page 0 dirty
        display.Update();
    } else if(what == 2) {                // full white then full black (LEGACY OBF)
        display.Fill(true);  display.Update();
        display.Fill(false); display.Update();
    } else if(what == 3) {                // OBB: two full-frame blits, ALL BLACK
        // Same bus traffic as OBF but near-zero panel current (OLED pixel
        // current ~ lit pixels). Splits "blit traffic kills" from "panel
        // current kills".
        display.Fill(false); display.Update();   // Fill() marks every page dirty,
        display.Fill(false); display.Update();   // so each Update blits all 8 pages
    } else if(what == 4) {                // OBW: one full-frame blit, ALL WHITE
        display.Fill(true);  display.Update();
    } else if(what == 5) {                // restore dark screen
        display.Fill(false); display.Update();
    } else if(what == 6) {                // OBC: 50% checkerboard, full frame
        for(int y = 0; y < 64; y++)
            for(int x = 0; x < 128; x++)
                display.DrawPixel(x, y, ((x ^ y) & 1) != 0);
        display.Update();
    } else if(what == 7) {                // OBH: top half white
        display.Fill(false);
        for(int y = 0; y < 32; y++)
            for(int x = 0; x < 128; x++)
                display.DrawPixel(x, y, true);
        display.Update();
    } else if(what == 8) {                // OBS: full white, one page per 50ms
        for(int y = 0; y < 64; y += 8) {
            for(int yy = y; yy < y + 8; yy++)
                for(int x = 0; x < 128; x++)
                    display.DrawPixel(x, yy, true);
            display.Update();             // only the newly-dirty page blits
            uint32_t t0 = System::GetNow();
            while(System::GetNow() - t0 < 50) { tud_task(); usb_audio_stream_task(); }
        }
    }
}
// PB5-level test, panel deselected (CS high): pixels cannot change, so this
// isolates the electrical effect of the MOSI/PB5 line level from any panel
// behaviour. White blits (MOSI high-heavy) kill USB, black blits (low-heavy)
// don't, at ANY contrast — so test the wire, not the screen.
extern "C" void pmor_pb5_test(int high) {
    static GPIO tmosi;
    static bool tinit = false;
    if(!tinit) { tmosi.Init(seed::D10, GPIO::Mode::OUTPUT); tinit = true; }
    uint32_t t0 = System::GetNow();
    tmosi.Write(high != 0);
    while(System::GetNow() - t0 < 10) {}   // hold 10ms — ~10x a white page blit
    tmosi.Write(true);                     // release to bus idle (high)
}
uint8_t pmor_oled_contrast_level = 0x01;
void oledSetContrast(uint8_t level) { if (level < 1) level = 1; if (level > 0x20) level = 0x20; pmor_oled_contrast_level = level; }

extern "C" void pmor_oled_contrast(int level) {
    // 0x81 = SSD1309 contrast command. Panel current scales with this.
    oledCmdRaw(0x81, (uint8_t)level);
}
