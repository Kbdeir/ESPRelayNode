#ifndef _TEMPSENSOR_H__
#define _TEMPSENSOR_H__

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 14


class TempSensor {
public:
  float Celcius;
  float Fahrenheit;
  TempSensor();
  TempSensor(uint8_t _pin);
  ~TempSensor();
  void tempbegin();
  void requestTemp();
  float getCurrentTemp(uint8_t index);

private:
  OneWire * oneWire;
  DallasTemperature * sensors;
  uint8_t pin;

};

#endif
