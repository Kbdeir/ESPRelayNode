
#ifndef JSONCONFIG_H
#define JSONCONFIG_H
#define ARDUINOJSON_ENABLE_NAN 1

  #include <ArduinoJson.h>
  #include "FS.h"
  #ifdef ESP32
    #include "SPIFFS.h"
  #endif

  #include <ConfigParams.h>
  // #ifndef ESP32
    #include <ESPAsyncWebServer.h>
  // #endif
  #include <string.h>


#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>


enum config_read_error_t {FAILURE, FILE_NOT_FOUND, ERROR_OPENING_FILE, JSONCONFIG_CORRUPTED, SPIFFS_ERROR, SUCCESS};


  // config_read_error_t loadConfig(TConfigParams &ConfParam) ;
  config_read_error_t loadConfig(TConfigParams &ConfParam);
  bool saveConfig(TConfigParams &ConfParam, AsyncWebServerRequest *request) ;
  bool saveConfig(TConfigParams &ConfParam) ;
  bool saveDefaultConfig();

  bool saveIRMapConfig(AsyncWebServerRequest *request) ;
  config_read_error_t loadIRMapConfig(TIRMap &IRMap);

  bool saveRelayDefaultConfig(uint8_t rnb);
  bool saveRelayConfig( AsyncWebServerRequest *request);
  bool saveRelayConfig(Trelayconf * RConfParam);
#endif
