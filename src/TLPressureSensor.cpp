#ifdef ESP32
//#ifdef _pressureSensor_

#include "Arduino.h"
#include "TLPressureSensor.h"
#include "esp_adc_cal.h"
#include "esp_task_wdt.h"
#include "CT_ProcessPower.h"
#include "SerialLog.h"

#define TL_BUFFER_SIZE  512
extern bool webing;
#ifdef _emonlib_ 
extern CTPROCESSOR CT_1;
#endif

esp_adc_cal_characteristics_t chars;

float TLPressureSensor::mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

TLPressureSensor::TLPressureSensor(uint8_t _Pin, int16_t _paramEmptyValue, uint16_t _paramFullValue, uint16_t _BurdenResistorValue)
 {
    SPin = _Pin;
    BurdenResistorValue = _BurdenResistorValue;
    paramEmptyValue = _paramEmptyValue;
    paramFullValue  = _paramFullValue;
    analogReadResolution(12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
    auto val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &chars);
    jsonPost[0] = '\0';
 }

TLPressureSensor::~TLPressureSensor(){
}


float TLPressureSensor::read(unsigned int nsamples) {
      if (!webing) {
        float acc = 0.0f;
        for (unsigned int nn = 0; nn < nsamples; nn++) {
          acc += analogRead(SPin);
          esp_task_wdt_reset();
        }
        sampleI = acc / nsamples;
        measure = mapf(sampleI, maSLC, maSHC, paramEmptyValue, paramFullValue);
        preparejson();
        SLOG(SLOG_CT136, "[CT136  ] ADC=%.1f  cm=%.1f  empty=%d  full=%d\n", sampleI, measure, paramEmptyValue, paramFullValue);
        return measure;
      }
      return measure;  // return last known value when web UI is active
}


void TLPressureSensor::preparejson() {
    float span = (float)(paramFullValue - paramEmptyValue);
    float fp   = (span != 0.0f) ? ((measure - paramEmptyValue) / span * 100.0f) : 0.0f;
    IPAddress ip = WiFi.localIP();
    char ipBuf[16];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    #ifdef _emonlib_
    char voltBuf[12];
    snprintf(voltBuf, sizeof(voltBuf), "%.0f", (double)CT_1.supplyVoltage);
    #else
    const char* voltBuf = "NA";
    #endif
    snprintf(jsonPost, sizeof(jsonPost),
        "json:{\"msg\":{\"source\":\"Controller_CT\",\"data\":[{"
        "\"ADC\":\"%.0f\",\"cm\":\"%.1f\",\"fp\":\"%.1f\","
        "\"param_empty\":\"%d\",\"param_full\":\"%d\","
        "\"IP\":\"%s\",\"voltage\":\"%s\"}]}}",
        (double)sampleI, (double)measure, (double)fp,
        (int)paramEmptyValue, (int)paramFullValue,
        ipBuf, voltBuf);
}

void TLPressureSensor::preparejsontemplate() {
    // no-op: template is now built inline in preparejson()
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
  if (size > TL_BUFFER_SIZE) {
    Serial.println(F("\n[INFO   ] PressureSensorConfig file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<TL_BUFFER_SIZE> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("Failed to read file, using default PressureSensorConfig configuration"));  

  if (error) {
    Serial.println(F("Failed to parse PressureSensorConfig file"));
    return JSONCONFIG_CORRUPTED;
  }

  para_PressureSensorConfig.maSTopic        = (json["maSTopic"].as<String>()!="")        ? json["maSTopic"].as<String>()          : "\\controller\\pressure";
  para_PressureSensorConfig.paramEmptyValue = (json["paramEmptyValue"].as<String>()!="") ? json["paramEmptyValue"].as<int16_t>()  : 0;
  para_PressureSensorConfig.paramFullValue  = (json["paramFullValue"].as<String>()!="")  ? json["paramFullValue"].as<uint16_t>()  : 400;
  para_PressureSensorConfig.maSHC           = (json["maSHC"].as<String>()!="")           ? json["maSHC"].as<uint16_t>()           : 3864;
  para_PressureSensorConfig.maSLC           = (json["maSLC"].as<String>()!="")           ? json["maSLC"].as<uint16_t>()           : 773;
  para_PressureSensorConfig.BurdenResistorValue = (json["maBurdenResistor"].as<String>()!="") ? json["maBurdenResistor"].as<uint16_t>() : 150;

  configFile.flush();
  configFile.close();
    
  return SUCCESS;
};



config_read_error_t TLsaveconfig(AsyncWebServerRequest *request){
  StaticJsonDocument<TL_BUFFER_SIZE> json;
    char  timerfilename[30] = "/PressureSensorConfig.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open PressureSensorConfig file for writing"));
    return ERROR_OPENING_FILE;
  }

  if (request->hasParam("maSTopic"))        json["maSTopic"]        = request->getParam("maSTopic")->value();
  if (request->hasParam("paramEmptyValue")) json["paramEmptyValue"] = request->getParam("paramEmptyValue")->value().toInt();
  if (request->hasParam("paramFullValue"))  json["paramFullValue"]  = request->getParam("paramFullValue")->value().toInt();
  if (request->hasParam("maSLC"))           json["maSLC"]           = (uint16_t)request->getParam("maSLC")->value().toInt();
  if (request->hasParam("maSHC"))           json["maSHC"]           = (uint16_t)request->getParam("maSHC")->value().toInt();
  if (request->hasParam("maBurdenResistor")) json["maBurdenResistor"] = (uint16_t)request->getParam("maBurdenResistor")->value().toInt();

  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
    configFile.flush();
    configFile.close();

  return SUCCESS;
};




  void TLPressureSensor::DisplayLevel(Adafruit_SSD1306 &display, bool wificonnected, bool mqttconnected, uint8_t sco ){
     #ifdef OLED_1306
        display.setRotation(sco); // 1 = upsidedown
        display.cp437(true); 
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);

        const int16_t startRow = 20;
        const int16_t rectHeight = 30;
        const int16_t rectWidth = display.width();
        const int16_t rectLeft = 0;

        display.drawRect(rectLeft, startRow, rectWidth, rectHeight, WHITE);

        float span = (float)(paramFullValue - paramEmptyValue);
        float fp   = (span != 0.0f) ? ((measure - paramEmptyValue) / span * 100.0f) : 0.0f;
        display.setCursor(0, 0);
        display.println(F("Water Level Readings"));
        display.printf("%.1f %%", fp);

        int16_t x1, y1;
        uint16_t w, h;
        const char *label = "Level";
        display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(rectLeft + ((rectWidth - w) / 2), startRow);
        display.print(label);

        char reading[16];
        snprintf(reading, sizeof(reading), "%.1f cm", measure);
        display.setTextColor(SSD1306_WHITE);
        display.getTextBounds(reading, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(rectLeft + ((rectWidth - w) / 2), startRow + 14);
        display.print(reading);


        display.setCursor(1,54);  // col,row      
        display.setTextColor(BLACK,WHITE);
        display.print(WiFi.localIP().toString());
        display.setTextColor(WHITE);
        display.setCursor(100,54);  // col,row    
        if (wificonnected)  display.print(F("*"));   
        if (!wificonnected) display.print(F("x"));  
        display.setCursor(110,54);  // col,row   
        if (mqttconnected) display.print(F("M"));   
        if (!mqttconnected) display.print(F("m"));    
        display.display();
      #endif    
  }

#endif
//#endif



