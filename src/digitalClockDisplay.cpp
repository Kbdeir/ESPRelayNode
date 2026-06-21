#include <Arduino.h>
#include <TimeLib.h>
#include <digitalClockDisplay.h>

void digitalClockDisplay(char* buf, size_t len) {
    snprintf(buf, len, "%02d:%02d:%02d %02d-%02d-%d",
             hour(), minute(), second(), day(), month(), year());
}

void uptimeDisplay(char* buf, size_t len) {
    uint64_t totalMinutes = millis() / 60000ULL;
    uint32_t days         = (uint32_t)(totalMinutes / 1440ULL);
    uint8_t  hrs          = (uint8_t)((totalMinutes % 1440ULL) / 60ULL);
    uint8_t  mins         = (uint8_t)(totalMinutes % 60ULL);
    snprintf(buf, len, "%lu days %02u:%02u", (unsigned long)days, hrs, mins);
}

// String wrappers — used only by AsyncWebServer template processors.
String digitalClockDisplay() {
    char buf[24];
    digitalClockDisplay(buf, sizeof(buf));
    return String(buf);
}

String uptimeDisplay() {
    char buf[20];
    uptimeDisplay(buf, sizeof(buf));
    return String(buf);
}
