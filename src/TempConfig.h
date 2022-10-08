#pragma once

//#ifndef _TIMERCONG_H__
//#define _TIMERCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include <OneButton.h>
#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);


 //#define buffer_size 1500 // json buffer size

 class TempConfig
{
  private:

  public:
    uint8_t id;
    uint16_t spanTempfrom;
    uint16_t spanTempto;
    uint16_t spanBuffer;
    boolean enabled;
    uint8_t relay;

    //char* Testchar;

    TempConfig(uint8_t para_id
      );

    TempConfig(uint8_t para_id,
              uint16_t para_spanTempfrom,
              uint16_t para_spanTempto,
              uint16_t para_spanBuffer,
              boolean para_enabled,
              uint8_t para_relay
            );


    ~TempConfig();
    void watch();
};

bool saveTempConfig(AsyncWebServerRequest *request);
config_read_error_t loadTempConfig(char* filename, TempConfig &para_TempConfig);
//TempConfig * gettimerbypin(uint8_t pn);
//#endif
