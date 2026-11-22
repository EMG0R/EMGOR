// sept_render_core.ck
// Septagraph v1 reference renderer -- DSP core (Task 2.1).
//
// This file contains NO OscIn / OscOut. It reads only the globals below --
// the identical file runs unmodified under WebChucK (Task 2.3), driven by
// chuck.setFloat()/setInt()/broadcastEvent() from JS instead of by
// sept_render_osc.ck. See septagraph/chuck/README.md for what each axis
// should sound like, and docs/AXIS_SEMANTICS_v1.md / WIRE_FORMAT_v1.md for
// the protocol this implements.
//
// Interface (exact names, required for WebChucK setFloat-by-name):
//   per voice slot n in 0..15:
//     float v_mu_n_0 .. v_mu_n_7      -- coordinate (freq,loud,harm,rate,scale,az,spread,dist)
//     float v_sig_n_0 .. v_sig_n_7    -- per-axis sigma (renderer liberty)
//     float v_vel_n                   -- velocity 0..1
//     int   v_gate_n                  -- 1 = held/onset, 0 = release
//     Event v_hit_n                   -- broadcast whenever v_gate_n or the
//                                        voice coordinate changes meaningfully
//   shape (play layer only, v1):
//     float sh_dur                    -- septagraph duration, seconds
//     int   sh_n                      -- number of valid knots (0 = static/no shape)
//     float sh_t_j                    -- knot j normalized time, j in 0..31
//     float sh_mu_j_0 .. sh_mu_j_7    -- knot j coordinate
//     float sh_sig_j_0 .. sh_sig_j_7  -- knot j sigma
//
// DSP per voice, honoring all 8 coordinate indices:
//   freq  (0) -> SawOsc+SinOsc fundamental, hz = 20 * 1000^k
//   harm  (2) -> crossfade tone bank <-> Noise+BPF(Q=2) centered at hz
//   loud  (1) -> overall gain, db = -60 + 60*k, combined with note velocity
//   rate  (3) -> sine LFO amplitude-modulates the voice; k<0.05 => depth 0
//   scale (4) -> feedback comb (Delay) tuned so ripple density ~= cyc/oct;
//                delay_s = cyc_per_oct / hz (documented approximation --
//                a true auditory "scale" percept needs a filterbank, not a
//                single comb; this is the reference renderer's liberty)
//   az    (5) -> Pan2.pan = -1 + 2k
//   spread(6) -> dry pan vs a PRCRev-diffused copy
//   dist  (7) -> dry/wet into NRev + LPF cutoff = 20000*(1-0.8k)
//
// Shape walk: on hit, the current shared play-layer shape is translated so
// knot0's mu == the hit's mu, and a control-rate shred walks it over
// sh_dur seconds. Per-axis sigma is realized with ONE random draw per hit
// (not re-drawn every step): mu_realized[i] = mu[i] + rand(-1,1)*sigma_eff[i],
// sigma_eff[i] = sqrt(hit.sigma[i]^2 + knot.sigma[i]^2). Release triggers an
// 80ms ADSR release tail (v_gate_n -> 0).

16 => int N_VOICES;
32 => int MAX_KNOTS;
6.283185307179586 => float TWO_PI;

// ---- Named per-voice globals (exact names; do not rename) ----
global float v_mu_0_0;
global float v_mu_0_1;
global float v_mu_0_2;
global float v_mu_0_3;
global float v_mu_0_4;
global float v_mu_0_5;
global float v_mu_0_6;
global float v_mu_0_7;
global float v_sig_0_0;
global float v_sig_0_1;
global float v_sig_0_2;
global float v_sig_0_3;
global float v_sig_0_4;
global float v_sig_0_5;
global float v_sig_0_6;
global float v_sig_0_7;
global float v_vel_0;
global int v_gate_0;
global Event v_hit_0;

global float v_mu_1_0;
global float v_mu_1_1;
global float v_mu_1_2;
global float v_mu_1_3;
global float v_mu_1_4;
global float v_mu_1_5;
global float v_mu_1_6;
global float v_mu_1_7;
global float v_sig_1_0;
global float v_sig_1_1;
global float v_sig_1_2;
global float v_sig_1_3;
global float v_sig_1_4;
global float v_sig_1_5;
global float v_sig_1_6;
global float v_sig_1_7;
global float v_vel_1;
global int v_gate_1;
global Event v_hit_1;

global float v_mu_2_0;
global float v_mu_2_1;
global float v_mu_2_2;
global float v_mu_2_3;
global float v_mu_2_4;
global float v_mu_2_5;
global float v_mu_2_6;
global float v_mu_2_7;
global float v_sig_2_0;
global float v_sig_2_1;
global float v_sig_2_2;
global float v_sig_2_3;
global float v_sig_2_4;
global float v_sig_2_5;
global float v_sig_2_6;
global float v_sig_2_7;
global float v_vel_2;
global int v_gate_2;
global Event v_hit_2;

global float v_mu_3_0;
global float v_mu_3_1;
global float v_mu_3_2;
global float v_mu_3_3;
global float v_mu_3_4;
global float v_mu_3_5;
global float v_mu_3_6;
global float v_mu_3_7;
global float v_sig_3_0;
global float v_sig_3_1;
global float v_sig_3_2;
global float v_sig_3_3;
global float v_sig_3_4;
global float v_sig_3_5;
global float v_sig_3_6;
global float v_sig_3_7;
global float v_vel_3;
global int v_gate_3;
global Event v_hit_3;

global float v_mu_4_0;
global float v_mu_4_1;
global float v_mu_4_2;
global float v_mu_4_3;
global float v_mu_4_4;
global float v_mu_4_5;
global float v_mu_4_6;
global float v_mu_4_7;
global float v_sig_4_0;
global float v_sig_4_1;
global float v_sig_4_2;
global float v_sig_4_3;
global float v_sig_4_4;
global float v_sig_4_5;
global float v_sig_4_6;
global float v_sig_4_7;
global float v_vel_4;
global int v_gate_4;
global Event v_hit_4;

global float v_mu_5_0;
global float v_mu_5_1;
global float v_mu_5_2;
global float v_mu_5_3;
global float v_mu_5_4;
global float v_mu_5_5;
global float v_mu_5_6;
global float v_mu_5_7;
global float v_sig_5_0;
global float v_sig_5_1;
global float v_sig_5_2;
global float v_sig_5_3;
global float v_sig_5_4;
global float v_sig_5_5;
global float v_sig_5_6;
global float v_sig_5_7;
global float v_vel_5;
global int v_gate_5;
global Event v_hit_5;

global float v_mu_6_0;
global float v_mu_6_1;
global float v_mu_6_2;
global float v_mu_6_3;
global float v_mu_6_4;
global float v_mu_6_5;
global float v_mu_6_6;
global float v_mu_6_7;
global float v_sig_6_0;
global float v_sig_6_1;
global float v_sig_6_2;
global float v_sig_6_3;
global float v_sig_6_4;
global float v_sig_6_5;
global float v_sig_6_6;
global float v_sig_6_7;
global float v_vel_6;
global int v_gate_6;
global Event v_hit_6;

global float v_mu_7_0;
global float v_mu_7_1;
global float v_mu_7_2;
global float v_mu_7_3;
global float v_mu_7_4;
global float v_mu_7_5;
global float v_mu_7_6;
global float v_mu_7_7;
global float v_sig_7_0;
global float v_sig_7_1;
global float v_sig_7_2;
global float v_sig_7_3;
global float v_sig_7_4;
global float v_sig_7_5;
global float v_sig_7_6;
global float v_sig_7_7;
global float v_vel_7;
global int v_gate_7;
global Event v_hit_7;

global float v_mu_8_0;
global float v_mu_8_1;
global float v_mu_8_2;
global float v_mu_8_3;
global float v_mu_8_4;
global float v_mu_8_5;
global float v_mu_8_6;
global float v_mu_8_7;
global float v_sig_8_0;
global float v_sig_8_1;
global float v_sig_8_2;
global float v_sig_8_3;
global float v_sig_8_4;
global float v_sig_8_5;
global float v_sig_8_6;
global float v_sig_8_7;
global float v_vel_8;
global int v_gate_8;
global Event v_hit_8;

global float v_mu_9_0;
global float v_mu_9_1;
global float v_mu_9_2;
global float v_mu_9_3;
global float v_mu_9_4;
global float v_mu_9_5;
global float v_mu_9_6;
global float v_mu_9_7;
global float v_sig_9_0;
global float v_sig_9_1;
global float v_sig_9_2;
global float v_sig_9_3;
global float v_sig_9_4;
global float v_sig_9_5;
global float v_sig_9_6;
global float v_sig_9_7;
global float v_vel_9;
global int v_gate_9;
global Event v_hit_9;

