// HelloOled.cpp — Pocket OpGorator prototype sensor display
// Backup 3: full sensor suite + corrected NeoTrellis seesaw protocol.
//
// All I2C is bit-bang (open-drain GPIO) because libDaisy's HAL I2C has an
// infinite busy-wait bug in TransmitBlocking that hangs the CPU permanently
// if the peripheral gets stuck. Bit-bang cannot hang — worst case is a NACK.
//
// Two independent I2C buses:
//   tbus — NeoTrellis only          — D11(SCL=PB5), D12(SDA=PB8)
//   sbus — MPU9250/AK8963/VL53L1X  — D13(SCL=PB9), D14(SDA=PB6)
//
// Seesaw protocol — confirmed correct from Adafruit source code:
//   STATUS_BASE    = 0x00
//   STATUS_HW_ID   = 0x01  ← register that returns 0x55 for SAMD09 (NOT 0x00)
//   STATUS_SWRST   = 0x7F  (write 0xFF to trigger software reset)
//   NEOPIXEL_BASE  = 0x0E
//   NEOPIXEL_SHOW  = 0x05  ← command is exactly {0x0E, 0x05} — 2 bytes only (NOT 3)
//
// OLED display rows (128×64, Font_6x8 = 8px tall, 6px wide, ~21 chars/row):
//   y=0  : Accel  "A ±X.X ±Y.Y ±Z.Z"   (g, ±2g range)
//   y=9  : Gyro   "G ±X.X ±Y.Y ±Z.Z"   (°/s, ±250°/s range)
//   y=18 : Mag    "M ±X.X ±Y.Y ±Z.Z"   (µT)
//   y=27 : Lidar  "D: NNNNmm S:XX"      (mm, hex status)
//   y=36 : Enc    "E1:>   E2:<"          (last event, 250ms hold)
//   y=45 : Mic    "[###...............]" (audio peak bar)
//   y=54 : FSR+SD "F:NNNN SD:OK"

#include <stdio.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "VL53L1X_mini.h"
#include "fatfs.h"

using namespace daisy;
using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed     hw;
MyOled        display;
VL53L1X_Mini  lidar;
// SDMMC + FatFS objects must be global (not stack) — hardware DMA requirement
SdmmcHandler  sd;
FatFSInterface fsi;

// ============================================================
// Bit-bang I2C bus  (open-drain GPIO, 100 kHz, ~5 µs half-period)
// ============================================================
// hd() = half-bit delay. At 5 µs, full clock period = 10 µs → 100 kHz.
// All methods are blocking but bounded — no HAL state machines involved.
// Two independent instances: tbus (NeoTrellis) and sbus (sensors).
// They share NO pins and cannot interfere with each other electrically.
struct BBus {
    dsy_gpio scl_g, sda_g;

