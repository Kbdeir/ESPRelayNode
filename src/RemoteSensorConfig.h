#pragma once
#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include <JSONConfig.h>

#define MAX_REMOTE_SENSORS 8

typedef struct {
  char  topic[64];
  char  jsonKey[32];
  char  label[32];
  char  lastPayload[256];  // JSON-escaped MQTT snapshot for web UI key inspector; best-effort, no mutex
  // Written by remoteSensorsOnMqttMessage (async_tcp task / Core 0).
  // Read by readConditionSensorValue (loop task / Core 1).
  // 32-bit aligned volatile stores are atomic on Xtensa LX6 — same pattern as
  // volatile bool mqttHealthAwaitingEcho and volatile int pending_relay_state_idx.
  volatile float    cachedValue;
  volatile uint32_t lastSeenMs;  // 0 = never received
} TRemoteSensor;

extern TRemoteSensor remoteSensors[MAX_REMOTE_SENSORS];

// Load /remotesensors.json into the in-memory array. Clears runtime cache fields.
// Call from setupScheduler() after LittleFS is mounted.
void loadRemoteSensors();

// Persist the form params from the web UI and reload into memory.
bool saveRemoteSensors(AsyncWebServerRequest* request);

// Subscribe to all configured remote sensor topics.
// Called from mqttSubscribeRelays() (loop task) — not from onMqttConnect directly,
// matching the deferred pending_mqtt_subscribe pattern used for relay subscriptions.
void remoteSensorsMqttSubscribe();

// Match incoming MQTT topic against configured slots and update cachedValue/lastSeenMs.
// Called from onMqttMessage (async_tcp task). Zero heap allocation — uses only
// stack buffers and StaticJsonDocument.
void remoteSensorsOnMqttMessage(const char* topic, const char* payload, size_t len);
