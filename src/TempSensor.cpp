#include <TempSensor.h>

TempSensor::TempSensor(uint8_t _pin) {
	pin = _pin;
  oneWire = new OneWire (ONE_WIRE_BUS);
  sensors = new DallasTemperature(oneWire);
  Celcius=0;
  Fahrenheit=0;
}




float TempSensor::getCurrentTemp(){
  sensors->requestTemperatures();
  Celcius=sensors->getTempCByIndex(0);
  Fahrenheit=sensors->toFahrenheit(Celcius);
  Serial.print("\n C  ");
  Serial.print(Celcius);
  Serial.print(" F  ");
  Serial.println(Fahrenheit);

	return Celcius;
}

TempSensor::~TempSensor(){
      delete oneWire ;
      delete sensors;
}
