# Smart Room dashboard

Updated on 4 October 2026, Australia/Brisbane. The saved Blynk dashboard has 20 widgets arranged as climate on the left, safety and windows in the center, lighting on the right, and two standalone history charts below. This describes branch `04/10`, including the dashboard telemetry and climate fixes committed in f43d4d3 and the subsequent 3,800 ppm smoke limit.

## Display and control mapping

| Item | Existing virtual pin | Behavior |
| --- | --- | --- |
| Simulated temperature gauge and history | V8 | Software room-temperature model, degrees Celsius |
| Humidity gauge and one history chart | V1 | DHT humidity, percentage |
| Target temperature below the model gauge | V9 | Minus and plus in 0.5 degree steps; range 16–40 degrees Celsius |
| Air conditioning | V6 | On when the relay is HIGH; Standby when LOW |
| AC on indicator | V11 | Actual GPIO14 relay output, 0/1 |
| Smoke text | V19 | High at estimated gas concentration 3,800 ppm or above; otherwise Low |
| Windows / curtains manual switch | V12 | 0 CLOSED; 1 OPEN |
| Windows open indicator | V7 | Effective commanded state, including smoke override |
| Light control | V21 | 0 Off; 1 On; 2 Auto |
| Light on indicator | V5 | Actual GPIO16 output; yellow #FADB14 when 1, transparent with a light border when 0 |
| Light status text | V20 | On or Off from actual output |
| Room occupied indicator | V4 | Retained occupancy, 0/1 |
| Climate, lighting, occupancy, safety and system health | V13–V17 | Simplified Working/Fault text |

Measured temperature V0 remains sensor telemetry for the controller, but no dashboard widget or chart displays it. V18 remains the applied-target acknowledgement in telemetry; the number control displays the requested V9 target. V10 room classification is not implemented. There is no smoke, raw-light or measured-temperature history chart in the saved dashboard. The only two chart series are simulated temperature V8 and humidity V1, using one-minute averages.

Smoke detection now converts GPIO35's raw ADC reading to estimated ppm using the measured Wokwi calibration in `smoke_ppm.h`. Both window override and dashboard smoke/safety/system status use the same inclusive 3,800 ppm comparison. Serial output shows estimated ppm and the raw reading. V3 remains raw ADC (integer, 0–4095) to match its existing cloud datastream; it is not a ppm value. See `SMOKE_PPM_VERIFICATION.md` for calibration and the new safety checks.

All physical GPIO assignments and existing control virtual pins are preserved: DHT15, PIR27, LDR34, smoke35, servo12, relay14, room light16 and status pixels2. Comparing PIN definitions against the committed sketch produced no differences.

The explanation directly below the manual window switch reads:

> If smoke is detected, this manual setting is overridden and the windows and curtains open automatically. Your selected setting is restored when the smoke clears.

The same servo represents windows and curtains; it commands 0 degrees for open and 100 degrees for closed. There is no physical position-feedback sensor. Smoke does not overwrite the saved V12 request. Off and On take priority over motion and darkness; Auto requires retained occupancy and light ADC at least 2000. Occupancy tracking continues in all three modes, with a configured ten-second timeout.

The firmware now serializes V6, V13–V17, V19 and V20 alongside its numeric telemetry. An accepted changed target resets climate mode to standby and switches the relay off before reassessment. A non-finite temperature also switches the relay off and clears model initialization. Cloud health labels are simplified: V14/V15 are fixed Working, and system health reflects climate validity and smoke rather than network success. Physical status pixels still use the existing boot health behavior.

## Build and run

Use `./build.ps1` with the existing Arduino CLI and ESP32 dependencies. Optional `-ArduinoCli` and `-ConfigFile` arguments select the toolchain. The script prints the firmware path, and `wokwi.toml` points to it.

The latest ESP32 build passed: 1,073,880 bytes flash and 50,584 bytes global RAM. The staged sketch and source have identical SHA-256:

`c97a3b23950b6c77534c9ab3d9b3e27f9dedf2c16784506db2d04013fb34102f`

Built merged firmware SHA-256 (full build; smoke behavior tested in an isolated browser harness):

`e48f3f94b0022f688064c1915a8993c9ea048635c79f4c9c51f02d4f858f37cf`

In the browser Wokwi project, focus the editor, press F1, choose **Upload Firmware and Start Simulation**, and select `build/main-dashboard/sketch/build/esp32.esp32.esp32/sketch.ino.merged.bin`. That is the workflow used for the checks below. Ordinary Play compiles the browser editor's source and does not automatically use this local firmware. No public project source was published during this verification.

## Earlier dashboard acceptance checks (f43d4d3, raw ADC alarm)

The earlier dashboard build was loaded into Wokwi and connected to the existing Smart Room ESP32 Blynk device. The observations below predate the ppm conversion and do not constitute a complete dashboard retest of the new firmware.

| Requirement | Observed evidence | Result |
| --- | --- | --- |
| Simulated temperature throughout dashboard | DHT stayed at 26.5 degrees while the V8 gauge rose through 27.5, 31.5 and 32.5; V8 history plotted the changing model | Passed |
| Plus/minus directly below temperature | Both controls changed V9 by 0.5; firmware read 39.5 and then 39; further decrement/increment checks returned to 39 | Passed |
| Exactly one humidity chart and no smoke history | Saved device view contains exactly two charts: V8 model and V1 humidity; both populated | Passed |
| Lighting values 0/1/2 | Saved selector labels Off/On/Auto; firmware accepted OFF, ON and AUTO through V21 | Passed |
| Yellow On and blank Off indicator | V5 LED fill was rgb(250,219,20) when on, rgba(250,219,20,0) when off; V20 and serial output agreed | Passed |
| Auto lighting | Motion in the dark room, ADC 3938, produced occupied and light On; subsequent motion timeout produced light Off | Passed |
| Manual open and close | V12 OPEN reached firmware, effective windows became OPEN and V7 lit; CLOSED was subsequently accepted | Passed |
| Emergency override and recovery | CLOSED request remained visible while ADC 4041 forced OPEN, Smoke High and Safety/System Fault; ADC 843 restored CLOSED, Smoke Low and Working | Passed |
| Explanation beneath switch | Applied HTML explanation is directly below the manual switch and describes opening plus restoration | Passed |
| AC On and Standby | V6 displayed both states; V11 was lit for On and blank for Standby after a temporary 32 degree target | Passed |
| Spatial zones and charts | Saved device screenshot shows climate left, safety/windows center, lighting right, and both charts below | Passed |
| Preserve pins | Source PIN definitions unchanged from HEAD; V9, V12 and V21 controls retained | Passed |

Successful HTTP 200 telemetry responses were recorded throughout the run. Prior user settings were restored: target 39 degrees, lighting On and manual CLOSED. The smoke slider was restored to its prior displayed 2 ppm setting. The simulator and dashboard remain open for inspection.

Evidence is saved in `../.codex-review/dashboard-controls-final-20261004.txt` and the screenshots `dashboard-final-live.jpg`, `dashboard-light-off-live.jpg`, `dashboard-auto-on-live.jpg`, `dashboard-ac-standby-live.jpg` and `dashboard-smoke-override-live.jpg`.

Simulator speed depends on window visibility; focusing Chrome raised it from about 3% toward real-time speed. HTTP requests remain synchronous and can delay local decisions and telemetry. One-minute telemetry expiry can show blanks if the simulator is stopped or heavily throttled. These checks verify the requested dashboard and simulated controls, not physical HVAC regulation, sensor-failure isolation, network-outage recovery or hardware performance.
