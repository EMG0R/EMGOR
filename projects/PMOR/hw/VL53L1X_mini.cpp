#include "VL53L1X_mini.h"
#include "daisy_seed.h"

// Main-file USB service (tud_task + audio drain + MIDI clock). Every
// blocking spin in this driver must pump it — see init()/read().
extern "C" void pmor_usb_service(void);

using namespace daisy;

// ============================================================ register map
// Unchanged from the v2 driver except for the additions below — this block
// was already a faithful (if oddly-cased) copy of ST's public register map;
// the new constants at the end fill in the gaps needed for updateDSS(),
// setupManualCalibration() and the ROI (v2-audit.md §11).
static const uint16_t R_SOFT_RESET           = 0x0000;
static const uint16_t R_OSC_FAST_FREQ        = 0x0006;
static const uint16_t R_PAD_I2C_HV_EXTSUP    = 0x002E;
static const uint16_t R_GPIO_TIO_HV_STATUS   = 0x0031;
static const uint16_t R_ALGO_CROSSTALK_HMM   = 0x0039;
static const uint16_t R_ALGO_RANGE_IGN_HMM   = 0x003E;
static const uint16_t R_ALGO_RANGE_MIN_CLIP  = 0x003F;
static const uint16_t R_ALGO_CONSISTENCY     = 0x0040;
static const uint16_t R_DSS_TARGET_RATE      = 0x0024;
static const uint16_t R_DSS_APERTURE_ATT     = 0x0057;
static const uint16_t R_DSS_ROI_MODE         = 0x004F;
static const uint16_t R_DSS_MANUAL_SPADS     = 0x0054;   // DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT
static const uint16_t R_SYS_THRESH_HIGH      = 0x0050;
static const uint16_t R_SYS_THRESH_LOW       = 0x0052;
static const uint16_t R_SYS_SEQUENCE         = 0x0081;
static const uint16_t R_SYS_GPH              = 0x0082;
static const uint16_t R_SYS_INTERRUPT_CLEAR  = 0x0086;
static const uint16_t R_SYS_MODE_START       = 0x0087;
static const uint16_t R_SYS_SEED_CONFIG      = 0x0077;
static const uint16_t R_SYS_INTERMEAS_PERIOD = 0x006C;
static const uint16_t R_GPH0                 = 0x0071;
static const uint16_t R_GPH1                 = 0x007C;
static const uint16_t R_SD_QUANTIFIER        = 0x007E;
static const uint16_t R_SD_WOI_SD0           = 0x0078;
static const uint16_t R_SD_WOI_SD1           = 0x0079;
static const uint16_t R_SD_INIT_PHASE_SD0    = 0x007A;
static const uint16_t R_SD_INIT_PHASE_SD1    = 0x007B;
static const uint16_t R_RANGE_SIGMA_THRESH   = 0x0064;
static const uint16_t R_RANGE_MIN_COUNT      = 0x0066;
static const uint16_t R_RANGE_VCSEL_A        = 0x0060;
static const uint16_t R_RANGE_VCSEL_B        = 0x0063;
static const uint16_t R_RANGE_VALID_PHASE    = 0x0069;
static const uint16_t R_PHASECAL_TIMEOUT     = 0x004B;
static const uint16_t R_MM_TIMEOUT_A         = 0x005A;
static const uint16_t R_MM_TIMEOUT_B         = 0x005C;
static const uint16_t R_RANGE_TIMEOUT_A      = 0x005E;
static const uint16_t R_RANGE_TIMEOUT_B      = 0x0061;
static const uint16_t R_SIGMA_PULSE_WIDTH    = 0x0036;
static const uint16_t R_SIGMA_AMBIENT_WIDTH  = 0x0037;
static const uint16_t R_RESULT_OSC_CAL       = 0x00DE;
static const uint16_t R_FIRMWARE_STATUS      = 0x00E5;
static const uint16_t R_IDENTIFICATION       = 0x010F;
static const uint16_t R_ALGO_PART_OFFSET     = 0x001E;
static const uint16_t R_MM_OUTER_OFFSET      = 0x0022;
static const uint16_t R_RESULT_RANGE_STATUS  = 0x0089;   // burst base (17 bytes, see read())
static const uint16_t R_RESULT_DISTANCE      = 0x0096;
static const uint32_t TIMING_GUARD           = 4528;

