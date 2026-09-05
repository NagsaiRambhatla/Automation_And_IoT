#include "DHTesp.h"
#include <ESP32Servo.h>

// Pin Definitions 
#define DHT_PIN    15
#define PIR_PIN    27
#define LDR_PIN    34
#define SMOKE_PIN  35
#define SERVO_PIN  12
#define RELAY_PIN  14

DHTesp dht;
Servo servo;

void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(SERVO_PIN, 500, 2400);
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(SMOKE_PIN, INPUT);
  Serial.println("Testing");
}

void loop() {

  // Temperature + Humidity
  TempAndHumidity data = dht.getTempAndHumidity();

  // Motion
  int motion = digitalRead(PIR_PIN);

  // Light
  int lightLevel = analogRead(LDR_PIN);

  // Smoke / Gas
  int smokeLevel = analogRead(SMOKE_PIN);

  Serial.println("-------- SENSOR DATA --------");
  servo.write(100);
  Serial.print("Temperature: ");
  Serial.print(data.temperature);
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

  Serial.println("------------");
  Serial.println();
  // servo.write(30);
  delay(2000);
}