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
  #endif  

  uint8_t CTPin, inPinV;
  double ICAL,VCAL,PHASECAL;


  double realPower;
  double apparentPower;
  double apparentPower_2;  
  double powerFactor ;
  double supplyVoltage;
  double Irms ;  
  double amps = 0;
  bool CT_RelayForceOff = false;

  uint32_t ThresholdCossHighTimerCounter = 0; //in seconds
  uint32_t ThresholdCossLowTimerCounter = 0;   // in seconds


  uint32_t time_on = 10; //in seconds
  uint32_t time_off = 10;   // in seconds
  unsigned long lwhtime = 0;            // Last watt hour time
  float wh, Instantaneous_Wh = 0.0;     // Stores watt hour data  
  float MTD_Wh, YTD_Wh = 0;
  float PreviousWh, CTSaveThreshold = 0;
  double Stabilized = 0;
  float saved_Wh, saved_MTD_Wh, saved_YTD_Wh = 0;
  String jsonPost_template = F("json:{\"msg\":{\"source\":\"[SOURCE]\",\"data\":[{\"voltage\":\"[VOLTAGE]\",\"amps\":\"[AMPS]\", \"power\":\"[POWER]\", \"r_power\":\"[R_POWER]\", \"wh\":\"[WH]\", \"MTD_wh\":\"[MTD_WH]\", \"YTD_wh\":\"[YTD_WH]\",\"PF\":\"[PF]\", \"PFC\":\"[PFC]\",\"IP\":\"[IP]\",\"R1\":\"[R1]\",\"R2\":\"[R2]\"}]}}");
  String jsonPost = " ";
 // char resx[8];

  CTPROCESSOR(uint8_t _CTPin, double _ICAL, uint8_t _inPinV, double _VCAL, double _PHASECAL,
              fnptr_a pThresholdHigh_callback = nullptr, fnptr_a pThresholdLow_callback = nullptr);
  ~CTPROCESSOR();  
  int readPower(float adjustment, uint8_t savethreshold);
  int readVoltage();  
  int DisplayPower(Adafruit_SSD1306 &display, AsyncMqttClient &mqttClient, uint8_t sco);

  bool ThresholdCossHighTimerActive() ;
  bool ThresholdCossHighTimerStart() ;
  bool ThresholdCossHighTimerStop() ;

  bool ThresholdCossLowTimerActive() ;
  bool ThresholdCossLowTimerStart() ;
  bool ThresholdCossLowTimerStop() ;

  fnptr_a fThresholdHigh_callback;
  fnptr_a fThresholdLow_callback;
  private:

};
#endif

#endif