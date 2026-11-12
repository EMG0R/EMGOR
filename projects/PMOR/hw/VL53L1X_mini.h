// VL53L1X_mini.h — minimal VL53L1X driver over the bit-banged sensor bus.
// Overhauled per v2-audit.md §11 / PMOR-V3-SPEC.md §11: the v2 driver read 0
// at close range because it never re-tuned the DSS SPAD count per
// measurement. Public API is UNCHANGED (hw/sensors.cpp depends on it):
// init(), startContinuous(period_ms), dataReady(), read(), lastStatus(),
// lastModelId().
#pragma once
#include <stdint.h>

// Bit-bang I2C callbacks — implemented in hw/sensors.cpp.
void vl53_write_reg(uint16_t reg, uint8_t* data, uint8_t len);
void vl53_read_reg(uint16_t reg, uint8_t* data, uint8_t len);

class VL53L1X_Mini {
public:
    bool     init();
    void     startContinuous(uint32_t period_ms);
    uint16_t read();           // whole read in one call (blocking, ~4 ms): kept for the boot path / tests
    bool     dataReady();
    // The same read split into bounded bus steps (each <= one burst, ~1.2 ms), so the main loop can run one per pass and keep the
    // keypad poll on time (2026-11-11: the single call was 4-5 ms of bit-banged I2C = a pad hit sitting in the seesaw FIFO).
    // Call in order after dataReady(): readBurst(); readBurst2(); readClear(); readFinish(); then calStep() until it returns true.
    void     readBurst();      // result fields, first half (status, SPADs, ambient) -> members
    void     readBurst2();     // second half (distance, peak signal)
    void     readClear();      // clear the sensor's interrupt
    uint16_t readFinish();     // DSS update + status handling; returns the distance (0 = invalid)
    bool     calStep();        // one piece of the one-time manual calibration (true = nothing left to do)
    uint16_t lastModelId() { return model_id_; }
    // Raw 5-bit RESULT__RANGE_STATUS (register 0x0089 & 0x1F) from the most
    // recent burst read — NOT remapped — so DASH keeps showing the code the
    // datasheet's status table documents (9 = range valid, 6 = sigma warning,
    // 8 = min-clip warning accepted below the tolerance, 7 = phase-consistency/
    // wrap, 4/5/12 = various not-valid, ported per Pololu's VL53L1X.cpp table).
    uint8_t  lastStatus()  { return last_status_; }

private:
    void     writeReg8(uint16_t reg, uint8_t val);
    void     writeReg16(uint16_t reg, uint16_t val);
    void     writeReg32(uint16_t reg, uint32_t val);
    uint8_t  readReg8(uint16_t reg);
    uint16_t readReg16(uint16_t reg);

    uint32_t calcMacroPeriod(uint8_t vcsel_period);
    uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_us, uint32_t macro_period_us);
    uint16_t encodeTimeout(uint32_t timeout_mclks);
    void     setTimingBudget33msShort();      // SHORT distance mode (was setTimingBudget50msLong/LONG)

    // ported from Pololu's VL53L1X.cpp (v2-audit.md §11 hypothesis 1 — the
    // "reads 0 at ~4in" fix): recompute DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT
    // from THIS measurement's signal/ambient rate so the return rate tracks
    // DSS_CONFIG__TARGET_TOTAL_RATE_MCPS instead of staying pinned at the
    // fixed 200 SPADs init() used to leave it at.
    void     updateDSS(uint16_t dss_actual_spads, uint16_t peak_signal_rate, uint16_t ambient_rate);
    // setupManualCalibration() (Pololu): once, after the first valid result -- disables the VHV auto-calibration and pins the phase
    // calibration; run as calStep() pieces (see above).

    uint16_t fast_osc_frequency_ = 0;
    uint16_t osc_calibrate_val_  = 0;
    uint16_t model_id_           = 0;
    uint16_t last_dist_          = 0;
    uint8_t  last_status_        = 0xFF;
    uint8_t  bad_count_          = 0;       // consecutive not-valid reads since the last good one
    bool     calibrated_         = false;   // setupManualCalibration() has run
    bool     cal_pending_        = false;   // first valid result seen, calibration steps still to run
    uint8_t  cal_step_           = 0;
    uint8_t  vhv_init_ = 0, vhv_timeout_ = 0;
    uint8_t  rb_status_ = 0; uint16_t rb_spads_ = 0, rb_ambient_ = 0, rb_dist_ = 0, rb_peak_ = 0;   // last burst
};
