// sept_render_osc.ck
// Septagraph v1 reference renderer -- native OSC shim (Task 2.1).
//
// Native-only: OscIn on port 7700 (spec WIRE_FORMAT_v1.md SS B). Parses
// /sept/v1/hit, /sept/v1/move, /sept/v1/shape, /sept/v1/manifest/request
// and writes ONLY into the named globals declared in sept_render_core.ck
// -- this file never touches DSP UGens directly, so sept_render_core.ck
// stays byte-identical between native ChucK and WebChucK (Task 2.3, which
// replaces this file with JS calling chuck.setFloat/setInt/broadcastEvent
// on the exact same global names).
//
// Run together as:
//   chuck --loop sept_render_core.ck sept_render_osc.ck sept_manifest.ck
//
// Voice slots: 16, LRU steal (a hit for a new voice_id when all 16 slots
// are held steals the least-recently-triggered slot).
//
// KNOWN DEVIATION from the plan text: ChucK 1.5.5.5's OscMsg/OscIn API
// does not expose the sender's source IP, so "manifest reply to the
// sender's IP" (as implemented in sept_manifest.ck) instead replies to
// 127.0.0.1:7701 and broadcasts to 255.255.255.255:7701. This is
// documented again in sept_manifest.ck and in chuck/README.md.

// ChucK requires every file that references a `global` to redeclare it
// (globals share storage across files, but each file's compiler needs its
// own type-checked declaration -- verified experimentally: a bare
// identifier from another file's `global` is a compile error here).
// ---- Named per-voice globals (exact names; must match sept_render_core.ck) ----
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

// ---- --trace flag (plan Task 3.4: latency probe) ----
// Off by default so normal runs stay quiet. Enable per-file via ChucK's
// colon-arg syntax on this specific shred:
//   chuck --loop sept_render_core.ck sept_render_osc.ck:--trace sept_manifest.ck
// When on, every /sept/v1/hit onset=1 prints one line the latency probe
// timestamps on arrival (stdout is buffered/flushed by the OS pipe, so the
// probe documents +-1ms jitter on this print vs. the true DSP-gate instant).
0 => int TRACE_ENABLED;
for (0 => int ai; ai < me.args(); ai++) {
    if (me.arg(ai) == "--trace") { 1 => TRACE_ENABLED; }
}

7700 => int PORT;
OscIn oin;
PORT => oin.port;
oin.addAddress("/sept/v1/hit");
oin.addAddress("/sept/v1/move");
oin.addAddress("/sept/v1/shape");
oin.addAddress("/sept/v1/manifest/request");
OscMsg msg;

global Event manifest_requested;

// ---- LRU voice slot map ----
string slotVoice[16];
int slotLastUse[16];
0 => int useCounter;
for (0 => int i; i < 16; i++) { "" => slotVoice[i]; 0 => slotLastUse[i]; }

// last-known sigma/velocity per slot, so /move (mu-only) can re-apply a
// hit without forgetting the voice's sigma/velocity.
float slotSig[16 * 8];
float slotVel[16];

fun int findSlotForRelease(string vid) {
    for (0 => int i; i < 16; i++) { if (slotVoice[i] == vid) return i; }
    return -1;
}

fun int findSlotForOnset(string vid) {
    for (0 => int i; i < 16; i++) { if (slotVoice[i] == vid) return i; }
    for (0 => int i; i < 16; i++) { if (slotVoice[i] == "") return i; }
    0 => int best;
    slotLastUse[0] => int bestUse;
    for (1 => int i; i < 16; i++) {
        if (slotLastUse[i] < bestUse) { i => best; slotLastUse[i] => bestUse; }
    }
    return best;
}

