#pragma once

//#ifndef _TIMERCONG_H__
//#define _TIMERCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>


typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);


 //#define buffer_size 1500 // json buffer size

 class ADS11x5Config
{
  private:

  public:
    uint8_t id;
    double Res1;
    double Res2;
    double ResMultiplier;
    boolean enabled;
    uint8_t relay1;
    uint8_t relay2;

    //char* Testchar;

    ADS11x5Config(uint8_t para_id
      );

    ADS11x5Config(uint8_t para_id,
              double para_Res1,
              double para_Res2,
              double para_ResMultiplier,
              boolean para_enabled,
              uint8_t para_relay1,
              uint8_t para_relay2
            );


    ~ADS11x5Config();
    void watch();
};

bool saveADS11x5Config(AsyncWebServerRequest *request);
config_read_error_t loadADS11x5Config(char* filename, ADS11x5Config &para_ADS11x5Config);
//TempConfig * gettimerbypin(uint8_t pn);
//#endif
