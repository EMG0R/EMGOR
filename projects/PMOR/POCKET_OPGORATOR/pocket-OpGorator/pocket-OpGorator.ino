/*
 * ================================================================
 * Pocket OpGorator
 * EMGOR SYNTH | Daisy Seed (STM32H750, PCM3060 codec)
 * ================================================================
 *
 * ARDUINO IDE SETUP:
 *   Board    : Daisy Seed  (electro-smith/arduino-daisy package)
 *   Optimize : Fast (-O1)
 *   USB Type : USB MIDI + Audio (composite device — set in board manager)
 *
 * LIBRARIES REQUIRED:
 *   DaisyDuino (electrosmith)
 *   Adafruit_GFX + Adafruit_SSD1306
 *   Adafruit_NeoTrellis + Adafruit_seesaw
 *   bolderflight/MPU9250
 *   SparkFun VL53L1X Arduino Library  ← NOT VL53L0X
 *   Encoder (Paul Stoffregen)
 *   Bounce2
 *   SD (built-in Arduino)
 *
 * PIN MAP (D-numbers match PCB net labels throughout):
 *   D0  → ENC1_A        Left encoder phase A   (left col 1st from bottom)
 *   D1  → ENC1_B        Left encoder phase B   (left col 2nd from bottom)
 *   D2  → ENC1_SW       Left encoder click     (left col 3rd from bottom, INPUT_PULLUP)
 *   D3  → OLED_DC       SPI1 data/command select (left col 4th from bottom)
 *   D4  → OLED_RST      OLED hardware reset    (left col 5th from bottom)
 *   D5  → unused        (left col 6th from bottom)
 *   D6  → unused        (left col 7th from bottom)
 *   D7  → OLED_CS       SPI1 OLED chip-select
 *   D8  → SPI_SCK       SPI1 clock  (OLED + SD shared)
 *   D9  → SPI_MISO      SPI1 MISO   (SD only)
 *   D10 → SPI_MOSI      SPI1 MOSI   (OLED + SD shared)
 *   D11 → I2C1_SCL      Wire → NeoTrellis seesaw
 *   D12 → I2C1_SDA      Wire → NeoTrellis seesaw
 *   D13 → I2C4_SCL      Wire1 → MPU9250 (0x68) + VL53L1X (0x29)
 *   D14 → I2C4_SDA      Wire1 → MPU9250 (0x68) + VL53L1X (0x29)
 *   D15 (A0) → FSR      Force-sensitive resistor (12-bit ADC)
 *   D16 (A1) → ENC2_B   Right encoder phase B
 *   D17 (A2) → ENC2_SW  Right encoder click (INPUT_PULLUP)
 *   D18 → unused        (PA7 — right col pin 25; same MCU pin as D10/SPI_MOSI)
 *   D19 → unused        (PA6 — right col pin 26; same MCU pin as D9/SPI_MISO)
 *   D20 (A5) → VBAT     Battery monitor — 100K/100K divider from TP4056 OUT+
 *   D21 → unused        (PC4 — right col pin 28; freed from SD_CS)
 *   D22 → unused        (PA5 — right col pin 29; same MCU pin as D8/SPI_SCK)
 *   D23 → unused        (PA4 — right col pin 30; DO NOT USE — same MCU pin as D7/OLED_CS)
 *   D24 → ENC2_A        Right encoder phase A (right col pin 31 / PA1)
 *   D25 → SD_MISO       Soft-SPI data in    (right col 9th from bottom)
 *   D26 → SD_CLK        Soft-SPI clock      (right col 8th from bottom)
 *   D27 → SD_MOSI       Soft-SPI data out   (right col 7th from bottom)
 *   D28 → SD_CS         Soft-SPI chip-select (right col 6th from bottom — lowest)
 *
 * AUDIO:  Built-in PCM3060 codec, 48 kHz / 24-bit stereo, AC-coupled.
 *         No external I2S codecs, no coupling caps needed.
 * USB:    Composite Audio + MIDI device via Daisy Seed micro USB port (external
 *         USB-C cable plugs into adapter, which plugs into the Daisy micro-USB).
 * SD:     External SPI module (WWZMDiB 6-pin, soft-SPI MISO=D25/CLK=D26/MOSI=D27/CS=D28). NOT built-in SDIO.
 *
 * Wire1 note: arduino-daisy maps Wire1 to I2C4 (D13/D14) on Daisy Seed.
 * If Wire1 is unavailable on your core version, move MPU9250 and VL53L1X
 * to Wire (I2C1) — no address conflicts with NeoTrellis (0x2E).
 * ================================================================
 */

#include "DaisyDuino.h"
#include <Wire.h>               // I2C1 — NeoTrellis  (SCL=D11, SDA=D12)
#include <SPI.h>                // SPI1 — OLED only   (SCK=D8, MISO=D9, MOSI=D10)
#include <SdFat_Adafruit_Fork.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>   // SSD1309 is SSD1306-compatible in SPI mode
#include <Adafruit_NeoTrellis.h>
#include "MPU9250.h"            // bolderflight/MPU9250
#include <SparkFun_VL53L1X.h>   // SparkFun VL53L1X — NOT VL53L0X
#include <Encoder.h>
#include <Bounce2.h>

