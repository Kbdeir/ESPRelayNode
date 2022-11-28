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
    String jsonPost = " ";

    String  maSTopic = "\\controller\pressure";
    uint16_t maSLC = 72;
    uint16_t maSHC = 945;
    uint16_t BurdenResistorValue = 51;          
    uint16_t max_sensor_measurment_capacity = 400;
    uint16_t max_tank_capacity = 300;  
    
    TLPressureSensor(uint8_t _Pin, uint16_t _max_sensor_measurment_capacity_meters, uint16_t _max_tank_capacity_meters, uint16_t _BurdenResistorValue);
    ~TLPressureSensor();  
    float read(unsigned int nsamples);
    int DisplayLevel(Adafruit_SSD1306 &display, bool wificonnected, bool mqttconnected, uint8_t sco) ;
    void preparejson();
    void preparejsontemplate();

  private:
    uint8_t SPin;
    String jsonPost_temp = F("json:{'msg':{'source':'[SOURCE]','data':[{'ADC':'[ADC]', 'cm':'[cm]', 'fp':'[fp]', 'max_tank':'[max_tank]', 'max_sensor':'[max_sensor]', 'IP':'[IP]'}]}}");  
    float mapf(float x, float in_min, float in_max, float out_min, float out_max);    
};

    config_read_error_t TLloadconfig(char* filename, TLPressureSensor &para_PressureSensorConfig);
    config_read_error_t TLsaveconfig(AsyncWebServerRequest *request);
#endif

#endif