global float v_mu_10_0;
global float v_mu_10_1;
global float v_mu_10_2;
global float v_mu_10_3;
global float v_mu_10_4;
global float v_mu_10_5;
global float v_mu_10_6;
global float v_mu_10_7;
global float v_sig_10_0;
global float v_sig_10_1;
global float v_sig_10_2;
global float v_sig_10_3;
global float v_sig_10_4;
global float v_sig_10_5;
global float v_sig_10_6;
global float v_sig_10_7;
global float v_vel_10;
global int v_gate_10;
global Event v_hit_10;

global float v_mu_11_0;
global float v_mu_11_1;
global float v_mu_11_2;
global float v_mu_11_3;
global float v_mu_11_4;
global float v_mu_11_5;
global float v_mu_11_6;
global float v_mu_11_7;
global float v_sig_11_0;
global float v_sig_11_1;
global float v_sig_11_2;
global float v_sig_11_3;
global float v_sig_11_4;
global float v_sig_11_5;
global float v_sig_11_6;
global float v_sig_11_7;
global float v_vel_11;
global int v_gate_11;
global Event v_hit_11;

global float v_mu_12_0;
global float v_mu_12_1;
global float v_mu_12_2;
global float v_mu_12_3;
global float v_mu_12_4;
global float v_mu_12_5;
global float v_mu_12_6;
global float v_mu_12_7;
global float v_sig_12_0;
global float v_sig_12_1;
global float v_sig_12_2;
global float v_sig_12_3;
global float v_sig_12_4;
global float v_sig_12_5;
global float v_sig_12_6;
global float v_sig_12_7;
global float v_vel_12;
global int v_gate_12;
global Event v_hit_12;

global float v_mu_13_0;
global float v_mu_13_1;
global float v_mu_13_2;
global float v_mu_13_3;
global float v_mu_13_4;
global float v_mu_13_5;
global float v_mu_13_6;
global float v_mu_13_7;
global float v_sig_13_0;
global float v_sig_13_1;
global float v_sig_13_2;
global float v_sig_13_3;
global float v_sig_13_4;
global float v_sig_13_5;
global float v_sig_13_6;
global float v_sig_13_7;
global float v_vel_13;
global int v_gate_13;
global Event v_hit_13;

global float v_mu_14_0;
global float v_mu_14_1;
global float v_mu_14_2;
global float v_mu_14_3;
global float v_mu_14_4;
global float v_mu_14_5;
global float v_mu_14_6;
global float v_mu_14_7;
global float v_sig_14_0;
global float v_sig_14_1;
global float v_sig_14_2;
global float v_sig_14_3;
global float v_sig_14_4;
global float v_sig_14_5;
global float v_sig_14_6;
global float v_sig_14_7;
global float v_vel_14;
global int v_gate_14;
global Event v_hit_14;

global float v_mu_15_0;
global float v_mu_15_1;
global float v_mu_15_2;
global float v_mu_15_3;
global float v_mu_15_4;
global float v_mu_15_5;
global float v_mu_15_6;
global float v_mu_15_7;
global float v_sig_15_0;
global float v_sig_15_1;
global float v_sig_15_2;
global float v_sig_15_3;
global float v_sig_15_4;
global float v_sig_15_5;
global float v_sig_15_6;
global float v_sig_15_7;
global float v_vel_15;
global int v_gate_15;
global Event v_hit_15;

