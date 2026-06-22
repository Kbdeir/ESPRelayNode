#pragma once
#include <Arduino.h>
#include <stdint.h>

// One bit per log category. Bits 0-12 assigned; add new ones at the end only.
#define SLOG_INFO    (1u <<  0)
#define SLOG_WG      (1u <<  1)
#define SLOG_WFS     (1u <<  2)
#define SLOG_MQTT    (1u <<  3)
#define SLOG_NTP     (1u <<  4)
#define SLOG_WIFI    (1u <<  5)
#define SLOG_CT      (1u <<  6)
#define SLOG_TIMERS  (1u <<  7)
#define SLOG_PING    (1u <<  8)
#define SLOG_SYSTEM  (1u <<  9)
#define SLOG_HTTP    (1u << 10)
#define SLOG_ALEXA   (1u << 11)
#define SLOG_OLED    (1u << 12)
#define SLOG_HADISC  (1u << 13)
#define SLOG_CT136   (1u << 14)
#define SLOG_ALL     0x7FFFu

extern volatile uint32_t g_serialLogMask;

// Printf-style conditional log
#define SLOG(cat, ...)    do { if (g_serialLogMask & (cat)) Serial.printf(__VA_ARGS__); } while(0)
// Print a flash-string (no newline)
#define SLOGP(cat, str)   do { if (g_serialLogMask & (cat)) Serial.print(F(str)); } while(0)
// Println a flash-string
#define SLOGLNP(cat, str) do { if (g_serialLogMask & (cat)) Serial.println(F(str)); } while(0)
// Multi-statement block guard: SLOG_IF(SLOG_WG) { Serial.print(...); Serial.println(...); }
#define SLOG_IF(cat)      if (g_serialLogMask & (cat))

void serialLogLoad();
void serialLogSave();
String serialLogToJson();
void serialLogFromJson(const String& json);

// C-callable bridge used by wireguardif.c (pure C, can't use SLOG macros)
#ifdef __cplusplus
extern "C" {
#endif
int serialLogWgEnabled(void);
#ifdef __cplusplus
}
#endif