// ================================================================
// HARDWARE
// ================================================================
DaisyHardware hw;
MidiUsbHandler midi;

// ================================================================
// PIN DEFINITIONS
// ================================================================
#define PIN_SD_MISO  25   // D25 = PA0  = right col 9th from bottom
#define PIN_SD_CLK   26   // D26 = PD11 = right col 8th from bottom
#define PIN_SD_MOSI  27   // D27 = PG9  = right col 7th from bottom
#define PIN_SD_CS    28   // D28 = PA2  = right col 6th from bottom (lowest)
#define PIN_ENC1_A   0    // left col 1st from bottom — any GPIO ✓
#define PIN_ENC1_B   1    // left col 2nd from bottom — any GPIO ✓
#define PIN_ENC1_SW  2    // left col 3rd from bottom — any GPIO ✓
#define PIN_OLED_DC  3    // left col 4th from bottom — any GPIO ✓
#define PIN_OLED_RST 4    // left col 5th from bottom — any GPIO ✓
#define PIN_ENC2_A   24   // D24 = PA1 = right col pin 31
#define PIN_OLED_CS  7
// D8  = SPI_SCK  }
// D9  = SPI_MISO } hardware SPI — no defines needed
// D10 = SPI_MOSI }
// D11 = I2C1_SCL (Wire)  }
// D12 = I2C1_SDA (Wire)  } set by Wire.begin()
// D13 = I2C4_SCL (Wire1) }
// D14 = I2C4_SDA (Wire1) } set by Wire1.begin()
#define PIN_FSR        A0    // D15 — 12-bit ADC
#define PIN_ENC2_B     A1    // D16
#define PIN_ENC2_SW    A2    // D17
// D18 (right col pin 25) and D19 (pin 26) are duplicate pads of SPI1 MOSI/MISO —
// not used in firmware. D21 (pin 28) and D22 (pin 29) freed from old SD assignments.
#define PIN_VBAT       A5    // D20 — 100K/100K divider

// Sample-rate constant shared by the clock/PO-sync engine below. Everything
// tick-timed derives from the audio callback's sample counter, not millis().
#define AUDIO_SR_HZ 48000.0f

// ================================================================
// SD CARD  (soft-SPI: MISO=D25, CLK=D26, MOSI=D27, CS=D28)
// Right col bottom→up: CS(6th=lowest), MOSI(7th), CLK(8th), MISO(9th)
// ================================================================
SoftSpiDriver<PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CLK> sdSpi;
SdFat32 sdFat;

// ================================================================
// DISPLAY  (SSD1309 128×64, SPI, SSD1306-compatible)
// ================================================================
#define SCREEN_W 128
#define SCREEN_H  64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &SPI,
                         PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);

// ================================================================
// NEOTRELLIS  (Wire = I2C1, address 0x2E)
// ================================================================
Adafruit_NeoTrellis trellis;

// ================================================================
// SENSORS  (Wire1 = I2C4, SCL=D13 SDA=D14)
// ================================================================
bfs::Mpu9250 mpu(&Wire1, bfs::Mpu9250::I2C_ADDR_PRIM);  // 0x68
SFEVL53L1X   lidar(Wire1);                               // 0x29
bool mpuOk   = false;
bool lidarOk = false;

// ================================================================
// CONTROLS
// ================================================================
Encoder enc1(PIN_ENC1_A, PIN_ENC1_B);
Encoder enc2(PIN_ENC2_A, PIN_ENC2_B);
Bounce  btn1, btn2;

// ================================================================
// AUDIO  (float samples in [-1, 1], 48 kHz stereo)
// ================================================================
// gain still updated by the volume encoder / controller mode so the rest of
// the firmware ("same stuff in background") keeps working — just not applied
// to the audio path while we're in raw passthrough.
static volatile float gainL = 1.0f;
static volatile float gainR = 1.0f;

// --- DSP DISABLED: raw input → output passthrough, no processing ---
// 1-pole speaker HPF commented out (re-enable when synthesis returns).
// static const float HPF_A = 0.9683f;
// static float hpXpL = 0.f, hpYpL = 0.f;
// static float hpXpR = 0.f, hpYpR = 0.f;

// ================================================================
// CLOCK + PO SYNC ENGINE — audio-callback-visible state
// ================================================================
// SY0-SY5 per pocket-OpGorator.md "PO / Volca Sync":
//   SY0 = normal stereo audio, no sync
//   SY1 = PO-sync OUT click on L (Daisy AUDIO_OUT_L), mono audio on R
//   SY2 = PO-sync OUT click on R, mono audio on L
//   SY3 = PO-sync IN edge-detect on L (Daisy AUDIO_IN_L), mono audio on R
//   SY4 = PO-sync IN edge-detect on R, mono audio on L
//   SY5 = passthrough: sync repeater on L, mono audio chain on R
// Any mode other than SY0/SY5 forces mono audio (per doc).
enum SyncMode : uint8_t { SY0_STEREO = 0, SY1_OUT_L, SY2_OUT_R, SY3_IN_L, SY4_IN_R, SY5_PASS };
volatile SyncMode syncMode = SY0_STEREO;   // no persistence mechanism exists yet -> boot default SY0

