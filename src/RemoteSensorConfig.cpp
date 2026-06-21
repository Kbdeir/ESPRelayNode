#include <RemoteSensorConfig.h>
#include <MQTT_Processes.h>
#include <ArduinoJson.h>
// JSONConfig.h (pulled in via RemoteSensorConfig.h) defines SPIFFS as LITTLEFS —
// do NOT include <SPIFFS.h> directly; it would conflict with that macro.

TRemoteSensor remoteSensors[MAX_REMOTE_SENSORS];

#define REMOTE_SENSORS_FILE "/remotesensors.json"

// 8 slots × ~130 bytes (topic + jsonKey + label + JSON keys/overhead) ≈ 1200 B.
// StaticJsonDocument is stack-allocated; 1536 B is within the async_tcp task
// stack budget (4096 B default) and matches the automation_buffer_size pattern.
#define RS_JSON_SIZE 1536

void loadRemoteSensors() {
  // Clear config fields first so absent/empty slots are well-defined.
  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    remoteSensors[i].topic[0]       = '\0';
    remoteSensors[i].jsonKey[0]     = '\0';
    remoteSensors[i].label[0]       = '\0';
    remoteSensors[i].lastPayload[0] = '\0';
    // Reset runtime cache so a stale value from a previous session is never
    // evaluated as "live" after the topic is cleared or changed.
    remoteSensors[i].cachedValue = 0.0f;
    remoteSensors[i].lastSeenMs  = 0;
  }

  if (!SPIFFS.exists(REMOTE_SENSORS_FILE)) return;

  File f = SPIFFS.open(REMOTE_SENSORS_FILE, "r");
  if (!f) return;

  StaticJsonDocument<RS_JSON_SIZE> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    Serial.println(F("[REMOTE ] Failed to parse /remotesensors.json — using empty config"));
    return;
  }
  f.close();

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull()) return;

  uint8_t i = 0;
  for (JsonObject obj : arr) {
    if (i >= MAX_REMOTE_SENSORS) break;
    strlcpy(remoteSensors[i].topic,   obj["topic"]   | "", sizeof(remoteSensors[i].topic));
    strlcpy(remoteSensors[i].jsonKey, obj["jsonKey"] | "", sizeof(remoteSensors[i].jsonKey));
    strlcpy(remoteSensors[i].label,   obj["label"]   | "", sizeof(remoteSensors[i].label));
    i++;
  }
}

bool saveRemoteSensors(AsyncWebServerRequest* request) {
  StaticJsonDocument<RS_JSON_SIZE> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    char topicKey[12], keyKey[10], labelKey[12];
    snprintf(topicKey, sizeof(topicKey), "RS%u_TOPIC",  (unsigned)(i + 1));
    snprintf(keyKey,   sizeof(keyKey),   "RS%u_KEY",    (unsigned)(i + 1));
    snprintf(labelKey, sizeof(labelKey), "RS%u_LABEL",  (unsigned)(i + 1));

    // request->arg() returns Arduino String by value — ArduinoJson copies the
    // content into its pool during assignment, so the temporary String lifetime
    // is not a concern (same pattern as saveAutomationRule in AutomationRules.cpp).
    JsonObject obj = arr.createNestedObject();
    obj["topic"]   = request->arg(topicKey);
    obj["jsonKey"] = request->arg(keyKey);
    obj["label"]   = request->arg(labelKey);
  }

  File f = SPIFFS.open(REMOTE_SENSORS_FILE, "w");
  if (!f) {
    Serial.println(F("[REMOTE ] Failed to open /remotesensors.json for writing"));
    return false;
  }
  serializeJsonPretty(doc, f);
  f.print('\n');
  f.flush();
  f.close();

  // Refresh in-memory config immediately so evaluateAutomationRules() sees the
  // new topics before the next MQTT reconnect triggers re-subscription.
  loadRemoteSensors();
  return true;
}

void remoteSensorsMqttSubscribe() {
  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    if (remoteSensors[i].topic[0] != '\0') {
      mqttClient.subscribe(remoteSensors[i].topic, 0);
      Serial.printf("[REMOTE ] Subscribed -> %s\n", remoteSensors[i].topic);
    }
  }
}

// If the MQTT payload is a nested wrapper such as {"msg":{"data":[{...}]}},
// extract the first element of the "data" array and return a pointer+length
// for that inner object.  If the pattern is absent the original src/len are
// returned unchanged, so flat payloads continue to work without modification.
static void unwrapDataArray(const char* src, size_t srcLen,
                            const char** outSrc, size_t* outLen) {
  *outSrc = src;
  *outLen = srcLen;
  static const char needle[] = "\"data\":[{";
  if (srcLen < 9) return;
  for (size_t j = 0; j <= srcLen - 9; j++) {
    if (memcmp(src + j, needle, 9) == 0) {
      const char* inner = src + j + 8; // points to the '{' of data[0]
      const char* limit = src + srcLen;
      int depth = 0;
      const char* p = inner;
      while (p < limit) {
        if      (*p == '{') depth++;
        else if (*p == '}') { if (--depth == 0) { *outSrc = inner; *outLen = (size_t)(p - inner + 1); return; } }
        p++;
      }
      return; // malformed — leave unchanged
    }
  }
}

