#ifndef _INPUTCONG_H__
#define _INPUTCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

 class InputSensor
{
  private:

  fnptr_a fon_callback;


  public:
    uint8_t pin;
    Bounce *Input_debouncer;
    boolean rchangedflag;
    uint8_t attached_to_relay;
    String mqtt_topic;
    boolean post_mqtt;


    InputSensor(uint8_t p,
      fnptr_a on_callback
        );


    ~InputSensor();

    void watch();


};

//  Input * getInputByPin(uint8_t pn);

#endif
