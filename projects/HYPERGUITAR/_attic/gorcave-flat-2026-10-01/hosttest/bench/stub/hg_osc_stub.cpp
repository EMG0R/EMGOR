/*
 * hg_osc_stub.cpp - host stand-in for hg_osc.cpp.
 *
 * hg_osc.cpp is the NEPTR control link: a UDP socket and the oscpkt
 * header-only library, neither of which exists on the bench host. The link
 * carries CONTROL only - it can set a lane, it is never in the audio path -
 * so for a DSP bench the honest model of it is "nobody ever pressed
 * anything": setup succeeds, the tick does nothing, every echo is dropped.
 * hg_lanes.cpp itself is linked FOR REAL, so the lane->effect mapping the
 * scenes go through is the device's own.
 */
#include "hg_link.h"

bool hgOscSetup(void) { return true; }
void hgOscTick(void *) { }
void hgOscClose(void) { }
void hgLaneEmit(int lane) { (void)lane; }
