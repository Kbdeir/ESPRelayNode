#include "HADiscovery.h"

#ifdef HA_DISCOVERY

#include <ArduinoJson.h>
#include <vector>

#include <ConfigParams.h>
#include <RelayClass.h>
#include <MQTT_Processes.h>
#include <WaterFlowSensor.h>
#include <SerialLog.h>

extern std::vector<void *> relays;  // populated in main.cpp setup(); relays[0] is always relay0

namespace {

// StaticJsonDocument pool size. Every field is published as an "unowned"
// const char* (the underlying Strings live in MyConfParam / relay configs /
// the static HAIdentity below, all of which outlive the publish call), so
// the pool only has to hold the variant tree itself plus the handful of
// small computed strings (uniq_id, value_template). 768 bytes leaves
// comfortable headroom without spilling into the heap.
constexpr size_t kDocCapacity = 768;
constexpr size_t kPayloadCapacity = 768;
constexpr size_t kTopicCapacity = 160;

// Identity shared by every discovery payload. CID() is derived from the
// eFuse MAC and never changes, so this is computed once and cached - the
// cached Strings live for the program's lifetime, so .c_str() pointers into
// them stay valid for every later publish.
struct HAIdentity {
  String deviceId;   // "smartconfig_<CID>" - discovery topic node_id + device.identifiers
  String willTopic;  // "/home/Controller<CID>/Birth" - availability_topic
};

const HAIdentity& identity() {
  static HAIdentity id;
  if (id.deviceId.isEmpty()) {
    id.deviceId  = "smartconfig_" + CID();
    id.willTopic = mqttWillTopic();
  }
  return id;
}

const char* boardModel() {
#if defined(ESP32_4RBoard)
  return "SmartConfig ESP32 4-Relay";
#elif defined(ESP32_3RBoard)
  return "SmartConfig ESP32 3-Relay";
#elif defined(ESP32_2RBoard)
  return "SmartConfig ESP32 2-Relay";
#elif defined(HWESP32)
  return "SmartConfig ESP32";
#else
  return "SmartConfig";
#endif
}

// Fills the fields common to every entity: unique_id, availability and the
// HA "device" block that groups all entities under one device card.
void fillCommon(JsonDocument& doc, const char* name, const String& objectId) {
  const HAIdentity& id = identity();

  doc["name"]    = name;
  doc["uniq_id"] = id.deviceId + "_" + objectId;

  doc["avty_t"]       = id.willTopic.c_str();
  doc["pl_avail"]     = "online";
  doc["pl_not_avail"] = "offline";

  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"][0] = id.deviceId.c_str();
  dev["name"]   = MyConfParam.v_PhyLoc.c_str();
  dev["mf"]     = "SmartConfig";
  dev["mdl"]    = boardModel();
  dev["sw"]     = __DATE__ " " __TIME__;
}

// Serializes doc and publishes it (retained) to
// homeassistant/<component>/<deviceId>/<objectId>/config.
void publishDiscovery(const char* component, const String& objectId, const JsonDocument& doc) {
  const HAIdentity& id = identity();

  char topic[kTopicCapacity];
  snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config",
           component, id.deviceId.c_str(), objectId.c_str());

  char payload[kPayloadCapacity];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0 || n >= sizeof(payload)) {
    Serial.printf("[HADISC ] payload too large for '%s', skipped\n", objectId.c_str());
    return;
  }

  uint16_t pktId = mqttPublish(topic, 1, true, payload, n);
  SLOG(SLOG_HADISC, "[HADISC ] %s -> %s (pktId=%u, %ub)\n", topic, pktId ? "sent" : "FAILED", pktId, (unsigned)n);
}

// ---- Relay switches ------------------------------------------------------

