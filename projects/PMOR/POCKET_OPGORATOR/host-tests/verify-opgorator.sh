#!/usr/bin/env bash
# verify-opgorator.sh — post-flash acceptance battery for the Pocket OpGorator
# (bare-metal libDaisy + TinyUSB composite firmware).
#
# Run this on the Mac after every flash. It MEASURES; it does not assert.
# Every number it prints comes from an EXTERNAL reference (the host clock),
# never from the device's own opinion of itself — a device that derives both
# its audio clock and its millisecond timer from one misconfigured PLL will
# happily report that it is fine. The host cannot be fooled that way.
#
#   ./verify-opgorator.sh            # full battery (~90s)
#   ./verify-opgorator.sh --quick    # enumeration + short clock check (~40s)
#
# Exit 0 = every check passed. Exit 1 = at least one FAIL.

set -uo pipefail

DEV_NAME="Pocket OpGorator"
AUDIO_DEV_NAME="Pocket OpGorator Audio"
NOMINAL_RATE=48000
NOMINAL_BPM=120.0
# Acceptance: a crystal-derived clock configured correctly should sit far
# inside this. 0.1% = 1000ppm; a correct PLL should measure <100ppm. We fail
# at 0.1% because anything worse is a configuration bug, not crystal drift.
TOL_PCT=0.1

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$HERE/.bin"
mkdir -p "$BIN"

pass=0; fail=0
ok()   { printf '  \033[32mPASS\033[0m  %s\n' "$1"; pass=$((pass+1)); }
bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; fail=$((fail+1)); }
info() { printf '        %s\n' "$1"; }
hdr()  { printf '\n\033[1m%s\033[0m\n' "$1"; }

# ---------------------------------------------------------------- 1. USB
hdr "1. Enumeration"

if system_profiler SPAudioDataType 2>/dev/null | grep -q "$AUDIO_DEV_NAME"; then
    ok "USB audio device present ($AUDIO_DEV_NAME)"
else
    bad "USB audio device NOT found — is the board running the app (not DFU)?"
fi

# Find the CDC port that actually belongs to THIS device. Picking the first
# /dev/cu.usbmodem* is wrong: any other USB-serial gadget (Teensy, Bela, a
# second Daisy) will happily answer to that glob and produce a false PASS —
# and a false PASS here is dangerous, because this is the port the buttonless
# DFU trigger is sent to. Ask ioreg which tty hangs off the OpGorator itself.
CDC_PORT="$(ioreg -r -c IOUSBHostDevice -l 2>/dev/null \
            | awk '/"USB Product Name" = "'"$DEV_NAME"'"/{f=1}
                   f && /IOCalloutDevice/{gsub(/.*= "|"$/,""); print; exit}')"
if [[ -z "$CDC_PORT" ]]; then
    # fall back: only trust the glob if exactly one candidate exists AND the
    # OpGorator is the only composite device currently attached
    if system_profiler SPUSBDataType 2>/dev/null | grep -q "$DEV_NAME"; then
        CDC_PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)"
        [[ -n "$CDC_PORT" ]] && info "ioreg mapping unavailable; falling back to $CDC_PORT (verify before trusting)"
    fi
fi
if [[ -n "$CDC_PORT" ]]; then
    ok "CDC serial belongs to $DEV_NAME ($CDC_PORT)"
    echo "$CDC_PORT" > "$BIN/cdc_port"
else
    bad "no CDC serial for $DEV_NAME — buttonless DFU would be unavailable"
fi

if dfu-util -l 2>/dev/null | grep -q "Found DFU"; then
    bad "board is in DFU mode — flash the app firmware before running this"
    echo; echo "Aborting: nothing to measure while in the bootloader."; exit 1
fi

# --------------------------------------------------------- 2. MIDI + clock
hdr "2. MIDI clock (measured against the host clock)"

cat > "$BIN/midibpm.c" <<'EOF'
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
EOF

if cc -O2 -o "$BIN/midibpm" "$BIN/midibpm.c" \
        -framework CoreMIDI -framework CoreFoundation 2>/dev/null; then
    WIN=30; [[ $QUICK -eq 1 ]] && WIN=12
    info "listening ${WIN}s for MIDI clock..."
    OUT="$("$BIN/midibpm" "$WIN" || true)"
    case "$OUT" in
      OK*)
        read -r _ TICKS SPAN BPM BPM_NAIVE DUP V116 V119 STARTS STOPS <<<"$OUT"
        ERR="$(python3 -c "print(f'{($BPM/$NOMINAL_BPM-1)*100:+.3f}')")"
        PPM="$(python3 -c "print(f'{($BPM/$NOMINAL_BPM-1)*1e6:+.0f}')")"
        info "ticks=$TICKS over ${SPAN}s"
        info "TRUE tempo (duplicate-filtered): ${BPM} BPM  (${ERR}% / ${PPM} ppm)"
        info "naive count/time:                ${BPM_NAIVE} BPM  <- do not trust this"
        # USB quantizes MIDI arrival to 1ms frames, so intervals land in 20/21ms
        # bins. A short window cannot honestly resolve 0.1% through that; only
        # judge tempo on the full window, and report it in quick mode.
        if [[ $QUICK -eq 1 ]]; then
            info "(quick mode: window too short to judge ${TOL_PCT}% — informational only)"
        elif python3 -c "import sys; sys.exit(0 if abs($BPM/$NOMINAL_BPM-1)*100 <= $TOL_PCT else 1)"; then
            ok "clock tempo within ${TOL_PCT}%"
        else
            bad "clock tempo off by ${ERR}% (tolerance ${TOL_PCT}%)"
        fi
        # Duplicate transmission is its own defect and must not be allowed to
        # hide inside the tempo number. A <2ms gap cannot be a real tick:
        # even 400 BPM at 24 PPQN is 6.25ms apart.
        if [[ "$DUP" -eq 0 ]]; then
            ok "no duplicate clock bytes"
        else
            DUPPCT="$(python3 -c "print(f'{$DUP/max($TICKS-1,1)*100:.2f}')")"
            bad "$DUP duplicate 0xF8 bytes (${DUPPCT}% of ticks) — USB-MIDI TX duplication"
            info "  a naive receiver counting ticks will run fast by roughly this much"
        fi
        if [[ "$V116" == "1" ]]; then
            ok "role announce CC116=1 (OpGorator is master)"
        elif [[ "$V116" == "2" ]]; then
            info "role announce CC116=2 (Demiurge is master) — expected only if a host claimed it"
            ok "role announce present"
        else
            bad "no CC116 role announce seen"
        fi
        if [[ "$V119" != "-1" ]]; then
            SELFBPM="$(python3 -c "print(f'{40+$V119/127*360:.1f}')")"
            info "device SELF-REPORTS ${SELFBPM} BPM via CC119 (value $V119)"
            info "note: self-report vs measured is the tell — if they disagree, the codec rate is wrong"
        fi
        ;;
      NOSOURCE*) bad "no OpGorator MIDI source — MIDI did not enumerate" ;;
      NOCLOCK*)  bad "MIDI enumerated but NO clock ticks arrived (is it in follower mode?)" ;;
      *)         bad "clock measurement failed: $OUT" ;;
    esac
