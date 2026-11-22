// I2C bus (D11 SCL pin 12, D12 SDA pin 13) + MPU6050 + VL53L0X for TEST 05. Main loop only.
// Each device is detected and retried independently: a missing sensor never blocks the other.
#pragma once
#include <math.h>
#include "bbus_v9.h"

// ---- ack-reporting transactions on top of the BBus primitives -----------------------
static BBus     g_bus;
static uint32_t g_i2c_err = 0;       // NACKs + clock-stretch timeouts since boot (all devices)
static bool i2cFin(bool ok) {
    if(g_bus.stretch_err) { g_bus.stretch_err = false; g_bus.recover(); ok = false; }
    if(!ok) g_i2c_err++;
    return ok;
}
static bool i2cProbe(uint8_t a) {                // address-only; does not count as an error
    g_bus.start(); bool ok = g_bus.byteW(a << 1); g_bus.stop(); g_bus.hd();
    if(g_bus.stretch_err) { g_bus.stretch_err = false; g_bus.recover(); return false; }
    return ok;
}
static bool i2cRd(uint8_t a, uint8_t reg, uint8_t* d, uint8_t n) {
    // write the register index, full STOP, then a fresh START for the read (Pololu/Arduino Wire semantics:
    // endTransmission() sends STOP). A repeated START read returned garbage ids from the VL53L0X.
    g_bus.start(); bool ok = g_bus.byteW(a << 1);
    ok = g_bus.byteW(reg) && ok;
    g_bus.stop(); g_bus.hd();
    g_bus.start(); ok = g_bus.byteW((a << 1) | 1) && ok;
    for(uint8_t i = 0; i < n; i++) d[i] = g_bus.byteR(i == n - 1);
    g_bus.stop(); g_bus.hd();
    return i2cFin(ok);
}
static bool i2cWr(uint8_t a, uint8_t reg, const uint8_t* d, uint8_t n) {
    g_bus.start(); bool ok = g_bus.byteW(a << 1);
    ok = g_bus.byteW(reg) && ok;
    for(uint8_t i = 0; i < n; i++) ok = g_bus.byteW(d[i]) && ok;
    g_bus.stop(); g_bus.hd();
    return i2cFin(ok);
}
static bool    w8(uint8_t a, uint8_t r, uint8_t v) { return i2cWr(a, r, &v, 1); }
static uint8_t r8(uint8_t a, uint8_t r) { uint8_t v = 0; i2cRd(a, r, &v, 1); return v; }

// ---- scan --------------------------------------------------------------------------
static uint8_t g_scan[16]; static uint8_t g_nscan = 0;
static void i2cScan() {
    g_nscan = 0;
    for(uint8_t a = 0x08; a < 0x78; a++)
        if(i2cProbe(a) && g_nscan < sizeof(g_scan)) g_scan[g_nscan++] = a;
}

// ---- MPU6050 @ 0x68 ----------------------------------------------------------------
enum DevState : uint8_t { DEV_MISSING = 0, DEV_OK = 1, DEV_ERROR = 2 };
static const uint8_t ADDR_IMU = 0x68, ADDR_LID = 0x29;
static DevState g_imu = DEV_MISSING;
static uint8_t  g_imu_who = 0;
static uint32_t g_imu_next = 0, g_imu_fail = 0;

static bool imuInit() {
    if(!i2cProbe(ADDR_IMU)) return false;
    w8(ADDR_IMU, 0x6B, 0x80); daisy::System::Delay(100);          // device reset
    w8(ADDR_IMU, 0x6B, 0x01);                                     // wake, PLL on gyro X
    daisy::System::Delay(10);
    g_imu_who = r8(ADDR_IMU, 0x75);                               // 0x68 genuine; 0x70-0x72/0x98 = clone/6500
    bool ok = true;
    ok &= w8(ADDR_IMU, 0x1A, 0x03);                               // DLPF 44 Hz accel / 42 Hz gyro, 1 kHz gyro rate
    ok &= w8(ADDR_IMU, 0x19, 9);                                  // sample rate 1000/(1+9) = 100 Hz
    ok &= w8(ADDR_IMU, 0x1B, 0x00);                               // gyro +-250 dps  (131 LSB/dps)
    ok &= w8(ADDR_IMU, 0x1C, 0x00);                               // accel +-2 g     (16384 LSB/g)
    return ok && g_imu_who != 0x00 && g_imu_who != 0xFF;
}