// ---- additions for the §11 overhaul ---------------------------------------
static const uint16_t R_ROI_CENTRE_SPAD      = 0x007F;   // ROI_CONFIG__USER_ROI_CENTRE_SPAD
static const uint16_t R_ROI_XY_SIZE          = 0x0080;   // ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE
static const uint16_t R_VHV_LOOP_BOUND       = 0x0008;   // VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND
static const uint16_t R_VHV_INIT             = 0x000E;   // VHV_CONFIG__INIT
static const uint16_t R_CAL_VCSEL_START      = 0x0047;   // CAL_CONFIG__VCSEL_START
static const uint16_t R_PHASECAL_OVERRIDE    = 0x004D;   // PHASECAL_CONFIG__OVERRIDE
static const uint16_t R_PHASECAL_RESULT_VCSEL= 0x00D8;   // PHASECAL_RESULT__VCSEL_START (lower confidence —
                                                          // ST's public map is unambiguous through 0x00DE
                                                          // (RESULT__OSC_CALIBRATE_VAL, used above and unchanged);
                                                          // verify this one specific byte against a datasheet/
                                                          // known-good capture during hardware bring-up)

// 8x8 centre ROI (spec §11 item 5) — narrows the beam for hand tracking.
// Off switches back to the sensor's default full 16x16 SPAD array.
static constexpr bool kCentreRoi8x8 = true;

void VL53L1X_Mini::writeReg8(uint16_t reg, uint8_t val) {
    vl53_write_reg(reg, &val, 1);
}
void VL53L1X_Mini::writeReg16(uint16_t reg, uint16_t val) {
    uint8_t b[2] = {(uint8_t)(val>>8), (uint8_t)val};
    vl53_write_reg(reg, b, 2);
}
void VL53L1X_Mini::writeReg32(uint16_t reg, uint32_t val) {
    uint8_t b[4] = {(uint8_t)(val>>24),(uint8_t)(val>>16),(uint8_t)(val>>8),(uint8_t)val};
    vl53_write_reg(reg, b, 4);
}
uint8_t VL53L1X_Mini::readReg8(uint16_t reg) {
    uint8_t v = 0; vl53_read_reg(reg, &v, 1); return v;
}
uint16_t VL53L1X_Mini::readReg16(uint16_t reg) {
    uint8_t b[2] = {0,0}; vl53_read_reg(reg, b, 2);
    return ((uint16_t)b[0]<<8)|b[1];
}

bool VL53L1X_Mini::dataReady() {
    return (readReg8(R_GPIO_TIO_HV_STATUS) & 0x01) == 0;
}
uint32_t VL53L1X_Mini::calcMacroPeriod(uint8_t vcsel_period) {
    uint32_t pll = ((uint32_t)1<<30) / fast_osc_frequency_;
    uint8_t  vc  = (vcsel_period+1)<<1;
    uint32_t mp  = (uint32_t)2304 * pll; mp>>=6; mp*=vc; mp>>=6;
    return mp;
}
uint32_t VL53L1X_Mini::timeoutMicrosecondsToMclks(uint32_t us, uint32_t mp) {
    return (((uint32_t)us<<12)+(mp>>1))/mp;
}
uint16_t VL53L1X_Mini::encodeTimeout(uint32_t mclks) {
    if(!mclks) return 0;
    uint32_t ls=mclks-1; uint16_t ms=0;
    while((ls&0xFFFFFF00)>0){ls>>=1;ms++;}
    return (ms<<8)|(ls&0xFF);
}

