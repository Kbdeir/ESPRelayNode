
#include <TempConfig.h>
#define Tbuffer_size  500


TempConfig::TempConfig(uint8_t para_id) {
    id = para_id;
    enabled = true;
    spanTempfrom = 0;
    spanTempto = 0;
    spanBuffer = 0;
    // Testchar = new char[22]; //"Hello there I am char";
}

TempConfig::TempConfig(uint8_t para_id,
              uint16_t para_spanTempfrom,
              uint16_t para_spanTempto,
              uint16_t para_spanBuffer,
              boolean para_enabled,
              uint8_t para_relay
) {
    id              = para_id;
    spanTempfrom    = para_spanTempfrom;
    spanTempto      = para_spanTempto;
    spanBuffer      = para_spanBuffer,
    relay           = para_relay;
    enabled         = true;
    //Testchar = new char[22]; //"Hello there I am char";    
}

TempConfig::~TempConfig(){
    // delete[] Testchar;
}

void TempConfig::watch(){
}

bool saveTempConfig(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
    char  timerfilename[20] = "/tempconfig.json";
  //strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open tempconfig file for writing"));
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



config_read_error_t loadTempConfig(char* filename, TempConfig &para_TempConfig) {

  Serial.println(F("[INFO  TP] opening /tempconfig.json file - 0"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] tempconfig file does not exist!:"));
    Serial.print(filename);
    return FILE_NOT_FOUND;
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
    return ERROR_OPENING_FILE;
  }


  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));  

  if (error) {
    Serial.println(F("Failed to parse tempconfig file"));
  //  saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  para_TempConfig.id = (json["TNumber"].as<String>()!="") ? json["TNumber"].as<uint8_t>() : 0;
  para_TempConfig.enabled = json["CEnabled"]; //.as<String>()!="") ? json["CEnabled"].as<uint8_t>() : 0;
  para_TempConfig.spanTempfrom = (json["spanTempfrom"].as<String>()!="") ? json["spanTempfrom"].as<uint16_t>() : 0;
  para_TempConfig.spanTempto = (json["spanTempto"].as<String>()!="") ? json["spanTempto"].as<uint16_t>() : 0;
  para_TempConfig.spanBuffer = (json["spanBuffer"].as<String>()!="") ? json["spanBuffer"].as<uint16_t>() : 0;  
  para_TempConfig.relay = (json["TRelay"].as<String>()!="") ? json["TRelay"].as<uint8_t>() : 0;
  //strcpy(para_TempConfig.Testchar,json["Testchar"] | "NA");

    configFile.flush();
    configFile.close();
    
  return SUCCESS;
}



