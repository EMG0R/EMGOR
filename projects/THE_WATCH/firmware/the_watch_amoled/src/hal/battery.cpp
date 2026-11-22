#include "battery.h"
#include "../config.h"

#include <Arduino.h>

// Divider ratio: the ADC pin sees VBAT through a 200K (top) / 100K (bottom)
// divider, i.e. VBAT/3. Multiply the calibrated pin voltage by 3 to recover the
// true LiPo voltage. (Waveshare ESP32-S3-Touch-LCD-1.69 wiki.)
#define BAT_DIVIDER_RATIO 3.0f

// LiPo discharge curve: open-ish-circuit voltage (mV) -> % remaining. Coarse but
// good enough for a glanceable gauge; interpolated linearly between points.
struct VP { int mv; int pct; };
static const VP kCurve[] = {
  {4200, 100}, {4100, 90}, {4000, 80}, {3900, 70}, {3800, 60},
  {3700, 45},  {3600, 25}, {3500, 12}, {3400, 5},  {3300, 0},
};

static int mv_to_pct(int mv) {
  if (mv >= kCurve[0].mv) return 100;
  const int n = sizeof(kCurve) / sizeof(kCurve[0]);
  if (mv <= kCurve[n - 1].mv) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (mv <= kCurve[i].mv && mv >= kCurve[i + 1].mv) {
      // linear interpolate between kCurve[i] (higher) and kCurve[i+1] (lower)
      int dv = kCurve[i].mv - kCurve[i + 1].mv;
      int dp = kCurve[i].pct - kCurve[i + 1].pct;
      return kCurve[i + 1].pct + (mv - kCurve[i + 1].mv) * dp / dv;
    }
  }
  return 0;
}

void battery_init() {
  // 12-bit ADC, 12 dB attenuation -> ~0..3.1 V full scale at the pin. VBAT/3 of
  // a 4.2 V cell is ~1.4 V, comfortably inside range.
  analogReadResolution(12);
  analogSetPinAttenuation(WATCH_BAT_ADC_PIN, ADC_11db);
  Serial.printf("BATT: init on GPIO%d, divider x%.1f (VERIFY-ON-HARDWARE)\n",
                WATCH_BAT_ADC_PIN, BAT_DIVIDER_RATIO);
}

int battery_read(int *out_mv) {
  // Average several calibrated samples to kill ADC jitter.
  const int N = 16;
  uint32_t acc = 0;
  for (int i = 0; i < N; i++) {
    acc += analogReadMilliVolts(WATCH_BAT_ADC_PIN);  // calibrated pin mV
    delayMicroseconds(200);
  }
  int pin_mv = (int)(acc / N);
  int vbat_mv = (int)(pin_mv * BAT_DIVIDER_RATIO + 0.5f);
  if (out_mv) *out_mv = vbat_mv;
  // Guard against an unplugged/implausible reading.
  if (vbat_mv < 2500 || vbat_mv > 4400) return -1;
  return mv_to_pct(vbat_mv);
}
