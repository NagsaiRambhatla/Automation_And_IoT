#include "DHTesp.h"
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

// Pin Definitions 
#define DHT_PIN    15
#define PIR_PIN    27
#define LDR_PIN    34
#define SMOKE_PIN  35
#define SERVO_PIN  12
#define RELAY_PIN  14
#define LED_STRIP_PIN 2
#define NUM_PIXELS 5
#define BULB_PIN 16
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

const int LIGHT_THRESHOLD = 2000;
const int SMOKE_THRESHOLD = 2000;

// Lighting  config
unsigned long lastMotionTime = 0;
const unsigned long OCCUPANCY_TIMEOUT = 10000;
bool occupied = false;

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

void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(SERVO_PIN, 500, 2400);
  strip.begin();
  strip.show();
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SMOKE_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BULB_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BULB_PIN, LOW);
  servo.write(CURTAINS_CLOSED);
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

void safetyCurtainControl(int smokeLevel) {
  if (smokeLevel >= SMOKE_THRESHOLD) {
    servo.write(CURTAINS_OPEN);
    Serial.println("SAFETY: Smoke detected");
    Serial.println("Curtains/Windows: OPEN");

  } else {
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

  int motion = digitalRead(PIR_PIN);
  int lightLevel = analogRead(LDR_PIN);
  int smokeLevel = analogRead(SMOKE_PIN);
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

  Serial.print("Smoke level: ");
  Serial.println(smokeLevel);

  Serial.println("----------");
  // Automation systems
  climateControl(data.temperature);

  lightingControl(motion, lightLevel);

  safetyCurtainControl(smokeLevel);
  delay(2000);
  // when HVAC is off
  ambientTemperatureDrift(data.temperature);
}