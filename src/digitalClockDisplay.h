#pragma once
#include <String.h>
#include <ConfigParams.h>

// Primary (zero-heap) API — write into a caller-supplied buffer.
// digitalClockDisplay needs at least 24 bytes ("HH:MM:SS DD-MM-YYYY").
// uptimeDisplay needs at least 20 bytes ("NNN days HH:MM").
void digitalClockDisplay(char* buf, size_t len);
void uptimeDisplay(char* buf, size_t len);

// Wrappers that return String — kept for AsyncWebServer template processors
// (which must return String).  All other callers should use the buffer forms.
String digitalClockDisplay();
String uptimeDisplay();
