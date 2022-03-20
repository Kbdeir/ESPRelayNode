
#ifndef WIFIHELPER_
    #define WIFIHELPER_

    #ifdef ESP32
    #include <WiFi.h>
    #include <esp_wifi.h>

    #else
    #include <ESP8266WiFi.h>
    #endif

#ifdef ESP32
    void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
    void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    void  blinkled();
#endif 




#endif