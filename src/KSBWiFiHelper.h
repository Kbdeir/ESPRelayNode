
#ifndef WIFIHELPER_
#define WIFIHELPER_

#define WIFI_AP_MODE 1
#define WIFI_CLT_MODE 0

    #ifdef ESP32
        #include <WiFi.h>
        #include <esp_wifi.h>
    #else
        #include <ESP8266WiFi.h>
    #endif

    #ifdef ESP32
        void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
        void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
        extern volatile bool pending_wifi_connected;
        extern volatile bool pending_softap;
    #endif 
    void  blinkled();
    void  blinkledTask();    
    void ScanMyWiFi(void);

#endif
