# Blynk dashboard handover

Verified on 25 September 2026 using the ESP32 simulation and the live Blynk device. The web dashboard has 19 datastreams and 24 widgets.

- [Open Smart Room ESP32](https://blynk.cloud/dashboard/1159205/global/devices/439359/organization/1159205/devices/4241147/dashboard). Sign in to an account with access to the existing organization.
- [Source branch: blynk-update](https://github.com/NagsaiRambhatla/Automation_And_IoT/tree/blynk-update).
- [Build and simulation instructions](README.md#run-the-updated-simulation).
- [Test evidence and limits](test-evidence/TEST_RESULTS.md).

## Requested features

| Teammate requirement | Delivered virtual pin / widget | Behaviour |
| --- | --- | --- |
| ServoPin: user opens/closes curtains or window | V12 Window Command, OPEN/CLOSED switch | Firmware moves GPIO12 servo to 0°/100°. V7 reports the effective commanded position. Smoke forces OPEN until clear. |
| RelayPin: AC On/Off | V11 Relay On, indicator | Reflects the GPIO14 relay output. Climate logic controls it; no manual AC override was requested. |
| TemperaturePin | V0 Measured Temperature | Gauge and historical line chart, in °C. |
| TargetTempPin | V9 Target Temperature, number input | User sets 16–40 °C, with 0.5 °C buttons. V18 Applied Target confirms the firmware accepted it. |
| LightPin: lights On/Off | V5 Light On, indicator | Reflects GPIO16 room lighting output. |
| HumidityPin | V1 Humidity | Gauge and separate historical line chart, in %. |
| Five status pins, with Checking/Fault/Working | V13 Climate, V14 Lighting, V15 Occupancy, V16 Safety, V17 Master | All five are provided as separate text labels, matching the five physical status pixels. |

Virtual pins are cloud channels, not ESP32 GPIO numbers. Keep this mapping when integrating teammates' firmware.

## Five-minute handover check

1. Follow the README to build this branch and upload its local merged firmware into Wokwi. The public Wokwi editor still has the original source; ordinary Play does not establish that this updated version is running.
2. Check serial output for `Blynk HTTPS upload: OK (HTTP 200)`, then compare temperature and humidity with the dashboard. Keep Wokwi visible so browser throttling does not slow the simulation.
3. Clear smoke below raw ADC 2000. Toggle the window OPEN and CLOSED; check the servo and Window Open indicator. Allow five seconds plus network/simulation delay for each command.
4. Enter a different target. Wait for Applied Target to match and observe the simulated temperature and relay response. The model moves gradually; the target setting does not change the measured DHT value.
5. Raise smoke above ADC 2000 with the switch CLOSED. Safety/Master show Fault and the window opens. Clearing smoke restores the latest switch choice and Working status.
6. Change temperature and humidity, allow completed one-minute averaging intervals, then reselect 1h to refresh both history charts. Stop the simulator after demonstrating.

## Status and operating limits

- The status words are Checking, Fault and Working. Master reflects both cloud directions and local faults. Climate validates DHT readings; Safety reports the smoke threshold. Lighting and Occupancy report running local processing, not independently diagnosed sensor health.
- Window position is commanded, without a position sensor. Climate is a software temperature model driving a simulated relay, not demonstrated physical HVAC.
- V9 and V12 persist in Blynk and are read after restart. Other readings expire after one minute without updates. Network failure retains the last valid local settings; reconnection/fault-injection timing still needs a dedicated resilience test.
- Keep `secrets.h` and compiled firmware private. They are excluded from Git. This handover does not change account access or publish a device token.
- This is the web dashboard. A separate mobile layout has not been created.

## Wider pitch work still remaining

The dashboard request is delivered. The broader pitch still requires a buzzer alarm, occupancy/darkness-driven curtain coordination, reduced climate use when empty, broader fault detection/isolation and test evidence. The final report, demonstration video and individual interview preparation are separate submission work.
