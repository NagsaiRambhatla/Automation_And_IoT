#pragma once
#include <math.h>
#include <stddef.h>

constexpr float SMOKE_THRESHOLD_PPM = 3800.0f;

// Estimated ppm for Wokwi's MQ2 connected to GPIO35, with 12-bit ADC_11db.
// Measured on 4 October 2026; see SMOKE_PPM_VERIFICATION.md.
// These simulator anchors must be replaced by calibration for real hardware.
inline float smokeAdcToPpm(int adc) {
  struct CalibrationPoint { int adc; float ppm; };
  static const CalibrationPoint points[] = {
    {843, 0.1f}, {1922, 1.9952623f}, {2585, 10.0f}, {3337, 100.0f},
    {3628, 400.0f}, {3762, 1000.0f}, {3839, 1995.2623f},
    {3892, 3630.7805f}, {3896, 3801.8940f}, {3899, 3981.0717f},
    {3959, 10000.0f}, {4041, 100000.0f}
  };
  const size_t count = sizeof(points) / sizeof(points[0]);
  if (adc <= points[0].adc) return points[0].ppm;
  for (size_t i = 1; i < count; ++i) {
    if (adc == points[i].adc) return points[i].ppm;
    if (adc < points[i].adc) {
      const float fraction = float(adc - points[i - 1].adc) /
                             float(points[i].adc - points[i - 1].adc);
      // Interpolate in log(ppm): MQ2's response is not linear in ppm.
      return powf(10.0f, log10f(points[i - 1].ppm) + fraction *
                         (log10f(points[i].ppm) - log10f(points[i - 1].ppm)));
    }
  }
  // Saturation is at least the highest calibrated concentration, hence High.
  return points[count - 1].ppm;
}

inline bool smokeAlarmActive(float smokePpm) {
  return smokePpm >= SMOKE_THRESHOLD_PPM;
}
