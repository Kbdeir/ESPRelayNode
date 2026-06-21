
#include <TempConfig.h>
#define Tbuffer_size  500

#define TEMP_DEFAULT_ID           1
#define TEMP_DEFAULT_TEMPFROM     40
#define TEMP_DEFAULT_TEMPTO       90
#define TEMP_DEFAULT_BUFFER       8
#define TEMP_DEFAULT_RELAY        1


TempConfig::TempConfig(uint8_t para_id) {
    id           = para_id;
    enabled      = true;
    spanTempfrom = TEMP_DEFAULT_TEMPFROM;
    spanTempto   = TEMP_DEFAULT_TEMPTO;
    spanBuffer   = TEMP_DEFAULT_BUFFER;
    relay        = TEMP_DEFAULT_RELAY;
    // Testchar = new char[22]; //"Hello there I am char";
}

TempConfig::TempConfig(uint8_t para_id,
              uint16_t para_spanTempfrom,
              uint16_t para_spanTempto,
              uint16_t para_spanBuffer,
              boolean para_enabled,
              uint8_t para_relay
) {
    id           = para_id;
    spanTempfrom = para_spanTempfrom;
    spanTempto   = para_spanTempto;
    spanBuffer   = para_spanBuffer;
    relay        = para_relay;
    enabled      = true;
    //Testchar = new char[22]; //"Hello there I am char";
}

TempConfig::~TempConfig(){
    // delete[] Testchar;
}

void TempConfig::watch(){
}

static void writeTempConfigDefaults(const char* filename, TempConfig &cfg) {
  StaticJsonDocument<buffer_size> json;
  json["TNumber"]      = cfg.id;
  json["CEnabled"]     = cfg.enabled ? "1" : "0";
  json["spanTempfrom"] = cfg.spanTempfrom;
  json["spanTempto"]   = cfg.spanTempto;
  json["spanBuffer"]   = cfg.spanBuffer;
  json["TRelay"]       = cfg.relay;

  File f = SPIFFS.open(filename, "w");
  if (f) {
    serializeJson(json, f);
    f.flush();
    f.close();
    Serial.println(F("\n[INFO   ] tempconfig defaults written."));
  }
}

bool saveTempConfig(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
  char timerfilename[20] = "/tempconfig.json";

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open tempconfig file for writing"));
    return false;
  }

  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] = request->arg(i);
  }

  request->hasParam("CEnabled") ? json["CEnabled"] = "1" : json["CEnabled"] = "0";

  // Serialize JSON to file
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.flush();
  configFile.close();

  return true;
}



config_read_error_t loadTempConfig(char* filename, TempConfig &para_TempConfig) {

  Serial.println(F("[INFO  TP] opening /tempconfig.json file - 0"));
  if (!SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] tempconfig file does not exist — creating with defaults."));
    writeTempConfigDefaults(filename, para_TempConfig);
    return SUCCESS;
  }

  Serial.println(F("[INFO  TP] opening /tempconfig.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open tempconfig file"));
    return ERROR_OPENING_FILE;
  }

  Serial.println(F("[INFO  TP] opening /tempconfig.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] tempconfig file size is too large, rebuilding."));
    configFile.close();
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  configFile.close();

  if (error) {
    Serial.println(F("Failed to parse tempconfig file"));
    return JSONCONFIG_CORRUPTED;
  }

  para_TempConfig.id           = (json["TNumber"].as<String>()!="")      ? json["TNumber"].as<uint8_t>()      : TEMP_DEFAULT_ID;
  para_TempConfig.enabled      = (json["CEnabled"].as<String>() == "1");
  para_TempConfig.spanTempfrom = (json["spanTempfrom"].as<String>()!="") ? json["spanTempfrom"].as<uint16_t>() : TEMP_DEFAULT_TEMPFROM;
  para_TempConfig.spanTempto   = (json["spanTempto"].as<String>()!="")   ? json["spanTempto"].as<uint16_t>()   : TEMP_DEFAULT_TEMPTO;
  para_TempConfig.spanBuffer   = (json["spanBuffer"].as<String>()!="")   ? json["spanBuffer"].as<uint16_t>()   : TEMP_DEFAULT_BUFFER;
  para_TempConfig.relay        = (json["TRelay"].as<String>()!="")       ? json["TRelay"].as<uint8_t>()        : TEMP_DEFAULT_RELAY;

  return SUCCESS;
}