// MIDI clock role. Default OPG_MASTER: OpGorator is master when attached
// (matches the Demiurge-side default in the integration design doc).
enum ClockRole : uint8_t { ROLE_OPG_MASTER = 0, ROLE_DEMIURGE_MASTER = 1 };
volatile ClockRole clockRole = ROLE_OPG_MASTER;

// Shared tempo, float for sub-BPM precision in tick/click scheduling.
// Mirrored into the existing uint8_t `bpm` (STATE section below) for display
// and for the encoder-2 UI, which keeps working exactly as it did before.
volatile float bpmF = 120.0f;

// Free-running sample counter, advanced by AudioCallback's block size only.
// Every tick/click/edge timestamp below is derived from this — never from
// millis() — so timing survives loop() jitter and stays sample-accurate.
volatile uint32_t sampleClock = 0;

// ---- PO sync OUT: click render (SY1/SY2 master, SY5 pass repeater) ----
static const float    CLICK_LEVEL       = 0.8f;                            // ~0.8 FS unipolar
static const uint32_t CLICK_LEN_SAMPLES = (uint32_t)(0.0025f * AUDIO_SR_HZ); // ~2.5ms
static double          poPhaseSamples   = 0.0;  // running phase within one 8th note, in samples
volatile uint32_t       poClickCounter  = 0;     // >0 while a click is being rendered (counts down)
volatile bool            poRepeaterFire = false; // SY5: re-emit a click on OUT L for the IN L edge just seen

// ---- PO sync IN: rising-edge detect (SY3/SY4/SY5) ----
static const float    SYNC_IN_THRESHOLD  = 0.25f;                            // rising-edge threshold, FS
static const uint32_t SYNC_IN_REFRACTORY = (uint32_t)(0.010f * AUDIO_SR_HZ); // 10ms refractory
static bool     syncInArmed         = true;  // re-armed once signal drops back below half-threshold
static uint32_t syncInRefractoryCtr = 0;
static uint32_t syncInLastEdgeSample = 0;
static bool     syncInHaveLastEdge   = false;

void AudioCallback(float **in, float **out, size_t size) {
    // Snapshot bpm once per block — a 32-bit float load/store is a single
    // atomic bus transaction on Cortex-M7, so a torn read here isn't a risk;
    // this just avoids the tempo changing mid-block underneath the loop.
    float  bpmNow          = bpmF;
    double samplesPerEighth = (30.0 * (double)AUDIO_SR_HZ) / (double)bpmNow; // 2 PPQN
    SyncMode mode = syncMode;

    for (size_t i = 0; i < size; i++) {
        float inL = in[0][i];
        float inR = in[1][i];

        // ---- PO-sync IN: rising-edge detect (SY3/SY4/SY5) ----
        bool  doEdgeDetect = (mode == SY3_IN_L || mode == SY4_IN_R || mode == SY5_PASS);
        float syncInSample = (mode == SY4_IN_R) ? inR : inL; // SY3/SY5 watch L, SY4 watches R
        if (doEdgeDetect) {
            if (syncInRefractoryCtr > 0) {
                syncInRefractoryCtr--;
            } else if (syncInArmed && syncInSample >= SYNC_IN_THRESHOLD) {
                syncInArmed         = false;
                syncInRefractoryCtr = SYNC_IN_REFRACTORY;
                uint32_t nowSample  = sampleClock + (uint32_t)i;
                if (syncInHaveLastEdge) {
                    uint32_t interval = nowSample - syncInLastEdgeSample;
                    if (interval > 0) {
                        // 2 PPQN pulse interval -> bpm: bpm = 60*SR/(interval*2)
                        float measuredBpm = (30.0f * AUDIO_SR_HZ) / (float)interval;
                        if (measuredBpm >= 20.0f && measuredBpm <= 400.0f) {
                            bpmF = measuredBpm;
                        }
                    }
                }
                syncInLastEdgeSample = nowSample;
                syncInHaveLastEdge   = true;
                poPhaseSamples = 0.0;              // pulse advances the clock phase by an 8th note
                if (mode == SY5_PASS) poRepeaterFire = true; // SY5: repeat the pulse on OUT L
            } else if (syncInSample < SYNC_IN_THRESHOLD * 0.5f) {
                syncInArmed = true; // simple hysteresis rearm
            }
        }

        // ---- PO-sync OUT: click phase/render (SY1/SY2 master, SY5 repeater) ----
        bool clickOutModeActive = (mode == SY1_OUT_L || mode == SY2_OUT_R);
        if (clickOutModeActive) {
            poPhaseSamples += 1.0;
            if (poPhaseSamples >= samplesPerEighth) {
                poPhaseSamples -= samplesPerEighth;
                poClickCounter = CLICK_LEN_SAMPLES;
            }
        }
        if (poRepeaterFire) {
            poClickCounter  = CLICK_LEN_SAMPLES;
            poRepeaterFire  = false;
        }
        float clickSample = 0.0f;
        if (poClickCounter > 0) {
            clickSample = CLICK_LEVEL;
            poClickCounter--;
        }

        // ---- Output routing per SY mode ----
        switch (mode) {
            case SY1_OUT_L: {
                float mono = 0.5f * (inL + inR);
                out[0][i] = clickSample;
                out[1][i] = mono;
                break;
            }
            case SY2_OUT_R: {
                float mono = 0.5f * (inL + inR);
                out[0][i] = mono;
                out[1][i] = clickSample;
                break;
            }
            case SY3_IN_L: // click consumed IN L, audio passes from IN R (mono, both outs)
                out[0][i] = inR;
                out[1][i] = inR;
                break;
            case SY4_IN_R: // click consumed IN R, audio passes from IN L (mono, both outs)
                out[0][i] = inL;
                out[1][i] = inL;
                break;
            case SY5_PASS:
                out[0][i] = clickSample; // sync repeater out
                out[1][i] = inR;         // mono audio chain
                break;
            case SY0_STEREO:
            default:
                out[0][i] = inL;
                out[1][i] = inR;
                break;
        }
    }

    sampleClock += (uint32_t)size;

    // --- DSP / gain / HPF path (disabled) ---
    // float gL = gainL, gR = gainR;
    // for (size_t i = 0; i < size; i++) {
    //     float L = in[0][i] * gL;
    //     float R = in[1][i] * gR;
    //     // HPF: y[n] = α * (y[n-1] + x[n] - x[n-1])
    //     float hL = HPF_A * (hpYpL + L - hpXpL);  hpXpL = L;  hpYpL = hL;
    //     float hR = HPF_A * (hpYpR + R - hpXpR);  hpXpR = R;  hpYpR = hR;
    //     out[0][i] = hL;
    //     out[1][i] = hR;
    // }
}

