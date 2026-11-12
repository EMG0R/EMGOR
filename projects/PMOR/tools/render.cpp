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
static void wr32(FILE* f, uint32_t v){ fwrite(&v,4,1,f);} static void wr16(FILE* f, uint16_t v){ fwrite(&v,2,1,f);}
int main(){ scaleInit(); drumsInit(); clk.step_samples=6000; clk.varispeed=1.f;
 static float L[BLOCK],R[BLOCK];
 FILE* idx=fopen("wav/index.txt","w");
 for(int gi=0;gi<4;gi++) for(int si=0;si<16;si++){
  uint8_t pad=0; for(int p=0;p<16;p++) if(drumPadGroup(p)==gi && drumPadIndexInGroup(p)==0){pad=p;break;}
  drums.pad[pad].sound=si; drums.pad[pad].length=0.5f; drums.pad[pad].pitch=0; drums.pad[pad].tone=0.5f; drums.pad[pad].reverse=false;
  for(int b=0;b<200;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);}
  drumsTrigger(pad,1.f);
  int nblk = (gi==0)? 1125 : 750;   // 1.5 s kicks, 1.0 s others
  char fn[64]; snprintf(fn,64,"wav/%d_%02d_%s.wav",gi,si,DRUM_SOUND_NAMES[gi][si]);
  FILE* f=fopen(fn,"wb"); uint32_t ns=nblk*BLOCK; uint32_t datasz=ns*2;
  fwrite("RIFF",1,4,f); wr32(f,36+datasz); fwrite("WAVEfmt ",1,8,f); wr32(f,16); wr16(f,1); wr16(f,1); wr32(f,48000); wr32(f,48000*2); wr16(f,2); wr16(f,16); fwrite("data",1,4,f); wr32(f,datasz);
  float pk=0;
  for(int b=0;b<nblk;b++){memset(L,0,sizeof L);memset(R,0,sizeof R);drumsProcess(L,R,BLOCK);
   for(int i=0;i<BLOCK;i++){ float l=L[i],r=R[i]; if(fabsf(l)>pk)pk=fabsf(l); if(fabsf(r)>pk)pk=fabsf(r);
     // store at half scale so peaks up to 2.0 survive 16-bit (page multiplies by 2)
     float m=0.5f*(l+r); int16_t a=(int16_t)fmaxf(-32767.f,fminf(32767.f,m*16383.5f)); fwrite(&a,2,1,f);} }
  fclose(f); fprintf(idx,"%d %d %s %s %.3f\n",gi,si,DRUM_SOUND_NAMES[gi][si],fn,pk);
 } fclose(idx); }
