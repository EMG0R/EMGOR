// neuralGrid — grid_test
// Pico (RP2040) driving 4x NeoTrellis 4x4 tiled into an 8x8 grid.
// Behavior: a reactive lava-lamp — three small pastel blobs (pink / blue /
// yellow) ooze around the 8x8 field. Holding a pad MAGNETISES the blobs toward
// it; releasing shoves them away. A held pad itself glows warm max-bright white.
//
// Board: "Raspberry Pi Pico" (arduino:mbed_rp2040:pico)
// Library: "Adafruit seesaw Library" (includes Adafruit_NeoTrellis / MultiTrellis)
// Default I2C: Wire = I2C0, SDA = GP4, SCL = GP5
//
// See README.md in this folder for physical tiling, addresses, power, and level-shifting.

#include "Adafruit_NeoTrellis.h"
#include <math.h>

#define X_DIM 8   // columns (2 boards wide)
#define Y_DIM 8   // rows    (2 boards tall)

// 2x2 array of boards, each with its unique I2C address.
// Layout matches physical placement:  [row][col]
//   0x2E (TL)  0x2F (TR)
//   0x30 (BL)  0x31 (BR)
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {
  { Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) },
  { Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x31) }
};

Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

// Which pads are currently held down.
bool held[Y_DIM][X_DIM] = { { false } };

// Set by a key edge so loop() renders that frame IMMEDIATELY instead of waiting
// for the next ~20ms tick — makes press/release lighting feel instant.
bool dirty = false;

// Global driver brightness: full, so a HELD pad reaches true max brightness.
// The idle lava-lamp is scaled WAY down in software (IDLE_DIV) so it stays a
// soft glow while a pressed pad blazes at full white.
const uint8_t BRIGHTNESS = 255;  // held pads hit real max
const float   IDLE_DIV   = 19.0; // idle glow level (dim; press to see full)

// Held-pad colour: warm near-white (slightly yellow/orange), max bright.
const uint8_t HELD_R = 255, HELD_G = 236, HELD_B = 205;

// Lava-lamp palette (full-scale; scaled by IDLE_DIV before display).
// Pastel / washed — pale but still clearly hued.
struct RGB { float r, g, b; };
const RGB PAL[3] = {
  { 255,  42, 128 },   // pink  — pushed toward vivid
  {  16,  86, 255 },   // blue  — pushed toward vivid
  { 255, 204,  20 },   // yellow — pushed toward vivid
};

// ── Blob physics ──────────────────────────────────────────────────────────
// Each blob has a position + velocity in grid space (0..7). Forces: a weak
// wandering "home" spring keeps it alive; held pads attract; recent releases
// repel. Damping makes it ooze.
float bx[3], by[3], vx[3], vy[3];

const float K_HOME    = 0.5f;    // weak pull toward the wandering home target
const float K_ATTRACT = 4.0f;    // pull toward a held pad (magnet ON)
const float K_REPEL   = 9.0f;    // push away from a just-released pad (magnet OFF)
const float DAMP      = 0.86f;   // velocity retention per frame (oozy)
const float REL_MS    = 850.0f;  // how long a release keeps pushing

// Ring buffer of recent release points (magnet-off shoves).
#define NREL 8
float    relx[NREL], rely[NREL];
uint32_t relt[NREL] = { 0 };   // millis() of release; 0 = empty slot
int      relhead = 0;

// Blob size: smaller = tighter. d^2 is multiplied by SIZE_INV, so 4.0 halves
// the radius vs the plain field (blobs ~2x smaller).
const float SIZE_INV = 4.0f;
const float SOFT     = 1.6f;    // core softness / glow

// ── Click ripples ──────────────────────────────────────────────────────────
// Each press spawns an expanding ring in a palette colour, radiating outward.
// Bright (near full palette level) so it pops against the dim idle.
#define NRIP 10
float    ripx[NRIP], ripy[NRIP];
uint32_t ript[NRIP] = { 0 };   // spawn millis(); 0 = empty slot
uint8_t  ripc[NRIP];           // palette colour index for this ripple
int      riphead = 0;
uint8_t  ripCycle = 0;         // cycles pink->blue->yellow per press

const float RIP_SPEED = 7.0f;    // cells/sec the ring expands
const float RIP_WIDTH = 1.3f;    // ring thickness (cells)
const float RIP_LIFE  = 750.0f;  // ms before it fades out

// Blended, IDLE-scaled colour at grid cell (x,y), packed 0x00RRGGBB.
uint32_t lava(float x, float y) {
  float w[3], ws = 0.0f;
  for (int i = 0; i < 3; i++) {
    float dx = x - bx[i], dy = y - by[i];
    float d2 = (dx * dx + dy * dy) * SIZE_INV + SOFT;
    float wi = 1.0f / d2;
    wi = wi * wi;              // sharpen: nearest blob dominates (kills grey mud)
    w[i] = wi;  ws += wi;
  }
  float r = (PAL[0].r * w[0] + PAL[1].r * w[1] + PAL[2].r * w[2]) / ws / IDLE_DIV;
  float g = (PAL[0].g * w[0] + PAL[1].g * w[1] + PAL[2].g * w[2]) / ws / IDLE_DIV;
  float b = (PAL[0].b * w[0] + PAL[1].b * w[1] + PAL[2].b * w[2]) / ws / IDLE_DIV;
  return ((uint32_t)(r + 0.5f) << 16) | ((uint32_t)(g + 0.5f) << 8) | (uint32_t)(b + 0.5f);
}

