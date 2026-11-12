// ============================================================
// WE-REMOTE LIGHTSABER
// ChucK synthesis-based lightsaber driven by We-Remote MIDI CC.
// ============================================================
// Firmware (We-Remote.ino) sends on MIDI channel 11 @ ~40Hz:
//   CC 28 = lidar  (0..127)    -> ignition / retraction trigger
//   CC 25 = gyroX (0..127, 64=center)  -> swing X (high whoosh + pitch)
//   CC 26 = gyroY (0..127, 64=center)  -> swing Y (low whoosh + pitch)
//
// Behavior:
//   - Sustained monotonic ramp from ~0 to ~127 on lidar  -> IGNITE
//   - Sustained monotonic ramp from ~127 to ~0 on lidar  -> SHEATH
//   - Gyro X/Y continuously modulate swing whooshes and a doppler
//     pitch bend on the hum (different character per axis).
//   - Noise-rejection: requires a long enough window of mostly
//     monotonic motion covering both extremes. Single spikes, jitter,
//     or partial swings do NOT toggle the saber.
//
// Usage:
//   chuck Lightsaber.ck              (auto-finds We-Remote, else port 0)
//   chuck Lightsaber.ck:N            (force a specific port index)
// Audio out goes to the system default device (your MacBook speakers).
// ============================================================

// ---------------- MIDI (auto-detect) -------------------------
MidiIn min;
MidiMsg msg;
-1 => int MIDI_PORT;

// If a port was explicitly passed, use it directly.
if (me.args() > 0) {
    Std.atoi(me.arg(0)) => MIDI_PORT;
    if (!min.open(MIDI_PORT)) {
        <<< "Could not open MIDI port", MIDI_PORT >>>;
        me.exit();
    }
} else {
    // Auto-scan: prefer a port whose name mentions the remote/MIDI device.
    // Fall back to the first port that opens at all.
    -1 => int firstOpen;
    for (0 => int p; p < 8; p++) {
        if (min.open(p)) {
            min.name() => string nm;
            // lowercase copy for matching
            "" => string lo;
            for (0 => int i; i < nm.length(); i++) {
                nm.substring(i, 1) => string ch;
                ch.lower() => ch;
                lo + ch => lo;
            }
            if (lo.find("remote") >= 0 || lo.find("we ") >= 0
                || lo.find("weremote") >= 0 || lo.find("bluetooth") >= 0) {
                p => MIDI_PORT;
                break;
            }
            if (firstOpen < 0) p => firstOpen;
        }
    }
    if (MIDI_PORT < 0) {
        if (firstOpen >= 0) {
            firstOpen => MIDI_PORT;
            min.open(MIDI_PORT);
        } else {
            <<< "No MIDI input devices found. Pair the We-Remote over BLE MIDI",
                "(Audio MIDI Setup -> Bluetooth) and re-run." >>>;
            me.exit();
        }
    }
}
<<< "Lightsaber MIDI in:", min.name(), "(port", MIDI_PORT, ")" >>>;
<<< "Audio out: system default." >>>;

// ============================================================
// *** VOLUMES — EDIT THESE TO BALANCE THE MIX ***
// Each constant is the final output level of that layer.
// Raise to make louder, lower to make quieter. 0 = muted.
// These feed directly into the matching Gain nodes below.
// ============================================================
1.00  => float VOL_HUM;          // main drone (sines + triangle + square + sub + h2)
0.90  => float VOL_CLICK;        // pitched Impulse crackles
0.015 => float VOL_POP;          // mid "thock" arc pops
0.010 => float VOL_BED;          // scratchy plasma bed under everything
0.006 => float VOL_GRIT;         // tasteful narrow-band warmth
0.060 => float VOL_SWING_X;      // gyro-X whoosh peak
0.060 => float VOL_SWING_Y;      // gyro-Y whoosh peak
0.196 => float VOL_ZAP_TONE;     // ignite/sheath tonal sweep
0.154 => float VOL_ZAP_NOISE;    // ignite/sheath noise burst (BPF swept)
0.010 => float VOL_ZAP_BED;      // continuous zap-character bed while saber is on
0.18  => float VOL_SWING_CRK;    // swing crack layer (fast, high-freq)
0.11  => float VOL_SWING_SWSH;   // swing swoosh layer (long, sweeping)
// ============================================================

