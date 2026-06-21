#pragma once

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>

#define MAX_NUMBER_OF_AUTOMATION_RULES 4

// Live sensor sources that a rule condition can compare against.
// COND_NONE in conditions[1] means the rule only uses conditions[0].
typedef enum {
  COND_NONE,
  COND_CT_CURRENT,
  COND_CT_VOLTAGE,
  COND_CT_POWER,
  COND_HST_CURRENT,
  COND_ADS_VOLT_CH2,
  COND_ADS_VOLT_CH3,
  COND_FLOW_RATE,
  COND_FLOW_TOTAL,
  COND_TEMP1,
  COND_TEMP2,
  COND_PRESSURE_LEVEL,  // appended so existing saved rules keep their numeric values
  // Remote board sensors — subscribed via MQTT and cached in RemoteSensorConfig.
  // Explicit values starting at 20: existing saved rules (0–11) are unaffected
  // even if new local sources are inserted between COND_NONE and COND_PRESSURE_LEVEL
  // in future, as long as their numeric values stay below 20.
  COND_REMOTE_1 = 20,
  COND_REMOTE_2 = 21,
  COND_REMOTE_3 = 22,
  COND_REMOTE_4 = 23,
  COND_REMOTE_5 = 24,
  COND_REMOTE_6 = 25,
  COND_REMOTE_7 = 26,
  COND_REMOTE_8 = 27,
} ConditionSource;

typedef enum { OP_GREATER_THAN, OP_LESS_THAN, OP_BETWEEN, OP_EQUAL = 3 } ConditionOperator;
typedef enum { ACT_ON, ACT_OFF, ACT_TOGGLE } RuleAction;
typedef enum { LOGIC_AND, LOGIC_OR } RuleLogic;
// TRIGGER_ON_CHANGE: fire once per condition state transition (default).
// TRIGGER_PERIODIC:  re-evaluate every second and re-apply the action continuously.
typedef enum { TRIGGER_ON_CHANGE = 0, TRIGGER_PERIODIC = 1 } RuleTriggerMode;

typedef struct TCondition {
  ConditionSource   source;
  ConditionOperator op;
  float value1;
  float value2; // only used when op == OP_BETWEEN (upper bound)
} TCondition;

class AutomationRule
{
  public:
    uint8_t    id;
    boolean    enabled;
    uint8_t    targetRelay;
    RuleAction action;
    RuleLogic  logic;
    TCondition conditions[2];

    RuleTriggerMode triggerMode;   // on-change (default) or periodic re-apply
    uint8_t  timeGateTimerId; // 0 = no time gate, else 1..MAX_NUMBER_OF_TIMERS
    uint16_t holdSeconds;     // condition must stay true this long before firing
    uint16_t cooldownSeconds; // minimum time between consecutive fires

    AutomationRule(uint8_t para_id);
    ~AutomationRule();
};

config_read_error_t loadAutomationRule(char* filename, AutomationRule &para_Rule);
bool saveAutomationRule(AsyncWebServerRequest *request);

// Flat (no-pointer) snapshot of an automation rule — safe to copy and store
// in a global array. Populated once at boot and refreshed after every save.
struct CachedAutomationRule {
  bool           loaded;        // false = slot not configured (file absent)
  bool           enabled;
  uint8_t        targetRelay;
  RuleAction     action;
  RuleLogic      logic;
  TCondition     conditions[2];
  RuleTriggerMode triggerMode;
  uint8_t        timeGateTimerId;
  uint16_t       holdSeconds;
  uint16_t       cooldownSeconds;
};
extern CachedAutomationRule g_automationRuleCache[MAX_NUMBER_OF_AUTOMATION_RULES];

// Populate one cache slot (1-based index) by reading the flash file once.
void refreshAutomationRuleCache(int ruleIdx);
// Populate all slots — call once at boot after filesystem is mounted.
void refreshAllAutomationRuleCache();

// Reads the live value for a given sensor source from the existing global sensor state.
float readConditionSensorValue(ConditionSource source);

// Applies the condition's operator/threshold(s) to a live value.
bool evaluateCondition(const TCondition &cond, float liveValue);

// Evaluates all automation rules against in-memory cache — no LittleFS access.
void evaluateAutomationRules();
