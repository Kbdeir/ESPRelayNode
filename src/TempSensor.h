#ifndef _TEMPSENSOR_H__
#define _TEMPSENSOR_H__

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 14


class TempSensor {
public:
  float Celcius;
  float Fahrenheit;
  TempSensor(uint8_t _pin);
  ~TempSensor();
  float getCurrentTemp();

  OneWire * oneWire;
  DallasTemperature * sensors;

private:
  uint8_t pin;

};

#endif
