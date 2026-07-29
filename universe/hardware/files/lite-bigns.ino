#include <Audio.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <Encoder.h>

// ── 20 modulated sine + envelope voices (5 per channel) ───────────────
AudioSynthWaveformModulated sines[20];
AudioSynthWaveform modulators[20];
AudioEffectEnvelope envs[20];
AudioAmplifier amps[20];
AudioConnection* mod_conns[20];

// ── White noise for waves and rain per channel ────────────────────────
AudioSynthNoiseWhite noises[4];
AudioFilterStateVariable lpfs[4];
AudioAmplifier amp_ns[4];

// ── White noise for thunder per channel ───────────────────────────────
AudioSynthNoiseWhite thunder_noises[4];
AudioFilterStateVariable thunder_lpfs[4];
AudioAmplifier thunder_amps[4];

// ── Sine sub-mixers per channel (for 5 voices) ───────────────────────
AudioMixer4 sine_mix_a[4];
AudioMixer4 sine_mix_b[4];

// ── Rain envelopes, filters, amps ─────────────────────────────────────
AudioEffectEnvelope rain_envs[20];
AudioFilterStateVariable rain_lpfs[20];
AudioAmplifier rain_amps[20];

// ── Rain sub-mixers per channel ───────────────────────────────────────
AudioMixer4 rain_mix_a[4];
AudioMixer4 rain_mix_b[4];

// ── Kick drum (ciesen-style: body + sub + click) ─────────────────────
AudioSynthWaveform kick_body;
AudioSynthWaveform kick_sub;
AudioSynthWaveform kick_click;
AudioEffectEnvelope kick_body_env;
AudioEffectEnvelope kick_sub_env;
AudioEffectEnvelope kick_click_env;
AudioMixer4 kick_mix;
AudioFilterStateVariable kick_hpf;
AudioAmplifier kick_out;
AudioConnection kc1(kick_body, 0, kick_body_env, 0);
AudioConnection kc2(kick_sub, 0, kick_sub_env, 0);
AudioConnection kc3(kick_click, 0, kick_click_env, 0);
AudioConnection kc4(kick_body_env, 0, kick_mix, 0);
AudioConnection kc5(kick_sub_env, 0, kick_mix, 1);
AudioConnection kc6(kick_click_env, 0, kick_mix, 2);
AudioConnection kc7(kick_mix, 0, kick_hpf, 0);
AudioConnection kc8(kick_hpf, 2, kick_out, 0); // output 2 = highpass

// ── Channel mixers for sine mix + waves + thunder + rain ──────────────
AudioMixer4 mixer1, mixer2, mixer3, mixer4;

// ── Pre-master mixers (channel mix + kick) ───────────────────────────
AudioMixer4 premix1, premix2, premix3, premix4;

// ── Master volume amplifiers (post-mixer) ────────────────────────────
AudioAmplifier master_amp1, master_amp2, master_amp3, master_amp4;
AudioOutputI2SQuad i2s_quad;

// ── Connections ────────────────────────────────────────────────────────
AudioConnection* voice_p[20];
AudioConnection* voice_pa[20];
AudioConnection* sine_a_to_b[4];
AudioConnection* sine_b_to_mixer[4];
AudioConnection* pn[4];
AudioConnection* pa_n[4];
AudioConnection* pm_n[4];
AudioConnection* thunder_pn[4];
AudioConnection* thunder_pa[4];
AudioConnection* thunder_pm[4];
AudioConnection* rain_voice_p[20];
AudioConnection* rain_lpf_conn[20];
AudioConnection* rain_voice_pa[20];
AudioConnection* rain_a_to_b[4];
AudioConnection* rain_b_to_mixer[4];

AudioConnection m1(mixer1, 0, premix1, 0);
AudioConnection m2(mixer2, 0, premix2, 0);
AudioConnection m3(mixer3, 0, premix3, 0);
AudioConnection m4(mixer4, 0, premix4, 0);
AudioConnection km1(kick_out, 0, premix1, 1);
AudioConnection km2(kick_out, 0, premix2, 1);
AudioConnection km3(kick_out, 0, premix3, 1);
AudioConnection km4(kick_out, 0, premix4, 1);
AudioConnection pm1(premix1, 0, master_amp1, 0);
AudioConnection pm2(premix2, 0, master_amp2, 0);
AudioConnection pm3(premix3, 0, master_amp3, 0);
AudioConnection pm4(premix4, 0, master_amp4, 0);
AudioConnection out1(master_amp1, 0, i2s_quad, 0);
AudioConnection out2(master_amp2, 0, i2s_quad, 1);
AudioConnection out3(master_amp3, 0, i2s_quad, 2);
AudioConnection out4(master_amp4, 0, i2s_quad, 3);

// ── 6-octave C-major scale ─────────────────────────────────────────────
const float cMajorScale[48] = {
   65.41,  73.42,  82.41,  87.31,  98.00, 110.00, 123.47, 130.81,
  130.81, 146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63,
  261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25,
  523.25, 587.33, 659.25, 698.46, 783.99, 880.00, 987.77,1046.50,
 1046.50,1174.66,1318.51,1396.91,1567.98,1760.00,1975.53,2093.00,
  2093.00,2349.32,2637.02,2793.83,3135.96,3520.00,3951.07,4186.01
};

// ── Voice state ────────────────────────────────────────────────────────
struct SineVoice {
  bool active = false;
  unsigned long nextTrigger = 0;
  unsigned long triggerTime = 0;
  unsigned long env_dur = 0;
  int note = 0;
  int ratio = 0;
  float index = 0.0f;
  float sweep_octave = 0.0f;
  bool isBird = false;
  bool isPad = false;
  int birdType = -1;
  float birdBaseFreq = 0.0f;
} sineVoices[20];

struct RainVoice {
  bool active = false;
  unsigned long triggerTime = 0;
  unsigned long env_dur = 0;
} rainVoices[20];

// ── Noise modulation state ────────────────────────────────────────────
struct NoiseState {
  float slight_phase = 0.0;
  float slight_freq = 0.1;
  float slight_amp = 450.0;
  bool sweeping = false;
  unsigned long sweepStart = 0;
  bool sweepUp = true;
  float maxCutoff = 300.0;
  unsigned long duration = 2000;
  unsigned long nextTrigger = 0;
  unsigned long lastUpdate = 0;
  float maxGain = 0.5f;
  float startCutoff = 800.0f;
} waves_states[4], thunder_states[4];

// ── Bird state ────────────────────────────────────────────────────────
struct BirdState {
  unsigned long nextCall = 0;
  int burstLeft = 0;
  unsigned long nextNote = 0;
  int currentNote = 0;
  int rel = 0;
} birdStates[4];