fun void setVoice(int n, float mu[], float sig[], float vel) {
    if (n == 0) {
        mu[0] => v_mu_0_0;
        mu[1] => v_mu_0_1;
        mu[2] => v_mu_0_2;
        mu[3] => v_mu_0_3;
        mu[4] => v_mu_0_4;
        mu[5] => v_mu_0_5;
        mu[6] => v_mu_0_6;
        mu[7] => v_mu_0_7;
        sig[0] => v_sig_0_0;
        sig[1] => v_sig_0_1;
        sig[2] => v_sig_0_2;
        sig[3] => v_sig_0_3;
        sig[4] => v_sig_0_4;
        sig[5] => v_sig_0_5;
        sig[6] => v_sig_0_6;
        sig[7] => v_sig_0_7;
        vel => v_vel_0;
        1 => v_gate_0;
        v_hit_0.broadcast();
    }
    else if (n == 1) {
        mu[0] => v_mu_1_0;
        mu[1] => v_mu_1_1;
        mu[2] => v_mu_1_2;
        mu[3] => v_mu_1_3;
        mu[4] => v_mu_1_4;
        mu[5] => v_mu_1_5;
        mu[6] => v_mu_1_6;
        mu[7] => v_mu_1_7;
        sig[0] => v_sig_1_0;
        sig[1] => v_sig_1_1;
        sig[2] => v_sig_1_2;
        sig[3] => v_sig_1_3;
        sig[4] => v_sig_1_4;
        sig[5] => v_sig_1_5;
        sig[6] => v_sig_1_6;
        sig[7] => v_sig_1_7;
        vel => v_vel_1;
        1 => v_gate_1;
        v_hit_1.broadcast();
    }
    else if (n == 2) {
        mu[0] => v_mu_2_0;
        mu[1] => v_mu_2_1;
        mu[2] => v_mu_2_2;
        mu[3] => v_mu_2_3;
        mu[4] => v_mu_2_4;
        mu[5] => v_mu_2_5;
        mu[6] => v_mu_2_6;
        mu[7] => v_mu_2_7;
        sig[0] => v_sig_2_0;
        sig[1] => v_sig_2_1;
        sig[2] => v_sig_2_2;
        sig[3] => v_sig_2_3;
        sig[4] => v_sig_2_4;
        sig[5] => v_sig_2_5;
        sig[6] => v_sig_2_6;
        sig[7] => v_sig_2_7;
        vel => v_vel_2;
        1 => v_gate_2;
        v_hit_2.broadcast();
    }
    else if (n == 3) {
        mu[0] => v_mu_3_0;
        mu[1] => v_mu_3_1;
        mu[2] => v_mu_3_2;
        mu[3] => v_mu_3_3;
        mu[4] => v_mu_3_4;
        mu[5] => v_mu_3_5;
        mu[6] => v_mu_3_6;
        mu[7] => v_mu_3_7;
        sig[0] => v_sig_3_0;
        sig[1] => v_sig_3_1;
        sig[2] => v_sig_3_2;
        sig[3] => v_sig_3_3;
        sig[4] => v_sig_3_4;
        sig[5] => v_sig_3_5;
        sig[6] => v_sig_3_6;
        sig[7] => v_sig_3_7;
        vel => v_vel_3;
        1 => v_gate_3;
        v_hit_3.broadcast();
    }
    else if (n == 4) {
        mu[0] => v_mu_4_0;
        mu[1] => v_mu_4_1;
        mu[2] => v_mu_4_2;
        mu[3] => v_mu_4_3;
        mu[4] => v_mu_4_4;
        mu[5] => v_mu_4_5;
        mu[6] => v_mu_4_6;
        mu[7] => v_mu_4_7;
        sig[0] => v_sig_4_0;
        sig[1] => v_sig_4_1;
        sig[2] => v_sig_4_2;
        sig[3] => v_sig_4_3;
        sig[4] => v_sig_4_4;
        sig[5] => v_sig_4_5;
        sig[6] => v_sig_4_6;
        sig[7] => v_sig_4_7;
        vel => v_vel_4;
        1 => v_gate_4;
        v_hit_4.broadcast();
    }
    else if (n == 5) {
        mu[0] => v_mu_5_0;
        mu[1] => v_mu_5_1;
        mu[2] => v_mu_5_2;
        mu[3] => v_mu_5_3;
        mu[4] => v_mu_5_4;
        mu[5] => v_mu_5_5;
        mu[6] => v_mu_5_6;
        mu[7] => v_mu_5_7;
        sig[0] => v_sig_5_0;
        sig[1] => v_sig_5_1;
        sig[2] => v_sig_5_2;
        sig[3] => v_sig_5_3;
        sig[4] => v_sig_5_4;
        sig[5] => v_sig_5_5;
        sig[6] => v_sig_5_6;
        sig[7] => v_sig_5_7;
        vel => v_vel_5;
        1 => v_gate_5;
        v_hit_5.broadcast();
    }
    else if (n == 6) {
        mu[0] => v_mu_6_0;
        mu[1] => v_mu_6_1;
        mu[2] => v_mu_6_2;
        mu[3] => v_mu_6_3;
        mu[4] => v_mu_6_4;
        mu[5] => v_mu_6_5;
        mu[6] => v_mu_6_6;
        mu[7] => v_mu_6_7;
        sig[0] => v_sig_6_0;
        sig[1] => v_sig_6_1;
        sig[2] => v_sig_6_2;
        sig[3] => v_sig_6_3;
        sig[4] => v_sig_6_4;
        sig[5] => v_sig_6_5;
        sig[6] => v_sig_6_6;
        sig[7] => v_sig_6_7;
        vel => v_vel_6;
        1 => v_gate_6;
        v_hit_6.broadcast();
    }
    else if (n == 7) {
        mu[0] => v_mu_7_0;
        mu[1] => v_mu_7_1;
        mu[2] => v_mu_7_2;
        mu[3] => v_mu_7_3;
        mu[4] => v_mu_7_4;
        mu[5] => v_mu_7_5;
        mu[6] => v_mu_7_6;
        mu[7] => v_mu_7_7;
        sig[0] => v_sig_7_0;
        sig[1] => v_sig_7_1;
        sig[2] => v_sig_7_2;
        sig[3] => v_sig_7_3;
        sig[4] => v_sig_7_4;
        sig[5] => v_sig_7_5;
        sig[6] => v_sig_7_6;
        sig[7] => v_sig_7_7;
        vel => v_vel_7;
        1 => v_gate_7;
        v_hit_7.broadcast();
    }
    else if (n == 8) {
        mu[0] => v_mu_8_0;
        mu[1] => v_mu_8_1;
        mu[2] => v_mu_8_2;
        mu[3] => v_mu_8_3;
        mu[4] => v_mu_8_4;
        mu[5] => v_mu_8_5;
        mu[6] => v_mu_8_6;
        mu[7] => v_mu_8_7;
        sig[0] => v_sig_8_0;
        sig[1] => v_sig_8_1;
        sig[2] => v_sig_8_2;
        sig[3] => v_sig_8_3;
        sig[4] => v_sig_8_4;
        sig[5] => v_sig_8_5;
        sig[6] => v_sig_8_6;
        sig[7] => v_sig_8_7;
        vel => v_vel_8;
        1 => v_gate_8;
        v_hit_8.broadcast();
    }
    else if (n == 9) {
        mu[0] => v_mu_9_0;
        mu[1] => v_mu_9_1;
        mu[2] => v_mu_9_2;
        mu[3] => v_mu_9_3;
        mu[4] => v_mu_9_4;
        mu[5] => v_mu_9_5;
        mu[6] => v_mu_9_6;
        mu[7] => v_mu_9_7;
        sig[0] => v_sig_9_0;
        sig[1] => v_sig_9_1;
        sig[2] => v_sig_9_2;
        sig[3] => v_sig_9_3;
        sig[4] => v_sig_9_4;
        sig[5] => v_sig_9_5;
        sig[6] => v_sig_9_6;
        sig[7] => v_sig_9_7;
        vel => v_vel_9;
        1 => v_gate_9;
        v_hit_9.broadcast();
    }
    else if (n == 10) {
        mu[0] => v_mu_10_0;
        mu[1] => v_mu_10_1;
        mu[2] => v_mu_10_2;
        mu[3] => v_mu_10_3;
        mu[4] => v_mu_10_4;
        mu[5] => v_mu_10_5;
        mu[6] => v_mu_10_6;
        mu[7] => v_mu_10_7;
        sig[0] => v_sig_10_0;
        sig[1] => v_sig_10_1;
        sig[2] => v_sig_10_2;
        sig[3] => v_sig_10_3;
        sig[4] => v_sig_10_4;
        sig[5] => v_sig_10_5;
        sig[6] => v_sig_10_6;
        sig[7] => v_sig_10_7;
        vel => v_vel_10;
        1 => v_gate_10;
        v_hit_10.broadcast();
    }
    else if (n == 11) {
        mu[0] => v_mu_11_0;
        mu[1] => v_mu_11_1;
        mu[2] => v_mu_11_2;
        mu[3] => v_mu_11_3;
        mu[4] => v_mu_11_4;
        mu[5] => v_mu_11_5;
        mu[6] => v_mu_11_6;
        mu[7] => v_mu_11_7;
        sig[0] => v_sig_11_0;
        sig[1] => v_sig_11_1;
        sig[2] => v_sig_11_2;
        sig[3] => v_sig_11_3;
        sig[4] => v_sig_11_4;
        sig[5] => v_sig_11_5;
        sig[6] => v_sig_11_6;
        sig[7] => v_sig_11_7;
        vel => v_vel_11;
        1 => v_gate_11;
        v_hit_11.broadcast();
    }
    else if (n == 12) {
        mu[0] => v_mu_12_0;
        mu[1] => v_mu_12_1;
        mu[2] => v_mu_12_2;
        mu[3] => v_mu_12_3;
        mu[4] => v_mu_12_4;
        mu[5] => v_mu_12_5;
        mu[6] => v_mu_12_6;
        mu[7] => v_mu_12_7;
        sig[0] => v_sig_12_0;
        sig[1] => v_sig_12_1;
        sig[2] => v_sig_12_2;
        sig[3] => v_sig_12_3;
        sig[4] => v_sig_12_4;
        sig[5] => v_sig_12_5;
        sig[6] => v_sig_12_6;
        sig[7] => v_sig_12_7;
        vel => v_vel_12;
        1 => v_gate_12;
        v_hit_12.broadcast();
    }
    else if (n == 13) {
        mu[0] => v_mu_13_0;
        mu[1] => v_mu_13_1;
        mu[2] => v_mu_13_2;
        mu[3] => v_mu_13_3;
        mu[4] => v_mu_13_4;
        mu[5] => v_mu_13_5;
        mu[6] => v_mu_13_6;
        mu[7] => v_mu_13_7;
        sig[0] => v_sig_13_0;
        sig[1] => v_sig_13_1;
        sig[2] => v_sig_13_2;
        sig[3] => v_sig_13_3;
        sig[4] => v_sig_13_4;
        sig[5] => v_sig_13_5;
        sig[6] => v_sig_13_6;
        sig[7] => v_sig_13_7;
        vel => v_vel_13;
        1 => v_gate_13;
        v_hit_13.broadcast();
    }
    else if (n == 14) {
        mu[0] => v_mu_14_0;
        mu[1] => v_mu_14_1;
        mu[2] => v_mu_14_2;
        mu[3] => v_mu_14_3;
        mu[4] => v_mu_14_4;
        mu[5] => v_mu_14_5;
        mu[6] => v_mu_14_6;
        mu[7] => v_mu_14_7;
        sig[0] => v_sig_14_0;
        sig[1] => v_sig_14_1;
        sig[2] => v_sig_14_2;
        sig[3] => v_sig_14_3;
        sig[4] => v_sig_14_4;
        sig[5] => v_sig_14_5;
        sig[6] => v_sig_14_6;
        sig[7] => v_sig_14_7;
        vel => v_vel_14;
        1 => v_gate_14;
        v_hit_14.broadcast();
    }
    else if (n == 15) {
        mu[0] => v_mu_15_0;
        mu[1] => v_mu_15_1;
        mu[2] => v_mu_15_2;
        mu[3] => v_mu_15_3;
        mu[4] => v_mu_15_4;
        mu[5] => v_mu_15_5;
        mu[6] => v_mu_15_6;
        mu[7] => v_mu_15_7;
        sig[0] => v_sig_15_0;
        sig[1] => v_sig_15_1;
        sig[2] => v_sig_15_2;
        sig[3] => v_sig_15_3;
        sig[4] => v_sig_15_4;
        sig[5] => v_sig_15_5;
        sig[6] => v_sig_15_6;
        sig[7] => v_sig_15_7;
        vel => v_vel_15;
        1 => v_gate_15;
        v_hit_15.broadcast();
    }
}