// ============================================================
// SYNTHESIS GRAPH
// ============================================================
// Hum  : 3 detuned saws + sub sine + narrow-band hiss -> body BPF
// Gate : sabOut.gain fades in/out during ignite/sheath
// Swing: two independently-colored noise whooshes (X=high, Y=low)
// Zap  : swept sine + swept noise burst used for ignition/retract
// ============================================================
// ---- HUM CORE (weighted LOW) ----
// Sine fundamental + beating partner give the thick core.
// Triangle adds a gently-curved edge. The square is heavily
// low-passed so only its fundamental and a whiff of 3rd harmonic
// survive — this supplies the "plasma bite" without the saw-like
// lawn-mower buzz. Sub sine an octave below gives weight. An
// explicit 2nd-harmonic sine gives warmth without brightness.
SinOsc fA  => Gain humBus;
SinOsc fB  => humBus;
TriOsc fT  => humBus;
SqrOsc fS  => LPF fSLPF => humBus;
SinOsc sub => humBus;
SinOsc h2  => humBus;

// Narrow "throat" body resonance emphasizes ~230 Hz plasma formant.
humBus => BPF bodyR => Gain bodyBus;
humBus => bodyBus;                 // parallel dry (keeps low end)

// Upper-mid shimmer — quiet, just enough to say "electric".
Noise  grit  => BPF gritBPF  => Gain gritG  => bodyBus;

// Gentle top-end tame + master gate. NO reverb — dry and close.
bodyBus => LPF mainLPF => Gain sabOut => dac;

// ---- CRACKLE STACK (three independent layers, all post-gate) ----
// 1) Sharp impulse clicks through resonant BPF — the classic zap tick.
//    Single-sample impulses ring the bandpass for ~10-20 ms each.
Impulse clk => BPF clkBPF => Gain clkG => dac;

// 2) Short noise pops — wider-band, slightly longer events, sparser.
Noise popN => ADSR popEnv => BPF popBPF => Gain popG => dac;

// 3) Low-level scratchy bed — breathes under everything, never silent
//    while saber is on; filter drifts so it never sits still.
Noise bedN => BPF bedBPF => Gain bedG => dac;

// Swing whooshes (X = bright, Y = dark).
Noise swXn => BPF swXbpf => Gain swXg => dac;
Noise swYn => BPF swYbpf => Gain swYg => dac;

// Ignition / retraction transient. BOTH sides gated by ADSR envelopes
// so the noise is mathematically silent when not keyed on — no
// bleed-through from a live noise source.
SinOsc zap  => ADSR zapEnv  => LPF zapLPF  => Gain zapG  => dac;
Noise  zapN => ADSR zapNEnv => BPF zapNBPF => Gain zapNG => dac;

// Swing crack: sharp high-freq noise burst, very fast ADSR.
Noise swCrk => BPF swCrkBPF => ADSR swCrkEnv => Gain swCrkG => dac;
// Swing swoosh: mid noise that sweeps down, long tail.
Noise swSwsh => BPF swSwshBPF => ADSR swSwshEnv => Gain swSwshG => dac;

// Continuous "zap bed" — same tonal character as the ignite sweep,
// running quietly all the time while the saber is on. Gated by bedG
// ramping with sabOut.
SinOsc zapBed => LPF zapBedLPF => Gain zapBedG => dac;

// LFOs (read via .last() in driver shreds).
SinOsc vibLFO  => blackhole;
SinOsc tremLFO => blackhole;
6.4 => vibLFO.freq;
5.3 => tremLFO.freq;
1.0 => vibLFO.gain;
1.0 => tremLFO.gain;

// ---- parameters ----
60.5 => float BASE_HUM;            // 3 semitones lower — deeper plasma
BASE_HUM          => fA.freq;
BASE_HUM * 1.008  => fB.freq;      // slow beating
BASE_HUM * 1.003  => fT.freq;
BASE_HUM          => fS.freq;
BASE_HUM * 0.5    => sub.freq;
BASE_HUM * 2.0    => h2.freq;