// Bird params from .ck
const int minBurst[4]     = {3, 2, 8, 4};
const int maxBurst[4]     = {6, 4, 16, 8};
const int minInt[4]       = {50, 200, 20, 40};
const int maxInt[4]       = {120, 400, 40, 80};
const int pitchMode[4]    = {1, -1, 0, 0};
const float bAtk[4]       = {15.0f, 40.0f, 5.0f, 10.0f};
const float bDec[4]       = {60.0f, 160.0f, 20.0f, 50.0f};
const int bStartNote[4]   = {32, 28, 38, 30};
const int bNoteRange[4]   = {8, 8, 8, 10};
const float bMinSweep[4]  = {0.3f, -1.0f, -0.15f, 0.05f};
const float bMaxSweep[4]  = {1.5f, -0.3f, 0.15f, 0.5f};
const float bVibRate[4]   = {25.0f, 6.0f, 40.0f, 15.0f};
const float bVibDepth[4]  = {0.5f, 0.12f, 1.0f, 0.25f};

// ── Hardware inputs ───────────────────────────────────────────────────
Encoder enc(6, 8);
const int encoderSWPin = 2;
const int potPins[3] = {14, 16, 17};
const int joystickXPin = 18;
const int joystickYPin = 19;
const int joystickButtonPin = 3;
const int arcadeButtonPin = 4;

long prevEncPos = 0;
int prevPotVals[3] = {0, 0, 0};
int prevJoyX = 0, prevJoyY = 0;
int prevJoyButton = HIGH, prevEncSW = HIGH, prevArcadeButton = HIGH;