// ================================================================
// MIDI HELPERS
// ================================================================
static void midiNoteOn(uint8_t note, uint8_t vel) {
    uint8_t m[3] = { 0x90, note, vel };
    midi.SendMessage(m, 3);
}
static void midiNoteOff(uint8_t note) {
    uint8_t m[3] = { 0x80, note, 0 };
    midi.SendMessage(m, 3);
}
static void midiCC(uint8_t cc, uint8_t val) {
    uint8_t m[3] = { 0xB0, cc, val };
    midi.SendMessage(m, 3);
}
static void midiAftertouch(uint8_t pressure) {
    uint8_t m[2] = { 0xD0, pressure };
    midi.SendMessage(m, 2);
}
// Single raw status byte (0xF8 clock, 0xFA start, 0xFC stop, ...). USB-MIDI
// packs this as CIN 0x05 "single byte" (see libDaisy hid/usb_midi.cpp,
// MidiToUsbSingle: any status >= 0xF4 with size==1 takes that path) — same
// SendMessage() call already used above for 2/3-byte messages.
static void midiRealtime(uint8_t status) {
    uint8_t m[1] = { status };
    midi.SendMessage(m, 1);
}
// Control Change on MIDI channel 16 (0-based channel nibble 0xF -> status 0xBF).
static void midiCC16(uint8_t cc, uint8_t val) {
    uint8_t m[3] = { (uint8_t)(0xB0 | 0x0F), cc, val };
    midi.SendMessage(m, 3);
}

// ================================================================
// STATE
// ================================================================
const uint8_t noteMap[16] = {
    48, 49, 50, 51,
    52, 53, 54, 55,
    56, 57, 58, 59,
    60, 61, 62, 63
};
const uint32_t COL_IDLE      = 0x140028;  // dim purple
const uint32_t COL_ACTIVE    = 0x0096FF;  // bright blue
const uint32_t COL_CTRL_IDLE = 0x282828;  // dim white

int32_t  enc1Last = 0, enc2Last = 0;
uint8_t  masterVol    = 100;
uint8_t  bpm          = 120;
uint8_t  batPct       = 100;
uint8_t  lastPressure = 0;
unsigned long batLast  = 0;
unsigned long lidarLast = 0;
bool controllerMode = false;
bool sdOk           = false;

// ---- UI pages ----
enum UiPage { PAGE_STATUS, PAGE_SYNC };
UiPage currentPage = PAGE_STATUS;

// ================================================================
// CLOCK ENGINE STATE (non-audio: MIDI tick scheduling + follower PLL)
// syncMode / clockRole / bpmF / sampleClock live above, next to
// AudioCallback, since the audio path reads them directly.
// ================================================================
static double        nextMidiTickSample = 0.0;   // sampleClock threshold for next 0xF8
static bool           midiTickArmed     = false;
static bool            seqRunning       = true;   // sequencer transport stub (no sequencer yet)
static bool             seqRunningPrev  = true;
static unsigned long     roleAnnounceLast = 0;    // ~2s CC116/CC119 announce cadence (master)