0.22 => fA.gain;
0.18 => fB.gain;
0.14 => fT.gain;
0.10 => fS.gain;
0.50 => sub.gain;                  // octave-down bumped for weight
0.08 => h2.gain;

260.0 => fSLPF.freq;  1.8 => fSLPF.Q;   // keeps square from sounding saw-like
230.0 => bodyR.freq;  1.0 => bodyR.Q;   // plasma throat

// ---- GRIT (continuous narrow-band warmth) : VOL_GRIT ----
1.0    => grit.gain;               // source stays at 1, VOL_GRIT sets final
850.0  => gritBPF.freq; 3.5 => gritBPF.Q;
VOL_GRIT => gritG.gain;

// ---- CLICKS (pitched Impulse crackles) : VOL_CLICK ----
// Impulse through resonant BPF — Q boost is HUGE, keep VOL_CLICK small.
1400.0 => clkBPF.freq; 8.0 => clkBPF.Q;
VOL_CLICK => clkG.gain;

// ---- POPS (mid thock events) : VOL_POP ----
1.0   => popN.gain;                // source at 1, popG ramped per event
popEnv.set(2::ms, 20::ms, 0.0, 40::ms);
900.0 => popBPF.freq; 3.0 => popBPF.Q;
0.0   => popG.gain;

// ---- BED (continuous scratchy plasma bed) : VOL_BED ----
1.0    => bedN.gain;               // source at 1, bedG ramped per tick
2200.0 => bedBPF.freq; 2.0 => bedBPF.Q;
0.0    => bedG.gain;

1600.0 => mainLPF.freq; 0.9 => mainLPF.Q;

1.0 => humBus.gain;
1.0 => bodyBus.gain;
0.0 => sabOut.gain;                 // muted until ignition (ramps to VOL_HUM)

// ---- SWINGS (gyro-driven whooshes) : VOL_SWING_X / VOL_SWING_Y ----
1800.0 => swXbpf.freq; 2.0 => swXbpf.Q;
520.0  => swYbpf.freq; 2.0 => swYbpf.Q;
1.0 => swXn.gain;  1.0 => swYn.gain;   // source at 1, swingDriver scales
0.0 => swXg.gain;  0.0 => swYg.gain;

// ---- IGNITION / SHEATH : VOL_ZAP_TONE, VOL_ZAP_NOISE ----
zapEnv.set (5::ms, 80::ms, 0.0, 250::ms);
zapNEnv.set(3::ms, 60::ms, 0.0, 180::ms);
2000.0 => zapLPF.freq;
1.0 => zap.gain;
1.0 => zapN.gain;
500.0 => zapNBPF.freq; 1.2 => zapNBPF.Q;
// output Gains can stay open — ADSR envelopes do the gating now.
VOL_ZAP_TONE  => zapG.gain;
VOL_ZAP_NOISE => zapNG.gain;

// ---- SWING CRACK (sharp electrical snap) : VOL_SWING_CRK ----
swCrkEnv.set(1::ms, 25::ms, 0.0, 80::ms);
1.0 => swCrk.gain;
4000.0 => swCrkBPF.freq; 6.0 => swCrkBPF.Q;
VOL_SWING_CRK => swCrkG.gain;

// ---- SWING SWOOSH (long sweeping body) : VOL_SWING_SWSH ----
swSwshEnv.set(4::ms, 320::ms, 0.0, 380::ms);
1.0 => swSwsh.gain;
3000.0 => swSwshBPF.freq; 1.8 => swSwshBPF.Q;
VOL_SWING_SWSH => swSwshG.gain;

// ---- ZAP BED (continuous tonal hum twin) : VOL_ZAP_BED ----
BASE_HUM * 2.0 => zapBed.freq;
1.0 => zapBed.gain;
3200.0 => zapBedLPF.freq; 0.9 => zapBedLPF.Q;
0.0 => zapBedG.gain;             // fades in with saberOn