// ---- Named shape globals (play layer, v1) ----
global float sh_dur;
global int sh_n;
global float sh_t_0;
global float sh_mu_0_0;
global float sh_mu_0_1;
global float sh_mu_0_2;
global float sh_mu_0_3;
global float sh_mu_0_4;
global float sh_mu_0_5;
global float sh_mu_0_6;
global float sh_mu_0_7;
global float sh_sig_0_0;
global float sh_sig_0_1;
global float sh_sig_0_2;
global float sh_sig_0_3;
global float sh_sig_0_4;
global float sh_sig_0_5;
global float sh_sig_0_6;
global float sh_sig_0_7;
global float sh_t_1;
global float sh_mu_1_0;
global float sh_mu_1_1;
global float sh_mu_1_2;
global float sh_mu_1_3;
global float sh_mu_1_4;
global float sh_mu_1_5;
global float sh_mu_1_6;
global float sh_mu_1_7;
global float sh_sig_1_0;
global float sh_sig_1_1;
global float sh_sig_1_2;
global float sh_sig_1_3;
global float sh_sig_1_4;
global float sh_sig_1_5;
global float sh_sig_1_6;
global float sh_sig_1_7;
global float sh_t_2;
global float sh_mu_2_0;
global float sh_mu_2_1;
global float sh_mu_2_2;
global float sh_mu_2_3;
global float sh_mu_2_4;
global float sh_mu_2_5;
global float sh_mu_2_6;
global float sh_mu_2_7;
global float sh_sig_2_0;
global float sh_sig_2_1;
global float sh_sig_2_2;
global float sh_sig_2_3;
global float sh_sig_2_4;
global float sh_sig_2_5;
global float sh_sig_2_6;
global float sh_sig_2_7;
global float sh_t_3;
global float sh_mu_3_0;
global float sh_mu_3_1;
global float sh_mu_3_2;
global float sh_mu_3_3;
global float sh_mu_3_4;
global float sh_mu_3_5;
global float sh_mu_3_6;
global float sh_mu_3_7;
global float sh_sig_3_0;
global float sh_sig_3_1;
global float sh_sig_3_2;
global float sh_sig_3_3;
global float sh_sig_3_4;
global float sh_sig_3_5;
global float sh_sig_3_6;
global float sh_sig_3_7;
global float sh_t_4;
global float sh_mu_4_0;
global float sh_mu_4_1;
global float sh_mu_4_2;
global float sh_mu_4_3;
global float sh_mu_4_4;
global float sh_mu_4_5;
global float sh_mu_4_6;
global float sh_mu_4_7;
global float sh_sig_4_0;
global float sh_sig_4_1;
global float sh_sig_4_2;
global float sh_sig_4_3;
global float sh_sig_4_4;
global float sh_sig_4_5;
global float sh_sig_4_6;
global float sh_sig_4_7;
global float sh_t_5;
global float sh_mu_5_0;
global float sh_mu_5_1;
global float sh_mu_5_2;
global float sh_mu_5_3;
global float sh_mu_5_4;
global float sh_mu_5_5;
global float sh_mu_5_6;
global float sh_mu_5_7;
global float sh_sig_5_0;
global float sh_sig_5_1;
global float sh_sig_5_2;
global float sh_sig_5_3;
global float sh_sig_5_4;
global float sh_sig_5_5;
global float sh_sig_5_6;
global float sh_sig_5_7;
global float sh_t_6;
global float sh_mu_6_0;
global float sh_mu_6_1;
global float sh_mu_6_2;
global float sh_mu_6_3;
global float sh_mu_6_4;
global float sh_mu_6_5;
global float sh_mu_6_6;
global float sh_mu_6_7;
global float sh_sig_6_0;
global float sh_sig_6_1;
global float sh_sig_6_2;
global float sh_sig_6_3;
global float sh_sig_6_4;
global float sh_sig_6_5;
global float sh_sig_6_6;
global float sh_sig_6_7;
global float sh_t_7;
global float sh_mu_7_0;
global float sh_mu_7_1;
global float sh_mu_7_2;
global float sh_mu_7_3;
global float sh_mu_7_4;
global float sh_mu_7_5;
global float sh_mu_7_6;
global float sh_mu_7_7;
global float sh_sig_7_0;
global float sh_sig_7_1;
global float sh_sig_7_2;
global float sh_sig_7_3;
global float sh_sig_7_4;
global float sh_sig_7_5;
global float sh_sig_7_6;
global float sh_sig_7_7;
global float sh_t_8;
global float sh_mu_8_0;
global float sh_mu_8_1;
global float sh_mu_8_2;
global float sh_mu_8_3;
global float sh_mu_8_4;
global float sh_mu_8_5;
global float sh_mu_8_6;
global float sh_mu_8_7;
global float sh_sig_8_0;
global float sh_sig_8_1;
global float sh_sig_8_2;
global float sh_sig_8_3;
global float sh_sig_8_4;
global float sh_sig_8_5;
global float sh_sig_8_6;
global float sh_sig_8_7;
global float sh_t_9;
global float sh_mu_9_0;
global float sh_mu_9_1;
global float sh_mu_9_2;
global float sh_mu_9_3;
global float sh_mu_9_4;
global float sh_mu_9_5;
global float sh_mu_9_6;
global float sh_mu_9_7;
global float sh_sig_9_0;
global float sh_sig_9_1;
global float sh_sig_9_2;
global float sh_sig_9_3;
global float sh_sig_9_4;
global float sh_sig_9_5;
global float sh_sig_9_6;
global float sh_sig_9_7;
global float sh_t_10;
global float sh_mu_10_0;
global float sh_mu_10_1;
global float sh_mu_10_2;
global float sh_mu_10_3;
global float sh_mu_10_4;
global float sh_mu_10_5;
global float sh_mu_10_6;
global float sh_mu_10_7;
global float sh_sig_10_0;
global float sh_sig_10_1;
global float sh_sig_10_2;
global float sh_sig_10_3;
global float sh_sig_10_4;
global float sh_sig_10_5;
global float sh_sig_10_6;
global float sh_sig_10_7;
global float sh_t_11;
global float sh_mu_11_0;
global float sh_mu_11_1;
global float sh_mu_11_2;
global float sh_mu_11_3;
global float sh_mu_11_4;
global float sh_mu_11_5;
global float sh_mu_11_6;
global float sh_mu_11_7;
global float sh_sig_11_0;
global float sh_sig_11_1;
global float sh_sig_11_2;
global float sh_sig_11_3;
global float sh_sig_11_4;
global float sh_sig_11_5;
global float sh_sig_11_6;
global float sh_sig_11_7;
global float sh_t_12;
global float sh_mu_12_0;
global float sh_mu_12_1;
global float sh_mu_12_2;
global float sh_mu_12_3;
global float sh_mu_12_4;
global float sh_mu_12_5;
global float sh_mu_12_6;
global float sh_mu_12_7;
global float sh_sig_12_0;
global float sh_sig_12_1;
global float sh_sig_12_2;
global float sh_sig_12_3;
global float sh_sig_12_4;
global float sh_sig_12_5;
global float sh_sig_12_6;
global float sh_sig_12_7;
global float sh_t_13;
global float sh_mu_13_0;
global float sh_mu_13_1;
global float sh_mu_13_2;
global float sh_mu_13_3;
global float sh_mu_13_4;
global float sh_mu_13_5;
global float sh_mu_13_6;
global float sh_mu_13_7;
global float sh_sig_13_0;
global float sh_sig_13_1;
global float sh_sig_13_2;
global float sh_sig_13_3;
global float sh_sig_13_4;
global float sh_sig_13_5;
global float sh_sig_13_6;
global float sh_sig_13_7;
global float sh_t_14;
global float sh_mu_14_0;
global float sh_mu_14_1;
global float sh_mu_14_2;
global float sh_mu_14_3;
global float sh_mu_14_4;
global float sh_mu_14_5;
global float sh_mu_14_6;
global float sh_mu_14_7;
global float sh_sig_14_0;
global float sh_sig_14_1;
global float sh_sig_14_2;
global float sh_sig_14_3;
global float sh_sig_14_4;
global float sh_sig_14_5;
global float sh_sig_14_6;
global float sh_sig_14_7;
global float sh_t_15;
global float sh_mu_15_0;
global float sh_mu_15_1;
global float sh_mu_15_2;
global float sh_mu_15_3;
global float sh_mu_15_4;
global float sh_mu_15_5;
global float sh_mu_15_6;
global float sh_mu_15_7;
global float sh_sig_15_0;
global float sh_sig_15_1;
global float sh_sig_15_2;
global float sh_sig_15_3;
global float sh_sig_15_4;
global float sh_sig_15_5;
global float sh_sig_15_6;
global float sh_sig_15_7;
global float sh_t_16;
global float sh_mu_16_0;
global float sh_mu_16_1;
global float sh_mu_16_2;
global float sh_mu_16_3;
global float sh_mu_16_4;
global float sh_mu_16_5;
global float sh_mu_16_6;
global float sh_mu_16_7;
global float sh_sig_16_0;
global float sh_sig_16_1;
global float sh_sig_16_2;
global float sh_sig_16_3;
global float sh_sig_16_4;
global float sh_sig_16_5;
global float sh_sig_16_6;
global float sh_sig_16_7;
global float sh_t_17;
global float sh_mu_17_0;
global float sh_mu_17_1;
global float sh_mu_17_2;
global float sh_mu_17_3;
global float sh_mu_17_4;
global float sh_mu_17_5;
global float sh_mu_17_6;
global float sh_mu_17_7;
global float sh_sig_17_0;
global float sh_sig_17_1;
global float sh_sig_17_2;
global float sh_sig_17_3;
global float sh_sig_17_4;
global float sh_sig_17_5;
global float sh_sig_17_6;
global float sh_sig_17_7;
global float sh_t_18;
global float sh_mu_18_0;
global float sh_mu_18_1;
global float sh_mu_18_2;
global float sh_mu_18_3;
global float sh_mu_18_4;
global float sh_mu_18_5;
global float sh_mu_18_6;
global float sh_mu_18_7;
global float sh_sig_18_0;
global float sh_sig_18_1;
global float sh_sig_18_2;
global float sh_sig_18_3;
global float sh_sig_18_4;
global float sh_sig_18_5;
global float sh_sig_18_6;
global float sh_sig_18_7;
global float sh_t_19;
global float sh_mu_19_0;
global float sh_mu_19_1;
global float sh_mu_19_2;
global float sh_mu_19_3;
global float sh_mu_19_4;
global float sh_mu_19_5;
global float sh_mu_19_6;
global float sh_mu_19_7;
global float sh_sig_19_0;
global float sh_sig_19_1;
global float sh_sig_19_2;
global float sh_sig_19_3;
global float sh_sig_19_4;
global float sh_sig_19_5;
global float sh_sig_19_6;
global float sh_sig_19_7;
global float sh_t_20;
global float sh_mu_20_0;
global float sh_mu_20_1;
global float sh_mu_20_2;
global float sh_mu_20_3;
global float sh_mu_20_4;
global float sh_mu_20_5;
global float sh_mu_20_6;
global float sh_mu_20_7;
global float sh_sig_20_0;
global float sh_sig_20_1;
global float sh_sig_20_2;
global float sh_sig_20_3;
global float sh_sig_20_4;
global float sh_sig_20_5;
global float sh_sig_20_6;
global float sh_sig_20_7;
global float sh_t_21;
global float sh_mu_21_0;
global float sh_mu_21_1;
global float sh_mu_21_2;
global float sh_mu_21_3;
global float sh_mu_21_4;
global float sh_mu_21_5;
global float sh_mu_21_6;
global float sh_mu_21_7;
global float sh_sig_21_0;
global float sh_sig_21_1;
global float sh_sig_21_2;
global float sh_sig_21_3;
global float sh_sig_21_4;
global float sh_sig_21_5;
global float sh_sig_21_6;
global float sh_sig_21_7;
global float sh_t_22;
global float sh_mu_22_0;
global float sh_mu_22_1;
global float sh_mu_22_2;
global float sh_mu_22_3;
global float sh_mu_22_4;
global float sh_mu_22_5;
global float sh_mu_22_6;
global float sh_mu_22_7;
global float sh_sig_22_0;
global float sh_sig_22_1;
global float sh_sig_22_2;
global float sh_sig_22_3;
global float sh_sig_22_4;
global float sh_sig_22_5;
global float sh_sig_22_6;
global float sh_sig_22_7;
global float sh_t_23;
global float sh_mu_23_0;
global float sh_mu_23_1;
global float sh_mu_23_2;
global float sh_mu_23_3;
global float sh_mu_23_4;
global float sh_mu_23_5;
global float sh_mu_23_6;
global float sh_mu_23_7;
global float sh_sig_23_0;
global float sh_sig_23_1;
global float sh_sig_23_2;
global float sh_sig_23_3;
global float sh_sig_23_4;
global float sh_sig_23_5;
global float sh_sig_23_6;
global float sh_sig_23_7;
global float sh_t_24;
global float sh_mu_24_0;
global float sh_mu_24_1;
global float sh_mu_24_2;
global float sh_mu_24_3;
global float sh_mu_24_4;
global float sh_mu_24_5;
global float sh_mu_24_6;
global float sh_mu_24_7;
global float sh_sig_24_0;
global float sh_sig_24_1;
global float sh_sig_24_2;
global float sh_sig_24_3;
global float sh_sig_24_4;
global float sh_sig_24_5;
global float sh_sig_24_6;
global float sh_sig_24_7;
global float sh_t_25;
global float sh_mu_25_0;
global float sh_mu_25_1;
global float sh_mu_25_2;
global float sh_mu_25_3;
global float sh_mu_25_4;
global float sh_mu_25_5;
global float sh_mu_25_6;
global float sh_mu_25_7;
global float sh_sig_25_0;
global float sh_sig_25_1;
global float sh_sig_25_2;
global float sh_sig_25_3;
global float sh_sig_25_4;
global float sh_sig_25_5;
global float sh_sig_25_6;
global float sh_sig_25_7;
global float sh_t_26;
global float sh_mu_26_0;
global float sh_mu_26_1;
global float sh_mu_26_2;
global float sh_mu_26_3;
global float sh_mu_26_4;
global float sh_mu_26_5;
global float sh_mu_26_6;
global float sh_mu_26_7;
global float sh_sig_26_0;
global float sh_sig_26_1;
global float sh_sig_26_2;
global float sh_sig_26_3;
global float sh_sig_26_4;
global float sh_sig_26_5;
global float sh_sig_26_6;
global float sh_sig_26_7;
global float sh_t_27;
global float sh_mu_27_0;
global float sh_mu_27_1;
global float sh_mu_27_2;
global float sh_mu_27_3;
global float sh_mu_27_4;
global float sh_mu_27_5;
global float sh_mu_27_6;
global float sh_mu_27_7;
global float sh_sig_27_0;
global float sh_sig_27_1;
global float sh_sig_27_2;
global float sh_sig_27_3;
global float sh_sig_27_4;
global float sh_sig_27_5;
global float sh_sig_27_6;
global float sh_sig_27_7;
global float sh_t_28;
global float sh_mu_28_0;
global float sh_mu_28_1;
global float sh_mu_28_2;
global float sh_mu_28_3;
global float sh_mu_28_4;
global float sh_mu_28_5;
global float sh_mu_28_6;
global float sh_mu_28_7;
global float sh_sig_28_0;
global float sh_sig_28_1;
global float sh_sig_28_2;
global float sh_sig_28_3;
global float sh_sig_28_4;
global float sh_sig_28_5;
global float sh_sig_28_6;
global float sh_sig_28_7;
global float sh_t_29;
global float sh_mu_29_0;
global float sh_mu_29_1;
global float sh_mu_29_2;
global float sh_mu_29_3;
global float sh_mu_29_4;
global float sh_mu_29_5;
global float sh_mu_29_6;
global float sh_mu_29_7;
global float sh_sig_29_0;
global float sh_sig_29_1;
global float sh_sig_29_2;
global float sh_sig_29_3;
global float sh_sig_29_4;
global float sh_sig_29_5;
global float sh_sig_29_6;
global float sh_sig_29_7;
global float sh_t_30;
global float sh_mu_30_0;
global float sh_mu_30_1;
global float sh_mu_30_2;
global float sh_mu_30_3;
global float sh_mu_30_4;
global float sh_mu_30_5;
global float sh_mu_30_6;
global float sh_mu_30_7;
global float sh_sig_30_0;
global float sh_sig_30_1;
global float sh_sig_30_2;
global float sh_sig_30_3;
global float sh_sig_30_4;
global float sh_sig_30_5;
global float sh_sig_30_6;
global float sh_sig_30_7;
global float sh_t_31;
global float sh_mu_31_0;
global float sh_mu_31_1;
global float sh_mu_31_2;
global float sh_mu_31_3;
global float sh_mu_31_4;
global float sh_mu_31_5;
global float sh_mu_31_6;
global float sh_mu_31_7;
global float sh_sig_31_0;
global float sh_sig_31_1;
global float sh_sig_31_2;
global float sh_sig_31_3;
global float sh_sig_31_4;
global float sh_sig_31_5;
global float sh_sig_31_6;
global float sh_sig_31_7;

