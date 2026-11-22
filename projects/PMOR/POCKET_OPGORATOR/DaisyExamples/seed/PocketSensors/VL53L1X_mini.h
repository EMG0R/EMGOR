#pragma once
#include <stdint.h>

// Bit-bang I2C callbacks — implemented in HelloOled.cpp
void vl53_write_reg(uint16_t reg, uint8_t* data, uint8_t len);
void vl53_read_reg(uint16_t reg, uint8_t* data, uint8_t len);

class VL53L1X_Mini {
public:
    bool     init();
    void     startContinuous(uint32_t period_ms);
    uint16_t read();
    bool     dataReady();
    uint16_t lastModelId() { return model_id_; }
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
    void     setTimingBudget50msLong();

    uint16_t fast_osc_frequency_;
    uint16_t osc_calibrate_val_;
    uint16_t model_id_;
    uint16_t last_dist_   = 0;
    uint8_t  last_status_ = 0xFF;
};
