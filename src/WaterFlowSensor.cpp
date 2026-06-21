#include "Arduino.h"
#include "defines.h"

#ifdef ESP32
#ifdef WaterFlowSensor

#include "WaterFlowSensor.h"
#include "esp_adc_cal.h" 
#include <JSONConfig.h>

#define Tbuffer_size  500

float calibrationFactor = 4.5;
String WaterFlowSensor_Topic = "Flow";


void saveDefaultWFSConfig(){
  StaticJsonDocument<500> json;

  json["wfs_Topic"]="/home/Controller" + CID() + "/WFS/FlowRate";
  json["wfs_Cal"]=4.5f;

  File configFile = SPIFFS.open("/WaterFlowSensorConfig.json", "w",true);
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to write default WaterFlowSensorConfig config file"));
    return;
  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to WaterFlowSensorConfig.json"));
  }
    configFile.print("\n");
    configFile.flush();
    configFile.close();

}


void loadconfigWFS(const char* filename){
  Serial.println(F("[INFO  TP] opening /WaterFlowSensorConfig.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] WaterFlowSensorConfig file does not exist!, creating defaults."));
    saveDefaultWFSConfig();
  }

  Serial.println(F("[INFO  TP] opening WaterFlowSensorConfig.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open WaterFlowSensorConfig file"));
    return;
  }

  Serial.println(F("[INFO  TP] opening /WaterFlowSensorConfig.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] WaterFlowSensorConfig file size is too large, rebuilding."));
    configFile.close();
    saveDefaultWFSConfig();
    return;
  }

  StaticJsonDocument<Tbuffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("Failed to parse WaterFlowSensorConfig file"));
    configFile.close();
    saveDefaultWFSConfig();
    return;
  }

  WaterFlowSensor_Topic = (json["wfs_Topic"].as<String>()!="") ? json["wfs_Topic"].as<String>() : ("/home/Controller" + CID() + "/WFS/FlowRate");
  calibrationFactor = json["wfs_Cal"] | 4.5f;
 
  configFile.close();

};



void saveconfigWFS(AsyncWebServerRequest *request){
  StaticJsonDocument<256> json;
    char  timerfilename[29] = "/WaterFlowSensorConfig.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open WaterFlowSensorConfig file for writing"));
    return;
  }

  if (request->hasParam("wfs_Topic")) json["wfs_Topic"] = request->getParam("wfs_Topic")->value();
  if (request->hasParam("wfs_Cal"))   json["wfs_Cal"]   = request->getParam("wfs_Cal")->value().toFloat();

  // request->hasParam("CEnabled")   ? json["CEnabled"]   =  "1"   : json["CEnabled"]   = "0" ;

  // Serialize JSON to file
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
    configFile.flush();
    configFile.close();
};




#endif
#endif




