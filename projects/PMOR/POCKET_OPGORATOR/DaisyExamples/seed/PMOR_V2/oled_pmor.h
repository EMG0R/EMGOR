#pragma once
//
// oled_pmor.h — PMOR's own SSD1309 driver. Pure GPIO bit-bang, no SPI
// peripheral, drop-in for libDaisy's OledDisplay<> graphics wrapper.
//
// WHY THIS EXISTS
// ---------------
// Using the SPI1 hardware peripheral to drive this panel kills USB instantly
// and permanently (device off the bus until a power cycle). That was
// established by elimination over a long debugging session — it tracks
// exactly with whether SPI1 TRANSMITS, and is unaffected by every software
// mitigation tried: interrupt priorities (6 -> 1), masking the OTG IRQ around
// the audio FIFO write, removing the USB audio push entirely, tud_task()
// yields inside Update(), disabling SPI1_IRQn, and minimum panel contrast.
//
// The decisive clue: driving the SAME pins by bit-bang left USB perfectly
// healthy. The only reason that attempt was abandoned is that the panel never
// lit — libDaisy's soft-SPI transport has no chip-select concept at all, so
// the panel's CS floated and it ignored every byte. That is a fixable bug in
// the transport, not a limitation of bit-banging.
//
// So: keep what demonstrably preserves USB (no SPI peripheral) and fix what
// broke the display (CS framing + timing) by owning the driver outright.
//
// PINS (fixed by the hardware — soldered, cannot change):
//   SCK  PG11  (Daisy D8)
//   MOSI PB5   (Daisy D10)   ** also the NeoTrellis bit-banged I2C SCL **
//   CS   PG10  (Daisy D7)
//   DC   PC10  (Daisy D3)
//   RST  PC9   (Daisy D4)
//
// NOTE ON PB5: it is shared with the NeoTrellis clock line. Both subsystems
// drive it as a plain GPIO output here, one at a time from the same thread,
// which is well-defined — unlike the previous arrangement where SPI1's
// alternate-function push-pull driver fought a bit-banged bus with pull-ups.
// Each transaction leaves the pin low and released to GPIO, so the trellis
// bus starts from a known state.
//
// COST: a full frame is 1KB of bit-banged bytes. That is why Update() only
// pushes PAGES WHOSE CONTENT CHANGED — on a status screen that is typically
// one or two of eight, cutting the per-frame cost by ~4-8x. It also pumps
// tud_task() between pages so a refresh cannot starve the USB stack (an
// earlier soft-SPI attempt made the audio stream crackle for exactly that
// reason).

#include "daisy_seed.h"
#include "tusb.h"
#include <cstring>

// Defined in the main translation unit: tud_task() + audio-ring drain +
// clock_engine_poll(). Update() calls it between page blits.
extern "C" void pmor_usb_service(void);

namespace pmor
{
/** Bit-banged 4-wire SPI SSD1309/SSD1306 driver, 128x64.
 *  Satisfies the interface libDaisy's OledDisplay<> requires:
 *  Config, Init, Height, Width, Fill, DrawPixel, Update, Reset,
 *  SendCommand, SendData.
 */
template <size_t width, size_t height>
class BitBangSSD130xDriver
{
  public:
    struct Config
    {
        daisy::Pin sclk;
        daisy::Pin mosi;
        daisy::Pin cs;
        daisy::Pin dc;
        daisy::Pin reset;
        /** Extra half-period delay in NOPs. 0 is usually fine at 480MHz
         *  because the GPIO writes themselves dominate; raise it if the panel
         *  shows garbage (too fast) rather than nothing (wrong CS/DC). */
        uint32_t   nop_delay;

