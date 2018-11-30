
#ifndef KSBNTP_H
#define KSBNTP_H

#include <TimeLib.h>

#include <WiFiUdp.h>
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif



const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message

extern WiFiUDP Udp;
extern unsigned int localPort;

String digitalClockDisplay();
String printDigits(int digits);
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);


#endif
