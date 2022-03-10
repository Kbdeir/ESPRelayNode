#pragma once

#include <TimeLib.h>
#include <ConfigParams.h>


#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncUDP.h>
#else
  #include <ESP8266WiFi.h>
  #include "ESPAsyncUDP.h"
#endif

typedef void (*fnptr)();
#define NTP_REQUEST_PORT      123

const int NTP_PACKET_SIZE_ = 48; // NTP time is in the first 48 bytes of message

extern boolean ftimesynced;
extern boolean CalendarNotInitiated;
extern AsyncUDP Audp;
extern unsigned int localPort;

String digitalClockDisplay();
String printDigits(int digits);
void sendAsyncNTPPacket(void);

void parsePacket(AsyncUDPPacket packet);

time_t timeprovider(void);

