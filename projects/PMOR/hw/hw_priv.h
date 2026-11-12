// hw_priv.h — symbols shared BETWEEN the hw/ modules only (boot order, boot
// marks, per-module init entry points). Nothing outside hw/ should include
// this; the public façade is hw.h.
#pragma once
#include <stdint.h>
#include "hw.h"

// ---- boot-time marks (ms since reset) — printed once over CDC by main()
extern volatile uint32_t g_t_usb, g_t_oled, g_t_sens, g_t_lidar, g_t_sd, g_t_boot;

// ---- usb_service.cpp
void PollCdc(void);                 // line-buffered CDC console → clock_engine_handle_cdc_line

// ---- trellis.cpp
bool ntInit();                      // boot only (SW reset + 500 ms usbDelay). Call LAST in boot.
void trellisBootInit();             // wraps ntInit() and seeds trel_ok

// ---- oled.cpp
// oledInit()/oledFlush() are public (hw.h)

// ---- sd.cpp
bool sdMount();                     // FATFS_LinkDriver + f_mount; sets sdOk()

// ---- sensors.cpp
void sensorsBusInit();              // sbus.init(D13,D14); sbus.us=3; tbus.init(D11,D12)
void imuInit();                     // MPU6050 reset/wake/ID
void lidarInit();                   // VL53L1X init + startContinuous(100)

// ---- controls.cpp
void controlsInit();                // enc1 + joystick SW + ADC (FSR ch0, JOY X ch1, JOY Y ch2)

// ---- boot.cpp
void hwRestartAudio();             // StartAudio again with the boot callback (CDC AUDIO 1)

// ---- dfu.cpp
void dfuBootGesture();              // D2 + D24 both low at power-on → bootloader (never returns if so)
