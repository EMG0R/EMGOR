// NeoTrellis focused test — proves tbus (D11=SCL, D12=SDA) is alive,
// shows what addresses ACK on the bus, displays HW ID, and runs
// a 16-pixel rainbow lava lamp. Incrementing counter on OLED proves
// the firmware is never hanging. No sensors, no SDMMC, no encoders.

#include <stdio.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed hw;
MyOled    display;

// ============================================================
// Bit-bang I2C bus  (open-drain GPIO, 100 kHz)
// Cannot hang — no HAL state machines involved, just GPIO writes/reads.
// tbus is on D11 (SCL = PB5) and D12 (SDA = PB8) — NeoTrellis only.
// ============================================================
struct BBus {
    dsy_gpio scl_g, sda_g;

    void init(dsy_gpio_pin sp, dsy_gpio_pin dp) {
        // Open-drain: write 0 to pull low, write 1 to release (pulled high externally)
        scl_g = {sp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        sda_g = {dp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        dsy_gpio_init(&scl_g); dsy_gpio_init(&sda_g);
        scl(1); sda(1); System::Delay(2); // idle = both high
    }

    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { System::DelayUs(5); } // 5 µs half-period = 100 kHz

    void start() { sda(1);hd();scl(1);hd();sda(0);hd();scl(0);hd(); }
    void stop()  { sda(0);hd();scl(1);hd();sda(1);hd(); }

    bool byteW(uint8_t b) {
        for(int i=7;i>=0;i--){sda((b>>i)&1);hd();scl(1);hd();scl(0);}
        sda(1);hd();scl(1);hd();
        bool ack=(dsy_gpio_read(&sda_g)==0); // slave pulls SDA low = ACK
        scl(0);hd();return ack;
    }
    uint8_t byteR(bool nack) {
        uint8_t v=0; sda(1);
        for(int i=7;i>=0;i--){hd();scl(1);hd();if(dsy_gpio_read(&sda_g))v|=(1<<i);scl(0);}
        sda(nack?1:0);hd();scl(1);hd();scl(0);hd();sda(1);
        return v;
    }

    // Returns true if device at 7-bit address a7 pulls SDA low (ACKs)
    bool probe(uint8_t a7) { start(); bool r=byteW((a7<<1)|0); stop(); hd(); return r; }

    // Write n bytes (caller includes module/reg/data — no auto-prefix)
    void send(uint8_t a7, const uint8_t* d, uint8_t n) {
        start(); byteW((a7<<1)|0);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop(); hd();
    }

    // Seesaw read: STOP + 500 µs + new START (seesaw requires full STOP, not repeated-START)
    // The SAMD09 needs ~500 µs to process the command and prepare the response byte.
    uint8_t ssR1(uint8_t a7, uint8_t base, uint8_t reg) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500); // mandatory seesaw processing gap
        start(); byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop(); hd(); return v;
    }

    // Scan 7-bit address space 0x08–0x77 and collect ACKing addresses.
    // Returns count. foundAddrs[] must hold at least maxFound entries.
    uint8_t scan(uint8_t* foundAddrs, uint8_t maxFound) {
        uint8_t cnt=0;
        for(uint8_t a=0x08; a<=0x77 && cnt<maxFound; a++) {
            if(probe(a)) foundAddrs[cnt++]=a;
        }
        return cnt;
    }
};

static BBus tbus; // D11=SCL(PB5), D12=SDA(PB8) — NeoTrellis bus

// ============================================================
// NeoTrellis / seesaw helpers
// ============================================================
static const uint8_t TREL = 0x2E; // default NeoTrellis 7-bit I2C address

// Maps 8-bit phase (0–255) to full-saturation rainbow RGB
static void wheel(uint8_t p, uint8_t& r, uint8_t& g, uint8_t& b) {
    if(p<85)       {r=p*3;    g=255-p*3;b=0;}
    else if(p<170) {p-=85; r=255-p*3;g=0;     b=p*3;}
    else           {p-=170;r=0;      g=p*3;   b=255-p*3;}
}