// /move: mu only, no envelope re-trigger of the ADSR (v_gate stays 1;
// re-broadcasting v_hit_n just nudges the live DSP params -- see
// sept_render_core.ck onset(), which re-applies mu/sigma every hit event
// regardless of ADSR state).
fun void setVoiceMuOnly(int n, float mu[]) {
    if (n == 0) {
        mu[0] => v_mu_0_0;
        mu[1] => v_mu_0_1;
        mu[2] => v_mu_0_2;
        mu[3] => v_mu_0_3;
        mu[4] => v_mu_0_4;
        mu[5] => v_mu_0_5;
        mu[6] => v_mu_0_6;
        mu[7] => v_mu_0_7;
        1 => v_gate_0;
        v_hit_0.broadcast();
    }
    else if (n == 1) {
        mu[0] => v_mu_1_0;
        mu[1] => v_mu_1_1;
        mu[2] => v_mu_1_2;
        mu[3] => v_mu_1_3;
        mu[4] => v_mu_1_4;
        mu[5] => v_mu_1_5;
        mu[6] => v_mu_1_6;
        mu[7] => v_mu_1_7;
        1 => v_gate_1;
        v_hit_1.broadcast();
    }
    else if (n == 2) {
        mu[0] => v_mu_2_0;
        mu[1] => v_mu_2_1;
        mu[2] => v_mu_2_2;
        mu[3] => v_mu_2_3;
        mu[4] => v_mu_2_4;
        mu[5] => v_mu_2_5;
        mu[6] => v_mu_2_6;
        mu[7] => v_mu_2_7;
        1 => v_gate_2;
        v_hit_2.broadcast();
    }
    else if (n == 3) {
        mu[0] => v_mu_3_0;
        mu[1] => v_mu_3_1;
        mu[2] => v_mu_3_2;
        mu[3] => v_mu_3_3;
        mu[4] => v_mu_3_4;
        mu[5] => v_mu_3_5;
        mu[6] => v_mu_3_6;
        mu[7] => v_mu_3_7;
        1 => v_gate_3;
        v_hit_3.broadcast();
    }
    else if (n == 4) {
        mu[0] => v_mu_4_0;
        mu[1] => v_mu_4_1;
        mu[2] => v_mu_4_2;
        mu[3] => v_mu_4_3;
        mu[4] => v_mu_4_4;
        mu[5] => v_mu_4_5;
        mu[6] => v_mu_4_6;
        mu[7] => v_mu_4_7;
        1 => v_gate_4;
        v_hit_4.broadcast();
    }
    else if (n == 5) {
        mu[0] => v_mu_5_0;
        mu[1] => v_mu_5_1;
        mu[2] => v_mu_5_2;
        mu[3] => v_mu_5_3;
        mu[4] => v_mu_5_4;
        mu[5] => v_mu_5_5;
        mu[6] => v_mu_5_6;
        mu[7] => v_mu_5_7;
        1 => v_gate_5;
        v_hit_5.broadcast();
    }
    else if (n == 6) {
        mu[0] => v_mu_6_0;
        mu[1] => v_mu_6_1;
        mu[2] => v_mu_6_2;
        mu[3] => v_mu_6_3;
        mu[4] => v_mu_6_4;
        mu[5] => v_mu_6_5;
        mu[6] => v_mu_6_6;
        mu[7] => v_mu_6_7;
        1 => v_gate_6;
        v_hit_6.broadcast();
    }
    else if (n == 7) {
        mu[0] => v_mu_7_0;
        mu[1] => v_mu_7_1;
        mu[2] => v_mu_7_2;
        mu[3] => v_mu_7_3;
        mu[4] => v_mu_7_4;
        mu[5] => v_mu_7_5;
        mu[6] => v_mu_7_6;
        mu[7] => v_mu_7_7;
        1 => v_gate_7;
        v_hit_7.broadcast();
    }
    else if (n == 8) {
        mu[0] => v_mu_8_0;
        mu[1] => v_mu_8_1;
        mu[2] => v_mu_8_2;
        mu[3] => v_mu_8_3;
        mu[4] => v_mu_8_4;
        mu[5] => v_mu_8_5;
        mu[6] => v_mu_8_6;
        mu[7] => v_mu_8_7;
        1 => v_gate_8;
        v_hit_8.broadcast();
    }
    else if (n == 9) {
        mu[0] => v_mu_9_0;
        mu[1] => v_mu_9_1;
        mu[2] => v_mu_9_2;
        mu[3] => v_mu_9_3;
        mu[4] => v_mu_9_4;
        mu[5] => v_mu_9_5;
        mu[6] => v_mu_9_6;
        mu[7] => v_mu_9_7;
        1 => v_gate_9;
        v_hit_9.broadcast();
    }
    else if (n == 10) {
        mu[0] => v_mu_10_0;
        mu[1] => v_mu_10_1;
        mu[2] => v_mu_10_2;
        mu[3] => v_mu_10_3;
        mu[4] => v_mu_10_4;
        mu[5] => v_mu_10_5;
        mu[6] => v_mu_10_6;
        mu[7] => v_mu_10_7;
        1 => v_gate_10;
        v_hit_10.broadcast();
    }
    else if (n == 11) {
        mu[0] => v_mu_11_0;
        mu[1] => v_mu_11_1;
        mu[2] => v_mu_11_2;
        mu[3] => v_mu_11_3;
        mu[4] => v_mu_11_4;
        mu[5] => v_mu_11_5;
        mu[6] => v_mu_11_6;
        mu[7] => v_mu_11_7;
        1 => v_gate_11;
        v_hit_11.broadcast();
    }
    else if (n == 12) {
        mu[0] => v_mu_12_0;
        mu[1] => v_mu_12_1;
        mu[2] => v_mu_12_2;
        mu[3] => v_mu_12_3;
        mu[4] => v_mu_12_4;
        mu[5] => v_mu_12_5;
        mu[6] => v_mu_12_6;
        mu[7] => v_mu_12_7;
        1 => v_gate_12;
        v_hit_12.broadcast();
    }
    else if (n == 13) {
        mu[0] => v_mu_13_0;
        mu[1] => v_mu_13_1;
        mu[2] => v_mu_13_2;
        mu[3] => v_mu_13_3;
        mu[4] => v_mu_13_4;
        mu[5] => v_mu_13_5;
        mu[6] => v_mu_13_6;
        mu[7] => v_mu_13_7;
        1 => v_gate_13;
        v_hit_13.broadcast();
    }
    else if (n == 14) {
        mu[0] => v_mu_14_0;
        mu[1] => v_mu_14_1;
        mu[2] => v_mu_14_2;
        mu[3] => v_mu_14_3;
        mu[4] => v_mu_14_4;
        mu[5] => v_mu_14_5;
        mu[6] => v_mu_14_6;
        mu[7] => v_mu_14_7;
        1 => v_gate_14;
        v_hit_14.broadcast();
    }
    else if (n == 15) {
        mu[0] => v_mu_15_0;
        mu[1] => v_mu_15_1;
        mu[2] => v_mu_15_2;
        mu[3] => v_mu_15_3;
        mu[4] => v_mu_15_4;
        mu[5] => v_mu_15_5;
        mu[6] => v_mu_15_6;
        mu[7] => v_mu_15_7;
        1 => v_gate_15;
        v_hit_15.broadcast();
    }
}

