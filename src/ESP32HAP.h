
#ifndef ESP32HAP_
#define ESP32HAP_
    #include <Arduino.h>
    #include <Wifi.h>
    #include <ESP32HomeKit.h>

    #ifdef ESP32
    static int identify(hap_acc_t *ha);
    static int fan_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv);
    static int fan_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv);
    void setupHAP();
    void resetHap();
    #endif

#endif
