#include "DHTesp.h"
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include "blynk_config.h"
#include "smoke_ppm.h"
 
// Pin Definitions
#define DHT_PIN    15 //Temperature and humidity sensor pin
#define PIR_PIN    27 //Motion sensor
#define LDR_PIN    34 //Light sensor
#define SMOKE_PIN  35 //Gas sensor
#define SERVO_PIN  12
#define RELAY_PIN  14 //Acting as AC
#define LED_STRIP_PIN 2 //Beginning of daisy chaining
#define NUM_PIXELS 5 // Defining number of Leds
#define BULB_PIN 16 //Light
// Status Leds order
#define CLIMATE_LED    0
#define LIGHTING_LED   1
#define OCCUPANCY_LED  2
#define SAFETY_LED     3
#define MASTER_LED     4
// Status Led colours
#define STATUS_GREEN   strip.Color(0, 255, 0)
#define STATUS_YELLOW  strip.Color(255, 255, 0)
#define STATUS_RED     strip.Color(255, 0, 0)
#define STATUS_OFF     strip.Color(0, 0, 0)
 
bool systemBoot = false;
// Curtain/Window Position
#define CURTAINS_OPEN   0
#define CURTAINS_CLOSED 100
bool windowOpen = false;
bool requestedWindowOpen = false;
// Blynk V21: 0 = Off, 1 = On, 2 = Auto.
enum LightMode {
  LIGHT_OFF = 0,
  LIGHT_ON = 1,
  LIGHT_AUTO = 2
};
LightMode requestedLightMode = LIGHT_OFF;
 
const int LIGHT_THRESHOLD = 2000;
 
// Lighting  config
unsigned long lastMotionTime = 0;
const unsigned long OCCUPANCY_TIMEOUT = 10000;
bool occupied = false;
volatile uint32_t motionEventCount = 0;
uint32_t processedMotionEventCount = 0;

enum ClimateMode {
  CLIMATE_STANDBY,
  CLIMATE_COOLING,
  CLIMATE_HEATING
};
 
ClimateMode climateMode = CLIMATE_STANDBY;
float temperatureTolerance = 1.0;
float targetTemperature = 38.00;
float simulatedTemperature;
bool temperatureInitialized = false;
bool acOn = false;
 
enum SystemStatus {
  STATUS_OK,
  STATUS_CHECKING,
  STATUS_FAULT
};
 
SystemStatus climateStatus   = STATUS_CHECKING;
SystemStatus lightingStatus  = STATUS_CHECKING;
SystemStatus occupancyStatus = STATUS_CHECKING;
SystemStatus safetyStatus    = STATUS_CHECKING;
 
Adafruit_NeoPixel strip(NUM_PIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);
 
DHTesp dht;
Servo servo;
 
// Preserve PIR pulses even while a Blynk HTTP request blocks the main loop.
void ARDUINO_ISR_ATTR onMotionDetected() {
  motionEventCount = motionEventCount + 1;
}

int readMotion() {
  const uint32_t eventCount = motionEventCount;
  const bool pendingMotion = eventCount != processedMotionEventCount;
  processedMotionEventCount = eventCount;
  return (digitalRead(PIR_PIN) == HIGH || pendingMotion) ? HIGH : LOW;
}

void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(SERVO_PIN, 500, 2400);
  strip.begin();
  strip.show();
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), onMotionDetected, RISING);
  pinMode(LDR_PIN, INPUT);
  pinMode(SMOKE_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SMOKE_PIN, ADC_11db);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BULB_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BULB_PIN, LOW);
  servo.write(CURTAINS_CLOSED);
  connectWiFi();
}
 