// ---- the VL53L1X (ACEIRMC TOF400C). There is exactly ONE lidar in this project. ----
// Driver = port of the Pololu VL53L1X Arduino library (VL53L1X.cpp/.h in fw/; ST API derived, see its header for licence).
// SHUT on D7 (pin 8, PG10), driven high to run. The unit stays at its power-up address 0x29: NO readdressing.
// SHORT distance mode, 50 ms budget, continuous ranging with a 50 ms inter-measurement period, data-ready polled (INT not wired).
// A missing or failed unit is retried every 2 s with a SHUT power cycle; the init waits are time-limited so a dead unit cannot hang the loop.
#include "VL53L1X.h"
extern "C" void pmor_usb_service(void) {}
static const uint8_t ADDR_L1X = 0x29;
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) {
    g_bus.start(); bool ok = g_bus.byteW(ADDR_L1X << 1);
    ok = g_bus.byteW(reg >> 8) && ok; ok = g_bus.byteW(reg & 0xFF) && ok;
    for(uint8_t i = 0; i < n; i++) ok = g_bus.byteW(d[i]) && ok;
    g_bus.stop(); g_bus.hd(); i2cFin(ok);
}
void vl53_read_reg(uint16_t reg, uint8_t* d, uint8_t n) {
    g_bus.start(); bool ok = g_bus.byteW(ADDR_L1X << 1);
    ok = g_bus.byteW(reg >> 8) && ok; ok = g_bus.byteW(reg & 0xFF) && ok;
    g_bus.stop(); g_bus.hd();
    g_bus.start(); ok = g_bus.byteW((ADDR_L1X << 1) | 1) && ok;
    for(uint8_t i = 0; i < n; i++) d[i] = g_bus.byteR(i == n - 1);
    g_bus.stop(); g_bus.hd(); i2cFin(ok);
}
struct Lid { VL53L1X drv; DevState st = DEV_MISSING; uint16_t model = 0; uint32_t next = 0, polls = 0, ready = 0; uint8_t r30 = 0, r31 = 0; };
static Lid g_lid;
static daisy::GPIO g_shut;
static void lidPower(bool on) { g_shut.Write(on); }
static bool lidInit() {                                          // powered and answering at 0x29 -> init and start ranging
    Lid& L = g_lid;
    daisy::System::Delay(5);                                     // >= 1.2 ms boot after SHUT high
    if(!i2cProbe(ADDR_L1X)) { L.st = DEV_MISSING; return false; }
    uint8_t b[2] = {0, 0}; vl53_read_reg(0x010F, b, 2); L.model = (uint16_t)(b[0] << 8 | b[1]);
    if(L.model != 0xEACC) { L.st = DEV_ERROR; return false; }
    L.drv.setTimeout(200);                                       // bounded waits inside init (boot status, calibration)
    if(!L.drv.init(true) || !L.drv.setDistanceMode(VL53L1X::Short) || !L.drv.setMeasurementTimingBudget(50000)) { L.st = DEV_ERROR; return false; }
    L.drv.startContinuous(50);
    L.st = DEV_OK; L.polls = 0; L.ready = 0;
    return true;
}
static void lidBringUp() { lidPower(false); daisy::System::Delay(10); lidPower(true); lidInit(); }
static void lidRetry(uint32_t now) {
    if(g_lid.st != DEV_OK && now >= g_lid.next) { g_lid.next = now + 2000; DevState w = g_lid.st; lidBringUp(); if(w != g_lid.st) g_cmd_hello = true; }
}

// ---- boot + poll -------------------------------------------------------------------
static void i2cBoot() {
    g_bus.init(daisy::seed::D11, daisy::seed::D12);
    g_shut.Init(daisy::seed::D7, daisy::GPIO::Mode::OUTPUT);
    lidPower(false);
    lidBringUp();
    i2cScan();
}
static void putd(int32_t v) { if(v < 0) { put('-'); v = -v; } putu((uint32_t)v); }