// SHORT distance mode, ~33ms timing budget (was setTimingBudget50msLong /
// LONG). Spec §11 item 3: ST recommends SHORT under ~1.3m in ambient light —
// more tolerant phase-check behaviour at close range, which is the useful
// span for a theremin (5-60cm). VCSEL A 0x07 / B 0x05, valid phase 0x38,
// WOI 0x07/0x05, init phase 6/6 replace LONG's 0x0F/0x0D, 0xB8, 0x0F/0x0D,
// 14/14. TIMING_GUARD (4528) is carried over from the LONG-budget path —
// Pololu's real per-budget guard table differs slightly by budget/mode; this
// keeps the same macro-period math the rest of the driver already uses and
// lands close enough that the 33ms target is what matters (25Hz vs the old
// 10Hz at 100ms IM), not the exact guard constant.
void VL53L1X_Mini::setTimingBudget33msShort() {
    writeReg8(R_RANGE_VCSEL_A, 0x07); writeReg8(R_RANGE_VCSEL_B, 0x05);
    writeReg8(R_RANGE_VALID_PHASE, 0x38);
    writeReg8(R_SD_WOI_SD0, 0x07);  writeReg8(R_SD_WOI_SD1, 0x05);
    writeReg8(R_SD_INIT_PHASE_SD0, 6); writeReg8(R_SD_INIT_PHASE_SD1, 6);
    uint32_t rt = (33000 - TIMING_GUARD) / 2;
    uint32_t ma = calcMacroPeriod(readReg8(R_RANGE_VCSEL_A));
    uint32_t pc = timeoutMicrosecondsToMclks(1000, ma); if(pc>0xFF) pc=0xFF;
    writeReg8(R_PHASECAL_TIMEOUT, (uint8_t)pc);
    writeReg16(R_MM_TIMEOUT_A,    encodeTimeout(timeoutMicrosecondsToMclks(1, ma)));
    writeReg16(R_RANGE_TIMEOUT_A, encodeTimeout(timeoutMicrosecondsToMclks(rt, ma)));
    uint32_t mb = calcMacroPeriod(readReg8(R_RANGE_VCSEL_B));
    writeReg16(R_MM_TIMEOUT_B,    encodeTimeout(timeoutMicrosecondsToMclks(1, mb)));
    writeReg16(R_RANGE_TIMEOUT_B, encodeTimeout(timeoutMicrosecondsToMclks(rt, mb)));
}

bool VL53L1X_Mini::init() {
    model_id_ = readReg16(R_IDENTIFICATION);
    if(model_id_ != 0xEACC) return false;
    writeReg8(R_SOFT_RESET, 0x00); System::DelayUs(100);
    writeReg8(R_SOFT_RESET, 0x01); System::Delay(1);
    uint32_t t = System::GetNow();
    // This can spin up to 100ms of solid I2C polling. Keep the USB plane
    // serviced through it — lidar init runs after USB is live, and un-pumped
    // 100ms stalls are exactly the class of bug that killed USB elsewhere
    // (SD autosave, ntInit, intro; see pmor_usb_service in the main file).
    while((readReg8(R_FIRMWARE_STATUS)&0x01)==0) {
        pmor_usb_service();
        if(System::GetNow()-t>100) return false;
    }
    writeReg8(R_PAD_I2C_HV_EXTSUP, readReg8(R_PAD_I2C_HV_EXTSUP)|0x01);
    fast_osc_frequency_ = readReg16(R_OSC_FAST_FREQ);
    osc_calibrate_val_  = readReg16(R_RESULT_OSC_CAL);
    writeReg16(R_DSS_TARGET_RATE, 0x0A1C);
    writeReg8(R_GPIO_TIO_HV_STATUS, 0x02);
    writeReg8(R_SIGMA_PULSE_WIDTH, 8); writeReg8(R_SIGMA_AMBIENT_WIDTH, 16);
    writeReg8(R_ALGO_CROSSTALK_HMM, 0x01); writeReg8(R_ALGO_RANGE_IGN_HMM, 0xFF);
    writeReg8(R_ALGO_RANGE_MIN_CLIP, 0x00); writeReg8(R_ALGO_CONSISTENCY, 0x02);
    writeReg16(R_SYS_THRESH_HIGH, 0); writeReg16(R_SYS_THRESH_LOW, 0);
    writeReg8(R_DSS_APERTURE_ATT, 0x38);
    writeReg16(R_RANGE_SIGMA_THRESH, 360); writeReg16(R_RANGE_MIN_COUNT, 192);
    writeReg8(R_GPH0, 0x01); writeReg8(R_GPH1, 0x01);
    writeReg8(R_SD_QUANTIFIER, 0x02); writeReg8(R_SYS_GPH, 0x00);
    writeReg8(R_SYS_SEED_CONFIG, 0x01); writeReg8(R_SYS_SEQUENCE, 0x8B);
    writeReg16(R_DSS_MANUAL_SPADS, 200<<8); writeReg8(R_DSS_ROI_MODE, 0x02);
    if(kCentreRoi8x8) {
        // spec §11 item 5: 8x8 centre ROI. XY size byte packs (width-1)<<4 |
        // (height-1); 7<<4|7 = 0x77 = 8x8. Centre SPAD 199 is the array's
        // physical centre on the VL53L1X's 16x16 SPAD map.
        writeReg8(R_ROI_CENTRE_SPAD, 199);
        writeReg8(R_ROI_XY_SIZE, 0x77);
    }
    setTimingBudget33msShort();       // SHORT mode (was LONG) — spec §11 item 3
    writeReg16(R_ALGO_PART_OFFSET, readReg16(R_MM_OUTER_OFFSET)*4);
    calibrated_ = false; cal_pending_ = false;
    bad_count_  = 0;
    return true;
}