// ---- Internal engine state: real arrays (not part of the named interface) ----
SawOsc eSaw[N_VOICES];
SinOsc eSin[N_VOICES];
Noise eNoise[N_VOICES];
BPF eBpf[N_VOICES];
Gain eToneGain[N_VOICES];
Gain eNoiseGain[N_VOICES];
Gain eSum[N_VOICES];
ADSR eEnv[N_VOICES];
Gain eCombIn[N_VOICES];
Delay eComb[N_VOICES];
Gain eCombFb[N_VOICES];
Gain eCombDry[N_VOICES];
Gain eCombWet[N_VOICES];
Gain eScaleOut[N_VOICES];
Gain eAmMod[N_VOICES];
Gain eSpreadDry[N_VOICES];
PRCRev eDiff[N_VOICES];
Gain eSpreadWet[N_VOICES];
Gain eDistIn[N_VOICES];
LPF eLpf[N_VOICES];
Gain eDistDry[N_VOICES];
NRev eDistRev[N_VOICES];
Gain eDistWet[N_VOICES];
Pan2 ePan[N_VOICES];

float eVel[N_VOICES];
float eRateHz[N_VOICES];
float eAmDepth[N_VOICES];
int eWalkId[N_VOICES];

// shared play-layer shape snapshot, gathered from the named sh_* globals
// once per hit (flattened: index = knot*8 + axis)
float gShapeT[MAX_KNOTS];
float gShapeMu[MAX_KNOTS * 8];
float gShapeSig[MAX_KNOTS * 8];
0 => int gShapeN;

// ---- wire the DSP graph (real loop -- this part needs no unrolling) ----
for (0 => int n; n < N_VOICES; n++) {
    eSaw[n] => eToneGain[n];
    eSin[n] => eToneGain[n];
    eNoise[n] => eBpf[n] => eNoiseGain[n];
    eToneGain[n] => eSum[n];
    eNoiseGain[n] => eSum[n];
    eSum[n] => eEnv[n];
    eEnv[n] => eAmMod[n];
    eAmMod[n] => eCombIn[n];

    eCombIn[n] => eCombDry[n] => eScaleOut[n];
    eCombIn[n] => eComb[n];
    eComb[n] => eCombFb[n] => eComb[n];   // feedback loop (comb resonator)
    eComb[n] => eCombWet[n] => eScaleOut[n];

    eScaleOut[n] => eSpreadDry[n] => eDistIn[n];
    eScaleOut[n] => eDiff[n] => eSpreadWet[n] => eDistIn[n];

    eDistIn[n] => eLpf[n];
    eLpf[n] => eDistDry[n] => ePan[n];
    eLpf[n] => eDistRev[n] => eDistWet[n] => ePan[n];

    ePan[n] => dac;

    // defaults
    2.0 => eBpf[n].Q;
    0.35 => eCombFb[n].gain;
    0.6 => eCombDry[n].gain;
    0.4 => eCombWet[n].gain;
    45::ms => eComb[n].max;
    2::ms => eComb[n].delay;
    eEnv[n].set(5::ms, 50::ms, 0.75, 80::ms);   // 80ms release per spec
    1.0 => eSpreadDry[n].gain;
    0.0 => eSpreadWet[n].gain;
    1.0 => eDistDry[n].gain;
    0.0 => eDistWet[n].gain;
    20000.0 => eLpf[n].freq;
    1.0 => eAmMod[n].gain;
    0.0 => ePan[n].pan;
    0.7 => eVel[n];
    0.0 => eRateHz[n];
    0.0 => eAmDepth[n];
    0 => eWalkId[n];
}

// ---- shape interpolation (mirrors py Septagraph.at) ----
fun void shapeAt(float tt, float baseMu[], float baseSig[]) {
    if (gShapeN <= 1) {
        for (0 => int i; i < 8; i++) { gShapeMu[i] => baseMu[i]; gShapeSig[i] => baseSig[i]; }
        return;
    }
    if (tt <= gShapeT[0]) {
        for (0 => int i; i < 8; i++) { gShapeMu[i] => baseMu[i]; gShapeSig[i] => baseSig[i]; }
        return;
    }
    for (1 => int k; k < gShapeN; k++) {
        if (tt <= gShapeT[k]) {
            gShapeT[k-1] => float ta;
            gShapeT[k] => float tb;
            (tb - ta) => float span;
            0.0 => float f;
            if (span > 0.0) { (tt - ta) / span => f; }
            for (0 => int i; i < 8; i++) {
                gShapeMu[(k-1)*8+i] => float a;
                gShapeMu[k*8+i] => float b;
                a + (b - a) * f => baseMu[i];
                gShapeSig[(k-1)*8+i] => float sa;
                gShapeSig[k*8+i] => float sb;
                sa + (sb - sa) * f => baseSig[i];
            }
            return;
        }
    }
    for (0 => int i; i < 8; i++) {
        gShapeMu[(gShapeN-1)*8+i] => baseMu[i];
        gShapeSig[(gShapeN-1)*8+i] => baseSig[i];
    }
}

// ---- apply a realized 8-coordinate to a voice's live DSP params ----
fun void applyCoord(int n, float mu[]) {
    20.0 * Math.pow(1000.0, mu[0]) => float hz;
    if (hz < 20.0) 20.0 => hz;
    hz => eSaw[n].freq;
    hz => eSin[n].freq;
    hz => eBpf[n].freq;

    mu[2] => eToneGain[n].gain;
    (1.0 - mu[2]) => eNoiseGain[n].gain;

    Math.pow(10.0, (-60.0 + 60.0 * mu[1]) / 20.0) => float loud;
    loud * (0.5 + 0.5 * eVel[n]) => eCombIn[n].gain;

    0.5 * Math.pow(128.0, mu[3]) => eRateHz[n];
    if (mu[3] < 0.05) { 0.0 => eAmDepth[n]; } else { 0.6 => eAmDepth[n]; }

    0.25 * Math.pow(32.0, mu[4]) => float cyc_per_oct;
    (cyc_per_oct / hz) => float ds;
    if (ds < 0.0005) 0.0005 => ds;
    if (ds > 0.045) 0.045 => ds;
    (ds :: second) => eComb[n].delay;

    (-1.0 + 2.0 * mu[5]) => ePan[n].pan;

    mu[6] => float spread;
    (1.0 - spread) => eSpreadDry[n].gain;
    spread => eSpreadWet[n].gain;

    mu[7] => float dist;
    (1.0 - 0.8 * dist) => eDistDry[n].gain;
    (0.8 * dist) => eDistWet[n].gain;
    (20000.0 * (1.0 - 0.8 * dist)) => eLpf[n].freq;
}

