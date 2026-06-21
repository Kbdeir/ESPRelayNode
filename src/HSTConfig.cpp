#include <HSTConfig.h>
#define Tbuffer_size  1000

#define HST_DEFAULT_AMPSVOLTSRATIO  6.25


HSTConfig::HSTConfig(uint8_t para_id)
{
    id = para_id;
    enabled = true;
    AmpsVoltsRatio = HST_DEFAULT_AMPSVOLTSRATIO;
    manualOffset = 0;

}

HSTConfig::HSTConfig(uint8_t para_id,
              double para_AmpsVoltsRatio,
              boolean para_enabled) 
{
    id              = para_id;
    AmpsVoltsRatio  = para_AmpsVoltsRatio;
    manualOffset    = 0;
    enabled         = true;

}

HSTConfig::~HSTConfig(){
    // delete[] Testchar;
}

void HSTConfig::watch(){
}

static void writeHSTConfigDefaults(const char* filename, HSTConfig &cfg) {
  StaticJsonDocument<buffer_size> json;
  json["HSTNB"]          = cfg.id;
  json["CEnabled"]       = "1";
  json["AmpsVoltsRatio"] = HST_DEFAULT_AMPSVOLTSRATIO;
  json["manualOffset"]   = cfg.manualOffset;

  File f = SPIFFS.open(filename, "w");
  if (f) {
    serializeJson(json, f);
    f.flush();
    f.close();
    Serial.println(F("\n[INFO   ] HSTConfig defaults written."));
  }
}

bool saveHSTConfig(AsyncWebServerRequest *request)
{
  StaticJsonDocument<buffer_size> json;
  char  timerfilename[20] = "/HSTConfig.json";
  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open HSTConfig file for writing"));
    return false;
  }

  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] =  request->arg(i) ;
  }

  request->hasParam("CEnabled")   ? json["CEnabled"]   =  "1"   : json["CEnabled"]   = "0" ;
  if (!request->hasParam("HSTNB")) json["HSTNB"] = "1";

  // Serialize JSON to file
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
    configFile.flush();
    configFile.close();

  return true;
}



config_read_error_t loadHSTConfig(char* filename, HSTConfig &para_HSTConfig) {

  //Serial.println(F("[INFO  TP] opening /HSTConfig.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] HSTConfig file does not exist — creating with defaults."));
    writeHSTConfigDefaults(filename, para_HSTConfig);
    return SUCCESS;
  }

  //Serial.println(F("[INFO  TP] opening /HSTConfig.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open HSTConfig file"));
    return ERROR_OPENING_FILE;
  }

  //Serial.println(F("[INFO  TP] opening /HSTConfig.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] HSTConfig file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }


  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("Failed to read HSTConfig file, using default configuration"));  

  if (error) {
    Serial.println(F("Failed to parse HSTConfig file"));
  //  saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  para_HSTConfig.id             = (json["HSTNB"].as<String>()!="") ? json["HSTNB"].as<uint8_t>() : 0;
  para_HSTConfig.enabled        = (json["CEnabled"].as<String>() == "1");
  para_HSTConfig.AmpsVoltsRatio = (json["AmpsVoltsRatio"].as<String>()!="") ? json["AmpsVoltsRatio"].as<double>() : HST_DEFAULT_AMPSVOLTSRATIO;
  para_HSTConfig.manualOffset   = (json["manualOffset"].as<String>()!="") ? json["manualOffset"].as<double>() : 0;

  configFile.close();
    
  return SUCCESS;
}



