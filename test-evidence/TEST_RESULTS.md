# Blynk integration test record

Date: 25 September 2026 (Australia/Brisbane). Tests used Wokwi's simulated ESP32 and the actual Blynk device **Smart Room ESP32**. No fabricated readings were inserted directly into the cloud.

## Results

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

The final dashboard has 15 widgets and 11 datastreams. During verification, combined charts only drew the first stream despite stored history existing for the other streams. The saved dashboard uses four individual charts to make each trend visible. The target value remains a label.

## Evidence

- `dashboard.png`: actual device dashboard with live readings and four populated history plots; header cropped to avoid account/token details.
- `wokwi-serial.txt`: serial output from the final compiled-firmware run, including HTTPS successes, occupancy states and actuator commands.
- Final merged firmware SHA-256: `B309B4869A0207E2989427871B0E3EFBA57726C34F38EBA26E27EEF7C1AC8F36`.

## Limits

Physical hardware and relay loads were not tested. Relay output plus a software temperature model does not establish real HVAC control. DHT invalid-reading recovery and loss-of-network handling were reviewed in source but were not fault-injected during this session. Cloud history is one-minute averages, not a record of every brief event. The master status reflects the latest upload outcome; dashboard expiry and device-offline timing are configured but were not independently timed. Web monitoring is configured; the separate mobile dashboard is not.

The initial measured-temperature/cooling test occurred before the final event-upload improvement. The final firmware was compiled and rerun, and occupancy, smoke clearing, bright-room classification, secure uploads and historical data were checked on that version. The public Wokwi project's editor still contains the original source; see `README.md` for building this branch and loading the private local firmware.
