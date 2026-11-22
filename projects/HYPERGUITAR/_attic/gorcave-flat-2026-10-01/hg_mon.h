/* hg_mon.h - web-UI telemetry + live volume control. See the .cpp. */
#ifndef HG_MON_H
#define HG_MON_H

#include "hyperguitar.h"

extern float gVolSub[NUM_STRINGS];   /* per-string hex pickup */
extern float gVolPiezo;              /* contact mics */
extern float gVolTrs;                /* built-in pickup via TRS */
extern float gVolOutA;               /* outputs 1+2 */
extern float gVolOutB;               /* outputs 3+  */
extern float gTrellisBright;         /* 0..1 NeoPixel scale (see hg_mon.cpp) */
extern float gTrellisOffDim;         /* unlit-pad brightness (current saver) */
extern int   gTrellisDark;           /* 1 = unlit pads fully off (big current cut) */

extern float gMonInRms[10];
extern float gMonOutRms[2];
/* clip audit: peaks since the last telemetry packet (reset by monTick),
 * clip counts cumulative since boot. Written by render(), read by monTick. */
extern float gMonInPk[10], gMonPrePk[2], gMonOutPk[2];
extern unsigned int gMonInClip[10], gMonPreClip;

void monSetup(void);
void monTick(const float *inDb, const float *outDb);   /* aux task, ~20 Hz */

/* Is this effect id currently on? Defined in trellis_ui.cpp next to gFx[],
 * for the same reason fxSetActive lives there - one map, not two. */
int fxIsOn(int fx);

/* Per-slot LED state for the dashboard: bit 0 = lit, bit 1 = armed.
 * Defined in trellis_ui.cpp so it shares the LED render's rule. */
int fxSlotLed(int cat, int slot);

/* USB audio on/off (HGSET usb 0|1, saved with the gains). Off = the firmware
 * stops feeding the bridge and the bridge closes the USB sound output. */
extern volatile int gUsbAudio;
int monUsbToggle(void);   /* flips it, saves, returns the new state */

#endif
