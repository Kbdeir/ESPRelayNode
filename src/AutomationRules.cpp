
#include <AutomationRules.h>
#include <TimerClass.h>
#include <RelayClass.h>

// Every field is stored (and reloaded) as a string — e.g. "C0Val1": "23.456"
// rather than a number — so deserializeJson() must duplicate ~17 key+value
// strings into the pool when parsing from a Stream; 512 B was marginal enough
// to silently overflow (NoMemory) for rules using most/all condition slots.
#define automation_buffer_size 1024

#ifdef ESP32
#include <sys/stat.h>
#endif

// Live sensor globals populated elsewhere in main.cpp — same extern pattern
// AsyncHTTP_Helper.cpp already uses to reach these (see AsyncHTTP_Helper.cpp:51-73).
#ifdef _emonlib_
#include <CT_ProcessPower.h>
extern CTPROCESSOR CT_1;
#endif
#ifdef _ADS1X15_
extern float g_adsV2, g_adsV3, g_adsMultiplier;
#ifdef _ADS1X15_DC_Current_
extern float g_adsFinalRMSCurrent, g_adsFinalDCCurrent;
#endif
#endif
#ifdef _pressureSensor_
#include <TLPressureSensor.h>
extern TLPressureSensor TL136;
#endif
extern float MCelcius;
extern float MCelcius2;
#ifdef WaterFlowSensor
extern float flowRate;
extern uint64_t totalMilliLitres;
#endif
#ifdef _REMOTE_SENSORS_
#include <RemoteSensorConfig.h>
#endif

CachedAutomationRule g_automationRuleCache[MAX_NUMBER_OF_AUTOMATION_RULES] = {};

void refreshAutomationRuleCache(int ruleIdx) {
  if (ruleIdx < 1 || ruleIdx > MAX_NUMBER_OF_AUTOMATION_RULES) return;
  CachedAutomationRule& c = g_automationRuleCache[ruleIdx - 1];
  AutomationRule r(ruleIdx);
  char fname[20];
  snprintf(fname, sizeof(fname), "/rule%d.json", ruleIdx);
  if (loadAutomationRule(fname, r) != SUCCESS) { c.loaded = false; return; }
  c.loaded           = true;
  c.enabled          = r.enabled;
  c.targetRelay      = r.targetRelay;
  c.action           = r.action;
  c.logic            = r.logic;
  c.conditions[0]    = r.conditions[0];
  c.conditions[1]    = r.conditions[1];
  c.triggerMode      = r.triggerMode;
  c.timeGateTimerId  = r.timeGateTimerId;
  c.holdSeconds      = r.holdSeconds;
  c.cooldownSeconds  = r.cooldownSeconds;
}

void refreshAllAutomationRuleCache() {
  for (int i = 1; i <= MAX_NUMBER_OF_AUTOMATION_RULES; i++) refreshAutomationRuleCache(i);
}

AutomationRule::AutomationRule(uint8_t para_id) {
  id = para_id;
  enabled = false;
  targetRelay = 0;
  action = ACT_ON;
  logic = LOGIC_AND;
  conditions[0] = { COND_NONE, OP_GREATER_THAN, 0.0f, 0.0f };
  conditions[1] = { COND_NONE, OP_GREATER_THAN, 0.0f, 0.0f };
  triggerMode = TRIGGER_ON_CHANGE;
  timeGateTimerId = 0;
  holdSeconds = 0;
  cooldownSeconds = 0;
}

AutomationRule::~AutomationRule() {
}

