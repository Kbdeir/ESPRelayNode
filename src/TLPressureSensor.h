#ifndef _TLPressureSensor_
#define _TLPressureSensor_

#ifdef ESP32
#include "Arduino.h"
#if defined(ESP32)
#define ADC_BITS    12
#define ADC_COUNTS  (1<<ADC_BITS)
#endif


#include <ConfigParams.h>
#include <JSONConfig.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>




class TLPressureSensor
{
  public:
    float sampleI, measure = 0;
    float_t sensor_lower_ma = 0.004;
    float_t sensor_upper_ma = 0.02;
    int SupplyVoltage = 3200;
    String jsonPost = " ";

    String  maSTopic;
    uint16_t maSLC = 0;
    uint16_t maSHC = 0;
    uint16_t BurdenResistorValue = 150;          
    uint16_t max_sensor_measurment_capacity_meters = 200;
  
    TLPressureSensor(uint8_t _Pin, uint8_t _max_sensor_measurment_capacity_meters, uint8_t _max_tank_capacity_meters, uint8_t _BurdenResistorValue);
    ~TLPressureSensor();  
    float read(unsigned int nsamples);

    int DisplayLevel(Adafruit_SSD1306 &display);

  private:
    uint8_t SPin;
    float_t sensor_zero_readings;
    uint16_t max_tank_capacity_meters = 300;

    String jsonPost_temp = F("json:{\"msg\":{\"source\":\"[SOURCE]\",\"data\":[{\"sampleADC\":\"[sampleADC]\", \"cm\":\"[cm]\"}]}}");  
    float mapf(float x, float in_min, float in_max, float out_min, float out_max);    
};

    config_read_error_t TLloadconfig(char* filename, TLPressureSensor &para_PressureSensorConfig);
    config_read_error_t TLsaveconfig(AsyncWebServerRequest *request);
#endif

#endif