# Blynk integration test record

Date: 25 September 2026 (Australia/Brisbane). Tests used Wokwi's simulated ESP32 and the actual Blynk device **Smart Room ESP32**. No fabricated readings were inserted directly into the cloud.

## Dashboard handover update

The applied dashboard now has **24 widgets and 19 datastreams**. It includes the requested window switch, adjustable target, relay/light displays, separate temperature/humidity line charts and all five system-status labels. The earlier monitoring-only results below remain a historical baseline.

| Check | Observed evidence | Result |
| --- | --- | --- |
| Updated firmware compile | ESP32 core 3.3.12 and ArduinoJson 7.4.2; 1,072,580 bytes flash (81%), 50,160 bytes global RAM (15%); exit 0 | Passed |
| Updated HTTPS communication | Saved control-run serial contains 30 successful HTTP 200 uploads; live device showed Online | Passed |
| Manual window opening | V12 OPEN reached firmware as `window request=OPEN`; servo command became 0 degrees while smoke ADC was 843; room assessment remained Room empty | Passed |
| Manual window closing | V12 CLOSED reached firmware; servo command became 100 degrees and Window Open feedback cleared | Passed |
| Target changes affect control | Earlier 38 to 24.5 command changed climate direction; saved run then changed 24.5 to 20. Firmware acknowledged V9, V18 showed 20, model cooled gradually to 20 and relay became OFF | Passed |
| Settings after restart | Restarted uploaded firmware fetched the saved 24.5 target and CLOSED request without re-entering them | Passed |
| Smoke priority | With CLOSED requested, smoke ADC 3628 forced OPEN and Safety/Master Fault. Clearing smoke to ADC 843 restored CLOSED and Working | Passed |
| Five independent status channels | V13–V17 are connected to Climate, Lighting, Occupancy, Safety and Master labels; all five displayed Working after smoke cleared | Passed |
| Sensor values and requested history | DHT changed from 42.6 °C / 40% to 20 °C / 60%; gauges matched and both temperature and humidity line charts visibly plotted the changes | Passed |
| Separate commands and feedback | V9/V12 stay as user settings; V18/V7 show applied target/effective window command. Source never writes telemetry over V9/V12 | Passed |

Current evidence:

- `dashboard-live.png`: actual sensor values, simulated/applied temperatures, relay/window indicators and temperature history. At capture the window was OPEN, climate Standby and relay OFF.
- `dashboard-controls.png`: CLOSED control, 20 °C target, five Working labels and populated humidity history.
- `controls-serial.txt`: 2,445 lines from the updated firmware run, including persisted settings, manual open/close, target change, smoke override/recovery, relay ON/OFF and cloud responses.
- Tested merged firmware SHA-256: `D6861F4004F2494AE3E62E3DA0ADCF64D894CCD2DD5AC4EC733960EAC91BD12B`. Subsequent source edits only corrected comments; executable logic was unchanged.

Checking/Fault/Working are supported as words on every status datastream. Working and smoke-related Fault were observed in this run. Checking initialization and the master communication-health path were reviewed in source; an isolated cloud Checking transition was not captured. Lighting/occupancy status does not diagnose a failed physical sensor. Hardware, DHT fault injection, network-loss/reconnection timing and offline expiry timing remain unverified.

## Earlier monitoring-only baseline

| Check | Observed evidence | Result |
| --- | --- | --- |
| Compile final firmware | Arduino ESP32 3.3.12; 1,063,440 bytes flash (81%), 50,160 bytes global RAM (15%); exit 0 | Passed |
| Authenticated HTTPS | Wokwi serial output repeatedly reported `Blynk HTTPS upload: OK (HTTP 200)`; Blynk changed to Online | Passed |
| Sensor-to-cloud mapping | Initial temperature 42.6 °C and humidity 40%; changed DHT to 20 °C and 60%, and the Blynk gauges matched | Passed |
| Climate model | Initial cooling from 42.1 toward target 38; after lowering ambient, heating/standby appeared. Reboot at 20 °C started simulated heating at 20.5 °C | Passed in simulation |
| Smoke rule | ADC 3628 produced Smoke alert and window-open command; reducing gas to 0.1 ppm produced ADC 843, window-closed command and Room empty | Passed in simulation |
| Dark occupied room | LDR at 0.1 lux produced ADC 4063; PIR motion turned room light on and produced Occupied and dark locally and in Blynk | Passed |
| Occupancy timeout | After PIR HIGH ended, occupancy persisted for the configured interval, then light turned off and Room empty reached Blynk | Passed |
| Bright occupied room | LDR at 100,000 lux produced ADC 32; motion produced Occupied and bright with room light off, locally and in Blynk | Passed |
| Prompt state changes | Final firmware uploaded occupancy and clearing transitions between periodic 30-second batches | Passed |
| Historical charts | Four individual charts visibly plotted measured temperature, smoke, light and simulated temperature from the test session | Passed |
| Credential exclusions | `secrets.h`, its staged build copy and the merged firmware are ignored by Git; candidate source files were checked for the exact token without printing it | Passed |

This earlier dashboard had 15 widgets and 11 datastreams. During verification, combined charts only drew the first stream despite stored history existing for the other streams. Four individual charts made each trend visible. At that stage the target value was a label; the handover update above adds editing and an applied-value acknowledgement.

## Earlier evidence

- `dashboard.png`: actual device dashboard with live readings and four populated history plots; header cropped to avoid account/token details.
- `wokwi-serial.txt`: serial output from the final compiled-firmware run, including HTTPS successes, occupancy states and actuator commands.
- Earlier merged firmware SHA-256: `B309B4869A0207E2989427871B0E3EFBA57726C34F38EBA26E27EEF7C1AC8F36`.

## Limits

Physical hardware and relay loads were not tested. Relay output plus a software temperature model does not establish real HVAC control. DHT invalid-reading recovery and loss-of-network handling were reviewed in source but were not fault-injected during this session. Cloud history is one-minute averages, not a record of every brief event. The current master status reflects uploads, command reads and local faults; dashboard expiry and device-offline timing are configured but were not independently timed. The web dashboard is configured; the separate mobile dashboard is not.

The initial measured-temperature/cooling test occurred before the final event-upload improvement. The final firmware was compiled and rerun, and occupancy, smoke clearing, bright-room classification, secure uploads and historical data were checked on that version. The public Wokwi project's editor still contains the original source; see `README.md` for building this branch and loading the private local firmware.