void remoteSensorsOnMqttMessage(const char* topic, const char* payload, size_t len) {
  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    if (remoteSensors[i].topic[0] == '\0') continue;
    if (strcmp(topic, remoteSensors[i].topic) != 0) continue;

    float value = 0.0f;

    // Always try JSON parse (unwrap msg.data wrapper first).
    // One StaticJsonDocument serves both key-extraction and value-lookup —
    // no second parse, no heap allocation.
    const char* src; size_t srcLen;
    unwrapDataArray(payload, len, &src, &srcLen);
    StaticJsonDocument<512> jdoc;
    const bool isJson = (deserializeJson(jdoc, src, srcLen) == DeserializationError::Ok);

    if (isJson) {
      // Resolve the object to inspect (unwrap top-level [{...}] array).
      JsonObject obj;
      if (jdoc.is<JsonArray>()) {
        JsonArray arr = jdoc.as<JsonArray>();
        if (arr.size() > 0) obj = arr[0].as<JsonObject>();
      } else {
        obj = jdoc.as<JsonObject>();
      }

      // Store comma-separated key list in lastPayload for the web UI inspector.
      // Keys are short alphanumeric names — no escaping needed in JSON responses.
      {
        char* out = remoteSensors[i].lastPayload;
        const char* limit = out + sizeof(remoteSensors[i].lastPayload) - 1;
        bool first = true;
        if (!obj.isNull()) {
          for (JsonPair kv : obj) {
            const char* k = kv.key().c_str();
            size_t kl = strlen(k);
            if (!first) { if (out >= limit) break; *out++ = ','; }
            first = false;
            if (out + kl > limit) break;
            memcpy(out, k, kl); out += kl;
          }
        }
        *out = '\0';
      }

      // Extract float for configured key; map string values to float.
      // For non-numeric strings ("on"/"off" etc.) also append "|<string>" to
      // lastPayload so the web UI can display the original string instead of 1/0.
      if (remoteSensors[i].jsonKey[0] != '\0' && !obj.isNull()) {
        JsonVariant v = obj[remoteSensors[i].jsonKey];
        if (!v.isNull()) {
          if (v.is<const char*>()) {
            const char* s = v.as<const char*>();
            bool isTrue  = strcasecmp(s,"on")==0||strcasecmp(s,"true")==0||strcasecmp(s,"yes")==0;
            bool isFalse = strcasecmp(s,"off")==0||strcasecmp(s,"false")==0||strcasecmp(s,"no")==0;
            if (isTrue || isFalse) {
              value = isTrue ? 1.0f : 0.0f;
              // Append "|displayString" for the web UI live value column
              size_t klen = strlen(remoteSensors[i].lastPayload);
              char* out   = remoteSensors[i].lastPayload + klen;
              const char* lim = remoteSensors[i].lastPayload + sizeof(remoteSensors[i].lastPayload) - 1;
              size_t slen = strlen(s);
              if (out + 1 + slen <= lim) {
                *out++ = '|';
                memcpy(out, s, slen); out[slen] = '\0';
              }
            } else {
              value = v.as<float>();
            }
          } else {
            value = v.as<float>();
          }
        }
      }
    } else {
      // Plain (non-JSON) payload: "on", "off", "42.5" etc.
      // Store the raw string in lastPayload so the web UI shows it directly.
      size_t storeLen = len < sizeof(remoteSensors[i].lastPayload) - 1
                      ? len : sizeof(remoteSensors[i].lastPayload) - 1;
      memcpy(remoteSensors[i].lastPayload, payload, storeLen);
      remoteSensors[i].lastPayload[storeLen] = '\0';

      // Map boolean strings; fall back to atof for numeric payloads.
      char buf[32];
      size_t copyLen = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
      memcpy(buf, payload, copyLen); buf[copyLen] = '\0';
      if      (strcasecmp(buf,"on")==0||strcasecmp(buf,"true")==0||strcasecmp(buf,"yes")==0) value=1.0f;
      else if (strcasecmp(buf,"off")==0||strcasecmp(buf,"false")==0||strcasecmp(buf,"no")==0) value=0.0f;
      else    value = (float)atof(buf);
    }

    // 32-bit aligned volatile writes are atomic on Xtensa LX6.
    remoteSensors[i].cachedValue = value;
    remoteSensors[i].lastSeenMs  = millis();
    break; // topics must be unique across slots — no need to scan further
  }
}
