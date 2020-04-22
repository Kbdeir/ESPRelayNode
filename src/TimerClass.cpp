
#include <TimerClass.h>
#define buffer_size  1500


NodeTimer::NodeTimer(uint8_t para_id) {
    id = para_id;
    TM_type = TM_FULL_SPAN;
    spanDatefrom  = "01-01-1970";
    spanDateto    = "01-01-2100";
    spantimefrom  = "00:00";
    spantimeto    = "00:00";

    weekdays = new TWeekdays;
    weekdays->clear();
    enabled = true;
    Mark_Hours = 0;
    Mark_Minutes = 0;
    // Testchar = new char[22]; //"Hello there I am char";
}

NodeTimer::NodeTimer(uint8_t para_id,
          String para_spanDatefrom,
          String para_spanDateto,
          String para_spantimefrom,
          String para_spantimeto,
          TWeekdays * para_weekdays,
          boolean para_enabled,
          uint8_t para_relay
) {
    id = para_id;
    TM_type = TM_FULL_SPAN;
    spanDatefrom  = para_spanDatefrom;
    spanDateto    = para_spanDateto;
    weekdays      = para_weekdays;
    enabled       = para_enabled;
    spantimefrom  = para_spantimefrom;
    spantimeto    = para_spantimeto;
    Mark_Hours    = 0;
    Mark_Minutes  = 0;
    //Testchar = new char[22]; //"Hello there I am char";
    relay =  para_relay;
}

NodeTimer::~NodeTimer(){
    delete weekdays;
    // delete[] Testchar;
}

void NodeTimer::watch(){
}

NodeTimer * gettimerbypin(uint8_t pn){
    NodeTimer * tmr = nullptr;
    return tmr;
}

bool saveNodeTimer(AsyncWebServerRequest *request){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

  int args = request->args();
  for(int i=0;i<args;i++){
    //Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    json[request->argName(i)] =  request->arg(i) ;
  }

  request->hasParam("CSunday")    ? json["Su"]    =  "1"   : json["Su"]   =  "0" ;
  request->hasParam("CMonday")    ? json["Mo"]    =  "1"   : json["Mo"]   =  "0" ;
  request->hasParam("CTuesday")   ? json["Tu"]    =  "1"   : json["Tu"]   =  "0" ;
  request->hasParam("CWednesday") ? json["We"]    =  "1"   : json["We"]   =  "0" ;
  request->hasParam("CThursday")  ? json["Th"]    =  "1"   : json["Th"]   =  "0" ;
  request->hasParam("CFriday")    ? json["Fr"]    =  "1"   : json["Fr"]   =  "0" ;
  request->hasParam("CSaturday")  ? json["Sa"]    =  "1"   : json["Sa"]   =  "0" ;
  request->hasParam("CEnabled")   ? json["CEnabled"]   =  "1"   : json["CEnabled"]   = "0" ;

  char  timerfilename[20] = "/timer";
  strcat(timerfilename, json["TNumber"]);
  strcat(timerfilename, ".json");

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to open timer file for writing"));
    return false;
  }

  json.printTo(configFile);
  configFile.close();


  return true;
}



config_read_error_t loadNodeTimer(char* filename, NodeTimer &para_NodeTimer) {

  if (! SPIFFS.exists(filename)) {
    Serial.println(F("\n[TIMERS] timer file does not exist!"));
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[TIMERS] Failed to open timer file"));
    return ERROR_OPENING_FILE;
  }

  size_t size = configFile.size();
  if (size > buffer_size) {
    Serial.println(F("\n[TIMERS] timer file size is too large, rebuilding."));
    return ERROR_OPENING_FILE;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<buffer_size> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println(F("Failed to parse timer file"));
    //saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  para_NodeTimer.id = (json["TNumber"].as<String>()!="") ? json["TNumber"].as<uint8_t>() : 0;
  para_NodeTimer.enabled = json["CEnabled"]; //.as<String>()!="") ? json["CEnabled"].as<uint8_t>() : 0;
  para_NodeTimer.spanDatefrom = (json["Dfrom"].as<String>()!="") ? json["Dfrom"].as<String>() : String("01-01-1970");
  para_NodeTimer.spanDateto  = (json["DTo"].as<String>()!="")  ? json["DTo"].as<String>() : String("01-01-2100");
  para_NodeTimer.spantimefrom = (json["TFrom"].as<String>()!="") ? json["TFrom"].as<String>() : String("00:00");
  para_NodeTimer.spantimeto  = (json["TTo"].as<String>()!="")  ? json["TTo"].as<String>() : String("00:00");
  para_NodeTimer.weekdays->Sunday     = json["Su"];
  para_NodeTimer.weekdays->Monday     = json["Mo"];
  para_NodeTimer.weekdays->Tuesday    = json["Tu"];
  para_NodeTimer.weekdays->Wednesday  = json["We"];
  para_NodeTimer.weekdays->Thursday   = json["Th"];
  para_NodeTimer.weekdays->Friday     = json["Fr"];
  para_NodeTimer.weekdays->Saturday   = json["Sa"];
  para_NodeTimer.Mark_Hours = (json["Mark_Hours"].as<String>()!="") ? json["Mark_Hours"].as<uint16_t>() : 0;
  para_NodeTimer.Mark_Minutes = (json["Mark_Minutes"].as<String>()!="") ? json["Mark_Minutes"].as<uint16_t>() : 0;
  para_NodeTimer.TM_type = static_cast<TimerType>(json["TMTYPEedit"].as<uint8_t>()); //default is full span
  para_NodeTimer.secondsspan = 0;
  para_NodeTimer.relay = (json["TRelay"].as<String>()!="") ? json["TRelay"].as<uint8_t>() : 0;
  //strcpy(para_NodeTimer.Testchar,json["Testchar"] | "NA");
  return SUCCESS;
}