fun void releaseVoice(int n) {
    if (n == 0) {
        0 => v_gate_0;
        v_hit_0.broadcast();
    }
    else if (n == 1) {
        0 => v_gate_1;
        v_hit_1.broadcast();
    }
    else if (n == 2) {
        0 => v_gate_2;
        v_hit_2.broadcast();
    }
    else if (n == 3) {
        0 => v_gate_3;
        v_hit_3.broadcast();
    }
    else if (n == 4) {
        0 => v_gate_4;
        v_hit_4.broadcast();
    }
    else if (n == 5) {
        0 => v_gate_5;
        v_hit_5.broadcast();
    }
    else if (n == 6) {
        0 => v_gate_6;
        v_hit_6.broadcast();
    }
    else if (n == 7) {
        0 => v_gate_7;
        v_hit_7.broadcast();
    }
    else if (n == 8) {
        0 => v_gate_8;
        v_hit_8.broadcast();
    }
    else if (n == 9) {
        0 => v_gate_9;
        v_hit_9.broadcast();
    }
    else if (n == 10) {
        0 => v_gate_10;
        v_hit_10.broadcast();
    }
    else if (n == 11) {
        0 => v_gate_11;
        v_hit_11.broadcast();
    }
    else if (n == 12) {
        0 => v_gate_12;
        v_hit_12.broadcast();
    }
    else if (n == 13) {
        0 => v_gate_13;
        v_hit_13.broadcast();
    }
    else if (n == 14) {
        0 => v_gate_14;
        v_hit_14.broadcast();
    }
    else if (n == 15) {
        0 => v_gate_15;
        v_hit_15.broadcast();
    }
}

