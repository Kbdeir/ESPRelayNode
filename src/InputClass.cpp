#include <InputClass.h>
#include <InputsArray.h>

extern std::vector<void *> relays ; // a list to hold all relays

InputSensor::InputSensor(uint8_t p,
            fnptr_d on_callback,input_mode clickmode
          ) {

  pin = p;
  fclickmode =  clickmode;
  pinMode ( pin, INPUT_PULLUP);
  onInputChange_RelayServiceRoutine = nullptr;
  onInputClick_RelayServiceRoutine = nullptr;

  Input_debouncer = new Bounce();
  Input_debouncer->attach(pin,INPUT_PULLUP);
  Input_debouncer->interval(25); // interval in ms

  rchangedflag = false;
  mqtt_topic="/none";
  post_mqtt = false;
  fon_callback = on_callback;
}


void InputSensor::SetInputSensorPin(uint8_t p) {
  if (Input_debouncer) {
    pin = p;
    pinMode ( pin, INPUT_PULLUP);
    Input_debouncer->attach(pin,INPUT_PULLUP);
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


void InputSensor::watch() {
    Input_debouncer->update();
    //  if (attachedrelay == nullptr) {
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

     } else {
     Relay * rtemp = nullptr;
     for (void* it : attachedrelays) {
     //for (std::vector<void *>::iterator it = attachedrelays.begin(); it != attachedrelays.end(); ++it)  {
       rtemp = static_cast<Relay *>(it);
       if (rtemp) {
         if (fclickmode==INPUT_COPY_TO_RELAY) {
           if (Input_debouncer->fell() || Input_debouncer->rose()) {
             if (onInputChange_RelayServiceRoutine != nullptr) {onInputChange_RelayServiceRoutine(rtemp, this);}
            }
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


InputSensor * getinputbynumber(uint8_t nb) {
  if (nb < inputs.size()) {
    InputSensor * inp = static_cast<InputSensor *>(inputs.at(nb));
    if (inp) {
      return inp;
    } else { return nullptr;}
  }   else { return nullptr;}
}


void applyIRMap(int8_t Inpn, int8_t rlyn) {
  if ((Inpn > -1) && (rlyn > -1)) {
    InputSensor * t = nullptr;
    Relay * r = nullptr;
    if ((Inpn <= inputs.size()-1) && (rlyn <= relays.size()-1)) {
      t = static_cast<InputSensor *>(inputs.at(Inpn));
      r = static_cast<Relay *>(relays.at(rlyn));
      if ((t != nullptr) && (r !=nullptr)) {
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