// ── NeoPixels ─────────────────────────────────────────────────────────
#define NEOPIXEL_PIN 5
Adafruit_NeoPixel pixels(2, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#define NEOPIXEL_STRIP_PIN 9
#define NEOPIXEL_STRIP_COUNT 25
Adafruit_NeoPixel strip(NEOPIXEL_STRIP_COUNT, NEOPIXEL_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// ── FM bias parameters ────────────────────────────────────────────────
const int MAX_RATIO = 3;
const float MAX_INDEX = 0.05f;
const float BIAS_RATIO = 3.0f;
const float BIAS_INDEX = 4.0f;

// ── Preset system ─────────────────────────────────────────────────────
// 6 presets on a globe (sphere). Navigate with joystick like spinning
// a globe: X = longitude, Y = latitude. Softmax over dot products
// blends ALL presets. Position sticks when joystick is released.
//   0: Waves (north pole)    1: Sine Pads (eq 0°)    2: Birds (eq 90°)
//   3: Storm (eq 180°)       4: Pluck (eq 270°)      5: Kick (south pole)
#define NUM_PRESETS 6
int current_preset = 0;
unsigned long lastPresetChange = 0;
// Preset positions as unit vectors on sphere (x, y, z)
//   Waves/Kick at poles, Pads/Birds/Storm/Pluck around equator
const float preset_cart[NUM_PRESETS][3] = {
  { 0.0f,  0.0f,  1.0f},  // 0: Waves   — north pole
  { 1.0f,  0.0f,  0.0f},  // 1: Pads    — equator 0°
  { 0.0f,  1.0f,  0.0f},  // 2: Birds   — equator 90°
  {-1.0f,  0.0f,  0.0f},  // 3: Storm   — equator 180°
  { 0.0f, -1.0f,  0.0f},  // 4: Pluck   — equator 270°
  { 0.0f,  0.0f, -1.0f},  // 5: Kick    — south pole
};
// Spherical coords for each preset (for encoder snap)
const float preset_theta[NUM_PRESETS] = { 0.0f, PI/2, PI/2, PI/2, PI/2, PI };
const float preset_phi[NUM_PRESETS]   = { 0.0f, 0.0f, PI/2, PI, 3*PI/2, 0.0f };
// Current position on globe (theta=colatitude 0..PI, phi=longitude 0..2PI)
float nav_theta = 0.0f; // starts at north pole (Waves)
float nav_phi = 0.0f;

// ── Chord burst timer ─────────────────────────────────────────────────
unsigned long nextChordTime = 15000;

// ── Global durations ──────────────────────────────────────────────────
unsigned long env_duration_ms = 8000;
unsigned long note_duration_ms = 31;

// ── Per-layer volume (computed each frame from joystick position) ─────
float layer_vol[NUM_PRESETS] = {0, 0, 0, 0, 0, 0};

// ── Kick drum state ──────────────────────────────────────────────────
unsigned long kickNextHit = 0;
unsigned long kickHitTime = 0;
int kickSweepPhase = -1; // -1=idle, 0-3=sweep steps
unsigned long kickSweepTime = 0;
const float kickBPM = 131.0f;

// ── Voice allocation helpers ──────────────────────────────────────────
int findFreePadVoice(bool split) {
  int limit = split ? 10 : 20;
  for (int i = 0; i < limit; i++)
    if (!sineVoices[i].active) return i;
  return -1;
}

int findFreeBirdVoice(int btype, bool split) {
  if (split) {
    int base = (btype < 2) ? 10 : 15;
    for (int i = base; i < base + 5; i++)
      if (!sineVoices[i].active) return i;
    return -1;
  } else {
    int ch = btype;
    int base = ch * 5;
    for (int i = base; i < base + 5; i++)
      if (!sineVoices[i].active) return i;
    return -1;
  }
}

int findFreeRainVoice(int ch) {
  int base = ch * 5;
  for (int i = 0; i < 5; i++)
    if (!rainVoices[base + i].active) return base + i;
  return -1;
}

int findFreeVoiceAny() {
  for (int i = 0; i < 20; i++)
    if (!sineVoices[i].active) return i;
  return -1;
}

void setup() {
  AudioMemory(150);
  randomSeed(analogRead(15));

  for (int i = 0; i < 20; i++) {
    sines[i].amplitude(0.22);
    modulators[i].amplitude(0.0);
    sines[i].begin(WAVEFORM_SINE);
    modulators[i].begin(WAVEFORM_SINE);
    rain_lpfs[i].resonance(0.707);
    rain_envs[i].attack(1);
    rain_envs[i].decay(5);
    rain_envs[i].sustain(0.0);
    rain_envs[i].release(30);
    rain_amps[i].gain(0.0);
  }

  for (int ch = 0; ch < 4; ch++) {
    noises[ch].amplitude(0.0);
    lpfs[ch].frequency(300); lpfs[ch].resonance(0.707);
    amp_ns[ch].gain(0.0);
    thunder_noises[ch].amplitude(0.0);
    thunder_lpfs[ch].frequency(300); thunder_lpfs[ch].resonance(2.5);
    thunder_amps[ch].gain(0.0);
    pn[ch] = new AudioConnection(noises[ch], 0, lpfs[ch], 0);
    pa_n[ch] = new AudioConnection(lpfs[ch], 0, amp_ns[ch], 0);
    pm_n[ch] = new AudioConnection(amp_ns[ch], 0, *(&mixer1 + ch), 1);
    thunder_pn[ch] = new AudioConnection(thunder_noises[ch], 0, thunder_lpfs[ch], 0);
    thunder_pa[ch] = new AudioConnection(thunder_lpfs[ch], 0, thunder_amps[ch], 0);
    thunder_pm[ch] = new AudioConnection(thunder_amps[ch], 0, *(&mixer1 + ch), 2);
  }

  for (int i = 0; i < 20; i++) {
    envs[i].attack(2000);
    envs[i].decay(6000);
    envs[i].sustain(0.0);
    envs[i].release(30);
  }

  for (int i = 0; i < 20; i++) amps[i].gain(0.0);

  for (int i = 0; i < 20; i++) {
    mod_conns[i] = new AudioConnection(modulators[i], 0, sines[i], 0);
    voice_p[i] = new AudioConnection(sines[i], 0, envs[i], 0);
    voice_pa[i] = new AudioConnection(envs[i], 0, amps[i], 0);
    int ch = i / 5;
    rain_voice_p[i] = new AudioConnection(noises[ch], 0, rain_envs[i], 0);
    rain_lpf_conn[i] = new AudioConnection(rain_envs[i], 0, rain_lpfs[i], 0);
    rain_voice_pa[i] = new AudioConnection(rain_lpfs[i], 0, rain_amps[i], 0);
  }

  for (int ch = 0; ch < 4; ch++) {
    int base = ch * 5;
    new AudioConnection(amps[base + 0], 0, sine_mix_a[ch], 0);
    new AudioConnection(amps[base + 1], 0, sine_mix_a[ch], 1);
    new AudioConnection(amps[base + 2], 0, sine_mix_a[ch], 2);
    new AudioConnection(amps[base + 3], 0, sine_mix_a[ch], 3);
    sine_a_to_b[ch] = new AudioConnection(sine_mix_a[ch], 0, sine_mix_b[ch], 0);
    new AudioConnection(amps[base + 4], 0, sine_mix_b[ch], 1);
    for (int g = 0; g < 4; g++) sine_mix_a[ch].gain(g, 0.18);
    sine_mix_b[ch].gain(0, 1.0); sine_mix_b[ch].gain(1, 0.18);
    sine_b_to_mixer[ch] = new AudioConnection(sine_mix_b[ch], 0, *(&mixer1 + ch), 0);
  }

  for (int ch = 0; ch < 4; ch++) {
    int base = ch * 5;
    new AudioConnection(rain_amps[base + 0], 0, rain_mix_a[ch], 0);
    new AudioConnection(rain_amps[base + 1], 0, rain_mix_a[ch], 1);
    new AudioConnection(rain_amps[base + 2], 0, rain_mix_a[ch], 2);
    new AudioConnection(rain_amps[base + 3], 0, rain_mix_a[ch], 3);
    rain_a_to_b[ch] = new AudioConnection(rain_mix_a[ch], 0, rain_mix_b[ch], 0);
    new AudioConnection(rain_amps[base + 4], 0, rain_mix_b[ch], 1);
    for (int g = 0; g < 4; g++) rain_mix_a[ch].gain(g, 0.18);
    rain_mix_b[ch].gain(0, 1.0); rain_mix_b[ch].gain(1, 0.18);
    rain_b_to_mixer[ch] = new AudioConnection(rain_mix_b[ch], 0, *(&mixer1 + ch), 3);
  }

  for (auto& m : { &mixer1, &mixer2, &mixer3, &mixer4 }) {
    m->gain(0, 1.0); m->gain(1, 1.0); m->gain(2, 1.0); m->gain(3, 1.0);
  }
  for (auto& m : { &premix1, &premix2, &premix3, &premix4 }) {
    m->gain(0, 1.0); m->gain(1, 1.0);
  }
  master_amp1.gain(1.0); master_amp2.gain(1.0);
  master_amp3.gain(1.0); master_amp4.gain(1.0);

  // Kick drum init (ciesen.ck style)
  kick_body.begin(WAVEFORM_SINE);
  kick_body.frequency(200.0f);
  kick_body.amplitude(0.0f);
  kick_sub.begin(WAVEFORM_SINE);
  kick_sub.frequency(120.0f);
  kick_sub.amplitude(0.0f);
  kick_click.begin(WAVEFORM_SINE);
  kick_click.frequency(350.0f);
  kick_click.amplitude(0.0f);
  kick_body_env.attack(2); kick_body_env.decay(160); kick_body_env.sustain(0.0); kick_body_env.release(10);
  kick_sub_env.attack(3); kick_sub_env.decay(120); kick_sub_env.sustain(0.0); kick_sub_env.release(10);
  kick_click_env.attack(1); kick_click_env.decay(30); kick_click_env.sustain(0.0); kick_click_env.release(5);
  kick_mix.gain(0, 0.40f); // body (reduced for small speakers)
  kick_mix.gain(1, 0.10f); // sub (mostly cut — can't reproduce on 1" drivers)
  kick_mix.gain(2, 0.35f); // click (boosted — gives transient on tiny speakers)
  kick_hpf.frequency(180.0f); // HPF: cut everything below 180Hz
  kick_hpf.resonance(0.707f); // flat response, no resonant peak
  kick_out.gain(0.8f);

  Serial.begin(9600);
  pinMode(encoderSWPin, INPUT_PULLUP);
  pinMode(joystickButtonPin, INPUT_PULLUP);
  pinMode(arcadeButtonPin, INPUT_PULLUP);

  unsigned long t = millis();
  for (int v = 0; v < 4; v++) {
    waves_states[v].slight_freq = random(500, 1501) / 10000.0f;
    waves_states[v].slight_phase = random(0, 628) / 100.0f;
    waves_states[v].nextTrigger = t + random(10000, 30000);
    waves_states[v].lastUpdate = t;
    thunder_states[v].slight_freq = random(500, 1501) / 10000.0f;
    thunder_states[v].slight_phase = random(0, 628) / 100.0f;
    thunder_states[v].nextTrigger = t + random(2000, 6000);
    thunder_states[v].lastUpdate = t;
  }
  for (int i = 0; i < 20; i++) {
    sineVoices[i].nextTrigger = t + random(31);
  }
  for (int ch = 0; ch < 4; ch++) {
    birdStates[ch].nextCall = t + random(500, 2000);
  }
  nextChordTime = t + 15000;

  pixels.begin(); pixels.clear();
  strip.begin(); strip.clear(); strip.show();
  long initial_ep = enc.read();
  long initial_pos = initial_ep / 2;
  prevEncPos = initial_pos;
  current_preset = (((initial_pos % NUM_PRESETS) + NUM_PRESETS) % NUM_PRESETS);
  nav_theta = preset_theta[current_preset];
  nav_phi = preset_phi[current_preset];
}

void loop() {
  unsigned long now = millis();
  static unsigned long last_now = 0;
  float dt = (now - last_now) / 1000.0f;
  last_now = now;

  // ── Master volume pot ──
  static int lastMaster = -1;
  int masterPot = analogRead(potPins[2]);
  if (abs(masterPot - lastMaster) > 10) {
    float vol = masterPot / 1023.0f;
    master_amp1.gain(vol); master_amp2.gain(vol);
    master_amp3.gain(vol); master_amp4.gain(vol);
    lastMaster = masterPot;
  }

  // ── Probability pot ──
  int probPot = analogRead(potPins[1]);
  int triggerChance = map(probPot, 0, 1023, 0, 300);

  // ── Pitch pot ──
  int pitchPot = analogRead(potPins[0]);
  float pitch_shift = (pitchPot / 1023.0f) * 24.0f - 12.0f;

  // ── Encoder snaps to preset on globe ──
  long ep = enc.read();
  long newPos = ep / 2;
  if (newPos != prevEncPos && now - lastPresetChange > 50) {
    long delta = newPos - prevEncPos;
    if (delta > 0) current_preset = (current_preset + 1) % NUM_PRESETS;
    else if (delta < 0) current_preset = (current_preset - 1 + NUM_PRESETS) % NUM_PRESETS;
    nav_theta = preset_theta[current_preset];
    nav_phi = preset_phi[current_preset];
    lastPresetChange = now;
    prevEncPos = newPos;
  }

  // ── Joystick spins the globe ──
  // Y axis = latitude (theta): push down → south pole, push up → north pole
  // X axis = longitude (phi): rotate around the equator
  // Position STAYS when joystick returns to center.
  int joyX = analogRead(joystickXPin);
  int joyY = analogRead(joystickYPin);

  // Debug: print joystick raw + nav state every 200ms
  static unsigned long lastJoyDebug = 0;
  if (now - lastJoyDebug >= 200) {
    lastJoyDebug = now;
    Serial.print("JoyX="); Serial.print(joyX);
    Serial.print(" JoyY="); Serial.print(joyY);
    Serial.print(" theta="); Serial.print(nav_theta, 2);
    Serial.print(" phi="); Serial.print(nav_phi, 2);
    for (int i = 0; i < NUM_PRESETS; i++) {
      Serial.print(" L"); Serial.print(i); Serial.print("="); Serial.print(layer_vol[i], 2);
    }
    Serial.println();
  }

  const float deadzone = 0.12f;
  float jx = (joyX - 512) / 512.0f;
  float jy = (joyY - 512) / 512.0f;
  float joyMag = sqrtf(jx * jx + jy * jy);
  if (joyMag > deadzone) {
    float strength = (joyMag - deadzone) / (1.0f - deadzone);
    float nx = jx / joyMag * strength;
    float ny = jy / joyMag * strength;
    float speed = 2.0f;
    // theta: joystick Y moves latitude
    nav_theta -= ny * speed * dt;
    if (nav_theta < 0.01f) nav_theta = 0.01f;
    if (nav_theta > PI - 0.01f) nav_theta = PI - 0.01f;
    // phi: joystick X rotates longitude
    float phi_speed = speed / max(sinf(nav_theta), 0.15f);
    nav_phi += nx * phi_speed * dt;
    nav_phi = fmod(nav_phi, 2.0f * PI);
    if (nav_phi < 0.0f) nav_phi += 2.0f * PI;
  }

  // ── Compute per-layer volumes: softmax over dot products on sphere ──
  // Current position as unit vector
  float cur_x = sinf(nav_theta) * cosf(nav_phi);
  float cur_y = sinf(nav_theta) * sinf(nav_phi);
  float cur_z = cosf(nav_theta);

  static float prev_layer[NUM_PRESETS] = {0, 0, 0, 0, 0, 0};
  const float sharpness = 5.0f; // how peaked the blending is

  float weights[NUM_PRESETS];
  float weight_sum = 0.0f;
  for (int i = 0; i < NUM_PRESETS; i++) {
    // dot product = cosine of angle between current pos and preset
    float dot = cur_x * preset_cart[i][0]
              + cur_y * preset_cart[i][1]
              + cur_z * preset_cart[i][2];
    weights[i] = expf(sharpness * dot);
    weight_sum += weights[i];
  }
  for (int i = 0; i < NUM_PRESETS; i++) {
    layer_vol[i] = weights[i] / weight_sum;
  }

  // Layer indices: 0=Waves, 1=Pads, 2=Birds, 3=Storm(rain+thunder), 4=Pluck, 5=Kick
  float waves_vol   = layer_vol[0];
  float sine_vol    = layer_vol[1];
  float bird_vol    = layer_vol[2];
  float storm_vol   = layer_vol[3];
  float rain_vol    = storm_vol;
  float thunder_vol = storm_vol;
  float pluck_vol   = layer_vol[4];
  float kick_vol    = layer_vol[5];

  bool split_voices = (sine_vol > 0.0f && bird_vol > 0.0f);

  // ── Set noise amplitudes ──
  float noise_amp = max(0.264f * waves_vol, 0.3f * rain_vol);
  for (int ch = 0; ch < 4; ch++) {
    noises[ch].amplitude(noise_amp);
    thunder_noises[ch].amplitude(0.4f * thunder_vol);
  }

  // ── Activate if newly on ──
  if (thunder_vol > 0.0f && prev_layer[3] == 0.0f) {
    for (int ch = 0; ch < 4; ch++) {
      thunder_states[ch].nextTrigger = now + random(200, 2000);
      thunder_states[ch].sweeping = false;
      thunder_states[ch].lastUpdate = now;
    }
  }
  if (waves_vol > 0.0f && prev_layer[0] == 0.0f) {
    for (int ch = 0; ch < 4; ch++) {
      waves_states[ch].slight_freq = random(500, 1501) / 10000.0f;
      waves_states[ch].slight_phase = random(0, 628) / 100.0f;
      waves_states[ch].slight_amp = 450.0f;
      waves_states[ch].nextTrigger = now + random(10000, 30000);
      waves_states[ch].sweeping = false;
      waves_states[ch].lastUpdate = now;
    }
  }
  if (sine_vol > 0.0f && prev_layer[1] == 0.0f) {
    for (int i = 0; i < 20; i++)
      sineVoices[i].nextTrigger = now + random(31);
  }
  if (bird_vol > 0.0f && prev_layer[2] == 0.0f) {
    for (int ch = 0; ch < 4; ch++) {
      birdStates[ch].nextCall = now + random(500, 2000);
      birdStates[ch].burstLeft = 0;
      birdStates[ch].nextNote = now;
      birdStates[ch].currentNote = 0;
      birdStates[ch].rel = 0;
    }
  }
  if (pluck_vol > 0.0f && prev_layer[4] == 0.0f) {
    for (int i = 0; i < 20; i++)
      sineVoices[i].nextTrigger = now + random(16);
  }
  if (kick_vol > 0.0f && prev_layer[5] == 0.0f) {
    kickNextHit = now;
    kick_body.amplitude(0.40f);
    kick_sub.amplitude(0.20f);
    kick_click.amplitude(0.35f);
  }

  // ── Force stop if off ──
  if (thunder_vol == 0.0f && prev_layer[3] > 0.0f) {
    for (int ch = 0; ch < 4; ch++) {
      thunder_states[ch].sweeping = false;
      thunder_amps[ch].gain(0.0f);
    }
  }
  if (rain_vol == 0.0f && prev_layer[3] > 0.0f && thunder_vol == 0.0f) {
    for (int i = 0; i < 20; i++) {
      rain_amps[i].gain(0.0f);
      rainVoices[i].active = false;
    }
  }
  if (sine_vol == 0.0f && prev_layer[1] > 0.0f) {
    for (int i = 0; i < 20; i++) {
      if (sineVoices[i].active && !sineVoices[i].isBird) {
        modulators[i].amplitude(0.0f);
        amps[i].gain(0.0f);
        sineVoices[i].active = false;
      }
    }
  }
  if (bird_vol == 0.0f && prev_layer[2] > 0.0f) {
    for (int i = 0; i < 20; i++) {
      if (sineVoices[i].active && sineVoices[i].isBird) {
        modulators[i].amplitude(0.0f);
        amps[i].gain(0.0f);
        sineVoices[i].active = false;
      }
    }
  }
  if (waves_vol == 0.0f && prev_layer[0] > 0.0f) {
    for (int ch = 0; ch < 4; ch++) amp_ns[ch].gain(0.0f);
  }
  if (pluck_vol == 0.0f && prev_layer[4] > 0.0f) {
    // pluck uses same sine voices, let them ring out naturally
  }
  if (kick_vol == 0.0f && prev_layer[5] > 0.0f) {
    kick_body.amplitude(0.0f);
    kick_sub.amplitude(0.0f);
    kick_click.amplitude(0.0f);
    kickSweepPhase = -1;
  }

  for (int i = 0; i < NUM_PRESETS; i++) prev_layer[i] = layer_vol[i];

  // ── Voice de-activate ──
  for (int v = 0; v < 20; v++) {
    SineVoice& voice = sineVoices[v];
    if (voice.active && now - voice.triggerTime > voice.env_dur) {
      modulators[v].amplitude(0.0);
      amps[v].gain(0.0);
      voice.active = false;
    }
  }
  for (int v = 0; v < 20; v++) {
    RainVoice& rv = rainVoices[v];
    if (rv.active && now - rv.triggerTime > rv.env_dur) {
      rain_amps[v].gain(0.0f);
      rv.active = false;
    }
  }

  // ── Sine pad triggering (layer 1) ──
  if (sine_vol > 0.0f) {
    float sine_amp = (waves_vol > 0.0f) ? 0.176f : 0.22f;
    for (int v = 0; v < (split_voices ? 10 : 20); v++) {
      SineVoice& voice = sineVoices[v];
      if (voice.isBird) continue;
      if (now >= voice.nextTrigger) {
        voice.nextTrigger = now + 31;
        if (random(10000UL) < (unsigned long)triggerChance) {
          float r = random(1000) / 1000.0f;
          int ratio = 1 + (int)(powf(r, BIAS_RATIO) * (MAX_RATIO - 1));
          float ri = random(1000) / 1000.0f;
          float index = powf(ri, BIAS_INDEX) * MAX_INDEX;
          int noteIdx = random(40);
          voice.env_dur = 8000;
          float carrier = cMajorScale[noteIdx] * powf(2.0f, pitch_shift / 12.0f);
          float mod_freq = carrier * ratio;
          sines[v].frequency(carrier);
          sines[v].amplitude(sine_amp);
          modulators[v].frequency(mod_freq);
          modulators[v].amplitude(index * mod_freq / 440.0f);
          envs[v].attack(1200);
          envs[v].decay(3500);
          envs[v].noteOn();
          amps[v].gain(0.22f * sine_vol);
          voice.active = true;
          voice.note = noteIdx;
          voice.ratio = ratio;
          voice.index = index;
          voice.triggerTime = now;
          voice.isBird = false;
          voice.isPad = true;
          voice.sweep_octave = 0.0f;
        }
      }
    }

    // Chord burst every ~15s
    if (now >= nextChordTime) {
      int numNotes = 10 + random(5);
      const int chordDeg[4] = {0, 2, 4, 6};
      for (int cn = 0; cn < numNotes; cn++) {
        int v = findFreePadVoice(split_voices);
        if (v < 0) break;
        int oct = 2 + random(4);
        int cNote = oct * 8 + chordDeg[random(4)];
        if (cNote > 47) cNote = 47;
        float carrier = cMajorScale[cNote] * powf(2.0f, pitch_shift / 12.0f);
        float r = random(1000) / 1000.0f;
        int ratio = 1 + (int)(powf(r, BIAS_RATIO) * (MAX_RATIO - 1));
        float ri = random(1000) / 1000.0f;
        float index = powf(ri, BIAS_INDEX) * MAX_INDEX;
        float mod_freq = carrier * ratio;
        sines[v].frequency(carrier);
        sines[v].amplitude(0.176f);
        modulators[v].frequency(mod_freq);
        modulators[v].amplitude(index * mod_freq / 440.0f);
        envs[v].attack(1200);
        envs[v].decay(3500);
        envs[v].noteOn();
        amps[v].gain(0.22f * 0.95f * sine_vol);
        sineVoices[v].active = true;
        sineVoices[v].note = cNote;
        sineVoices[v].ratio = ratio;
        sineVoices[v].index = index;
        sineVoices[v].triggerTime = now;
        sineVoices[v].env_dur = 8000;
        sineVoices[v].isBird = false;
        sineVoices[v].isPad = true;
        sineVoices[v].sweep_octave = 0.0f;
      }
      nextChordTime = now + 15000;
    }
  }

  // ── Pluck triggering (layer 5) ──
  if (pluck_vol > 0.0f) {
    static int pluckVoice = 0;
    static unsigned long pluckNext = 0;
    static const float pluckRatios[4] = {1.0f, 2.0f, 1.5f, 3.0f};
    if (now >= pluckNext) {
      pluckNext = now + 16;
      if (random(10000UL) < (unsigned long)triggerChance) {
        int v = findFreeVoiceAny();
        if (v >= 0) {
          int noteIdx = random(40);
          float carrier = cMajorScale[noteIdx] * powf(2.0f, pitch_shift / 12.0f);
          float ratio = pluckRatios[pluckVoice % 4];
          float mod_freq = carrier * ratio;
          float modGain = carrier * ratio * (0.5f + random(1500) / 1000.0f);
          sines[v].frequency(carrier);
          sines[v].amplitude(0.22f);
          modulators[v].frequency(mod_freq);
          modulators[v].amplitude(modGain / 440.0f);
          envs[v].attack(3);
          envs[v].decay(140);
          envs[v].noteOn();
          amps[v].gain(0.25f * pluck_vol);
          sineVoices[v].active = true;
          sineVoices[v].note = noteIdx;
          sineVoices[v].ratio = (int)ratio;
          sineVoices[v].index = modGain / mod_freq;
          sineVoices[v].triggerTime = now;
          sineVoices[v].env_dur = 200;
          sineVoices[v].isBird = false;
          sineVoices[v].isPad = false;
          sineVoices[v].sweep_octave = 0.0f;
          pluckVoice = (pluckVoice + 1) % 4;
        }
      }
    }
  }

  // ── Bird triggering (layer 2) ──
  if (bird_vol > 0.0f) {
    float birdProb = min(bird_vol / 0.4f, 1.0f) * 0.23f;
    birdProb = birdProb * birdProb * birdProb;
    float birdGainMul = 0.3f;
    if (bird_vol > 0.4f) birdGainMul = 0.3f + (bird_vol - 0.4f) / 0.6f * 0.7f;

    for (int t = 0; t < 4; t++) {
      BirdState& bs = birdStates[t];
      if (now >= bs.nextCall && bs.burstLeft <= 0) {
        float roll = random(10000) / 10000.0f;
        if (roll < birdProb) {
          bs.burstLeft = random(minBurst[t], maxBurst[t] + 1);
          bs.currentNote = bStartNote[t] + random(bNoteRange[t] + 1);
          bs.rel = 0;
          bs.nextNote = now;
        }
        bs.nextCall = now + 20;
      }

      if (bs.burstLeft > 0 && now >= bs.nextNote) {
        int v = findFreeBirdVoice(t, split_voices);
        if (v >= 0) {
          int noteIdx = bs.currentNote + bs.rel;
          noteIdx = max(0, min(47, noteIdx));
          float carrier = cMajorScale[noteIdx] * powf(2.0f, pitch_shift / 12.0f);
          float atk = bAtk[t] + random(-5, 6);
          float dec = bDec[t] + random(-10, 11);
          if (atk < 1.0f) atk = 1.0f;
          if (dec < 1.0f) dec = 1.0f;
          envs[v].attack(atk);
          envs[v].decay(dec);
          envs[v].noteOn();
          float sweep = bMinSweep[t] + random((int)((bMaxSweep[t] - bMinSweep[t]) * 100.0f + 1)) / 100.0f;
          sines[v].frequency(carrier);
          sines[v].amplitude(0.22f);
          modulators[v].frequency(carrier);
          modulators[v].amplitude(0.01f);
          amps[v].gain(0.3f * birdGainMul * bird_vol);
          sineVoices[v].active = true;
          sineVoices[v].note = noteIdx;
          sineVoices[v].ratio = 1;
          sineVoices[v].index = 0.01f;
          sineVoices[v].triggerTime = now;
          sineVoices[v].env_dur = (unsigned long)(atk + dec);
          sineVoices[v].isBird = true;
          sineVoices[v].birdType = t;
          sineVoices[v].birdBaseFreq = carrier;
          sineVoices[v].sweep_octave = sweep;
          int delta = 0;
          if (pitchMode[t] == 1) delta = random(1, 4);
          else if (pitchMode[t] == -1) delta = -random(1, 4);
          else delta = random(-3, 4);
          bs.rel += delta + random(-1, 2);
          bs.rel = max(-10, min(10, bs.rel));
          bs.nextNote = now + random(minInt[t], maxInt[t] + 1);
          bs.burstLeft--;
          if (bs.burstLeft <= 0) bs.nextCall = now + 800 + random(2000);
        } else {
          bs.nextNote = now + 10;
        }
      }
    }
  }

  // ── Update active voice frequencies (vibrato + sweep) ──
  for (int v = 0; v < 20; v++) {
    SineVoice& voice = sineVoices[v];
    if (!voice.active) continue;
    float current_carrier = cMajorScale[voice.note] * powf(2.0f, pitch_shift / 12.0f);
    float elapsed_sec = (now - voice.triggerTime) / 1000.0f;
    float progress = (float)(now - voice.triggerTime) / (float)voice.env_dur;

    if (voice.isBird) {
      if (progress > 1.0f) progress = 1.0f;
      float swept = current_carrier * powf(2.0f, voice.sweep_octave * progress);
      int bt = voice.birdType;
      if (bt >= 0 && bt < 4) {
        float vib = sinf(elapsed_sec * bVibRate[bt] * 6.2832f);
        swept *= powf(2.0f, bVibDepth[bt] / 12.0f * vib);
      }
      sines[v].frequency(swept);
      modulators[v].frequency(swept);
    } else {
      float base_carrier = voice.isPad ? current_carrier * 0.70711f : current_carrier;
      float swept = base_carrier * powf(2.0f, voice.sweep_octave * progress);
      float vib = sinf(elapsed_sec * 0.5f * 6.2832f);
      swept *= (1.0f + vib * 0.0075f);
      sines[v].frequency(swept);
      float mod_freq = swept * voice.ratio;
      modulators[v].frequency(mod_freq);
      modulators[v].amplitude(voice.index * mod_freq / 440.0f);
    }
  }

  // ── Rain triggering (layer 3, Minnaert model) ──
  if (rain_vol > 0.0f) {
    for (int ch = 0; ch < 4; ch++) {
      if (random(10000UL) < (unsigned long)(rain_vol * rain_vol * 9000)) {
        int v = findFreeRainVoice(ch);
        if (v != -1) {
          float dropSize = powf(random(1000) / 1000.0f, 2.2f);
          float impactFreq = 9000.0f - dropSize * 7500.0f + random(-600, 601);
          impactFreq = max(1000.0f, min(11000.0f, impactFreq));
          float impactQ = 1.2f + (1.0f - dropSize) * 2.8f;
          float atk = 0.3f + random(5) / 10.0f;
          float dec = 3.0f + dropSize * 57.0f + random(-2, 7);
          if (dec < 3.0f) dec = 3.0f;
          rain_lpfs[v].frequency(impactFreq);
          rain_lpfs[v].resonance(impactQ);
          rain_envs[v].attack(atk);
          rain_envs[v].decay(dec);
          rain_envs[v].noteOn();
          rain_amps[v].gain((0.009f + random(16) / 1000.0f) * (rain_vol + 0.2f));
          rainVoices[v].active = true;
          rainVoices[v].triggerTime = now;
          rainVoices[v].env_dur = (unsigned long)(atk + dec + 5);
        }
      }
    }
  }

  // ── Kick drum (layer 5, 4-on-the-floor, ciesen-style) ──
  if (kick_vol > 0.0f) {
    // Set kick output volume based on layer blend
    kick_mix.gain(0, 0.40f * kick_vol);
    kick_mix.gain(1, 0.10f * kick_vol);
    kick_mix.gain(2, 0.35f * kick_vol);

    // 4-on-the-floor trigger
    if (now >= kickNextHit) {
      // Frequencies shifted up for 1" speakers (body 200→150, sub 120→90, click 350)
      kick_body.frequency(200.0f);
      kick_sub.frequency(120.0f);
      kick_click.frequency(350.0f);
      kick_body_env.noteOn();
      kick_sub_env.noteOn();
      kick_click_env.noteOn();
      kickHitTime = now;
      kickSweepPhase = 0;
      kickSweepTime = now;
      unsigned long beatMs = (unsigned long)(60000.0f / kickBPM);
      kickNextHit = now + beatMs;
    }

    // Pitch sweep: body 200->175->158->150, sub 120->105->95->90, 6ms per step
    if (kickSweepPhase >= 0) {
      unsigned long sweepElapsed = now - kickSweepTime;
      if (kickSweepPhase == 0 && sweepElapsed >= 6) {
        kick_body.frequency(175.0f);
        kick_sub.frequency(105.0f);
        kickSweepPhase = 1;
        kickSweepTime = now;
      } else if (kickSweepPhase == 1 && sweepElapsed >= 6) {
        kick_body.frequency(158.0f);
        kick_sub.frequency(95.0f);
        kickSweepPhase = 2;
        kickSweepTime = now;
      } else if (kickSweepPhase == 2 && sweepElapsed >= 6) {
        kick_body.frequency(150.0f);
        kick_sub.frequency(90.0f);
        kickSweepPhase = -1;
      }
    }
  } else {
    kick_mix.gain(0, 0.0f);
    kick_mix.gain(1, 0.0f);
    kick_mix.gain(2, 0.0f);
  }

  // ── Waves (layer 0, slow sine-modulated LPF) ──
  for (int ch = 0; ch < 4; ch++) {
    if (waves_vol > 0.0f) {
      NoiseState& n = waves_states[ch];
      float filterMax = 300.0f + (pitch_shift + 12.0f) / 24.0f * 1200.0f;
      unsigned long dms = now - n.lastUpdate;
      float ds = dms / 1000.0f;
      n.slight_phase += PI * 2 * n.slight_freq * ds;
      if (n.slight_phase > PI * 2) n.slight_phase -= PI * 2;
      n.lastUpdate = now;
      float cutoff = 150.0f + sinf(n.slight_phase) * 80.0f;

      if (now >= n.nextTrigger && !n.sweeping) {
        float sweepProb = min(waves_vol / 0.4f, 1.0f) * 0.25f;
        if (random(1000) / 1000.0f < sweepProb) {
          n.sweeping = true;
          n.sweepUp = true;
          n.sweepStart = now;
          if (random(100) < 15)
            n.maxCutoff = filterMax * (0.7f + random(300) / 1000.0f);
          else
            n.maxCutoff = filterMax * (0.15f + random(200) / 1000.0f);
          n.duration = 3000 + random(4000);
        }
        n.nextTrigger = now + random(6000, 22000);
      }

      if (n.sweeping) {
        unsigned long e = now - n.sweepStart;
        float p = (float)e / n.duration;
        if (n.sweepUp) {
          if (p < 1.0f) cutoff += (n.maxCutoff - 120.0f) * p;
          else { n.sweepUp = false; n.sweepStart = now; }
        } else {
          if (p < 1.0f) cutoff += (n.maxCutoff - 120.0f) * (1.0f - p);
          else n.sweeping = false;
        }
      }
      cutoff = max(50.0f, min(12000.0f, cutoff));
      lpfs[ch].frequency(cutoff);
      amp_ns[ch].gain(0.85f * waves_vol);
    } else {
      amp_ns[ch].gain(0.0f);
    }
  }

  // ── Thunder (linked with rain as Storm layer) ──────────────────────
  // Real thunder: initial crack ~200-500ms, then long rolling rumble 2-8s.
  // Filter stays low/mid (200-500 Hz) — no harsh highs on small speakers.
  // Softer attack than a snare, more like a distant boom that swells and rolls.
  for (int ch = 0; ch < 4; ch++) {
    if (thunder_vol > 0.0f) {
      NoiseState& n = thunder_states[ch];
      n.lastUpdate = now;

      float trigProb = min(thunder_vol / 0.25f, 1.0f) * 0.35f;

      if (now >= n.nextTrigger && !n.sweeping) {
        if (random(1000) / 1000.0f < trigProb) {
          n.sweeping = true;
          n.sweepStart = now;
          // Thunder duration: 2-7 seconds (real thunder rolls)
          n.duration = 2000 + random(5000);
          // Filter starts in low-mid range (initial boom)
          n.startCutoff = 280.0f + random(220);
          // Settles into low rumble
          n.maxCutoff = 100.0f + random(150);
          // Gain
          n.maxGain = 0.5f + random(300) / 1000.0f;
          n.nextTrigger = now + random(3000, 10000);
        } else {
          n.nextTrigger = now + 500;
        }
      }

      float gain = 0.0f;
      float cutoff = 120.0f;

      if (n.sweeping) {
        unsigned long e = now - n.sweepStart;
        float p = (float)e / n.duration;
        if (p >= 1.0f) {
          n.sweeping = false;
          gain = 0.0f;
          cutoff = 120.0f;
        } else {
          // Gain envelope: ~8% attack swell, then slow power-curve decay
          // with organic wobble throughout the tail
          if (p < 0.08f) {
            // Soft swell, not a hard hit
            float ramp = p / 0.08f;
            gain = ramp * ramp * n.maxGain;
          } else {
            // Long rolling decay with random gain wobble for realism
            float baseG = n.maxGain * powf(1.0f - (p - 0.08f) / 0.92f, 0.5f);
            float wobble = 0.7f + 0.3f * sinf(p * 12.0f + ch * 1.5f);
            gain = baseG * wobble;
          }
          // Filter: starts at boom freq, slowly sweeps down into rumble
          // Gradual sweep over first 30%, then stays in rumble zone
          if (p < 0.30f) {
            float sweep_p = p / 0.30f;
            cutoff = n.startCutoff - (n.startCutoff - n.maxCutoff) * sweep_p;
          } else {
            cutoff = n.maxCutoff + sinf(p * 8.0f) * 30.0f;
          }
          cutoff = max(80.0f, min(500.0f, cutoff));
          thunder_lpfs[ch].resonance(1.2f + random(80) / 100.0f);
        }
      }
      thunder_lpfs[ch].frequency(cutoff);
      thunder_amps[ch].gain(gain * thunder_vol);
    } else {
      thunder_amps[ch].gain(0.0f);
    }
  }

  // ── Hardware reads ──
  int es = digitalRead(encoderSWPin);
  if (es != prevEncSW) prevEncSW = es;
  for (int i = 0; i < 3; i++) {
    int pv = analogRead(potPins[i]);
    if (abs(pv - prevPotVals[i]) > 10) prevPotVals[i] = pv;
  }
  int jxr = analogRead(joystickXPin);
  if (abs(jxr - prevJoyX) > 10) prevJoyX = jxr;
  int jyr = analogRead(joystickYPin);
  if (abs(jyr - prevJoyY) > 10) prevJoyY = jyr;
  int jb = digitalRead(joystickButtonPin);
  if (jb != prevJoyButton) prevJoyButton = jb;

  // ── Arcade button: manual trigger ──
  int ab = digitalRead(arcadeButtonPin);
  if (ab != prevArcadeButton) {
    if (ab == LOW) {
      if (thunder_vol > 0.0f) {
        int ch = random(4);
        NoiseState& n = thunder_states[ch];
        n.sweeping = true;
        n.sweepStart = now;
        n.duration = 2000 + random(5000);
        n.startCutoff = 300.0f + random(200);
        n.maxCutoff = 100.0f + random(150);
        n.maxGain = 0.6f + random(250) / 1000.0f;
        n.nextTrigger = now + random(5000, 15000);
      } else if (bird_vol > 0.0f) {
        int t = random(4);
        birdStates[t].burstLeft = random(minBurst[t], maxBurst[t] + 1);
        birdStates[t].currentNote = bStartNote[t] + random(bNoteRange[t] + 1);
        birdStates[t].rel = 0;
        birdStates[t].nextNote = now;
      } else if (sine_vol > 0.0f || pluck_vol > 0.0f) {
        int numNotes = 8 + random(5);
        const int chordDeg[4] = {0, 2, 4, 6};
        for (int cn = 0; cn < numNotes; cn++) {
          int v = findFreeVoiceAny();
          if (v < 0) break;
          int oct = 3 + random(3);
          int cNote = oct * 8 + chordDeg[random(4)];
          if (cNote > 47) cNote = 47;
          float carrier = cMajorScale[cNote] * powf(2.0f, pitch_shift / 12.0f);
          sines[v].frequency(carrier);
          sines[v].amplitude(0.176f);
          modulators[v].frequency(carrier);
          modulators[v].amplitude(0.0f);
          envs[v].attack(1200);
          envs[v].decay(3500);
          envs[v].noteOn();
          amps[v].gain(0.22f * max(sine_vol, pluck_vol));
          sineVoices[v].active = true;
          sineVoices[v].note = cNote;
          sineVoices[v].ratio = 1;
          sineVoices[v].index = 0.0f;
          sineVoices[v].triggerTime = now;
          sineVoices[v].env_dur = 8000;
          sineVoices[v].isBird = false;
          sineVoices[v].isPad = true;
          sineVoices[v].sweep_octave = 0.0f;
        }
      }
    }
    prevArcadeButton = ab;
  }

  // ── LEDs ──
  static unsigned long lastNeo = 0;
  float period = 20000.0f;
  float t = fmod(now, 2 * period) / period;
  float prog;
  if (t <= 1.0f) prog = t;
  else prog = 2.0f - t;
  uint16_t hue = uint16_t(120 + 180 * prog) * (65536 / 360);
  float lm1 = sinf(now * 0.0005f) * 10 * (65536 / 360);
  float lm2 = sinf(now * 0.0005f + PI / 2) * 10 * (65536 / 360);
  pixels.setPixelColor(0, pixels.ColorHSV(hue + int(lm1), 255, 255));
  pixels.setPixelColor(1, pixels.ColorHSV(hue + int(lm2), 255, 255));
  if (ab == LOW) {
    pixels.setPixelColor(0, 255, 255, 255);
    pixels.setPixelColor(1, 255, 255, 255);
  }
  if (now - lastNeo >= 50 || ab != prevArcadeButton) {
    lastNeo = now;
    pixels.show();

    // 25-pixel strip: color follows preset blend, kick pulses brightness
    static float kickFlash = 0.0f;
    if (kick_vol > 0.0f && kickSweepPhase >= 0) kickFlash = 1.0f;
    else kickFlash *= 0.88f;
    float baseBright = 0.15f + kickFlash * 0.85f;
    for (int i = 0; i < NEOPIXEL_STRIP_COUNT; i++) {
      float pos = (float)i / (NEOPIXEL_STRIP_COUNT - 1);
      float wave = sinf(pos * PI * 4.0f + now * 0.002f) * 0.5f + 0.5f;
      uint16_t stripHue = hue + (uint16_t)(pos * 10000);
      uint8_t bri = (uint8_t)(255.0f * baseBright * (0.5f + 0.5f * wave));
      strip.setPixelColor(i, strip.ColorHSV(stripHue, 255, bri));
    }
    strip.show();
  }
  delay(1);
}
