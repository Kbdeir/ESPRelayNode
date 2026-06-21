#ifndef ESP32
  #include <arduino_homekit_server.h>
#endif
#include <AsyncHTTP_Helper.h>
#include <vector>
#include <memory>
#include <ArduinoJson.h>
#include <FirmwareUpdateState.h>
#include <MQTT_Processes.h>
#include <RelayClass.h>
#include <TimerClass.h>
#ifdef _AUTOMATION_RULES_
#include <AutomationRules.h>
#endif
#ifdef _REMOTE_SENSORS_
#include <RemoteSensorConfig.h>
#endif
#include <TempConfig.h>
#include <ADS11x5Config.h>
#include <HSTConfig.h>
#include <WireGuardConfig.h>
#include <WireGuardManager.h>
#include <TLPressureSensor.h>
#include <AccelStepper.h>
#include <digitalClockDisplay.h>
#include <ConfigParams.h>
#include <KSBAsyncNTP.h>
#include <SerialLog.h>
#include <WaterFlowSensor.h>
#ifdef ESP32
#include <WiFi.h>
#include "esp_core_dump.h"
#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "esp_ota_ops.h"
// Cached heapMaxBlk sampled from loop() without active TCP buffers (see main.cpp).
extern uint32_t g_heapMaxBlkCached;
#else
#include <ESP8266WiFi.h>
#endif
#ifdef _emonlib_
#include <CT_ProcessPower.h>
#endif

#ifdef ESP32
  #ifdef AppleHK
    #include <ESP32HAP.h>
  #endif 
#endif

#ifdef _ESP_ALEXA_
  #define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
  #include <Espalexa.h>
  extern Espalexa espalexa; 
#endif
#ifdef _ALEXA_
#include "fauxmoESP.h"
extern fauxmoESP fauxmo;
#endif

extern boolean CalendarNotInitiated ;
extern NodeTimer NTmr;
#ifdef _AUTOMATION_RULES_
extern AutomationRule ARule;
#endif
extern TempConfig PTempConfig;
#ifdef _ADS1X15_
extern ADS11x5Config PADS11x5Config;
extern float g_adsV2, g_adsV3, g_adsMultiplier;
#ifdef _ADS1X15_DC_Current_
extern float g_adsFinalRMSCurrent, g_adsFinalDCCurrent;
#endif
#endif
#ifdef _HST_
extern HSTConfig PAHSTConfig;
#endif
#ifdef _pressureSensor_
extern TLPressureSensor TL136;
#endif 
extern float MCelcius;
extern float MCelcius2;
extern float ACS_I_Current;
extern bool homekitNotInitialised;
#ifdef _emonlib_
extern CTPROCESSOR CT_1;
#endif

extern void setupInputs();
extern void clearIRMap();
#if defined (_emonlib_) || defined (_pressureSensor_)
extern void applyPowerTaskIntervals();
#endif
#ifdef _emonlib_
extern void applyCTCalibration();
#endif
#include <InputClass.h>
extern std::vector<void *> inputs;
extern std::vector<void *> relays;
extern const char* IRMapfilename;
#ifdef OLED_1306
extern void applyOledTaskInterval();
#endif

extern bool webing;

static volatile bool firmwareOtaActive = false;
static volatile bool firmwareOtaAccepted = false;
static volatile bool firmwareOtaWriteFailed = false;
static uint32_t firmwareOtaStartedMs = 0;

static StoredWireGuardConfig PWireGuardConfig;

static volatile bool wifiScanRequested = false;
static bool wifiScanRunning = false;
static String wifiScanPayload = "{\"scanning\":false,\"count\":0,\"networks\":[]}";


static void buildWifiScanPayload(int networkCount, bool scanning)
{
  DynamicJsonDocument json(256 + (networkCount > 0 ? networkCount : 0) * 160);
  JsonArray networks = json.createNestedArray("networks");
  json["scanning"] = scanning;
  json["count"] = networkCount > 0 ? networkCount : 0;

  for (int i = 0; i < networkCount; ++i) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
  }

  String payload;
  serializeJson(json, payload);
  wifiScanPayload = payload;
}


// Shared by the /api/files/* routes (the SPIFFSEditor replacement below):
// every path comes straight from a client-supplied query/form parameter, so
// reject anything that isn't an absolute path or that could escape the
// filesystem root via "..".
static bool isValidFilesApiPath(const String &path) {
  return path.length() > 1 && path.startsWith("/") && path.indexOf("..") < 0;
}

// Rewrites a timer/rule JSON file in place with `key` forced to "0", leaving
// every other field untouched. Used by /api/resetconfig to disable existing
// timer and automation-rule schedules without erasing their configured
// times/conditions. No-op if the file doesn't exist or fails to parse.
static void disableJsonEnableFlag(const char* filename, const char* key) {
  if (!SPIFFS.exists(filename)) return;

  StaticJsonDocument<1024> json;
  {
    File f = SPIFFS.open(filename, "r");
    if (!f) return;
    DeserializationError error = deserializeJson(json, f);
    f.close();
    if (error) return;
  }

  json[key] = "0";

  File f = SPIFFS.open(filename, "w");
  if (!f) return;
  serializeJsonPretty(json, f);
  f.print("\n");
  f.flush();
  f.close();
}

extern std::vector<void *> relays ; // a list to hold all relays
//extern  int currentPosition;
//extern  int newPosition;  

#ifdef StepperMode
extern  AccelStepper shadeStepper;
extern bool steperrun;
#endif

uint8_t AppliedRelayNumber = 0;

// const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
// AsyncWebServer AsyncWeb_server(80);
#if defined _ESP_ALEXA_
AsyncWebServer AsyncWeb_server(80);
#else
// AsyncWebServer AsyncWeb_server(8080);
AsyncWebServer AsyncWeb_server(80);
#endif


// AsyncEventSource events("/events"); // https://randomnerdtutorials.com/esp32-web-server-sent-events-sse/
// AsyncWebSocket ws("/test");


//File cf;


String timerprocessor(const String& var)
{
  if(var == F( "systemtime" ))          return digitalClockDisplay();
  if(var == F( "uptime" ))              return uptimeDisplay();
  if(var == F( "heap" ))                return String(ESP.getFreeHeap());
  if(var == F( "TNBT" ))                return  String(NTmr.id);
  if(var == F( "TRelay" ))              return  String(NTmr.relay);
  if(var == F( "Dfrom" ))               return  String(NTmr.spanDatefrom.c_str());
  if(var == F( "DTo" ))                 return String( NTmr.spanDateto.c_str());
  if(var == F( "TFrom" ))               return  String(NTmr.spantimefrom.c_str());
  if(var == F( "TTo" ))                 return String( NTmr.spantimeto.c_str());
  if(var == F( "CMonday" ))             { if (NTmr.weekdays && NTmr.weekdays->Monday)    return "1\" checked=\"\""; };
  if(var == F( "CTuesday" ))            { if (NTmr.weekdays && NTmr.weekdays->Tuesday )  return "1\" checked=\"\""; };
  if(var == F( "CWednesday" ))          { if (NTmr.weekdays && NTmr.weekdays->Wednesday) return "1\" checked=\"\""; };
  if(var == F( "CThursday" ))           { if (NTmr.weekdays && NTmr.weekdays->Thursday ) return "1\" checked=\"\""; };
  if(var == F( "CFriday" ))             { if (NTmr.weekdays && NTmr.weekdays->Friday )   return "1\" checked=\"\""; };
  if(var == F( "CSaturday" ))           { if (NTmr.weekdays && NTmr.weekdays->Saturday ) return "1\" checked=\"\""; };
  if(var == F( "CSunday" ))             { if (NTmr.weekdays && NTmr.weekdays->Sunday )   return "1\" checked=\"\""; };
  if(var == F( "CEnabled" ))            { if (NTmr.enabled)             return "1\" checked=\"\""; };
  if(var == F( "Mark_Hours" ))          return String(NTmr.Mark_Hours);
  if(var == F( "Mark_Minutes" ))        return String(NTmr.Mark_Minutes);
  if(var == F( "MONTHDAY" ))            return String(NTmr.monthDay ? NTmr.monthDay : 1);
  if(var == F( "TMTYPEedit" ))          return String(NTmr.TM_type);

  if(var == F( "TIMER_RELAY_OPTIONS" )) {
    String opts;
    opts.reserve(relays.size() * 48 + 64);
    opts += F("<option value=\"");
    opts += String((unsigned)TIMER_NULL_RELAY);
    opts += F("\"");
    if (NTmr.relay == TIMER_NULL_RELAY) opts += F(" selected");
    opts += F(">(none &mdash; schedule window only)</option>");
    for (size_t i = 0; i < relays.size(); i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if ((int)i == NTmr.relay) opts += F(" selected");
      opts += F(">Relay ");
      opts += String(i);
      opts += F("</option>");
    }
    return opts;
  }
  /*if(var == F( "Testchar" ))            return [](){
        String s = String(NTmr.Testchar);
        s.replace('%', '-');
        return s;
      }();*/


  if(var == F( "TSTATE" ))              return
      [](){
        if (getrelaybynumber(NTmr.relay) != nullptr) {
          return (getrelaybynumber(NTmr.relay)->timerpaused == 1) ? "PAUSED" : "NOT PAUSED";
        } else return "NA";
      }();

  if(var == F( "RSTATE" ))              return
      [](){
        if (getrelaybynumber(NTmr.relay) != nullptr) {
          return (getrelaybynumber(NTmr.relay)->readrelay() == HIGH) ? "ON" : "OFF";
        } else return "NA";
      }();

  return String();
}

#ifdef _AUTOMATION_RULES_
static void automationAppendSourceOption(String &opts, ConditionSource value, const char* label, ConditionSource selected) {
  opts += F("<option value=\"");
  opts += String((uint8_t)value);
  opts += F("\"");
  if (value == selected) opts += F(" selected");
  opts += F(">");
  opts += label;
  opts += F("</option>");
}

// Only offers sources whose backing feature is actually compiled in (per defines.h),
// matching the same #ifdef guards readConditionSensorValue() uses in AutomationRules.cpp
// — otherwise a saved rule could reference a sensor that always reads as 0.0f.
static String automationConditionSourceOptions(ConditionSource selected) {
  String opts;
  opts.reserve(1024);  // pre-alloc prevents realloc chains that fragment heap
  automationAppendSourceOption(opts, COND_NONE, "(none)", selected);
#ifdef _emonlib_
  automationAppendSourceOption(opts, COND_CT_CURRENT, "CT Current (A)", selected);
  automationAppendSourceOption(opts, COND_CT_VOLTAGE, "CT Voltage (V)", selected);
  automationAppendSourceOption(opts, COND_CT_POWER,   "CT Power (W)",   selected);
#endif
#if defined(_ADS1X15_) && defined(_ADS1X15_DC_Current_)
  automationAppendSourceOption(opts, COND_HST_CURRENT, "HST Current (A)", selected);
#endif
#ifdef _ADS1X15_
  automationAppendSourceOption(opts, COND_ADS_VOLT_CH2, "AIN2 Scaled Voltage (V)", selected);
  automationAppendSourceOption(opts, COND_ADS_VOLT_CH3, "AIN3 Scaled Voltage (V)", selected);
#endif
#ifdef WaterFlowSensor
  automationAppendSourceOption(opts, COND_FLOW_RATE,  "Flow Rate (L/min)", selected);
  automationAppendSourceOption(opts, COND_FLOW_TOTAL, "Flow Total (L)",    selected);
#endif
  automationAppendSourceOption(opts, COND_TEMP1, "Temperature 1 (C)", selected);
  automationAppendSourceOption(opts, COND_TEMP2, "Temperature 2 (C)", selected);
#ifdef _pressureSensor_
  automationAppendSourceOption(opts, COND_PRESSURE_LEVEL, "Pressure/Level Sensor (cm)", selected);
#endif
#ifdef _REMOTE_SENSORS_
  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    if (remoteSensors[i].topic[0] == '\0') continue;
    ConditionSource src = (ConditionSource)((uint8_t)COND_REMOTE_1 + i);
    char lbl[56];
    // Use the user-supplied label if set, otherwise fall back to the topic itself.
    const char* name = (remoteSensors[i].label[0] != '\0')
                       ? remoteSensors[i].label
                       : remoteSensors[i].topic;
    snprintf(lbl, sizeof(lbl), "Remote %u: %s", (unsigned)(i + 1), name);
    automationAppendSourceOption(opts, src, lbl, selected);
  }
#endif
  return opts;
}

static String automationConditionOperatorOptions(ConditionOperator selected) {
  struct { ConditionOperator value; const char* label; } ops[] = {
    { OP_GREATER_THAN, "&gt; (greater than)" },
    { OP_LESS_THAN,    "&lt; (less than)" },
    { OP_BETWEEN,      "between" },
    { OP_EQUAL,        "= (equal to)" },
  };
  String opts;
  opts.reserve(256);  // pre-alloc prevents realloc chains that fragment heap
  for (auto &o : ops) {
    opts += F("<option value=\"");
    opts += String((uint8_t)o.value);
    opts += F("\"");
    if (o.value == selected) opts += F(" selected");
    opts += F(">");
    opts += o.label;
    opts += F("</option>");
  }
  return opts;
}

// Renders a short human-readable description of timer N's stored schedule
// (date span / weekdays + time-of-day window), reusing loadNodeTimer the same
// way isNodeTimerActiveNow() does (TimerClass.cpp:66-77) — so the Automation
// page can show what a "gate timer" choice actually means without the user
// having to flip over to the Timer page to check.
#ifdef _AUTOMATION_RULES_
static String automationGateTimerSummaryText(uint8_t timerId) {
  if (timerId < 1 || timerId > MAX_NUMBER_OF_TIMERS) {
    return F("No time gate selected &mdash; the rule is evaluated continuously.");
  }

  static NodeTimer SummaryTmr(timerId);
  char timerfilename[20];
  snprintf(timerfilename, sizeof(timerfilename), "/timer%u.json", (unsigned)timerId);

  if (loadNodeTimer(timerfilename, SummaryTmr) != SUCCESS) {
    return F("This timer slot is not configured yet.");
  }
  if (!SummaryTmr.enabled) {
    return F("This timer is currently disabled, so the rule will never fire while gated to it.");
  }

  String when;
  if (SummaryTmr.TM_type == TM_WEEKDAY_SPAN) {
    if (SummaryTmr.weekdays && SummaryTmr.weekdays->AllWeek) {
      when = F("Every day");
    } else if (SummaryTmr.weekdays) {
      const char* names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
      bool flags[7] = { SummaryTmr.weekdays->Sunday,    SummaryTmr.weekdays->Monday,
                        SummaryTmr.weekdays->Tuesday,   SummaryTmr.weekdays->Wednesday,
                        SummaryTmr.weekdays->Thursday,  SummaryTmr.weekdays->Friday,
                        SummaryTmr.weekdays->Saturday };
      for (uint8_t d = 0; d < 7; d++) {
        if (flags[d]) {
          if (when.length()) when += F(", ");
          when += names[d];
        }
      }
      if (!when.length()) when = F("(no weekdays selected)");
    }
  } else {
    when = SummaryTmr.spanDatefrom + F(" &rarr; ") + SummaryTmr.spanDateto;
  }

  // Duration mode (the only mode for TM_WEEKDAY_SPAN) computes the end from
  // start+duration — `spantimeto` is a stale/unused field in that case.
  // Mirrors TimerStatus.json's "effTimeTo" computation (AsyncHTTP_Helper.cpp:1512-1517)
  // and isNodeTimerActiveNow()'s effective-window math (TimerClass.cpp:115-127),
  // so the summary shown here matches what actually gates the rule.
  String timeTo = SummaryTmr.spantimeto;
  if (SummaryTmr.Mark_Hours + SummaryTmr.Mark_Minutes > 0) {
    int fromH = 0, fromM = 0;
    sscanf(SummaryTmr.spantimefrom.c_str(), "%d:%d", &fromH, &fromM);
    int toSecs = (fromH * 3600 + fromM * 60) + (int)SummaryTmr.Mark_Hours * 3600 + (int)SummaryTmr.Mark_Minutes * 60;
    char effTimeTo[6];
    snprintf(effTimeTo, sizeof(effTimeTo), "%02d:%02d", (toSecs % 86400) / 3600, (toSecs % 3600) / 60);
    timeTo = effTimeTo;
  }

  return when + F(" &middot; ") + SummaryTmr.spantimefrom + F("&ndash;") + timeTo;
}

// Pre-renders every gate-timer choice's summary as a JS string array so the
// browser can swap the displayed text instantly when the dropdown changes,
// without a round trip to the device for each selection.
static String automationGateTimerSummariesArray() {
  String js = F("[\"");
  String none = automationGateTimerSummaryText(0);
  none.replace("\\", "\\\\");
  none.replace("\"", "\\\"");
  js += none;
  js += F("\"");
  for (uint8_t i = 1; i <= MAX_NUMBER_OF_TIMERS; i++) {
    String s = automationGateTimerSummaryText(i);
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    js += F(",\"");
    js += s;
    js += F("\"");
  }
  js += F("]");
  return js;
}
#endif

