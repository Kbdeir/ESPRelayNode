#pragma once

#include <TimeLib.h>
#include <ConfigParams.h>

extern boolean ftimesynced;
extern boolean CalendarNotInitiated;

String digitalClockDisplay();
String printDigits(int digits);
void ntpBegin();
void ntpStopForFirmwareUpdate();
void ntpLoop();
time_t timeprovider(void);