// Add every active click-ripple's expanding ring onto a base pixel colour.
uint32_t addRipples(uint32_t base, float x, float y, uint32_t now) {
  float r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
  for (int i = 0; i < NRIP; i++) {
    if (ript[i] == 0) continue;
    float age = (float)(now - ript[i]);
    if (age >= RIP_LIFE) { ript[i] = 0; continue; }
    float radius = RIP_SPEED * (age / 1000.0f);
    float dx = x - ripx[i], dy = y - ripy[i];
    float ring = fabsf(sqrtf(dx * dx + dy * dy) - radius);
    if (ring < RIP_WIDTH) {
      float inten = (1.0f - ring / RIP_WIDTH) * (1.0f - age / RIP_LIFE);
      const RGB &c = PAL[ripc[i]];
      r += c.r * inten;  g += c.g * inten;  b += c.b * inten;   // near full-bright ring
    }
  }
  if (r > 255) r = 255;  if (g > 255) g = 255;  if (b > 255) b = 255;
  return ((uint32_t)(r + 0.5f) << 16) | ((uint32_t)(g + 0.5f) << 8) | (uint32_t)(b + 0.5f);
}

// Step the blob sim. dt in seconds; now = millis().
void updateBlobs(float dt, uint32_t now) {
  float tt = now / 1000.0f;
  for (int i = 0; i < 3; i++) {
    float fx = 0, fy = 0;

    // Wandering home target so the lamp keeps oozing when nothing is pressed.
    float hx = 3.5f + 2.5f * sinf(tt * (0.35f + 0.07f * i) + i * 2.1f);
    float hy = 3.5f + 2.5f * cosf(tt * (0.30f + 0.06f * i) + i * 1.7f);
    fx += (hx - bx[i]) * K_HOME;
    fy += (hy - by[i]) * K_HOME;

    // Attraction to every currently-held pad (magnet ON).
    for (int y = 0; y < Y_DIM; y++)
      for (int x = 0; x < X_DIM; x++)
        if (held[y][x]) {
          fx += (x - bx[i]) * K_ATTRACT;
          fy += (y - by[i]) * K_ATTRACT;
        }

    // Repulsion from recent releases (magnet OFF), decaying over REL_MS.
    for (int r = 0; r < NREL; r++) {
      if (relt[r] == 0) continue;
      float age = (float)(now - relt[r]);
      if (age >= REL_MS) { relt[r] = 0; continue; }
      float s = 1.0f - age / REL_MS;                 // decay 1 -> 0
      float dx = bx[i] - relx[r], dy = by[i] - rely[r];
      float d = sqrtf(dx * dx + dy * dy) + 0.4f;
      fx += (dx / d) * K_REPEL * s;
      fy += (dy / d) * K_REPEL * s;
    }

    // Integrate with damping.
    vx[i] = vx[i] * DAMP + fx * dt;
    vy[i] = vy[i] * DAMP + fy * dt;
    bx[i] += vx[i] * dt;
    by[i] += vy[i] * dt;

    // Soft-bounce off the edges so blobs stay on the grid.
    if (bx[i] < 0) { bx[i] = 0; vx[i] = -vx[i] * 0.5f; }
    if (bx[i] > 7) { bx[i] = 7; vx[i] = -vx[i] * 0.5f; }
    if (by[i] < 0) { by[i] = 0; vy[i] = -vy[i] * 0.5f; }
    if (by[i] > 7) { by[i] = 7; vy[i] = -vy[i] * 0.5f; }
  }
}

// Key event callback. Runs synchronously inside trellis.read() (loop context),
// so touching held[]/the release buffer here is race-free.
TrellisCallback onKey(keyEvent evt) {
  uint8_t x = evt.bit.NUM % X_DIM;
  uint8_t y = evt.bit.NUM / X_DIM;
  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    held[y][x] = true;
    // Spawn an expanding ripple from this pad, cycling palette colours.
    ripx[riphead] = x;  ripy[riphead] = y;  ript[riphead] = millis();
    ripc[riphead] = ripCycle;  ripCycle = (ripCycle + 1) % 3;
    riphead = (riphead + 1) % NRIP;
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
    held[y][x] = false;
    // Record a magnet-off shove from this pad.
    relx[relhead] = x;  rely[relhead] = y;  relt[relhead] = millis();
    relhead = (relhead + 1) % NREL;
  }
  dirty = true;   // render this frame now, don't wait for the tick
  return 0;
}

void setup() {
  Serial.begin(115200);
  Wire.setClock(800000);   // 800kHz — halves the 64-pad show() time for snappier response; drop to 400000 if the bus is flaky

  if (!trellis.begin()) {
    Serial.println("NeoTrellis not found — check addresses / wiring / power.");
    while (1) delay(10);
  }

  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(BRIGHTNESS);

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, onKey);
    }

  // Spread the three blobs out to start.
  bx[0] = 2; by[0] = 2;  bx[1] = 6; by[1] = 2;  bx[2] = 4; by[2] = 6;
}

void loop() {
  trellis.read();   // fires onKey() for any new press/release

  static uint32_t last = 0;
  uint32_t now = millis();
  // Render on the ~50fps tick OR immediately when a key edge set dirty.
  if (now - last < 20 && !dirty) return;
  dirty = false;
  float dt = (now - last) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;      // clamp after a long stall
  last = now;

  updateBlobs(dt, now);

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      uint32_t color;
      if (held[y][x]) {
        color = ((uint32_t)HELD_R << 16) | ((uint32_t)HELD_G << 8) | HELD_B;  // warm max-bright white
      } else {
        color = addRipples(lava((float)x, (float)y), (float)x, (float)y, now);
      }
      trellis.setPixelColor(x, y, color);
    }
  trellis.show();
}
