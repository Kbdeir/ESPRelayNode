#pragma once
#ifndef _SETTINGS_
#define _SETTINGS_

#include <arduino.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <ConfigParams.h>

#ifdef ESP32
#include "SPIFFS.h"
#endif

       

class Settings
{
  public:
    bool _valid = false;
    String _wifiSsid = "ksba";
    String _wifiPass = "samsam12";
    String _deviceType = "PIP";
    String _deviceName = "test";
    String _mqttServer = "192.168.50.34";
    // short _mqttPort = "1883";
    String _mqttUser = "";
    String _mqttPassword = "";
    short  _mqttPort = 1883;

    Settings();   
    //void load();
    //void save();
    //bool saveConfig(TConfigParams &ConfParam);
    //bool saveDefaultConfig();
    //void savedefaults();
    //config_read_error_t loadConfig(TConfigParams &ConfParam);
};

#endif