fun void gatherShape() {
    sh_n => gShapeN;
    if (gShapeN < 0) { 0 => gShapeN; }
    if (gShapeN > MAX_KNOTS) { MAX_KNOTS => gShapeN; }
    sh_t_0 => gShapeT[0];
    [sh_mu_0_0, sh_mu_0_1, sh_mu_0_2, sh_mu_0_3, sh_mu_0_4, sh_mu_0_5, sh_mu_0_6, sh_mu_0_7] @=> float _gmu0[];
    [sh_sig_0_0, sh_sig_0_1, sh_sig_0_2, sh_sig_0_3, sh_sig_0_4, sh_sig_0_5, sh_sig_0_6, sh_sig_0_7] @=> float _gsig0[];
    for (0 => int i; i < 8; i++) { _gmu0[i] => gShapeMu[0*8+i]; _gsig0[i] => gShapeSig[0*8+i]; }
    sh_t_1 => gShapeT[1];
    [sh_mu_1_0, sh_mu_1_1, sh_mu_1_2, sh_mu_1_3, sh_mu_1_4, sh_mu_1_5, sh_mu_1_6, sh_mu_1_7] @=> float _gmu1[];
    [sh_sig_1_0, sh_sig_1_1, sh_sig_1_2, sh_sig_1_3, sh_sig_1_4, sh_sig_1_5, sh_sig_1_6, sh_sig_1_7] @=> float _gsig1[];
    for (0 => int i; i < 8; i++) { _gmu1[i] => gShapeMu[1*8+i]; _gsig1[i] => gShapeSig[1*8+i]; }
    sh_t_2 => gShapeT[2];
    [sh_mu_2_0, sh_mu_2_1, sh_mu_2_2, sh_mu_2_3, sh_mu_2_4, sh_mu_2_5, sh_mu_2_6, sh_mu_2_7] @=> float _gmu2[];
    [sh_sig_2_0, sh_sig_2_1, sh_sig_2_2, sh_sig_2_3, sh_sig_2_4, sh_sig_2_5, sh_sig_2_6, sh_sig_2_7] @=> float _gsig2[];
    for (0 => int i; i < 8; i++) { _gmu2[i] => gShapeMu[2*8+i]; _gsig2[i] => gShapeSig[2*8+i]; }
    sh_t_3 => gShapeT[3];
    [sh_mu_3_0, sh_mu_3_1, sh_mu_3_2, sh_mu_3_3, sh_mu_3_4, sh_mu_3_5, sh_mu_3_6, sh_mu_3_7] @=> float _gmu3[];
    [sh_sig_3_0, sh_sig_3_1, sh_sig_3_2, sh_sig_3_3, sh_sig_3_4, sh_sig_3_5, sh_sig_3_6, sh_sig_3_7] @=> float _gsig3[];
    for (0 => int i; i < 8; i++) { _gmu3[i] => gShapeMu[3*8+i]; _gsig3[i] => gShapeSig[3*8+i]; }
    sh_t_4 => gShapeT[4];
    [sh_mu_4_0, sh_mu_4_1, sh_mu_4_2, sh_mu_4_3, sh_mu_4_4, sh_mu_4_5, sh_mu_4_6, sh_mu_4_7] @=> float _gmu4[];
    [sh_sig_4_0, sh_sig_4_1, sh_sig_4_2, sh_sig_4_3, sh_sig_4_4, sh_sig_4_5, sh_sig_4_6, sh_sig_4_7] @=> float _gsig4[];
    for (0 => int i; i < 8; i++) { _gmu4[i] => gShapeMu[4*8+i]; _gsig4[i] => gShapeSig[4*8+i]; }
    sh_t_5 => gShapeT[5];
    [sh_mu_5_0, sh_mu_5_1, sh_mu_5_2, sh_mu_5_3, sh_mu_5_4, sh_mu_5_5, sh_mu_5_6, sh_mu_5_7] @=> float _gmu5[];
    [sh_sig_5_0, sh_sig_5_1, sh_sig_5_2, sh_sig_5_3, sh_sig_5_4, sh_sig_5_5, sh_sig_5_6, sh_sig_5_7] @=> float _gsig5[];
    for (0 => int i; i < 8; i++) { _gmu5[i] => gShapeMu[5*8+i]; _gsig5[i] => gShapeSig[5*8+i]; }
    sh_t_6 => gShapeT[6];
    [sh_mu_6_0, sh_mu_6_1, sh_mu_6_2, sh_mu_6_3, sh_mu_6_4, sh_mu_6_5, sh_mu_6_6, sh_mu_6_7] @=> float _gmu6[];
    [sh_sig_6_0, sh_sig_6_1, sh_sig_6_2, sh_sig_6_3, sh_sig_6_4, sh_sig_6_5, sh_sig_6_6, sh_sig_6_7] @=> float _gsig6[];
    for (0 => int i; i < 8; i++) { _gmu6[i] => gShapeMu[6*8+i]; _gsig6[i] => gShapeSig[6*8+i]; }
    sh_t_7 => gShapeT[7];
    [sh_mu_7_0, sh_mu_7_1, sh_mu_7_2, sh_mu_7_3, sh_mu_7_4, sh_mu_7_5, sh_mu_7_6, sh_mu_7_7] @=> float _gmu7[];
    [sh_sig_7_0, sh_sig_7_1, sh_sig_7_2, sh_sig_7_3, sh_sig_7_4, sh_sig_7_5, sh_sig_7_6, sh_sig_7_7] @=> float _gsig7[];
    for (0 => int i; i < 8; i++) { _gmu7[i] => gShapeMu[7*8+i]; _gsig7[i] => gShapeSig[7*8+i]; }
    sh_t_8 => gShapeT[8];
    [sh_mu_8_0, sh_mu_8_1, sh_mu_8_2, sh_mu_8_3, sh_mu_8_4, sh_mu_8_5, sh_mu_8_6, sh_mu_8_7] @=> float _gmu8[];
    [sh_sig_8_0, sh_sig_8_1, sh_sig_8_2, sh_sig_8_3, sh_sig_8_4, sh_sig_8_5, sh_sig_8_6, sh_sig_8_7] @=> float _gsig8[];
    for (0 => int i; i < 8; i++) { _gmu8[i] => gShapeMu[8*8+i]; _gsig8[i] => gShapeSig[8*8+i]; }
    sh_t_9 => gShapeT[9];
    [sh_mu_9_0, sh_mu_9_1, sh_mu_9_2, sh_mu_9_3, sh_mu_9_4, sh_mu_9_5, sh_mu_9_6, sh_mu_9_7] @=> float _gmu9[];
    [sh_sig_9_0, sh_sig_9_1, sh_sig_9_2, sh_sig_9_3, sh_sig_9_4, sh_sig_9_5, sh_sig_9_6, sh_sig_9_7] @=> float _gsig9[];
    for (0 => int i; i < 8; i++) { _gmu9[i] => gShapeMu[9*8+i]; _gsig9[i] => gShapeSig[9*8+i]; }
    sh_t_10 => gShapeT[10];
    [sh_mu_10_0, sh_mu_10_1, sh_mu_10_2, sh_mu_10_3, sh_mu_10_4, sh_mu_10_5, sh_mu_10_6, sh_mu_10_7] @=> float _gmu10[];
    [sh_sig_10_0, sh_sig_10_1, sh_sig_10_2, sh_sig_10_3, sh_sig_10_4, sh_sig_10_5, sh_sig_10_6, sh_sig_10_7] @=> float _gsig10[];
    for (0 => int i; i < 8; i++) { _gmu10[i] => gShapeMu[10*8+i]; _gsig10[i] => gShapeSig[10*8+i]; }
    sh_t_11 => gShapeT[11];
    [sh_mu_11_0, sh_mu_11_1, sh_mu_11_2, sh_mu_11_3, sh_mu_11_4, sh_mu_11_5, sh_mu_11_6, sh_mu_11_7] @=> float _gmu11[];
    [sh_sig_11_0, sh_sig_11_1, sh_sig_11_2, sh_sig_11_3, sh_sig_11_4, sh_sig_11_5, sh_sig_11_6, sh_sig_11_7] @=> float _gsig11[];
    for (0 => int i; i < 8; i++) { _gmu11[i] => gShapeMu[11*8+i]; _gsig11[i] => gShapeSig[11*8+i]; }
    sh_t_12 => gShapeT[12];
    [sh_mu_12_0, sh_mu_12_1, sh_mu_12_2, sh_mu_12_3, sh_mu_12_4, sh_mu_12_5, sh_mu_12_6, sh_mu_12_7] @=> float _gmu12[];
    [sh_sig_12_0, sh_sig_12_1, sh_sig_12_2, sh_sig_12_3, sh_sig_12_4, sh_sig_12_5, sh_sig_12_6, sh_sig_12_7] @=> float _gsig12[];
    for (0 => int i; i < 8; i++) { _gmu12[i] => gShapeMu[12*8+i]; _gsig12[i] => gShapeSig[12*8+i]; }
    sh_t_13 => gShapeT[13];
    [sh_mu_13_0, sh_mu_13_1, sh_mu_13_2, sh_mu_13_3, sh_mu_13_4, sh_mu_13_5, sh_mu_13_6, sh_mu_13_7] @=> float _gmu13[];
    [sh_sig_13_0, sh_sig_13_1, sh_sig_13_2, sh_sig_13_3, sh_sig_13_4, sh_sig_13_5, sh_sig_13_6, sh_sig_13_7] @=> float _gsig13[];
    for (0 => int i; i < 8; i++) { _gmu13[i] => gShapeMu[13*8+i]; _gsig13[i] => gShapeSig[13*8+i]; }
    sh_t_14 => gShapeT[14];
    [sh_mu_14_0, sh_mu_14_1, sh_mu_14_2, sh_mu_14_3, sh_mu_14_4, sh_mu_14_5, sh_mu_14_6, sh_mu_14_7] @=> float _gmu14[];
    [sh_sig_14_0, sh_sig_14_1, sh_sig_14_2, sh_sig_14_3, sh_sig_14_4, sh_sig_14_5, sh_sig_14_6, sh_sig_14_7] @=> float _gsig14[];
    for (0 => int i; i < 8; i++) { _gmu14[i] => gShapeMu[14*8+i]; _gsig14[i] => gShapeSig[14*8+i]; }
    sh_t_15 => gShapeT[15];
    [sh_mu_15_0, sh_mu_15_1, sh_mu_15_2, sh_mu_15_3, sh_mu_15_4, sh_mu_15_5, sh_mu_15_6, sh_mu_15_7] @=> float _gmu15[];
    [sh_sig_15_0, sh_sig_15_1, sh_sig_15_2, sh_sig_15_3, sh_sig_15_4, sh_sig_15_5, sh_sig_15_6, sh_sig_15_7] @=> float _gsig15[];
    for (0 => int i; i < 8; i++) { _gmu15[i] => gShapeMu[15*8+i]; _gsig15[i] => gShapeSig[15*8+i]; }
    sh_t_16 => gShapeT[16];
    [sh_mu_16_0, sh_mu_16_1, sh_mu_16_2, sh_mu_16_3, sh_mu_16_4, sh_mu_16_5, sh_mu_16_6, sh_mu_16_7] @=> float _gmu16[];
    [sh_sig_16_0, sh_sig_16_1, sh_sig_16_2, sh_sig_16_3, sh_sig_16_4, sh_sig_16_5, sh_sig_16_6, sh_sig_16_7] @=> float _gsig16[];
    for (0 => int i; i < 8; i++) { _gmu16[i] => gShapeMu[16*8+i]; _gsig16[i] => gShapeSig[16*8+i]; }
    sh_t_17 => gShapeT[17];
    [sh_mu_17_0, sh_mu_17_1, sh_mu_17_2, sh_mu_17_3, sh_mu_17_4, sh_mu_17_5, sh_mu_17_6, sh_mu_17_7] @=> float _gmu17[];
    [sh_sig_17_0, sh_sig_17_1, sh_sig_17_2, sh_sig_17_3, sh_sig_17_4, sh_sig_17_5, sh_sig_17_6, sh_sig_17_7] @=> float _gsig17[];
    for (0 => int i; i < 8; i++) { _gmu17[i] => gShapeMu[17*8+i]; _gsig17[i] => gShapeSig[17*8+i]; }
    sh_t_18 => gShapeT[18];
    [sh_mu_18_0, sh_mu_18_1, sh_mu_18_2, sh_mu_18_3, sh_mu_18_4, sh_mu_18_5, sh_mu_18_6, sh_mu_18_7] @=> float _gmu18[];
    [sh_sig_18_0, sh_sig_18_1, sh_sig_18_2, sh_sig_18_3, sh_sig_18_4, sh_sig_18_5, sh_sig_18_6, sh_sig_18_7] @=> float _gsig18[];
    for (0 => int i; i < 8; i++) { _gmu18[i] => gShapeMu[18*8+i]; _gsig18[i] => gShapeSig[18*8+i]; }
    sh_t_19 => gShapeT[19];
    [sh_mu_19_0, sh_mu_19_1, sh_mu_19_2, sh_mu_19_3, sh_mu_19_4, sh_mu_19_5, sh_mu_19_6, sh_mu_19_7] @=> float _gmu19[];
    [sh_sig_19_0, sh_sig_19_1, sh_sig_19_2, sh_sig_19_3, sh_sig_19_4, sh_sig_19_5, sh_sig_19_6, sh_sig_19_7] @=> float _gsig19[];
    for (0 => int i; i < 8; i++) { _gmu19[i] => gShapeMu[19*8+i]; _gsig19[i] => gShapeSig[19*8+i]; }
    sh_t_20 => gShapeT[20];
    [sh_mu_20_0, sh_mu_20_1, sh_mu_20_2, sh_mu_20_3, sh_mu_20_4, sh_mu_20_5, sh_mu_20_6, sh_mu_20_7] @=> float _gmu20[];
    [sh_sig_20_0, sh_sig_20_1, sh_sig_20_2, sh_sig_20_3, sh_sig_20_4, sh_sig_20_5, sh_sig_20_6, sh_sig_20_7] @=> float _gsig20[];
    for (0 => int i; i < 8; i++) { _gmu20[i] => gShapeMu[20*8+i]; _gsig20[i] => gShapeSig[20*8+i]; }
    sh_t_21 => gShapeT[21];
    [sh_mu_21_0, sh_mu_21_1, sh_mu_21_2, sh_mu_21_3, sh_mu_21_4, sh_mu_21_5, sh_mu_21_6, sh_mu_21_7] @=> float _gmu21[];
    [sh_sig_21_0, sh_sig_21_1, sh_sig_21_2, sh_sig_21_3, sh_sig_21_4, sh_sig_21_5, sh_sig_21_6, sh_sig_21_7] @=> float _gsig21[];
    for (0 => int i; i < 8; i++) { _gmu21[i] => gShapeMu[21*8+i]; _gsig21[i] => gShapeSig[21*8+i]; }
    sh_t_22 => gShapeT[22];
    [sh_mu_22_0, sh_mu_22_1, sh_mu_22_2, sh_mu_22_3, sh_mu_22_4, sh_mu_22_5, sh_mu_22_6, sh_mu_22_7] @=> float _gmu22[];
    [sh_sig_22_0, sh_sig_22_1, sh_sig_22_2, sh_sig_22_3, sh_sig_22_4, sh_sig_22_5, sh_sig_22_6, sh_sig_22_7] @=> float _gsig22[];
    for (0 => int i; i < 8; i++) { _gmu22[i] => gShapeMu[22*8+i]; _gsig22[i] => gShapeSig[22*8+i]; }
    sh_t_23 => gShapeT[23];
    [sh_mu_23_0, sh_mu_23_1, sh_mu_23_2, sh_mu_23_3, sh_mu_23_4, sh_mu_23_5, sh_mu_23_6, sh_mu_23_7] @=> float _gmu23[];
    [sh_sig_23_0, sh_sig_23_1, sh_sig_23_2, sh_sig_23_3, sh_sig_23_4, sh_sig_23_5, sh_sig_23_6, sh_sig_23_7] @=> float _gsig23[];
    for (0 => int i; i < 8; i++) { _gmu23[i] => gShapeMu[23*8+i]; _gsig23[i] => gShapeSig[23*8+i]; }
    sh_t_24 => gShapeT[24];
    [sh_mu_24_0, sh_mu_24_1, sh_mu_24_2, sh_mu_24_3, sh_mu_24_4, sh_mu_24_5, sh_mu_24_6, sh_mu_24_7] @=> float _gmu24[];
    [sh_sig_24_0, sh_sig_24_1, sh_sig_24_2, sh_sig_24_3, sh_sig_24_4, sh_sig_24_5, sh_sig_24_6, sh_sig_24_7] @=> float _gsig24[];
    for (0 => int i; i < 8; i++) { _gmu24[i] => gShapeMu[24*8+i]; _gsig24[i] => gShapeSig[24*8+i]; }
    sh_t_25 => gShapeT[25];
    [sh_mu_25_0, sh_mu_25_1, sh_mu_25_2, sh_mu_25_3, sh_mu_25_4, sh_mu_25_5, sh_mu_25_6, sh_mu_25_7] @=> float _gmu25[];
    [sh_sig_25_0, sh_sig_25_1, sh_sig_25_2, sh_sig_25_3, sh_sig_25_4, sh_sig_25_5, sh_sig_25_6, sh_sig_25_7] @=> float _gsig25[];
    for (0 => int i; i < 8; i++) { _gmu25[i] => gShapeMu[25*8+i]; _gsig25[i] => gShapeSig[25*8+i]; }
    sh_t_26 => gShapeT[26];
    [sh_mu_26_0, sh_mu_26_1, sh_mu_26_2, sh_mu_26_3, sh_mu_26_4, sh_mu_26_5, sh_mu_26_6, sh_mu_26_7] @=> float _gmu26[];
    [sh_sig_26_0, sh_sig_26_1, sh_sig_26_2, sh_sig_26_3, sh_sig_26_4, sh_sig_26_5, sh_sig_26_6, sh_sig_26_7] @=> float _gsig26[];
    for (0 => int i; i < 8; i++) { _gmu26[i] => gShapeMu[26*8+i]; _gsig26[i] => gShapeSig[26*8+i]; }
    sh_t_27 => gShapeT[27];
    [sh_mu_27_0, sh_mu_27_1, sh_mu_27_2, sh_mu_27_3, sh_mu_27_4, sh_mu_27_5, sh_mu_27_6, sh_mu_27_7] @=> float _gmu27[];
    [sh_sig_27_0, sh_sig_27_1, sh_sig_27_2, sh_sig_27_3, sh_sig_27_4, sh_sig_27_5, sh_sig_27_6, sh_sig_27_7] @=> float _gsig27[];
    for (0 => int i; i < 8; i++) { _gmu27[i] => gShapeMu[27*8+i]; _gsig27[i] => gShapeSig[27*8+i]; }
    sh_t_28 => gShapeT[28];
    [sh_mu_28_0, sh_mu_28_1, sh_mu_28_2, sh_mu_28_3, sh_mu_28_4, sh_mu_28_5, sh_mu_28_6, sh_mu_28_7] @=> float _gmu28[];
    [sh_sig_28_0, sh_sig_28_1, sh_sig_28_2, sh_sig_28_3, sh_sig_28_4, sh_sig_28_5, sh_sig_28_6, sh_sig_28_7] @=> float _gsig28[];
    for (0 => int i; i < 8; i++) { _gmu28[i] => gShapeMu[28*8+i]; _gsig28[i] => gShapeSig[28*8+i]; }
    sh_t_29 => gShapeT[29];
    [sh_mu_29_0, sh_mu_29_1, sh_mu_29_2, sh_mu_29_3, sh_mu_29_4, sh_mu_29_5, sh_mu_29_6, sh_mu_29_7] @=> float _gmu29[];
    [sh_sig_29_0, sh_sig_29_1, sh_sig_29_2, sh_sig_29_3, sh_sig_29_4, sh_sig_29_5, sh_sig_29_6, sh_sig_29_7] @=> float _gsig29[];
    for (0 => int i; i < 8; i++) { _gmu29[i] => gShapeMu[29*8+i]; _gsig29[i] => gShapeSig[29*8+i]; }
    sh_t_30 => gShapeT[30];
    [sh_mu_30_0, sh_mu_30_1, sh_mu_30_2, sh_mu_30_3, sh_mu_30_4, sh_mu_30_5, sh_mu_30_6, sh_mu_30_7] @=> float _gmu30[];
    [sh_sig_30_0, sh_sig_30_1, sh_sig_30_2, sh_sig_30_3, sh_sig_30_4, sh_sig_30_5, sh_sig_30_6, sh_sig_30_7] @=> float _gsig30[];
    for (0 => int i; i < 8; i++) { _gmu30[i] => gShapeMu[30*8+i]; _gsig30[i] => gShapeSig[30*8+i]; }
    sh_t_31 => gShapeT[31];
    [sh_mu_31_0, sh_mu_31_1, sh_mu_31_2, sh_mu_31_3, sh_mu_31_4, sh_mu_31_5, sh_mu_31_6, sh_mu_31_7] @=> float _gmu31[];
    [sh_sig_31_0, sh_sig_31_1, sh_sig_31_2, sh_sig_31_3, sh_sig_31_4, sh_sig_31_5, sh_sig_31_6, sh_sig_31_7] @=> float _gsig31[];
    for (0 => int i; i < 8; i++) { _gmu31[i] => gShapeMu[31*8+i]; _gsig31[i] => gShapeSig[31*8+i]; }
}


