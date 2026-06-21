
#include <TimerClass.h>
#include <Chronos.h>
 #define buffer_size  1024

#ifdef ESP32
#include <sys/stat.h>
#endif


NodeTimer::NodeTimer(uint8_t para_id) {
    id = para_id;
    TM_type = TM_FULL_SPAN;
    spanDatefrom  = "01-01-1970";
    spanDateto    = "01-01-2100";
    spantimefrom  = "00:00";
    spantimeto    = "00:00";

    weekdays = new TWeekdays;
    if (weekdays) weekdays->clear();
    enabled = true;
    relay = 0;
    Mark_Hours = 0;
    Mark_Minutes = 0;
    monthDay = 0;
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
    monthDay      = 0;
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

bool isNodeTimerActiveNow(uint8_t timerId) {
  if (timerId < 1 || timerId > MAX_NUMBER_OF_TIMERS) return false;

  // Evaluate from the in-memory cache — no LittleFS access on the hot path.
  // Cache is populated at boot (refreshAllTimerCache) and after every save.
  const TimerScheduleCache& c = g_timerScheduleCache[timerId - 1];
  if (!c.loaded || !c.enabled) return false;

  Chronos::DateTime nowDt(Chronos::DateTime::now());

  // 1. Date-range check — skipped for weekly-recurring timers
  if (c.TM_type != TimerType::TM_WEEKDAY_SPAN) {
    int fy, fm, fd, fh = 0, fmin = 0;
    int ty, tm, td, th = 0, tmin = 0;
    char DF[20], DT[20];
    snprintf(DF, sizeof(DF), "%s %s", c.spanDatefrom, c.spantimefrom);
    snprintf(DT, sizeof(DT), "%s %s", c.spanDateto,   c.spantimeto);
    sscanf(DF, "%d-%d-%d %d:%d", &fy, &fm, &fd, &fh, &fmin);
    sscanf(DT, "%d-%d-%d %d:%d", &ty, &tm, &td, &th, &tmin);

    Chronos::DateTime fromDt(fy, fm, fd, 0, 0, 0);
    Chronos::DateTime toDt(ty, tm, td, 23, 59, 59);
    if (nowDt < fromDt || nowDt > toDt) return false;
  }

  // 2. Weekday check — only meaningful for weekly-recurring timers
  if (c.TM_type == TimerType::TM_WEEKDAY_SPAN) {
    Chronos::WeekDay wd = nowDt.weekday();
    bool dayMatches =
      (wd == Chronos::Weekday::Sunday    && c.weekdays.Sunday)    ||
      (wd == Chronos::Weekday::Monday    && c.weekdays.Monday)    ||
      (wd == Chronos::Weekday::Tuesday   && c.weekdays.Tuesday)   ||
      (wd == Chronos::Weekday::Wednesday && c.weekdays.Wednesday) ||
      (wd == Chronos::Weekday::Thursday  && c.weekdays.Thursday)  ||
      (wd == Chronos::Weekday::Friday    && c.weekdays.Friday)    ||
      (wd == Chronos::Weekday::Saturday  && c.weekdays.Saturday);
    if (!dayMatches) return false;
  }

  // 3. Time-of-day window check, handling overnight wraparound (e.g. 22:00-06:00)
  int fromH = 0, fromM = 0;
  sscanf(c.spantimefrom, "%d:%d", &fromH, &fromM);
  int fromMinutes = fromH * 60 + fromM;

  int toMinutes;
  if (c.Mark_Hours + c.Mark_Minutes > 0) {
    toMinutes = fromMinutes + c.Mark_Hours * 60 + c.Mark_Minutes;
  } else {
    int toH = 0, toM = 0;
    sscanf(c.spantimeto, "%d:%d", &toH, &toM);
    toMinutes = toH * 60 + toM;
  }

  int curMinutes = nowDt.hour() * 60 + nowDt.minute();

  if (toMinutes >= 1440) {
    toMinutes -= 1440;
    return (curMinutes >= fromMinutes) || (curMinutes <= toMinutes);
  }
  if (fromMinutes <= toMinutes) {
    return (curMinutes >= fromMinutes) && (curMinutes <= toMinutes);
  }
  return (curMinutes >= fromMinutes) || (curMinutes <= toMinutes);
}

TimerScheduleCache g_timerScheduleCache[MAX_NUMBER_OF_TIMERS] = {};

void refreshTimerCache(int timerIdx) {
  if (timerIdx < 1 || timerIdx > MAX_NUMBER_OF_TIMERS) return;
  TimerScheduleCache& c = g_timerScheduleCache[timerIdx - 1];
  NodeTimer t(timerIdx);
  char fname[20];
  snprintf(fname, sizeof(fname), "/timer%d.json", timerIdx);
  if (loadNodeTimer(fname, t) != SUCCESS) { c.loaded = false; return; }
  c.loaded      = true;
  c.enabled     = t.enabled;
  c.relay       = t.relay;
  c.id          = t.id;
  c.TM_type     = t.TM_type;
  c.Mark_Hours  = t.Mark_Hours;
  c.Mark_Minutes= t.Mark_Minutes;
  c.monthDay    = t.monthDay;
  strncpy(c.spanDatefrom,  t.spanDatefrom.c_str(),  sizeof(c.spanDatefrom)  - 1); c.spanDatefrom[sizeof(c.spanDatefrom)-1]  = '\0';
  strncpy(c.spanDateto,    t.spanDateto.c_str(),    sizeof(c.spanDateto)    - 1); c.spanDateto[sizeof(c.spanDateto)-1]      = '\0';
  strncpy(c.spantimefrom,  t.spantimefrom.c_str(),  sizeof(c.spantimefrom)  - 1); c.spantimefrom[sizeof(c.spantimefrom)-1]  = '\0';
  strncpy(c.spantimeto,    t.spantimeto.c_str(),    sizeof(c.spantimeto)    - 1); c.spantimeto[sizeof(c.spantimeto)-1]      = '\0';
  if (t.weekdays) c.weekdays = *t.weekdays; else memset(&c.weekdays, 0, sizeof(c.weekdays));
}

void refreshAllTimerCache() {
  for (int i = 1; i <= MAX_NUMBER_OF_TIMERS; i++) refreshTimerCache(i);
}

bool saveNodeTimer(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size> json;
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

  const char* tnum = json["TNumber"] | "0";
  char timerfilename[20];
  snprintf(timerfilename, sizeof(timerfilename), "/timer%s.json", tnum);

  File configFile = SPIFFS.open(timerfilename, "w");
  if (!configFile) {
    Serial.println(F("[TIMERS ] Failed to open timer file for writing"));
    return false;
  }


  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[TIMERS ] Failed to write to file"));
  }
  configFile.print("\n");
  /*<64> dummy;
  dummy["end"]="0";
  if (serializeJsonPretty(dummy, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }*/

  configFile.flush();
  configFile.close();

    #ifndef ESP32  
    ESP.wdtFeed(); 
    #endif

  return true;
}



