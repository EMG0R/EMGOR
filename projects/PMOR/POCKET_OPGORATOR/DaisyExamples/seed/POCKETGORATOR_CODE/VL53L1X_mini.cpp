#include "VL53L1X_mini.h"
#include "daisy_seed.h"

// Main-file USB service (tud_task + audio drain + MIDI clock). Every
// blocking spin in this driver must pump it — see init()/read().
extern "C" void pmor_usb_service(void);

using namespace daisy;

static const uint16_t R_SOFT_RESET           = 0x0000;
static const uint16_t R_OSC_FAST_FREQ        = 0x0006;
static const uint16_t R_PAD_I2C_HV_EXTSUP   = 0x002E;
static const uint16_t R_GPIO_TIO_HV_STATUS   = 0x0031;
static const uint16_t R_ALGO_CROSSTALK_HMM   = 0x0039;
static const uint16_t R_ALGO_RANGE_IGN_HMM   = 0x003E;
static const uint16_t R_ALGO_RANGE_MIN_CLIP  = 0x003F;
static const uint16_t R_ALGO_CONSISTENCY     = 0x0040;
static const uint16_t R_DSS_TARGET_RATE      = 0x0024;
static const uint16_t R_DSS_APERTURE_ATT     = 0x0057;
static const uint16_t R_DSS_ROI_MODE         = 0x004F;
static const uint16_t R_DSS_MANUAL_SPADS     = 0x0054;
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
static const uint16_t R_RESULT_DISTANCE      = 0x0096;
static const uint32_t TIMING_GUARD           = 4528;

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
void VL53L1X_Mini::setTimingBudget50msLong() {
    writeReg8(R_RANGE_VCSEL_A, 0x0F); writeReg8(R_RANGE_VCSEL_B, 0x0D);
    writeReg8(R_RANGE_VALID_PHASE, 0xB8);
    writeReg8(R_SD_WOI_SD0, 0x0F);  writeReg8(R_SD_WOI_SD1, 0x0D);
    writeReg8(R_SD_INIT_PHASE_SD0, 14); writeReg8(R_SD_INIT_PHASE_SD1, 14);
    uint32_t rt = (50000 - TIMING_GUARD) / 2;
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
    setTimingBudget50msLong();
    writeReg16(R_ALGO_PART_OFFSET, readReg16(R_MM_OUTER_OFFSET)*4);
    return true;
}
void VL53L1X_Mini::startContinuous(uint32_t period_ms) {
    writeReg32(R_SYS_INTERMEAS_PERIOD, period_ms * osc_calibrate_val_);
    writeReg8(R_SYS_INTERRUPT_CLEAR, 0x01);
    writeReg8(R_SYS_MODE_START, 0x40);
}
uint16_t VL53L1X_Mini::read() {
    // NON-BLOCKING (2026-08-16): the sensor ranges continuously at ~10Hz and
    // this is called from the main loop every pass. The old up-to-200ms
    // dataReady() spin per call collapsed the loop (MIDI tick rate fell 4x
    // even with the USB plane pumped). Not ready => hand back the last
    // reading immediately; a 100ms-period sensor never needs waiting on.
    if(!dataReady()) return last_dist_;
    uint8_t  status = readReg8(0x0089);
    uint16_t dist   = readReg16(R_RESULT_DISTANCE);
    writeReg8(R_SYS_INTERRUPT_CLEAR, 0x01);
    last_status_ = status & 0x1F;
    if((last_status_==9||last_status_==6||last_status_==8) && dist>0) { last_dist_=dist; return dist; }
    if(last_status_==7) return last_dist_;
    return 0;
}