// ---- onset / release / shape-walk ----
fun void applyRealized(int n, float hitMu[], float hitSig[], float delta[], float r[], float tt) {
    float baseMu[8];
    float baseSig[8];
    shapeAt(tt, baseMu, baseSig);
    float outMu[8];
    for (0 => int i; i < 8; i++) {
        baseMu[i] + delta[i] => float tm;
        if (tm < 0.0) 0.0 => tm;
        if (tm > 1.0) 1.0 => tm;
        Math.sqrt(hitSig[i]*hitSig[i] + baseSig[i]*baseSig[i]) => float sigEff;
        tm + r[i] * sigEff => float rm;
        if (rm < 0.0) 0.0 => rm;
        if (rm > 1.0) 1.0 => rm;
        rm => outMu[i];
    }
    applyCoord(n, outMu);
}

fun void walkShape(int n, float hitMu[], float hitSig[], float delta[], float r[]) {
    now => time t0;
    while (true) {
        ((now - t0) / second) / sh_dur => float tt;
        if (tt > 1.0) 1.0 => tt;
        applyRealized(n, hitMu, hitSig, delta, r, tt);
        if (tt >= 1.0) break;
        5::ms => now;
    }
    0 => eWalkId[n];
}

fun void onset(int n, float hitMu[], float hitSig[], float vel) {
    vel => eVel[n];
    if (eWalkId[n] > 0) { Machine.remove(eWalkId[n]); 0 => eWalkId[n]; }

    gatherShape();

    float r[8];
    for (0 => int i; i < 8; i++) { Math.random2f(-1.0, 1.0) => r[i]; }

    float delta[8];
    if (gShapeN <= 0) {
        for (0 => int i; i < 8; i++) { 0.0 => delta[i]; }
    } else {
        for (0 => int i; i < 8; i++) { hitMu[i] - gShapeMu[i] => delta[i]; }
    }

    applyRealized(n, hitMu, hitSig, delta, r, 0.0);
    eEnv[n].keyOn();

    if (gShapeN > 1 && sh_dur > 0.0) {
        Shred sh;
        spork ~ walkShape(n, hitMu, hitSig, delta, r) @=> sh;
        sh.id() => eWalkId[n];
    }
}

