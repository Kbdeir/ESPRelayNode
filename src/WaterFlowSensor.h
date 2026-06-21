
#ifdef WaterFlowSensor

#ifndef _WaterFlowSensor_
#define _WaterFlowSensor_

#include "Arduino.h"
#include <ESPAsyncWebServer.h>

#ifdef ESP32
    extern float calibrationFactor;
    extern String WaterFlowSensor_Topic;
    extern volatile uint32_t pulseCountCumulative;
    extern portMUX_TYPE pulseCountMux;
    extern float flowRate;
    extern uint64_t totalMilliLitres;
    void loadconfigWFS(const char* filename);
    void saveconfigWFS(AsyncWebServerRequest *request);
#endif

#endif
#endif