void publishRelaySwitch(size_t relayIdx) {
  Relay* rly = static_cast<Relay*>(relays[relayIdx]);
  if (!rly) return;
  Trelayconf* cfg = rly->RelayConfParam;

  const String objectId = "relay" + String(cfg->v_relaynb);
  const String name = cfg->v_PhyLoc.isEmpty() ? ("Relay " + String(cfg->v_relaynb)) : cfg->v_PhyLoc;

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, name.c_str(), objectId);

  doc["stat_t"] = cfg->v_STATE_PUB_TOPIC.c_str();
  doc["cmd_t"]  = cfg->v_PUB_TOPIC1.c_str();
  doc["pl_on"]  = ON;
  doc["pl_off"] = OFF;

  publishDiscovery("switch", objectId, doc);
}

// ---- Temperature sensors (DS18B20, optional 2nd probe) -------------------

void publishTemperatureSensor(int sensorNum, const String& stateTopic) {
  const String objectId = "temperature" + String(sensorNum);
  const String name = "Temperature " + String(sensorNum);

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, name.c_str(), objectId);

  doc["stat_t"]       = stateTopic.c_str();
  doc["dev_cla"]      = "temperature";
  doc["unit_of_meas"] = "\xC2\xB0" "C";  // degree symbol, UTF-8
  doc["stat_cla"]     = "measurement";

  publishDiscovery("sensor", objectId, doc);
}

// ---- Water flow sensor (rate + cumulative volume) -------------------------

#ifdef WaterFlowSensor

void publishWaterFlowRate() {
  const String objectId = "waterflow_rate";

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, "Water Flow Rate", objectId);

  doc["stat_t"]       = WaterFlowSensor_Topic.c_str();
  doc["unit_of_meas"] = "L/min";
  doc["stat_cla"]     = "measurement";
  doc["icon"]         = "mdi:water";

  publishDiscovery("sensor", objectId, doc);
}

void publishWaterFlowTotal() {
  const String objectId = "waterflow_total";

  // Mirrors the suffix substitution in main.cpp's WaterFlowSensor publish:
  // the cumulative-volume topic is WaterFlowSensor_Topic with its last path
  // segment replaced by "TotalLiters".
  String stateTopic = WaterFlowSensor_Topic;
  const int slash = stateTopic.lastIndexOf('/');
  stateTopic = (slash >= 0) ? (stateTopic.substring(0, slash + 1) + "TotalLiters") : "TotalLiters";

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, "Water Total Volume", objectId);

  doc["stat_t"]       = stateTopic.c_str();
  doc["unit_of_meas"] = "L";
  doc["dev_cla"]      = "water";
  doc["stat_cla"]     = "total_increasing";

  publishDiscovery("sensor", objectId, doc);
}

#endif // WaterFlowSensor

// ---- ACS712 per-relay current ---------------------------------------------

#ifdef _ACS712_

void publishCurrentSensor(size_t relayIdx) {
  Relay* rly = static_cast<Relay*>(relays[relayIdx]);
  if (!rly) return;
  Trelayconf* cfg = rly->RelayConfParam;

  const String objectId = "current" + String(cfg->v_relaynb);
  const String baseName = cfg->v_PhyLoc.isEmpty() ? ("Relay " + String(cfg->v_relaynb)) : cfg->v_PhyLoc;
  const String name = baseName + " Current";

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, name.c_str(), objectId);

  doc["stat_t"]       = cfg->v_ACS_AMPS.c_str();
  doc["dev_cla"]      = "current";
  doc["unit_of_meas"] = "A";
  doc["stat_cla"]     = "measurement";

  publishDiscovery("sensor", objectId, doc);
}

#endif // _ACS712_

// ---- CT / EmonLib power sensors -------------------------------------------

#ifdef _emonlib_

struct EmonField {
  const char* objectId;
  const char* name;
  const char* jsonKey;
  const char* deviceClass;  // nullptr -> omit dev_cla
  const char* unit;         // nullptr -> omit unit_of_meas
  const char* stateClass;
};

constexpr EmonField kEmonFields[] = {
  {"ct_voltage",     "CT Voltage",      "voltage", "voltage", "V",   "measurement"},
  {"ct_current",     "CT Current",      "amps",    "current", "A",   "measurement"},
  {"ct_power",       "CT Power",        "power",   "power",   "W",   "measurement"},
  {"ct_energy",      "CT Energy",       "wh",      "energy",  "kWh", "total_increasing"},
  {"ct_powerfactor", "CT Power Factor", "PF",      nullptr,   nullptr, "measurement"},
};
constexpr size_t kEmonFieldCount = sizeof(kEmonFields) / sizeof(kEmonFields[0]);