float readConditionSensorValue(ConditionSource source) {
#ifdef _emonlib_
  if (source == COND_CT_CURRENT)  return CT_1.amps;
  if (source == COND_CT_VOLTAGE)  return CT_1.supplyVoltage;
  if (source == COND_CT_POWER)    return CT_1.realPower;
#endif
#if defined(_ADS1X15_) && defined(_ADS1X15_DC_Current_)
  if (source == COND_HST_CURRENT) return g_adsFinalRMSCurrent;
#endif
#ifdef _ADS1X15_
  // Compare against the *scaled* battery voltage (what the UI labels "AIN2/AIN3
  // Scaled" and what users naturally think of as "the voltage") rather than the
  // raw ADC-pin voltage — g_adsV2/g_adsV3 hold the raw value (see main.cpp:3663-3669).
  if (source == COND_ADS_VOLT_CH2) return g_adsV2 * g_adsMultiplier;
  if (source == COND_ADS_VOLT_CH3) return g_adsV3 * g_adsMultiplier;
#endif
#ifdef WaterFlowSensor
  if (source == COND_FLOW_RATE)  return flowRate;
  if (source == COND_FLOW_TOTAL) return (float)(totalMilliLitres / 1000.0);
#endif
#ifdef _pressureSensor_
  if (source == COND_PRESSURE_LEVEL) return TL136.measure;
#endif
  if (source == COND_TEMP1) return MCelcius;
  if (source == COND_TEMP2) return MCelcius2;

#ifdef _REMOTE_SENSORS_
  if (source >= COND_REMOTE_1 && source <= COND_REMOTE_8) {
    uint8_t idx = (uint8_t)source - (uint8_t)COND_REMOTE_1;
    // Return NAN if the slot has been deleted (topic cleared) or if this board
    // has never received a message on this topic yet. NAN propagates through
    // evaluateCondition(): all IEEE 754 comparisons against NAN return false,
    // so the rule stays dormant rather than evaluating against a stale 0.0f.
    if (remoteSensors[idx].topic[0] == '\0' || remoteSensors[idx].lastSeenMs == 0)
      return NAN;
    return remoteSensors[idx].cachedValue;
  }
#endif

  return 0.0f; // COND_NONE or a source whose feature isn't compiled in
}

bool evaluateCondition(const TCondition &cond, float liveValue) {
  switch (cond.op) {
    case OP_GREATER_THAN: return liveValue > cond.value1;
    case OP_LESS_THAN:    return liveValue < cond.value1;
    case OP_BETWEEN:      return (liveValue >= cond.value1) && (liveValue <= cond.value2);
    case OP_EQUAL:        return fabsf(liveValue - cond.value1) < 0.001f;
    default:              return false;
  }
}

// Reads an existing timer file, sets CEnabled to "0", and writes it back.
// Called alongside disableAutomationRuleInFile when the rule has a time gate,
// so the timer is also shown as disabled in the UI and stays off after reboot.
static void disableTimerInFile(uint8_t timerId) {
  char filename[20];
  snprintf(filename, sizeof(filename), "/timer%u.json", (unsigned)timerId);

  File f = SPIFFS.open(filename, "r");
  if (!f) return;

  StaticJsonDocument<automation_buffer_size> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    return;
  }
  f.close();

  doc[F("CEnabled")] = "0";

  f = SPIFFS.open(filename, "w");
  if (!f) return;
  serializeJsonPretty(doc, f);
  f.print('\n');
  f.flush();
  f.close();
  Serial.printf("[AUTOMAT] Timer %u disabled in %s (rule source unavailable)\n",
                (unsigned)timerId, filename);
}

// Reads an existing rule file, sets REnabled to "0", and writes it back.
// Called once (guarded by hasResult) when an unavailable sensor source is detected,
// so the rule is shown as disabled in the UI and stays off after reboot.
static void disableAutomationRuleInFile(const char* filename) {
  File f = SPIFFS.open(filename, "r");
  if (!f) return;

  StaticJsonDocument<automation_buffer_size> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    return;
  }
  f.close();

  doc[F("REnabled")] = "0";

  f = SPIFFS.open(filename, "w");
  if (!f) return;
  serializeJsonPretty(doc, f);
  f.print('\n');
  f.flush();
  f.close();
  Serial.printf("[AUTOMAT] Rule disabled in %s (source unavailable)\n", filename);
}

