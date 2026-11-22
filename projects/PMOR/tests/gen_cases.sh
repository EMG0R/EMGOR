#!/usr/bin/env bash
# Regenerates tests/cases/*.scn (deterministic scenario scripts). Cases:
#   drum_G_SS   4 groups x 16 sounds, one hit each
#   drumv_*     pitch/tone/reverse/length variants
#   synth_A     8 algorithms, 3-note chord, release
#   synthseq_*  sequenced + generative synth (octave rules, arp)
#   smp_*       sampler playback with pitch/formant/reverse/cutoff
#   seq_*       three-lane sequencer, swing, tempo, record metronome
#   fx_BBB      all 128 FX ids (8 banks x 16) held over a drum+synth+sampler loop
set -euo pipefail
d="$(cd "$(dirname "$0")" && pwd)/cases"; rm -rf "$d"; mkdir -p "$d"
# first pad of each drum group (pad layout in engine/drums.cpp: TL perc, TR hats, BL kicks, BR snares)
declare -a GP=( 8 10 2 0 )   # group index DG_KICK=0,SNARE=1,HAT=2,PERC=3 -> a pad inside that group (pad = row*4+col)
for g in 0 1 2 3; do for s in $(seq 0 15); do
  f=$(printf "%s/drum_%d_%02d.scn" "$d" $g $s)
  { echo "seconds 1.2"; echo "set drums.pad.${GP[$g]}.sound $s"; echo "at 0.0 drum ${GP[$g]} 1.0"; } > "$f"
done; done
{ echo "seconds 1.2"; echo "set drums.pad.8.pitch 0.7"; echo "set drums.pad.8.tone 0.9"; echo "at 0 drum 8 0.8"; } > $d/drumv_kick_pitch.scn
{ echo "seconds 1.2"; echo "set drums.pad.10.reverse 1"; echo "set drums.pad.10.length 0.8"; echo "at 0 drum 10 1.0"; } > $d/drumv_snare_rev.scn
{ echo "seconds 1.2"; echo "set drums.pad.2.pitch -0.6"; echo "set drums.pad.2.tone 0.1"; echo "set drums.pad.2.vol 1.7"; echo "at 0 drum 2 1.0"; echo "at 0.2 drum 2 0.5"; } > $d/drumv_hat_retrig.scn
{ echo "seconds 1.2"; echo "set drums.pad.0.length 0.1"; echo "set drums.pad.0.sound 7"; echo "at 0 drum 0 1.0"; echo "at 0.01 drum 0 1.0"; } > $d/drumv_perc_choke.scn
for a in 0 1 2 3 4 5 6 7; do
  { echo "seconds 2.0"; echo "set syn.algo $a"
    echo "at 0.0 synth_on 0 0.9"; echo "at 0.0 synth_on 4 0.8"; echo "at 0.0 synth_on 9 0.7"
    echo "at 1.0 synth_off 0"; echo "at 1.0 synth_off 4"; echo "at 1.0 synth_off 9"; } > $d/synth_$a.scn
done
for a in 0 3 4; do
  { echo "seconds 4.0"; echo "set syn.algo $a"; echo "set seq.1.mode 1"; echo "set seq.1.oct_amount 2"; echo "set seq.1.oct_prob 0.8"
    echo "seq_step 1 0 0x1"; echo "seq_step 1 4 0x10"; echo "seq_step 1 8 0x200"; echo "seq_step 1 12 0x5"; } > $d/synthseq_$a.scn
done
{ echo "seconds 4.0"; echo "set syn.algo 1"; echo "set seq.1.mode 2"; echo "set seq.1.density 0.6"; echo "set seq.1.probability 0.9"; echo "set seq.1.arp_on 1"; echo "at 0 pads_held 0x11"; } > $d/synthseq_gen_arp.scn
for v in "0 0 1 0" "0.5 0 1 0" "-0.5 0.6 0.4 0" "0 0 1 1" ; do set -- $v
  n=$(echo "$v" | tr -d ' .-' ); { echo "seconds 2.0"; echo "smp_fill 0 60000"; echo "set smp.slot.0.pitch $1"; echo "set smp.slot.0.formant $2"; echo "set smp.slot.0.cutoff $3"; echo "set smp.slot.0.reverse $4"; echo "at 0 smp 0 1.0"; } > $d/smp_$n.scn
done
{ echo "seconds 4.0"; echo "smp_fill 1 48000"; echo "smp_fill 2 30000"; echo "set seq.2.mode 1"; echo "seq_step 2 0 0x2"; echo "seq_step 2 3 0x4"; echo "seq_step 2 8 0x2"; echo "seq_step 2 11 0x4"; } > $d/smp_seq.scn
{ echo "seconds 6.0"; echo "bpm 133"; echo "set clk.swing 0.4"
  for e in 0 1 2; do echo "set seq.$e.mode 1"; done
  echo "smp_fill 0 40000"; echo "set smp.slot.0.cutoff 1"
  for s in 0 4 8 12; do echo "seq_step 0 $s 0x1000"; done
  for s in 4 12; do echo "seq_step 0 $s 0x4000"; done
  for s in 0 2 4 6 8 10 12 14; do echo "seq_step 0 $s 0x4"; done
  echo "seq_step 1 0 0x1"; echo "seq_step 1 6 0x40"; echo "seq_step 1 10 0x8"; echo "seq_step 2 2 0x1"; echo "seq_step 2 9 0x1"; } > $d/seq_three_lanes.scn
{ echo "seconds 6.0"; echo "bpm 90"; echo "set seq.0.mode 2"; echo "set seq.0.density 0.7"; echo "set seq.0.probability 0.8"; echo "set seq.1.mode 2"; echo "set seq.1.density 0.4"; echo "set seq.1.probability 0.7"; echo "set seq.1.oct_amount 3"; echo "set seq.1.oct_prob 0.6"; } > $d/seq_generative.scn
{ echo "seconds 4.0"; echo "bpm 100"; echo "set seq.0.rec_on 1"; echo "set seq.0.mode 1"; echo "seq_step 0 0 0x1000"; } > $d/seq_record_metro.scn
{ echo "seconds 6.0"; echo "set seq.0.bars 2"; echo "set seq.0.spb 12"; echo "set seq.1.bars 3"; echo "set seq.1.mode 1"; echo "set seq.0.mode 1"; for s in 0 5 11 13 20; do echo "seq_step 0 $s 0x1000"; done; echo "seq_step 1 1 0x3"; echo "seq_step 1 30 0x8"; } > $d/seq_odd_loops.scn
# FX: every id over a backdrop loop (drums + synth + sampler), FSR held at 0.7
for id in $(seq 0 127); do
  f=$(printf "%s/fx_%03d.scn" "$d" $id)
  { echo "seconds 3.0"; echo "bpm 120"; echo "smp_fill 0 48000"; echo "set g.fsr 0.7"; echo "set g.lidar 0.4"; echo "set g.accel_x 0.2"
    echo "set seq.0.mode 1"; echo "set seq.1.mode 1"; echo "set seq.2.mode 1"
    for s in 0 4 8 12; do echo "seq_step 0 $s 0x1000"; done
    for s in 4 12; do echo "seq_step 0 $s 0x4000"; done
    for s in 0 2 4 6 8 10 12 14; do echo "seq_step 0 $s 0x4"; done
    echo "seq_step 1 0 0x11"; echo "seq_step 1 8 0x42"; echo "seq_step 2 2 0x1"; echo "seq_step 2 10 0x1"
    echo "at 0.5 fx_press $id"; echo "at 2.0 fx_release $id"; } > "$f"
done
ls "$d" | wc -l
