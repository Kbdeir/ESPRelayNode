#ifndef _CTPROCESSOR_
#define _CTPROCESSOR_

#ifdef ESP32
#include "Arduino.h"
#include "EmonLib.h"      

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncMqttClient.h>
#include <ConfigParams.h>

class CTPROCESSOR
{
  public:
  EnergyMonitor emon1;                  // Create an instance 

  uint8_t CTPin, inPinV;
  double ICAL,VCAL,PHASECAL;

  int   CTSaveThreshold_value = 10;
  float realPower;
  float apparentPower;
  float apparentPower_2;  
  float powerFActor ;
  float supplyVoltage;
  float Irms ;  
  double amps = 0;
  uint32_t time_on = 10;
  uint32_t time_off = 10;   
  unsigned long lwhtime = 0;            // Last watt hour time
  float wh, Instantaneous_Wh = 0.0;     // Stores watt hour data  
  float MTD_Wh, YTD_Wh = 0;
  float PreviousWh, CTSaveThreshold = 0;
  double Stabilized = 0;
  float saved_Wh, saved_MTD_Wh, saved_YTD_Wh = 0;

  char resx[8];

  CTPROCESSOR(uint8_t _CTPin, double _ICAL, uint8_t _inPinV, double _VCAL, double _PHASECAL);
  ~CTPROCESSOR();  
  int readPower(float adjustment);
  int DisplayPower(Adafruit_SSD1306 &display, AsyncMqttClient &mqttClient);

  private:

};
#endif

#endif