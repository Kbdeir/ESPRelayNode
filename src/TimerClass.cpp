#include <TimerClass.h>
#define buffer_size  1500


NodeTimer::NodeTimer(uint8_t para_id,
                      unsigned int para_mark,
                      uint8_t para_marktype) {
    id = para_id;
    TM_type = TM_FULL_SPAN;
    spanDatefrom  = "01-01-1970";
    spanDateto    = "01-01-2100";
    spantimefrom  = "00:00";
    spantimeto    = "00:00";

    weekdays = new TWeekdays;
    weekdays->Monday    = true;
    weekdays->Tuesday   = true;
    weekdays->Wednesday = true;
    weekdays->Thursday  = true;
    weekdays->Friday    = true;
    weekdays->Saturday  = true;
    weekdays->Sunday    = true;

    mark = para_mark;
    marktype = para_marktype;
    enabled = true;
    Mark_Hours = 0;
    Mark_Minutes = 0;
    Testchar = new char[22]; //"Hello there I am char";
}

NodeTimer::NodeTimer(uint8_t para_id,
          String para_spanDatefrom,
          String para_spanDateto,
          String para_spantimefrom,
          String para_spantimeto,
          TWeekdays * para_weekdays,
          unsigned int para_mark,
          uint8_t para_marktype,
          boolean para_enabled
) {
    id = para_id;
    TM_type = TM_FULL_SPAN;
    spanDatefrom  = para_spanDatefrom;
    spanDateto    = para_spanDateto;
    weekdays      = para_weekdays;
    mark          = para_mark;
    marktype      = para_marktype;
    enabled       = para_enabled;
    spantimefrom  = para_spantimefrom;
    spantimeto    = para_spantimeto;
    Mark_Hours    = 0;
    Mark_Minutes  = 0;
    Testchar = new char[22]; //"Hello there I am char";

}

NodeTimer::~NodeTimer(){
    delete weekdays;
    delete Testchar;
}

void NodeTimer::watch(){
}

NodeTimer * gettimerbypin(uint8_t pn){
    NodeTimer * tmr = NULL;
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

  request->hasParam("CSunday")    ? json["CSunday"]    =  "1"   : json["CSunday"]   =  "0" ;
  request->hasParam("CMonday")    ? json["CMonday"]    =  "1"   : json["CMonday"]   =  "0" ;
  request->hasParam("CTuesday")   ? json["CTuesday"]   =  "1"   : json["CTuesday"]  =  "0" ;
  request->hasParam("CWednesday") ? json["CWednesday"] =  "1"   : json["CWednesday"]=  "0" ;
  request->hasParam("CThursday")  ? json["CThursday"]  =  "1"   : json["CThursday"] =  "0" ;
  request->hasParam("CFriday")    ? json["CFriday"]    =  "1"   : json["CFriday"]   =  "0" ;
  request->hasParam("CSaturday")  ? json["CSaturday"]  =  "1"   : json["CSaturday"] =  "0" ;
  request->hasParam("CEnabled")   ? json["CEnabled"]   =  "1"   : json["CEnabled"]   = "0" ;

  char  timerfilename[30] = "/timer";
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
    Serial.println(F("timer file does not exist!"));
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("Failed to open timer file"));
    return ERROR_OPENING_FILE;
  }

  size_t size = configFile.size();
  if (size > buffer_size) {
    Serial.println(F("timer file size is too large, rebuilding."));
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
  para_NodeTimer.weekdays->Sunday = json["CSunday"];
  para_NodeTimer.weekdays->Monday = json["CMonday"];
  para_NodeTimer.weekdays->Tuesday = json["CTuesday"];
  para_NodeTimer.weekdays->Wednesday = json["CWednesday"];
  para_NodeTimer.weekdays->Thursday = json["CThursday"];
  para_NodeTimer.weekdays->Friday = json["CFriday"];
  para_NodeTimer.weekdays->Saturday = json["CSaturday"];
  para_NodeTimer.fyear=2018;
  para_NodeTimer.mark = 0;
  para_NodeTimer.marktype = 0;
  para_NodeTimer.Mark_Hours = (json["Mark_Hours"].as<String>()!="") ? json["Mark_Hours"].as<uint16_t>() : 0;
  para_NodeTimer.Mark_Minutes = (json["Mark_Minutes"].as<String>()!="") ? json["Mark_Minutes"].as<uint16_t>() : 0;
  para_NodeTimer.TM_type = static_cast<TimerType>(json["TMTYPEedit"].as<uint8_t>()); //default is full span
  para_NodeTimer.secondsspan = 0;

  strcpy(para_NodeTimer.Testchar, json["Testchar"] | "Hello");

  return SUCCESS;
}
