#include <stdio.h>
#include <string.h>
#include <math.h>
#define SDRAM_BSS
#include "config.h"
#include "pmor_core/shared_state.h"
#include "pmor_core/scale.h"
#include "pmor_core/drums.h"
#include "pmor_core/sequencer.h"
#include "pmor_core/clock.h"
#include "pmor_core/synth.h"
#include "pmor_core/sampler.h"
SharedState g; SeqState seq; ClockState clk;
int main(){
  scaleInit(); drumsInit(); clk.step_samples=SR*15.f/120.f; clk.varispeed=1.f;
  static float L[BLOCK],R[BLOCK];
  printf("DRUMS peak per sound (group/idx name peak rms_dB len_ms)\n");
  for(int gi=0;gi<DG_COUNT;gi++) for(int si=0;si<DRUM_SOUNDS;si++){
    uint8_t pad=0; for(int p=0;p<16;p++) if(drumPadGroup(p)==gi && drumPadIndexInGroup(p)==0){pad=p;break;}
    drums.pad[pad].sound=si; drums.pad[pad].length=0.5f; drums.pad[pad].pitch=0; drums.pad[pad].tone=0.5f; drums.pad[pad].reverse=false;
    // flush
    for(int b=0;b<200;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);}
    drumsTrigger(pad,1.f);
    float pk=0,sq=0; int n=0,last=0;
    for(int b=0;b<1500;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);
      for(int i=0;i<BLOCK;i++){float m=fabsf(L[i])>fabsf(R[i])?fabsf(L[i]):fabsf(R[i]); if(m>pk)pk=m; sq+=L[i]*L[i]+R[i]*R[i]; n++; if(m>1e-3f) last=b*BLOCK+i;}}
    printf("%d/%2d %-6s pk=%.3f rms=%.1fdB len=%dms\n",gi,si,DRUM_SOUND_NAMES[gi][si],pk,10*log10(sq/(2*n)+1e-12),last*1000/48000);
  }
  // synth
  synthInit(); printf("\nSYNTH peak per algo (chord of 4, vel 1)\n");
  for(int a=0;a<ALG_COUNT;a++){ syn.algo=a; synthAllOff(); seq.lane[ENG_SYNTH].polyphony=4;
    for(int b=0;b<100;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);synthProcess(L,R,BLOCK);}
    synthNoteOn(12,1.f);synthNoteOn(13,1.f);synthNoteOn(14,1.f);synthNoteOn(15,1.f);
    float pk=0,sq=0;int n=0;
    for(int b=0;b<750;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);synthProcess(L,R,BLOCK);
      for(int i=0;i<BLOCK;i++){float m=fabsf(L[i])>fabsf(R[i])?fabsf(L[i]):fabsf(R[i]); if(m>pk)pk=m; sq+=L[i]*L[i]+R[i]*R[i]; n++;}}
    synthAllOff();
    printf("%d %-6s pk=%.3f rms=%.1fdB\n",a,SYN_ALGO_NAMES[a],pk,10*log10(sq/(2*n)+1e-12));
  }
  return 0;
}
