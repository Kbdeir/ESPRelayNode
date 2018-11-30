#ifdef ESP32
#ifdef USEPREF
#include <ESPAsyncWebServer.h>
#include <string.h>
#include "ConfigParams.h"

void ReadParams(TConfigParams &ConfParam, Preferences preferences);
void SaveParams(TConfigParams &ConfParam, Preferences preferences, AsyncWebServerRequest *request);
void SaveWifi_to_Preferences(Preferences preferences, String ssid, String pass);
#endif
#endif