void VL53L1X_Mini::startContinuous(uint32_t period_ms) {
    writeReg32(R_SYS_INTERMEAS_PERIOD, period_ms * osc_calibrate_val_);
    writeReg8(R_SYS_INTERRUPT_CLEAR, 0x01);
    writeReg8(R_SYS_MODE_START, 0x40);
}

// Ported from Pololu's VL53L1X.cpp updateDSS(): recompute the manual
// "requested effective SPADs" register from THIS measurement's total return
// rate (peak signal + ambient, per SPAD) so it tracks the configured target
// (DSS_CONFIG__TARGET_TOTAL_RATE_MCPS, 0x0A1C — set once in init()) instead
// of staying pinned at init()'s fixed 200 SPADs. At close range a strong
// return saturates the fixed-SPAD rate check and the sensor reports a
// not-valid status (4/5/12) — the root cause behind "reads 0 at ~4in, fine
// further out" (v2-audit.md §11 hypothesis 1).
void VL53L1X_Mini::updateDSS(uint16_t dss_actual_spads, uint16_t peak_signal_rate, uint16_t ambient_rate) {
    if(dss_actual_spads == 0) { writeReg16(R_DSS_MANUAL_SPADS, 0x8000); return; }   // avoid div-by-zero, mid-point fallback
    uint32_t total_rate = (uint32_t)peak_signal_rate + (uint32_t)ambient_rate;
    if(total_rate > 0xFFFFu) total_rate = 0xFFFFu;
    total_rate <<= 16;                       // fixed-point headroom, as Pololu's driver does
    total_rate /= dss_actual_spads;
    if(total_rate == 0) { writeReg16(R_DSS_MANUAL_SPADS, 0x8000); return; }
    uint32_t required_spads = ((uint32_t)0x0A1Cu << 16) / total_rate;   // 0x0A1C = the target rate written in init()
    if(required_spads > 0xFFFFu) required_spads = 0xFFFFu;
    writeReg16(R_DSS_MANUAL_SPADS, (uint16_t)required_spads);
}

// Ported from Pololu's VL53L1X.cpp setupManualCalibration(): once the sensor
// has produced one real result, freeze the automatic VHV (temperature/
// voltage) calibration that would otherwise re-run every measurement, and
// override the phase calibration with the value the sensor already
// converged on. See the R_PHASECAL_RESULT_VCSEL comment above for the one
// register in here whose address is a best-effort reconstruction rather
// than cross-checked against this file's pre-existing (confirmed-correct)
// constants.
uint16_t VL53L1X_Mini::read() {
    // Whole read in one call. NON-BLOCKING when the sensor has nothing (2026-08-16): not ready => last reading.
    if(!dataReady()) return last_dist_;
    readBurst();
    readBurst2();
    readClear();
    uint16_t d = readFinish();
    while(!calStep()) {}
    return d;
}

