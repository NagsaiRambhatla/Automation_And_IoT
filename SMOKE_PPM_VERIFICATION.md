# 3,800 ppm smoke detection

Verified 4 October 2026, Australia/Brisbane, on branch `04/10`.

The alarm condition is `smokePpm >= SMOKE_THRESHOLD_PPM`, where the limit is 3,800 ppm. `smokeAdcToPpm()` converts the raw reading before the comparison. Window override and the V16/V17/V19 dashboard status all use `smokeAlarmActive()`. V3 remains the existing raw ADC datastream, not ppm; serial output labels both units explicitly.

## Calibration

Wokwi's [MQ2 reference](https://docs.wokwi.com/parts/wokwi-gas-sensor) documents the ppm control and increasing analog voltage but supplies no conversion equation. The lookup anchors below were measured directly in the existing circuit, GPIO35, 12-bit resolution and ADC_11db attenuation. Interpolation uses log(ppm) between anchors; values beyond the sampled range are clamped. This is an estimate for this simulator configuration. A physical MQ2 requires its own gas-specific calibration.

| Simulator concentration (ppm) | ADC |
| ---: | ---: |
| 0.1 | 843 |
| 1.9953 | 1922 |
| 10 | 2585 |
| 100 | 3337 |
| 400 | 3628 |
| 1000 | 3762 |
| 1995.2623 | 3839 |
| 3630.7805 | 3892 |
| 3801.8940 | 3896 |
| 3981.0717 | 3899 |
| 10000 | 3959 |
| 100000 | 4041 |

The logarithmic slider's nearest tested setting is 3,801.894 ppm, displayed as 3,802 ppm. The 12-bit ADC cannot distinguish every single ppm: near the threshold, one ADC count represents roughly 40–60 ppm. The configured software threshold is exactly 3,800 ppm, while the first alarm-producing ADC count is 3896. The probe tested actual slider changes, not a relabelled raw ADC value.

## Results

An isolated browser harness used the same conversion and safety control logic, without Wi-Fi or cloud writes. It produced:

```text
BOUNDARY_AND_RANGE_PASS
CHECK ADC=3628 ppm=400.0 alarm=0 window=CLOSED
CHECK ADC=3892 ppm=3630.8 alarm=0 window=CLOSED
CHECK ADC=3896 ppm=3801.9 alarm=1 window=OPEN
CHECK ADC=3899 ppm=3981.1 alarm=1 window=OPEN
CHECK ADC=3892 ppm=3630.8 alarm=0 window=CLOSED
```

The boundary check asserted inactive at 3,799.9 ppm, active at exactly 3,800.0 ppm and active at 3,800.1 ppm. It also checked finite, nondecreasing conversion for every ADC input from 0 through 4095. With the manual request CLOSED, smoke forced the window OPEN and clearing smoke restored CLOSED. The existing servo commands remain 0 degrees OPEN and 100 degrees CLOSED; servo library readback is not physical position feedback.

The complete ESP32 firmware compiled successfully with core 3.3.12: 1,073,880 bytes flash and 50,584 bytes global RAM. Staged source/header hashes matched the repository files. The complete Blynk dashboard was not retested during this isolated smoke check; its earlier acceptance observations are retained separately in `DASHBOARD.md`.

Local evidence: `../.codex-review/smoke-ppm-probe.ino` and `../.codex-review/smoke-3800ppm-verified.jpg`. Temporary browser edits were discarded; no Wokwi project was published.
