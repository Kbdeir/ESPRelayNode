
#ifndef JSONCONFIG_H
#define JSONCONFIG_H

  #include <ArduinoJson.h>
  #include "FS.h"
  #ifdef ESP32
    #include "SPIFFS.h"
  #endif

  #include <ConfigParams.h>
  #include <ESPAsyncWebServer.h>
  #include <string.h>



enum config_read_error_t {FAILURE, FILE_NOT_FOUND, ERROR_OPENING_FILE, JSONCONFIG_CORRUPTED, SPIFFS_ERROR, SUCCESS};


  // config_read_error_t loadConfig(TConfigParams &ConfParam) ;
  config_read_error_t loadConfig(TConfigParams &ConfParam);
  bool saveConfig(TConfigParams &ConfParam, AsyncWebServerRequest *request) ;
  bool saveConfig(TConfigParams &ConfParam) ;
  bool saveDefaultConfig();

  bool saveIRMapConfig(AsyncWebServerRequest *request) ;
  config_read_error_t loadIRMapConfig(TIRMap &IRMap);
#endif