String automationprocessor(const String& var)
{
  if(var == F( "systemtime" ))   return digitalClockDisplay();
  if(var == F( "uptime" ))       return uptimeDisplay();
  if(var == F( "heap" ))         return String(ESP.getFreeHeap());
  if(var == F( "RNBT" ))         return String(ARule.id);
  if(var == F( "REnabled" ))     { if (ARule.enabled) return "1\" checked=\"\""; };
  if(var == F( "RHold" ))        return String(ARule.holdSeconds);
  if(var == F( "RCooldown" ))    return String(ARule.cooldownSeconds);
  if(var == F( "RC0Val1" ) || var == F( "RC1Val1" )) {
    // Re-read the rule JSON to get the raw string ("on"/"off" vs numeric).
    // StaticJsonDocument<1024> is stack-only — zero BSS, zero heap cost. Page-load only.
    char _fname[20];
    snprintf(_fname, sizeof(_fname), "/rule%u.json", (unsigned)ARule.id);
    float _fb = (var == F("RC0Val1")) ? ARule.conditions[0].value1 : ARule.conditions[1].value1;
    File _f = SPIFFS.open(_fname, "r");
    if (!_f) return String(_fb, 2);
    StaticJsonDocument<1024> _doc;
    DeserializationError _err = deserializeJson(_doc, _f);
    _f.close();
    if (_err) return String(_fb, 2);
    const char* _rk = (var == F("RC0Val1")) ? "C0Val1" : "C1Val1";
    JsonVariantConst _v = _doc[_rk];
    if (!_v.isNull() && _v.is<const char*>()) { const char* _s = _v.as<const char*>(); if (_s[0]) return String(_s); }
    return String(_fb, 2);
  }
  if(var == F( "RC0Val2" ))      return String(ARule.conditions[0].value2, 2);
  if(var == F( "RC1Val2" ))      return String(ARule.conditions[1].value2, 2);

  if(var == F( "RTARGET_RELAY_OPTIONS" )) {
    String opts;
    opts.reserve(relays.size() * 48 + 8);
    for (size_t i = 0; i < relays.size(); i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if ((int)i == ARule.targetRelay) opts += F(" selected");
      opts += F(">Relay ");
      opts += String(i);
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RACTION_OPTIONS" )) {
    const char* labels[3] = { "Turn ON", "Turn OFF", "Toggle" };
    String opts;
    opts.reserve(192);
    for (uint8_t i = 0; i < 3; i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if (i == (uint8_t)ARule.action) opts += F(" selected");
      opts += F(">");
      opts += labels[i];
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RLOGIC_OPTIONS" )) {
    const char* labels[2] = { "AND", "OR" };
    String opts;
    opts.reserve(128);
    for (uint8_t i = 0; i < 2; i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if (i == (uint8_t)ARule.logic) opts += F(" selected");
      opts += F(">");
      opts += labels[i];
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RTRIGGERMODE_OPTIONS" )) {
    const char* labels[2] = { "On change (fire once per transition)", "Periodic (re-apply every second)" };
    String opts;
    opts.reserve(160);
    for (uint8_t i = 0; i < 2; i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if (i == (uint8_t)ARule.triggerMode) opts += F(" selected");
      opts += F(">");
      opts += labels[i];
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RGATE_TIMER_OPTIONS" )) {
    String opts;
    opts.reserve(MAX_NUMBER_OF_TIMERS * 52 + 32);
    opts += F("<option value=\"0\"");
    if (ARule.timeGateTimerId == 0) opts += F(" selected");
    opts += F(">None</option>");
    for (uint8_t i = 1; i <= MAX_NUMBER_OF_TIMERS; i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if (i == ARule.timeGateTimerId) opts += F(" selected");
      opts += F(">Timer ");
      opts += String(i);
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RGATE_TIMER_SUMMARY" ))  return automationGateTimerSummaryText(ARule.timeGateTimerId);
  if(var == F( "RGATE_TIMER_SUMMARIES" )) return automationGateTimerSummariesArray();

  if(var == F( "RC0_SOURCE_OPTIONS" )) return automationConditionSourceOptions(ARule.conditions[0].source);
  if(var == F( "RC1_SOURCE_OPTIONS" )) return automationConditionSourceOptions(ARule.conditions[1].source);
  if(var == F( "RC0_OP_OPTIONS" ))     return automationConditionOperatorOptions(ARule.conditions[0].op);
  if(var == F( "RC1_OP_OPTIONS" ))     return automationConditionOperatorOptions(ARule.conditions[1].op);

  return String();
}
#endif

#ifdef _REMOTE_SENSORS_
// Builds the 8 sensor-slot rows as an HTML string substituted into %RS_ROWS%.
// Kept server-side so the HTML file stays free of C-style per-slot repetition
// and avoids 24 individual template vars.
static String remoteSensorsRows() {
  String html;
  html.reserve(3200);
  for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
    uint8_t n = i + 1;
    char ns[3]; snprintf(ns, sizeof(ns), "%u", (unsigned)n);

    html += F("<div class=\"subhead\">Slot ");
    html += ns;
    html += F("</div><div class=\"form-grid rs-grid\">");

    // Label
    html += F("<div class=\"field\"><label>Label</label>"
              "<input type=\"text\" name=\"RS");
    html += ns;
    html += F("_LABEL\" value=\"");
    html += String(remoteSensors[i].label);
    html += F("\" maxlength=\"31\" placeholder=\"e.g. Board B Power\"></div>");

    // MQTT Topic
    html += F("<div class=\"field\"><label>MQTT Topic</label>"
              "<input type=\"text\" name=\"RS");
    html += ns;
    html += F("_TOPIC\" value=\"");
    html += String(remoteSensors[i].topic);
    html += F("\" maxlength=\"63\" placeholder=\"e.g. home/boardB/sensors\"></div>");

    // JSON Key + Parse JSON button
    html += F("<div class=\"field\"><label>JSON Key "
              "<span class=\"hint\">(blank&nbsp;=&nbsp;raw&nbsp;float)</span></label>"
              "<input type=\"text\" id=\"rs");
    html += ns;
    html += F("_key\" name=\"RS");
    html += ns;
    html += F("_KEY\" value=\"");
    html += String(remoteSensors[i].jsonKey);
    html += F("\" maxlength=\"31\" placeholder=\"e.g. realPower\">"
              "<button type=\"button\" class=\"secondary parse-btn\" onclick=\"parsePayload(");
    html += ns;
    html += F(")\">Parse JSON</button></div>");

    // Live value
    html += F("<div class=\"field\"><label>Live Value</label>"
              "<div class=\"check-row\" style=\"font-family:monospace;gap:14px\">"
              "<span id=\"rs");
    html += ns;
    html += F("_v\" style=\"font-weight:700\">--</span>"
              "<span class=\"hint\" id=\"rs");
    html += ns;
    html += F("_a\"></span></div></div>");

    html += F("</div>"); // close form-grid
  }
  return html;
}

String remoteSensorsProcessor(const String& var) {
  if (var == F("systemtime")) return digitalClockDisplay();
  if (var == F("uptime"))     return uptimeDisplay();
  if (var == F("heap"))       return String(ESP.getFreeHeap());
  if (var == F("RS_ROWS"))    return remoteSensorsRows();
  return String();
}
#endif


String TempConfProcessor(const String& var)
{

  if(var == F( "TNBT" ))                return String(PTempConfig.id);
  if(var == F( "TRelay" ))              return String(PTempConfig.relay);
  if(var == F( "spanTempfrom" ))        return String(PTempConfig.spanTempfrom);
  if(var == F( "spanTempto" ))          return String(PTempConfig.spanTempto);
  if(var == F( "spanBuffer" ))          return String(PTempConfig.spanBuffer);  
  if(var == F( "CEnabled" ))            { if (PTempConfig.enabled)             return "1\" checked=\"\""; };
  if(var == F( "RelayMaxNB" ))          return String((int)relays.size() - 1);
  if(var == F( "RSTATE" ))             {
    Relay* r = getrelaybynumber(PTempConfig.relay);
    if (r) return (r->readrelay() == HIGH) ? "ON" : "OFF";
    return "NA";
  };

  return String();
}

#ifdef _ADS1X15_
String ADS11x5ConfProcessor(const String& var)
{

  if(var == F( "ADSNB" ))               return String(PADS11x5Config.id);
  if(var == F( "TRelay1" ))             return String(PADS11x5Config.relay1);
  if(var == F( "TRelay2" ))             return String(PADS11x5Config.relay2);
  if(var == F( "Res1" ))                return String(PADS11x5Config.Res1);
  if(var == F( "Res2" ))                return String(PADS11x5Config.Res2);
  if(var == F( "ResMultiplier" ))       return String(PADS11x5Config.ResMultiplier, 6);
  if(var == F( "CEnabled" ))            { if (PADS11x5Config.enabled)             return "1\" checked=\"\""; };

  return String();
}
#endif

#ifdef _HST_
String HSTConfProcessor(const String& var)
{

  if(var == F( "HSTNB" ))               return String(PAHSTConfig.id);
  if(var == F( "AmpsVoltsRatio" ))      return String(PAHSTConfig.AmpsVoltsRatio);
  if(var == F( "manualOffset" ))        return String(PAHSTConfig.manualOffset, 4);
  if(var == F( "CEnabled" ))            { if (PAHSTConfig.enabled)   return "1\" checked=\"\""; };
  if(var == F( "systemtime" ))          return digitalClockDisplay();

  return String();
}
#endif

String WireGuardConfProcessor(const String& var)
{
  if(var == F( "VPNEnabled" ))          { if (PWireGuardConfig.enabled) return "1\" checked=\"\""; };
  if(var == F( "VPNProtocol" ))         return PWireGuardConfig.protocol;
  if(var == F( "WGPrivateKey" ))        return PWireGuardConfig.privateKey;
  if(var == F( "WGLocalIP" ))           return PWireGuardConfig.localIp;
  if(var == F( "WGPeerPublicKey" ))     return PWireGuardConfig.peerPublicKey;
  if(var == F( "WGPeerEndpoint" ))      return PWireGuardConfig.peerEndpoint;
  if(var == F( "WGAllowedIPs" ))        return PWireGuardConfig.allowedIps;
  if(var == F( "WGDnsServers" ))        return PWireGuardConfig.dnsServers;
  if(var == F( "WGDnsLivenessCheck" ))  { if (PWireGuardConfig.dnsLivenessCheck) return "1\" checked=\"\""; };
  if(var == F( "WGDnsLivenessInterval" )) return String(PWireGuardConfig.dnsLivenessInterval);

  return String();
}


#ifdef _pressureSensor_
String TLProcessor(const String& var)
{
  if(var == F( "maSTopic" ))         return String(TL136.maSTopic);
  if(var == F( "paramEmptyValue" ))  return String(TL136.paramEmptyValue);
  if(var == F( "paramFullValue" ))   return String(TL136.paramFullValue);
  if(var == F( "maSLC" ))            return String(TL136.maSLC);
  if(var == F( "maSHC" ))            return String(TL136.maSHC);
  if(var == F( "maBurdenResistor" )) return String(TL136.BurdenResistorValue);
  if(var == F( "systemtime" ))       return digitalClockDisplay();
  if(var == F( "ma_ADC" ))           return String(TL136.sampleI, 1);
  return String();
}
#endif

#ifdef WaterFlowSensor
// WFSProcessor removed — WaterFlowSensor.html now fetches config via /api/wfsconfig
#endif



String processor(const String& var)
{
  Relay * AppliedRelay = getrelaybynumber(AppliedRelayNumber);

  // Serial.println(String("AppliedRelayNumber = ") + AppliedRelayNumber + " \n");

  if(var == F( "NODEID" ))              return CID();
  if(var == F( "MACADDR" ))            { String cid = CID(); char buf[64]; snprintf(buf, sizeof(buf), "%s - Chip id: %s", MAC.c_str(), cid.c_str()); return String(buf); }
  if(var == F( "ssid" ))                return  String(MyConfParam.v_ssid.c_str());
  if(var == F( "pass" ))                return String( MyConfParam.v_pass.c_str());

  if(var == F( "mqttUser" ))            return  String(MyConfParam.v_mqttUser.c_str());
  if(var == F( "mqttPass" ))            return String( MyConfParam.v_mqttPass.c_str());

  if(var == F( "RWD" ))   return String( MyConfParam.v_Reboot_on_WIFI_Disconnection);
  if(var == F( "PRST" ))  return String( MyConfParam.v_PRST);        

  if(var == F( "PhyLoc" ))              return String( MyConfParam.v_PhyLoc.c_str());
  if(var == F( "MQTT_BROKER" ))         return String(MyConfParam.v_MQTT_BROKER.c_str()) ; // MyConfParam.v_MQTT_BROKER.toString();
  if(var == F( "MQTT_B_PRT" ))          return String( MyConfParam.v_MQTT_B_PRT);
  if(var == F( "MQTT_KeepAliveSeconds" )) return String( MyConfParam.v_MQTT_KeepAliveSeconds);
  if(var == F( "MQTT_STATUS" ))         return mqttStatusText();
  if(var == F( "MQTT_CONNECTED_SERVER" )) return MyConfParam.v_MQTT_BROKER + String(F(":")) + String(MyConfParam.v_MQTT_B_PRT);
  if(var == F( "MQTT_HEALTH_TOPIC" )) return mqttHealthTopic();
  if(var == F( "MQTT_ROUTE" )) return mqttRouteText();

  if (AppliedRelay != nullptr) {
    if(var == F( "ACS_elasticity" ))    return String( AppliedRelay->RelayConfParam->v_ACS_elasticity); 
    if(var == F( "RELAYNB" ))             return String( AppliedRelay->RelayConfParam->v_relaynb);
    if(var == F( "PUB_TOPIC1" ))          return String( AppliedRelay->RelayConfParam->v_PUB_TOPIC1.c_str());
    if(var == F( "TemperatureValue" ))    return String( AppliedRelay->RelayConfParam->v_TemperatureValue.c_str());

    if(var == F("HasTempRelay") || var == F("HasOtherTempRelay")) {
      bool hasAny = false, hasOther = false;
      for (auto it : relays) {
        Relay *r = static_cast<Relay*>(it);
        if (!r || !r->RelayConfParam) continue;
        const String& tv = r->RelayConfParam->v_TemperatureValue;
        if (tv.length() > 0 && tv != "0") {
          hasAny = true;
          if (r != AppliedRelay) hasOther = true;
        }
      }
      return (var == F("HasOtherTempRelay"))
               ? (hasOther ? F("1") : F("0"))
               : (hasAny   ? F("1") : F("0"));
    }

    if(var == F( "AlexaName" ))    return String( AppliedRelay->RelayConfParam->v_AlexaName.c_str());

    if(var == F( "ACS_Sensor_Model" ))    return String( AppliedRelay->RelayConfParam->v_ACS_Sensor_Model.c_str());
    if(var == F( "ttl" ))                 return String( AppliedRelay->RelayConfParam->v_ttl);
    if(var == F( "STATE_PUB_TOPIC" ))     return String( AppliedRelay->RelayConfParam->v_STATE_PUB_TOPIC.c_str());
    if(var == F( "TTL_PUB_TOPIC" ))       return String( AppliedRelay->RelayConfParam->v_ttl_PUB_TOPIC.c_str());
    if(var == F( "CURR_TTL_PUB_TOPIC" ))  return String( AppliedRelay->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str());
    if(var == F( "i_ttl_PUB_TOPIC" ))     return String( AppliedRelay->RelayConfParam->v_i_ttl_PUB_TOPIC.c_str());
    if(var == F( "ACS_AMPS" ))            return String( AppliedRelay->RelayConfParam->v_ACS_AMPS.c_str());
    if(var == F( "tta" ))                 return String( AppliedRelay->RelayConfParam->v_tta);
    if(var == F( "Max_Current" ))         return String( AppliedRelay->RelayConfParam->v_Max_Current);
    if(var == F( "LWILL_TOPIC" ))         return String( AppliedRelay->RelayConfParam->v_LWILL_TOPIC.c_str());
    if(var == F( "SUB_TOPIC1" ))          return String( AppliedRelay->RelayConfParam->v_SUB_TOPIC1.c_str());
    if(var == F( "ACS_Active" ))          { if (AppliedRelay->RelayConfParam->v_ACS_Active) return "1\" checked=\"\""; }; 
  }

  if(var == F( "FRM_IP" ))             return MyConfParam.v_FRM_IP.toString();
  if(var == F( "FRM_PRT" ))            return String( MyConfParam.v_FRM_PRT);
  if(var == F( "WIFI_IP" ))            return WiFi.localIP().toString();
  if(var == F( "WIFI_GATEWAY" ))       return WiFi.gatewayIP().toString();
  if(var == F( "WIFI_DNS1" ))          return WiFi.dnsIP(0).toString();
  if(var == F( "WIFI_DNS2" ))          return WiFi.dnsIP(1).toString();
  if(var == F( "WIFI_STATUS" ))        return (WiFi.status() == WL_CONNECTED) ? String(F("connected")) : String(F("disconnected"));

  if(var == F( "I12_STS_PTP"))         return String( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str());
  if(var == F( "I14_STS_PTP"))         return String( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str());

  #ifdef ESP32
    if(var == F( "I01_STS_PTP"))       return String( MyConfParam.v_InputPin01_STATE_PUB_TOPIC.c_str());
    if(var == F( "I02_STS_PTP"))       return String( MyConfParam.v_InputPin02_STATE_PUB_TOPIC.c_str());
  #endif

  if(var == F( "I03_STS_PTP"))          return String( MyConfParam.v_InputPin03_STATE_PUB_TOPIC.c_str());
  if(var == F( "I04_STS_PTP"))          return String( MyConfParam.v_InputPin04_STATE_PUB_TOPIC.c_str());
  if(var == F( "I05_STS_PTP"))          return String( MyConfParam.v_InputPin05_STATE_PUB_TOPIC.c_str());
  if(var == F( "I06_STS_PTP"))          return String( MyConfParam.v_InputPin06_STATE_PUB_TOPIC.c_str()); 

  if(var == F( "timeserver" ))          return MyConfParam.v_timeserver ;
  if(var == F( "NTP_UseVPN"))           { if (MyConfParam.v_NTP_UseVPN) return "1\" checked=\"\""; };
  if(var == F( "Pingserver" ))          return MyConfParam.v_Pingserver.toString() ;
  if(var == F( "ntptz" ))               return String( MyConfParam.v_ntptz);
  if(var == F( "MQTT_Active"))         { if (MyConfParam.v_MQTT_Active) return "1\" checked=\"\""; };
  if(var == F( "MQTT_UseVPN"))         { if (MyConfParam.v_MQTT_UseVPN) return "1\" checked=\"\""; };
  if(var == F( "Update_now" ))         { if (MyConfParam.v_Update_now)  return "1\" checked=\"\""; };
  if(var == F( "systemtime" ))          return digitalClockDisplay();
  if(var == F( "uptime" ))              return uptimeDisplay();
  if(var == F( "heap" ))                return String(ESP.getFreeHeap());

  if(var == F( "ATEMP" ))                 return String(MCelcius, 2);
  if(var == F( "ATEMP1" ))                return String(MCelcius, 2);
  if(var == F( "ATEMP2" ))                return String(MCelcius2, 2);

  if(var == F( "ACS" ))                   return String(ACS_I_Current, 2);
  if(var == F( "ACS1" ))                  {
    Relay *_r0 = getrelaybynumber(0);
    return (_r0 && _r0->RelayConfParam->v_ACS_Active) ? String(ACS_I_Current, 2) : String(F("0.00"));
  }
  if(var == F( "ACS2" ))                  {
    Relay *_r1 = getrelaybynumber(1);
    return (_r1 && _r1->RelayConfParam->v_ACS_Active) ? String(ACS_I_Current, 2) : String(F("0.00"));
  }
  //String(MCelcius);
  if(var == F( "TOGGLE_BTN_PUB_TOPIC" ))  return String( MyConfParam.v_TOGGLE_BTN_PUB_TOPIC.c_str());
  if(var == F( "I0MODE" ))                return String( MyConfParam.v_IN0_INPUTMODE);
  if(var == F( "I1MODE" ))                return String( MyConfParam.v_IN1_INPUTMODE);
  if(var == F( "I2MODE" ))                return String( MyConfParam.v_IN2_INPUTMODE);

  if(var == F( "I3MODE" ))                return String( MyConfParam.v_IN3_INPUTMODE);
  if(var == F( "I4MODE" ))                return String( MyConfParam.v_IN4_INPUTMODE);
  if(var == F( "I5MODE" ))                return String( MyConfParam.v_IN5_INPUTMODE);
  if(var == F( "I6MODE" ))                return String( MyConfParam.v_IN6_INPUTMODE);
  #ifdef WaterFlowSensor
  if(var == F( "WaterFlowSensor" ))       return F("1");
  #else
  if(var == F( "WaterFlowSensor" ))       return F("0");
  #endif
  #ifdef _pressureSensor_
  if(var == F( "PressureSensor" ))        return F("1");
  #else
  if(var == F( "PressureSensor" ))        return F("0");
  #endif
  #ifdef _ADS1X15_
  if(var == F( "ADS1X15" ))              return F("1");
  #else
  if(var == F( "ADS1X15" ))              return F("0");
  #endif
  #ifdef _HST_
  if(var == F( "HST" ))                  return F("1");
  #else
  if(var == F( "HST" ))                  return F("0");
  #endif
  #ifdef _emonlib_
  if(var == F( "EmonLib" ))              return F("1");
  #else
  if(var == F( "EmonLib" ))              return F("0");
  #endif
  #ifdef OLED_1306
  if(var == F( "OLED1306" ))             return F("1");
  #else
  if(var == F( "OLED1306" ))             return F("0");
  #endif
  #ifdef HWESP32
  if(var == F( "TempSensorPin01Active" )) return String( MyConfParam.v_TempSensorPin01_Active);
  if(var == F( "TempSensorPin02Active" )) return String( MyConfParam.v_TempSensorPin02_Active);
  #endif

  if(var == F( "RSTATE0" ))               return [](){
    Relay *_r0 = getrelaybynumber(0);
    if (_r0 != nullptr) {
      return (_r0->readrelay() == HIGH) ? "ON" : "OFF";
    } else return "NA";
  }();
  if(var == F( "RSTATE1" ))               return [](){
    Relay *_r1 = getrelaybynumber(1);
    if (_r1 != nullptr) {
      return (_r1->readrelay() == HIGH) ? "ON" : "OFF";
    } else return "NA";
  }();
  if(var == F( "RSTATE_APPLIED" )) {
    if (AppliedRelay != nullptr)
      return (AppliedRelay->readrelay() == HIGH) ? String(F("ON")) : String(F("OFF"));
    return String(F("NA"));
  }
  
  if(var == F( "Sonar_distance" ))        return String( MyConfParam.v_Sonar_distance.c_str());
  if(var == F( "Sonar_distance_max" ))    return String( MyConfParam.v_Sonar_distance_max);

  if(var == F( "CurrentTransformer_max_current" ))  return String( MyConfParam.v_CurrentTransformer_max_current);
  if(var == F( "calibration" ))  return String( MyConfParam.v_calibration);    
  if(var == F( "PhaseCal" ))  return String( MyConfParam.v_PhaseCal);     
  if(var == F( "CurrentTransformerTopic" ))  return String( MyConfParam.v_CurrentTransformerTopic);   
  #ifdef _emonlib_
  if(var == F( "CT_VOLTAGE" )) return String(CT_1.supplyVoltage, 0);
  if(var == F( "CT_CURRENT" )) return String(CT_1.Irms, 1);
  if(var == F( "CT_POWER" )) return String(CT_1.realPower, 0);
  if(var == F( "CT_PF" )) return String(CT_1.powerFactor, 2);
  #else
  if(var == F( "CT_VOLTAGE" )) return String(F("NA"));
  if(var == F( "CT_CURRENT" )) return String(F("NA"));
  if(var == F( "CT_POWER" )) return String(F("NA"));
  if(var == F( "CT_PF" )) return String(F("NA"));
  #endif
  if(var == F( "ToleranceOffTime" ))  return String( MyConfParam.v_ToleranceOffTime);   
  if(var == F( "CT_adjustment" ))  return String( MyConfParam.v_CT_adjustment);     
  if(var == F( "ToleranceOnTime" ))  return String( MyConfParam.v_ToleranceOnTime);   
  if(var == F( "CT_MaxAllowed_current" ))  return String( MyConfParam.v_CT_MaxAllowed_current);   
  if(var == F( "CT_saveThreshold" ))  return String( MyConfParam.v_CT_saveThreshold);         
  if(var == F( "CT_ZeroLoadCurrentMax" ))  return String( MyConfParam.v_CT_ZeroLoadCurrentMax, 3);
  if(var == F( "CT_ZeroLoadPowerMin" ))    return String( MyConfParam.v_CT_ZeroLoadPowerMin, 2);
  if(var == F( "CT_ZeroLoadPowerMax" ))    return String( MyConfParam.v_CT_ZeroLoadPowerMax, 2);
  if(var == F( "EmonReaderIntervalMs" ))   return String( MyConfParam.v_EmonReaderIntervalMs);
  if(var == F( "EmonPublisherIntervalMs" )) return String( MyConfParam.v_EmonPublisherIntervalMs);
  if(var == F( "OLEDUpdateIntervalMs" ))   return String( MyConfParam.v_OLEDUpdateIntervalMs);
  if(var == F( "OLEDAlwaysOn" ))           { if (MyConfParam.v_OLEDAlwaysOn) return "1\" checked=\"\""; };
  
  if(var == F( "Screen_orientation" ))  return String( MyConfParam.v_Screen_orientation);

  if(var == F( "RELAYNB_OPTIONS" )) {
    String opts;
    opts.reserve(relays.size() * 40 + 8);
    for (size_t i = 0; i < relays.size(); i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if ((int)i == AppliedRelayNumber) opts += F(" selected");
      opts += F(">");
      opts += String(i);
      opts += F("</option>");
    }
    return opts;
  }

  if(var == F( "RELAY_CONFIG_OPTIONS" )) {
    String opts;
    opts.reserve(relays.size() * 48 + 8);
    for (size_t i = 0; i < relays.size(); i++) {
      opts += F("<option value=\"");
      opts += String(i);
      opts += F("\"");
      if ((int)i == AppliedRelayNumber) opts += F(" selected");
      opts += F(">Relay ");
      opts += String(i);
      opts += F("</option>");
    }
    return opts;
  }

  return String();
}


static uint8_t inputNumberFromPin(uint8_t pin) {
  #ifdef InputPin01
  if (pin == InputPin01) return 1;
  #endif
  #ifdef InputPin02
  if (pin == InputPin02) return 2;
  #endif
  #ifdef InputPin03
  if (pin == InputPin03) return 3;
  #endif
  #ifdef InputPin04
  if (pin == InputPin04) return 4;
  #endif
  #ifdef InputPin05
  if (pin == InputPin05) return 5;
  #endif
  #ifdef InputPin06
  if (pin == InputPin06) return 6;
  #endif
  return 0;
}

static String buildInputOptions(int8_t selectedPin) {
  String opts;
  opts.reserve(inputs.size() * 56 + 32);
  opts += F("<option value=\"-1\"");
  if (selectedPin == -1) opts += F(" selected");
  opts += F(">-- None --</option>");
  for (void* it : inputs) {
    InputSensor* inp = static_cast<InputSensor*>(it);
    if (!inp) continue;
    uint8_t inNum = inputNumberFromPin(inp->pin);
    opts += F("<option value=\"");
    opts += String(inp->pin);
    opts += F("\"");
    if (inp->pin == (uint8_t)selectedPin) opts += F(" selected");
    opts += F(">Input ");
    opts += (inNum > 0) ? String(inNum) : String(F("?"));
    opts += F(" (GPIO");
    opts += String(inp->pin);
    opts += F(")</option>");
  }
  return opts;
}

static String buildRelayOptions(int8_t selectedRelay) {
  String opts;
  opts.reserve(relays.size() * 44 + 32);
  opts += F("<option value=\"-1\"");
  if (selectedRelay == -1) opts += F(" selected");
  opts += F(">-- None --</option>");
  for (size_t i = 0; i < relays.size(); i++) {
    opts += F("<option value=\"");
    opts += String(i);
    opts += F("\"");
    if ((int8_t)i == selectedRelay) opts += F(" selected");
    opts += F(">Relay ");
    opts += String(i);
    opts += F("</option>");
  }
  return opts;
}

String IRMAPprocessor(const String& var)
{
  if(var == F("I1_OPT"))  return buildInputOptions(myIRMap.I1);
  if(var == F("I2_OPT"))  return buildInputOptions(myIRMap.I2);
  if(var == F("I3_OPT"))  return buildInputOptions(myIRMap.I3);
  if(var == F("I4_OPT"))  return buildInputOptions(myIRMap.I4);
  if(var == F("I5_OPT"))  return buildInputOptions(myIRMap.I5);
  if(var == F("I6_OPT"))  return buildInputOptions(myIRMap.I6);
  if(var == F("I7_OPT"))  return buildInputOptions(myIRMap.I7);
  if(var == F("I8_OPT"))  return buildInputOptions(myIRMap.I8);
  if(var == F("I9_OPT"))  return buildInputOptions(myIRMap.I9);
  if(var == F("I10_OPT")) return buildInputOptions(myIRMap.I10);
  if(var == F("R1_OPT"))  return buildRelayOptions(myIRMap.R1);
  if(var == F("R2_OPT"))  return buildRelayOptions(myIRMap.R2);
  if(var == F("R3_OPT"))  return buildRelayOptions(myIRMap.R3);
  if(var == F("R4_OPT"))  return buildRelayOptions(myIRMap.R4);
  if(var == F("R5_OPT"))  return buildRelayOptions(myIRMap.R5);
  if(var == F("R6_OPT"))  return buildRelayOptions(myIRMap.R6);
  if(var == F("R7_OPT"))  return buildRelayOptions(myIRMap.R7);
  if(var == F("R8_OPT"))  return buildRelayOptions(myIRMap.R8);
  if(var == F("R9_OPT"))  return buildRelayOptions(myIRMap.R9);
  if(var == F("R10_OPT")) return buildRelayOptions(myIRMap.R10);

  return String();
}

/*
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.println("Websocket client connection received");
  } else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");
  } else if(type == WS_EVT_DATA){
    Serial.println("Data received: ");
    for(int i=0; i < len; i++) {
          Serial.print((char) data[i]);
    }
    Serial.println();
    String msg = "";

    for(size_t i=0; i < len; i++) {
      msg += (char) data[i];
    }

    if (msg == "on") {
      client->text("on command executed");
      Relay * rtmp =  getrelaybynumber(0);
      rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
    }
    if (msg == "off") {
      client->text("off command executed");
      Relay * rtmp =  getrelaybynumber(0);
      rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
    }
  }
}
*/


// Called from loop(). If an upload started but the connection was dropped before
// the final chunk arrived, the response handler never fires and the board never
// reboots. This watchdog detects that stale state and forces a restart.
void firmwareOtaWatchdog()
{
  if (!firmwareOtaActive) return;
  if (firmwareOtaStartedMs == 0) return;
  const uint32_t elapsed = millis() - firmwareOtaStartedMs;
  if (elapsed > 180000UL) {
    Serial.println(F("[SYSTEM  ] OTA watchdog: upload incomplete, forcing reboot"));
    restartRequired = true;
  }
}

void wifiScanLoop()
{
  if (wifiScanRequested && !wifiScanRunning) {
    wifiScanRequested = false;
    Serial.println(F("[WIFI   ] Scan starting"));
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    wifiScanRunning = true;
    buildWifiScanPayload(0, true);
  }

  if (!wifiScanRunning) return;

  int networkCount = WiFi.scanComplete();
  if (networkCount == WIFI_SCAN_RUNNING) {
    return;
  }

  if (networkCount == WIFI_SCAN_FAILED) {
    wifiScanRunning = false;
    buildWifiScanPayload(0, false);
    return;
  }

  buildWifiScanPayload(networkCount, false);
  WiFi.scanDelete();
  wifiScanRunning = false;
}


// ── JSONP helpers ──────────────────────────────────────────────────────────
// JSONP bypasses CORS and Private Network Access entirely because <script> tags
// have no origin restrictions. Used as an automatic fallback by LiveReadingsRest.html
// when fetch() is blocked (e.g. Android content:// context).

// Minimal Base64 decoder — used to verify ?_auth=base64(user:pass) on JSONP requests.
static String _b64dec(const String& s) {
  static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String r; int bits = 0, acc = 0;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c == '=') break;
    const char *p = strchr(tbl, c);
    if (!p) continue;
    acc = (acc << 6) | (int)(p - tbl);
    if ((bits += 6) >= 8) { bits -= 8; r += (char)((acc >> bits) & 0xFF); }
  }
  return r;
}

// Strips any character that is not [A-Za-z0-9_.] from a JSONP callback name.
static String _safeCB(const String& cb) {
  String out;
  for (size_t i = 0; i < cb.length() && i < 64; i++) {
    char c = cb.charAt(i);
    if (isAlphaNumeric(c) || c == '_' || c == '.') out += c;
  }
  return out;
}

// Returns true if ?_auth=base64(user:pass) matches the board credentials.
static bool _jsonpAuth(AsyncWebServerRequest *req) {
  if (!req->hasParam("_auth")) return false;
  return _b64dec(req->getParam("_auth")->value()) == "user:pass";
}

// Returns a validated callback name, or "" if none / invalid.
static String _jsonpCB(AsyncWebServerRequest *req) {
  if (!req->hasParam("callback")) return "";
  return _safeCB(req->getParam("callback")->value());
}

// Sends payload wrapped in a JSONP callback. Content-Type is application/javascript
// so browsers execute it as a script and trigger the named callback function.
static void sendJSONP(AsyncWebServerRequest *req, const String& cb, const String& payload) {
  req->send(200, "application/javascript", cb + "(" + payload + ")");
}

// ── CORS helpers ────────────────────────────────────────────────────────────

// Reflects the request Origin back as Access-Control-Allow-Origin.
// Using the exact origin (including "null" from file:// pages) instead of "*"
// is required because Chrome does not accept ACAO:* for opaque null origins.
static void sendWithCORS(AsyncWebServerRequest *req, int code,
                         const String& contentType, const String& payload) {
  AsyncWebServerResponse *r = req->beginResponse(code, contentType, payload);
  String origin = req->hasHeader("Origin") ? req->header("Origin") : String("*");
  r->addHeader("Access-Control-Allow-Origin", origin);
  r->addHeader("Vary", "Origin");
  req->send(r);
}

// char* overload — avoids constructing a temporary String from the payload buffer.
static void sendWithCORS(AsyncWebServerRequest *req, int code,
                         const char* contentType, const char* payload) {
  AsyncWebServerResponse *r = req->beginResponse(code, contentType, payload);
  if (req->hasHeader("Origin")) {
    r->addHeader("Access-Control-Allow-Origin", req->header("Origin"));
  } else {
    r->addHeader("Access-Control-Allow-Origin", "*");
  }
  r->addHeader("Vary", "Origin");
  req->send(r);
}

static void sendWithCORSStream(AsyncWebServerRequest *req, AsyncResponseStream *resp) {
  String origin = req->hasHeader("Origin") ? req->header("Origin") : String("*");
  resp->addHeader("Access-Control-Allow-Origin", origin);
  resp->addHeader("Vary", "Origin");
  req->send(resp);
}

// Intercepts every CORS preflight before any route handler can respond
// without CORS headers. Must be defined at file scope (virtual methods).
class _CORSPreflightHandler : public AsyncWebHandler {
public:
  bool canHandle(AsyncWebServerRequest *req) override {
    return req->method() == HTTP_OPTIONS;
  }
  void handleRequest(AsyncWebServerRequest *req) override {
    Serial.printf("[CORS   ] OPTIONS %-30s  Origin: %-25s  ACRH: %s  ACRPN: %s\n",
      req->url().c_str(),
      req->hasHeader("Origin")                               ? req->header("Origin").c_str()                               : "-",
      req->hasHeader("Access-Control-Request-Headers")       ? req->header("Access-Control-Request-Headers").c_str()       : "-",
      req->hasHeader("Access-Control-Request-Private-Network") ? req->header("Access-Control-Request-Private-Network").c_str() : "-");
    // Headers set explicitly here — do NOT rely on DefaultHeaders in case the library
    // version doesn't apply them to manually-sent responses.
    AsyncWebServerResponse *r = req->beginResponse(200, "text/plain", "");
    String origin = req->hasHeader("Origin") ? req->header("Origin") : String("*");
    r->addHeader("Access-Control-Allow-Origin",          origin);
    r->addHeader("Access-Control-Allow-Methods",         "GET, POST, PUT, DELETE, OPTIONS");
    r->addHeader("Access-Control-Allow-Headers",         "Content-Type, Authorization");
    r->addHeader("Access-Control-Allow-Private-Network", "true");
    r->addHeader("Access-Control-Max-Age",               "86400");
    r->addHeader("Vary",                                 "Origin");
    req->send(r);
  }
};

// ── Browser-retry / weak-WiFi flood protection ─────────────────────────────
//
// Problem: on a weak connection the browser times out mid-load and retries,
// creating multiple concurrent requests for the same HTML page.  Each in-flight
// response holds response buffers + template-processor allocations.  Three or
// four stacked like this can deplete heap enough to crash.
//
// Solution: two lightweight AsyncWebHandler subclasses inserted at the front
// of the handler chain (before all route handlers):
//
//   _RequestTracker  – never "handles" the request (canHandle returns false),
//                      but counts page requests in flight via onDisconnect so
//                      the guard has early warning before heap even drops.
//
//   _PageFloodGuard  – registered before _RequestTracker so blocked requests
//                      are NOT counted.  Rejects with 503 when either:
//                        • 3+ page responses already in flight, OR
//                        • largest free heap block < 10 KB.
//
// Both skip .json endpoints and /api/ paths — those are tiny (char-buf based)
// and have their own per-handler heap guards.

static volatile int8_t g_pageInFlight = 0;

// Returns true if this URL is a lightweight JSON / API endpoint that does not
// need the page-flood guard.
static inline bool isLightEndpoint(const char* url) {
    // ends with .json
    size_t len = strlen(url);
    if (len >= 5 && strcmp(url + len - 5, ".json") == 0) return true;
    // starts with /api/
    if (strncmp(url, "/api/", 5) == 0) return true;
    return false;
}

class _RequestTracker : public AsyncWebHandler {
public:
    // Never owns the request — just increments/decrements the in-flight counter.
    bool canHandle(AsyncWebServerRequest *req) override {
        if (req->method() == HTTP_GET && !isLightEndpoint(req->url().c_str())) {
            g_pageInFlight++;
            req->onDisconnect([]{ if (g_pageInFlight > 0) g_pageInFlight--; });
        }
        return false; // always pass to next handler
    }
    void handleRequest(AsyncWebServerRequest *) override {}
};

class _PageFloodGuard : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest *req) override {
        if (req->method() != HTTP_GET) return false;
        if (isLightEndpoint(req->url().c_str())) return false;
        // Block when too many concurrent page responses OR heap is critically low.
        // g_pageInFlight is checked BEFORE _RequestTracker increments it, so the
        // effective limit is: guard fires when the (N+1)th request would push
        // in-flight to 3.
        return (g_pageInFlight >= 2 ||
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 10240);
    }
    void handleRequest(AsyncWebServerRequest *req) override {
        Serial.printf("[HTTP   ] 503 flood-guard: inflight=%d heapBlk=%lu url=%s\n",
                      (int)g_pageInFlight,
                      (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                      req->url().c_str());
        req->send(503, "text/html",
            F("<!doctype html><html><head><meta charset='utf-8'>"
              "<style>body{font-family:sans-serif;padding:40px;background:#f8fafc}"
              "h2{color:#b45309}a{color:#0f766e}</style></head><body>"
              "<h2>&#9203; Server busy</h2>"
              "<p>Too many concurrent page requests (weak connection?).</p>"
              "<p><a href='javascript:location.reload()'>Retry</a></p>"
              "</body></html>"));
    }
};
// ── end flood protection ─────────────────────────────────────────────────────

void SetAsyncHTTP(){

  // Guard: AsyncWebServer routes and handlers must be registered exactly once.
  // Calling SetAsyncHTTP() on every WiFi reconnect would duplicate handlers
  // and leak heap objects. The static flag ensures this runs only once.
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  // ACAO is NOT in DefaultHeaders — it must reflect the exact request Origin
  // (including "null" from file:// pages) rather than use a wildcard that
  // Chrome rejects for opaque origins. See sendWithCORS() below.
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",         "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",         "Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Private-Network", "true");
  DefaultHeaders::Instance().addHeader("Vary",                                 "Origin");

  // CORS preflight handler registered first — before all routes — so it
  // claims every OPTIONS request regardless of URL.
  AsyncWeb_server.addHandler(new _CORSPreflightHandler());

  // Flood guard registered before tracker: rejected requests must not increment
  // g_pageInFlight, so the guard must intercept them before the tracker sees them.
  AsyncWeb_server.addHandler(new _PageFloodGuard());
  AsyncWeb_server.addHandler(new _RequestTracker());

  //ws.onEvent(onWsEvent);
  //AsyncWeb_server.addHandler(&ws);

    AsyncWeb_server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    AsyncWeb_server.on("/JConfig", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/config.json");
        // int args = request->args();
    });

    AsyncWeb_server.on("/Timer1", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          if (request->hasParam("GetTimer")) {
            AsyncWebParameter * Para = request->getParam("GetTimer");
            String tmp = Para->value();
            char  timerfilename[20] = "";
            strcpy(timerfilename, "/timer");
            strcat(timerfilename, tmp.c_str());
            strcat(timerfilename, ".json");
            if (loadNodeTimer(timerfilename,NTmr)== SUCCESS) {
                  request->send(SPIFFS, "/Timer1.html", String(), false, timerprocessor);
            } else {
                  request->send(SPIFFS, "/Timer1.html", String(), false, timerprocessor);
            };
        } else {
          request->send(SPIFFS, "/Timer1.html", String(), false, timerprocessor);
        }
        // int args = request->args();
    });

    AsyncWeb_server.on("/AdvancedTemperatureConf.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        config_read_error_t res = loadTempConfig("/tempconfig.json",PTempConfig); 
        request->send(SPIFFS, "/AdvancedTemperatureConf.html", String(), false, TempConfProcessor);
      });

    AsyncWeb_server.on("/AdvancedTempSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/AdvancedTempSave.html");
            saveTempConfig(request);
            config_read_error_t res = loadTempConfig("/tempconfig.json",PTempConfig);  
    });

    AsyncWeb_server.on("/savetimer.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/savetimer.html");
            saveNodeTimer(request);
            CalendarNotInitiated = true;
    });

    #ifdef _AUTOMATION_RULES_
    AsyncWeb_server.on("/Automation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          if (request->hasParam("GetRule")) {
            AsyncWebParameter * Para = request->getParam("GetRule");
            String tmp = Para->value();
            char  rulefilename[20] = "";
            strcpy(rulefilename, "/rule");
            strcat(rulefilename, tmp.c_str());
            strcat(rulefilename, ".json");
            loadAutomationRule(rulefilename, ARule);
            request->send(SPIFFS, "/Automation.html", String(), false, automationprocessor);
        } else {
          request->send(SPIFFS, "/Automation.html", String(), false, automationprocessor);
        }
    });

    AsyncWeb_server.on("/saveautomationrule.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/saveautomationrule.html");
            saveAutomationRule(request);
    });
    #endif

    #ifdef _REMOTE_SENSORS_
    AsyncWeb_server.on("/RemoteSensors", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/RemoteSensors.html", String(), false, remoteSensorsProcessor);
    });

    AsyncWeb_server.on("/saveremotesensors.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/saveremotesensors.html");
      saveRemoteSensors(request);
      // Re-trigger MQTT subscriptions so new topics take effect without requiring
      // a broker reconnect. Safe to set from the async_tcp task — volatile flag
      // is checked by serviceMqttPending() on the next loop() iteration.
      if (mqttClient.connected()) pending_mqtt_subscribe = true;
    });

    // Streams all slot data including the last received payload.
    // AsyncResponseStream flushes per TCP segment — no large heap String.
    // lastPayload is pre-JSON-escaped at receive time, safe to embed directly.
    AsyncWeb_server.on("/api/remotesensors", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncResponseStream *resp = request->beginResponseStream(F("application/json"));
      uint32_t now = millis();
      resp->print('[');
      for (uint8_t i = 0; i < MAX_REMOTE_SENSORS; i++) {
        if (i) resp->print(',');
        char entry[512];
        bool valid = (remoteSensors[i].lastSeenMs != 0);
        snprintf(entry, sizeof(entry),
          "{\"slot\":%u,\"topic\":\"%s\",\"valid\":%s,\"value\":%.3f,\"age\":%lu,\"raw\":\"%s\"}",
          (unsigned)(i + 1),
          remoteSensors[i].topic,
          valid ? "true" : "false",
          (double)(float)remoteSensors[i].cachedValue,
          valid ? (unsigned long)((now - remoteSensors[i].lastSeenMs) / 1000UL) : 0UL,
          remoteSensors[i].lastPayload);
        resp->print(entry);
      }
      resp->print(']');
      request->send(resp);
    });

    #endif

    #ifdef _pressureSensor_
    AsyncWeb_server.on("/PressureSensorConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        TLloadconfig("/PressureSensorConfig.json", TL136);
        request->send(SPIFFS, "/PressureSensorConfig.html");
      });

    AsyncWeb_server.on("/api/psconfig", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        char buf[384];
        char st[28];
        digitalClockDisplay(st, sizeof(st));
        StaticJsonDocument<384> doc;
        doc["maSTopic"]        = TL136.maSTopic;
        doc["paramEmptyValue"] = (int)TL136.paramEmptyValue;
        doc["paramFullValue"]  = (int)TL136.paramFullValue;
        doc["maSLC"]           = (unsigned)TL136.maSLC;
        doc["maSHC"]           = (unsigned)TL136.maSHC;
        doc["maBurdenResistor"]= (unsigned)TL136.BurdenResistorValue;
        doc["systemtime"]      = st;
        serializeJson(doc, buf, sizeof(buf));
        request->send(200, "application/json", buf);
    });

    AsyncWeb_server.on("/PressureSensorSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      TLsaveconfig(request);
      TLloadconfig("/PressureSensorConfig.json", TL136);
      request->send(SPIFFS, "/PressureSensorSave.html");
    });

    AsyncWeb_server.on("/PressureSensor.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      float span = (float)(TL136.paramFullValue - TL136.paramEmptyValue);
      float fp   = (span != 0.0f) ? ((TL136.measure - TL136.paramEmptyValue) / span * 100.0f) : 0.0f;
      char json[64];
      snprintf(json, sizeof(json), "{\"adc\":%.1f,\"cm\":%.1f,\"fp\":%.1f}",
               (double)TL136.sampleI, (double)TL136.measure, (double)fp);
      request->send(200, "application/json", json);
    });
    #endif

    #ifdef _ADS1X15_
      AsyncWeb_server.on("/ADS11x5Config.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          config_read_error_t res = loadADS11x5Config("/ADS11x5Config.json",PADS11x5Config); 
          request->send(SPIFFS, "/ADS11x5Config.html", String(), false, ADS11x5ConfProcessor);
        });

      AsyncWeb_server.on("/ADS11x5ConfigSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/ADS11x5ConfigSave.html");
              saveADS11x5Config(request);
              config_read_error_t res = loadADS11x5Config("/ADS11x5Config.json",PADS11x5Config);
      });

      AsyncWeb_server.on("/api/ads", HTTP_GET, [](AsyncWebServerRequest *request){
        char buf[200];
        #ifdef _ADS1X15_DC_Current_
        snprintf(buf, sizeof(buf),
          "{\"v2\":%.4f,\"v3\":%.4f,\"sv2\":%.4f,\"sv3\":%.4f,\"rms\":%.4f,\"dc\":%.4f}",
          g_adsV2, g_adsV3,
          g_adsV2 * g_adsMultiplier, g_adsV3 * g_adsMultiplier,
          g_adsFinalRMSCurrent, g_adsFinalDCCurrent);
        #else
        snprintf(buf, sizeof(buf),
          "{\"v2\":%.4f,\"v3\":%.4f,\"sv2\":%.4f,\"sv3\":%.4f,\"rms\":0,\"dc\":0}",
          g_adsV2, g_adsV3,
          g_adsV2 * g_adsMultiplier, g_adsV3 * g_adsMultiplier);
        #endif
        request->send(200, "application/json", buf);
      });
    #endif

    // ── CORS/JSONP connectivity probe — no auth ────────────────────────────────
    AsyncWeb_server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.printf("[CORS   ] GET /api/ping from %s  Origin: %s\n",
        request->client()->remoteIP().toString().c_str(),
        request->hasHeader("Origin") ? request->header("Origin").c_str() : "-");
      String cb = _jsonpCB(request);
      if (cb.length()) { sendJSONP(request, cb, "{\"pong\":true}"); return; }
      sendWithCORS(request, 200, "application/json", "{\"pong\":true}");
    });

    // ── Comprehensive live-readings REST endpoint ──────────────────────────
    AsyncWeb_server.on("/api/live", HTTP_GET, [](AsyncWebServerRequest *request){
      String cb = _jsonpCB(request);
      if (cb.length()) {
        if (!_jsonpAuth(request)) { request->send(401, "application/javascript", "/* Unauthorized */"); return; }
      } else {
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      }

      // StaticJsonDocument lives on the stack — no heap alloc.
      // Sized for MAX_NUMBER_OF_TIMERS=4 with all sensor features enabled (~2.4 KB actual use).
      StaticJsonDocument<3072> doc;

      // System
      JsonObject sys = doc.createNestedObject("system");
      // Use the MQTT client-ID cache (set once at boot) to avoid a heap
      // allocation from CID() on every poll interval.
      { const char* cid = mqttGetClientId();
        sys["nodeId"] = (cid && *cid) ? (cid + 6) : CID().c_str(); }  // strip "ESP32-" prefix
      sys["mac"]      = MAC.c_str();
      sys["location"] = MyConfParam.v_PhyLoc;
      char _tb[24], _ub[20]; digitalClockDisplay(_tb, sizeof(_tb)); uptimeDisplay(_ub, sizeof(_ub));
      sys["time"]     = _tb;
      sys["uptime"]   = _ub;
      sys["heapFree"] = (uint32_t)ESP.getFreeHeap();

      // WiFi — snprintf IP octets directly, no toString() heap allocs
      JsonObject wifi = doc.createNestedObject("wifi");
      { IPAddress lip=WiFi.localIP(), gip=WiFi.gatewayIP(), d1=WiFi.dnsIP(0), d2=WiFi.dnsIP(1);
        char ipbuf[64];
        wifi["connected"] = (WiFi.status() == WL_CONNECTED);
        snprintf(ipbuf,sizeof(ipbuf),"%d.%d.%d.%d",lip[0],lip[1],lip[2],lip[3]); wifi["ip"]      = ipbuf;
        snprintf(ipbuf,sizeof(ipbuf),"%d.%d.%d.%d",gip[0],gip[1],gip[2],gip[3]); wifi["gateway"] = ipbuf;
        snprintf(ipbuf,sizeof(ipbuf),"%d.%d.%d.%d",d1[0],d1[1],d1[2],d1[3]);     wifi["dns1"]    = ipbuf;
        snprintf(ipbuf,sizeof(ipbuf),"%d.%d.%d.%d",d2[0],d2[1],d2[2],d2[3]);     wifi["dns2"]    = ipbuf;
      }

      // MQTT
      JsonObject mqtt = doc.createNestedObject("mqtt");
      mqtt["enabled"]     = mqttIsEnabled();
      mqtt["connected"]   = mqttClient.connected();
      { char _srv[80]; snprintf(_srv, sizeof(_srv), "%s:%d",
          MyConfParam.v_MQTT_BROKER.c_str(), (int)MyConfParam.v_MQTT_B_PRT);
        mqtt["server"] = _srv; }
      mqtt["route"]       = mqttRouteText();
      mqtt["healthTopic"] = mqttHealthTopicStorage.c_str();

      // VPN
      JsonObject vpn = doc.createNestedObject("vpn");
      WireGuardStatus wgs = wireGuardGetStatus();
      vpn["enabled"]   = wgs.enabled;
      vpn["connected"] = wgs.connected || wgs.networkUp;
      vpn["address"]   = wgs.localIp;
      vpn["endpoint"]  = wgs.configuredEndpoint;
      vpn["dns"]       = wgs.dnsServers;

      // Relays
      JsonArray rlys = doc.createNestedArray("relays");
      for (size_t i = 0; i < relays.size(); i++) {
        Relay *r = static_cast<Relay*>(relays[i]);
        JsonObject ro = rlys.createNestedObject();
        ro["id"]    = (int)i;
        ro["state"] = (r && r->readrelay() == HIGH) ? "ON" : "OFF";
      }

      // Temperature — store floats directly, no String conversion
      JsonObject temp = doc.createNestedObject("temperature");
      temp["sensor1"] = (double)MCelcius;
      temp["sensor2"] = (double)MCelcius2;

      // ACS current sensors
      Relay *r0 = getrelaybynumber(0);
      Relay *r1 = getrelaybynumber(1);
      JsonObject acs = doc.createNestedObject("acs");
      acs["acs1"] = (r0 && r0->RelayConfParam->v_ACS_Active) ? (double)ACS_I_Current : 0.0;
      acs["acs2"] = (r1 && r1->RelayConfParam->v_ACS_Active) ? (double)ACS_I_Current : 0.0;

      // Power monitor (EmonLib)
      #ifdef _emonlib_
      JsonObject pwr = doc.createNestedObject("power");
      pwr["voltage"]     = CT_1.supplyVoltage;
      pwr["current"]     = CT_1.Irms;
      pwr["realPower"]   = CT_1.realPower;
      pwr["powerFactor"] = CT_1.powerFactor;
      pwr["wh"]          = CT_1.wh;
      pwr["mtdWh"]       = CT_1.MTD_Wh;
      pwr["ytdWh"]       = CT_1.YTD_Wh;
      #endif

      // Battery voltage (ADS11x5)
      #ifdef _ADS1X15_
      JsonObject bv = doc.createNestedObject("batteryVoltage");
      bv["ain2Raw"]    = g_adsV2;
      bv["ain2Scaled"] = g_adsV2 * g_adsMultiplier;
      bv["ain3Raw"]    = g_adsV3;
      bv["ain3Scaled"] = g_adsV3 * g_adsMultiplier;
      #ifdef _ADS1X15_DC_Current_
      JsonObject bc = doc.createNestedObject("batteryCurrent");
      bc["rmsAmps"] = g_adsFinalRMSCurrent;
      bc["dcAmps"]  = g_adsFinalDCCurrent;
      #endif
      #endif

      // Water flow sensor
      #ifdef WaterFlowSensor
      {
        portENTER_CRITICAL(&pulseCountMux);
        uint32_t snap       = pulseCountCumulative;
        uint64_t totalSnap  = totalMilliLitres;   // 64-bit read under mux — not atomic on 32-bit core
        portEXIT_CRITICAL(&pulseCountMux);
        JsonObject wf = doc.createNestedObject("waterFlow");
        wf["flowRateLpm"] = flowRate;
        wf["totalLitres"] = (double)totalSnap / 1000.0;
        wf["pulses"]      = snap;
      }
      #endif

      // Pressure / level sensor
      #ifdef _pressureSensor_
      {
        float span = (float)(TL136.paramFullValue - TL136.paramEmptyValue);
        float fp   = (span != 0.0f) ? ((TL136.measure - TL136.paramEmptyValue) / span * 100.0f) : 0.0f;
        JsonObject prs = doc.createNestedObject("pressure");
        prs["adc"]         = TL136.sampleI;
        prs["levelCm"]     = TL136.measure;
        prs["fillPercent"] = fp;
      }
      #endif

      // Feature flags
      JsonObject feat = doc.createNestedObject("features");
      #ifdef _emonlib_
        feat["emonlib"]         = true;
      #else
        feat["emonlib"]         = false;
      #endif
      #ifdef _ADS1X15_
        feat["ads1x15"]         = true;
      #else
        feat["ads1x15"]         = false;
      #endif
      #ifdef _ADS1X15_DC_Current_
        feat["adsDCCurrent"]    = true;
      #else
        feat["adsDCCurrent"]    = false;
      #endif
      #ifdef _HST_
        feat["hst"]             = true;
      #else
        feat["hst"]             = false;
      #endif
      #ifdef WaterFlowSensor
        feat["waterFlowSensor"] = true;
      #else
        feat["waterFlowSensor"] = false;
      #endif
      #ifdef _pressureSensor_
        feat["pressureSensor"]  = true;
      #else
        feat["pressureSensor"]  = false;
      #endif

      // Scheduled timers — reads from in-memory cache populated by chronosInit()
      // (refreshed at boot and after every timer save).  No LittleFS access here.
      {
        int nowSecs = hour() * 3600 + minute() * 60 + second();
        const char* tmrTypeNames[] = {"Unknown","Date Range","Daily","Weekly","Monthly","Yearly"};
        JsonArray tmrArr = doc.createNestedArray("timers");
        for (int ti = 1; ti <= MAX_NUMBER_OF_TIMERS; ti++) {
          const TimerScheduleCache& tc = g_timerScheduleCache[ti - 1];
          JsonObject tobj = tmrArr.createNestedObject();
          tobj["id"] = ti;
          if (!tc.loaded) { tobj["exists"] = false; continue; }
          tobj["exists"]      = true;
          tobj["enabled"]     = tc.enabled ? 1 : 0;
          tobj["relay"]       = tc.relay;
          tobj["type"]        = (int)tc.TM_type;
          tobj["typeName"]    = ((int)tc.TM_type <= 5) ? tmrTypeNames[(int)tc.TM_type] : "Unknown";
          tobj["dateFrom"]    = tc.spanDatefrom;
          tobj["dateTo"]      = tc.spanDateto;
          tobj["timeFrom"]    = tc.spantimefrom;
          tobj["timeTo"]      = tc.spantimeto;
          tobj["markHours"]   = tc.Mark_Hours;
          tobj["markMinutes"] = tc.Mark_Minutes;
          tobj["monthDay"]    = tc.monthDay;
          JsonObject tdays = tobj.createNestedObject("days");
          tdays["Mo"] = tc.weekdays.Monday;   tdays["Tu"] = tc.weekdays.Tuesday;
          tdays["We"] = tc.weekdays.Wednesday; tdays["Th"] = tc.weekdays.Thursday;
          tdays["Fr"] = tc.weekdays.Friday;   tdays["Sa"] = tc.weekdays.Saturday;
          tdays["Su"] = tc.weekdays.Sunday;
          Relay *tr = getrelaybynumber(tc.relay);
          bool timerActive = tr && tr->hastimerrunning;
          bool timerPaused = tr && tr->timerpaused;
          tobj["timerActive"] = timerActive;
          tobj["timerPaused"] = timerPaused;
          int fromH = 0, fromM = 0, toH = 0, toM = 0;
          sscanf(tc.spantimefrom, "%d:%d", &fromH, &fromM);
          sscanf(tc.spantimeto,   "%d:%d", &toH,   &toM);
          int fromSecs = fromH * 3600 + fromM * 60;
          int toSecs;
          if (tc.Mark_Hours + tc.Mark_Minutes > 0) {
            toSecs = fromSecs + (int)tc.Mark_Hours * 3600 + (int)tc.Mark_Minutes * 60;
            char effTimeTo[6];
            snprintf(effTimeTo, sizeof(effTimeTo), "%02d:%02d",
                     (toSecs % 86400) / 3600, (toSecs % 3600) / 60);
            tobj["timeTo"] = effTimeTo;
          } else {
            toSecs = toH * 3600 + toM * 60;
          }
          if (timerActive) {
            long rem;
            if (tc.TM_type == TM_FULL_SPAN) {
              int ty, tm2, td, th, tmin;
              sscanf(tc.spanDateto, "%d-%d-%d", &ty, &tm2, &td);
              sscanf(tc.spantimeto, "%d:%d", &th, &tmin);
              tmElements_t te; te.Year = CalendarYrToTm(ty); te.Month = (uint8_t)tm2;
              te.Day = (uint8_t)td; te.Hour = (uint8_t)th; te.Minute = (uint8_t)tmin; te.Second = 0;
              time_t endT = makeTime(te);
              rem = (long)(endT - now());
              if (rem < 0) rem = 0;
            } else {
              rem = toSecs - nowSecs;
              if (rem < 0) rem += 86400;
            }
            tobj["secondsToOff"] = rem;
            tobj["secondsToOn"]  = -1;
          } else {
            long until = -1;
            switch (tc.TM_type) {
              case TM_WEEKDAY_SPAN: {
                uint8_t todayWd = weekday();
                for (int off = 0; off <= 7; off++) {
                  uint8_t wd = ((todayWd - 1 + off) % 7) + 1;
                  bool act = false;
                  switch(wd){case 1:act=tc.weekdays.Sunday;break;case 2:act=tc.weekdays.Monday;break;
                    case 3:act=tc.weekdays.Tuesday;break;case 4:act=tc.weekdays.Wednesday;break;
                    case 5:act=tc.weekdays.Thursday;break;case 6:act=tc.weekdays.Friday;break;
                    case 7:act=tc.weekdays.Saturday;break;}
                  if (act) { long c=(long)off*86400+fromSecs-nowSecs; if(c>0){until=c;break;} }
                }
                break;
              }
              case TM_MONTHLY_SPAN: {
                int td=day(), tgt=tc.monthDay>0?tc.monthDay:1;
                if (td==tgt && fromSecs>nowSecs) { until=fromSecs-nowSecs; }
                else if (td<tgt) { until=(long)(tgt-td)*86400-nowSecs+fromSecs; }
                else {
                  const uint8_t dim[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
                  int m=month(),y=year(),dim2=dim[m];
                  if(m==2&&((y%4==0&&y%100!=0)||y%400==0))dim2=29;
                  until=(long)(dim2-td+tgt)*86400-nowSecs+fromSecs;
                }
                break;
              }
              default: {
                int c=fromSecs-nowSecs; until=(c>0)?c:c+86400; break;
              }
            }
            tobj["secondsToOff"] = -1;
            tobj["secondsToOn"]  = until;
          }
        }
      }

      if (doc.overflowed())
        Serial.printf("[api/live] doc overflowed! capacity=%u used=%u\n",
                      (unsigned)doc.capacity(), (unsigned)doc.memoryUsage());

      if (cb.length()) {
        String payload; serializeJson(doc, payload);
        sendJSONP(request, cb, payload);
        return;
      }
      // Pre-size the stream buffer to the exact JSON output size so cbuf never
      // has to realloc mid-write (realloc leaves a freed hole that fragments the heap).
      AsyncResponseStream *resp = request->beginResponseStream("application/json", measureJson(doc) + 1);
      serializeJson(doc, *resp);
      sendWithCORSStream(request, resp);
    });

    #ifdef ESP32
    #ifdef WaterFlowSensor
    AsyncWeb_server.on("/WaterFlowSensor.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        loadconfigWFS("/WaterFlowSensorConfig.json");
        request->send(SPIFFS, "/WaterFlowSensor.html");
      });

    AsyncWeb_server.on("/api/wfsconfig", HTTP_GET, [](AsyncWebServerRequest *request){
        char buf[200];
        char st[28];
        digitalClockDisplay(st, sizeof(st));
        snprintf(buf, sizeof(buf),
            "{\"wfs_Topic\":\"%s\",\"wfs_Cal\":%.4f,\"systemtime\":\"%s\"}",
            WaterFlowSensor_Topic.c_str(), calibrationFactor, st);
        request->send(200, "application/json", buf);
    });

    AsyncWeb_server.on("/WaterFlowSensorSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/WaterFlowSensorSave.html");
            saveconfigWFS(request);
            loadconfigWFS("/WaterFlowSensorConfig.json");
    });

    AsyncWeb_server.on("/api/wfs", HTTP_GET, [](AsyncWebServerRequest *request){
      portENTER_CRITICAL(&pulseCountMux);
      uint32_t snap      = pulseCountCumulative;
      uint64_t totalSnap = totalMilliLitres;
      portEXIT_CRITICAL(&pulseCountMux);
      char buf[80];
      snprintf(buf, sizeof(buf), "{\"pulses\":%lu,\"flow\":%.1f,\"total\":%.3f}",
               (unsigned long)snap, flowRate, (double)totalSnap / 1000.0);
      request->send(200, "application/json", buf);
    });

    AsyncWeb_server.on("/api/wfs/reset", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      portENTER_CRITICAL(&pulseCountMux);
      totalMilliLitres = 0;
      portEXIT_CRITICAL(&pulseCountMux);
      File wfFile = SPIFFS.open("/wfs_total.dat", "w");
      if (wfFile) {
        uint64_t zero = 0;
        wfFile.write((const uint8_t*)&zero, sizeof(zero));
        wfFile.close();
      }
      request->send(200, "application/json", "{\"ok\":true}");
    });
    #endif
    #endif

    #ifdef ESP32
    #ifdef _HST_
      AsyncWeb_server.on("/HSTConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          config_read_error_t res = loadHSTConfig("/HSTConfig.json",PAHSTConfig); 
          request->send(SPIFFS, "/HSTConfig.html", String(), false, HSTConfProcessor);
        });

      AsyncWeb_server.on("/HSTConfigSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/HSTConfigSave.html");
              saveHSTConfig(request);
              config_read_error_t res = loadHSTConfig("/HSTConfig.json",PAHSTConfig);  
      });
    #endif   
    #endif   

    AsyncWeb_server.on("/CurrentConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/CurrentConfig.html", "text/html");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "max-age=60");
      request->send(r);
    });

    AsyncWeb_server.on("/InputsConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/InputsConfig.html", "text/html");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "max-age=60");
      request->send(r);
    });

    // GET /InputsConfig.json — return current input modes, topics and feature flags
    AsyncWeb_server.on("/InputsConfig.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      // Show chip-ID-based defaults for any topic that has never been configured,
      // without persisting them — "/none" is the loadConfig() fallback value, and
      // empty covers a fresh struct. This used to call saveConfig() here, but a
      // GET request shouldn't have the side effect of overwriting config.json —
      // notably, that clobbered restored backups whose source board legitimately
      // left a topic blank, baking *this* board's CID() over the restored value.
      // Fields genuinely never configured will keep showing (and get re-derived
      // as) a sane default each time this page loads; nothing is lost by not
      // persisting it. Precompute CID once — displayDefault may be called
      // up to 7 times, each of which would otherwise alloc/free a String.
      String cid = CID();
      auto displayDefault = [&cid](const String &field, int inputNum) -> String {
        if (field.isEmpty() || field == F("/none")) {
          char buf[64];
          snprintf(buf, sizeof(buf), "/home/Controller%s/INS/sts/IN%d", cid.c_str(), inputNum);
          return String(buf);
        }
        return field;
      };

      StaticJsonDocument<768> doc;
      { char _tb[24], _ub[20]; digitalClockDisplay(_tb, sizeof(_tb)); uptimeDisplay(_ub, sizeof(_ub));
        doc["systemtime"] = _tb; doc["uptime"] = _ub; }
      doc["heap"] = (uint32_t)ESP.getFreeHeap();

      doc["TOGGLE_BTN_PUB_TOPIC"] = displayDefault(MyConfParam.v_TOGGLE_BTN_PUB_TOPIC,      0);
      doc["I0MODE"] = MyConfParam.v_IN0_INPUTMODE;
      doc["I1MODE"] = MyConfParam.v_IN1_INPUTMODE;
      doc["I2MODE"] = MyConfParam.v_IN2_INPUTMODE;
      doc["I3MODE"] = MyConfParam.v_IN3_INPUTMODE;
      doc["I4MODE"] = MyConfParam.v_IN4_INPUTMODE;
      doc["I5MODE"] = MyConfParam.v_IN5_INPUTMODE;
      doc["I6MODE"] = MyConfParam.v_IN6_INPUTMODE;
      doc["I12_STS_PTP"] = displayDefault(MyConfParam.v_InputPin12_STATE_PUB_TOPIC, 1);
      doc["I14_STS_PTP"] = displayDefault(MyConfParam.v_InputPin14_STATE_PUB_TOPIC, 2);
      doc["I03_STS_PTP"] = displayDefault(MyConfParam.v_InputPin03_STATE_PUB_TOPIC, 3);
      doc["I04_STS_PTP"] = displayDefault(MyConfParam.v_InputPin04_STATE_PUB_TOPIC, 4);
      doc["I05_STS_PTP"] = displayDefault(MyConfParam.v_InputPin05_STATE_PUB_TOPIC, 5);
      doc["I06_STS_PTP"] = displayDefault(MyConfParam.v_InputPin06_STATE_PUB_TOPIC, 6);

      // Feature flags — needed by UI for IN6 flow-sensor lock and tool visibility
      #ifdef WaterFlowSensor
        doc["WaterFlowSensor"] = 1;
      #else
        doc["WaterFlowSensor"] = 0;
      #endif
      #ifdef SolarHeaterControllerMode
        doc["solarHeater"] = true;
      #else
        doc["solarHeater"] = false;
      #endif

      // hasTempRelay — if any relay has a temp threshold set, IN1/IN2 must stay in mode 5
      bool hasTempRelay = false;
      for (auto it : relays) {
        Relay *r = static_cast<Relay*>(it);
        if (!r || !r->RelayConfParam) continue;
        const String& tv = r->RelayConfParam->v_TemperatureValue;
        if (tv.length() > 0 && tv != "0") { hasTempRelay = true; break; }
      }
      doc["hasTempRelay"] = hasTempRelay ? 1 : 0;

      AsyncResponseStream *resp = request->beginResponseStream("application/json");
      serializeJson(doc, *resp);
      request->send(resp);
    });

    // POST /InputsConfig.json — receive JSON body, update MyConfParam, save
    AsyncWeb_server.on("/InputsConfig.json", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
        String* body = (String*)request->_tempObject;
        request->_tempObject = nullptr;
        if (!body || body->isEmpty()) {
          delete body;
          request->send(400, "application/json", F("{\"ok\":false,\"message\":\"empty body\"}"));
          return;
        }
        StaticJsonDocument<1536> doc;
        DeserializationError err = deserializeJson(doc, *body);
        delete body;
        if (err) {
          request->send(400, "application/json", F("{\"ok\":false,\"message\":\"json parse error\"}"));
          return;
        }

        if (doc.containsKey("TOGGLE_BTN_PUB_TOPIC")) MyConfParam.v_TOGGLE_BTN_PUB_TOPIC      = doc["TOGGLE_BTN_PUB_TOPIC"].as<String>();
        if (doc.containsKey("I0MODE"))               MyConfParam.v_IN0_INPUTMODE              = doc["I0MODE"].as<uint8_t>();
        if (doc.containsKey("I1MODE"))               MyConfParam.v_IN1_INPUTMODE              = doc["I1MODE"].as<uint8_t>();
        if (doc.containsKey("I2MODE"))               MyConfParam.v_IN2_INPUTMODE              = doc["I2MODE"].as<uint8_t>();
        if (doc.containsKey("I3MODE"))               MyConfParam.v_IN3_INPUTMODE              = doc["I3MODE"].as<uint8_t>();
        if (doc.containsKey("I4MODE"))               MyConfParam.v_IN4_INPUTMODE              = doc["I4MODE"].as<uint8_t>();
        if (doc.containsKey("I5MODE"))               MyConfParam.v_IN5_INPUTMODE              = doc["I5MODE"].as<uint8_t>();
        if (doc.containsKey("I6MODE"))               MyConfParam.v_IN6_INPUTMODE              = doc["I6MODE"].as<uint8_t>();
        if (doc.containsKey("I12_STS_PTP"))          MyConfParam.v_InputPin12_STATE_PUB_TOPIC = doc["I12_STS_PTP"].as<String>();
        if (doc.containsKey("I14_STS_PTP"))          MyConfParam.v_InputPin14_STATE_PUB_TOPIC = doc["I14_STS_PTP"].as<String>();
        if (doc.containsKey("I03_STS_PTP"))          MyConfParam.v_InputPin03_STATE_PUB_TOPIC = doc["I03_STS_PTP"].as<String>();
        if (doc.containsKey("I04_STS_PTP"))          MyConfParam.v_InputPin04_STATE_PUB_TOPIC = doc["I04_STS_PTP"].as<String>();
        if (doc.containsKey("I05_STS_PTP"))          MyConfParam.v_InputPin05_STATE_PUB_TOPIC = doc["I05_STS_PTP"].as<String>();
        if (doc.containsKey("I06_STS_PTP"))          MyConfParam.v_InputPin06_STATE_PUB_TOPIC = doc["I06_STS_PTP"].as<String>();

        saveConfig(MyConfParam);
        setupInputs();
        Serial.println(F("[HTTP   ] Inputs config saved."));
        request->send(200, "application/json", F("{\"ok\":true,\"message\":\"saved\"}"));
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          request->_tempObject = new String();
          request->onDisconnect([request]() {
            if (request->_tempObject) { delete (String*)request->_tempObject; request->_tempObject = nullptr; }
          });
        }
        String* body = (String*)request->_tempObject;
        if (body) body->concat((char*)data, len);
      }
    );

    // GET /CurrentConfig.json — return current CT/Sonar params
    AsyncWeb_server.on("/CurrentConfig.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      StaticJsonDocument<768> doc;
      { char _tb[24], _ub[20]; digitalClockDisplay(_tb, sizeof(_tb)); uptimeDisplay(_ub, sizeof(_ub));
        doc["systemtime"] = _tb; doc["uptime"] = _ub; }
      doc["heap"] = (uint32_t)ESP.getFreeHeap();

      doc["Sonar_distance"]                 = MyConfParam.v_Sonar_distance;
      doc["Sonar_distance_max"]             = MyConfParam.v_Sonar_distance_max;
      doc["CurrentTransformerTopic"]        = MyConfParam.v_CurrentTransformerTopic;
      doc["CurrentTransformer_max_current"] = MyConfParam.v_CurrentTransformer_max_current;
      doc["calibration"]                    = MyConfParam.v_calibration;
      doc["PhaseCal"]                       = MyConfParam.v_PhaseCal;
      doc["VCal_Vp"]                        = MyConfParam.v_VCal_Vp;
      doc["VCal_Vs"]                        = MyConfParam.v_VCal_Vs;
      doc["VCal_R1"]                        = MyConfParam.v_VCal_R1;
      doc["VCal_R2"]                        = MyConfParam.v_VCal_R2;
      doc["ToleranceOffTime"]               = MyConfParam.v_ToleranceOffTime;
      doc["ToleranceOnTime"]                = MyConfParam.v_ToleranceOnTime;
      doc["CT_MaxAllowed_current"]          = MyConfParam.v_CT_MaxAllowed_current;
      doc["CT_adjustment"]                  = MyConfParam.v_CT_adjustment;
      doc["CT_saveThreshold"]               = MyConfParam.v_CT_saveThreshold;
      doc["CT_ZeroLoadCurrentMax"]          = String(MyConfParam.v_CT_ZeroLoadCurrentMax, 3);
      doc["CT_ZeroLoadPowerMin"]            = String(MyConfParam.v_CT_ZeroLoadPowerMin, 2);
      doc["CT_ZeroLoadPowerMax"]            = String(MyConfParam.v_CT_ZeroLoadPowerMax, 2);
      doc["EmonReaderIntervalMs"]           = MyConfParam.v_EmonReaderIntervalMs;
      doc["EmonPublisherIntervalMs"]        = MyConfParam.v_EmonPublisherIntervalMs;

      #ifdef _emonlib_
      doc["CT_VOLTAGE"] = String(CT_1.supplyVoltage, 0);
      doc["CT_CURRENT"] = String(CT_1.Irms, 1);
      doc["CT_POWER"]   = String(CT_1.realPower, 0);
      doc["CT_PF"]      = String(CT_1.powerFactor, 2);
      doc["emonLib"]    = true;
      #else
      doc["CT_VOLTAGE"] = "NA";  doc["CT_CURRENT"] = "NA";
      doc["CT_POWER"]   = "NA";  doc["CT_PF"]      = "NA";
      doc["emonLib"]    = false;
      #endif

      AsyncResponseStream *resp = request->beginResponseStream("application/json");
      serializeJson(doc, *resp);
      request->send(resp);
    });

    // POST /CurrentConfig.json — receive JSON body, update MyConfParam, save
    AsyncWeb_server.on("/CurrentConfig.json", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
        String* body = (String*)request->_tempObject;
        request->_tempObject = nullptr;
        if (!body || body->isEmpty()) {
          delete body;
          request->send(400, "application/json", F("{\"ok\":false,\"message\":\"empty body\"}"));
          return;
        }
        StaticJsonDocument<768> doc;
        DeserializationError err = deserializeJson(doc, *body);
        delete body;
        if (err) {
          request->send(400, "application/json", F("{\"ok\":false,\"message\":\"json parse error\"}"));
          return;
        }

        if (doc.containsKey("Sonar_distance"))                 MyConfParam.v_Sonar_distance = doc["Sonar_distance"].as<String>();
        if (doc.containsKey("Sonar_distance_max"))             MyConfParam.v_Sonar_distance_max = doc["Sonar_distance_max"].as<uint16_t>();
        if (doc.containsKey("CurrentTransformerTopic"))        MyConfParam.v_CurrentTransformerTopic = doc["CurrentTransformerTopic"].as<String>();
        if (doc.containsKey("CurrentTransformer_max_current")) MyConfParam.v_CurrentTransformer_max_current = doc["CurrentTransformer_max_current"].as<float>();
        if (doc.containsKey("calibration"))                    MyConfParam.v_calibration = doc["calibration"].as<double>();
        if (doc.containsKey("PhaseCal"))                       MyConfParam.v_PhaseCal = doc["PhaseCal"].as<double>();
        if (doc.containsKey("VCal_Vp"))                        MyConfParam.v_VCal_Vp = doc["VCal_Vp"].as<float>();
        if (doc.containsKey("VCal_Vs"))                        MyConfParam.v_VCal_Vs = doc["VCal_Vs"].as<float>();
        if (doc.containsKey("VCal_R1"))                        MyConfParam.v_VCal_R1 = doc["VCal_R1"].as<float>();
        if (doc.containsKey("VCal_R2"))                        MyConfParam.v_VCal_R2 = doc["VCal_R2"].as<float>();
        if (doc.containsKey("ToleranceOffTime"))               MyConfParam.v_ToleranceOffTime = doc["ToleranceOffTime"].as<uint16_t>();
        if (doc.containsKey("ToleranceOnTime"))                MyConfParam.v_ToleranceOnTime = doc["ToleranceOnTime"].as<uint16_t>();
        if (doc.containsKey("CT_MaxAllowed_current"))          MyConfParam.v_CT_MaxAllowed_current = doc["CT_MaxAllowed_current"].as<uint16_t>();
        if (doc.containsKey("CT_adjustment"))                  MyConfParam.v_CT_adjustment = doc["CT_adjustment"].as<float>();
        if (doc.containsKey("CT_saveThreshold"))               MyConfParam.v_CT_saveThreshold = doc["CT_saveThreshold"].as<uint16_t>();
        if (doc.containsKey("CT_ZeroLoadCurrentMax"))          MyConfParam.v_CT_ZeroLoadCurrentMax = doc["CT_ZeroLoadCurrentMax"].as<float>();
        if (doc.containsKey("CT_ZeroLoadPowerMin"))            MyConfParam.v_CT_ZeroLoadPowerMin = doc["CT_ZeroLoadPowerMin"].as<float>();
        if (doc.containsKey("CT_ZeroLoadPowerMax"))            MyConfParam.v_CT_ZeroLoadPowerMax = doc["CT_ZeroLoadPowerMax"].as<float>();
        if (doc.containsKey("EmonReaderIntervalMs")) {
          uint32_t v = doc["EmonReaderIntervalMs"].as<uint32_t>();
          MyConfParam.v_EmonReaderIntervalMs = (v < 500UL) ? 500UL : v;
        }
        if (doc.containsKey("EmonPublisherIntervalMs")) {
          uint32_t v = doc["EmonPublisherIntervalMs"].as<uint32_t>();
          MyConfParam.v_EmonPublisherIntervalMs = (v < 1000UL) ? 1000UL : v;
        }

        saveConfig(MyConfParam);
        #if defined (_emonlib_) || defined (_pressureSensor_)
        applyPowerTaskIntervals();
        #endif
        #ifdef _emonlib_
        applyCTCalibration();
        #endif
        Serial.println(F("[HTTP   ] SonarCT config saved."));
        request->send(200, "application/json", F("{\"ok\":true,\"message\":\"saved\"}"));
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          request->_tempObject = new String();
          request->onDisconnect([request]() {
            if (request->_tempObject) { delete (String*)request->_tempObject; request->_tempObject = nullptr; }
          });
        }
        String* body = (String*)request->_tempObject;
        if (body) body->concat((char*)data, len);
      }
    );

    AsyncWeb_server.on("/VPNConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/VPNConfig.html");
    });

    AsyncWeb_server.on("/api/vpnconfig", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      loadWireGuardConfig("/WireGuardConfig.json", PWireGuardConfig);
      StaticJsonDocument<768> doc;
      doc["VPNEnabled"]            = PWireGuardConfig.enabled ? "1" : "0";
      doc["VPNProtocol"]           = PWireGuardConfig.protocol;
      doc["WGPrivateKey"]          = PWireGuardConfig.privateKey;
      doc["WGLocalIP"]             = PWireGuardConfig.localIp;
      doc["WGPeerPublicKey"]       = PWireGuardConfig.peerPublicKey;
      doc["WGPeerEndpoint"]        = PWireGuardConfig.peerEndpoint;
      doc["WGAllowedIPs"]          = PWireGuardConfig.allowedIps;
      doc["WGDnsServers"]          = PWireGuardConfig.dnsServers;
      doc["WGDnsLivenessCheck"]    = PWireGuardConfig.dnsLivenessCheck ? "1" : "0";
      doc["WGDnsLivenessInterval"] = (int)PWireGuardConfig.dnsLivenessInterval;
      char payload[512];
      serializeJson(doc, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/api/vpnconfig", HTTP_POST,
      [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        if (!request->_tempObject) { request->send(400, "application/json", F("{\"ok\":false}")); return; }
        String* body = reinterpret_cast<String*>(request->_tempObject);
        bool ok = saveWireGuardConfigFromJson(*body);
        delete body;
        request->_tempObject = nullptr;
        if (ok) {
          loadWireGuardConfig("/WireGuardConfig.json", PWireGuardConfig);
          wireGuardRequestApplyConfig();
        }
        request->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (!index) request->_tempObject = new String();
        reinterpret_cast<String*>(request->_tempObject)->concat((char*)data, len);
      }
    );

    AsyncWeb_server.on("/WireGuardStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      WireGuardStatus status = wireGuardGetStatus();
      StaticJsonDocument<768> json;
      json["enabled"] = status.enabled;
      json["started"] = status.started;
      json["connected"] = status.connected;
      json["networkUp"] = status.networkUp;
      json["reconnecting"] = status.reconnecting;
      json["address"] = status.localIp;
      json["endpoint"] = status.configuredEndpoint;
      json["resolvedEndpoint"] = status.resolvedEndpointIp;
      json["peerIp"] = status.peerIp;
      json["port"] = status.peerPort;
      json["dns"] = status.dnsServers;
      json["dnsLivenessCheck"] = status.dnsLivenessCheck;
      json["dnsLivenessInterval"] = status.dnsLivenessInterval;
      json["dnsLivenessFailures"] = status.dnsLivenessFailures;

      char payload[512];
      serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/MqttStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      char srvBuf[80];
      snprintf(srvBuf, sizeof(srvBuf), "%s:%d",
               MyConfParam.v_MQTT_BROKER.c_str(), (int)MyConfParam.v_MQTT_B_PRT);
      StaticJsonDocument<384> json;
      json["enabled"]     = mqttIsEnabled();
      json["connected"]   = mqttClient.connected();
      json["connecting"]  = (bool)mqttConnecting;
      json["status"]      = mqttStatusText();       // now const char* — no heap
      json["server"]      = srvBuf;
      json["healthTopic"] = mqttHealthTopicStorage.c_str(); // cached, no alloc
      json["route"]       = mqttRouteText();        // now const char* — no heap
      char payload[300];
      serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/SensorStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      Relay *r0 = getrelaybynumber(0);
      Relay *r1 = getrelaybynumber(1);

      // All floats formatted into a shared stack buffer to avoid heap allocs.
      // This handler fires every 2 s; eliminating ~15 String heap alloc/free
      // cycles per call prevents the gradual heap fragmentation that makes
      // large contiguous allocations fail after hours of runtime.
      StaticJsonDocument<768> json;
      char tmp[20];

      snprintf(tmp, sizeof(tmp), "%.2f", (double)MCelcius);  json["ATEMP1"] = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)MCelcius2); json["ATEMP2"] = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)ACS_I_Current);
      json["ACS1"] = (r0 && r0->RelayConfParam->v_ACS_Active) ? tmp : "0.00";
      json["ACS2"] = (r1 && r1->RelayConfParam->v_ACS_Active) ? tmp : "0.00";

      #ifdef _emonlib_
      snprintf(tmp, sizeof(tmp), "%.1f", (double)CT_1.supplyVoltage); json["CT_VOLTAGE"] = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)CT_1.Irms);          json["CT_CURRENT"] = tmp;
      snprintf(tmp, sizeof(tmp), "%.1f", (double)CT_1.realPower);     json["CT_POWER"]   = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)CT_1.powerFactor);   json["CT_PF"]      = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)CT_1.wh);            json["CT_WH"]      = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)CT_1.MTD_Wh);        json["CT_MTD_WH"]  = tmp;
      snprintf(tmp, sizeof(tmp), "%.2f", (double)CT_1.YTD_Wh);        json["CT_YTD_WH"]  = tmp;
      #else
      json["CT_VOLTAGE"] = "NA"; json["CT_CURRENT"] = "NA";
      json["CT_POWER"]   = "NA"; json["CT_PF"]      = "NA";
      json["CT_WH"]      = "NA"; json["CT_MTD_WH"]  = "NA"; json["CT_YTD_WH"] = "NA";
      #endif

      JsonArray relayArr = json.createNestedArray("relays");
      for (size_t i = 0; i < relays.size(); i++) {
        Relay *r = static_cast<Relay*>(relays[i]);
        JsonObject ro = relayArr.createNestedObject();
        ro["nb"]    = i;
        ro["state"] = (r && r->readrelay() == HIGH) ? "ON" : "OFF";
      }
      json["RSTATE0"] = (r0 && r0->readrelay() == HIGH) ? "ON" : "OFF";
      json["RSTATE1"] = (r1 && r1->readrelay() == HIGH) ? "ON" : "OFF";

      // Buffer forms — zero heap allocs.
      char _ub[20], _tb[24];
      uptimeDisplay(_ub, sizeof(_ub));       json["uptime"]     = _ub;
      json["heap"]        = (uint32_t)ESP.getFreeHeap();
      json["heapMin"]     = (uint32_t)ESP.getMinFreeHeap();
      json["heapMaxBlk"]  = g_heapMaxBlkCached;
      digitalClockDisplay(_tb, sizeof(_tb)); json["time"]       = _tb;

      char payload[640];
      serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/RelayState.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      int nb = 0;
      if (request->hasParam("relay")) nb = request->getParam("relay")->value().toInt();
      Relay *r = getrelaybynumber(nb);
      char payload[48];
      snprintf(payload, sizeof(payload), "{\"relay\":%d,\"state\":\"%s\"}",
               nb, r ? (r->readrelay() == HIGH ? "ON" : "OFF") : "NA");
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/FeatureFlags.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      StaticJsonDocument<384> json;
      #ifdef WaterFlowSensor
        json["waterFlowSensor"] = true;
      #else
        json["waterFlowSensor"] = false;
      #endif
      #ifdef OLED_1306
        json["oled1306"] = true;
      #else
        json["oled1306"] = false;
      #endif
      #ifdef _emonlib_
        json["emonlib"] = true;
      #else
        json["emonlib"] = false;
      #endif
      #ifdef SolarHeaterControllerMode
        json["solarHeater"] = true;
      #else
        json["solarHeater"] = false;
      #endif
      #ifdef _ADS1X15_
        json["ads1x15"] = true;
      #else
        json["ads1x15"] = false;
      #endif
      #ifdef _HST_
        json["hst"] = true;
      #else
        json["hst"] = false;
      #endif
      #ifdef _pressureSensor_
        json["pressureSensor"] = true;
      #else
        json["pressureSensor"] = false;
      #endif
      #ifdef _ADS1X15_DC_Current_
        json["adsDCCurrent"] = true;
      #else
        json["adsDCCurrent"] = false;
      #endif

      char payload[256];
      serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/WifiScan.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      if (request->hasParam("start")) {
        if (!wifiScanRunning && !wifiScanRequested) {
          wifiScanRequested = true;
          Serial.println(F("[WIFI   ] Scan requested"));
        } else {
          Serial.println(F("[WIFI   ] Scan request ignored; scan already pending/running"));
        }
        request->send(200, "application/json", "{\"scanning\":true,\"count\":0,\"networks\":[]}");
        return;
      }

      request->send(200, "application/json", wifiScanPayload);
    });

    AsyncWeb_server.on("/WifiStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      bool up = (WiFi.status() == WL_CONNECTED);
      IPAddress lip = WiFi.localIP(), gip = WiFi.gatewayIP(),
                d1  = WiFi.dnsIP(0),  d2  = WiFi.dnsIP(1);
      char payload[200];
      snprintf(payload, sizeof(payload),
        "{\"connected\":%s,\"status\":\"%s\","
        "\"ip\":\"%d.%d.%d.%d\",\"gateway\":\"%d.%d.%d.%d\","
        "\"dns1\":\"%d.%d.%d.%d\",\"dns2\":\"%d.%d.%d.%d\"}",
        up ? "true" : "false", up ? "connected" : "disconnected",
        lip[0],lip[1],lip[2],lip[3], gip[0],gip[1],gip[2],gip[3],
        d1[0],d1[1],d1[2],d1[3],   d2[0],d2[1],d2[2],d2[3]);
      request->send(200, "application/json", payload);
    });

    AsyncWeb_server.on("/ResetAccumulatedPower.json", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      StaticJsonDocument<128> json;
      #ifdef _emonlib_
      CT_1.wh = 0;
      CT_1.MTD_Wh = 0;
      CT_1.YTD_Wh = 0;
      CT_1.PreviousWh = 0;
      CT_1.CTSaveThreshold = 0;
      saveCTReadings(0, 0, 0, CT_1._last_reset_day, CT_1._last_reset_month, CT_1._last_reset_year);
      Serial.println(F("[CT     ] Web reset: power accumulators reset to zero."));
      json["ok"] = true;
      json["message"] = "Accumulated power reset";
      #else
      json["ok"] = false;
      json["message"] = "Power sensor is not enabled";
      #endif

      char payload[80];
      serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    // ── GET /coredump.html ───────────────────────────────────────────────────
    AsyncWeb_server.on("/coredump.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/coredump.html", "text/html");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "no-store");
      request->send(r);
    });

    // ── GET /api/coredump — stream raw base64 dump data ─────────────────────
    // Reads the coredump partition in 192-byte chunks (192 B → 256 B base64,
    // divisible by 3 for clean base64 groups with no inter-chunk padding).
    // Flash reads do NOT disable the CPU cache, so no TWDT risk.
    AsyncWeb_server.on("/api/coredump", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      #ifdef ESP32
      const esp_partition_t *part = esp_partition_find_first(
          ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
      if (!part) { request->send(404, "text/plain", "No coredump partition"); return; }
      uint32_t cdSize = 0;
      esp_partition_read(part, 0, &cdSize, sizeof(cdSize));
      if (cdSize == 0 || cdSize == 0xFFFFFFFF || cdSize > part->size) {
        request->send(404, "text/plain", "No core dump stored"); return;
      }
      // Stream base64 via AsyncResponseStream — TCP backpressure keeps heap use bounded.
      AsyncResponseStream *resp = request->beginResponseStream("text/plain");
      const size_t CHUNK = 192;
      uint8_t buf[CHUNK];
      uint8_t b64[264];
      size_t outLen;
      uint32_t offset = 0, lineChunks = 0;
      while (offset < cdSize) {
        size_t toRead = (uint32_t)CHUNK < cdSize - offset ? CHUNK : cdSize - offset;
        if (esp_partition_read(part, offset, buf, toRead) != ESP_OK) break;
        if (mbedtls_base64_encode(b64, sizeof(b64), &outLen, buf, toRead) == 0)
          resp->write((const char*)b64, outLen);
        offset += toRead;
        if ((++lineChunks & 3) == 0) resp->write("\n", 1); // ~1 KB per line
      }
      resp->write("\n", 1);
      request->send(resp);
      #else
      request->send(404, "text/plain", "Not supported on this platform");
      #endif
    });

    AsyncWeb_server.on("/CoreDumpStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      #ifdef ESP32
      esp_core_dump_summary_t summary;
      bool hasDump = (esp_core_dump_get_summary(&summary) == ESP_OK);
      StaticJsonDocument<192> json;
      json["hasDump"] = hasDump;
      if (hasDump) {
        json["task"]      = summary.exc_task;
        json["excCause"]  = summary.ex_info.exc_cause;
        json["epc1"]      = summary.ex_info.epcx[0];
      }
      char payload[256]; serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
      #else
      request->send(200, "application/json", "{\"hasDump\":false}");
      #endif
    });

    AsyncWeb_server.on("/EraseCoreDump.json", HTTP_POST, [](AsyncWebServerRequest *request){
      #ifdef ESP32
      esp_err_t err = esp_core_dump_image_erase();
      StaticJsonDocument<64> json;
      json["ok"] = (err == ESP_OK);
      if (err != ESP_OK) json["message"] = "Erase failed";
      Serial.printf("[SYSTEM  ] Core dump erase requested via web UI: %s\n", err == ESP_OK ? "OK" : "FAILED");
      char payload[64]; serializeJson(json, payload, sizeof(payload));
      request->send(200, "application/json", payload);
      #else
      request->send(200, "application/json", "{\"ok\":false}");
      #endif
    });

    AsyncWeb_server.on("/Backup.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/Backup.html");
    });

    AsyncWeb_server.on("/Files.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/Files.html");
    });

    // ── GET /api/backup?includeWeb=0|1 — download a combined settings backup ─
    // Bundles every *.json config file (always) plus, optionally, every other
    // file on LittleFS (the web UI pages normally flashed via uploadfs) into
    // one JSON document, each entry base64-encoded so arbitrary text/binary
    // content drops straight into a JSON string with zero escaping concerns.
    //
    // Generated via beginChunkedResponse() with a pull-based filler callback —
    // unlike AsyncResponseStream (whose write()/print() calls only ever buffer
    // into an internal cbuf that's grown with realloc+copy and flushed *after*
    // the handler returns), this callback is invoked on demand as the TCP send
    // buffer drains, so memory use stays bounded to one small file-name string
    // plus a 256 B base64 chunk regardless of how many/large the bundled files
    // are — a `?includeWeb=1` backup of this project's web UI alone is ~350 KB
    // raw (~470 KB once base64-inflated), which would not fit alongside the
    // rest of the heap if buffered whole.
    AsyncWeb_server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      const bool includeWeb = request->hasParam("includeWeb") &&
                              request->getParam("includeWeb")->value() == "1";

      auto wanted = [includeWeb](const String &name) {
        return name != "/.exclude.files" && (includeWeb || name.endsWith(".json"));
      };

      auto files = std::make_shared<std::vector<String>>();
      {
        File root = SPIFFS.open("/");
        unsigned seen = 0, skipped = 0;
        for (File e = root.openNextFile(); e; e = root.openNextFile()) {
          if (e.isDirectory()) continue;
          seen++;
          String name = e.name();
          if (!name.startsWith("/")) name = "/" + name;
          if (wanted(name)) {
            files->push_back(name);
          } else {
            skipped++;
          }
        }
        Serial.printf("[BACKUP ] Directory scan: %u file(s) seen, %u matched, %u skipped by filter\n",
                      seen, (unsigned)files->size(), skipped);
        for (const String &n : *files) Serial.printf("[BACKUP ]   + %s\n", n.c_str());
      }

      enum BackupPhase { BK_HEADER, BK_FILE_KEY, BK_FILE_DATA, BK_FILE_CLOSE, BK_FOOTER, BK_DONE };
      struct BackupGenState {
        std::shared_ptr<std::vector<String>> files;
        size_t fileIdx = 0;
        File current;
        String chunk;
        size_t chunkPos = 0;
        BackupPhase phase = BK_HEADER;
      };
      auto st = std::make_shared<BackupGenState>();
      st->files = files;

      Serial.printf("[BACKUP ] Backup download starting: %u file(s)%s\n",
                    (unsigned)files->size(), includeWeb ? " (incl. web UI)" : "");

      AsyncWebServerResponse *resp = request->beginChunkedResponse("application/json",
        [st, includeWeb](uint8_t *buf, size_t maxLen, size_t /*index*/) -> size_t {
          size_t written = 0;
          while (written < maxLen) {
            if (st->chunkPos < st->chunk.length()) {
              size_t avail = st->chunk.length() - st->chunkPos;
              size_t toCopy = avail < (maxLen - written) ? avail : (maxLen - written);
              memcpy(buf + written, st->chunk.c_str() + st->chunkPos, toCopy);
              st->chunkPos += toCopy;
              written += toCopy;
              continue;
            }
            if (st->phase == BK_DONE) break;

            // Current chunk fully drained — produce the next piece of content.
            st->chunk = String();
            st->chunkPos = 0;
            switch (st->phase) {
              case BK_HEADER: {
                String h;
                h.reserve(80);
                h += F("{\"_backup_version\":1,\"_chipid\":\"");
                h += CID();
                h += F("\",\"_includesWeb\":");
                h += (includeWeb ? F("true") : F("false"));
                h += F(",\"_files\":");
                h += st->files->size();
                h += F(",");
                st->chunk = h;
                st->phase = st->files->empty() ? BK_FOOTER : BK_FILE_KEY;
                break;
              }
              case BK_FILE_KEY: {
                const String &name = (*st->files)[st->fileIdx];
                st->current = SPIFFS.open(name, "r");
                st->chunk = "\"" + name + "\":\"";
                st->phase = BK_FILE_DATA;
                break;
              }
              case BK_FILE_DATA: {
                uint8_t raw[192], b64[264];
                size_t got = st->current ? st->current.read(raw, sizeof(raw)) : 0;
                if (got == 0) {
                  if (st->current) st->current.close();
                  st->phase = BK_FILE_CLOSE;
                } else {
                  size_t outLen = 0;
                  if (mbedtls_base64_encode(b64, sizeof(b64), &outLen, raw, got) == 0) {
                    b64[outLen] = 0;
                    st->chunk = String((const char*)b64);
                  }
                }
                break;
              }
              case BK_FILE_CLOSE: {
                st->fileIdx++;
                bool more = st->fileIdx < st->files->size();
                st->chunk = more ? F("\",") : F("\"");
                st->phase = more ? BK_FILE_KEY : BK_FOOTER;
                break;
              }
              case BK_FOOTER:
                st->chunk = F("}");
                st->phase = BK_DONE;
                break;
              case BK_DONE:
              default:
                break;
            }
          }
          if (written == 0 && st->phase == BK_DONE) {
            Serial.println(F("[BACKUP ] Backup download complete"));
          }
          return written;
        });
      resp->addHeader("Content-Disposition", "attachment; filename=\"smartconfig-backup.json\"");
      request->send(resp);
    });

    // ── POST /api/restore?chipIdMode=keep|replace&restoreWeb=0|1 — restore ──
    // Earlier version buffered the whole upload into one String and parsed it
    // with a small DynamicJsonDocument — for a settings+web-UI backup that's
    // 500 KB+, neither fits in this board's RAM, so the body silently
    // truncated and only the first handful of files got restored.
    //
    // Instead, walk the upload byte-by-byte as it arrives in onBody (which is
    // itself fed straight from the TCP receive buffer — never the full body
    // at once) with a small hand-rolled scanner for this format's exact shape,
    // a flat `{"_meta":val,...,"/path":"<base64>",...}` object: every
    // non-`_` key is a filename whose value is a base64 string, decoded four
    // characters at a time and written straight to LittleFS as it streams in.
    // Memory use stays bounded to a handful of short Strings (key, metadata
    // values, a 4-byte base64 group) regardless of backup size.
    struct RestoreState {
      enum Phase {
        RS_KEY_START, RS_KEY, RS_COLON, RS_VALUE_START,
        RS_META_STRING, RS_META_BARE, RS_FILE_STRING, RS_COMMA, RS_DONE
      };
      Phase phase = RS_KEY_START;
      String key;
      String metaValue;
      String oldChipId;
      String newChipId;
      String targetChipId;
      String chipIdModeParam;
      bool sawVersion = false;
      bool includesWeb = false;
      bool restoreWeb = false;
      bool doReplace = false;
      bool chipIdResolved = false;
      unsigned written = 0;

      String currentFilename;
      bool skipCurrentFile = false;
      File outFile;
      size_t decodedLen = 0;
      uint8_t b64group[4];
      uint8_t b64groupLen = 0;
      // Only used in chip-ID-replace mode, where each file's decoded bytes
      // must be buffered so String::replace() can run before writing —
      // bounded per-file (largest web UI page here is ~14 KB), unlike
      // buffering the entire multi-hundred-KB upload up front.
      String fileBuffer;

      // Three modes, selected via ?chipIdMode=:
      //   "keep"     — write the backup's file content verbatim, untouched.
      //   "replace"  — normalize every "Controller<ID>" reference (whatever
      //                ID(s) it currently holds) to *this* board's live CID().
      //   "original" — normalize every "Controller<ID>" reference to the
      //                *backup's* recorded _chipid — i.e. force the restored
      //                settings to self-identify as the source board did,
      //                regardless of what's actually baked into the content.
      //                Useful for a 1:1 dead-board replacement where external
      //                MQTT/HomeKit subscriptions already expect that ID.
      // Note: neither "replace" nor "original" compares oldChipId to
      // newChipId first. The backup's _chipid only records the *source*
      // board's hardware ID at backup time — it does not guarantee that's the
      // ID actually embedded in the saved topic strings (those are free-text
      // fields the user can edit, copy from another board, or carry over
      // verbatim from an earlier cross-board "keep" restore). So whenever the
      // user asks for a specific identity, scan-and-normalize the content
      // directly (see normalizeControllerIds / finishFileValue) to that
      // target rather than trusting what's already there.
      void resolveChipIdMode() {
        if (chipIdResolved) return;
        chipIdResolved = true;
        newChipId = CID();
        if (chipIdModeParam == "replace") {
          targetChipId = newChipId;
          doReplace = true;
        } else if (chipIdModeParam == "original" && oldChipId.length() == 12) {
          targetChipId = oldChipId;
          doReplace = true;
        } else {
          doReplace = false;
        }
      }

      static bool isHexChar(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
      }

      // Rewrites every "Controller<12-hex-chars>" occurrence in buf to carry
      // thisId instead, regardless of what ID(s) were originally embedded —
      // see the note in resolveChipIdMode for why scanning the content is the
      // only reliable way to normalize every reference to this board's own
      // identity.
      static void normalizeControllerIds(String &buf, const String &thisId) {
        const char *marker = "Controller";
        const int markerLen = 10;
        int idx = 0;
        while ((idx = buf.indexOf(marker, idx)) >= 0) {
          int idStart = idx + markerLen;
          bool validId = (idStart + 12 <= (int)buf.length());
          for (int i = 0; validId && i < 12; i++) {
            if (!isHexChar(buf[idStart + i])) validId = false;
          }
          if (validId) {
            if (!buf.substring(idStart, idStart + 12).equalsIgnoreCase(thisId)) {
              buf = buf.substring(0, idStart) + thisId + buf.substring(idStart + 12);
            }
            idx = idStart + thisId.length();
          } else {
            idx = idStart;
          }
        }
      }

      void finishMetaValue() {
        if (key == "_backup_version") {
          sawVersion = true;
        } else if (key == "_chipid") {
          oldChipId = metaValue;
        } else if (key == "_includesWeb") {
          includesWeb = (metaValue == "true");
          Serial.printf("[BACKUP ] Restore stream: chipid=%s includesWeb=%s restoreWeb=%s chipIdMode=%s\n",
                        oldChipId.c_str(), includesWeb ? "true" : "false",
                        restoreWeb ? "1" : "0", chipIdModeParam.c_str());
        }
      }

      void beginFileValue() {
        currentFilename = key;
        const bool isJson = currentFilename.endsWith(".json");
        skipCurrentFile = restoreWeb ? isJson : !isJson;
        decodedLen = 0;
        b64groupLen = 0;
        fileBuffer = String();
        outFile = File();
        if (skipCurrentFile) {
          Serial.printf("[BACKUP ]   - %s skipped (%s-only restore)\n",
                        currentFilename.c_str(), restoreWeb ? "web" : "config");
          return;
        }
        resolveChipIdMode();
        if (!doReplace) {
          outFile = SPIFFS.open(currentFilename, "w");
          if (!outFile) {
            Serial.printf("[BACKUP ]   - %s skipped (could not open for write)\n", currentFilename.c_str());
            skipCurrentFile = true;
          }
        }
      }

      void emitDecoded(const uint8_t *buf, size_t n) {
        decodedLen += n;
        if (doReplace) fileBuffer.concat((const char*)buf, n);
        else if (outFile) outFile.write(buf, n);
      }

      void feedBase64Char(char c) {
        if (skipCurrentFile) return;
        b64group[b64groupLen++] = (uint8_t)c;
        if (b64groupLen == 4) {
          uint8_t raw[3];
          size_t outLen = 0;
          if (mbedtls_base64_decode(raw, sizeof(raw), &outLen, b64group, 4) == 0) {
            emitDecoded(raw, outLen);
          }
          b64groupLen = 0;
        }
      }

      void finishFileValue() {
        if (!skipCurrentFile) {
          if (doReplace) {
            normalizeControllerIds(fileBuffer, targetChipId);
            File f = SPIFFS.open(currentFilename, "w");
            if (f) {
              f.print(fileBuffer);
              f.close();
              written++;
              Serial.printf("[BACKUP ] Restored %s (%u bytes) [chip ID -> %s]\n",
                            currentFilename.c_str(), (unsigned)fileBuffer.length(), targetChipId.c_str());
            } else {
              Serial.printf("[BACKUP ]   - %s skipped (could not open for write)\n", currentFilename.c_str());
            }
          } else if (outFile) {
            outFile.close();
            written++;
            Serial.printf("[BACKUP ] Restored %s (%u bytes)\n", currentFilename.c_str(), (unsigned)decodedLen);
          }
        }
        outFile = File();
        fileBuffer = String();
      }

      void processByte(char c) {
        switch (phase) {
          case RS_KEY_START:
            if (c == '"') { key = String(); phase = RS_KEY; }
            else if (c == '}') phase = RS_DONE;
            break;
          case RS_KEY:
            if (c == '"') phase = RS_COLON;
            else key += c;
            break;
          case RS_COLON:
            if (c == ':') phase = RS_VALUE_START;
            break;
          case RS_VALUE_START:
            if (c == '"') {
              if (key.startsWith("_")) { metaValue = String(); phase = RS_META_STRING; }
              else { beginFileValue(); phase = RS_FILE_STRING; }
            } else {
              metaValue = String(c);
              phase = RS_META_BARE;
            }
            break;
          case RS_META_STRING:
            if (c == '"') { finishMetaValue(); phase = RS_COMMA; }
            else metaValue += c;
            break;
          case RS_META_BARE:
            if (c == ',' || c == '}') {
              finishMetaValue();
              phase = (c == '}') ? RS_DONE : RS_KEY_START;
            } else {
              metaValue += c;
            }
            break;
          case RS_FILE_STRING:
            if (c == '"') { finishFileValue(); phase = RS_COMMA; }
            else feedBase64Char(c);
            break;
          case RS_COMMA:
            if (c == ',') phase = RS_KEY_START;
            else if (c == '}') phase = RS_DONE;
            break;
          case RS_DONE:
          default:
            break;
        }
      }
    };

    AsyncWeb_server.on("/api/restore", HTTP_POST,
      [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        auto *st = reinterpret_cast<RestoreState*>(request->_tempObject);
        request->_tempObject = nullptr;

        bool ok = false;
        String message;
        if (!st) {
          message = F("No data received");
        } else if (!st->sawVersion) {
          message = F("Not a recognized backup file");
        } else {
          ok = st->written > 0;
          message = String(st->written) + " file(s) restored";
          if (st->doReplace) message += " (controller ID set to " + st->targetChipId + ")";
        }

        StaticJsonDocument<192> resJson;
        resJson["ok"] = ok;
        resJson["message"] = message;
        char payload[192]; serializeJson(resJson, payload, sizeof(payload));
        request->send(200, "application/json", payload);

        if (ok) {
          Serial.println(F("[BACKUP ] Restore complete, rebooting.."));
          restartRequired = true;
        }
        delete st;
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        RestoreState *st;
        if (!index) {
          st = new RestoreState();
          st->chipIdModeParam = request->hasParam("chipIdMode") ?
                                request->getParam("chipIdMode")->value() : "keep";
          st->restoreWeb = request->hasParam("restoreWeb") &&
                           request->getParam("restoreWeb")->value() == "1";
          request->_tempObject = st;
          if (st->restoreWeb) {
            // Collect all non-JSON filenames first, then delete, so we
            // don't modify the directory while the iterator is open.
            std::vector<String> toDelete;
            File root = SPIFFS.open("/");
            for (File e = root.openNextFile(); e; e = root.openNextFile()) {
              String name = e.name();
              if (!name.startsWith("/")) name = "/" + name;
              if (!name.endsWith(".json")) toDelete.push_back(std::move(name));
            }
            root.close();
            for (const String &n : toDelete) SPIFFS.remove(n);
            Serial.printf("[BACKUP ] Restore: cleared %u web file(s) before restore\n",
                          (unsigned)toDelete.size());
          }
          Serial.printf("[BACKUP ] Restore upload starting (%u bytes, streaming)\n", (unsigned)total);
        } else {
          st = reinterpret_cast<RestoreState*>(request->_tempObject);
        }
        if (!st) return;
        for (size_t i = 0; i < len; i++) st->processByte((char)data[i]);
      }
    );

    // ── POST /api/resetconfig — wipe and recreate every config JSON file ──
    // Deletes all per-submodule config files (system, IRMAP, relays, temp,
    // ADS1x15, HST, water flow sensor) and recreates each with this board's
    // CHIPID-based defaults via initializeMissingConfigFiles(), then reboots
    // so the freshly written defaults load through the normal boot sequence.
    AsyncWeb_server.on("/api/resetconfig", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      Serial.println(F("[SYSTEM ] Resetting all configuration files to defaults..."));

      SPIFFS.remove("/config.json");
      SPIFFS.remove(IRMapfilename);
      SPIFFS.remove("/tempconfig.json");
      for (size_t i = 0; i < relays.size(); i++) {
        char relayfilename[20];
        mkRelayConfigName(relayfilename, (uint8_t)i);
        SPIFFS.remove(relayfilename);
      }
      #ifdef _ADS1X15_
      SPIFFS.remove("/ADS11x5Config.json");
      #endif
      #ifdef _HST_
      SPIFFS.remove("/HSTConfig.json");
      #endif
      #ifdef WaterFlowSensor
      SPIFFS.remove("/WaterFlowSensorConfig.json");
      #endif

      // Leave timer/rule schedules themselves intact (so the user isn't forced
      // to re-enter them after a reset) but disable every one — their relay
      // targets and gating conditions were defined against the configuration
      // that was just wiped, so leaving them enabled could drive relays in
      // ways the now-default config doesn't expect.
      for (uint8_t i = 1; i <= MAX_NUMBER_OF_TIMERS; i++) {
        char timerfilename[20];
        snprintf(timerfilename, sizeof(timerfilename), "/timer%u.json", (unsigned)i);
        disableJsonEnableFlag(timerfilename, "CEnabled");
      }
      #ifdef _AUTOMATION_RULES_
      for (uint8_t i = 1; i <= MAX_NUMBER_OF_AUTOMATION_RULES; i++) {
        char rulefilename[20];
        snprintf(rulefilename, sizeof(rulefilename), "/rule%u.json", (unsigned)i);
        disableJsonEnableFlag(rulefilename, "REnabled");
      }
      #endif

      initializeMissingConfigFiles();

      StaticJsonDocument<128> resJson;
      resJson["ok"] = true;
      resJson["message"] = "Configuration reset to defaults";
      char payload[128]; serializeJson(resJson, payload, sizeof(payload));
      request->send(200, "application/json", payload);

      Serial.println(F("[SYSTEM ] Configuration reset complete, rebooting.."));
      restartRequired = true;
    });

    // ── File manager: list/view/edit/upload/delete/rename any file on
    // LittleFS — replaces SPIFFSEditor with routes that follow this
    // project's JSON-API conventions and stay within the same bounded-memory
    // discipline as the rest of this file: GET /content streams straight from
    // flash via request->send(SPIFFS,...) and POST /content writes straight to
    // flash as the upload streams through onBody — neither ever buffers a
    // whole file in RAM, so editing/uploading large web-UI pages is as safe as
    // editing a small JSON config. isValidFilesApiPath() (above) guards every
    // client-supplied path against traversal.
    AsyncWeb_server.on("/api/files/list", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.printf("[FILES  ] /api/files/list hit from %s\n", request->client()->remoteIP().toString().c_str());
      if (!request->authenticate("user", "pass")) {
        Serial.println(F("[FILES  ] /api/files/list: auth failed, sending 401"));
        return request->requestAuthentication();
      }

      uint32_t freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      Serial.printf("[FILES  ] heap free block: %u\n", freeBlock);
      if (freeBlock < 2048) {
        Serial.println(F("[FILES  ] heap too fragmented, sending 507"));
        request->send(507, "application/json", F("{\"error\":\"heap too fragmented\"}"));
        return;
      }
      StaticJsonDocument<6144> doc;
      doc["fsTotal"] = (uint32_t)SPIFFS.totalBytes();
      doc["fsUsed"]  = (uint32_t)SPIFFS.usedBytes();
      Serial.printf("[FILES  ] LittleFS total=%u used=%u\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
      JsonArray arr = doc.createNestedArray("files");
      File root = SPIFFS.open("/");
      if (!root) {
        Serial.println(F("[FILES  ] SPIFFS.open('/') failed"));
      } else {
        int fileCount = 0;
        for (File e = root.openNextFile(); e; e = root.openNextFile()) {
          if (e.isDirectory()) continue;
          String name = e.name();
          if (!name.startsWith("/")) name = "/" + name;
          JsonObject o = arr.createNestedObject();
          o["name"] = name;
          o["size"] = (uint32_t)e.size();
          fileCount++;
        }
        root.close();
        Serial.printf("[FILES  ] enumerated %d files, doc overflowed=%s\n", fileCount, doc.overflowed() ? "YES" : "no");
      }
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      if (!response) {
        Serial.println(F("[FILES  ] beginResponseStream returned null, sending 500"));
        request->send(500, "application/json", F("{\"error\":\"out of memory\"}"));
        return;
      }
      size_t written = serializeJson(doc, *response);
      Serial.printf("[FILES  ] serialized %u bytes, sending response\n", (unsigned)written);
      request->send(response);
    });

    AsyncWeb_server.on("/api/files/content", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      String path = request->hasParam("path") ? request->getParam("path")->value() : String();
      if (!isValidFilesApiPath(path) || !SPIFFS.exists(path)) {
        request->send(404, "text/plain", "Not found");
        return;
      }
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, path, "text/plain");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "no-store");
      request->send(r);
    });

    AsyncWeb_server.on("/api/files/delete", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      String path = request->hasParam("path", true) ? request->getParam("path", true)->value() : String();

      StaticJsonDocument<160> res;
      if (!isValidFilesApiPath(path) || !SPIFFS.exists(path)) {
        res["ok"] = false; res["message"] = "File not found";
      } else if (SPIFFS.remove(path)) {
        res["ok"] = true; res["message"] = "Deleted " + path;
        Serial.printf("[FILES  ] Deleted %s\n", path.c_str());
      } else {
        res["ok"] = false; res["message"] = "Could not delete " + path;
      }
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      serializeJson(res, *response);
      request->send(response);
    });

    AsyncWeb_server.on("/api/files/rename", HTTP_POST, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      String from = request->hasParam("from", true) ? request->getParam("from", true)->value() : String();
      String to   = request->hasParam("to",   true) ? request->getParam("to",   true)->value() : String();

      StaticJsonDocument<192> res;
      if (!isValidFilesApiPath(from) || !isValidFilesApiPath(to) || !SPIFFS.exists(from)) {
        res["ok"] = false; res["message"] = "Invalid source or destination path";
      } else if (SPIFFS.exists(to)) {
        res["ok"] = false; res["message"] = "A file already exists at " + to;
      } else if (SPIFFS.rename(from, to)) {
        res["ok"] = true; res["message"] = "Renamed to " + to;
        Serial.printf("[FILES  ] Renamed %s -> %s\n", from.c_str(), to.c_str());
      } else {
        res["ok"] = false; res["message"] = "Rename failed";
      }
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      serializeJson(res, *response);
      request->send(response);
    });

    // POST /api/files/content?path=<dest> — saves edits to an existing file or
    // uploads a brand-new one (creates/overwrites the destination either way).
    // A small heap-allocated state struct (mirroring RestoreState just above)
    // carries the open File handle across onBody calls via request->_tempObject.
    struct FileSaveState {
      File file;
      String path;
      size_t written = 0;
    };

    AsyncWeb_server.on("/api/files/content", HTTP_POST,
      [](AsyncWebServerRequest *request){
        auto *st = reinterpret_cast<FileSaveState*>(request->_tempObject);
        request->_tempObject = nullptr;

        StaticJsonDocument<192> res;
        if (!st) {
          res["ok"] = false; res["message"] = "No data received or invalid path";
        } else {
          st->file.close();
          res["ok"] = true; res["message"] = String(st->written) + " byte(s) written to " + st->path;
          Serial.printf("[FILES  ] Wrote %s (%u bytes)\n", st->path.c_str(), (unsigned)st->written);
          delete st;
        }
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        serializeJson(res, *response);
        request->send(response);
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        FileSaveState *st;
        if (!index) {
          st = nullptr;
          String path = request->hasParam("path") ? request->getParam("path")->value() : String();
          if (isValidFilesApiPath(path)) {
            File f = SPIFFS.open(path, "w");
            if (f) {
              st = new FileSaveState();
              st->file = f;
              st->path = path;
            }
          }
          request->_tempObject = st;
          if (st) Serial.printf("[FILES  ] Save upload starting (%u bytes, streaming) -> %s\n",
                                (unsigned)total, st->path.c_str());
        } else {
          st = reinterpret_cast<FileSaveState*>(request->_tempObject);
        }
        if (!st) return;
        st->written += st->file.write(data, len);
      }
    );

    AsyncWeb_server.on("/VPNConfigSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      saveWireGuardConfig(request);
      loadWireGuardConfig("/WireGuardConfig.json", PWireGuardConfig);
      wireGuardRequestApplyConfig();
      request->send(SPIFFS, "/VPNConfigSave.html");
    });

    AsyncWeb_server.on("/GetConfig.json", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

      uint8_t relayNb = AppliedRelayNumber;
      if (request->hasParam("relay"))
        relayNb = (uint8_t)constrain(request->getParam("relay")->value().toInt(), 0, 3);
      Relay *r  = getrelaybynumber(relayNb);
      Relay *r0 = getrelaybynumber(0);
      Relay *r1 = getrelaybynumber(1);

      if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 2048) {
        request->send(507, "application/json", F("{\"error\":\"heap too fragmented\"}"));
        return;
      }
      // StaticJsonDocument on async_tcp task stack (16 KB) — eliminates the
      // heap TOCTOU race that caused NULL-deref crashes (exception cause 28).
      StaticJsonDocument<4096> doc;

      char _tb[24], _ub[20];
      digitalClockDisplay(_tb, sizeof(_tb));
      uptimeDisplay(_ub, sizeof(_ub));
      // Compute CID once — avoids two separate String alloc/frees and the
      // 3-temp-String concatenation chain that formed "MAC - Chip id: CID".
      String cid = CID();
      doc["NODEID"]  = cid;
      { char macbuf[64];
        snprintf(macbuf, sizeof(macbuf), "%s - Chip id: %s", MAC.c_str(), cid.c_str());
        doc["MACADDR"] = macbuf; }
      doc["systemtime"] = _tb;
      doc["uptime"]     = _ub;
      doc["heap"]       = (uint32_t)ESP.getFreeHeap();
      doc["PhyLoc"]     = MyConfParam.v_PhyLoc;
      doc["ssid"]       = MyConfParam.v_ssid;
      doc["pass"]       = MyConfParam.v_pass;
      doc["mqttUser"]   = MyConfParam.v_mqttUser;
      doc["mqttPass"]   = MyConfParam.v_mqttPass;
      doc["RWD"]        = MyConfParam.v_Reboot_on_WIFI_Disconnection;
      doc["PRST"]       = MyConfParam.v_PRST;
      doc["DailyRestartEnabled"] = MyConfParam.v_DailyRestartEnabled;
      doc["DailyRestartTime"]    = MyConfParam.v_DailyRestartTime;
      doc["timeserver"] = MyConfParam.v_timeserver;
      doc["NTP_UseVPN"] = MyConfParam.v_NTP_UseVPN ? 1 : 0;
      doc["ntptz"]      = MyConfParam.v_ntptz;
      doc["Pingserver"] = MyConfParam.v_Pingserver.toString();
      doc["Screen_orientation"]   = MyConfParam.v_Screen_orientation;
      doc["OLEDUpdateIntervalMs"] = MyConfParam.v_OLEDUpdateIntervalMs;
      doc["OLEDAlwaysOn"]         = MyConfParam.v_OLEDAlwaysOn ? 1 : 0;
      doc["OLED_HW_Active"]       = MyConfParam.v_OLED_HW_Active ? 1 : 0;
      doc["ADS_HW_Active"]        = MyConfParam.v_ADS_HW_Active  ? 1 : 0;

      doc["MQTT_Active"]           = MyConfParam.v_MQTT_Active;
      doc["MQTT_UseVPN"]           = MyConfParam.v_MQTT_UseVPN ? 1 : 0;
      doc["MQTT_BROKER"]           = MyConfParam.v_MQTT_BROKER;
      doc["MQTT_B_PRT"]            = MyConfParam.v_MQTT_B_PRT;
      doc["MQTT_KeepAliveSeconds"] = MyConfParam.v_MQTT_KeepAliveSeconds;
      doc["MQTT_STATUS"]           = mqttStatusText();
      doc["MQTT_ROUTE"]            = mqttRouteText();
      { char _srv[80]; snprintf(_srv, sizeof(_srv), "%s:%d",
          MyConfParam.v_MQTT_BROKER.c_str(), (int)MyConfParam.v_MQTT_B_PRT);
        doc["MQTT_CONNECTED_SERVER"] = _srv; }
      doc["MQTT_HEALTH_TOPIC"]     = mqttHealthTopicStorage.c_str();

      doc["TOGGLE_BTN_PUB_TOPIC"] = MyConfParam.v_TOGGLE_BTN_PUB_TOPIC;
      doc["I0MODE"] = MyConfParam.v_IN0_INPUTMODE;
      doc["I1MODE"] = MyConfParam.v_IN1_INPUTMODE;
      doc["I2MODE"] = MyConfParam.v_IN2_INPUTMODE;
      doc["I3MODE"] = MyConfParam.v_IN3_INPUTMODE;
      doc["I4MODE"] = MyConfParam.v_IN4_INPUTMODE;
      doc["I5MODE"] = MyConfParam.v_IN5_INPUTMODE;
      doc["I6MODE"] = MyConfParam.v_IN6_INPUTMODE;
      doc["I12_STS_PTP"] = MyConfParam.v_InputPin12_STATE_PUB_TOPIC;
      doc["I14_STS_PTP"] = MyConfParam.v_InputPin14_STATE_PUB_TOPIC;
      doc["I03_STS_PTP"] = MyConfParam.v_InputPin03_STATE_PUB_TOPIC;
      doc["I04_STS_PTP"] = MyConfParam.v_InputPin04_STATE_PUB_TOPIC;
      doc["I05_STS_PTP"] = MyConfParam.v_InputPin05_STATE_PUB_TOPIC;
      doc["I06_STS_PTP"] = MyConfParam.v_InputPin06_STATE_PUB_TOPIC;

      // Store floats directly — ArduinoJson serializes them natively,
      // eliminating ~8 String heap allocs per 10-second poll.
      doc["ATEMP1"] = (double)MCelcius;
      doc["ATEMP2"] = (double)MCelcius2;
      doc["ACS1"]   = (r0 && r0->RelayConfParam->v_ACS_Active) ? (double)ACS_I_Current : 0.0;
      doc["ACS2"]   = (r1 && r1->RelayConfParam->v_ACS_Active) ? (double)ACS_I_Current : 0.0;
      #ifdef _emonlib_
      doc["CT_VOLTAGE"] = (double)CT_1.supplyVoltage;
      doc["CT_CURRENT"] = (double)CT_1.Irms;
      doc["CT_POWER"]   = (double)CT_1.realPower;
      doc["CT_PF"]      = (double)CT_1.powerFactor;
      #else
      doc["CT_VOLTAGE"] = "NA";  doc["CT_CURRENT"] = "NA";
      doc["CT_POWER"]   = "NA";  doc["CT_PF"]      = "NA";
      #endif
      doc["RSTATE0"] = (r0 && r0->readrelay() == HIGH) ? "ON" : "OFF";
      doc["RSTATE1"] = (r1 && r1->readrelay() == HIGH) ? "ON" : "OFF";

      doc["Sonar_distance"]      = MyConfParam.v_Sonar_distance;
      doc["Sonar_distance_max"]  = MyConfParam.v_Sonar_distance_max;
      doc["CurrentTransformerTopic"]        = MyConfParam.v_CurrentTransformerTopic;
      doc["CurrentTransformer_max_current"] = MyConfParam.v_CurrentTransformer_max_current;
      doc["calibration"]        = MyConfParam.v_calibration;
      doc["PhaseCal"]           = MyConfParam.v_PhaseCal;
      doc["ToleranceOffTime"]   = MyConfParam.v_ToleranceOffTime;
      doc["ToleranceOnTime"]    = MyConfParam.v_ToleranceOnTime;
      doc["CT_MaxAllowed_current"] = MyConfParam.v_CT_MaxAllowed_current;
      doc["CT_adjustment"]         = MyConfParam.v_CT_adjustment;
      doc["CT_saveThreshold"]      = MyConfParam.v_CT_saveThreshold;
      doc["CT_ZeroLoadCurrentMax"] = (double)MyConfParam.v_CT_ZeroLoadCurrentMax;
      doc["CT_ZeroLoadPowerMin"]   = (double)MyConfParam.v_CT_ZeroLoadPowerMin;
      doc["CT_ZeroLoadPowerMax"]   = (double)MyConfParam.v_CT_ZeroLoadPowerMax;
      doc["EmonReaderIntervalMs"]  = MyConfParam.v_EmonReaderIntervalMs;
      doc["EmonPublisherIntervalMs"] = MyConfParam.v_EmonPublisherIntervalMs;

      if (r) {
        doc["RELAYNB"]           = r->RelayConfParam->v_relaynb;
        doc["PUB_TOPIC1"]        = r->RelayConfParam->v_PUB_TOPIC1;
        doc["STATE_PUB_TOPIC"]   = r->RelayConfParam->v_STATE_PUB_TOPIC;
        doc["TTL_PUB_TOPIC"]     = r->RelayConfParam->v_ttl_PUB_TOPIC;
        doc["CURR_TTL_PUB_TOPIC"] = r->RelayConfParam->v_CURR_TTL_PUB_TOPIC;
        doc["i_ttl_PUB_TOPIC"]   = r->RelayConfParam->v_i_ttl_PUB_TOPIC;
        doc["ACS_AMPS"]          = r->RelayConfParam->v_ACS_AMPS;
        doc["LWILL_TOPIC"]       = r->RelayConfParam->v_LWILL_TOPIC;
        doc["SUB_TOPIC1"]        = r->RelayConfParam->v_SUB_TOPIC1;
        doc["TemperatureValue"]  = r->RelayConfParam->v_TemperatureValue;
        doc["AlexaName"]         = r->RelayConfParam->v_AlexaName;
        doc["ttl"]               = r->RelayConfParam->v_ttl;
        doc["tta"]               = r->RelayConfParam->v_tta;
        doc["ACS_elasticity"]    = r->RelayConfParam->v_ACS_elasticity;
        doc["Max_Current"]       = r->RelayConfParam->v_Max_Current;
        doc["ACS_Sensor_Model"]  = r->RelayConfParam->v_ACS_Sensor_Model;
        doc["ACS_Active"]        = r->RelayConfParam->v_ACS_Active ? 1 : 0;
        bool hasAny = false, hasOther = false;
        for (auto it : relays) {
          Relay *rv = static_cast<Relay*>(it);
          if (!rv || !rv->RelayConfParam) continue;
          const String& tv = rv->RelayConfParam->v_TemperatureValue;
          if (tv.length() > 0 && tv != "0") { hasAny = true; if (rv != r) hasOther = true; }
        }
        doc["HasTempRelay"]      = hasAny  ? 1 : 0;
        doc["HasOtherTempRelay"] = hasOther ? 1 : 0;
      } else {
        doc["RELAYNB"] = relayNb;
      }

      #ifdef WaterFlowSensor
      doc["WaterFlowSensor"] = 1;
      #else
      doc["WaterFlowSensor"] = 0;
      #endif
      #ifdef _pressureSensor_
      doc["PressureSensor"] = 1;
      #else
      doc["PressureSensor"] = 0;
      #endif
      #ifdef _ADS1X15_
      doc["ADS1X15"] = 1;
      #else
      doc["ADS1X15"] = 0;
      #endif
      #ifdef _HST_
      doc["HST"] = 1;
      #else
      doc["HST"] = 0;
      #endif
      #ifdef _emonlib_
      doc["EmonLib"] = 1;
      #else
      doc["EmonLib"] = 0;
      #endif
      #ifdef OLED_1306
      doc["OLED1306"] = 1;
      #else
      doc["OLED1306"] = 0;
      #endif

      // Stream JSON directly to the response buffer — avoids allocating
      // a ~2 KB String payload just to immediately send and discard it.
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      serializeJson(doc, *response);
      request->send(response);
    });

    AsyncWeb_server.on("/wscontrol.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          AppliedRelayNumber = 0;
          if (request->hasParam("GETRELAYNB")) {
              String t = request->getParam("GETRELAYNB")->value();
              AppliedRelayNumber = t.toInt();
          
          if (request->hasParam("RELAYACTION")) {
            String msg = request->getParam("RELAYACTION")->value();
            Relay * rtmp = getrelaybynumber(AppliedRelayNumber);
            if (rtmp) {
              if (msg == "ON")  rtmp->mdigitalWrite(rtmp->getRelayPin(), HIGH);
              if (msg == "OFF") rtmp->mdigitalWrite(rtmp->getRelayPin(), LOW);
            }
          }
          }
          request->send(SPIFFS, "/wscontrol.html", String(), false, processor);
    });

    // ── GET /api/config — return current config.json ─────────────────────────
    AsyncWeb_server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      if (SPIFFS.exists("/config.json")) {
        request->send(SPIFFS, "/config.json", "application/json");
      } else {
        request->send(404, "application/json", F("{\"ok\":false,\"error\":\"config.json not found\"}"));
      }
    });

    // ── POST /api/config — receive config JSON, save and reload ───────────────
    AsyncWeb_server.on("/api/config", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
        String* body = (String*)request->_tempObject;
        request->_tempObject = nullptr;           // claim before delete — disconnect handler sees null
        if (!body || body->isEmpty()) {
          delete body;
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"empty body\"}"));
          return;
        }

        if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 2048) {
          request->send(507, "application/json", F("{\"ok\":false,\"error\":\"heap too fragmented, retry\"}"));
          return;
        }

        // Validate JSON before writing
        StaticJsonDocument<3072> doc;
        DeserializationError err = deserializeJson(doc, *body);
        delete body;
        if (err) {
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"json parse error\"}"));
          return;
        }
        // Require at least ssid to be present as a basic sanity check
        if (!doc.containsKey("ssid")) {
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"missing required field: ssid\"}"));
          return;
        }

        // Write to config.json
        File f = SPIFFS.open("/config.json", "w");
        if (!f) {
          request->send(500, "application/json", F("{\"ok\":false,\"error\":\"failed to open config.json\"}"));
          return;
        }
        serializeJsonPretty(doc, f);
        f.print("\n");
        f.flush();
        f.close();

        // Reload and apply — same sequence as /Apply.html
        loadConfig(MyConfParam);
        #if defined (_emonlib_) || defined (_pressureSensor_)
        applyPowerTaskIntervals();
        #endif
        #ifdef OLED_1306
        applyOledTaskInterval();
        #endif
        applyMqttActiveState();
        setupInputs();
        clearIRMap();
        loadIRMapConfig(myIRMap);

        request->send(200, "application/json", F("{\"ok\":true,\"message\":\"config saved and reloaded\"}"));
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 2048) return;
          request->_tempObject = new(std::nothrow) String();
          request->onDisconnect([request]() {
            if (request->_tempObject) {
              delete (String*)request->_tempObject;
              request->_tempObject = nullptr;
            }
          });
        }
        String* body = (String*)request->_tempObject;
        if (body) body->concat((char*)data, len);
      }
    );

    // ── GET /api/heap — lightweight heap stats for topbar on all pages ───────
    AsyncWeb_server.on("/api/heap", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      char buf[80];
      snprintf(buf, sizeof(buf), "{\"heap\":%lu,\"heapMin\":%lu,\"heapMaxBlk\":%lu}",
               (unsigned long)ESP.getFreeHeap(),
               (unsigned long)ESP.getMinFreeHeap(),
               (unsigned long)g_heapMaxBlkCached);
      request->send(200, "application/json", buf);
    });

    // ── GET /api/id ──────────────────────────────────────────────────────────
    AsyncWeb_server.on("/api/id", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      String cid = CID();
      char payload[160];
      snprintf(payload, sizeof(payload), "{\"nodeId\":\"%s\",\"mac\":\"%s\",\"location\":\"%s\"}",
               cid.c_str(), MAC.c_str(), MyConfParam.v_PhyLoc.c_str());
      request->send(200, "application/json", payload);
    });

    // ── GET /api/relay/state — returns all relay states for live polling ─────
    AsyncWeb_server.on("/api/relay/state", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
      StaticJsonDocument<256> doc;
      JsonArray arr = doc.createNestedArray("relays");
      for (size_t i = 0; i < relays.size(); i++) {
        Relay *r = getrelaybynumber((uint8_t)i);
        JsonObject o = arr.createNestedObject();
        o["id"]    = (int)i;
        o["state"] = (r && r->readrelay() == HIGH) ? "ON" : "OFF";
      }
      char payload[128];
      serializeJson(doc, payload, sizeof(payload));
      request->send(200, "application/json", payload);
    });

    // ── POST /api/relay  {"relay":0,"state":"ON"|"OFF"} ─────────────────────
    AsyncWeb_server.on("/api/relay", HTTP_POST,
      // Request handler — fires after all body chunks are received
      [](AsyncWebServerRequest *request) {
        if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
        String* body = (String*)request->_tempObject;
        request->_tempObject = nullptr;
        if (!body || body->isEmpty()) {
          delete body;
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"empty body\"}"));
          return;
        }
        StaticJsonDocument<192> doc;
        DeserializationError err = deserializeJson(doc, *body);
        delete body;
        if (err) {
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"json parse error\"}"));
          return;
        }
        int nb = doc["relay"] | 0;
        const char* st = doc["state"] | "";
        Relay *r = getrelaybynumber(nb);
        if (r) {
          if (strcmp(st, "ON")  == 0) r->mdigitalWrite(r->getRelayPin(), HIGH);
          if (strcmp(st, "OFF") == 0) r->mdigitalWrite(r->getRelayPin(), LOW);
        }
        bool on = r && (r->readrelay() == HIGH);
        char resp[48];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"relay\":%d,\"state\":\"%s\"}", nb, on ? "ON" : "OFF");
        request->send(200, "application/json", resp);
      },
      nullptr,
      // Body chunk accumulator
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          request->_tempObject = new String();
          request->onDisconnect([request]() {
            if (request->_tempObject) {
              delete (String*)request->_tempObject;
              request->_tempObject = nullptr;
            }
          });
        }
        String* body = (String*)request->_tempObject;
        if (body) body->concat((char*)data, len);
      }
    );

    // ── POST /api/timer  {"relay":0,"action":"pause"|"resume"} ──────────────
    AsyncWeb_server.on("/api/timer", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!request->authenticate("user", "pass")) { request->requestAuthentication(); return; }
        String* body = (String*)request->_tempObject;
        request->_tempObject = nullptr;
        if (!body || body->isEmpty()) {
          delete body;
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"empty body\"}"));
          return;
        }
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, *body);
        delete body;
        if (err) {
          request->send(400, "application/json", F("{\"ok\":false,\"error\":\"json parse error\"}"));
          return;
        }
        int nb           = doc["relay"]  | 0;
        const char* act  = doc["action"] | "";
        Relay *r = getrelaybynumber(nb);
        if (r) {
          if (strcmp(act, "pause")  == 0) r->timerpaused = true;
          if (strcmp(act, "resume") == 0) r->timerpaused = false;
        }
        bool paused = r && r->timerpaused;
        char resp[48];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"relay\":%d,\"paused\":%s}", nb, paused ? "true" : "false");
        request->send(200, "application/json", resp);
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          request->_tempObject = new String();
          request->onDisconnect([request]() {
            if (request->_tempObject) {
              delete (String*)request->_tempObject;
              request->_tempObject = nullptr;
            }
          });
        }
        String* body = (String*)request->_tempObject;
        if (body) body->concat((char*)data, len);
      }
    );

    AsyncWeb_server.on("/RelayControl.json", HTTP_GET, [](AsyncWebServerRequest *request){
      // Avoid allocating the JSONP callback String on every call — only do it when
      // the ?callback= param is actually present (rare; LiveReadings uses plain XHR).
      bool isJsonp = request->hasParam("callback");
      if (isJsonp) {
        if (!_jsonpAuth(request)) { request->send(401, "application/javascript", "/* Unauthorized */"); return; }
      } else {
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      }

      int nb = 0;
      // Store action as pointer into the request param — no heap alloc needed.
      const char* action = "";
      if (request->hasParam("relay"))  nb     = request->getParam("relay")->value().toInt();
      if (request->hasParam("action")) action  = request->getParam("action")->value().c_str();

      Relay *r = getrelaybynumber(nb);
      if (r && *action) {
        if (strcmp(action, "ON")  == 0) r->mdigitalWrite(r->getRelayPin(), HIGH);
        if (strcmp(action, "OFF") == 0) r->mdigitalWrite(r->getRelayPin(), LOW);
      }
      bool state = r ? (r->readrelay() == HIGH) : false;
      char body[64];
      snprintf(body, sizeof(body), "{\"ok\":true,\"relay\":%d,\"state\":\"%s\"}",
               nb, state ? "ON" : "OFF");

      if (isJsonp) { String cb = _jsonpCB(request); sendJSONP(request, cb, body); return; }

      // Connection: close — relay toggle is a one-shot command, not a data stream.
      // Without this the browser keeps the TCP connection open for ~3–5 minutes
      // (HTTP/1.1 keep-alive), holding 7–8 KB of lwIP PCB + send/recv buffers per
      // connection. With 4–5 rapid toggles that adds up to ~37 KB stuck for minutes.
      { AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", body);
        if (request->hasHeader("Origin")) resp->addHeader("Access-Control-Allow-Origin", request->header("Origin"));
        else                              resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Vary",       "Origin");
        resp->addHeader("Connection", "close");
        request->send(resp); }
    });

    AsyncWeb_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
       if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        AppliedRelayNumber = 0;
        webing = true;
        if (request->hasParam("GETRELAYNB")) {
            String t = request->getParam("GETRELAYNB")->value();
            AppliedRelayNumber = t.toInt();
            Relay * rtmp =  getrelaybynumber(AppliedRelayNumber);
            if (rtmp != nullptr ) {
              if (request->hasParam("RELAYACTION")) {
                String msg = request->getParam("RELAYACTION")->value();
                if (msg == "ON") {
                  rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH); 
                  #ifdef StepperMode
                  steperrun = ! steperrun;
                  shadeStepper.setCurrentPosition(0);
                  #endif
                }
                if (msg == "OFF") {
                 rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW); 
                }
              }
            } else {
              AppliedRelayNumber = 0; // revert to relay 0 if relay n is not found 
            }
        } 
        // Config.html has no %VAR% substitutions — serve as a static file.
        // This enables AsyncWebServer's built-in ETag/304 handling and allows
        // the browser to cache the HTML shell (dynamic config data comes from
        // /GetConfig.json via JS, so caching the HTML is always safe).
        { AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/config.html", "text/html");
          if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
          r->addHeader("Cache-Control", "max-age=60");
          request->send(r); }
        webing = false;
    }
    );

    AsyncWeb_server.on("/LiveReadings.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/LiveReadings.html", "text/html");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "max-age=60");
      request->send(r);
    });

    AsyncWeb_server.on("/LiveReadingsRest.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AsyncWebServerResponse *r = request->beginResponse(SPIFFS, "/LiveReadingsRest.html", "text/html");
      if (!r) { request->send(503, "text/plain", "File not found — reflash filesystem"); return; }
      r->addHeader("Cache-Control", "max-age=60");
      request->send(r);
    });

    
    AsyncWeb_server.on("/TimerStatus.json", HTTP_GET, [](AsyncWebServerRequest *request){
      String cb = _jsonpCB(request);
      if (cb.length()) {
        if (!_jsonpAuth(request)) { request->send(401, "application/javascript", "/* Unauthorized */"); return; }
      } else {
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      }

      int nowSecs = hour() * 3600 + minute() * 60 + second();

      char timeBuf[10];
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", hour(), minute(), second());

      StaticJsonDocument<2560> doc;
      doc["currentTime"] = timeBuf;
      JsonArray arr = doc.createNestedArray("timers");

      const char* typeNames[] = {"Unknown","Date Range","Daily","Weekly","Monthly","Yearly"};

      for (int i = 1; i <= MAX_NUMBER_OF_TIMERS; i++) {
        NodeTimer t(i);
        char fname[20];
        snprintf(fname, sizeof(fname), "/timer%d.json", i);

        JsonObject obj = arr.createNestedObject();
        obj["id"] = i;

        if (loadNodeTimer(fname, t) != SUCCESS) {
          obj["exists"] = false;
          continue;
        }

        obj["exists"]   = true;
        obj["enabled"]  = t.enabled ? 1 : 0;
        obj["relay"]    = t.relay;
        obj["type"]     = (int)t.TM_type;
        obj["typeName"] = ((int)t.TM_type <= 5) ? typeNames[(int)t.TM_type] : "Unknown";
        obj["dateFrom"] = t.spanDatefrom;
        obj["dateTo"]   = t.spanDateto;
        obj["timeFrom"] = t.spantimefrom;
        obj["timeTo"]   = t.spantimeto;
        obj["markHours"]   = t.Mark_Hours;
        obj["markMinutes"] = t.Mark_Minutes;
        obj["monthDay"]    = t.monthDay;

        JsonObject days = obj.createNestedObject("days");
        if (t.weekdays) {
          days["Mo"] = t.weekdays->Monday;
          days["Tu"] = t.weekdays->Tuesday;
          days["We"] = t.weekdays->Wednesday;
          days["Th"] = t.weekdays->Thursday;
          days["Fr"] = t.weekdays->Friday;
          days["Sa"] = t.weekdays->Saturday;
          days["Su"] = t.weekdays->Sunday;
        }

        Relay *r = getrelaybynumber(t.relay);
        bool timerActive = r && r->hastimerrunning;
        bool timerPaused = r && r->timerpaused;
        bool relayOn     = r && (r->readrelay() == HIGH);

        obj["relayState"]  = relayOn ? "ON" : "OFF";
        obj["timerActive"] = timerActive;
        obj["timerPaused"] = timerPaused;

        int fromH = 0, fromM = 0, toH = 0, toM = 0;
        sscanf(t.spantimefrom.c_str(), "%d:%d", &fromH, &fromM);
        sscanf(t.spantimeto.c_str(),   "%d:%d", &toH,   &toM);
        int fromSecs = fromH * 3600 + fromM * 60;
        int toSecs;
        if (t.Mark_Hours + t.Mark_Minutes > 0) {
          toSecs = fromSecs + (int)t.Mark_Hours * 3600 + (int)t.Mark_Minutes * 60;
          char effTimeTo[6];
          snprintf(effTimeTo, sizeof(effTimeTo), "%02d:%02d",
                   (toSecs % 86400) / 3600, (toSecs % 3600) / 60);
          obj["timeTo"] = effTimeTo;  // override the stored "00:00"
        } else {
          toSecs = toH * 3600 + toM * 60;
        }

        // Helper: is a given TimeLib weekday number (1=Sun…7=Sat) active?
        auto isDayActive = [&](uint8_t wdNum) -> bool {
          if (!t.weekdays) return false;
          switch (wdNum) {
            case 1: return t.weekdays->Sunday;
            case 2: return t.weekdays->Monday;
            case 3: return t.weekdays->Tuesday;
            case 4: return t.weekdays->Wednesday;
            case 5: return t.weekdays->Thursday;
            case 6: return t.weekdays->Friday;
            case 7: return t.weekdays->Saturday;
            default: return false;
          }
        };

        if (timerActive) {
          long rem;
          if (t.TM_type == TM_FULL_SPAN) {
            int ty, tm2, td, th, tmin;
            sscanf(t.spanDateto.c_str(), "%d-%d-%d", &ty, &tm2, &td);
            sscanf(t.spantimeto.c_str(), "%d:%d", &th, &tmin);
            tmElements_t te; te.Year = CalendarYrToTm(ty); te.Month = (uint8_t)tm2;
            te.Day = (uint8_t)td; te.Hour = (uint8_t)th; te.Minute = (uint8_t)tmin; te.Second = 0;
            time_t endT = makeTime(te);
            rem = (long)(endT - now());
            if (rem < 0) rem = 0;
          } else {
            rem = toSecs - nowSecs;
            if (rem < 0) rem += 86400;
          }
          obj["secondsToOff"] = rem;
          obj["secondsToOn"]  = -1;
        } else {
          long until = -1;

          switch (t.TM_type) {

            case TM_WEEKDAY_SPAN: {
              // Find the next active weekday (today counts if time hasn't passed yet)
              uint8_t todayWd = weekday(); // 1=Sun … 7=Sat
              for (int offset = 0; offset <= 7; offset++) {
                uint8_t checkWd = ((todayWd - 1 + offset) % 7) + 1;
                if (isDayActive(checkWd)) {
                  long candidate = (long)offset * 86400 + fromSecs - nowSecs;
                  if (candidate > 0) { until = candidate; break; }
                  // offset=0 but time already passed — skip to next active day
                }
              }
              break;
            }

            case TM_MONTHLY_SPAN: {
              int todayDay   = day();
              int targetDay  = t.monthDay > 0 ? t.monthDay : 1;
              if (todayDay == targetDay && fromSecs > nowSecs) {
                until = fromSecs - nowSecs;
              } else if (todayDay < targetDay) {
                until = (long)(targetDay - todayDay) * 86400 - nowSecs + fromSecs;
              } else {
                // Target has passed this month — compute days in current month
                const uint8_t dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
                int m = month(); int y = year();
                int daysThisMonth = dim[m];
                if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) daysThisMonth = 29;
                int daysUntil = (daysThisMonth - todayDay) + targetDay;
                until = (long)daysUntil * 86400 - nowSecs + fromSecs;
              }
              break;
            }

            default: {
              // Daily / full-span / yearly: next occurrence of the time window
              int candidate = fromSecs - nowSecs;
              until = (candidate > 0) ? candidate : candidate + 86400;
              break;
            }
          }

          obj["secondsToOff"] = -1;
          obj["secondsToOn"]  = until;
        }
      }

      if (cb.length()) {
        // JSONP path still needs a String (sendJSONP wraps the payload)
        String payload;
        serializeJson(doc, payload);
        sendJSONP(request, cb, payload);
        return;
      }
      AsyncResponseStream *resp = request->beginResponseStream("application/json", measureJson(doc) + 1);
      serializeJson(doc, *resp);
      request->send(resp);
    });

    AsyncWeb_server.on("/TimerControl.json", HTTP_GET, [](AsyncWebServerRequest *request){
      String cb = _jsonpCB(request);
      if (cb.length()) {
        if (!_jsonpAuth(request)) { request->send(401, "application/javascript", "/* Unauthorized */"); return; }
      } else {
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      }
      int nb = 0;
      String action = "";
      if (request->hasParam("relay"))  nb     = request->getParam("relay")->value().toInt();
      if (request->hasParam("action")) action = request->getParam("action")->value();
      Relay *r = getrelaybynumber(nb);
      if (r) {
        if (action == "pause")  r->timerpaused = true;
        if (action == "resume") r->timerpaused = false;
      }
      bool paused = r && r->timerpaused;
      String json = "{\"ok\":true,\"relay\":" + String(nb) + ",\"paused\":" + (paused ? "true" : "false") + "}";
      if (cb.length()) { sendJSONP(request, cb, json); return; }
      sendWithCORS(request, 200, "application/json", json);
    });

    AsyncWeb_server.on("/RelayConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      AppliedRelayNumber = 0;
      if (request->hasParam("GETRELAYNB")) {
        String t = request->getParam("GETRELAYNB")->value();
        AppliedRelayNumber = t.toInt();
        if (request->hasParam("RELAYACTION")) {
          String msg = request->getParam("RELAYACTION")->value();
          Relay * rtmp = getrelaybynumber(AppliedRelayNumber);
          if (rtmp) {
            if (msg == "ON")  rtmp->mdigitalWrite(rtmp->getRelayPin(), HIGH);
            if (msg == "OFF") rtmp->mdigitalWrite(rtmp->getRelayPin(), LOW);
          }
        }
      }
      request->send(SPIFFS, "/RelayConfig.html", String(), false, processor);
    });

    AsyncWeb_server.on("/Apply.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/Apply.html");
        // int args = request->args();
          #ifdef USEPREF
          SaveParams(MyConfParam,preferences,request);
          #endif
          saveConfig(MyConfParam, request);
          loadConfig(MyConfParam);
          #if defined (_emonlib_) || defined (_pressureSensor_)
          applyPowerTaskIntervals();
          #endif
          #ifdef OLED_1306
          applyOledTaskInterval();
          #endif
          applyMqttActiveState();
          setupInputs();
          clearIRMap();
          loadIRMapConfig(myIRMap);
          Serial.printf("[HTTP   ] Config saved. mDNS: http://%s.local  WiFi SSID: %s\n",
                        MyConfParam.v_PhyLoc.c_str(), MyConfParam.v_ssid.c_str());
    });

    AsyncWeb_server.on("/ResetHomeKit", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/ResetHomeKit.html");
        // int args = request->args();
      #ifdef AppleHK
        #ifndef ESP32
            homekit_storage_reset();   
            Serial.println(F("HomeKit Web Reset, rebooting.."));
            restartRequired = true; // main function will take care of restarting
        #endif    
      #endif

      #ifdef ESP32
        #ifdef AppleHK
              resetHap(); 
        #endif      
      #endif
    });

    AsyncWeb_server.on("/RelayCmdApplied.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/RelayCmdApplied.html");
    });    

    AsyncWeb_server.on("/ApplyRelay.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();

        if(request->hasParam("RELAYNB") || request->hasParam("RelayNB")) {
          Serial.println(F("[HTTP   ] /ApplyRelay.html relay save request"));
          String tmp = request->hasParam("RELAYNB") ? request->getParam("RELAYNB")->value() : request->getParam("RelayNB")->value();
          for (int i = 0; i < request->args(); i++) {
            if (request->argName(i) == "RELAYNB" || request->argName(i) == "RelayNB") {
              tmp = request->arg(i);
            }
          }
          uint8_t relayNumber = tmp.toInt();
          if (!saveRelayConfig(request)) {
            request->send(500, "text/plain", "Failed to save relay configuration");
            return;
          }
          AppliedRelayNumber = relayNumber;
          Relay *rtmp = getrelaybynumber(relayNumber);
          // saveRelayConfig() already updated the in-memory relay struct —
          // loadrelayparams() is NOT called here (it adds 2 more flash ops
          // that contributed to TWDT crashes on a 74%-full filesystem).
          if (rtmp && rtmp->RelayConfParam) {
            char ttlBuf[12];
            snprintf(ttlBuf, sizeof(ttlBuf), "%lu", (unsigned long)rtmp->RelayConfParam->v_ttl);
            if (mqttIsEnabled() && mqttClient.connected()) {
              mqttPublish(rtmp->RelayConfParam->v_i_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, ttlBuf);
              mqttPublish(rtmp->RelayConfParam->v_ttl_PUB_TOPIC.c_str(),   QOS2, RETAINED, ttlBuf);
            }
          }
          request->redirect("/RelayConfig.html?GETRELAYNB=" + tmp);
          return;
        }

      request->send(SPIFFS, "/ApplyRelay.html");

      }
    );


    AsyncWeb_server.on("/Input_Relays_Map", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/Input_Relays_Map.html", String(), false, IRMAPprocessor);
      });

    AsyncWeb_server.on("/InputModesHelp.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/InputModesHelp.html");
      });

    AsyncWeb_server.on("/ApplyIRMap.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/ApplyIRMap.html");
        // int args = request->args();
          saveIRMapConfig(request);
          loadIRMapConfig(myIRMap);
    });


    AsyncWeb_server.on("/SerialLogConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/SerialLogConfig.html", "text/html");
    });

    AsyncWeb_server.on("/api/seriallog", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(200, "application/json", serialLogToJson());
    });

    AsyncWeb_server.on("/api/seriallog", HTTP_POST,
      [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        if (!request->_tempObject) {
          request->send(400, "application/json", F("{\"ok\":false}"));
          return;
        }
        String* body = reinterpret_cast<String*>(request->_tempObject);
        serialLogFromJson(*body);
        delete body;
        request->_tempObject = nullptr;
        request->send(200, "application/json", F("{\"ok\":true}"));
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (!index) { request->_tempObject = new String(); }
        reinterpret_cast<String*>(request->_tempObject)->concat((char*)data, len);
      }
    );

    AsyncWeb_server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
      // the request handler is triggered after the upload has finished...
      // create the response, add header, and send response
      const bool updateOk = firmwareOtaAccepted && !firmwareOtaWriteFailed && !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", updateOk ? "OK... Update Successful" : "FAIL");
      response->addHeader("Connection", "close");
      response->addHeader("Access-Control-Allow-Origin", "*");
      restartRequired = true;  // Reboot after both successful and failed firmware updates.
      request->send(response);
        },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        //Upload handler chunks in data
          if(!index){ // if index == 0 then this is the first frame of data
            Serial.print(F("\n[SYSTEM  ] firmware upload started (will reboot once done): "));
            Serial.print (filename.c_str());
            Serial.print(F("\n"));
            firmwareOtaActive = true;
            firmwareOtaAccepted = false;
            firmwareOtaWriteFailed = false;
            firmwareOtaStartedMs = millis();
            firmwareUpdateBegin();
            schedulerStopForFirmwareUpdate();
            mqttStopForFirmwareUpdate();
            ntpStopForFirmwareUpdate();
            // Serial.setDebugOutput(true);

            #ifdef ESP32
                const size_t otaSize = ESP.getFreeSketchSpace();
                Serial.printf("[SYSTEM  ] OTA free sketch space: %u bytes\n",
                              static_cast<unsigned>(otaSize));
                if (!Update.begin(otaSize)) {
                  firmwareOtaWriteFailed = true;
                  firmwareOtaActive = false;    // H5: clear so watchdog doesn't count down
                  firmwareOtaStartedMs = 0;
                  Update.printError(Serial);
                } else {
                  firmwareOtaAccepted = true;
                }
            #else
              //   calculate sketch space required for the update
                uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                if(!Update.begin(maxSketchSpace)){//start with max available size
                  firmwareOtaWriteFailed = true;
                  firmwareOtaActive = false;    // H5: clear so watchdog doesn't count down
                  firmwareOtaStartedMs = 0;
                  Update.printError(Serial);
                } else {
                  firmwareOtaAccepted = true;
                }
                Update.runAsync(true); // tell the updaterClass to run in async mode
            #endif
          }

          //Write chunked data to the free sketch space
          if(!firmwareOtaAccepted || firmwareOtaWriteFailed) {
            return;
          }

          if(Update.write(data, len) != len){
              firmwareOtaWriteFailed = true;
              Update.printError(Serial);
          }

          if(final){ // if the final flag is set then this is the last frame of data
            firmwareOtaActive = false;
            firmwareOtaStartedMs = 0;
            if(!firmwareOtaWriteFailed && Update.end(true)){ //true to set the size to the current progress
                Serial.print(F("Update Success... \nRebooting..."));
                Serial.print(index+len);
              } else {
                firmwareOtaWriteFailed = true;
                Update.printError(Serial);
              }
              // Serial.setDebugOutput(false);
          }
    });



      /*AsyncWeb_server.on("/firmware.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          request->send(SPIFFS, "/firmware.html", String(), false, processor);
      		MyConfParam.v_Update_now =  MyConfParam.v_Update_now;
          execOTA(MyConfParam.v_FRM_IP,MyConfParam.v_FRM_PRT.toInt());
          });
          */

    AsyncWeb_server.on("/updatefs", HTTP_POST, [](AsyncWebServerRequest *request){
        const bool ok = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", ok ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        restartRequired = true;
        request->send(response);
      }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (!index) {
          Serial.printf("[SYSTEM  ] filesystem upload started: %s\n", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
          }
        }
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
        if (final) {
          if (Update.end(true)) {
            Serial.printf("[SYSTEM  ] filesystem update success: %u bytes\n", (unsigned)(index + len));
          } else {
            Update.printError(Serial);
          }
        }
    });

    AsyncWeb_server.on("/api/filesystem/download", HTTP_GET, [](AsyncWebServerRequest *request){
        const esp_partition_t* part = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        if (!part) { request->send(503, "text/plain", "No filesystem partition"); return; }
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "application/octet-stream",
            [part](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
                if (index >= part->size) return 0;
                size_t toRead = min(maxLen, part->size - index);
                if (esp_partition_read(part, index, buf, toRead) != ESP_OK) return 0;
                return toRead;
            }
        );
        response->addHeader("Content-Disposition", "attachment; filename=\"littlefs.bin\"");
        response->addHeader("Content-Length", String(part->size));
        request->send(response);
    });

    AsyncWeb_server.on("/api/firmware/download", HTTP_GET, [](AsyncWebServerRequest *request){
        const esp_partition_t* part = esp_ota_get_running_partition();
        if (!part) { request->send(503, "text/plain", "No running partition"); return; }
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "application/octet-stream",
            [part](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
                if (index >= part->size) return 0;
                size_t toRead = min(maxLen, part->size - index);
                if (esp_partition_read(part, index, buf, toRead) != ESP_OK) return 0;
                return toRead;
            }
        );
        response->addHeader("Content-Disposition", "attachment; filename=\"firmware.bin\"");
        response->addHeader("Content-Length", String(part->size));
        request->send(response);
    });

	  AsyncWeb_server.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
	      request->send(SPIFFS, "/Reset.html");
				Serial.println(F("rebooting.."));
        restartRequired = true; // main function will take care of restarting
	      });

    #ifdef _ESP_ALEXA_
    AsyncWeb_server.onNotFound([](AsyncWebServerRequest *request){
      if (request->method() == HTTP_OPTIONS) { request->send(200, "text/plain", ""); return; }
      if (!espalexa.handleAlexaApiCall(request))
        request->send(404, "text/plain", "Not found");
    });
    #endif
    #ifdef _ALEXA_
      // These two callbacks are required for gen1 and gen3 compatibility
      AsyncWeb_server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
          if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
          // Handle any other body request here...
      });
      AsyncWeb_server.onNotFound([](AsyncWebServerRequest *request) {
          if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *r = request->beginResponse(204);
            addCORSHeaders(request, r, request->hasHeader("Origin"));
            request->send(r);
            return;
          }
          String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
          if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
      });
    #endif

    #if !defined(_ESP_ALEXA_) && !defined(_ALEXA_)
    AsyncWeb_server.onNotFound([](AsyncWebServerRequest *request){
      if (request->method() == HTTP_OPTIONS) { request->send(200, "text/plain", ""); return; }
      request->send(404, "text/plain", "Not found");
    });
    #endif

        /*
        AsyncWeb_server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
          // the request handler is triggered after the upload has finished...
          // create the response, add header, and send response
          AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (true)?"OK... upload Successful":"OK... upload Successful");
          response->addHeader("Connection", "close");
          response->addHeader("Access-Control-Allow-Origin", "*");
          restartRequired = false;  // Tell the main loop to restart the ESP
          request->send(response);
            },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){

                                if (!index) {

                                  Serial.print (index);
                                //  cf = SPIFFS.open("/Config1.html", "w");
                                String temp = "/" + filename;
                                Serial.printf("\n saving to file --->%s\n ",temp.c_str());
                                  cf = SPIFFS.open(temp, "w");
                                  if (!cf) {
                                    Serial.println(F("can not create file "));
                                    //return ERROR_OPENING_FILE;
                                  } else {
                                    Serial.println(F("file created"));
                                  }
                                  Serial.printf("\n UploadStart: %s\n", filename.c_str());
                                  Serial.printf("\n first frame: %s\n", filename.c_str());
                                  Serial.setDebugOutput(true);
                                }

                                  cf.write(data,len);
                                  printf("%s", (const char*)data);
                                  Serial.print(index);

                                if(final) {
                                  Serial.printf("\n UploadEnd: %s (%u)\n", filename.c_str(), index+len);
                                  cf.close();
                                }
                              });
        */
	Serial.println(F("[INFO   ] Starting HTTP server"));
  AsyncWeb_server.begin();
}
