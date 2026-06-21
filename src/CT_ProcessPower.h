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

#ifdef ESP32
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
	 #include "freertos/FreeRTOS.h"
	 #include "freertos/timers.h"
	 #include "freertos/semphr.h"
}
#endif

typedef void (*fnptr_a)(void* t);
class CTPROCESSOR
{
  public:
  EnergyMonitor emon1;                  // Create an instance 

  #ifdef ESP32
   TimerHandle_t ThresholdCossHighTimer;
   TimerHandle_t ThresholdCossLowTimer;
   SemaphoreHandle_t jsonPostMutex = nullptr;
  #endif  

  uint8_t CTPin, inPinV;
  double ICAL,VCAL,PHASECAL;


  float realPower = 0.0f;
  float apparentPower = 0.0f;
  float apparentPower_2 = 0.0f;
  float powerFactor = 0.0f;
  float supplyVoltage = 0.0f;
  float Irms = 0.0f;
  float amps = 0.0f;
  bool CT_RelayForceOff = false;

  uint32_t ThresholdCossHighTimerCounter = 0; //in seconds
  uint32_t ThresholdCossLowTimerCounter = 0;   // in seconds


  uint32_t time_on = 10; //in seconds
  uint32_t time_off = 10;   // in seconds
  unsigned long lwhtime = 0;            // Last watt hour time
  double wh = 0.0, Instantaneous_Wh = 0.0;
  double MTD_Wh = 0.0, YTD_Wh = 0.0;
  double PreviousWh = 0.0, CTSaveThreshold = 0.0;
  uint32_t Stabilized = 0;
  static const uint8_t WarmupReadings = 3;
  bool voltageSmoothingPrimed = false;
  float smoothedSupplyVoltage = 0.0f;
  double saved_Wh = 0.0, saved_MTD_Wh = 0.0, saved_YTD_Wh = 0.0;
  char jsonPost[512];

 // char resx[8];

  CTPROCESSOR(uint8_t _CTPin, double _ICAL, uint8_t _inPinV, double _VCAL, double _PHASECAL,
              fnptr_a pThresholdHigh_callback = nullptr, fnptr_a pThresholdLow_callback = nullptr);
  ~CTPROCESSOR();  
  void readPower(float adjustment, uint16_t savethreshold);
  void readVoltage();  
  void DisplayPower(Adafruit_SSD1306 &display, AsyncMqttClient &mqttClient, uint8_t sco);

  bool ThresholdCossHighTimerActive() ;
  void ThresholdCossHighTimerStart() ;
  void ThresholdCossHighTimerStop() ;

  bool ThresholdCossLowTimerActive() ;
  void ThresholdCossLowTimerStart() ;
  void ThresholdCossLowTimerStop() ;

  fnptr_a fThresholdHigh_callback;
  fnptr_a fThresholdLow_callback;

  int _last_reset_day   = -1;
  int _last_reset_month = -1;
  int _last_reset_year  = -1;

  private:

};
#endif

#endif