        Config()
        {
            // VERIFIED against libDaisy 2026-08-15 (daisy_seed.h:204-208 and
            // the working SSD130x 4-wire SPI transport defaults):
            //   D7 = PG10, D8 = PG11, D9 = PB4, D10 = PB5, D11 = PB8
            // The transport this replaces used sclk=PG11, mosi=PB5, nss=PG10.
            // An earlier version of this file had all three shifted (D9/D11/D8
            // = PB4/PB8/PG11), i.e. it clocked the MISO pin and sent data to a
            // pin the panel is not connected to. That -- not signal integrity,
            // slew rate or open-drain vs push-pull -- is why the panel stayed
            // dark in EVERY electrical configuration tried.
            sclk      = daisy::seed::D8;   // PG11
            mosi      = daisy::seed::D10;  // PB5 (shared w/ NeoTrellis SCL)
            cs        = daisy::seed::D7;   // PG10
            dc        = daisy::seed::D3;
            reset     = daisy::seed::D4;
            nop_delay = 0;
        }
    };

    void Init(const Config &config)
    {
        cfg_ = config;
        sclk_.Init(cfg_.sclk, daisy::GPIO::Mode::OUTPUT);
        // MOSI is PB5 — PHYSICALLY SHARED with the NeoTrellis bit-banged I2C
        // clock line, which has bus pull-ups and the seesaw chip attached.
        // Driving it PUSH-PULL fights that bus: when we assert low against a
        // pull-up (and potentially against the seesaw driving), the resulting
        // current spikes are the most plausible remaining explanation for the
        // USB PHY dropping off (the trellis itself hammers this pin all day
        // with USB perfectly healthy — but it drives OPEN-DRAIN, as I2C must).
        // So drive it open-drain too: we can pull low, and the bus pull-ups
        // provide the high level. Costs a slower rising edge, which is fine at
        // our bit-bang rate.
#if defined(PMOR_MOSI_OPEN_DRAIN)
        // MEASURED 2026-08-15, with CORRECT pins for the first time: push-pull
        // on PB5 lights the panel but kills USB (no CDC at all), even with the
        // NeoTrellis never initialized. So the conflict is real, not the
        // artifact of the earlier wrong-pin driver.
        // Open-drain is worth a genuine try now: the board's PHYSICAL I2C
        // pull-ups still sit on PB5 even with the trellis disabled, so the
        // rising edge comes from those (kilohms, not the ~40k internal pull-up
        // that earlier reasoning assumed) and should still be fast enough.
        mosi_.Init(cfg_.mosi, daisy::GPIO::Mode::OUTPUT_OD, daisy::GPIO::Pull::PULLUP);
#elif defined(PMOR_OLED_USB_ONLY)
        mosi_.Init(cfg_.mosi, daisy::GPIO::Mode::OUTPUT);
#else
        mosi_.Init(cfg_.mosi, daisy::GPIO::Mode::OUTPUT_OD, daisy::GPIO::Pull::PULLUP);
#endif
        cs_.Init(cfg_.cs, daisy::GPIO::Mode::OUTPUT);
        dc_.Init(cfg_.dc, daisy::GPIO::Mode::OUTPUT);
        rst_.Init(cfg_.reset, daisy::GPIO::Mode::OUTPUT);

        cs_.Write(true);     // deasserted (active low)
        sclk_.Write(false);  // CPOL=0 — idle low, data sampled on rising edge
        Reset();
        InitPanel();
        Fill(false);
        // force a full push on the first Update()
        memset(dirty_, 1, sizeof(dirty_));
        Update();
    }

    void Reset()
    {
        rst_.Write(false);
        daisy::System::Delay(10);
        rst_.Write(true);
        daisy::System::Delay(10);
    }

    uint16_t Height() const { return height; }
    uint16_t Width() const { return width; }

    void Fill(bool on)
    {
        memset(buffer_, on ? 0xFF : 0x00, sizeof(buffer_));
        memset(dirty_, 1, sizeof(dirty_));
    }

    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on)
    {
        if(x >= width || y >= height)
            return;
        const size_t  idx  = x + (y / 8) * width;
        const uint8_t mask = (uint8_t)(1u << (y & 7));
        const uint8_t prev = buffer_[idx];
        buffer_[idx] = on ? (uint8_t)(prev | mask) : (uint8_t)(prev & ~mask);
        if(buffer_[idx] != prev)
            dirty_[y / 8] = 1;
    }

