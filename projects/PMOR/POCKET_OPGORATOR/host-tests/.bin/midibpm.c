#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach_time.h>
static int clocks=0,starts=0,stops=0,cc116=0,cc119=0;
static int last116=-1,last119=-1;
static uint64_t t_first=0,t_last=0,t_prev=0;
#define MAXIV 200000
static double ivl[MAXIV]; static int nv=0;
static mach_timebase_info_data_t tb;
static void proc(const MIDIPacketList *pl,void*a,void*b){
  (void)a;(void)b; const MIDIPacket *p=&pl->packet[0];
  for(unsigned i=0;i<pl->numPackets;i++){
    for(unsigned j=0;j<p->length;j++){
      unsigned char s=p->data[j];
      if(s==0xF8){ uint64_t n=mach_absolute_time();
                   if(t_prev && nv<MAXIV)
                     ivl[nv++]=(double)(n-t_prev)*tb.numer/tb.denom/1e6; /* ms */
                   t_prev=n;
                   if(!t_first) t_first=n; else t_last=n; clocks++; }
      else if(s==0xFA) starts++;
      else if(s==0xFC) stops++;
      else if((s&0xF0)==0xB0 && j+2<p->length){
        if(p->data[j+1]==116){cc116++; last116=p->data[j+2];}
        else if(p->data[j+1]==119){cc119++; last119=p->data[j+2];}
        j+=2;
      }
    }
    p=MIDIPacketNext(p);
  }
}
int main(int argc,char**argv){
  double window = (argc>1)? atof(argv[1]) : 30.0;
  mach_timebase_info(&tb);
  MIDIClientRef c; MIDIClientCreate(CFSTR("verify"),NULL,NULL,&c);
  MIDIPortRef in; MIDIInputPortCreate(c,CFSTR("in"),proc,NULL,&in);
  ItemCount n=MIDIGetNumberOfSources(); int found=0;
  for(ItemCount i=0;i<n;i++){
    MIDIEndpointRef e=MIDIGetSource(i); CFStringRef nm=NULL;
    MIDIObjectGetStringProperty(e,kMIDIPropertyDisplayName,&nm);
    char b[256]={0};
    if(nm){CFStringGetCString(nm,b,256,kCFStringEncodingUTF8);CFRelease(nm);}
    if(strstr(b,"OpGorator")){MIDIPortConnectSource(in,e,NULL);found=1;}
  }
  if(!found){ printf("NOSOURCE\n"); return 2; }
  CFRunLoopRunInMode(kCFRunLoopDefaultMode,window,false);
  if(clocks<2){ printf("NOCLOCK %d\n",clocks); return 3; }
  double span=(double)(t_last-t_first)*tb.numer/tb.denom/1e9;
  /* NAIVE rate: ticks/time. THIS LIES if the device duplicates bytes --
     a duplicate arrives with a ~0ms gap and inflates the count without
     changing the real spacing. It read +2.2% "fast" for three debugging
     rounds on a clock that was actually accurate to 0.04%. Kept only to
     expose the discrepancy. */
  double bpm_naive=(clocks-1)/span*60.0/24.0;
  /* TRUE rate: mean of intervals that represent REAL spacing. Anything
     under 2ms cannot be a genuine tick gap (24 PPQN at 400 BPM is still
     6.25ms), so it is a duplicate, not a tick. */
  int dup=0; double isum=0; int ireal=0;
  for(int i=0;i<nv;i++){ if(ivl[i]<2.0) dup++; else { isum+=ivl[i]; ireal++; } }
  double bpm_true = (ireal>0)? (1000.0/(isum/ireal)*60.0/24.0) : 0.0;
  /* machine-readable: ticks span bpm_true bpm_naive dup cc116 cc119 starts stops */
  printf("OK %d %.6f %.6f %.6f %d %d %d %d %d\n",
         clocks,span,bpm_true,bpm_naive,dup,last116,last119,starts,stops);
  return 0;
}