    void init(dsy_gpio_pin sp, dsy_gpio_pin dp) {
        // Open-drain with no pull (external 4.7kΩ pull-ups on each bus)
        scl_g = {sp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        sda_g = {dp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        dsy_gpio_init(&scl_g); dsy_gpio_init(&sda_g);
        scl(1); sda(1); System::Delay(2); // idle bus = both high
    }

    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { System::DelayUs(5); }       // 100 kHz half-period

    // Standard I2C START: SDA falls while SCL high
    void start() { sda(1);hd();scl(1);hd();sda(0);hd();scl(0);hd(); }
    // Standard I2C STOP: SDA rises while SCL high
    void stop()  { sda(0);hd();scl(1);hd();sda(1);hd(); }

    // Write one byte MSB-first, return true if slave ACKed
    bool byteW(uint8_t b) {
        for(int i=7;i>=0;i--){sda((b>>i)&1);hd();scl(1);hd();scl(0);}
        sda(1);hd();scl(1);hd(); // release SDA for ACK
        bool ack=(dsy_gpio_read(&sda_g)==0); // low = ACK
        scl(0);hd();return ack;
    }
    // Read one byte MSB-first; nack=true on last byte (signals end-of-read)
    uint8_t byteR(bool nack) {
        uint8_t v=0; sda(1); // release SDA
        for(int i=7;i>=0;i--){hd();scl(1);hd();if(dsy_gpio_read(&sda_g))v|=(1<<i);scl(0);}
        sda(nack?1:0);hd();scl(1);hd();scl(0);hd();sda(1);
        return v;
    }

    // Test if a device ACKs at 7-bit address a7
    bool probe(uint8_t a7) { start();bool r=byteW((a7<<1)|0);stop();hd();return r; }

    // Write n bytes to device (no register prefix — caller includes reg bytes in d)
    void send(uint8_t a7, const uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }

    // Write [reg, val] to device — standard 8-bit-register sensors (MPU9250, AK8963)
    void w8(uint8_t a7, uint8_t reg, uint8_t val) {
        uint8_t b[2]={reg,val}; send(a7,b,2);
    }

    // Read 1 byte from 8-bit register using repeated-START (standard I2C read)
    uint8_t r8(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1); // repeated START, switch to read direction
        uint8_t v=byteR(true); stop();hd(); return v;
    }

    // Read 2 bytes big-endian from 8-bit register (used for MPU9250 sensor data)
    int16_t r16be(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t hi=byteR(false), lo=byteR(true); // ACK hi, NACK lo (last byte)
        stop();hd(); return (int16_t)((hi<<8)|lo);
    }

    // Write n bytes to 16-bit register address (VL53L1X uses 16-bit register map)
    void w16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }

    // Read n bytes from 16-bit register address using repeated-START
    void r16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) d[i]=byteR(i==n-1); // NACK only on last byte
        stop();hd();
    }

    // Seesaw read: STOP + 500 µs + new START (seesaw REQUIRES this — NOT repeated-START).
    // The SAMD09 on NeoTrellis needs ~500 µs to prepare the response after a write command.
    // Using repeated-START (no delay) corrupts the read on seesaw — key difference from
    // standard I2C sensors which happily accept repeated-START with no gap.
    uint8_t ssR1(uint8_t a7, uint8_t base, uint8_t reg) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500); // mandatory seesaw processing delay
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    void ssRN(uint8_t a7, uint8_t base, uint8_t reg, uint8_t* out, uint8_t n) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) out[i]=byteR(i==n-1);
        stop();hd();
    }
};

static BBus tbus; // NeoTrellis bus  — D11=SCL(PB5),  D12=SDA(PB8)
static BBus sbus; // Sensor bus      — D13=SCL(PB9),  D14=SDA(PB6)

// VL53L1X_mini.cpp calls these global callbacks — routes to sensor bus with 16-bit addressing
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg (uint16_t reg, uint8_t* d, uint8_t n) { sbus.r16a(0x29,reg,d,n); }

// ============================================================
// NeoTrellis helpers  (Adafruit seesaw / WS2812B protocol)
// ============================================================
// Seesaw neopixel module = 0x0E, functions (confirmed from Adafruit_seesaw.h):
//   0x01 = NEOPIXEL_PIN        (which GPIO pin drives data line — always 3 on NeoTrellis)
//   0x02 = NEOPIXEL_SPEED      (0x00=400kHz, 0x01=800kHz — WS2812B requires 800kHz)
//   0x03 = NEOPIXEL_BUF_LENGTH (total buffer bytes: 16 pixels × 3 bytes = 48 = 0x30)
//   0x04 = NEOPIXEL_BUF        (raw pixel data write, with 16-bit byte-offset header)
//   0x05 = NEOPIXEL_SHOW       (latch buffer → LEDs — command is exactly 2 bytes: {0x0E,0x05})
//
// SAMD09 I2C wire buffer is ~32 bytes. Sending all 16 pixels at once
// (4-byte header + 48 data = 52 bytes) overflows it — only ~10 pixels light up.
// Fix: 2 chunks of 28 bytes (4-byte header + 24 bytes = 8 pixels per chunk).
// 500 µs between chunks lets seesaw firmware copy each burst into its pixel RAM.
static const uint8_t TREL = 0x2E; // NeoTrellis factory default 7-bit I2C address

