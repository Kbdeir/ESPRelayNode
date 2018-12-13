#ifndef __EXECOTA_
#define __EXECOTA_

#ifdef ESP32
  #include <WiFi.h>
  #include <Update.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP.h>
#endif




// Utility to extract header value from headers
String getHeaderValue(String header, String headerName);
// OTA Logic
void execOTA(String host, int port);

#endif