void uploadTelemetry(
  float temperature,
  float humidity,
  int lightLevel,
  int smokeAdc,
  float smokePpm
) {
 
  String values = "";
 
  values += "&V0=" + String(temperature, 2);
  values += "&V1=" + String(humidity, 2);
  values += "&V2=" + String(lightLevel);
  // V3's existing cloud datastream is raw ADC (integer, 0-4095).
  values += "&V3=" + String(smokeAdc);
  values += "&V4=" + String(occupied ? 1 : 0);
  values += "&V5=" + String(digitalRead(BULB_PIN) == HIGH ? 1 : 0);
  values += "&V6=" + String(digitalRead(RELAY_PIN) == HIGH ? "On" : "Standby");
  values += "&V7=" + String(windowOpen ? 1 : 0);
  values += "&V8=" + String(simulatedTemperature, 2);
  values += "&V11=" + String(digitalRead(RELAY_PIN) == HIGH ? 1 : 0);
  values += "&V18=" + String(targetTemperature, 2);

  // Use the existing readable dashboard streams; controls keep their own pins.
  const bool climateHealthy = isfinite(temperature) && isfinite(humidity);
  const bool smokeHigh = smokeAlarmActive(smokePpm);
  values += "&V13=" + String(climateHealthy ? "Working" : "Fault");
  values += "&V14=Working&V15=Working";
  values += "&V16=" + String(smokeHigh ? "Fault" : "Working");
  values += "&V17=" + String(!climateHealthy || smokeHigh ? "Fault" : "Working");
  values += "&V19=" + String(smokeHigh ? "High" : "Low");
  values += "&V20=" + String(digitalRead(BULB_PIN) == HIGH ? "On" : "Off");
 
  updateBlynk(values);
}
 
void getBlynkControls() {
 
  String targetResponse = getBlynkValue("V9");
 
  if (targetResponse != "") {
 
    float newTarget = targetResponse.toFloat();
 
    if (newTarget >= 16.0 && newTarget <= 40.0) {
 
      if (newTarget != targetTemperature) {
        targetTemperature = newTarget;
        // Reassess a changed target without jumping the temperature model.
        climateMode = CLIMATE_STANDBY;
        digitalWrite(RELAY_PIN, LOW);
      }
 
      Serial.print("Blynk Target Temperature: ");
      Serial.println(targetTemperature);
    }
  }
   // Window Command - V12
  String windowResponse = getBlynkValue("V12");
 
  if (windowResponse != "") {
 
    int windowCommand = windowResponse.toInt();
 
    if (windowCommand == 0 || windowCommand == 1) {
 
      requestedWindowOpen = (windowCommand == 1);
 
      Serial.print("Blynk Window Request: ");
      Serial.println(requestedWindowOpen ? "OPEN" : "CLOSED");
    }
  }
  // Light control - V21
  String lightResponse = getBlynkValue("V21");
  lightResponse.trim();
  if (lightResponse == "0" || lightResponse == "1" || lightResponse == "2") {
    requestedLightMode = static_cast<LightMode>(lightResponse.toInt());
    Serial.print("Blynk Light Mode: ");
    Serial.println(requestedLightMode == LIGHT_OFF ? "OFF" :
                   (requestedLightMode == LIGHT_ON ? "ON" : "AUTO"));
  }
  // Missing or invalid commands retain the last accepted mode.
}
 
// This function handles the assignment of colours to RGB LED Status Indicators
void setStatusLED(int pixel, SystemStatus status) {
 
  switch (status) {
    case STATUS_OK:
      strip.setPixelColor(pixel, strip.Color(0, 255, 0));
      break;
    case STATUS_CHECKING:
      strip.setPixelColor(pixel, strip.Color(255, 255, 0));
      break;
    case STATUS_FAULT:
      strip.setPixelColor(pixel, strip.Color(255, 0, 0));
      break;
  }
  strip.show();
}
 
void runHealthCheck(){
  Serial.println("-------System Health Check Active----------");
 
  TempAndHumidity data = dht.getTempAndHumidity();
  if(!isnan(data.temperature) && !isnan(data.humidity)){
    climateStatus = STATUS_OK;
    Serial.println("Climate System: OK");
    setStatusLED(MASTER_LED, STATUS_OK);
  }else{
    climateStatus = STATUS_FAULT;
    Serial.println("Climate System: Fault");
  }
  setStatusLED(CLIMATE_LED, climateStatus);
}
 