// Follower PLL: sliding window of raw incoming-tick intervals, in samples.
#define TICK_PLL_WINDOW 24
static uint32_t tickIntervals[TICK_PLL_WINDOW];
static uint8_t  tickIntervalCount = 0;
static uint8_t  tickIntervalIdx   = 0;
static uint32_t tickIntervalSum   = 0;
static uint32_t lastRxTickSample  = 0;
static bool     haveLastRxTick    = false;
static unsigned long lastRxTickMillis = 0; // wall-clock only used for the >2s freewheel gap check

// ================================================================
// TRELLIS CALLBACK
// ================================================================
TrellisCallback onKey(keyEvent evt) {
    uint8_t key = evt.bit.NUM;
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        // 12-bit FSR: threshold at ~4% (160/4095) mirrors original 40/1023 ratio
        int raw = analogRead(PIN_FSR);
        uint8_t vel = (raw < 160) ? 1 : (uint8_t)map(raw, 160, 4095, 1, 127);
        midiNoteOn(noteMap[key], vel);
        trellis.pixels.setPixelColor(key, controllerMode ? 0xFFFFFF : COL_ACTIVE);
    } else {
        midiNoteOff(noteMap[key]);
        trellis.pixels.setPixelColor(key, controllerMode ? COL_CTRL_IDLE : COL_IDLE);
    }
    trellis.pixels.show();
    return 0;
}

// ================================================================
// SETUP
// ================================================================
void setup() {
    // Daisy 12-bit ADC (0–4095)
    analogReadResolution(12);

    // ---- Daisy audio init ----
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    DAISY.begin(AudioCallback);

    // ---- USB MIDI ----
    MidiUsbHandler::Config mcfg;
    midi.Init(mcfg);
    midi.StartReceive();

    // ---- Clock engine: announce role on boot (default OpGorator master) ----
    midiCC16(116, clockRole == ROLE_OPG_MASTER ? 1 : 2);
    roleAnnounceLast = millis();
    seqRunningPrev   = seqRunning;

    // ---- OLED (SPI) ----
    SPI.begin();
    // SSD1309 uses external VCC supply — use EXTERNALVCC (no internal charge pump cmd)
    // SSD1306_SWITCHCAPVCC works on the SSD1309 (the SSD1306 charge-pump enable
    // command is silently ignored by the SSD1309 controller; the 12V OLED VCC is
    // generated by the onboard boost on the DWEII module). Community-verified.
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x00)) {
        // OLED init failed — verify D3 DC, D4 RST, D7 CS, D8 SCK, D10 MOSI
        while (1);
    }
    drawSplash();

    // ---- NeoTrellis (Wire = I2C1: SCL=D11, SDA=D12) ----
    Wire.begin();
    Wire.setClock(400000);
    if (!trellis.begin()) {
        // NeoTrellis init failed — verify 5V supply and D11/D12 connections
        while (1);
    }
    for (int i = 0; i < 16; i++) {
        trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING,  true);
        trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING, true);
        trellis.registerCallback(i, onKey);
        trellis.pixels.setPixelColor(i, COL_IDLE);
    }
    // Cap at 80% (204/255) — keeps NeoTrellis within 2A rail budget
    trellis.pixels.setBrightness(204);
    trellis.pixels.show();

    // ---- Sensors (Wire1 = I2C4: SCL=D13, SDA=D14) ----
    Wire1.begin();
    Wire1.setClock(400000);

    mpuOk = mpu.Begin();
    if (mpuOk) {
        mpu.ConfigAccelRange(bfs::Mpu9250::ACCEL_RANGE_4G);
        mpu.ConfigGyroRange(bfs::Mpu9250::GYRO_RANGE_500DPS);
        mpu.ConfigSrd(19);  // 1 kHz / (19+1) = 50 Hz update rate
        // After full assembly: run hard-iron calibration sketch; store offsets to SD.
        // Nearby speaker magnets and XL3608 inductor will cause heading offsets.
    }

    lidarOk = (lidar.begin() == 0);
    if (lidarOk) {
        lidar.startRanging();  // default long mode: up to ~4 m
    }

    // ---- Encoder switches ----
    btn1.attach(PIN_ENC1_SW, INPUT_PULLUP);
    btn2.attach(PIN_ENC2_SW, INPUT_PULLUP);
    btn1.interval(5);
    btn2.interval(5);

    // ---- SD card (soft-SPI, CS=D25) ----
    sdOk = sdFat.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &sdSpi));

    updateDisplay();
}