    void SendCommand(uint8_t cmd)
    {
        dc_.Write(false);
        cs_.Write(false);
        WriteByte(cmd);
        cs_.Write(true);
        ReleaseBus();
    }

    void SendData(uint8_t *buff, size_t size)
    {
        dc_.Write(true);
        cs_.Write(false);
        for(size_t i = 0; i < size; i++)
            WriteByte(buff[i]);
        cs_.Write(true);
        ReleaseBus();
    }

    /** Push only the pages whose pixels actually changed. */
    void Update()
    {
        // Self-heal: re-assert the display config every ~2s (at 15fps) so a
        // corrupted register cannot persist. ~11 bytes, negligible cost.
        if(++cfg_tick_ >= 30) { cfg_tick_ = 0; AssertDisplayConfig(); }

        const uint8_t high_col = (height == 32) ? 0x12 : 0x10;
        for(uint8_t page = 0; page < (height / 8); page++)
        {
            if(!dirty_[page])
                continue;
            SendCommand((uint8_t)(0xB0 + page));
            SendCommand(0x00);      // lower column start
            SendCommand(high_col);  // higher column start
            SendData(&buffer_[width * page], width);
            dirty_[page] = 0;
            // Bit-banging a page is long enough to matter to USB; service the
            // FULL USB plane between pages — task pump, audio-ring drain, and
            // MIDI-clock emission — so a refresh can never starve the audio
            // endpoint or delay a 0xF8 tick by a whole frame. (tud_task()
            // alone left ~3ms feed gaps per frame at nop_delay=24: audible
            // capture dropouts + BPM flicker in the host app, 2026-08-16.)
            pmor_usb_service();
        }
    }

  private:
    uint16_t cfg_tick_ = 0;
    /** Leave the SHARED pin high after every transaction.
     *
     *  MOSI is PB5, which is also the NeoTrellis bit-banged I2C SCL. That
     *  bus's code polls SCL until it reads HIGH (clock-stretch handling,
     *  `waitScl()`), so if a display transaction happens to end with the last
     *  data bit low, the very next trellis access spins FOREVER and the whole
     *  main loop hangs — no CDC, no MIDI, no audio, while USB stays
     *  enumerated because the host only needs the descriptors it already got.
     *  That is precisely the symptom seen before this was added.
     *
     *  Open-drain + pull-up means writing 1 simply releases the line, which
     *  is exactly the idle state I2C expects.
     */

    /** The shared pin (MOSI = PB5) needs two different personalities.
     *
     *  PUSH-PULL gives the panel a clean, fast-rising edge — which is the only
     *  way it reads data — but its switching energy into a bus that carries
     *  I2C pull-ups and the seesaw chip is what disturbs the USB PHY.
     *  OPEN-DRAIN is safe for USB but can only pull low; the ~40k internal
     *  pull-up then takes microseconds to rise and the panel sees nothing.
     *
     *  So: push-pull ONLY while actually shifting bits, at the LOWEST slew
     *  rate the silicon offers (OSPEEDR=00) to blunt the edges, and back to
     *  open-drain the instant the transaction ends. libDaisy's GPIO class has
     *  no slew-rate control, hence the direct register access.
     *  PB5 => GPIOB, pin 5: MODER[11:10], OTYPER[5], OSPEEDR[11:10].
     */
    // MEASURED 2026-08-15: switching PB5 to push-pull KILLS USB even when it
    // is done only during a transaction AND at the slowest slew rate the
    // silicon offers (OSPEEDR=00). Open-drain keeps USB alive indefinitely.
    // These are kept for the record but are NOT called.
    inline void MosiPushPullSlow()
    {
        GPIOB->OSPEEDR &= ~(3u << (5 * 2));          // 00 = low speed
        GPIOB->OTYPER  &= ~(1u << 5);                // 0  = push-pull
    }
    inline void MosiOpenDrain()
    {
        GPIOB->OTYPER  |= (1u << 5);                 // 1  = open-drain
    }

