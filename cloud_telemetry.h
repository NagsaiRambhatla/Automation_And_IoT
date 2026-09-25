#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <atomic>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cloud_config.h"
#include "cloud_ca.h"

// Copyable snapshot: only the main loop accesses sensors and actuators.
struct CloudSample {
  float temperature;
  float humidity;
  float simulatedTemperature;
  float targetTemperature;
  int lightLevel;
  int smokeLevel;
  bool occupied;
  bool lightOn;
  bool windowOpen;
  bool sensorValid;
  unsigned char climateMode; // 0 standby, 1 cooling, 2 heating
};

static QueueHandle_t cloudSamples = nullptr;
static std::atomic<bool> cloudUploadHealthy{false};

// Rule-based edge classification combines occupancy memory, darkness and smoke.
// Smoke takes priority, then an invalid climate sensor, then occupancy/light.
inline const char* roomAssessment(const CloudSample& sample) {
  if (sample.windowOpen) return "Smoke alert";
  if (!sample.sensorValid) return "Climate sensor fault";
  if (!sample.occupied) return "Room empty";
  if (sample.lightOn) return "Occupied and dark";
  return "Occupied and bright";
}

inline const char* climateDescription(const CloudSample& sample) {
  if (!sample.sensorValid) return "Sensor fault";
  if (sample.climateMode == 1) return "Cooling";
  if (sample.climateMode == 2) return "Heating";
  return "Standby";
}

inline String encodeCloudValue(const char* value) {
  const char* hex = "0123456789ABCDEF";
  String encoded;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
        (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      encoded += static_cast<char>(*p);
    } else {
      encoded += '%';
      encoded += hex[*p >> 4];
      encoded += hex[*p & 15];
    }
  }
  return encoded;
}

inline String cloudValues(const CloudSample& sample) {
  String values;
  values.reserve(260);
  // Invalid numeric readings are omitted; their widgets expire instead of
  // presenting old readings as current. Fault text is still uploaded.
  if (sample.sensorValid) {
    values += "&v0=" + String(sample.temperature, 2);
    values += "&v1=" + String(sample.humidity, 2);
    values += "&v8=" + String(sample.simulatedTemperature, 2);
  }
  values += "&v2=" + String(sample.lightLevel);
  values += "&v3=" + String(sample.smokeLevel);
  values += "&v4=" + String(sample.occupied ? 1 : 0);
  values += "&v5=" + String(sample.lightOn ? 1 : 0);
  values += "&v6=" + encodeCloudValue(climateDescription(sample));
  values += "&v7=" + String(sample.windowOpen ? 1 : 0);
  values += "&v9=" + String(sample.targetTemperature, 2);
  values += "&v10=" + encodeCloudValue(roomAssessment(sample));
  return values;
}

inline void cloudWorker(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  unsigned long lastAttempt = 0;
  unsigned long lastReconnect = millis();
  bool firstUpload = true;
  CloudSample previousAttempt{};

  for (;;) {
    const unsigned long now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      cloudUploadHealthy.store(false);
      if (now - lastReconnect >= 15000) {
        lastReconnect = now;
        WiFi.reconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }
    // Certificate validity checks need the real date. Never disable TLS checks.
    if (time(nullptr) < 1700000000) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    CloudSample sample{};
    const bool haveSample = xQueuePeek(cloudSamples, &sample, 0) == pdTRUE;
    // A short occupancy or safety transition must not fall between periodic
    // uploads. Rate-limit these extra batches; climate cycling stays periodic.
    const bool stateChanged = haveSample &&
        (sample.occupied != previousAttempt.occupied ||
         sample.lightOn != previousAttempt.lightOn ||
         sample.windowOpen != previousAttempt.windowOpen ||
         sample.sensorValid != previousAttempt.sensorValid);
    if (haveSample && (firstUpload || now - lastAttempt >= CLOUD_UPLOAD_INTERVAL_MS ||
                       (stateChanged && now - lastAttempt >= 2000))) {
      firstUpload = false;
      lastAttempt = now;
      previousAttempt = sample;
      WiFiClientSecure client;
      client.setCACert(BLYNK_ROOT_CA);
      client.setHandshakeTimeout(10);
      HTTPClient http;
      http.setConnectTimeout(5000);
      http.setTimeout(5000);
      const String url = String("https://") + BLYNK_SERVER +
          "/external/api/batch/update?token=" + encodeCloudValue(BLYNK_DEVICE_TOKEN) + cloudValues(sample);
      int result = -1;
      if (http.begin(client, url)) {
        result = http.GET();
        http.end();
      }
      cloudUploadHealthy.store(result == HTTP_CODE_OK);
      // Do not log the URL: it contains the private device credential.
      Serial.printf("Blynk HTTPS upload: %s (HTTP %d)\n", result == HTTP_CODE_OK ? "OK" : "FAILED", result);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

inline void beginCloudTelemetry() {
  if (strlen(BLYNK_DEVICE_TOKEN) == 0) {
    Serial.println("Cloud disabled: add a device token in local secrets.h. Local automation remains active.");
    return;
  }
  cloudSamples = xQueueCreate(1, sizeof(CloudSample));
  if (!cloudSamples) {
    Serial.println("Cloud queue allocation failed.");
    return;
  }
  // Networking has its own task, so slow HTTPS or a disconnected network cannot
  // block smoke response, motion detection or the other local automation.
  if (xTaskCreate(cloudWorker, "blynk_https", 12288, nullptr, 1, nullptr) != pdPASS) {
    vQueueDelete(cloudSamples);
    cloudSamples = nullptr;
    Serial.println("Cloud task creation failed.");
  }
}

inline void publishCloudSample(const CloudSample& sample) {
  if (cloudSamples) xQueueOverwrite(cloudSamples, &sample);
}