// ================================================================
// LOOP
// ================================================================
void loop() {
    // ---- USB MIDI receive (process incoming clock, notes, etc.) ----
    midi.Listen();
    clockEngineUpdate();

    // ---- NeoTrellis ----
    trellis.read();

    // ---- Battery monitor (every 2 s) ----
    if (millis() - batLast > 2000) {
        batLast = millis();
        // 100K+100K divider halves VBAT. Daisy 12-bit ADC: 3.3V = 4095.
        // 4.2V → 2.1V → ADC 2606.  3.0V → 1.5V → ADC 1861.
        int raw = analogRead(PIN_VBAT);
        batPct = (uint8_t)constrain(map(raw, 1861, 2606, 0, 100), 0, 100);
    }

    // ---- FSR → continuous channel aftertouch (smoothed, 12-bit) ----
    {
        static float smooth = 0.f;
        int raw = analogRead(PIN_FSR);
        float target = (raw < 160) ? 0.f : (float)map(raw, 160, 4095, 0, 127);
        smooth += 0.2f * (target - smooth);
        uint8_t pressure = (uint8_t)constrain((int)smooth, 0, 127);
        if (pressure != lastPressure) {
            midiAftertouch(pressure);
            lastPressure = pressure;
        }
    }

    // ---- VL53L1X → MIDI CC74 (filter cutoff theremin), ~30 Hz ----
    if (lidarOk && millis() - lidarLast > 33) {
        lidarLast = millis();
        if (lidar.checkForDataReady()) {
            int16_t dist = lidar.getDistance();
            lidar.clearInterrupt();
            // 40mm blind spot per ACEIRMC TOF400C datasheet (the published 0-4cm region
            // returns unreliable readings; below that the device clamps to zero).
            if (dist > 40 && dist < 1300) {
                // Close hand = high cutoff, far = low cutoff
                midiCC(74, (uint8_t)map(dist, 40, 1300, 127, 0));
            }
        }
    }

    // ---- MPU9250 tilt → MIDI CC1 (mod wheel), 50 Hz via ConfigSrd ----
    if (mpuOk && mpu.Read()) {
        // Tilt on X axis: ±4g range → map to 0-127
        // Deadband ±0.5 g (±4.9 m/s²) in the middle to suppress noise
        float ax = mpu.accel_x_mps2();
        static float axSmooth = 0.f;
        axSmooth += 0.15f * (ax - axSmooth);
        if (fabsf(axSmooth) > 4.9f) {
            midiCC(1, (uint8_t)constrain(map((int)(axSmooth * 100), -3920, 3920, 0, 127), 0, 127));
        }
    }

    // ---- Encoder 1 → master volume (CC7), or SY sync mode on the SYNC page ----
    {
        int32_t e = enc1.read() / 4;
        if (e != enc1Last) {
            int8_t d = (int8_t)(e - enc1Last);
            enc1Last = e;
            if (currentPage == PAGE_SYNC) {
                int m = ((int)syncMode + d) % 6;
                if (m < 0) m += 6;
                syncMode = (SyncMode)m;
            } else {
                masterVol = (uint8_t)constrain((int)masterVol + d, 0, 127);
                if (!controllerMode) {
                    float v = masterVol / 127.0f;
                    gainL = v;
                    gainR = v;
                }
                midiCC(7, masterVol);
            }
            updateDisplay();
        }
    }

    // ---- Encoder 2 → BPM (master only; follower tempo comes from the bus/PO sync in) ----
    {
        int32_t e = enc2.read() / 4;
        if (e != enc2Last) {
            int8_t d = (int8_t)(e - enc2Last);
            enc2Last = e;
            if (clockRole == ROLE_OPG_MASTER) {
                bpm  = (uint8_t)constrain((int)bpm + d, 20, 240);
                bpmF = (float)bpm;
                midiTickArmed = false; // re-derive tick phase from the new tempo immediately
            }
            updateDisplay();
        }
    }

    // ---- Encoder buttons ----
    btn1.update();
    btn2.update();
    if (btn1.fell() && !btn2.read()) {
        toggleControllerMode();
    } else if (btn2.fell() && !btn1.read()) {
        toggleControllerMode();
    } else {
        if (btn1.fell()) {
            // ENC1 solo: menu enter/cycle — STATUS <-> SYNC page
            currentPage = (currentPage == PAGE_STATUS) ? PAGE_SYNC : PAGE_STATUS;
            updateDisplay();
        }
        if (btn2.fell()) {
            // ENC2 solo: on the SYNC page, flip clock role; elsewhere, start/stop transport
            if (currentPage == PAGE_SYNC) toggleClockRoleLocal();
            else                          toggleTransport();
        }
    }
}

// ================================================================
// CONTROLLER MODE TOGGLE
// ================================================================
void toggleControllerMode() {
    controllerMode = !controllerMode;
    if (controllerMode) {
        gainL = 0.f;
        gainR = 0.f;
        for (int i = 0; i < 16; i++)
            trellis.pixels.setPixelColor(i, 0xFFFFFF);
        trellis.pixels.show();
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(4,  8);  display.print("CONTROLLER");
        display.setCursor(28, 32); display.print("MODE");
        display.display();
        delay(600);
        for (int i = 0; i < 16; i++)
            trellis.pixels.setPixelColor(i, COL_CTRL_IDLE);
        trellis.pixels.show();
    } else {
        float v = masterVol / 127.0f;
        gainL = v;
        gainR = v;
        for (int i = 0; i < 16; i++)
            trellis.pixels.setPixelColor(i, 0xFFFFFF);
        trellis.pixels.show();
        delay(300);
        for (int i = 0; i < 16; i++)
            trellis.pixels.setPixelColor(i, COL_IDLE);
        trellis.pixels.show();
        updateDisplay();
    }
}