config_read_error_t loadNodeTimer(char* filename, NodeTimer &para_NodeTimer) {

  // Existence check via POSIX stat() bypasses arduino-esp32's
  // SPIFFS/LittleFS exists()->open() path that emits a log_e() error
  // for every missing file.  log_e() is compile-time-gated by
  // ARDUHAL_LOG_LEVEL, so esp_log_level_set() at runtime cannot
  // suppress it — the only way to silence it is to not trip the
  // failed open() in the first place.  An absent timer slot is a
  // normal boot condition (no schedule configured for that index).
#ifdef ESP32
  char fullpath[96];
  snprintf(fullpath, sizeof(fullpath), "/littlefs%s", filename);
  struct stat st;
  const bool present = (stat(fullpath, &st) == 0);
#else
  const bool present = SPIFFS.exists(filename);
#endif

  if (!present) {
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("[TIMERS ] * Failed to open timer file *"));
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<buffer_size> json;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.print(F("[TIMERS ] * Failed to read file, using default timer configuration *"));
    Serial.println(filename);
    configFile.close();
    return JSONCONFIG_CORRUPTED;
  }


  para_NodeTimer.id = (json[F("TNumber")].as<String>()!="") ? json[F("TNumber")].as<uint8_t>() : 0;
  para_NodeTimer.enabled = json[F("CEnabled")].as<uint8_t>() ; //(json["CEnabled"] == "1"); //.as<String>()!="") ? json["CEnabled"].as<uint8_t>() : 0;
  para_NodeTimer.spanDatefrom = (json[F("Dfrom")].as<String>()!="") ? json[F("Dfrom")].as<String>() : String(F("01-01-1970"));
  para_NodeTimer.spanDateto  = (json[F("DTo")].as<String>()!="")  ? json[F("DTo")].as<String>() : String(F("01-01-2100"));
  para_NodeTimer.spantimefrom = (json[F("TFrom")].as<String>()!="") ? json[F("TFrom")].as<String>() : String(F("00:00"));
  para_NodeTimer.spantimeto  = (json[F("TTo")].as<String>()!="")  ? json[F("TTo")].as<String>() : String(F("00:00"));
  if (para_NodeTimer.weekdays) {
    para_NodeTimer.weekdays->Sunday    = (json["Su"]== "1");
    para_NodeTimer.weekdays->Monday    = (json["Mo"]== "1");
    para_NodeTimer.weekdays->Tuesday   = (json["Tu"]== "1");
    para_NodeTimer.weekdays->Wednesday = (json["We"]== "1");
    para_NodeTimer.weekdays->Thursday  = (json["Th"]== "1");
    para_NodeTimer.weekdays->Friday    = (json["Fr"]== "1");
    para_NodeTimer.weekdays->Saturday  = (json["Sa"]== "1");
  }
  para_NodeTimer.Mark_Hours = (json[F("Mark_Hours")].as<String>()!="") ? json[F("Mark_Hours")].as<uint16_t>() : 0;
  para_NodeTimer.Mark_Minutes = (json[F("Mark_Minutes")].as<String>()!="") ? json[F("Mark_Minutes")].as<uint16_t>() : 0;
  para_NodeTimer.monthDay = json[F("MonthDay")].as<uint8_t>();
  para_NodeTimer.TM_type = static_cast<TimerType>(json["TMTYPEedit"].as<uint8_t>()); //default is full span
  para_NodeTimer.secondsspan = 0;
  para_NodeTimer.relay = (json[F("TRelay")].as<String>()!="") ? json[F("TRelay")].as<uint8_t>() : 0;
  //strcpy(para_NodeTimer.Testchar,json["Testchar"] | "NA");

  configFile.close();
  return SUCCESS;
}