void systemBootUp(){
  setStatusLED(CLIMATE_LED, STATUS_CHECKING);
  setStatusLED(LIGHTING_LED, STATUS_CHECKING);
  setStatusLED(OCCUPANCY_LED, STATUS_CHECKING);
  setStatusLED(SAFETY_LED, STATUS_CHECKING);
  setStatusLED(MASTER_LED, STATUS_CHECKING);
  delay(1000);
  runHealthCheck();
  systemBoot = true;
 
}
 
void climateControl(float sensorTemperature) {
  if (!isfinite(sensorTemperature)) {
    digitalWrite(RELAY_PIN, LOW);
    climateMode = CLIMATE_STANDBY;
    temperatureInitialized = false;
    return;
  }
  // Initialise simulated room temperature once
  if (!temperatureInitialized) {
    simulatedTemperature = sensorTemperature;
    temperatureInitialized = true;
  }
 
  // If target changes while cooling/heating,
  // allow the system to stop and re-evaluate.
  if (climateMode == CLIMATE_COOLING &&
      simulatedTemperature <= targetTemperature) {
    simulatedTemperature = targetTemperature;
    climateMode = CLIMATE_STANDBY;
  }
  if (climateMode == CLIMATE_HEATING &&
      simulatedTemperature >= targetTemperature) {
    simulatedTemperature = targetTemperature;
    climateMode = CLIMATE_STANDBY;
  }
 
 
  // HVAC currently idle:
  // only start if temperature leaves tolerance range.
  if (climateMode == CLIMATE_STANDBY) {
    if (simulatedTemperature >= targetTemperature + temperatureTolerance) {
      climateMode = CLIMATE_COOLING;
    }
 
    else if (simulatedTemperature <= targetTemperature - temperatureTolerance) {
      climateMode = CLIMATE_HEATING;
    }
 
    else {
      digitalWrite(RELAY_PIN, LOW);
      Serial.print("AC: STANDBY | Temperature: ");
      Serial.print(simulatedTemperature);
      Serial.println(" C");
      return;
    }
  }
 
 
  // COOLING
  if (climateMode == CLIMATE_COOLING) {
    digitalWrite(RELAY_PIN, HIGH);
    simulatedTemperature -= 0.5;
    if (simulatedTemperature <= targetTemperature) {
      simulatedTemperature = targetTemperature;
      digitalWrite(RELAY_PIN, LOW);
      climateMode = CLIMATE_STANDBY;
      Serial.print("AC: STANDBY | Target reached: ");
      Serial.print(simulatedTemperature);
      Serial.println(" C");
 
      return;
    }
    Serial.print("AC: COOLING | Temperature: ");
    Serial.print(simulatedTemperature);
    Serial.println(" C");
  }
  // HEATING
  else if (climateMode == CLIMATE_HEATING) {
    digitalWrite(RELAY_PIN, HIGH);
    simulatedTemperature += 0.5;
    if (simulatedTemperature >= targetTemperature) {
      simulatedTemperature = targetTemperature;
      digitalWrite(RELAY_PIN, LOW);
      climateMode = CLIMATE_STANDBY;
      Serial.print("AC: STANDBY | Target reached: ");
      Serial.print(simulatedTemperature);
      Serial.println(" C");
      return;
    }
    Serial.print("AC: HEATING | Temperature: ");
    Serial.print(simulatedTemperature);
    Serial.println(" C");
  }
}
 
void ambientTemperatureDrift(float ambientTemperature) {
 
  if (!temperatureInitialized) {
    return;
  }
 
  if (climateMode != CLIMATE_STANDBY) {
    return;
  }
 
  if (simulatedTemperature < ambientTemperature) {
    simulatedTemperature += 0.5;
    if (simulatedTemperature > ambientTemperature) {
      simulatedTemperature = ambientTemperature;
    }
  }
 
  else if (simulatedTemperature > ambientTemperature) {
    simulatedTemperature -= 0.5;
    if (simulatedTemperature < ambientTemperature) {
      simulatedTemperature = ambientTemperature;
    }
  }
}
 
