#include <ADS11x5Config.h>
#define Tbuffer_size  1000


ADS11x5Config::ADS11x5Config(uint8_t para_id) {
    id = para_id;
    enabled = true;
    Res1 = 0;
    Res2 = 0;
    ResMultiplier = 0;
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
    ResMultiplier   = para_ResMultiplier,
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

bool saveADS11x5Config(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
    char  timerfilename[20] = "/ADS11x5Config.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open ADS11x5Config file for writing"));
    return false;
  }


  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] =  request->arg(i) ;
  }

  request->hasParam("CEnabled")   ? json["CEnabled"]   =  "1"   : json["CEnabled"]   = "0" ;

  // Serialize JSON to file
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
    configFile.flush();
    configFile.close();

  return true;
}



config_read_error_t loadADS11x5Config(char* filename, ADS11x5Config &para_ADS11x5Config) {

  //Serial.println(F("[INFO  TP] opening /ADS11x5Config.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] ADS11x5Config file does not exist!:"));
    Serial.print(filename);
    return FILE_NOT_FOUND;
  }

  //Serial.println(F("[INFO  TP] opening /ADS11x5Config.json file - 1"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open ADS11x5Config file"));
    return ERROR_OPENING_FILE;
  }

  //Serial.println(F("[INFO  TP] opening /ADS11x5Config.json file - 2"));
  size_t size = configFile.size();
  if (size > Tbuffer_size) {
    Serial.println(F("\n[INFO   ] ADS11x5Config file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }


  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("Failed to read ADS11x5Config file, using default configuration"));  

  if (error) {
    Serial.println(F("Failed to parse ADS11x5Config file"));
  //  saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  para_ADS11x5Config.id             = (json["ADSNB"].as<String>()!="") ? json["ADSNB"].as<uint8_t>() : 0;
  para_ADS11x5Config.enabled        = json["CEnabled"]; //.as<String>()!="") ? json["CEnabled"].as<uint8_t>() : 0;
  para_ADS11x5Config.Res1           = (json["Res1"].as<String>()!="") ? json["Res1"].as<double>() : 0;
  para_ADS11x5Config.Res2           = (json["Res2"].as<String>()!="") ? json["Res2"].as<double>() : 0;
  para_ADS11x5Config.ResMultiplier  = (json["ResMultiplier"].as<String>()!="") ? json["ResMultiplier"].as<double>() : 0;  
  para_ADS11x5Config.relay1         = (json["TRelay1"].as<String>()!="") ? json["TRelay1"].as<uint8_t>() : 0;
  para_ADS11x5Config.relay2         = (json["TRelay2"].as<String>()!="") ? json["TRelay2"].as<uint8_t>() : 0;

  configFile.close();
    
  return SUCCESS;
}