// Send 48-byte GRB pixel buffer to NeoTrellis in 2 chunks of 28 bytes each.
// SAMD09 wire buffer limit ~34 bytes — all 16 pixels (4+48=52 bytes) overflows it,
// so we send 8 pixels at a time. 500 µs between chunks lets seesaw store each burst.
static void ntShow(const uint8_t* grb48) {
    for(uint8_t chunk=0;chunk<2;chunk++) {
        uint16_t off=(uint16_t)chunk*24; // byte offset in seesaw neopixel buffer
        uint8_t pkt[28];
        pkt[0]=0x0E; pkt[1]=0x04;         // NEOPIXEL module, BUF function
        pkt[2]=off>>8; pkt[3]=off&0xFF;    // offset big-endian
        for(uint8_t i=0;i<24;i++) pkt[4+i]=grb48[off+i]; // 8 pixels × 3 GRB bytes
        tbus.send(TREL,pkt,28);
        System::DelayUs(500); // seesaw needs time to absorb chunk into its RAM
    }
    // NEOPIXEL_SHOW: latch buffer → LEDs
    const uint8_t sh[]={0x0E,0x05,0x01};
    tbus.send(TREL,sh,3);
}

// ============================================================
// OLED helper
// ============================================================
static char row[32];
static void orow(int y, const char* s) {
    display.SetCursor(0,y);
    display.WriteString(s, Font_6x8, true);
}