// Rainbow wheel: maps 0–255 phase to RGB — full saturation, no white
static void wheel(uint8_t p, uint8_t& r, uint8_t& g, uint8_t& b) {
    if(p<85)       {r=p*3;     g=255-p*3; b=0;}
    else if(p<170) {p-=85; r=255-p*3; g=0;     b=p*3;}
    else           {p-=170;r=0;      g=p*3;   b=255-p*3;}
}

// Write 16-pixel GRB buffer to seesaw then latch with SHOW command.
// Two 28-byte chunks (8 pixels each) stay within SAMD09 wire buffer limit.
// SHOW is exactly {0x0E, 0x05} — 2 bytes per Adafruit seesaw library source.
// (A 3rd byte would be parsed as a new command prefix, corrupting protocol state.)
static void ntShow(uint8_t* grb48) {
    for(uint8_t chunk=0;chunk<2;chunk++) {
        uint16_t off = (uint16_t)chunk * 24; // byte offset into the 48-byte neopixel buffer
        uint8_t pkt[28];
        pkt[0]=0x0E; pkt[1]=0x04;            // NEOPIXEL_BASE, NEOPIXEL_BUF
        pkt[2]=off>>8; pkt[3]=off&0xFF;       // offset big-endian
        for(uint8_t i=0;i<24;i++) pkt[4+i]=grb48[off+i]; // 8 pixels × 3 bytes GRB
        tbus.send(TREL,pkt,28);
        System::DelayUs(500); // seesaw SAMD09 needs time to store chunk in its RAM buffer
    }
    // NEOPIXEL_SHOW — exactly 2 bytes. Do NOT add a 3rd byte here.
    const uint8_t sh[]={0x0E,0x05};
    tbus.send(TREL,sh,2);
}

// ============================================================
// Quadrature encoders  (polled every ~1.3 ms in AudioCallback)
// ============================================================
// delta and clicked are volatile because they're written in AudioCallback ISR
// and read in the main loop. Without volatile, the compiler may cache stale values
// in registers across loop iterations at -O2, making encoder events invisible to main.
struct Enc {
    dsy_gpio a, b, sw;
    uint8_t  lab, psw;          // previous A/B state, previous switch state
    volatile int8_t  delta;     // accumulated quadrature count since last main-loop read
    volatile bool    clicked;   // set true on falling edge of switch, cleared by main loop
};
static Enc  enc1, enc2;
static bool enc_ready = false;  // guards AudioCallback from polling before GPIO is ready