// ============================================================
// STATE
// ============================================================
0  => int saberOn;
0  => int prevBtn32;
64 => int gyroXCC;
64 => int gyroYCC;

now => time lastToggle;
300::ms => dur COOLDOWN;            // debounce button retrigger

// ============================================================
// SOUND ACTIONS
// ============================================================
// Ignite: wide noise burst sweeping 150Hz→7kHz + tone rising 30Hz→BASE_HUM*3.
// Crack punch fires at the moment of ignition. Dramatic, loud, noisy.
fun void igniteSweep() {
    30.0 => zap.freq;
    150.0 => zapNBPF.freq;
    1.5 => zapNBPF.Q;
    zapEnv.set(3::ms, 700::ms, 0.0, 400::ms);
    zapNEnv.set(2::ms, 500::ms, 0.0, 500::ms);
    zapEnv.keyOn();
    zapNEnv.keyOn();
    // crack punch at ignition start
    4500.0 + Math.random2f(-800.0, 800.0) => swCrkBPF.freq;
    Math.random2f(5.0, 10.0) => swCrkBPF.Q;
    VOL_SWING_CRK => swCrkG.gain;
    swCrkEnv.keyOn();
    // swoosh sweeps upward with the ignition
    200.0 => swSwshBPF.freq;
    1.5 => swSwshBPF.Q;
    VOL_SWING_SWSH * 1.4 => swSwshG.gain;
    swSwshEnv.set(6::ms, 800::ms, 0.0, 500::ms);
    swSwshEnv.keyOn();
    0 => int k;
    while (k < 80) {
        k / 80.0 => float t;
        t * t => float tc;
        30.0 + (BASE_HUM * 3.0 - 30.0) * tc => zap.freq;
        150.0  + tc * 6850.0 => zapNBPF.freq;
        200.0  + tc * 4800.0 => swSwshBPF.freq;
        2500.0 => zapLPF.freq;
        8::ms => now;
        k++;
    }
    zapEnv.keyOff();
    zapNEnv.keyOff();
    swSwshEnv.keyOff();
    500::ms => now;
    zapEnv.set(5::ms, 80::ms, 0.0, 250::ms);
    zapNEnv.set(3::ms, 60::ms, 0.0, 180::ms);
    swSwshEnv.set(4::ms, 320::ms, 0.0, 380::ms);
}

// Sheath: tone drops BASE_HUM*3→25Hz, noise sweeps 7kHz→100Hz.
fun void sheathSweep() {
    BASE_HUM * 3.0 => zap.freq;
    7000.0 => zapNBPF.freq;
    1.5 => zapNBPF.Q;
    zapEnv.set(3::ms, 750::ms, 0.0, 350::ms);
    zapNEnv.set(2::ms, 600::ms, 0.0, 400::ms);
    zapEnv.keyOn();
    zapNEnv.keyOn();
    3500.0 + Math.random2f(-600.0, 600.0) => swCrkBPF.freq;
    Math.random2f(5.0, 10.0) => swCrkBPF.Q;
    VOL_SWING_CRK => swCrkG.gain;
    swCrkEnv.keyOn();
    5000.0 => swSwshBPF.freq;
    1.5 => swSwshBPF.Q;
    VOL_SWING_SWSH * 1.4 => swSwshG.gain;
    swSwshEnv.set(6::ms, 850::ms, 0.0, 450::ms);
    swSwshEnv.keyOn();
    0 => int k;
    while (k < 80) {
        k / 80.0 => float t;
        1.0 - (1.0 - t) * (1.0 - t) => float tc;
        BASE_HUM * 3.0 + (25.0 - BASE_HUM * 3.0) * tc => zap.freq;
        7000.0 - tc * 6900.0 => zapNBPF.freq;
        5000.0 - tc * 4700.0 => swSwshBPF.freq;
        250.0 + (1.0 - tc) * 2250.0 => zapLPF.freq;
        9::ms => now;
        k++;
    }
    zapEnv.keyOff();
    zapNEnv.keyOff();
    swSwshEnv.keyOff();
    450::ms => now;
    zapEnv.set(5::ms, 80::ms, 0.0, 250::ms);
    zapNEnv.set(3::ms, 60::ms, 0.0, 180::ms);
    swSwshEnv.set(4::ms, 320::ms, 0.0, 380::ms);
}

