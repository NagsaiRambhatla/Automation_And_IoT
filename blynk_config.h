#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define BLYNK_AUTH_TOKEN "IrWjjg1Mk-3EqqZvjulQScvLt-8wF0eS"
#define BLYNK_TEMPLATE_ID "TMPL67Jur1hEj"
#define BLYNK_TEMPLATE_NAME "3707ICT Smart Room"

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define BLYNK_SERVER "sgp1.blynk.cloud"

unsigned long lastBlynkUpload = 0;
const unsigned long BLYNK_UPLOAD_INTERVAL = 2000;

unsigned long lastBlynkControl = 0;
const unsigned long BLYNK_CONTROL_INTERVAL = 2000;

void connectWiFi() {

  Serial.print("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
}

bool updateBlynk(String values) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Blynk: WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
      String("https://") + BLYNK_SERVER +
      "/external/api/batch/update?token=" +
      BLYNK_AUTH_TOKEN +
      values;

  http.begin(client, url);

  int responseCode = http.GET();

  Serial.print("Blynk upload response: ");
  Serial.println(responseCode);

  http.end();

  return responseCode == 200;
}

String getBlynkValue(String pin) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Blynk: WiFi not connected");
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
      String("https://") + BLYNK_SERVER +
      "/external/api/get?token=" +
      BLYNK_AUTH_TOKEN +
      "&" + pin;

  http.begin(client, url);

  int responseCode = http.GET();

  if (responseCode != 200) {
    Serial.print("Blynk GET failed: ");
    Serial.println(responseCode);

    http.end();
    return "";
  }

  String response = http.getString();

  http.end();

  return response;
}