config_read_error_t loadAutomationRule(char* filename, AutomationRule &para_Rule) {
  // Mirrors loadNodeTimer's existence check (TimerClass.cpp:115-135) — an absent
  // rule slot is a normal boot condition, so avoid tripping LittleFS's log_e().
#ifdef ESP32
  char fullpath[96];
  snprintf(fullpath, sizeof(fullpath), "/littlefs%s", filename);
  struct stat st;
  const bool present = (stat(fullpath, &st) == 0);
#else
  const bool present = SPIFFS.exists(filename);
#endif

  if (!present) {
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("[AUTOMAT] * Failed to open automation rule file *"));
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<automation_buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.print(F("[AUTOMAT] * Failed to read file, using default rule configuration *"));
    Serial.println(filename);
    configFile.close();
    return JSONCONFIG_CORRUPTED;
  }

  para_Rule.id          = (json[F("RNumber")].as<String>() != "") ? json[F("RNumber")].as<uint8_t>() : 0;
  para_Rule.enabled     = json[F("REnabled")].as<uint8_t>();
  para_Rule.targetRelay = (json[F("TargetRelay")].as<String>() != "") ? json[F("TargetRelay")].as<uint8_t>() : 0;
  para_Rule.action      = static_cast<RuleAction>(json[F("Action")].as<uint8_t>());
  para_Rule.logic       = static_cast<RuleLogic>(json[F("Logic")].as<uint8_t>());

  // Map boolean strings saved as "on"/"off" back to 1.0/0.0 for evaluation.
  // The raw string is read directly from this file by the template processor at page-serve
  // time, so no extra BSS field is needed here.
  auto readCondFloat = [](const StaticJsonDocument<automation_buffer_size>& doc,
                          const __FlashStringHelper* key) -> float {
    JsonVariantConst v = doc[key];
    if (v.isNull()) return 0.0f;
    if (v.is<const char*>()) {
      const char* s = v.as<const char*>();
      if (strcasecmp(s,"on")==0||strcasecmp(s,"true")==0||strcasecmp(s,"yes")==0)  return 1.0f;
      if (strcasecmp(s,"off")==0||strcasecmp(s,"false")==0||strcasecmp(s,"no")==0) return 0.0f;
    }
    return v.as<float>();
  };

  para_Rule.conditions[0].source = static_cast<ConditionSource>(json[F("C0Source")].as<uint8_t>());
  para_Rule.conditions[0].op     = static_cast<ConditionOperator>(json[F("C0Op")].as<uint8_t>());
  para_Rule.conditions[0].value1 = readCondFloat(json, F("C0Val1"));
  para_Rule.conditions[0].value2 = json[F("C0Val2")].as<float>();

  para_Rule.conditions[1].source = static_cast<ConditionSource>(json[F("C1Source")].as<uint8_t>());
  para_Rule.conditions[1].op     = static_cast<ConditionOperator>(json[F("C1Op")].as<uint8_t>());
  para_Rule.conditions[1].value1 = readCondFloat(json, F("C1Val1"));
  para_Rule.conditions[1].value2 = json[F("C1Val2")].as<float>();

  para_Rule.triggerMode     = static_cast<RuleTriggerMode>(json[F("TriggerMode")].as<uint8_t>());
  para_Rule.timeGateTimerId = json[F("GateTimer")].as<uint8_t>();
  para_Rule.holdSeconds     = json[F("Hold")].as<uint16_t>();
  para_Rule.cooldownSeconds = json[F("Cooldown")].as<uint16_t>();

  configFile.close();
  return SUCCESS;
}

bool saveAutomationRule(AsyncWebServerRequest *request) {
  StaticJsonDocument<automation_buffer_size> json;
  int args = request->args();
  for (int i = 0; i < args; i++) {
    json[request->argName(i)] = request->arg(i);
  }

  request->hasParam("REnabled") ? json["REnabled"] = "1" : json["REnabled"] = "0";

  const char* rnum = json["RNumber"] | "0";
  char rulefilename[20];
  snprintf(rulefilename, sizeof(rulefilename), "/rule%s.json", rnum);

  File configFile = SPIFFS.open(rulefilename, "w");
  if (!configFile) {
    Serial.println(F("[AUTOMAT] Failed to open rule file for writing"));
    return false;
  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[AUTOMAT] Failed to write to file"));
  }
  configFile.print("\n");
  configFile.flush();
  configFile.close();

#ifndef ESP32
  ESP.wdtFeed();
#endif

  int ruleId = atoi(rnum);
  if (ruleId >= 1 && ruleId <= MAX_NUMBER_OF_AUTOMATION_RULES) {
    refreshAutomationRuleCache(ruleId);
  }

  return true;
}

// Runtime-only debounce/cooldown bookkeeping per rule slot — not persisted,
// parallels how Relay keeps lockupdate/hastimerrunning/timerpaused as live
// flags (RelayClass.h:81-84) rather than storing them in JSON.
//
// A rule is *level-based*: the relay should be ON only while its condition(s)
// are true (for ACT_ON — inverted for ACT_OFF), and reverted the moment the
// condition goes false again. So we track the combined condition's stable
// state (debounced by holdSeconds) and fire the corresponding relay command
// exactly once per stable state — `firedForState` blocks repeat firing while
// nothing has changed, and is cleared whenever `result` flips.
typedef struct {
  bool hasResult;
  bool lastResult;
  bool firedForState;
  uint32_t resultSinceMs;
  uint32_t lastFiredMs;
} TRuleRuntimeState;