// Swing: crack snap + sweeping noise whoosh, both scaled to magnitude.
fun void swingZap(float mag) {
    // crack — high-freq electrical snap
    Math.random2f(2500.0, 6000.0) => swCrkBPF.freq;
    Math.random2f(2.5, 5.0) => swCrkBPF.Q;
    (0.5 + 0.5 * mag) * VOL_SWING_CRK => swCrkG.gain;
    swCrkEnv.keyOn();
    // swoosh — mid noise sweeping down
    Math.random2f(2000.0, 5000.0) => float swStart;
    swStart => swSwshBPF.freq;
    Math.random2f(1.5, 3.5) => swSwshBPF.Q;
    (0.4 + 0.6 * mag) * VOL_SWING_SWSH => swSwshG.gain;
    swSwshEnv.keyOn();
    0 => int k;
    while (k < 35) {
        k / 35.0 => float t;
        swStart * (1.0 - 0.75 * t) => swSwshBPF.freq;
        7::ms => now;
        k++;
    }
    swSwshEnv.keyOff();
    swCrkEnv.keyOff();
}

fun void ignite() {
    <<< "  *** SABER IGNITE ***" >>>;
    1 => saberOn;
    spork ~ igniteSweep();
    0.0 => float g;
    VOL_HUM / 40.0 => float step;
    0 => int k;
    while (k < 40) {
        g + step => g;
        if (g > VOL_HUM) VOL_HUM => g;
        g => sabOut.gain;
        (g / VOL_HUM) * VOL_ZAP_BED => zapBedG.gain;
        10::ms => now;
        k++;
    }
    VOL_HUM    => sabOut.gain;
    VOL_ZAP_BED => zapBedG.gain;
}

fun void sheath() {
    <<< "  *** SABER SHEATH ***" >>>;
    0 => saberOn;
    spork ~ sheathSweep();
    sabOut.gain() => float g;
    0 => int k;
    while (k < 50) {
        g - 0.028 => g;
        if (g < 0.0) 0.0 => g;
        g => sabOut.gain;
        (g / VOL_HUM) * VOL_ZAP_BED => zapBedG.gain;
        12::ms => now;
        k++;
    }
    0.0 => sabOut.gain;
    0.0 => zapBedG.gain;
    0.0 => swXg.gain;
    0.0 => swYg.gain;
}

// ============================================================
// DRIFT DRIVER — random-walk pitch multiplier (slow, organic,
// uneven). Adds the wavering quality a smooth sine LFO can't
// give you. Clamped to +/-2% so it stays lightsaber, not siren.
// ============================================================
1.0 => float humDrift;
fun void driftDriver() {
    0.0 => float vel;
    while (true) {
        Math.random2f(-0.012, 0.012) => float kick;
        vel + kick => vel;
        vel * 0.88 => vel;                     // damping
        humDrift + vel * 0.06 => humDrift;
        humDrift + (1.0 - humDrift) * 0.02 => humDrift;   // pull to 1
        if (humDrift < 0.975) 0.975 => humDrift;
        if (humDrift > 1.025) 1.025 => humDrift;
        Math.random2(30, 70)::ms => now;
    }
}
spork ~ driftDriver();