// 4-state Gray-code LUT: lut[prev_ab][curr_ab] = ±1 or 0
// CW rotation: 0→2→3→1→0 (each step = +1)
// CCW rotation: 0→1→3→2→0 (each step = -1)
static void ePoll(Enc& e) {
    static const int8_t lut[4][4]={{0,-1,+1,0},{+1,0,0,-1},{-1,0,0,+1},{0,+1,-1,0}};
    uint8_t ab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    if(ab!=e.lab){e.delta+=lut[e.lab][ab];e.lab=ab;}
    uint8_t sw=dsy_gpio_read(&e.sw);
    if(e.psw&&!sw) e.clicked=true; // falling edge = button press
    e.psw=sw;
}
static void eInit(Enc& e, dsy_gpio_pin pa, dsy_gpio_pin pb, dsy_gpio_pin ps) {
    e.a ={pa,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.b ={pb,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.sw={ps,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    dsy_gpio_init(&e.a); dsy_gpio_init(&e.b); dsy_gpio_init(&e.sw);
    e.lab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b); // capture initial state
    e.psw=1; e.delta=0; e.clicked=false;
}

// AudioCallback: called every 64 samples @ 48 kHz ≈ every 1.3 ms.
// Only does encoder polling + mic peak detection — no I2C, no blocking.
static volatile float mic_amp = 0.f; // peak magnitude of current audio block
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t sz) {
    if(enc_ready){ ePoll(enc1); ePoll(enc2); }
    float pk=0.f;
    for(size_t i=0;i<sz;i++){
        float s=in[0][i]; if(s<0)s=-s; if(s>pk)pk=s;
        out[0][i]=0; out[1][i]=0; // silence outputs
    }
    mic_amp=pk;
}

// ============================================================
// OLED display helpers
// ============================================================
// fmtf: format a float as ±W.F with 1 decimal place (e.g. "+1.0", "-9.8")
static int fmtf(char* o, float v) {
    bool neg=v<0; if(neg) v=-v;
    int w=(int)v, f=(int)((v-w)*10+.5f);
    if(f>=10){w++;f=0;}
    return sprintf(o,"%c%d.%d",neg?'-':'+',w,f);
}
static char row[32]; // scratch buffer for one OLED row (32 bytes to avoid any overflow)
static void orow(int y, const char* s) {
    display.SetCursor(0,y); display.WriteString(s,Font_6x8,true);
}

// ============================================================
// main
// ============================================================
int main(void)
{
    hw.Configure(); hw.Init();

    // ── OLED (SPI, D3=DC, D4=RST) ──────────────────────────────────────────
    MyOled::Config oc;
    oc.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oc.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oc);
    display.Fill(false);
    display.SetCursor(0,0); display.WriteString("INIT...",Font_6x8,true);
    display.Update();

    // ── I2C buses ──────────────────────────────────────────────────────────
    // Sensor bus first (D13=PB9 SCL, D14=PB6 SDA) — MPU9250, AK8963, VL53L1X
    sbus.init(hw.GetPin(13), hw.GetPin(14));
    // Trellis bus (D11=PB5 SCL, D12=PB8 SDA) — NeoTrellis only
    tbus.init(hw.GetPin(11), hw.GetPin(12));

    // ── MPU9250 accel/gyro (addr 0x68) ─────────────────────────────────────
    // Hard reset the device first (DEVICE_RESET bit in PWR_MGMT_1).
    // Some clone boards ship in a bad state after power-on; reset clears it.
    // After reset the bit self-clears; wait 100 ms for all registers to reload.
    sbus.w8(0x68, 0x6B, 0x80); System::Delay(100); // DEVICE_RESET
    // Wake from sleep, select PLL clock (auto-selects best gyro oscillator)
    sbus.w8(0x68, 0x6B, 0x01); System::Delay(50);
    // WHO_AM_I (reg 0x75): MPU9250=0x71, MPU9255=0x73, some clones=0x70
    // Reject 0x00 and 0xFF which indicate bus failure (no ACK / SDA stuck).
    uint8_t imu_id = sbus.r8(0x68, 0x75);
    bool imu_ok = (imu_id==0x71 || imu_id==0x73 || imu_id==0x70);

    // ── AK8963 magnetometer via MPU9250 I2C bypass (addr 0x0C) ─────────────
    // Enable bypass so the AK8963 is directly visible on sbus (not behind MPU master)
    sbus.w8(0x68, 0x37, 0x02); System::Delay(10); // INT_PIN_CFG: BYPASS_EN=1
    sbus.w8(0x0C, 0x0A, 0x00); System::Delay(10); // CNTL: power-down before mode change
    bool mag_ok = (sbus.r8(0x0C, 0x00) == 0x48);  // WIA = 0x48 for AK8963
    // 16-bit output, continuous mode 2 (100 Hz)
    sbus.w8(0x0C, 0x0A, 0x16); System::Delay(10);

    // ── VL53L1X lidar (addr 0x29) ──────────────────────────────────────────
    // lidar.init() does soft-reset, waits for firmware ready, configures long-range 50ms budget.
    // Uses vl53_write_reg / vl53_read_reg callbacks → sbus.w16a / sbus.r16a above.
    bool lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(100); // trigger measurement every 100 ms

    // ── SD card via hardware SDMMC1 ────────────────────────────────────────
    // SDMMC1 pins are fixed in HAL (not configurable via GetPin):
    //   PC12=CK (D7), PD2=CMD (solder pad — NOT on standard through-hole header!),
    //   PC8=D0 (D5) — 1-bit mode only, avoids PC11=D2 conflict with enc1 switch.
    // If PD2 is not wired, f_mount returns FR_NOT_READY and sd_ok stays false.
    bool sd_ok = false;
    {
        SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd_cfg.width = SdmmcHandler::BusWidth::BITS_1; // avoid PC11/D2 conflict
        sd.Init(sd_cfg);
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        // Immediate mount (3rd arg=1): tries to talk to card now.
        // Fails fast with FR_NOT_READY if no card or CMD not wired.
        sd_ok = (f_mount(&fsi.GetSDFileSystem(), "/", 1) == FR_OK);
    }

    // ── Encoders ───────────────────────────────────────────────────────────
    // Enc1: A=D0, B=D1, SW=D2    (D2=PC11 — safe in 1-bit SDMMC mode above)
    // Enc2: A=D24(PA5), B=D16(PC0), SW=D17(PA1)
    eInit(enc1, hw.GetPin(0),  hw.GetPin(1),  hw.GetPin(2));
    eInit(enc2, hw.GetPin(24), hw.GetPin(16), hw.GetPin(17));
    enc_ready = true; // let AudioCallback start polling

    // ── FSR analog input (D15=PB7) ─────────────────────────────────────────
    AdcChannelConfig adc_cfg;
    adc_cfg.InitSingle(hw.GetPin(15));
    hw.adc.Init(&adc_cfg, 1);
    hw.adc.Start();

    // ── Audio engine ───────────────────────────────────────────────────────
    hw.SetAudioBlockSize(64); // 64 samples/block @ 48 kHz → AudioCallback every ~1.3 ms
    hw.StartAudio(AudioCallback);

    // ── NeoTrellis init (LAST — after all other inits to avoid SDMMC interference) ─
    // The seesaw SAMD09 must be initialized AFTER any slow operations (SDMMC f_mount
    // can block seconds). Doing it last ensures the seesaw is freshly reset and
    // its state is clean right before the main loop starts sending pixels.
    bool trel_ok = false;
    {
        // Software reset: STATUS_BASE=0x00, STATUS_SWRST=0x7F, magic=0xFF
        const uint8_t rst[]={0x00,0x7F,0xFF}; tbus.send(TREL,rst,3);
        System::Delay(500); // SAMD09 re-flashes its firmware state — needs full 500 ms

        // Read hardware ID: STATUS_BASE=0x00, STATUS_HW_ID=0x01.
        // Returns 0x55 for SAMD09 (the chip on NeoTrellis).
        // IMPORTANT: register is 0x01, NOT 0x00. Confirmed from Adafruit_seesaw.h source.
        uint8_t hwid = tbus.ssR1(TREL, 0x00, 0x01);
        trel_ok = (hwid == 0x55);

        if(trel_ok) {
            // Set neopixel data pin (always pin 3 on NeoTrellis hardware routing)
            const uint8_t pin[]={0x0E,0x01,3}; tbus.send(TREL,pin,3);
            System::Delay(10);

            // Set signal speed: 0x01 = 800 kHz (required for WS2812B LEDs)
            const uint8_t spd[]={0x0E,0x02,0x01}; tbus.send(TREL,spd,3);
            System::Delay(10);

            // Set neopixel buffer length: 16 pixels × 3 bytes/pixel = 48 bytes (0x0030).
            // This triggers RAM allocation on the SAMD09 — 10 ms for it to complete.
            const uint8_t blen[]={0x0E,0x03,0x00,0x30}; tbus.send(TREL,blen,4);
            System::Delay(10);

            // Clear all pixels to black on startup (2 chunks of zeros)
            uint8_t blank[28]={};
            blank[0]=0x0E; blank[1]=0x04; blank[2]=0; blank[3]=0;   // chunk 0 at offset 0
            tbus.send(TREL,blank,28); System::DelayUs(500);
            blank[3]=24;                                               // chunk 1 at offset 24
            tbus.send(TREL,blank,28); System::DelayUs(500);
            // SHOW — exactly 2 bytes. No trailing byte.
            const uint8_t sh[]={0x0E,0x05}; tbus.send(TREL,sh,2);
            System::Delay(5);

            // NOTE: keypad event enables deliberately omitted.
            // We don't read the FIFO in the main loop, so enabling events just fills
            // the FIFO → wraps → wastes SAMD09 cycles. Simpler = more reliable.
        }
    }

    // ── Init result screen (shown 1.5s before entering main loop) ──────────
    display.Fill(false);
    // Row 0: IMU WHO_AM_I shown as hex so user can see actual device ID if check fails
    sprintf(row,"IMU:%s(0x%02X) MAG:%s",imu_ok?"OK":"--",imu_id,mag_ok?"OK":"--"); orow(0,row);
    sprintf(row,"LDR:%s TRL:%s SD:%s",lidar_ok?"OK":"--",trel_ok?"OK":"--",sd_ok?"OK":"--"); orow(9,row);
    display.Update(); System::Delay(1500);

    // ── Lava lamp state ────────────────────────────────────────────────────
    // DRIFT[i] = phase units pixel i advances per loop iteration.
    // Different speeds (1, 2, 3) create the "each pixel has its own velocity" lava effect.
    static const uint8_t DRIFT[16]={1,3,2,1, 2,1,3,2, 1,2,1,3, 3,1,2,1};
    // Evenly-spaced initial phases so all 16 colors are visible immediately on boot.
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    // ── Main loop variables ────────────────────────────────────────────────
    uint16_t dist=0;
    float    mx=0, my=0, mz=0; // magnetometer (only updated when new data ready)
    char     e1s[6]="--", e2s[6]="--"; // encoder display strings
    uint32_t e1until=0, e2until=0;     // timestamps until which to keep event string shown

    // =====================================================================
    while(1)
    {
        // ── Encoders ───────────────────────────────────────────────────────
        // Display event string for exactly 250 ms wall-clock time (not iterations),
        // so the display hold is independent of how long the rest of the loop takes.
        uint32_t now = System::GetNow();

        // Encoder 1 — reads volatile delta written by AudioCallback ISR
        if(enc1.delta>0)      { sprintf(e1s,">");   enc1.delta=0; e1until=now+250; }
        else if(enc1.delta<0) { sprintf(e1s,"<");   enc1.delta=0; e1until=now+250; }
        if(enc1.clicked)      { sprintf(e1s,"CLK"); enc1.clicked=false; e1until=now+250; }
        if(now >= e1until) sprintf(e1s,"--"); // timer expired → idle indicator

        // Encoder 2 — same logic
        if(enc2.delta>0)      { sprintf(e2s,">");   enc2.delta=0; e2until=now+250; }
        else if(enc2.delta<0) { sprintf(e2s,"<");   enc2.delta=0; e2until=now+250; }
        if(enc2.clicked)      { sprintf(e2s,"CLK"); enc2.clicked=false; e2until=now+250; }
        if(now >= e2until) sprintf(e2s,"--");

        // ── IMU accel + gyro ────────────────────────────────────────────────
        // Registers 0x3B–0x48: ACCEL_XOUT_H, _Y, _Z then GYRO_XOUT_H, _Y, _Z
        // Each is 16-bit signed big-endian. r16be does write-reg + repeated-START + 2-byte read.
        // Sensitivity: ±2g range → 16384 LSB/g; ±250°/s → 131 LSB/(°/s)
        float ax=0,ay=0,az=0,gx=0,gy=0,gz=0;
        if(imu_ok) {
            ax = sbus.r16be(0x68,0x3B)/16384.f;
            ay = sbus.r16be(0x68,0x3D)/16384.f;
            az = sbus.r16be(0x68,0x3F)/16384.f;
            gx = sbus.r16be(0x68,0x43)/131.f;
            gy = sbus.r16be(0x68,0x45)/131.f;
            gz = sbus.r16be(0x68,0x47)/131.f;
        }

        // ── AK8963 magnetometer ─────────────────────────────────────────────
        // ST1 reg 0x02 bit0 = DRDY (data ready). Read only when new sample available.
        // Must read ST2 reg 0x09 after data regs to clear DRDY and allow next sample.
        if(mag_ok && (sbus.r8(0x0C,0x02)&0x01)) {
            mx = sbus.r16be(0x0C,0x03)*0.15f; // 16-bit mode sensitivity = 0.15 µT/LSB
            my = sbus.r16be(0x0C,0x05)*0.15f;
            mz = sbus.r16be(0x0C,0x07)*0.15f;
            sbus.r8(0x0C,0x09); // read ST2 to release data-ready latch
        }

        // ── VL53L1X lidar ───────────────────────────────────────────────────
        // Non-blocking: dataReady() reads one register over I2C. Only call read()
        // when data is ready — read() internally blocks up to 200 ms waiting if
        // called before the measurement completes.
        if(lidar_ok && lidar.dataReady()) dist = lidar.read();

        // ── FSR pressure sensor ─────────────────────────────────────────────
        // 3-sample median filter removes ADC noise spikes.
        uint16_t fsr;
        {
            static uint16_t fsrbuf[3]={}; static uint8_t fsridx=0;
            fsrbuf[fsridx]=(uint16_t)(hw.adc.GetFloat(0)*4095.f); fsridx=(fsridx+1)%3;
            uint16_t a=fsrbuf[0],c=fsrbuf[1],d=fsrbuf[2];
            if(a>c){uint16_t t=a;a=c;c=t;} if(c>d){uint16_t t=c;c=d;d=t;} if(a>c){uint16_t t=a;a=c;c=t;}
            fsr=c; // middle value after sort
        }

        float amp = mic_amp; // peak audio amplitude this block (volatile read from ISR)

        // ── NeoTrellis lava lamp ────────────────────────────────────────────
        // Pure pixel writes, no keypad FIFO reads — keeps seesaw bus state clean.
        // Each pixel advances its phase by DRIFT[i] per iteration, then gets
        // rainbow-wheel color at half brightness (right-shift by 1).
        if(trel_ok) {
            for(uint8_t i=0;i<16;i++) pphase[i]+=DRIFT[i]; // advance phases
            uint8_t grb[48];
            for(uint8_t i=0;i<16;i++){
                uint8_t r,g,b;
                wheel(pphase[i],r,g,b);
                r>>=1; g>>=1; b>>=1; // half brightness — full brightness is harsh
                // Seesaw neopixel byte order is GRB (not RGB)
                grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
            }
            ntShow(grb); // two 28-byte chunks + 2-byte SHOW command
        }

        // ── OLED 7-row display ──────────────────────────────────────────────
        display.Fill(false);
        int n=0;

        // Row 0: Accelerometer (g) — at rest flat: ~+1.0 on whichever axis points up
        row[n++]='A';
        n+=fmtf(row+n,ax); row[n++]=' ';
        n+=fmtf(row+n,ay); row[n++]=' ';
        n+=fmtf(row+n,az); row[n]=0; orow(0,row); n=0;

        // Row 9: Gyroscope (°/s) — at rest all near 0.0
        row[n++]='G';
        n+=fmtf(row+n,gx); row[n++]=' ';
        n+=fmtf(row+n,gy); row[n++]=' ';
        n+=fmtf(row+n,gz); row[n]=0; orow(9,row); n=0;

        // Row 18: Magnetometer (µT) — varies by orientation and environment
        row[n++]='M';
        n+=fmtf(row+n,mx); row[n++]=' ';
        n+=fmtf(row+n,my); row[n++]=' ';
        n+=fmtf(row+n,mz); row[n]=0; orow(18,row);

        // Row 27: Lidar distance (mm) + last range status byte
        // Status: 9=valid/phase, 6=valid/noamb, 8=valid/wrap; 7=hold-last; others=invalid
        sprintf(row,"D:%4dmm S:%02X",dist,lidar.lastStatus()); orow(27,row);

        // Row 36: Encoder last events (held 250 ms each)
        sprintf(row,"E1:%-3s E2:%-3s",e1s,e2s); orow(36,row);

        // Row 45: Microphone peak bar — 19 segments, mapped from 0.0–1.0 amplitude
        { int f=(int)(amp*19); if(f>19)f=19;
          row[0]='['; for(int i=0;i<19;i++) row[1+i]=(i<f)?'#':'.'; row[20]=']'; row[21]=0; }
        orow(45,row);

        // Row 54: FSR (0–4095 ADC) and SD card status
        sprintf(row,"F:%4d SD:%s",fsr,sd_ok?"OK":"--"); orow(54,row);

        display.Update();

        System::Delay(20); // ~20 ms floor — actual loop is longer due to sensor reads
    }
}
