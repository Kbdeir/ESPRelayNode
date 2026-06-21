#include <arduino.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <Settings.h>
#include <defines.h>



#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LittleFS.h>
#else
  #include <SPIFFS.h>
#endif 



#ifndef ESP32
#include <EEPROM.h>
#endif

extern TConfigParams MyConfParam;

extern const char* filename; // change this one


// #define buffer_size  2000

    Settings::Settings()
    {
    //  load(); 
    //  loadConfig(MyConfParam);
    }