// ================================================================
// CLOCK ENGINE — MIDI clock master/follower, role negotiation
// ================================================================
static void setClockRole(ClockRole newRole, bool announce) {
    clockRole = newRole;
    if (announce) {
        midiCC16(116, newRole == ROLE_OPG_MASTER ? 1 : 2);
    }
    roleAnnounceLast = millis(); // re-sync the ~2s announce cadence on a fresh claim
}

// Local menu flip: OPG <-> DEMIURGE. Always announces CC116 (per design doc
// §1: "the side whose menu flips the role sends CC116 on the bus").
void toggleClockRoleLocal() {
    ClockRole newRole = (clockRole == ROLE_OPG_MASTER) ? ROLE_DEMIURGE_MASTER : ROLE_OPG_MASTER;
    setClockRole(newRole, true);
    if (newRole == ROLE_DEMIURGE_MASTER) {
        haveLastRxTick    = false;
        tickIntervalCount = 0;
        tickIntervalIdx   = 0;
        tickIntervalSum   = 0;
        midiTickArmed     = false;
    } else {
        midiTickArmed = false; // re-derive tick-emission phase from "now" on reclaim
    }
    updateDisplay();
}

void toggleTransport() {
    seqRunning = !seqRunning;
    updateDisplay();
}

// Push one measured tick interval (samples) into the follower PLL window
// and recompute bpmF once the window (>=24 ticks) is full.
static void pushTickInterval(uint32_t interval) {
    // Reject implausible intervals (glitches) outside 20..400 BPM at 24 PPQN.
    if (interval < (uint32_t)(AUDIO_SR_HZ * 60.0f / 400.0f / 24.0f)) return;
    if (interval > (uint32_t)(AUDIO_SR_HZ * 60.0f / 20.0f  / 24.0f)) return;

    if (tickIntervalCount < TICK_PLL_WINDOW) {
        tickIntervals[tickIntervalCount++] = interval;
        tickIntervalSum += interval;
    } else {
        tickIntervalSum -= tickIntervals[tickIntervalIdx];
        tickIntervals[tickIntervalIdx] = interval;
        tickIntervalSum += interval;
        tickIntervalIdx = (tickIntervalIdx + 1) % TICK_PLL_WINDOW;
    }
    if (tickIntervalCount >= TICK_PLL_WINDOW) {
        float avg = (float)tickIntervalSum / (float)TICK_PLL_WINDOW;
        bpmF = 120000.0f / avg;   // 120000 = 60 * SR / 24 PPQN
        bpm  = (uint8_t)constrain((int)(bpmF + 0.5f), 20, 240);
    }
}

// Drains the incoming MIDI queue (role/transport/tick bookkeeping) and, if
// this device is MASTER, emits the 24-PPQN clock + Start/Stop + periodic
// role/BPM announce. Call once per loop() iteration.
void clockEngineUpdate() {
    while (midi.HasEvents()) {
        MidiEvent e = midi.PopEvent();

        if (e.type == SystemRealTime) {
            if (clockRole == ROLE_DEMIURGE_MASTER && e.srt_type == TimingClock) {
                uint32_t now = sampleClock;
                if (haveLastRxTick && (millis() - lastRxTickMillis > 2000)) {
                    // Freewheel window exceeded — this tick starts a fresh
                    // PLL lock rather than extending the (stale) old one.
                    haveLastRxTick    = false;
                    tickIntervalCount = 0;
                    tickIntervalIdx   = 0;
                    tickIntervalSum   = 0;
                }
                if (haveLastRxTick) pushTickInterval(now - lastRxTickSample);
                lastRxTickSample = now;
                haveLastRxTick   = true;
                lastRxTickMillis = millis();
            } else if (e.srt_type == Start || e.srt_type == Continue) {
                seqRunning     = true;
                seqRunningPrev = true; // bus-driven; don't echo it back as a local edge
            } else if (e.srt_type == Stop) {
                seqRunning     = false;
                seqRunningPrev = false;
            }
        } else if (e.type == ControlChange && e.channel == 15) {
            ControlChangeEvent cc = e.AsControlChange();
            if (cc.control_number == 116) {
                if (cc.value == 1 && clockRole != ROLE_OPG_MASTER) {
                    setClockRole(ROLE_OPG_MASTER, false);
                    midiTickArmed = false;
                } else if (cc.value == 2 && clockRole != ROLE_DEMIURGE_MASTER) {
                    setClockRole(ROLE_DEMIURGE_MASTER, false);
                    haveLastRxTick    = false;
                    tickIntervalCount = 0;
                    tickIntervalIdx   = 0;
                    tickIntervalSum   = 0;
                }
            } else if (cc.control_number == 119 && clockRole == ROLE_DEMIURGE_MASTER) {
                // demiurge-clock.cpp:213 mapping: 40..400 BPM
                bpmF = 40.0f + (cc.value / 127.0f) * 360.0f;
                bpm  = (uint8_t)constrain((int)(bpmF + 0.5f), 20, 240);
            }
        }
    }
    // Freewheel (>2s without a tick while FOLLOWER) needs no explicit action:
    // pushTickInterval() simply stops being called, so bpmF (and everything
    // derived from it — PO sync out, the internal generator) free-runs at
    // the last locked tempo.

    if (clockRole == ROLE_OPG_MASTER) {
        if (seqRunning != seqRunningPrev) {
            midiRealtime(seqRunning ? 0xFA : 0xFC);
            seqRunningPrev = seqRunning;
            if (seqRunning) midiTickArmed = false; // resync tick phase to "now" on Start
        }

        uint32_t now = sampleClock;
        double samplesPerTick = ((double)AUDIO_SR_HZ * 60.0) / ((double)bpmF * 24.0);
        if (!midiTickArmed) {
            nextMidiTickSample = (double)now + samplesPerTick;
            midiTickArmed = true;
        }
        // Bounded catch-up in case loop() got delayed — never floods more
        // than a handful of ticks in one pass.
        int guard = 0;
        while ((double)now >= nextMidiTickSample && guard++ < 8) {
            if (seqRunning) midiRealtime(0xF8);
            nextMidiTickSample += samplesPerTick;
        }

        if (millis() - roleAnnounceLast > 2000) {
            roleAnnounceLast = millis();
            midiCC16(116, 1);
            midiCC16(119, (uint8_t)constrain((int)(((bpmF - 40.0f) / 360.0f) * 127.0f), 0, 127));
        }
    }
}

