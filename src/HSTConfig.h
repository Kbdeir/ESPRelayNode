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

 class HSTConfig
{
  private:

  public:
    uint8_t id;
    double AmpsVoltsRatio;
    boolean enabled;

    //char* Testchar;

    HSTConfig(uint8_t para_id
      );

    HSTConfig(uint8_t para_id,
              double para_AmpsVoltsRatio,
              boolean para_enabled
            );


    ~HSTConfig();
    void watch();
};

bool saveHSTConfig(AsyncWebServerRequest *request);
config_read_error_t loadHSTConfig(char* filename, HSTConfig &para_HSTConfig);
//TempConfig * gettimerbypin(uint8_t pn);
//#endif