fun void release(int n) {
    eEnv[n].keyOff();
}

// ---- amplitude-modulation ("rate" axis) control-rate shred, all voices ----
fun void amLoop() {
    while (true) {
        for (0 => int n; n < N_VOICES; n++) {
            if (eAmDepth[n] > 0.0) {
                (now / second) $ float => float t;
                (1.0 - eAmDepth[n] + eAmDepth[n] * (0.5 + 0.5 * Math.sin(TWO_PI * eRateHz[n] * t))) => eAmMod[n].gain;
            } else {
                1.0 => eAmMod[n].gain;
            }
        }
        2::ms => now;
    }
}
spork ~ amLoop();

// ---- per-voice watchers: literal names required (no dynamic global lookup in ChucK) ----
fun void watch_0() {
    while (true) {
        v_hit_0 => now;
        if (v_gate_0 == 1) {
            [v_mu_0_0, v_mu_0_1, v_mu_0_2, v_mu_0_3, v_mu_0_4, v_mu_0_5, v_mu_0_6, v_mu_0_7] @=> float hitMu[];
            [v_sig_0_0, v_sig_0_1, v_sig_0_2, v_sig_0_3, v_sig_0_4, v_sig_0_5, v_sig_0_6, v_sig_0_7] @=> float hitSig[];
            onset(0, hitMu, hitSig, v_vel_0);
        } else {
            release(0);
        }
    }
}
spork ~ watch_0();
fun void watch_1() {
    while (true) {
        v_hit_1 => now;
        if (v_gate_1 == 1) {
            [v_mu_1_0, v_mu_1_1, v_mu_1_2, v_mu_1_3, v_mu_1_4, v_mu_1_5, v_mu_1_6, v_mu_1_7] @=> float hitMu[];
            [v_sig_1_0, v_sig_1_1, v_sig_1_2, v_sig_1_3, v_sig_1_4, v_sig_1_5, v_sig_1_6, v_sig_1_7] @=> float hitSig[];
            onset(1, hitMu, hitSig, v_vel_1);
        } else {
            release(1);
        }
    }
}
spork ~ watch_1();
fun void watch_2() {
    while (true) {
        v_hit_2 => now;
        if (v_gate_2 == 1) {
            [v_mu_2_0, v_mu_2_1, v_mu_2_2, v_mu_2_3, v_mu_2_4, v_mu_2_5, v_mu_2_6, v_mu_2_7] @=> float hitMu[];
            [v_sig_2_0, v_sig_2_1, v_sig_2_2, v_sig_2_3, v_sig_2_4, v_sig_2_5, v_sig_2_6, v_sig_2_7] @=> float hitSig[];
            onset(2, hitMu, hitSig, v_vel_2);
        } else {
            release(2);
        }
    }
}
spork ~ watch_2();
fun void watch_3() {
    while (true) {
        v_hit_3 => now;
        if (v_gate_3 == 1) {
            [v_mu_3_0, v_mu_3_1, v_mu_3_2, v_mu_3_3, v_mu_3_4, v_mu_3_5, v_mu_3_6, v_mu_3_7] @=> float hitMu[];
            [v_sig_3_0, v_sig_3_1, v_sig_3_2, v_sig_3_3, v_sig_3_4, v_sig_3_5, v_sig_3_6, v_sig_3_7] @=> float hitSig[];
            onset(3, hitMu, hitSig, v_vel_3);
        } else {
            release(3);
        }
    }
}
spork ~ watch_3();
fun void watch_4() {
    while (true) {
        v_hit_4 => now;
        if (v_gate_4 == 1) {
            [v_mu_4_0, v_mu_4_1, v_mu_4_2, v_mu_4_3, v_mu_4_4, v_mu_4_5, v_mu_4_6, v_mu_4_7] @=> float hitMu[];
            [v_sig_4_0, v_sig_4_1, v_sig_4_2, v_sig_4_3, v_sig_4_4, v_sig_4_5, v_sig_4_6, v_sig_4_7] @=> float hitSig[];
            onset(4, hitMu, hitSig, v_vel_4);
        } else {
            release(4);
        }
    }
}
spork ~ watch_4();
fun void watch_5() {
    while (true) {
        v_hit_5 => now;
        if (v_gate_5 == 1) {
            [v_mu_5_0, v_mu_5_1, v_mu_5_2, v_mu_5_3, v_mu_5_4, v_mu_5_5, v_mu_5_6, v_mu_5_7] @=> float hitMu[];
            [v_sig_5_0, v_sig_5_1, v_sig_5_2, v_sig_5_3, v_sig_5_4, v_sig_5_5, v_sig_5_6, v_sig_5_7] @=> float hitSig[];
            onset(5, hitMu, hitSig, v_vel_5);
        } else {
            release(5);
        }
    }
}
spork ~ watch_5();
fun void watch_6() {
    while (true) {
        v_hit_6 => now;
        if (v_gate_6 == 1) {
            [v_mu_6_0, v_mu_6_1, v_mu_6_2, v_mu_6_3, v_mu_6_4, v_mu_6_5, v_mu_6_6, v_mu_6_7] @=> float hitMu[];
            [v_sig_6_0, v_sig_6_1, v_sig_6_2, v_sig_6_3, v_sig_6_4, v_sig_6_5, v_sig_6_6, v_sig_6_7] @=> float hitSig[];
            onset(6, hitMu, hitSig, v_vel_6);
        } else {
            release(6);
        }
    }
}
spork ~ watch_6();
fun void watch_7() {
    while (true) {
        v_hit_7 => now;
        if (v_gate_7 == 1) {
            [v_mu_7_0, v_mu_7_1, v_mu_7_2, v_mu_7_3, v_mu_7_4, v_mu_7_5, v_mu_7_6, v_mu_7_7] @=> float hitMu[];
            [v_sig_7_0, v_sig_7_1, v_sig_7_2, v_sig_7_3, v_sig_7_4, v_sig_7_5, v_sig_7_6, v_sig_7_7] @=> float hitSig[];
            onset(7, hitMu, hitSig, v_vel_7);
        } else {
            release(7);
        }
    }
}
spork ~ watch_7();
fun void watch_8() {
    while (true) {
        v_hit_8 => now;
        if (v_gate_8 == 1) {
            [v_mu_8_0, v_mu_8_1, v_mu_8_2, v_mu_8_3, v_mu_8_4, v_mu_8_5, v_mu_8_6, v_mu_8_7] @=> float hitMu[];
            [v_sig_8_0, v_sig_8_1, v_sig_8_2, v_sig_8_3, v_sig_8_4, v_sig_8_5, v_sig_8_6, v_sig_8_7] @=> float hitSig[];
            onset(8, hitMu, hitSig, v_vel_8);
        } else {
            release(8);
        }
    }
}
spork ~ watch_8();
fun void watch_9() {
    while (true) {
        v_hit_9 => now;
        if (v_gate_9 == 1) {
            [v_mu_9_0, v_mu_9_1, v_mu_9_2, v_mu_9_3, v_mu_9_4, v_mu_9_5, v_mu_9_6, v_mu_9_7] @=> float hitMu[];
            [v_sig_9_0, v_sig_9_1, v_sig_9_2, v_sig_9_3, v_sig_9_4, v_sig_9_5, v_sig_9_6, v_sig_9_7] @=> float hitSig[];
            onset(9, hitMu, hitSig, v_vel_9);
        } else {
            release(9);
        }
    }
}
spork ~ watch_9();
fun void watch_10() {
    while (true) {
        v_hit_10 => now;
        if (v_gate_10 == 1) {
            [v_mu_10_0, v_mu_10_1, v_mu_10_2, v_mu_10_3, v_mu_10_4, v_mu_10_5, v_mu_10_6, v_mu_10_7] @=> float hitMu[];
            [v_sig_10_0, v_sig_10_1, v_sig_10_2, v_sig_10_3, v_sig_10_4, v_sig_10_5, v_sig_10_6, v_sig_10_7] @=> float hitSig[];
            onset(10, hitMu, hitSig, v_vel_10);
        } else {
            release(10);
        }
    }
}
spork ~ watch_10();
fun void watch_11() {
    while (true) {
        v_hit_11 => now;
        if (v_gate_11 == 1) {
            [v_mu_11_0, v_mu_11_1, v_mu_11_2, v_mu_11_3, v_mu_11_4, v_mu_11_5, v_mu_11_6, v_mu_11_7] @=> float hitMu[];
            [v_sig_11_0, v_sig_11_1, v_sig_11_2, v_sig_11_3, v_sig_11_4, v_sig_11_5, v_sig_11_6, v_sig_11_7] @=> float hitSig[];
            onset(11, hitMu, hitSig, v_vel_11);
        } else {
            release(11);
        }
    }
}
spork ~ watch_11();
fun void watch_12() {
    while (true) {
        v_hit_12 => now;
        if (v_gate_12 == 1) {
            [v_mu_12_0, v_mu_12_1, v_mu_12_2, v_mu_12_3, v_mu_12_4, v_mu_12_5, v_mu_12_6, v_mu_12_7] @=> float hitMu[];
            [v_sig_12_0, v_sig_12_1, v_sig_12_2, v_sig_12_3, v_sig_12_4, v_sig_12_5, v_sig_12_6, v_sig_12_7] @=> float hitSig[];
            onset(12, hitMu, hitSig, v_vel_12);
        } else {
            release(12);
        }
    }
}
spork ~ watch_12();
fun void watch_13() {
    while (true) {
        v_hit_13 => now;
        if (v_gate_13 == 1) {
            [v_mu_13_0, v_mu_13_1, v_mu_13_2, v_mu_13_3, v_mu_13_4, v_mu_13_5, v_mu_13_6, v_mu_13_7] @=> float hitMu[];
            [v_sig_13_0, v_sig_13_1, v_sig_13_2, v_sig_13_3, v_sig_13_4, v_sig_13_5, v_sig_13_6, v_sig_13_7] @=> float hitSig[];
            onset(13, hitMu, hitSig, v_vel_13);
        } else {
            release(13);
        }
    }
}
spork ~ watch_13();
fun void watch_14() {
    while (true) {
        v_hit_14 => now;
        if (v_gate_14 == 1) {
            [v_mu_14_0, v_mu_14_1, v_mu_14_2, v_mu_14_3, v_mu_14_4, v_mu_14_5, v_mu_14_6, v_mu_14_7] @=> float hitMu[];
            [v_sig_14_0, v_sig_14_1, v_sig_14_2, v_sig_14_3, v_sig_14_4, v_sig_14_5, v_sig_14_6, v_sig_14_7] @=> float hitSig[];
            onset(14, hitMu, hitSig, v_vel_14);
        } else {
            release(14);
        }
    }
}
spork ~ watch_14();
fun void watch_15() {
    while (true) {
        v_hit_15 => now;
        if (v_gate_15 == 1) {
            [v_mu_15_0, v_mu_15_1, v_mu_15_2, v_mu_15_3, v_mu_15_4, v_mu_15_5, v_mu_15_6, v_mu_15_7] @=> float hitMu[];
            [v_sig_15_0, v_sig_15_1, v_sig_15_2, v_sig_15_3, v_sig_15_4, v_sig_15_5, v_sig_15_6, v_sig_15_7] @=> float hitSig[];
            onset(15, hitMu, hitSig, v_vel_15);
        } else {
            release(15);
        }
    }
}
spork ~ watch_15();