// Called every sample frame (5 ms): IMU on even frames at 100 Hz, lidar poll on odd frames (>= 33 Hz result rate).
static void i2cPoll(uint32_t now, uint32_t seq, uint32_t* rescan) {
    // (re)detect missing / failed devices every 2 s, each on its own
    if(g_imu != DEV_OK && now >= g_imu_next) {
        g_imu_next = now + 2000;
        DevState was = g_imu;
        if(i2cProbe(ADDR_IMU)) { g_imu = imuInit() ? DEV_OK : DEV_ERROR; g_imu_fail = 0; } else g_imu = DEV_MISSING;
        if(g_imu != was) *rescan = 1;
        return;                                              // init costs ms: skip the rest of this frame
    }
    lidRetry(now);
    if(g_imu == DEV_OK && (seq & 1) == 0) {
        uint8_t b[14];
        if(i2cRd(ADDR_IMU, 0x3B, b, 14)) {
            g_imu_fail = 0;
            int16_t v[7]; for(int i = 0; i < 7; i++) v[i] = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]);
#ifdef TEST_DELAY
            g_dly_ay = g_dly_ay + 0.25f * ((float)v[1] / 16384.0f - g_dly_ay);   // accel Y in g, smoothed at the sensor rate (100 Hz)
#endif
            float ax = v[0], ay = v[1], az = v[2];
            float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f;
            float roll  = atan2f(ay, az) * 57.29578f;
            ensure(200);
            puts_("{\"t\":\"imu\",\"ms\":"); putu(now);
            puts_(",\"a\":["); putd(v[0]); put(','); putd(v[1]); put(','); putd(v[2]);
            puts_("],\"g\":["); putd(v[4]); put(','); putd(v[5]); put(','); putd(v[6]);
            puts_("],\"tmp\":"); putd(v[3]);
            puts_(",\"pr\":["); putd((int32_t)(pitch * 100)); put(','); putd((int32_t)(roll * 100)); puts_("]}\n");
        } else if(++g_imu_fail >= 5) { g_imu = DEV_ERROR; g_imu_next = now + 2000; *rescan = 1; }
    } else if((seq & 3) == 1) {                              // lidar data-ready poll at ~50 Hz (INT pin not wired)
        Lid& L = g_lid;
        if(L.st == DEV_OK) {
            L.polls++;
            if((L.polls & 63) == 0) { uint8_t t = 0; vl53_read_reg(0x0031, &t, 1); L.r31 = t; vl53_read_reg(0x0030, &t, 1); L.r30 = t; }
            if(L.drv.dataReady()) {
                L.ready++;
                uint16_t mm = L.drv.read(false); uint8_t st = L.drv.rawStatus();      // raw ST status 9 = range valid
#ifdef TEST_DELAY
                g_lid_mm = (float)mm; g_lid_st = st; g_lid_ms = now;
#endif
                ensure(80);
                puts_("{\"t\":\"lid\",\"ms\":"); putu(now); puts_(",\"mm\":"); putu(mm); puts_(",\"st\":"); putu(st); puts_("}\n");
            }
        }
    }
}
static const char* devName(DevState s) { return s == DEV_OK ? "ok" : s == DEV_ERROR ? "error" : "missing"; }
static void putI2cHello() {
    puts_(",\"i2c\":{\"scl\":\"D11/12\",\"sda\":\"D12/13\",\"err\":"); putu(g_i2c_err);
    puts_(",\"scan\":["); for(int i = 0; i < g_nscan; i++) { if(i) put(','); putu(g_scan[i]); }
    puts_("],\"imu\":{\"s\":\""); puts_(devName(g_imu)); puts_("\",\"who\":"); putu(g_imu_who);
    puts_("},\"lid\":{\"s\":\""); puts_(devName(g_lid.st)); puts_("\",\"id\":"); putu(g_lid.model); puts_(",\"addr\":41,\"shut\":\"D7\",\"dbg\":[");
    putu(g_lid.polls); put(','); putu(g_lid.ready); put(','); putu(g_lid.r31); put(','); putu(g_lid.r30); puts_("]}}");
}
