// hal/battery.h — WATCH's OWN LiPo battery gauge (local ADC).
//
// Distinct from the PHONE battery (which arrives over BLE). This reads the
// on-board battery voltage divider and converts it to a percentage.
//
// Board: Waveshare ESP32-S3-Touch-LCD-1.69. Battery is sensed on GPIO1 through
// a 200K/100K divider (the ADC node is VBAT/3), per Waveshare's wiki. So true
// VBAT = ADC_millivolts * 3.  (VERIFY-ON-HARDWARE: pin + ratio are from the
// Waveshare wiki, not a measured schematic in-hand.)
#pragma once

#include <stdint.h>

// Configure the ADC pin/attenuation. Call once in setup after Serial.
void battery_init();

// Take a fresh averaged reading. Fills *out_mv (true battery millivolts) and
// returns battery percentage 0..100 (LiPo curve). Returns -1 if read failed.
int  battery_read(int *out_mv);