    inline void ReleaseBus()
    {
        mosi_.Write(true);
        sclk_.Write(false);
    }

    inline void Nops() const
    {
        for(uint32_t i = 0; i < cfg_.nop_delay; i++)
            __asm__ volatile("nop");
    }

    /** MSB-first, mode 0: data set while clock low, latched on rising edge. */
    inline void WriteByte(uint8_t b)
    {
        for(int bit = 7; bit >= 0; bit--)
        {
            mosi_.Write((b >> bit) & 1);
            Nops();
            sclk_.Write(true);
            Nops();
            sclk_.Write(false);
        }
    }

    // The subset of config that, if a stray byte lands on it, visibly wrecks
    // the picture: remap, scan direction, start line, offset, multiplex,
    // contrast, normal/inverse. Re-asserted periodically from Update() so a
    // corrupted register SELF-HEALS instead of persisting.
    //
    // Symptom that motivated this (2026-09-13): after minutes of running the
    // panel split in half and swapped the halves -- i.e. the start line
    // register changed at runtime. Nothing writes it, so it was a miswritten
    // command byte. Same class as the "8 NOPs -> sporadic miswritten bytes"
    // finding; belt and braces alongside the slower clock.
    void AssertDisplayConfig()
    {
        SendCommand(0xA0);              // segment remap (un-mirrored)
        SendCommand(0xC0);              // COM scan -- 0xC0 pairs with 0xA0 for 180deg
        SendCommand(0x40);              // start line 0
        SendCommand(0xD3); SendCommand(0x00);   // display offset 0
        SendCommand(0xA8); SendCommand(0x3F);   // multiplex 64
        SendCommand(0xA6);              // normal, not inverted
        SendCommand(0x81); SendCommand(0x01);   // contrast MINIMUM -- see note
        SendCommand(0xAF);              // display on
    }

    void InitPanel()
    {
        SendCommand(0xAE);              // display off
        SendCommand(0xD5);              // clock divide
        SendCommand(0x80);
        SendCommand(0xA8);              // multiplex
        SendCommand(0x3F);
        SendCommand(0xDA);              // COM pins
        SendCommand(0x12);
        SendCommand(0xD3);              // display offset
        SendCommand(0x00);
        SendCommand(0x40);              // start line 0
        SendCommand(0xA6);              // normal (not inverted)
        SendCommand(0xA4);              // resume from RAM
        SendCommand(0x8D);              // charge pump
        SendCommand(0x14);              // enable
        SendCommand(0xA0);              // segment remap -- 0xA0 un-mirrored
                                        // (0xA1 rendered mirror-image on this panel, 2026-09-13)
        SendCommand(0xC0);              // COM scan -- 0xC0 pairs with 0xA0 for 180deg
        SendCommand(0x81);              // contrast
        // 0x01 = minimum segment current (2026-08-16 pixel-current hunt):
        // full-white blits at the old 0x8F killed USB (rail sag from the
        // panel boost under max pixel load); black blits never did. Raise
        // cautiously once the safe ceiling is measured over CDC (OCx cmds).
        SendCommand(0x01);
        SendCommand(0xD9);              // pre-charge
        SendCommand(0x25);
        SendCommand(0xDB);              // VCOM detect
        SendCommand(0x34);
        SendCommand(0x20);              // addressing mode
        SendCommand(0x02);              // page addressing
        SendCommand(0xAF);              // display on
        daisy::System::Delay(100);
    }

    Config      cfg_;
    daisy::GPIO sclk_, mosi_, cs_, dc_, rst_;
    uint8_t     buffer_[width * height / 8];
    uint8_t     dirty_[height / 8];
};

using BitBangSSD130x128x64 = BitBangSSD130xDriver<128, 64>;

} // namespace pmor
