#ifdef ESP32
#ifdef WaterFlowSensor

#include "Arduino.h"
#include "WaterFlowSensor.h"
#include "esp_adc_cal.h" 
#include <JSONConfig.h>

#define Tbuffer_size  500

float calibrationFactor = 4.5;
String WaterFlowSensor_Topic ="\\Flow" ;


void saveDefaultWFSConfig(){
  StaticJsonDocument<500> json;

  json["wfs_Topic"]="FlowRate";
  json["wfs_Cal"]="4.5";

  File configFile = SPIFFS.open("/WaterFlowSensorConfig.json", "w",true);
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to write default WaterFlowSensorConfig config file"));
    exit;

  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to WaterFlowSensorConfig.json"));
  }
    configFile.println("\n\n");
    configFile.flush();
    configFile.close();

}


void loadconfigWFS(char* filename){
  Serial.println(F("[INFO  TP] opening /WaterFlowSensorConfig.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] WaterFlowSensorConfig file does not exist!, creating defaults."));
    saveDefaultWFSConfig();
  }

  Serial.println(F("[INFO  TP] opening WaterFlowSensorConfig.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open WaterFlowSensorConfig file"));
    exit;
  }

  Serial.println(F("[INFO  TP] opening /WaterFlowSensorConfig.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] WaterFlowSensorConfig file size is too large, rebuilding."));
    saveDefaultWFSConfig();
    exit;
  }

  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("Failed to parse WaterFlowSensorConfig file"));
    saveDefaultWFSConfig();
    exit;
  }

  WaterFlowSensor_Topic = (json["wfs_Topic"].as<String>()!="") ? json["wfs_Topic"].as<String>() : "FlowRate";
  calibrationFactor = (json["wfs_Cal"].as<String>()!="") ? json["wfs_Cal"].as<float>() : 4.5;
 
  configFile.close();

};



void saveconfigWFS(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
    char  timerfilename[29] = "/WaterFlowSensorConfig.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open WaterFlowSensorConfig file for writing"));
  }

  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] =  request->arg(i) ;
  }

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