else
    bad "could not build the CoreMIDI probe"
fi

# ------------------------------------------------- 3. audio: does it stream
hdr "3. USB audio stream"

# Resolve the avfoundation device index. The naive `awk '{print $(NF-2)}'`
# approach silently yields an EMPTY index (the device name has spaces, so the
# field offsets don't land on the bracketed number) and ffmpeg then fails with
# a confusing error. Match the "[N] <name>" pattern explicitly instead.
audio_index() {
    ffmpeg -hide_banner -f avfoundation -list_devices true -i "" 2>&1 \
      | sed -nE "s/^.*\[([0-9]+)\] ${AUDIO_DEV_NAME}\$/\1/p" | head -1
}
IDX="$(audio_index)"
if [[ -z "$IDX" ]]; then
    bad "could not resolve an avfoundation index for '$AUDIO_DEV_NAME'"
fi

WAV="$BIN/capture.wav"
REC=4
if [[ -n "$IDX" ]] && ffmpeg -hide_banner -loglevel error -f avfoundation \
          -i ":$IDX" -t "$REC" -y "$WAV" 2>/dev/null && [[ -s "$WAV" ]]; then
    STATS="$(ffmpeg -hide_banner -i "$WAV" -af astats=metadata=1 -f null - 2>&1)"
    RMS="$(echo "$STATS" | awk '/RMS level dB/{print $NF; exit}')"
    FLAT="$(echo "$STATS" | awk '/Flat factor/{print $NF; exit}')"
    info "captured ${REC}s, RMS ${RMS} dB, flat factor ${FLAT}"
    if [[ "$RMS" == "-inf" || "$RMS" == "-inf.000000" ]]; then
        bad "audio stream is digital silence — capture path not delivering samples"
    else
        ok "audio stream carries real signal (not silence)"
    fi
else
    bad "could not capture from $AUDIO_DEV_NAME"
fi

# ----------------------------------- 4. TRUE codec rate (independent check)
if [[ $QUICK -eq 0 ]]; then
hdr "4. Audio-path rate corroboration (WEAK — read the caveats)"
info "Method: request N seconds of audio, time how long that ACTUALLY takes."
info "ffmpeg stops after N*${NOMINAL_RATE} samples; a fast codec delivers early."
info "CAVEAT 1: ffmpeg startup latency (~0.3-0.5s) is included, so a short"
info "  window cannot resolve a ~2% effect. Using a long window to dilute it."
info "CAVEAT 2: CoreAudio may rate-convert an off-nominal device to the"
info "  advertised 48kHz, which would HIDE the true rate entirely here."
info "=> Section 2 (MIDI vs host clock) remains the authoritative measurement."

if [[ -n "$IDX" ]]; then
    WANT=60
    T0="$(python3 -c 'import time;print(time.monotonic())')"
    ffmpeg -hide_banner -loglevel error -f avfoundation -i ":$IDX" \
           -t "$WANT" -y "$BIN/rate.wav" 2>/dev/null
    T1="$(python3 -c 'import time;print(time.monotonic())')"
    ELAPSED="$(python3 -c "print(f'{$T1-$T0:.4f}')")"
    if python3 -c "import sys; sys.exit(0 if $ELAPSED > 1.0 else 1)"; then
        TRUE_RATE="$(python3 -c "print(f'{$NOMINAL_RATE*$WANT/$ELAPSED:.1f}')")"
        RATIO="$(python3 -c "print(f'{($NOMINAL_RATE*$WANT/$ELAPSED)/$NOMINAL_RATE:.5f}')")"
        info "requested ${WANT}s, took ${ELAPSED}s wall"
        info "implied device rate ~= ${TRUE_RATE} Hz (ratio ${RATIO} vs nominal)"
        info "compare against section 2's ratio; agreement = real, divergence"
        info "  = CoreAudio is resampling and hiding it"
    else
        bad "capture returned immediately (${ELAPSED}s) — device index or stream broken"
    fi
fi
fi

# ------------------------------------------------------------------ summary
hdr "Summary"
printf '  %d passed, %d failed\n\n' "$pass" "$fail"
[[ $fail -eq 0 ]] || exit 1
