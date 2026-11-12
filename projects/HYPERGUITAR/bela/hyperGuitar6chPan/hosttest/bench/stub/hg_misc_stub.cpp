/*
 * hg_misc_stub.cpp - host stand-ins for hg_ctrl.cpp / hg_mon.cpp /
 * i2c_sensors.cpp (hardware I/O, telemetry, I2C). render.cpp references them
 * but none is in the audio DSP path. Volumes use the device defaults from
 * hg_mon.cpp so the render() gain staging is the real one.
 */
#include "hg_ctrl.h"
#include "hg_mon.h"
#include "i2c_sensors.h"

volatile float gHgPot = 0.5f;
void hgCtrlSetup(void) { }
void hgCtrlTick(BelaContext *) { }
void hgEncDiag(int, int, int, int) { }

float gVolSub[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
float gVolPiezo = 2.50f;
float gVolTrs = 1.0f;
float gVolOutA = 1.0f;
float gVolOutB = 1.0f;
float gTrellisBright = 1.0f;
float gTrellisOffDim = 1.0f;
int gTrellisDark = 0;
float gMonInPk[10], gMonPrePk[2], gMonOutPk[2];
unsigned int gMonInClip[10], gMonPreClip;
volatile int gUsbAudio = 0;
void monSetup(void) { }
void monTick(const float *, const float *) { }
int monUsbToggle(void) { return gUsbAudio; }

int gEnableMpu = 0;
int gEnableTof = 0;
int sensorsInit(SensorState *) { return 0; }
void sensorsRead(SensorState *) { }