// ============================================================
// main
// ============================================================
int main(void)
{
    hw.Configure(); hw.Init();

    // OLED on SPI — D3=DC, D4=RST (hardware SPI handled by libDaisy)
    MyOled::Config oc;
    oc.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oc.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oc);
    display.Fill(false);
    display.SetCursor(0,0); display.WriteString("TBUS INIT...",Font_6x8,true);
    display.Update();

    // ── Init tbus (NeoTrellis bus only) ────────────────────────────────────
    // D11 = PB5 = SCL,  D12 = PB8 = SDA
    // These are the physical header pins the NeoTrellis SCL/SDA wires connect to.
    tbus.init(hw.GetPin(11), hw.GetPin(12));
    System::Delay(10); // let bus settle after init

    // ── I2C bus scan on tbus — shows every address that ACKs ───────────────
    // Scans 0x08–0x77. NeoTrellis default addr is 0x2E. Print up to 4 found.
    uint8_t found[8]={};
    uint8_t nfound = tbus.scan(found, 8);

    // Show scan result on OLED immediately
    display.Fill(false);
    if(nfound==0) {
        orow(0,"TBUS: nothing found");
        orow(9,"check D11/D12 wiring");
    } else {
        // Print first 4 found addresses
        char scanrow[32]="SCAN:";
        int pos=5;
        for(uint8_t i=0;i<nfound && i<4;i++){
            pos+=sprintf(scanrow+pos," %02X",found[i]);
        }
        orow(0,scanrow);
    }
    display.Update();
    System::Delay(800); // let user read scan result

    // ── NeoTrellis init ─────────────────────────────────────────────────────
    bool trel_ok = false;
    uint8_t hwid = 0;

    // Software reset (STATUS module=0x00, SWRST fn=0x7F, magic=0xFF)
    const uint8_t rst[]={0x00,0x7F,0xFF};
    tbus.send(TREL,rst,3);
    System::Delay(500); // SAMD09 firmware reload takes full 500 ms

    // Read hardware ID — must be 0x55 for SAMD09 (the chip used on NeoTrellis)
    hwid   = tbus.ssR1(TREL, 0x00, 0x00);
    trel_ok = (hwid == 0x55);

    if(trel_ok) {
        // Set neopixel data pin: pin 3 is hardwired to the LED array on NeoTrellis PCB
        const uint8_t pin[]={0x0E,0x01,3}; tbus.send(TREL,pin,3);
        System::Delay(10);

        // Set signal speed: 0x01 = 800 kHz (required for WS2812B)
        const uint8_t spd[]={0x0E,0x02,0x01}; tbus.send(TREL,spd,3);
        System::Delay(10);

        // Set buffer length: 16 pixels * 3 bytes/pixel = 48 bytes (0x30)
        // This tells the SAMD09 to allocate 48 bytes of RAM for the neopixel buffer
        const uint8_t blen[]={0x0E,0x03,0x00,0x30}; tbus.send(TREL,blen,4);
        System::Delay(10);

        // Clear all pixels to black on startup (blank show)
        uint8_t blank[28]={};
        blank[0]=0x0E; blank[1]=0x04; blank[2]=0; blank[3]=0;  // offset=0
        tbus.send(TREL,blank,28); System::DelayUs(500);
        blank[3]=24;                                              // offset=24
        tbus.send(TREL,blank,28); System::DelayUs(500);
        const uint8_t sh[]={0x0E,0x05,0x01}; tbus.send(TREL,sh,3);
        System::Delay(5);
    }

    // ── Show full init result on OLED ───────────────────────────────────────
    display.Fill(false);
    // Row 0: bus scan summary
    {
        char scanrow[32]="TBUS D11/D12:";
        int pos=13;
        for(uint8_t i=0;i<nfound && i<3;i++) pos+=sprintf(scanrow+pos," %02X",found[i]);
        if(nfound==0) sprintf(scanrow+pos," ---");
        orow(0,scanrow);
    }
    // Row 9: NeoTrellis HW ID and status
    sprintf(row,"ID:0x%02X TREL:%s",hwid,trel_ok?"OK":"FAIL"); orow(9,row);
    // Row 18: pixel config confirmation
    sprintf(row,"PIX:16 PIN:3 800kHz"); orow(18,row);
    // Row 27: address in use
    sprintf(row,"ADDR:0x%02X",TREL); orow(27,row);
    display.Update();
    System::Delay(1500); // hold init screen

    // ── Lava lamp state ─────────────────────────────────────────────────────
    // 16 pixels each drift at their own speed (1, 2, or 3 phase units/frame)
    static const uint8_t DRIFT[16]={1,3,2,1, 2,1,3,2, 1,2,1,3, 3,1,2,1};
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    uint32_t frame=0;     // frame counter — proves firmware is not hanging
    uint32_t rng=12345;   // LCG pseudo-random state

    // =====================================================================
    while(1)
    {
        frame++;

        // Simple LCG pseudo-random — changes every frame, visually proves no freeze
        // Parameters: multiplier 1664525, increment 1013904223 (Numerical Recipes)
        rng = rng * 1664525u + 1013904223u;
        uint16_t rval = (uint16_t)(rng >> 16); // upper 16 bits are more random

        // ── Rainbow lava lamp ───────────────────────────────────────────────
        if(trel_ok) {
            for(uint8_t i=0;i<16;i++) pphase[i]+=DRIFT[i]; // each pixel advances at own speed
            uint8_t grb[48];
            for(uint8_t i=0;i<16;i++){
                uint8_t r,g,b;
                wheel(pphase[i],r,g,b);
                r>>=1; g>>=1; b>>=1; // half brightness — full is very intense
                grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b; // NeoTrellis uses GRB order
            }
            ntShow(grb);
        }

        // ── OLED update ─────────────────────────────────────────────────────
        // Show bus status, NeoTrellis state, and always-changing numbers
        // that prove the firmware is alive and not stuck anywhere.
        display.Fill(false);

        // Row 0: bus scan (persistent from init — shows which addresses ACK)
        {
            char scanrow[32]="BUS:";
            int pos=4;
            for(uint8_t i=0;i<nfound && i<4;i++) pos+=sprintf(scanrow+pos," %02X",found[i]);
            if(nfound==0) sprintf(scanrow+pos," NOTHING");
            orow(0,scanrow);
        }

        // Row 9: NeoTrellis status — HW ID and OK/FAIL
        sprintf(row,"TREL:%s ID:0x%02X",trel_ok?"OK  ":"FAIL",hwid); orow(9,row);

        // Row 18: time in milliseconds — always counting up, proves no hang
        sprintf(row,"T:%8lums",System::GetNow()); orow(18,row);

        // Row 27: frame counter — shows how many loop iterations have completed
        sprintf(row,"FRM:%8lu",frame); orow(27,row);

        // Row 36: pseudo-random number — jumps every frame, proves no freeze
        sprintf(row,"RND:%5u",rval); orow(36,row);

        // Row 45: NeoTrellis pixel phase of pixel 0 — shows animation is running
        sprintf(row,"PH0:%3u PH8:%3u",pphase[0],pphase[8]); orow(45,row);

        // Row 54: bus address and chunk size info
        sprintf(row,"ADDR:0x%02X CHK:2x28",TREL); orow(54,row);

        display.Update();
        System::Delay(20); // ~20 ms floor per loop
    }
}
