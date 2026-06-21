#include <InputClass.h>
#include <InputsArray.h>

extern std::vector<void *> relays ; // a list to hold all relays

InputSensor::InputSensor(uint8_t p, fnptr_d on_callback, input_mode clickmode) {
  pin                              = p;
  fclickmode                       = clickmode;
  fon_callback                     = on_callback;
  Input_debouncer                  = nullptr; // allocated in initialize()
  onInputChange_RelayServiceRoutine = nullptr;
  onInputClick_RelayServiceRoutine  = nullptr;
  onInputLOOP_RelayServiceRoutine   = nullptr;
  rchangedflag                     = false;
  mqtt_topic                       = "/none";
  post_mqtt                        = false;
}


void InputSensor::SetInputSensorPin(uint8_t p, int PULLMODE) {
  if (Input_debouncer) {
    pin = p;
    pinMode ( pin, PULLMODE);
    Input_debouncer->attach(pin,PULLMODE);
    Input_debouncer->interval(25); // interval in ms
    rchangedflag = false;
  }
}

InputSensor::~InputSensor() {
      delete Input_debouncer;
    }

void InputSensor::addrelay(Relay * rly) {
    attachedrelays.push_back(rly);
};

void InputSensor::initialize(uint8_t p, fnptr_d on_callback, input_mode clickmode, int PULLMODE ) {
    pin = p;
    fclickmode =  clickmode;
    //pinMode ( pin, INPUT_PULLUP);
    pinMode ( pin, PULLMODE);

    onInputChange_RelayServiceRoutine = nullptr;
    onInputClick_RelayServiceRoutine = nullptr;

    onInputLOOP_RelayServiceRoutine = nullptr;

    delete Input_debouncer;
    Input_debouncer = new Bounce();
    //Input_debouncer->attach(pin,INPUT_PULLUP);
    Input_debouncer->attach(pin,PULLMODE);
    Input_debouncer->interval(10); // interval in ms

    rchangedflag = false;
    mqtt_topic="/none";
    post_mqtt = false;
    fon_callback = on_callback;
};

void InputSensor::watch() {
    if (!this->Input_debouncer) return;
    this->Input_debouncer->update();
    if (attachedrelays.size() == 0) {
       if (fclickmode==INPUT_NORMAL) {
          if (Input_debouncer->fell() || Input_debouncer->rose()) {
            if (fon_callback) fon_callback(this, nullptr);
          }
        }
        if (fclickmode==INPUT_TOGGLE) {
          if (Input_debouncer->fell()) {
           if (fon_callback) fon_callback(this,nullptr);
          }
       }
     }
     else
     {

       Relay * rtemp = nullptr;
       for (void* it : attachedrelays) {
       //for (std::vector<void *>::iterator it = attachedrelays.begin(); it != attachedrelays.end(); ++it)  {
         rtemp = static_cast<Relay *>(it);
         if (rtemp != nullptr) {
           if (fclickmode==INPUT_COPY_TO_RELAY) {
              if (this->Input_debouncer->fell() || this->Input_debouncer->rose()) {
               if (onInputChange_RelayServiceRoutine != nullptr) {onInputChange_RelayServiceRoutine(rtemp, this);}
              }
              /*
              this will tie the relay to the physical input, mqtt is ignored if physical input is different from mqtt message
              if (digitalRead(rtemp->getRelayPin()) != digitalRead(this->pin)) {
                digitalWrite(rtemp->getRelayPin(), digitalRead(this->pin));
              }
              */
            }

            if (fclickmode==INPUT_RELAY_TOGGLE) {
             if (Input_debouncer->fell()) {
              if (onInputClick_RelayServiceRoutine != nullptr) {onInputClick_RelayServiceRoutine(rtemp,this);}
             }
           }


         }
       }
     }
}


void InputSensor::clearAttachedRelays() {
  attachedrelays.clear();
}


InputSensor * getinputbynumber(uint8_t pin) {
  for (void* it : inputs) {
    InputSensor* inp = static_cast<InputSensor *>(it);
    if (inp && inp->pin == pin) return inp;
  }
  return nullptr;
}


void applyIRMap(int8_t Inpn, int8_t rlyn) {
  if ((Inpn > -1) && (rlyn > -1)) {
    if (inputs.empty() || relays.empty()) return;
    InputSensor * t = nullptr;
    for (void* it : inputs) {
      InputSensor* inp = static_cast<InputSensor *>(it);
      if (inp && inp->pin == (uint8_t)Inpn) { t = inp; break; }
    }
    if (t == nullptr) return;
    if ((size_t)rlyn < relays.size()) {
      Relay * r = static_cast<Relay *>(relays.at(rlyn));
      if (r != nullptr) {
        if ((t->fclickmode==INPUT_COPY_TO_RELAY) || (t->fclickmode==INPUT_RELAY_TOGGLE)) {
          t->addrelay(r);
        }
      }
    }
  }
}


void clearIRMap() {
      InputSensor * itemp = nullptr;
      for (auto it = inputs.begin(); it != inputs.end(); ++it)  {
        itemp = static_cast<InputSensor *>(*it);
        itemp->clearAttachedRelays();
      }
}
