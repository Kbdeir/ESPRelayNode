#ifndef _INPUTCONG_H__
#define _INPUTCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);
typedef void (*fnptr_d)(void* t, void* t1);

enum input_mode {INPUT_NONE, INPUT_TOGGLE, INPUT_NORMAL, INPUT_RELAY_TOGGLE, INPUT_COPY_TO_RELAY} ;

 class InputSensor
{
  private:
  fnptr_d fon_callback;
  public:
    uint8_t pin;
    Bounce *Input_debouncer;
    boolean rchangedflag;
    uint8_t attached_to_relay;
    String mqtt_topic;
    boolean post_mqtt;
    input_mode fclickmode;

    InputSensor(uint8_t p,
      fnptr_d on_callback,
      input_mode clickmode
        );

    ~InputSensor();

    void watch();
};

//  Input * getInputByPin(uint8_t pn);

#endif