// ============================================================
// PITCH DRIVER — flutter + drift + gyro doppler.
// Sets frequencies on ALL oscillators (only driver that writes them).
// ============================================================
fun void pitchDriver() {
    while (true) {
        vibLFO.last() => float v;          // -1..1
        1.0 + v * 0.0055 => float flutter;

        1.0 => float dop;
        if (saberOn) {
            gyroXCC - 64 => int rawX;
            gyroYCC - 64 => int ny_raw;
            // deadzone: snap to no-doppler instantly when near center
            6 => int DEAD;
            0.0 => float nx;
            0.0 => float ny;
            if (rawX >  DEAD) (rawX - DEAD) / 58.0 => nx;
            if (rawX < -DEAD) (rawX + DEAD) / 58.0 => nx;
            if (ny_raw >  DEAD) (ny_raw - DEAD) / 58.0 => ny;
            if (ny_raw < -DEAD) (ny_raw + DEAD) / 58.0 => ny;
            1.0 + nx * 0.259 + ny * 0.161 => dop;
            if (dop < 0.55) 0.55 => dop;
            if (dop > 1.80) 1.80 => dop;
        }

        flutter * dop * humDrift => float mul;
        BASE_HUM         * mul => fA.freq;
        BASE_HUM * 1.008 * mul => fB.freq;
        BASE_HUM * 1.003 * mul => fT.freq;
        BASE_HUM         * mul => fS.freq;
        BASE_HUM * 0.5   * mul => sub.freq;
        BASE_HUM * 2.0   * mul => h2.freq;
        BASE_HUM * 2.0   * mul => zapBed.freq;    // tracks hum so the bed "is" the saber

        tremLFO.last() => float tr;
        1.0 + tr * 0.07 => humBus.gain;

        8::ms => now;
    }
}
spork ~ pitchDriver();

// ============================================================
// FLICKER DRIVER — gentle random gain jitter on each oscillator
// so no two moments sound identical. Creates the "uneven texture".
// ============================================================
fun void flickerDriver() {
    while (true) {
        0.22 * Math.random2f(0.85, 1.15) => fA.gain;
        0.18 * Math.random2f(0.85, 1.15) => fB.gain;
        0.14 * Math.random2f(0.88, 1.12) => fT.gain;
        0.10 * Math.random2f(0.80, 1.20) => fS.gain;
        0.18 * Math.random2f(0.90, 1.10) => sub.gain;
        0.08 * Math.random2f(0.80, 1.20) => h2.gain;
        Math.random2(55, 140)::ms => now;
    }
}
spork ~ flickerDriver();

// ============================================================
// SWING DRIVER — whoosh levels + filter wobble only.
// ============================================================
fun void swingDriver() {
    0.0 => float swXlev;
    0.0 => float swYlev;
    while (true) {
        gyroXCC $ float => float gx;
        gyroYCC $ float => float gy;
        Math.fabs(gx - 64.0) / 64.0 => float magX;
        Math.fabs(gy - 64.0) / 64.0 => float magY;
        if (magX > 1.0) 1.0 => magX;
        if (magY > 1.0) 1.0 => magY;

        if (saberOn) {
            (magX * magX) * VOL_SWING_X => float tgtX;
            (magY * magY) * VOL_SWING_Y => float tgtY;
            swXlev + (tgtX - swXlev) * 0.18 => swXlev;
            swYlev + (tgtY - swYlev) * 0.18 => swYlev;

            (now / second) $ float => float ts;
            1800.0 + 800.0 * Math.sin(ts * 11.0) * magX => swXbpf.freq;
            520.0  + 320.0 * Math.sin(ts *  7.3) * magY => swYbpf.freq;
            6.0 + 4.0 * magX => swXbpf.Q;
            4.0 + 3.0 * magY => swYbpf.Q;

            swXlev => swXg.gain;
            swYlev => swYg.gain;
        } else {
            swXlev * 0.80 => swXlev;
            swYlev * 0.80 => swYlev;
            swXlev => swXg.gain;
            swYlev => swYg.gain;
        }
        15::ms => now;
    }
}
spork ~ swingDriver();

// ============================================================
// CLICK DRIVER — sharp impulse-driven tick clicks with cluster
// bursts (sometimes one, sometimes a flurry of 3-7 in 50-150ms).
// Each click re-randomizes BPF freq + Q, so every click sounds
// different. This is the dominant "crackle" layer.
// ============================================================
fun void clickDriver() {
    while (true) {
        if (!saberOn) { 150::ms => now; continue; }

        // 8% chance of a cluster of tiny crackles, otherwise sparse singles
        if (Math.randomf() < 0.08) {
            Math.random2(2, 5) => int n;
            for (0 => int i; i < n; i++) {
                if (!saberOn) break;
                Math.random2f(1800.0, 5200.0) => clkBPF.freq;
                Math.random2f(8.0, 22.0)      => clkBPF.Q;
                clk.next(Math.random2f(0.20, 0.90));
                Math.random2(18, 60)::ms => now;
            }
            Math.random2(400, 1400)::ms => now;
        } else {
            Math.random2(120, 900)::ms => now;     // much sparser singles
            if (!saberOn) continue;
            Math.random2f(2000.0, 5500.0) => clkBPF.freq;
            Math.random2f(7.0, 18.0)      => clkBPF.Q;
            clk.next(Math.random2f(0.15, 0.70));
        }
    }
}
spork ~ clickDriver();

