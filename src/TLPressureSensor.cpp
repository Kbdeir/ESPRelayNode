#ifdef ESP32

#include "Arduino.h"
#include "TLPressureSensor.h"
#include "esp_adc_cal.h" 
#define Tbuffer_size  500

esp_adc_cal_characteristics_t chars;

float TLPressureSensor::mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

TLPressureSensor::TLPressureSensor(uint8_t _Pin, uint16_t _max_sensor_measurment_capacity_meters, uint16_t _max_tank_capacity_meters, uint16_t _BurdenResistorValue)
 {
  SPin = _Pin;
  pinMode (35, INPUT_PULLDOWN);
 // analogReadResolution(12);
 // adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_0);   //ADC_ATTEN_DB_11
  auto val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &chars);
  sensor_zero_readings = sensor_lower_ma * BurdenResistorValue;
  max_tank_capacity_meters = _max_tank_capacity_meters;
  max_sensor_measurment_capacity_meters = _max_sensor_measurment_capacity_meters;
 }


TLPressureSensor::~TLPressureSensor(){
}


float TLPressureSensor::read(unsigned int nsamples) {
      double measure2 = 0;
      for (unsigned int nn = 0; nn < nsamples; nn++)
        {           
          sampleI += adc1_get_raw((adc1_channel_t)SPin); //analogRead(SPin); //
          yield();
        }  
        sampleI /=  nsamples;
        double voltage1 = esp_adc_cal_raw_to_voltage(sampleI, &chars);            
        double top = maSHC; // 3000;
        measure2 =  (sampleI-maSLC) * (max_sensor_measurment_capacity_meters/(top - maSLC));      //277 / 169; // maSLC = 553 
        measure = mapf(sampleI, maSLC, top ,0 , max_sensor_measurment_capacity_meters);
        
        Serial.printf("[CT136  ] pressure ADC reading = %.1f , measurment by maping ADC reading - cm: = %.1f, measurement by steps - cm: %.1f , voltage1 %.2f, top %f \n", 
        sampleI, measure, measure2, voltage1, top);    

        jsonPost = jsonPost_temp; 
        jsonPost.replace("[SOURCE]","Controller_CT");        
        jsonPost.replace("[sampleADC]",String(sampleI));   
        jsonPost.replace("[cm]",String(measure));           
 }



config_read_error_t TLloadconfig(char* filename, TLPressureSensor &para_PressureSensorConfig){

  Serial.println(F("[INFO  TP] opening /PressureSensorConfig.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] PressureSensorConfig file does not exist!:"));
    Serial.print(filename);
    return FILE_NOT_FOUND;
  }

  Serial.println(F("[INFO  TP] opening /PressureSensorConfig.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open PressureSensorConfig file"));
    return ERROR_OPENING_FILE;
  }

  Serial.println(F("[INFO  TP] opening /PressureSensorConfig.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] PressureSensorConfig file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }


  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("Failed to read file, using default PressureSensorConfig configuration"));  

  if (error) {
    Serial.println(F("Failed to parse PressureSensorConfig file"));
  //  saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  para_PressureSensorConfig.maSTopic = (json["maSTopic"].as<String>()!="") ? json["maSTopic"].as<String>() : "0";
  para_PressureSensorConfig.max_sensor_measurment_capacity_meters = (json["maSHL"].as<String>()!="") ? json["maSHL"].as<uint16_t>() : 0;
  para_PressureSensorConfig.max_tank_capacity_meters = (json["TankHeight"].as<String>()!="") ? json["TankHeight"].as<uint16_t>() : 0;  
  para_PressureSensorConfig.maSHC = (json["maSHC"].as<String>()!="") ? json["maSHC"].as<uint16_t>() : 0;
  para_PressureSensorConfig.maSLC = (json["maSLC"].as<String>()!="") ? json["maSLC"].as<uint16_t>() : 0;  
  para_PressureSensorConfig.BurdenResistorValue = (json["maBurdenResistor"].as<String>()!="") ? json["maBurdenResistor"].as<uint16_t>() : 0;    

    configFile.flush();
    configFile.close();
    
  return SUCCESS;
};


config_read_error_t TLsaveconfig(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
    char  timerfilename[30] = "/PressureSensorConfig.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open PressureSensorConfig file for writing"));
    return SUCCESS;
  }


  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] =  request->arg(i) ;
  }

  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
    configFile.flush();
    configFile.close();

  return SUCCESS;
};




  int TLPressureSensor::DisplayLevel(Adafruit_SSD1306 &display, bool wificonnected, bool mqttconnected){
     #ifdef OLED_1306
        display.clearDisplay();
        display.setTextSize(1);
        #define StartRow 20 
        #define rect_height 30
        #define rect_width 110

        display.drawRect(0,StartRow,rect_width,rect_height, WHITE);
 
        display.setCursor(0, 0);
        display.println("Water Level Readings");      
        display.printf("%.1f percent", (measure/max_tank_capacity_meters)*100);                        
        
        display.setCursor(40, StartRow);
        display.println(F("Level"));
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(20,StartRow + 14);       
        display.setTextSize(2); 
        display.printf("%.1f",measure);
        display.setTextSize(1);        
        display.print(" cm");

        display.setCursor(1,54);  // col,row      
        display.setTextColor(BLACK,WHITE);
        display.print(WiFi.localIP().toString());
        display.setTextColor(WHITE);
        display.setCursor(120,54);  // col,row    
        if (wificonnected)  display.print("*");   
        if (!wificonnected) display.print("x");  
        display.setCursor(110,54);  // col,row   
        if (mqttconnected) display.print("M");   
        if (!mqttconnected) display.print("m");    
      #endif    
  }

#endif