fun void setKnot(int j, float t, float mu[], float sig[]) {
    if (j == 0) {
        t => sh_t_0;
        mu[0] => sh_mu_0_0;
        mu[1] => sh_mu_0_1;
        mu[2] => sh_mu_0_2;
        mu[3] => sh_mu_0_3;
        mu[4] => sh_mu_0_4;
        mu[5] => sh_mu_0_5;
        mu[6] => sh_mu_0_6;
        mu[7] => sh_mu_0_7;
        sig[0] => sh_sig_0_0;
        sig[1] => sh_sig_0_1;
        sig[2] => sh_sig_0_2;
        sig[3] => sh_sig_0_3;
        sig[4] => sh_sig_0_4;
        sig[5] => sh_sig_0_5;
        sig[6] => sh_sig_0_6;
        sig[7] => sh_sig_0_7;
    }
    else if (j == 1) {
        t => sh_t_1;
        mu[0] => sh_mu_1_0;
        mu[1] => sh_mu_1_1;
        mu[2] => sh_mu_1_2;
        mu[3] => sh_mu_1_3;
        mu[4] => sh_mu_1_4;
        mu[5] => sh_mu_1_5;
        mu[6] => sh_mu_1_6;
        mu[7] => sh_mu_1_7;
        sig[0] => sh_sig_1_0;
        sig[1] => sh_sig_1_1;
        sig[2] => sh_sig_1_2;
        sig[3] => sh_sig_1_3;
        sig[4] => sh_sig_1_4;
        sig[5] => sh_sig_1_5;
        sig[6] => sh_sig_1_6;
        sig[7] => sh_sig_1_7;
    }
    else if (j == 2) {
        t => sh_t_2;
        mu[0] => sh_mu_2_0;
        mu[1] => sh_mu_2_1;
        mu[2] => sh_mu_2_2;
        mu[3] => sh_mu_2_3;
        mu[4] => sh_mu_2_4;
        mu[5] => sh_mu_2_5;
        mu[6] => sh_mu_2_6;
        mu[7] => sh_mu_2_7;
        sig[0] => sh_sig_2_0;
        sig[1] => sh_sig_2_1;
        sig[2] => sh_sig_2_2;
        sig[3] => sh_sig_2_3;
        sig[4] => sh_sig_2_4;
        sig[5] => sh_sig_2_5;
        sig[6] => sh_sig_2_6;
        sig[7] => sh_sig_2_7;
    }
    else if (j == 3) {
        t => sh_t_3;
        mu[0] => sh_mu_3_0;
        mu[1] => sh_mu_3_1;
        mu[2] => sh_mu_3_2;
        mu[3] => sh_mu_3_3;
        mu[4] => sh_mu_3_4;
        mu[5] => sh_mu_3_5;
        mu[6] => sh_mu_3_6;
        mu[7] => sh_mu_3_7;
        sig[0] => sh_sig_3_0;
        sig[1] => sh_sig_3_1;
        sig[2] => sh_sig_3_2;
        sig[3] => sh_sig_3_3;
        sig[4] => sh_sig_3_4;
        sig[5] => sh_sig_3_5;
        sig[6] => sh_sig_3_6;
        sig[7] => sh_sig_3_7;
    }
    else if (j == 4) {
        t => sh_t_4;
        mu[0] => sh_mu_4_0;
        mu[1] => sh_mu_4_1;
        mu[2] => sh_mu_4_2;
        mu[3] => sh_mu_4_3;
        mu[4] => sh_mu_4_4;
        mu[5] => sh_mu_4_5;
        mu[6] => sh_mu_4_6;
        mu[7] => sh_mu_4_7;
        sig[0] => sh_sig_4_0;
        sig[1] => sh_sig_4_1;
        sig[2] => sh_sig_4_2;
        sig[3] => sh_sig_4_3;
        sig[4] => sh_sig_4_4;
        sig[5] => sh_sig_4_5;
        sig[6] => sh_sig_4_6;
        sig[7] => sh_sig_4_7;
    }
    else if (j == 5) {
        t => sh_t_5;
        mu[0] => sh_mu_5_0;
        mu[1] => sh_mu_5_1;
        mu[2] => sh_mu_5_2;
        mu[3] => sh_mu_5_3;
        mu[4] => sh_mu_5_4;
        mu[5] => sh_mu_5_5;
        mu[6] => sh_mu_5_6;
        mu[7] => sh_mu_5_7;
        sig[0] => sh_sig_5_0;
        sig[1] => sh_sig_5_1;
        sig[2] => sh_sig_5_2;
        sig[3] => sh_sig_5_3;
        sig[4] => sh_sig_5_4;
        sig[5] => sh_sig_5_5;
        sig[6] => sh_sig_5_6;
        sig[7] => sh_sig_5_7;
    }
    else if (j == 6) {
        t => sh_t_6;
        mu[0] => sh_mu_6_0;
        mu[1] => sh_mu_6_1;
        mu[2] => sh_mu_6_2;
        mu[3] => sh_mu_6_3;
        mu[4] => sh_mu_6_4;
        mu[5] => sh_mu_6_5;
        mu[6] => sh_mu_6_6;
        mu[7] => sh_mu_6_7;
        sig[0] => sh_sig_6_0;
        sig[1] => sh_sig_6_1;
        sig[2] => sh_sig_6_2;
        sig[3] => sh_sig_6_3;
        sig[4] => sh_sig_6_4;
        sig[5] => sh_sig_6_5;
        sig[6] => sh_sig_6_6;
        sig[7] => sh_sig_6_7;
    }
    else if (j == 7) {
        t => sh_t_7;
        mu[0] => sh_mu_7_0;
        mu[1] => sh_mu_7_1;
        mu[2] => sh_mu_7_2;
        mu[3] => sh_mu_7_3;
        mu[4] => sh_mu_7_4;
        mu[5] => sh_mu_7_5;
        mu[6] => sh_mu_7_6;
        mu[7] => sh_mu_7_7;
        sig[0] => sh_sig_7_0;
        sig[1] => sh_sig_7_1;
        sig[2] => sh_sig_7_2;
        sig[3] => sh_sig_7_3;
        sig[4] => sh_sig_7_4;
        sig[5] => sh_sig_7_5;
        sig[6] => sh_sig_7_6;
        sig[7] => sh_sig_7_7;
    }
    else if (j == 8) {
        t => sh_t_8;
        mu[0] => sh_mu_8_0;
        mu[1] => sh_mu_8_1;
        mu[2] => sh_mu_8_2;
        mu[3] => sh_mu_8_3;
        mu[4] => sh_mu_8_4;
        mu[5] => sh_mu_8_5;
        mu[6] => sh_mu_8_6;
        mu[7] => sh_mu_8_7;
        sig[0] => sh_sig_8_0;
        sig[1] => sh_sig_8_1;
        sig[2] => sh_sig_8_2;
        sig[3] => sh_sig_8_3;
        sig[4] => sh_sig_8_4;
        sig[5] => sh_sig_8_5;
        sig[6] => sh_sig_8_6;
        sig[7] => sh_sig_8_7;
    }
    else if (j == 9) {
        t => sh_t_9;
        mu[0] => sh_mu_9_0;
        mu[1] => sh_mu_9_1;
        mu[2] => sh_mu_9_2;
        mu[3] => sh_mu_9_3;
        mu[4] => sh_mu_9_4;
        mu[5] => sh_mu_9_5;
        mu[6] => sh_mu_9_6;
        mu[7] => sh_mu_9_7;
        sig[0] => sh_sig_9_0;
        sig[1] => sh_sig_9_1;
        sig[2] => sh_sig_9_2;
        sig[3] => sh_sig_9_3;
        sig[4] => sh_sig_9_4;
        sig[5] => sh_sig_9_5;
        sig[6] => sh_sig_9_6;
        sig[7] => sh_sig_9_7;
    }
    else if (j == 10) {
        t => sh_t_10;
        mu[0] => sh_mu_10_0;
        mu[1] => sh_mu_10_1;
        mu[2] => sh_mu_10_2;
        mu[3] => sh_mu_10_3;
        mu[4] => sh_mu_10_4;
        mu[5] => sh_mu_10_5;
        mu[6] => sh_mu_10_6;
        mu[7] => sh_mu_10_7;
        sig[0] => sh_sig_10_0;
        sig[1] => sh_sig_10_1;
        sig[2] => sh_sig_10_2;
        sig[3] => sh_sig_10_3;
        sig[4] => sh_sig_10_4;
        sig[5] => sh_sig_10_5;
        sig[6] => sh_sig_10_6;
        sig[7] => sh_sig_10_7;
    }
    else if (j == 11) {
        t => sh_t_11;
        mu[0] => sh_mu_11_0;
        mu[1] => sh_mu_11_1;
        mu[2] => sh_mu_11_2;
        mu[3] => sh_mu_11_3;
        mu[4] => sh_mu_11_4;
        mu[5] => sh_mu_11_5;
        mu[6] => sh_mu_11_6;
        mu[7] => sh_mu_11_7;
        sig[0] => sh_sig_11_0;
        sig[1] => sh_sig_11_1;
        sig[2] => sh_sig_11_2;
        sig[3] => sh_sig_11_3;
        sig[4] => sh_sig_11_4;
        sig[5] => sh_sig_11_5;
        sig[6] => sh_sig_11_6;
        sig[7] => sh_sig_11_7;
    }
    else if (j == 12) {
        t => sh_t_12;
        mu[0] => sh_mu_12_0;
        mu[1] => sh_mu_12_1;
        mu[2] => sh_mu_12_2;
        mu[3] => sh_mu_12_3;
        mu[4] => sh_mu_12_4;
        mu[5] => sh_mu_12_5;
        mu[6] => sh_mu_12_6;
        mu[7] => sh_mu_12_7;
        sig[0] => sh_sig_12_0;
        sig[1] => sh_sig_12_1;
        sig[2] => sh_sig_12_2;
        sig[3] => sh_sig_12_3;
        sig[4] => sh_sig_12_4;
        sig[5] => sh_sig_12_5;
        sig[6] => sh_sig_12_6;
        sig[7] => sh_sig_12_7;
    }
    else if (j == 13) {
        t => sh_t_13;
        mu[0] => sh_mu_13_0;
        mu[1] => sh_mu_13_1;
        mu[2] => sh_mu_13_2;
        mu[3] => sh_mu_13_3;
        mu[4] => sh_mu_13_4;
        mu[5] => sh_mu_13_5;
        mu[6] => sh_mu_13_6;
        mu[7] => sh_mu_13_7;
        sig[0] => sh_sig_13_0;
        sig[1] => sh_sig_13_1;
        sig[2] => sh_sig_13_2;
        sig[3] => sh_sig_13_3;
        sig[4] => sh_sig_13_4;
        sig[5] => sh_sig_13_5;
        sig[6] => sh_sig_13_6;
        sig[7] => sh_sig_13_7;
    }
    else if (j == 14) {
        t => sh_t_14;
        mu[0] => sh_mu_14_0;
        mu[1] => sh_mu_14_1;
        mu[2] => sh_mu_14_2;
        mu[3] => sh_mu_14_3;
        mu[4] => sh_mu_14_4;
        mu[5] => sh_mu_14_5;
        mu[6] => sh_mu_14_6;
        mu[7] => sh_mu_14_7;
        sig[0] => sh_sig_14_0;
        sig[1] => sh_sig_14_1;
        sig[2] => sh_sig_14_2;
        sig[3] => sh_sig_14_3;
        sig[4] => sh_sig_14_4;
        sig[5] => sh_sig_14_5;
        sig[6] => sh_sig_14_6;
        sig[7] => sh_sig_14_7;
    }
    else if (j == 15) {
        t => sh_t_15;
        mu[0] => sh_mu_15_0;
        mu[1] => sh_mu_15_1;
        mu[2] => sh_mu_15_2;
        mu[3] => sh_mu_15_3;
        mu[4] => sh_mu_15_4;
        mu[5] => sh_mu_15_5;
        mu[6] => sh_mu_15_6;
        mu[7] => sh_mu_15_7;
        sig[0] => sh_sig_15_0;
        sig[1] => sh_sig_15_1;
        sig[2] => sh_sig_15_2;
        sig[3] => sh_sig_15_3;
        sig[4] => sh_sig_15_4;
        sig[5] => sh_sig_15_5;
        sig[6] => sh_sig_15_6;
        sig[7] => sh_sig_15_7;
    }
    else if (j == 16) {
        t => sh_t_16;
        mu[0] => sh_mu_16_0;
        mu[1] => sh_mu_16_1;
        mu[2] => sh_mu_16_2;
        mu[3] => sh_mu_16_3;
        mu[4] => sh_mu_16_4;
        mu[5] => sh_mu_16_5;
        mu[6] => sh_mu_16_6;
        mu[7] => sh_mu_16_7;
        sig[0] => sh_sig_16_0;
        sig[1] => sh_sig_16_1;
        sig[2] => sh_sig_16_2;
        sig[3] => sh_sig_16_3;
        sig[4] => sh_sig_16_4;
        sig[5] => sh_sig_16_5;
        sig[6] => sh_sig_16_6;
        sig[7] => sh_sig_16_7;
    }
    else if (j == 17) {
        t => sh_t_17;
        mu[0] => sh_mu_17_0;
        mu[1] => sh_mu_17_1;
        mu[2] => sh_mu_17_2;
        mu[3] => sh_mu_17_3;
        mu[4] => sh_mu_17_4;
        mu[5] => sh_mu_17_5;
        mu[6] => sh_mu_17_6;
        mu[7] => sh_mu_17_7;
        sig[0] => sh_sig_17_0;
        sig[1] => sh_sig_17_1;
        sig[2] => sh_sig_17_2;
        sig[3] => sh_sig_17_3;
        sig[4] => sh_sig_17_4;
        sig[5] => sh_sig_17_5;
        sig[6] => sh_sig_17_6;
        sig[7] => sh_sig_17_7;
    }
    else if (j == 18) {
        t => sh_t_18;
        mu[0] => sh_mu_18_0;
        mu[1] => sh_mu_18_1;
        mu[2] => sh_mu_18_2;
        mu[3] => sh_mu_18_3;
        mu[4] => sh_mu_18_4;
        mu[5] => sh_mu_18_5;
        mu[6] => sh_mu_18_6;
        mu[7] => sh_mu_18_7;
        sig[0] => sh_sig_18_0;
        sig[1] => sh_sig_18_1;
        sig[2] => sh_sig_18_2;
        sig[3] => sh_sig_18_3;
        sig[4] => sh_sig_18_4;
        sig[5] => sh_sig_18_5;
        sig[6] => sh_sig_18_6;
        sig[7] => sh_sig_18_7;
    }
    else if (j == 19) {
        t => sh_t_19;
        mu[0] => sh_mu_19_0;
        mu[1] => sh_mu_19_1;
        mu[2] => sh_mu_19_2;
        mu[3] => sh_mu_19_3;
        mu[4] => sh_mu_19_4;
        mu[5] => sh_mu_19_5;
        mu[6] => sh_mu_19_6;
        mu[7] => sh_mu_19_7;
        sig[0] => sh_sig_19_0;
        sig[1] => sh_sig_19_1;
        sig[2] => sh_sig_19_2;
        sig[3] => sh_sig_19_3;
        sig[4] => sh_sig_19_4;
        sig[5] => sh_sig_19_5;
        sig[6] => sh_sig_19_6;
        sig[7] => sh_sig_19_7;
    }
    else if (j == 20) {
        t => sh_t_20;
        mu[0] => sh_mu_20_0;
        mu[1] => sh_mu_20_1;
        mu[2] => sh_mu_20_2;
        mu[3] => sh_mu_20_3;
        mu[4] => sh_mu_20_4;
        mu[5] => sh_mu_20_5;
        mu[6] => sh_mu_20_6;
        mu[7] => sh_mu_20_7;
        sig[0] => sh_sig_20_0;
        sig[1] => sh_sig_20_1;
        sig[2] => sh_sig_20_2;
        sig[3] => sh_sig_20_3;
        sig[4] => sh_sig_20_4;
        sig[5] => sh_sig_20_5;
        sig[6] => sh_sig_20_6;
        sig[7] => sh_sig_20_7;
    }
    else if (j == 21) {
        t => sh_t_21;
        mu[0] => sh_mu_21_0;
        mu[1] => sh_mu_21_1;
        mu[2] => sh_mu_21_2;
        mu[3] => sh_mu_21_3;
        mu[4] => sh_mu_21_4;
        mu[5] => sh_mu_21_5;
        mu[6] => sh_mu_21_6;
        mu[7] => sh_mu_21_7;
        sig[0] => sh_sig_21_0;
        sig[1] => sh_sig_21_1;
        sig[2] => sh_sig_21_2;
        sig[3] => sh_sig_21_3;
        sig[4] => sh_sig_21_4;
        sig[5] => sh_sig_21_5;
        sig[6] => sh_sig_21_6;
        sig[7] => sh_sig_21_7;
    }
    else if (j == 22) {
        t => sh_t_22;
        mu[0] => sh_mu_22_0;
        mu[1] => sh_mu_22_1;
        mu[2] => sh_mu_22_2;
        mu[3] => sh_mu_22_3;
        mu[4] => sh_mu_22_4;
        mu[5] => sh_mu_22_5;
        mu[6] => sh_mu_22_6;
        mu[7] => sh_mu_22_7;
        sig[0] => sh_sig_22_0;
        sig[1] => sh_sig_22_1;
        sig[2] => sh_sig_22_2;
        sig[3] => sh_sig_22_3;
        sig[4] => sh_sig_22_4;
        sig[5] => sh_sig_22_5;
        sig[6] => sh_sig_22_6;
        sig[7] => sh_sig_22_7;
    }
    else if (j == 23) {
        t => sh_t_23;
        mu[0] => sh_mu_23_0;
        mu[1] => sh_mu_23_1;
        mu[2] => sh_mu_23_2;
        mu[3] => sh_mu_23_3;
        mu[4] => sh_mu_23_4;
        mu[5] => sh_mu_23_5;
        mu[6] => sh_mu_23_6;
        mu[7] => sh_mu_23_7;
        sig[0] => sh_sig_23_0;
        sig[1] => sh_sig_23_1;
        sig[2] => sh_sig_23_2;
        sig[3] => sh_sig_23_3;
        sig[4] => sh_sig_23_4;
        sig[5] => sh_sig_23_5;
        sig[6] => sh_sig_23_6;
        sig[7] => sh_sig_23_7;
    }
    else if (j == 24) {
        t => sh_t_24;
        mu[0] => sh_mu_24_0;
        mu[1] => sh_mu_24_1;
        mu[2] => sh_mu_24_2;
        mu[3] => sh_mu_24_3;
        mu[4] => sh_mu_24_4;
        mu[5] => sh_mu_24_5;
        mu[6] => sh_mu_24_6;
        mu[7] => sh_mu_24_7;
        sig[0] => sh_sig_24_0;
        sig[1] => sh_sig_24_1;
        sig[2] => sh_sig_24_2;
        sig[3] => sh_sig_24_3;
        sig[4] => sh_sig_24_4;
        sig[5] => sh_sig_24_5;
        sig[6] => sh_sig_24_6;
        sig[7] => sh_sig_24_7;
    }
    else if (j == 25) {
        t => sh_t_25;
        mu[0] => sh_mu_25_0;
        mu[1] => sh_mu_25_1;
        mu[2] => sh_mu_25_2;
        mu[3] => sh_mu_25_3;
        mu[4] => sh_mu_25_4;
        mu[5] => sh_mu_25_5;
        mu[6] => sh_mu_25_6;
        mu[7] => sh_mu_25_7;
        sig[0] => sh_sig_25_0;
        sig[1] => sh_sig_25_1;
        sig[2] => sh_sig_25_2;
        sig[3] => sh_sig_25_3;
        sig[4] => sh_sig_25_4;
        sig[5] => sh_sig_25_5;
        sig[6] => sh_sig_25_6;
        sig[7] => sh_sig_25_7;
    }
    else if (j == 26) {
        t => sh_t_26;
        mu[0] => sh_mu_26_0;
        mu[1] => sh_mu_26_1;
        mu[2] => sh_mu_26_2;
        mu[3] => sh_mu_26_3;
        mu[4] => sh_mu_26_4;
        mu[5] => sh_mu_26_5;
        mu[6] => sh_mu_26_6;
        mu[7] => sh_mu_26_7;
        sig[0] => sh_sig_26_0;
        sig[1] => sh_sig_26_1;
        sig[2] => sh_sig_26_2;
        sig[3] => sh_sig_26_3;
        sig[4] => sh_sig_26_4;
        sig[5] => sh_sig_26_5;
        sig[6] => sh_sig_26_6;
        sig[7] => sh_sig_26_7;
    }
    else if (j == 27) {
        t => sh_t_27;
        mu[0] => sh_mu_27_0;
        mu[1] => sh_mu_27_1;
        mu[2] => sh_mu_27_2;
        mu[3] => sh_mu_27_3;
        mu[4] => sh_mu_27_4;
        mu[5] => sh_mu_27_5;
        mu[6] => sh_mu_27_6;
        mu[7] => sh_mu_27_7;
        sig[0] => sh_sig_27_0;
        sig[1] => sh_sig_27_1;
        sig[2] => sh_sig_27_2;
        sig[3] => sh_sig_27_3;
        sig[4] => sh_sig_27_4;
        sig[5] => sh_sig_27_5;
        sig[6] => sh_sig_27_6;
        sig[7] => sh_sig_27_7;
    }
    else if (j == 28) {
        t => sh_t_28;
        mu[0] => sh_mu_28_0;
        mu[1] => sh_mu_28_1;
        mu[2] => sh_mu_28_2;
        mu[3] => sh_mu_28_3;
        mu[4] => sh_mu_28_4;
        mu[5] => sh_mu_28_5;
        mu[6] => sh_mu_28_6;
        mu[7] => sh_mu_28_7;
        sig[0] => sh_sig_28_0;
        sig[1] => sh_sig_28_1;
        sig[2] => sh_sig_28_2;
        sig[3] => sh_sig_28_3;
        sig[4] => sh_sig_28_4;
        sig[5] => sh_sig_28_5;
        sig[6] => sh_sig_28_6;
        sig[7] => sh_sig_28_7;
    }
    else if (j == 29) {
        t => sh_t_29;
        mu[0] => sh_mu_29_0;
        mu[1] => sh_mu_29_1;
        mu[2] => sh_mu_29_2;
        mu[3] => sh_mu_29_3;
        mu[4] => sh_mu_29_4;
        mu[5] => sh_mu_29_5;
        mu[6] => sh_mu_29_6;
        mu[7] => sh_mu_29_7;
        sig[0] => sh_sig_29_0;
        sig[1] => sh_sig_29_1;
        sig[2] => sh_sig_29_2;
        sig[3] => sh_sig_29_3;
        sig[4] => sh_sig_29_4;
        sig[5] => sh_sig_29_5;
        sig[6] => sh_sig_29_6;
        sig[7] => sh_sig_29_7;
    }
    else if (j == 30) {
        t => sh_t_30;
        mu[0] => sh_mu_30_0;
        mu[1] => sh_mu_30_1;
        mu[2] => sh_mu_30_2;
        mu[3] => sh_mu_30_3;
        mu[4] => sh_mu_30_4;
        mu[5] => sh_mu_30_5;
        mu[6] => sh_mu_30_6;
        mu[7] => sh_mu_30_7;
        sig[0] => sh_sig_30_0;
        sig[1] => sh_sig_30_1;
        sig[2] => sh_sig_30_2;
        sig[3] => sh_sig_30_3;
        sig[4] => sh_sig_30_4;
        sig[5] => sh_sig_30_5;
        sig[6] => sh_sig_30_6;
        sig[7] => sh_sig_30_7;
    }
    else if (j == 31) {
        t => sh_t_31;
        mu[0] => sh_mu_31_0;
        mu[1] => sh_mu_31_1;
        mu[2] => sh_mu_31_2;
        mu[3] => sh_mu_31_3;
        mu[4] => sh_mu_31_4;
        mu[5] => sh_mu_31_5;
        mu[6] => sh_mu_31_6;
        mu[7] => sh_mu_31_7;
        sig[0] => sh_sig_31_0;
        sig[1] => sh_sig_31_1;
        sig[2] => sh_sig_31_2;
        sig[3] => sh_sig_31_3;
        sig[4] => sh_sig_31_4;
        sig[5] => sh_sig_31_5;
        sig[6] => sh_sig_31_6;
        sig[7] => sh_sig_31_7;
    }
}

