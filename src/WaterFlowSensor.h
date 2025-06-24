#ifndef _WaterFlowSensor_
#define _WaterFlowSensor_

#include "Arduino.h"
#include <ESPAsyncWebServer.h>

#ifdef ESP32
    extern float calibrationFactor;
    extern String WaterFlowSensor_Topic ;
    void loadconfigWFS(char* filename);
    void saveconfigWFS(AsyncWebServerRequest *request);
#endif

#endif