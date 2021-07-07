#include <TempSensor.h>

TempSensor::TempSensor() {
}

TempSensor::TempSensor(uint8_t _pin) {
  pin = _pin;
  Celcius=0;
  Fahrenheit=0;
  oneWire = new OneWire (pin);
  this->sensors = new DallasTemperature(oneWire );
  pinMode(pin, INPUT_PULLUP);
}


void TempSensor::tempbegin(uint8_t _pin) {

}



float TempSensor::getCurrentTemp(uint8_t index){
  if (this->sensors) {
    this->sensors->requestTemperatures();
    Celcius=this->sensors->getTempCByIndex(0);
  //  MCelcius = Celcius;
  }
	return Celcius;
}

TempSensor::~TempSensor(){
      delete this->oneWire ;
      delete this->sensors;
}
