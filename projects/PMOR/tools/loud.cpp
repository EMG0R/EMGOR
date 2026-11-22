#include <stdio.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "pmor_core/shared_state.h"
#include "pmor_core/scale.h"
#include "pmor_core/drums.h"
#include "pmor_core/sequencer.h"
#include "pmor_core/clock.h"
SharedState g; SeqState seq; ClockState clk;
// rough A-weighting: 2nd-order HP @ 200 Hz + 1st-order LP @ 7 kHz, RMS over first 250 ms
int main(){ scaleInit(); drumsInit(); clk.step_samples=6000; clk.varispeed=1.f;
 static float L[BLOCK],R[BLOCK];
 for(int gi=0;gi<4;gi++) for(int si=0;si<16;si++){
  uint8_t pad=0; for(int p=0;p<16;p++) if(drumPadGroup(p)==gi && drumPadIndexInGroup(p)==0){pad=p;break;}
  drums.pad[pad].sound=si; drums.pad[pad].length=0.5f; drums.pad[pad].pitch=0; drums.pad[pad].tone=0.5f; drums.pad[pad].reverse=false;
  for(int b=0;b<200;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);}
  drumsTrigger(pad,1.f);
  float pk=0,sq=0,sq0=0; int n=0; float h1=0,h2=0,x1=0,x2=0,lp=0;
  const float fc=200.f/48000.f, r=1.f-2.f*3.1416f*fc; const float a=2.f*3.1416f*7000.f/48000.f;
  for(int b=0;b<190;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);
   for(int i=0;i<BLOCK;i++){float x=L[i]+R[i]; float m=fabsf(x); if(m>pk)pk=m; if(n<7200) sq0+=x*x;
     float y=x-x1+r*h1; x1=x; h1=y; float y2=y-x2+r*h2; x2=y; h2=y2; lp+= (y2-lp)*a; sq+=lp*lp; n++;}}
  printf("%d/%2d %-6s pk=%.3f loud=%.4f rms=%.4f\n",gi,si,DRUM_SOUND_NAMES[gi][si],pk,sqrtf(sq/n),sqrtf(sq0/7200));
 }}
