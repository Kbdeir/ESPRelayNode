#include <ADS11x5Config.h>
#define Tbuffer_size  1000

#define ADS_DEFAULT_ID           1
#define ADS_DEFAULT_RES1         100000.0
#define ADS_DEFAULT_RES2         3300.0
#define ADS_DEFAULT_MULTIPLIER   ((ADS_DEFAULT_RES2 / (ADS_DEFAULT_RES1 + ADS_DEFAULT_RES2)) * 1000.0)
#define ADS_DEFAULT_RELAY        1


ADS11x5Config::ADS11x5Config(uint8_t para_id) {
    id            = para_id;
    enabled       = true;
    Res1          = ADS_DEFAULT_RES1;
    Res2          = ADS_DEFAULT_RES2;
    ResMultiplier = ADS_DEFAULT_MULTIPLIER;
    relay1        = ADS_DEFAULT_RELAY;
    relay2        = ADS_DEFAULT_RELAY;
    // Testchar = new char[22]; //"Hello there I am char";
}

ADS11x5Config::ADS11x5Config(uint8_t para_id,
              double para_Res1,
              double para_Res2,
              double para_ResMultiplier,
              boolean para_enabled,
              uint8_t para_relay1,
              uint8_t para_relay2
) {
    id              = para_id;
    Res1            = para_Res1;
    Res2            = para_Res2;
    ResMultiplier   = para_ResMultiplier;
    relay1          = para_relay1;
    relay2          = para_relay2;
    enabled         = true;
    //Testchar = new char[22]; //"Hello there I am char";
}

ADS11x5Config::~ADS11x5Config(){
    // delete[] Testchar;
}

void ADS11x5Config::watch(){
}

static void writeADS11x5Defaults(const char* filename, ADS11x5Config &cfg) {
  StaticJsonDocument<buffer_size> json;
  json["ADSNB"]         = cfg.id;
  json["CEnabled"]      = cfg.enabled ? "1" : "0";
  json["Res1"]          = cfg.Res1;
  json["Res2"]          = cfg.Res2;
  json["ResMultiplier"] = cfg.ResMultiplier;
  json["TRelay1"]       = cfg.relay1;
  json["TRelay2"]       = cfg.relay2;

  File f = SPIFFS.open(filename, "w");
  if (f) {
    serializeJson(json, f);
    f.flush();
    f.close();
    Serial.println(F("\n[INFO   ] ADS11x5Config defaults written."));
  }
}

bool saveADS11x5Config(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
  char timerfilename[20] = "/ADS11x5Config.json";

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open ADS11x5Config file for writing"));
    return false;
  }

  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] = request->arg(i);
  }

  request->hasParam("CEnabled") ? json["CEnabled"] = "1" : json["CEnabled"] = "0";

  // Disabled fields are not submitted — preserve defaults
  if (!request->hasParam("ADSNB"))   json["ADSNB"]   = ADS_DEFAULT_ID;
  if (!request->hasParam("TRelay1")) json["TRelay1"] = ADS_DEFAULT_RELAY;
  if (!request->hasParam("TRelay2")) json["TRelay2"] = ADS_DEFAULT_RELAY;

  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.flush();
  configFile.close();

  return true;
}



config_read_error_t loadADS11x5Config(char* filename, ADS11x5Config &para_ADS11x5Config) {

  if (!SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] ADS11x5Config file does not exist — creating with defaults."));
    writeADS11x5Defaults(filename, para_ADS11x5Config);
    return SUCCESS;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open ADS11x5Config file"));
    return ERROR_OPENING_FILE;
  }

  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] ADS11x5Config file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  configFile.close();

  if (error) {
    Serial.println(F("Failed to parse ADS11x5Config file"));
    return JSONCONFIG_CORRUPTED;
  }

  para_ADS11x5Config.id            = (json["ADSNB"].as<String>()!="")         ? json["ADSNB"].as<uint8_t>()        : ADS_DEFAULT_ID;
  para_ADS11x5Config.enabled       = (json["CEnabled"].as<String>() == "1");
  para_ADS11x5Config.Res1          = (json["Res1"].as<String>()!="")           ? json["Res1"].as<double>()          : ADS_DEFAULT_RES1;
  para_ADS11x5Config.Res2          = (json["Res2"].as<String>()!="")           ? json["Res2"].as<double>()          : ADS_DEFAULT_RES2;
  para_ADS11x5Config.ResMultiplier = (json["ResMultiplier"].as<String>()!="")  ? json["ResMultiplier"].as<double>() : ADS_DEFAULT_MULTIPLIER;
  para_ADS11x5Config.relay1        = (json["TRelay1"].as<String>()!="")        ? json["TRelay1"].as<uint8_t>()      : ADS_DEFAULT_RELAY;
  para_ADS11x5Config.relay2        = (json["TRelay2"].as<String>()!="")        ? json["TRelay2"].as<uint8_t>()      : ADS_DEFAULT_RELAY;

  return SUCCESS;
}


