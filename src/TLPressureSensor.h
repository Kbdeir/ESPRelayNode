//#ifdef _pressureSensor_

#ifndef _TLPressureSensor_
#define _TLPressureSensor_

#ifdef ESP32

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ADC_BITS    12
#define ADC_COUNTS  (1<<ADC_BITS)
class TLPressureSensor
{
  public:
    float sampleI, measure = 0;
    float_t sensor_lower_ma = 0.004;
    float_t sensor_upper_ma = 0.02;
    int SupplyVoltage = 3290;
    char jsonPost[256];

    String maSTopic = "\\controller\\pressure";
    uint16_t maSLC = 773;
    uint16_t maSHC = 3864;
    uint16_t BurdenResistorValue = 150;
    int16_t  paramEmptyValue = 0;
    uint16_t paramFullValue  = 400;
    
    TLPressureSensor(uint8_t _Pin, int16_t _paramEmptyValue, uint16_t _paramFullValue, uint16_t _BurdenResistorValue);
    ~TLPressureSensor();  
    float read(unsigned int nsamples);
    void DisplayLevel(Adafruit_SSD1306 &display, bool wificonnected, bool mqttconnected, uint8_t sco) ;
    void preparejson();
    void preparejsontemplate();

  private:
    uint8_t SPin;
    float mapf(float x, float in_min, float in_max, float out_min, float out_max);    
};

    config_read_error_t TLloadconfig(char* filename, TLPressureSensor &para_PressureSensorConfig);
    config_read_error_t TLsaveconfig(AsyncWebServerRequest *request);
#endif

#endif
//#endif