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


void TempSensor::tempbegin() {
  sensors->begin();
  // Non-blocking mode: requestTemperatures() returns immediately;
  // getTempCByIndex() reads the result of the previous conversion.
  // Caller must wait >= 750 ms (12-bit) between requestTemp() and getCurrentTemp().
  sensors->setWaitForConversion(false);
}

// Trigger a conversion on all sensors on this bus. Returns immediately.
void TempSensor::requestTemp() {
  if (this->sensors) {
    this->sensors->requestTemperatures();
  }
}

// Read the result of the last conversion. Call at least 750 ms after requestTemp().
float TempSensor::getCurrentTemp(uint8_t index) {
  if (this->sensors) {
    Celcius = this->sensors->getTempCByIndex(index);
  }
  return Celcius;
}

TempSensor::~TempSensor(){
      delete this->oneWire ;
      delete this->sensors;
}
