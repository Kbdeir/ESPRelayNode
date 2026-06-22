#include "SerialLog.h"
#include <ArduinoJson.h>
#include <defines.h>
#include "FS.h"
#ifdef USE_LittleFS
  #ifdef ESP32
    #define SPIFFS LITTLEFS
  #else
    #define SPIFFS LittleFS
  #endif
  #include <LittleFS.h>
#else
  #include <SPIFFS.h>
#endif

volatile uint32_t g_serialLogMask = SLOG_ALL;

static const char* kPath = "/seriallog.json";

static const struct { const char* key; uint32_t bit; } kCategories[] = {
  { "INFO",   SLOG_INFO   },
  { "WG",     SLOG_WG     },
  { "WFS",    SLOG_WFS    },
  { "MQTT",   SLOG_MQTT   },
  { "NTP",    SLOG_NTP    },
  { "WIFI",   SLOG_WIFI   },
  { "CT",     SLOG_CT     },
  { "TIMERS", SLOG_TIMERS },
  { "PING",   SLOG_PING   },
  { "SYSTEM", SLOG_SYSTEM },
  { "HTTP",   SLOG_HTTP   },
  { "ALEXA",  SLOG_ALEXA  },
  { "OLED",   SLOG_OLED   },
  { "HADISC", SLOG_HADISC },
  { "CT136",  SLOG_CT136  },
};
static const int kNumCats = sizeof(kCategories) / sizeof(kCategories[0]);

void serialLogLoad() {
  File f = SPIFFS.open(kPath, "r");
  if (!f) return;
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
  f.close();
  uint32_t mask = 0;
  for (int i = 0; i < kNumCats; i++) {
    if (doc[kCategories[i].key] | 1) mask |= kCategories[i].bit;
  }
  g_serialLogMask = mask;
}

void serialLogSave() {
  StaticJsonDocument<512> doc;
  for (int i = 0; i < kNumCats; i++) {
    doc[kCategories[i].key] = (g_serialLogMask & kCategories[i].bit) ? 1 : 0;
  }
  File f = SPIFFS.open(kPath, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

String serialLogToJson() {
  StaticJsonDocument<512> doc;
  for (int i = 0; i < kNumCats; i++) {
    doc[kCategories[i].key] = (g_serialLogMask & kCategories[i].bit) ? 1 : 0;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

extern "C" int serialLogWgEnabled(void) {
  return (g_serialLogMask & SLOG_WG) ? 1 : 0;
}

void serialLogFromJson(const String& json) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return;
  uint32_t mask = 0;
  for (int i = 0; i < kNumCats; i++) {
    if (doc[kCategories[i].key] | 1) mask |= kCategories[i].bit;
  }
  g_serialLogMask = mask;
  serialLogSave();
}