// ================================================================
// DISPLAY
// ================================================================
void drawSplash() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(14, 12); display.print("POCKET");
    display.setCursor(2,  36); display.print("opGorator");
    display.display();
    delay(1400);
}

const char* syncModeName(SyncMode m) {
    switch (m) {
        case SY0_STEREO: return "SY0 STEREO";
        case SY1_OUT_L:  return "SY1 OUT-L";
        case SY2_OUT_R:  return "SY2 OUT-R";
        case SY3_IN_L:   return "SY3 IN-L";
        case SY4_IN_R:   return "SY4 IN-R";
        case SY5_PASS:   return "SY5 PASS";
        default:         return "?";
    }
}

void drawSyncPage() {
    display.setTextSize(1);
    display.setCursor(0, 0);  display.print("SYNC");
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    display.setCursor(0, 16); display.print("SY MODE ");
    display.print(syncModeName(syncMode));

    display.setCursor(0, 28); display.print("CLOCK   ");
    display.print(clockRole == ROLE_OPG_MASTER ? "OPG MASTER" : "DEMIURGE");

    display.setCursor(0, 40); display.print("BPM     ");
    display.print((int)(bpmF + 0.5f));

    display.setCursor(0, 52); display.print("XPORT   ");
    display.print(seqRunning ? "RUN" : "STOP");

    display.display();
}

void updateDisplay() {
    if (controllerMode) return;
    display.clearDisplay();

    if (currentPage == PAGE_SYNC) {
        drawSyncPage();
        return;
    }

    // Top status bar
    display.setTextSize(1);
    display.setCursor(0,  0); display.print("V:"); display.print(masterVol);
    display.setCursor(36, 0); display.print("BPM:"); display.print(bpm);
    display.setCursor(84, 0);
    if (sdOk) display.print("SD ");
    display.print(batPct); display.print("%");

    display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // Volume bar
    display.setCursor(0, 13); display.print("V");
    display.fillRect(10, 13, map(masterVol, 0, 127, 0, 116), 5, SSD1306_WHITE);

    // Pressure bar
    display.setCursor(0, 21); display.print("P");
    display.fillRect(10, 21, map(constrain(lastPressure, 0, 127), 0, 127, 0, 116), 5, SSD1306_WHITE);

    display.drawFastHLine(0, 29, 128, SSD1306_WHITE);

    // 4×4 pad grid
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            display.drawRect(32 + c * 16, 33 + r * 8, 13, 6, SSD1306_WHITE);

    display.setCursor(0, 33); display.print("MIDI");
    display.setCursor(0, 41); display.print("CH 1");

    display.display();
}

// ================================================================
// DSP — RAW PASSTHROUGH (synthesis disabled)
// ================================================================
// AudioCallback (above) now routes input straight to output:
//   mic / line-in → DAC → PAM8403 + headphone jack   (no HPF, no gain, no DSP)
//
// SYNTHESIS COMMENTED OUT for this build. To bring it back:
//   - Implement here: synth oscillators, sample playback from SD, effects.
//   - SD sample playback: libDaisy's SdmmcHandler + WavPlayer, or Arduino SD + WAV parser.
//   - Session storage: save/load presets/patterns as files on SD (FAT32, SPI via CS=D0).
//   - Re-enable the gain + 250 Hz speaker HPF block inside AudioCallback above
//     (uncomment HPF_A / hpXp* state and the disabled processing loop).
