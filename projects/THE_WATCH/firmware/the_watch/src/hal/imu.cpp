#include "imu.h"
#include "../config.h"

#include <Arduino.h>
#include "SensorQMI8658.hpp"  // from Waveshare's bundled SensorLib

static SensorQMI8658 s_qmi;
static bool s_ready = false;

bool imu_init(TwoWire &wire) {
  if (!s_qmi.begin(wire, WATCH_IMU_ADDR, WATCH_I2C_SDA, WATCH_I2C_SCL)) {
    s_ready = false;
    return false;
  }
  s_qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                            SensorQMI8658::ACC_ODR_1000Hz,
                            SensorQMI8658::LPF_MODE_0);
  s_qmi.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,
                        SensorQMI8658::GYR_ODR_896_8Hz,
                        SensorQMI8658::LPF_MODE_3);
  s_qmi.enableAccelerometer();
  s_qmi.enableGyroscope();
  s_ready = true;
  return true;
}

bool imu_read(ImuSample &out) {
  if (!s_ready) return false;
  if (!s_qmi.getDataReady()) return false;
  s_qmi.getAccelerometer(out.ax, out.ay, out.az);
  s_qmi.getGyroscope(out.gx, out.gy, out.gz);
  return true;
}

bool imu_wrist_raised() {
  ImuSample s;
  if (!imu_read(s)) return false;
  // Face-up-toward-wearer: gravity mostly on Y, Z near zero. Coarse heuristic.
  return (fabsf(s.ay) > 0.6f && fabsf(s.az) < 0.5f);
}
