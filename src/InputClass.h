#ifndef _INPUTCONG_H__
#define _INPUTCONG_H__

#include "Arduino.h"
#include <Bounce2.h>
#include <RelayClass.h>


typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);
typedef void (*fnptr_d)(void* t, void* t1);

enum input_mode {INPUT_NONE, INPUT_TOGGLE, INPUT_NORMAL, INPUT_RELAY_TOGGLE, INPUT_COPY_TO_RELAY} ;

class InputSensor{
  private:
  fnptr_d fon_callback;
  std::vector<void *> attachedrelays ; // a list to hold all relays

  public:
    uint8_t pin;
    Bounce *Input_debouncer;
    boolean rchangedflag;
    String mqtt_topic;
    boolean post_mqtt;
    input_mode fclickmode;
    fnptr_d onInputChange_RelayServiceRoutine;
    fnptr_d onInputClick_RelayServiceRoutine;

    InputSensor(
      uint8_t p,
      fnptr_d on_callback,
      input_mode clickmode
    );

    void SetInputSensorPin(uint8_t p);

    ~InputSensor();
    void addrelay(Relay * rly);
    void watch();
    InputSensor * getinputbynumber(uint8_t nb);
};


#endif