// ---- main receive loop ----
while (true) {
    oin => now;
    while (oin.recv(msg)) {
        if (msg.address == "/sept/v1/hit") {
            msg.getString(0) => string vid;
            msg.getInt(2) => int onset;
            msg.getFloat(3) => float vel;
            float mu[8];
            float sig[8];
            for (0 => int i; i < 8; i++) { msg.getFloat(4+i) => mu[i]; }
            for (0 => int i; i < 8; i++) { msg.getFloat(12+i) => sig[i]; }
            // clamp defensively (conformance C2: out-of-range mu/sigma must not crash)
            for (0 => int i; i < 8; i++) {
                if (mu[i] < 0.0) 0.0 => mu[i];
                if (mu[i] > 1.0) 1.0 => mu[i];
                if (sig[i] < 0.0) 0.0 => sig[i];
                if (sig[i] > 1.0) 1.0 => sig[i];
            }

            if (onset == 1) {
                findSlotForOnset(vid) => int n;
                vid => slotVoice[n];
                useCounter++;
                useCounter => slotLastUse[n];
                for (0 => int i; i < 8; i++) { sig[i] => slotSig[n*8+i]; }
                vel => slotVel[n];
                setVoice(n, mu, sig, vel);
                if (TRACE_ENABLED) { <<< "sept_gate", now/second >>>; }
            } else {
                findSlotForRelease(vid) => int n;
                if (n >= 0) {
                    if (slotVoice[n] == vid) { "" => slotVoice[n]; }
                    releaseVoice(n);
                }
                // unknown voice_id release: no-op (conformance C3)
            }

        } else if (msg.address == "/sept/v1/move") {
            msg.getString(0) => string vid;
            float mu[8];
            for (0 => int i; i < 8; i++) { msg.getFloat(1+i) => mu[i]; }
            for (0 => int i; i < 8; i++) {
                if (mu[i] < 0.0) 0.0 => mu[i];
                if (mu[i] > 1.0) 1.0 => mu[i];
            }
            if (vid != "") {
                findSlotForRelease(vid) => int n;
                if (n >= 0) { setVoiceMuOnly(n, mu); }
                // voice_id unknown to this renderer: silently dropped (renderers
                // "may drop /move" per WIRE_FORMAT_v1.md; voice_id "" = global,
                // not bound to a per-voice slot -- not realized in v1, documented
                // deviation, matches lab_render.py's /move-ignored precedent).
            }

        } else if (msg.address == "/sept/v1/shape") {
            msg.getInt(1) => int layer;
            msg.getFloat(2) => float durationS;
            msg.getInt(3) => int nKnots;
            if (nKnots < 0) 0 => nKnots;
            if (nKnots > 32) 32 => nKnots;
            durationS => sh_dur;
            nKnots => sh_n;
            4 => int idx;
            for (0 => int j; j < nKnots; j++) {
                msg.getFloat(idx) => float t;
                idx++;
                float mu[8];
                float sig[8];
                for (0 => int i; i < 8; i++) { msg.getFloat(idx) => mu[i]; idx++; }
                for (0 => int i; i < 8; i++) { msg.getFloat(idx) => sig[i]; idx++; }
                setKnot(j, t, mu, sig);
            }
            // layer (0=play,1=loop) is parsed but only the play layer (layer==0)
            // is realized by sept_render_core.ck in v1 -- documented in the plan
            // (Task 2.1: "play layer only in v1").

        } else if (msg.address == "/sept/v1/manifest/request") {
            manifest_requested.broadcast();
        }
        // else: unknown /sept/v1/* address -> ignore silently (conformance C1)
    }
}
