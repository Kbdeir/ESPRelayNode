#include <InputClass.h>

InputSensor::InputSensor(uint8_t p,
            fnptr_a on_callback
          ) {

  pin = p;
  pinMode ( pin, INPUT_PULLUP);
  attached_to_relay = 0;

  Input_debouncer = new Bounce();
  Input_debouncer->attach(pin,INPUT_PULLUP);
  Input_debouncer->interval(25); // interval in ms

  rchangedflag = false;
  mqtt_topic="/none";
  post_mqtt = false;
  fon_callback = on_callback;

}

InputSensor::~InputSensor(){
      delete Input_debouncer;
    }


void InputSensor::watch(){
  if (attached_to_relay == 0) {
     Input_debouncer->update();
     if (Input_debouncer->fell() || Input_debouncer->rose()) {
        if (fon_callback) fon_callback(this);
   }
  }
}