void publishEmonSensor(size_t fieldIdx) {
  const EmonField& f = kEmonFields[fieldIdx];
  const String objectId = f.objectId;

  StaticJsonDocument<kDocCapacity> doc;
  fillCommon(doc, f.name, objectId);

  doc["stat_t"] = MyConfParam.v_CurrentTransformerTopic.c_str();
  if (f.deviceClass) doc["dev_cla"]      = f.deviceClass;
  if (f.unit)        doc["unit_of_meas"] = f.unit;
  if (f.stateClass)  doc["stat_cla"]     = f.stateClass;

  // CT_ProcessPower publishes "json:{...}" - strip the 5-char "json:" prefix
  // before parsing, then index into msg.data[0].
  char tpl[80];
  snprintf(tpl, sizeof(tpl), "{{ (value[5:] | from_json).msg.data[0].%s }}", f.jsonKey);
  doc["val_tpl"] = tpl;

  publishDiscovery("sensor", objectId, doc);
}

#endif // _emonlib_

// ---- Dispatcher -------------------------------------------------------------

// Publishes the discovery config for entity `idx` (stable but otherwise
// arbitrary order). Returns false once idx is past the last entity.
bool publishEntity(int idx) {
  int cursor = 0;

  for (size_t i = 0; i < relays.size(); ++i) {
    if (cursor == idx) { publishRelaySwitch(i); return true; }
    cursor++;
  }

#ifdef HWESP32
  if (!relays.empty()) {
    Relay* rly0 = static_cast<Relay*>(relays[0]);
    if (rly0 && rly0->RelayConfParam->v_TemperatureValue != "0") {
      if (cursor == idx) { publishTemperatureSensor(1, rly0->RelayConfParam->v_TemperatureValue); return true; }
      cursor++;
      if (MyConfParam.v_TempSensorPin02_Active) {
        if (cursor == idx) {
          publishTemperatureSensor(2, rly0->RelayConfParam->v_TemperatureValue + "_2");
          return true;
        }
        cursor++;
      }
    }
  }
#endif

#ifdef WaterFlowSensor
  if (cursor == idx) { publishWaterFlowRate(); return true; }
  cursor++;
  if (cursor == idx) { publishWaterFlowTotal(); return true; }
  cursor++;
#endif

#ifdef _ACS712_
  for (size_t i = 0; i < relays.size(); ++i) {
    Relay* rly = static_cast<Relay*>(relays[i]);
    if (rly && rly->RelayConfParam->v_ACS_Active) {
      if (cursor == idx) { publishCurrentSensor(i); return true; }
      cursor++;
    }
  }
#endif

#ifdef _emonlib_
  for (size_t i = 0; i < kEmonFieldCount; ++i) {
    if (cursor == idx) { publishEmonSensor(i); return true; }
    cursor++;
  }
#endif

  return false;
}

int pendingIdx = -1;             // -1 = idle, 0..N-1 = next entity to publish
unsigned long nextPublishMs = 0;

}  // namespace

void haDiscoveryBegin(unsigned long startAtMs) {
  pendingIdx = 0;
  nextPublishMs = startAtMs;
  SLOG(SLOG_HADISC, "[HADISC ] discovery sequence scheduled, starts at %lums (now=%lums)\n", startAtMs, millis());
}

void haDiscoveryService() {
  if (pendingIdx < 0) return;
  if (!mqttIsEnabled() || !mqttClient.connected()) return;
  if (millis() < nextPublishMs) return;

  if (!publishEntity(pendingIdx)) {
    SLOG(SLOG_HADISC, "[HADISC ] discovery sequence done (%d entities)\n", pendingIdx);
    pendingIdx = -1;
    return;
  }
  pendingIdx++;
  nextPublishMs = millis() + 250;
}

#endif // HA_DISCOVERY