static TRuleRuntimeState ruleState[MAX_NUMBER_OF_AUTOMATION_RULES + 1];

static void resetRuleConditionState(uint8_t ruleId) {
  ruleState[ruleId].hasResult = false;
  ruleState[ruleId].firedForState = false;
  ruleState[ruleId].resultSinceMs = 0;
  // lastFiredMs is intentionally preserved so a rule that gets disabled and
  // re-enabled (or leaves/re-enters its time gate) still respects cooldown.
}

void evaluateAutomationRules() {
  // Evaluates from the in-memory g_automationRuleCache — no LittleFS access.
  // Cache is populated at boot and refreshed after each saveAutomationRule().
  for (uint8_t ruleId = 1; ruleId <= MAX_NUMBER_OF_AUTOMATION_RULES; ruleId++) {
    const CachedAutomationRule& cr = g_automationRuleCache[ruleId - 1];

    if (!cr.loaded || !cr.enabled) {
      resetRuleConditionState(ruleId);
      continue;
    }

    bool gateOpen = true;
    if (cr.timeGateTimerId >= 1 && cr.timeGateTimerId <= MAX_NUMBER_OF_TIMERS) {
      gateOpen = isNodeTimerActiveNow(cr.timeGateTimerId);
    }

    float v0 = readConditionSensorValue(cr.conditions[0].source);
    float v1 = (cr.conditions[1].source != COND_NONE)
               ? readConditionSensorValue(cr.conditions[1].source)
               : 0.0f;

    bool c0Unavailable = isnan(v0);
    bool c1Unavailable = (cr.conditions[1].source != COND_NONE) && isnan(v1);
    if (c0Unavailable || c1Unavailable) {
      if (ruleState[ruleId].hasResult) {
        char rulefilename[20];
        snprintf(rulefilename, sizeof(rulefilename), "/rule%u.json", (unsigned)ruleId);
        Serial.printf("[AUTOMAT] Rule %u suspended — source unavailable (%s)\n",
                      (unsigned)ruleId, c0Unavailable ? "C0" : "C1");
        disableAutomationRuleInFile(rulefilename);
        g_automationRuleCache[ruleId - 1].enabled = false;
        if (cr.timeGateTimerId >= 1 && cr.timeGateTimerId <= MAX_NUMBER_OF_TIMERS) {
          disableTimerInFile(cr.timeGateTimerId);
          refreshTimerCache(cr.timeGateTimerId);
        }
      }
      resetRuleConditionState(ruleId);
      continue;
    }

    bool result = false;
    if (gateOpen) {
      result = evaluateCondition(cr.conditions[0], v0);
      if (cr.conditions[1].source != COND_NONE) {
        bool c1 = evaluateCondition(cr.conditions[1], v1);
        result = (cr.logic == LOGIC_AND) ? (result && c1) : (result || c1);
      }
    }

    TRuleRuntimeState &st = ruleState[ruleId];
    uint32_t nowMs = millis();

    if (!st.hasResult || result != st.lastResult) {
      st.hasResult = true;
      st.lastResult = result;
      st.resultSinceMs = nowMs;
      st.firedForState = false;
    }

    if (st.firedForState && cr.triggerMode == TRIGGER_ON_CHANGE) continue;

    if ((nowMs - st.resultSinceMs) < (uint32_t)cr.holdSeconds * 1000UL) continue;

    if (st.lastFiredMs != 0 && (nowMs - st.lastFiredMs) < (uint32_t)cr.cooldownSeconds * 1000UL) continue;

    Relay * rly = getrelaybynumber(cr.targetRelay);
    if (rly) {
      uint8_t pin = rly->getRelayPin();
      switch (cr.action) {
        case ACT_ON:     rly->mdigitalWrite(pin, result ? HIGH : LOW); break;
        case ACT_OFF:    rly->mdigitalWrite(pin, result ? LOW : HIGH); break;
        case ACT_TOGGLE: rly->mdigitalWrite(pin, !digitalRead(pin)); break;
      }
      st.lastFiredMs = nowMs;
      if (cr.triggerMode == TRIGGER_ON_CHANGE) st.firedForState = true;
      Serial.printf("[AUTOMAT] Rule %u %s -> relay %u action %u\n",
                    (unsigned)ruleId, result ? "matched" : "cleared",
                    (unsigned)cr.targetRelay, (unsigned)cr.action);
    }
  }
}