void safetyCurtainControl(float smokePpm) {
 
  bool smokeDetected = smokeAlarmActive(smokePpm);
 
  // Smoke has priority over the Blynk command
  if (smokeDetected) {
 
    windowOpen = true;
 
    Serial.println("SAFETY: Smoke detected - Window forced OPEN");
  }
 
  else {
 
    windowOpen = requestedWindowOpen;
  }
 
 
  if (windowOpen) {
 
    servo.write(CURTAINS_OPEN);
    Serial.println("Curtains/Windows: OPEN");
  }
 
  else {
 
    servo.write(CURTAINS_CLOSED);
    Serial.println("Curtains/Windows: CLOSED");
  }
}
 
void lightingControl(int motion, int lightLevel) {
  // Motion detected means room is occupied
  if (motion == HIGH) {
    occupied = true;
    lastMotionTime = millis();
  }
  // No motion for long enough - room is now considered empty
  // For simulation i am using 10 seconds
  if (occupied && millis() - lastMotionTime >= OCCUPANCY_TIMEOUT) {
    occupied = false;
  }
  if (requestedLightMode == LIGHT_OFF) {
    digitalWrite(BULB_PIN, LOW);
    Serial.println("LIGHT: OFF - Manual control");
    return;
  }
  if (requestedLightMode == LIGHT_ON) {
    digitalWrite(BULB_PIN, HIGH);
    Serial.println("LIGHT: ON - Manual control");
    return;
  }
  // Higher LDR value means darker
  bool darkRoom = (lightLevel >= LIGHT_THRESHOLD);
  if (occupied && darkRoom) {
    digitalWrite(BULB_PIN, HIGH);
    Serial.println("LIGHT: ON - Room occupied and dark");
  } else {
    digitalWrite(BULB_PIN, LOW);
    if (!occupied) {
      Serial.println("LIGHT: OFF - Room unoccupied");
    } else {
      Serial.println("LIGHT: OFF - Enough ambient light");
    }
  }
}
 
void loop() {
 
  if (!systemBoot) {
    systemBootUp();
  }
 
  // Reading sensors
  TempAndHumidity data = dht.getTempAndHumidity();
 
  int motion = readMotion();
  int lightLevel = analogRead(LDR_PIN);
  int smokeAdc = analogRead(SMOKE_PIN);
  float smokePpm = smokeAdcToPpm(smokeAdc);
  Serial.println("---- SENSOR DATA --------");
 
  Serial.print("Actual Temperature: ");
  Serial.print(data.temperature);
  Serial.println(" C");
 
  Serial.print("Simulated Temperature: ");
  Serial.print(simulatedTemperature);
  Serial.println(" C");
 
  Serial.print("Humidity: ");
  Serial.print(data.humidity);
  Serial.println(" %");
 
  Serial.print("Motion: ");
  Serial.println(motion ? "DETECTED" : "None");
 
  Serial.print("Light level: ");
  Serial.println(lightLevel);
 
  Serial.print("Smoke level (estimated): ");
  Serial.print(smokePpm, 1);
  Serial.print(" ppm | Raw ADC: ");
  Serial.println(smokeAdc);
 
  Serial.println("----------");
  // Automation systems
  climateControl(data.temperature);
 
  lightingControl(motion, lightLevel);
  safetyCurtainControl(smokePpm);
  delay(2000);
  // when HVAC is off
  ambientTemperatureDrift(data.temperature);
  if (millis() - lastBlynkUpload >= BLYNK_UPLOAD_INTERVAL) {
 
  lastBlynkUpload = millis();
 
  uploadTelemetry(
    data.temperature,
    data.humidity,
    lightLevel,
    smokeAdc,
    smokePpm
  );
 }
 if (millis() - lastBlynkControl >= BLYNK_CONTROL_INTERVAL) {
 
  lastBlynkControl = millis();
 
  getBlynkControls();
 }
}