// ============================================================
// POP DRIVER — rarer, fatter short noise pops (~10-25ms).
// Lower-pitched, chunkier than the clicks. Gives the occasional
// "thock" of arc discharge without sounding like white noise.
// ============================================================
fun void popDriver() {
    while (true) {
        Math.random2(700, 3000)::ms => now;     // rare
        if (!saberOn) continue;
        Math.random2f(250.0, 1300.0) => popBPF.freq;
        Math.random2f(5.0, 12.0)     => popBPF.Q;
        Math.random2f(0.4, 1.0) * VOL_POP => popG.gain;
        popEnv.keyOn();
        Math.random2(6, 15)::ms => now;
        popEnv.keyOff();
    }
}
spork ~ popDriver();

// ============================================================
// BED DRIVER — very-low-level scratchy noise with drifting
// filter center. Provides continuous uneven texture so the
// space between crackles isn't dead quiet.
// ============================================================
fun void bedDriver() {
    while (true) {
        0.0 => float target;
        if (saberOn) Math.random2f(0.3, 1.0) * VOL_BED => target;
        bedG.gain() => float g;
        g + (target - g) * 0.18 => g;
        g => bedG.gain;
        Math.random2f(1300.0, 3400.0) => bedBPF.freq;
        Math.random2f(2.0, 5.0)       => bedBPF.Q;
        Math.random2(35, 130)::ms => now;
    }
}
spork ~ bedDriver();

// ============================================================
// SWING ZAP DRIVER — fires a short pitched transient each time
// the gyro crosses a motion threshold. Pitch is generative (random
// within a musical range) and bends downward over the event for a
// "zing" character. Hysteresis + refractory period prevent rapid
// retrigger while still allowing fast successive swings.
// ============================================================
fun void swingZapDriver() {
    0.0 => float prevMag;
    now => time lastZap;
    120::ms => dur REFRACTORY;
    while (true) {
        if (!saberOn) { prevMag * 0.5 => prevMag; 20::ms => now; continue; }
        gyroXCC - 64 => int rx;
        gyroYCC - 64 => int ry;
        Math.sqrt((rx*rx + ry*ry) $ float) / 64.0 => float mag;
        if (mag > 1.0) 1.0 => mag;

        // rising edge past 0.35 with enough delta → fire, scaled to magnitude
        if (mag > 0.35 && (mag - prevMag) > 0.12 && (now - lastZap) > REFRACTORY) {
            now => lastZap;
            spork ~ swingZap(mag);
        }
        mag => prevMag;
        15::ms => now;
    }
}
spork ~ swingZapDriver();

// ============================================================
// MIDI LOOP
// ============================================================
<<< "Ready. Press B button (CC 32) to ignite / sheath." >>>;

while (true) {
    min => now;
    while (min.recv(msg)) {
        msg.data1 & 0xF0 => int status;
        if (status != 0xB0) continue;           // CC only

        msg.data2 => int cc;
        msg.data3 => int val;

        if (cc == 32) {                          // B button toggle
            if (val >= 64 && prevBtn32 == 0) {
                if (now - lastToggle > COOLDOWN) {
                    now => lastToggle;
                    if (!saberOn) spork ~ ignite();
                    else          spork ~ sheath();
                }
            }
            if (val >= 64) 1 => prevBtn32;
            else           0 => prevBtn32;
        } else if (cc == 25) {                   // gyroX
            val => gyroXCC;
        } else if (cc == 26) {                   // gyroY
            val => gyroYCC;
        }
    }
}