void VL53L1X_Mini::readBurst() {
    // Result fields from RESULT__RANGE_STATUS (0x0089), per Pololu's ResultBuffer / updateDSS(): status, (reserved), stream count, DSS
    // actual effective SPADs (0x008C-D), (reserved), ambient rate (0x0090-1), (reserved: sigma/phase), distance (0x0096-7), peak signal
    // rate (0x0098-9) -- offsets 0,2,3-4,7-8,13-14,15-16 from the base. Pololu reads all 17 in one go; that is ~1.5 ms of bit-banged I2C
    // in one piece, so it is two reads here (offsets 0-8, then 13-16: the 4 unused bytes 9-12 are skipped as a bonus).
    uint8_t a[9];
    vl53_read_reg(R_RESULT_RANGE_STATUS, a, sizeof(a));
    rb_status_  = a[0] & 0x1F;
    rb_spads_   = ((uint16_t)a[3]  << 8) | a[4];
    rb_ambient_ = ((uint16_t)a[7]  << 8) | a[8];
}
void VL53L1X_Mini::readBurst2() {
    uint8_t b[4];
    vl53_read_reg(R_RESULT_RANGE_STATUS + 13, b, sizeof(b));
    rb_dist_    = ((uint16_t)b[0] << 8) | b[1];
    rb_peak_    = ((uint16_t)b[2] << 8) | b[3];
}

void VL53L1X_Mini::readClear() { writeReg8(R_SYS_INTERRUPT_CLEAR, 0x01); }

uint16_t VL53L1X_Mini::readFinish() {
    uint8_t  status = rb_status_;
    uint16_t dist   = rb_dist_;
    last_status_ = status;

    updateDSS(rb_spads_, rb_peak_, rb_ambient_);   // every measurement, per Pololu

    // Status handling per Pololu's table (v2-audit.md §11 item 2): 9 =
    // RANGE_VALID, 6 = SIGMA warning, 8 = MIN_CLIP warning — all three
    // accepted as good distances. 7 = PHASE_CONSISTENCY/wrap: hold the last
    // good value (the target is still there, just an ambiguous phase this
    // cycle). Anything else — including 4 (MSRC no target/signal), 5 (range
    // phase out of bounds) and 12 (crosstalk/range-ignore threshold) — is
    // not valid, but a single bad status shouldn't zero out a theremin mid-
    // gesture: keep the last good value for up to 3 consecutive bad reads
    // before finally reporting 0 (spec §11 item 4).
    if((status==9||status==6||status==8) && dist>0) {
        last_dist_ = dist;
        bad_count_ = 0;
        if(!calibrated_ && !cal_pending_) { cal_pending_ = true; cal_step_ = 0; }
        return dist;
    }
    if(status==7) return last_dist_;
    if(bad_count_ < 3) { bad_count_++; return last_dist_; }
    return 0;
}

// setupManualCalibration() in four bounded pieces (<= 2 register ops each).
bool VL53L1X_Mini::calStep() {
    if(!cal_pending_) return true;
    switch(cal_step_) {
        case 0: vhv_init_ = readReg8(R_VHV_INIT); vhv_timeout_ = readReg8(R_VHV_LOOP_BOUND); break;
        case 1: writeReg8(R_VHV_INIT, vhv_init_ & 0x7F);                                   // disable VHV init
                writeReg8(R_VHV_LOOP_BOUND, (uint8_t)((vhv_timeout_ & 0x03) + (3 << 2))); break;   // tuning-param loop bound
        case 2: writeReg8(R_PHASECAL_OVERRIDE, 0x01); vhv_init_ = readReg8(R_PHASECAL_RESULT_VCSEL); break;
        default: writeReg8(R_CAL_VCSEL_START, vhv_init_); calibrated_ = true; cal_pending_ = false; return true;
    }
    cal_step_++;
    return false;
}
