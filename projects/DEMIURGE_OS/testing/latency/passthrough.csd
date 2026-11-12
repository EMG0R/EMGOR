<CsoundSynthesizer>
; passthrough.csd — measures what CSOUND ITSELF costs in round-trip latency.
;
; This is NOT part of any instrument. It is a measurement fixture, and it is
; deliberately the emptiest possible csound program: read the input, write it
; to the output, do nothing else. Anything it adds to the round trip is
; csound's engine and its JACK backend, not DSP.
;
; The question it answers:
;
;   testing/latency/rtdelay measures out -> DAC -> cable -> ADC -> in with
;   RTDELAY as the JACK client. That is the round trip for a client sitting one
;   quantum deep in the graph. It is only the instrument's round trip if csound
;   also behaves like a normal one-quantum JACK client — and there is a
;   specific reason to doubt that. csound's rtjack backend does NOT compute DSP
;   in the JACK callback: it runs its engine on its own thread with a ring
;   buffer in between (which is why demiurge-run-csound has to pin that thread
;   with chrt separately from pipewire's callback thread). The wrapper floors
;   -B at 1024 because "csound refuses anything below 1024 on this Pi". If that
;   ring sits in the signal path, csound costs real milliseconds that rtdelay's
;   number does not include.
;
; Method — the two-pass trick, with ONE loopback cable:
;
;   Both rtdelay and this program write to hardware playback and read from
;   hardware capture. A click emitted by rtdelay goes out the DAC, down the
;   cable, and arrives at the ADC, where BOTH programs hear it. rtdelay
;   registers that first arrival: that is pass 1, the plain round trip.
;   This program then re-emits the click to the DAC, so it makes a SECOND
;   trip down the cable and arrives at rtdelay again.
;
;     second arrival - first arrival = csound's own added latency
;                                      + one more converter round trip
;
;   So run rtdelay with and without this program loaded and difference the
;   arrival times; the extra converter trip is the already-measured baseline,
;   which is why the baseline has to be measured first and on the same day.
;
; Run it out-of-band, NOT through live.conf — this must never become part of
; the user's chain. Flags mirror demiurge-run-csound so the comparison is
; against the real backend configuration and not a different one:
;
;   ssh gorpi7@demiurge.local
;   chrt --fifo 74 csound -+rtaudio=jack -odac -iadc \
;        -r 48000 -k 750 -b 64 -B 1024 -d testing/latency/passthrough.csd
;
;   (-b MUST equal pipewire clock.force-quantum or pipewire-jack registers the
;    client and then produces silence. -B is the value under test: rerun at
;    1024 and at lower values to find where csound actually refuses.)
;
; Then wire it and measure:
;
;   pw-link <hw>:capture_FL csound6:input1
;   pw-link csound6:output1 <hw>:playback_FL
;   ./rtdelay.sh 40 250
<CsOptions>
-d
</CsOptions>
<CsInstruments>
sr     = 48000
; ksmps is macro-driven so the SAME fixture can be run at whatever the graph
; quantum currently is. It has to be: the embedded host (src/demiurge-csound-host.cpp)
; runs exactly quantum/ksmps control periods per JACK callback and refuses to
; start when ksmps does not divide the quantum, so a fixture pinned at 64 can
; only ever be measured at a quantum that is a multiple of 64. Override with
; --omacro:KSMPS=32.
#ifndef KSMPS
#define KSMPS #64#
#endif
ksmps  = $KSMPS
nchnls = 2
0dbfs  = 1

; ksmps is pinned to the graph quantum on purpose. csound's control rate is a
; latency term in its own right: the engine cannot respond to an input sooner
; than the control block it lands in, so a large ksmps would show up in this
; measurement as csound "costing" latency that is really a patch setting. At
; ksmps = quantum the block boundary and the graph boundary coincide and the
; number isolates the backend.

instr 1
  al, ar ins
  outs   al, ar
endin
</CsInstruments>
<CsScore>
i1 0 86400
e
</CsScore>
</CsoundSynthesizer>
