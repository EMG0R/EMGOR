// hal/imu.h — IMU Hardware Abstraction Layer (QMI8658 6-axis).
//
// Minimal for this clock/UI firmware: used only for wrist-tilt wake and a raw
// accel read the menu page can show. The sensor-fusion / step / activity work
// belongs to the *other* (full sensor) project, not here.
#pragma once

#include <Wire.h>

struct ImuSample {
  float ax, ay, az;  // g
  float gx, gy, gz;  // dps
};

// Initialise QMI8658 on the shared I2C bus. Returns false if not found.
bool imu_init(TwoWire &wire);

// Latest sample. Returns false if the IMU is absent / not ready.
bool imu_read(ImuSample &out);

// Cheap heuristic: is the watch face roughly tilted toward the wearer?
// (az small / ay dominant). Used for wake-on-wrist-raise. Coarse on purpose.
bool imu_wrist_raised();
