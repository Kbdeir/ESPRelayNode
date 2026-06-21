#include <RelayClass.h>
#include <RelaysArray.h>
#include <JSONConfig.h>

#ifdef ESP32

// Callbacks must match TimerCallbackFunction_t = void(*)(TimerHandle_t).
// The relay pointer is recovered via pvTimerGetTimerID.
// Actual work (which includes mqttClient.publish) is deferred to watch()
// via volatile flags so it runs on the Arduino task, not the timer daemon task.

void fnTTA_CallBack(TimerHandle_t xTimer) {
    Relay * rly = static_cast<Relay *>(pvTimerGetTimerID(xTimer));
    if (rly) {
        bool should_stop = false;
        portENTER_CRITICAL(&rly->timerFlagsMux);
        rly->running_TTA++;
        if (rly->running_TTA >= rly->RelayConfParam->v_tta) {
            rly->tta_expired = true;
            should_stop = true;
        }
        portEXIT_CRITICAL(&rly->timerFlagsMux);
        if (should_stop) rly->stop_tta_timer();
    }
}

void fnTTL_CallBack(TimerHandle_t xTimer) {
    Relay * rly = static_cast<Relay *>(pvTimerGetTimerID(xTimer));
    if (rly) {
        bool should_stop = false;
        portENTER_CRITICAL(&rly->timerFlagsMux);
        rly->running_TTL++;
        if (rly->running_TTL > 2) rly->ttl_update = true;
        if (rly->running_TTL >= rly->RelayConfParam->v_ttl) {
            rly->ttl_expired = true;
            should_stop = true;
        }
        portEXIT_CRITICAL(&rly->timerFlagsMux);
        if (should_stop) rly->stop_ttl_timer();
    }
}
#endif


void Relay::freelockfunc() {
  unsigned long cmillis = millis();
	if (cmillis - pmillis >= freeinterval) {
		pmillis = cmillis;
    lockupdate = false;
	}
  }

  void Relay::freelockreset() {
    this->pmillis = millis();
    }

// class implementation
Relay::Relay(uint8_t p,
              fnptr_a ttlcallback,
              fnptr_a ttlupdatecallback,
              fnptr_a ACSfunc,
              fnptr_a ACSfunc_mqtt,
              fnptr_a onchangeInterruptService,
              fnptr_a ttacallback
              /*
              #ifdef ESP32
                ,fnptr_a_xtimer TTL_CallBack              
                ,fnptr_a_xtimer TTA_CallBack
              #endif 
              */             
               ) {

  pin = p;
  pinMode ( pin, OUTPUT);
  RelayConfParam = new Trelayconf;

  lockupdate = false;
  pmillis = 0;
  freeinterval = 10; // was 200
  r_in_mode = 1;
  timerpaused = false;
  hastimerrunning = false;
  freelock = nullptr;

  // tickers callback functions for ttl, acs, tta
  fttlupdatecallback        = ttlupdatecallback;
  fticker_ACS712_func       = ACSfunc;
  fticker_ACS712_mqtt_func  = ACSfunc_mqtt;
  fttlcallback              = ttlcallback;
  fttacallback              = ttacallback;

  // timers
  ticker_relay_ttl = new Schedule_timer(fttlcallback,60000,0,MILLIS_,fttlupdatecallback, SECONDS_);
  ticker_relay_tta = new Schedule_timer(fttacallback,60000,0,MILLIS_);  
  ticker_ACS712 = new Schedule_timer (fticker_ACS712_func,1000,0,MILLIS_);
  ticker_ACS_MQTT = new Schedule_timer (fticker_ACS712_mqtt_func,1000,0,MILLIS_);


  #ifdef ESP32
    timerFlagsMux = portMUX_INITIALIZER_UNLOCKED;
    ttltimer = xTimerCreate("ttltimer", pdMS_TO_TICKS(1000), pdTRUE, (void*) this, fnTTL_CallBack);
    ttatimer = xTimerCreate("ttatimer", pdMS_TO_TICKS(1000), pdTRUE, (void*) this, fnTTA_CallBack);
  #endif

  fonchangeInterruptService = onchangeInterruptService;
  fgeneralinLoopFunc        = NULL;
  rchangedflag = false;
}

// class destructor
Relay::~Relay(){
      delete RelayConfParam ;
      delete ticker_relay_ttl;
      delete ticker_ACS712;
      delete ticker_ACS_MQTT;
      delete ticker_relay_tta;
    }


void Relay::watch(){

  // Process FreeRTOS timer expirations deferred from the timer daemon task.
  // Flags are set by fnTTL_CallBack / fnTTA_CallBack (timer task context);
  // callbacks are invoked here in the Arduino main loop so mqttClient.publish
  // is always called from the correct task.
  #ifdef ESP32
  bool local_ttl_update, local_ttl_expired, local_tta_expired, local_rchanged;
  portENTER_CRITICAL(&timerFlagsMux);
  local_ttl_update  = this->ttl_update;   this->ttl_update   = false;
  local_ttl_expired = this->ttl_expired;  this->ttl_expired  = false;
  local_tta_expired = this->tta_expired;  this->tta_expired  = false;
  local_rchanged    = this->rchangedflag; this->rchangedflag = false;
  portEXIT_CRITICAL(&timerFlagsMux);
  if (local_ttl_update  && fttlupdatecallback)    fttlupdatecallback(this);
  if (local_ttl_expired && fttlcallback)           fttlcallback(this);
  if (local_tta_expired && fttacallback)           fttacallback(this);
  // fonchangeInterruptService deferred here — must run on the Arduino loop task
  // because it calls mqttClient.publish() which is not AsyncTCP-task-safe.
  if (local_rchanged    && fonchangeInterruptService) fonchangeInterruptService(this);
  #endif

   if (this->ticker_relay_ttl)  this->ticker_relay_ttl->update(this);
   if (this->ticker_relay_tta)  this->ticker_relay_tta->update(this);
   if (this->ticker_ACS712)     this->ticker_ACS712->update(this);
   if (this->ticker_ACS_MQTT)   this->ticker_ACS_MQTT->update(this);

   if (this->freelock) this->freelock->update(this);
   freelockfunc();
   if (fgeneralinLoopFunc) fgeneralinLoopFunc(this);
}

String Relay::getRelayPubTopic() {
      return RelayConfParam->v_PUB_TOPIC1;
   }

void Relay::setRelayConfig(Trelayconf * RelayConf) {
      delete RelayConfParam;
      RelayConfParam = RelayConf;
   }

Trelayconf * Relay::getRelayConfig() {
      return RelayConfParam;
   }

void Relay::setRelayTag(char *Relayt)
   { strlcpy(RelayTag, Relayt, sizeof(RelayTag)); }

void Relay::setIdNumber(int id)
   { IDRelayTag = id; }

char * Relay::getName()
   { return RelayTag; }

int Relay::getIdNumber()
   { return IDRelayTag;}

void Relay::start_ttl_timer() {
  #ifdef ESP32
      xTimerStart(this->ttltimer,0);
  #else
      this->ticker_relay_ttl->start();
  #endif   
}

void Relay::stop_ttl_timer() {
  #ifdef ESP32  
    xTimerStop(this->ttltimer,0);  
    this->running_TTL = 0;
  #else  
      this->ticker_relay_ttl->stop();
  #endif    
}

void Relay::start_tta_timer() {
  #ifdef ESP32
     xTimerStart(this->ttatimer,0);
  #else
      this->ticker_relay_tta->start();
  #endif   
}

void Relay::stop_tta_timer() {
  #ifdef ESP32  
    xTimerStop(this->ttatimer,0);  
    this->running_TTA = 0;
  #else  
      this->ticker_relay_tta->stop();
  #endif    
}


void Relay::start_ACS712() {
      this->ticker_ACS712->start();
}

void Relay::stop_ACS712() {
      this->ticker_ACS712->stop();
}

void Relay::start_ACS712_mqtt() {
        this->ticker_ACS_MQTT->start();
}

void Relay::stop_ACS712_mqtt() {
        this->ticker_ACS_MQTT->stop();
}

uint32_t Relay::getRelayTTLperiodscounter() {
  #ifdef ESP32
      return this->running_TTL;
  #else
      return this->ticker_relay_ttl->periodscounter();
  #endif    
}

void Relay::setRelayTTL_Timer_Interval(uint32_t interval){
  #ifdef ESP32
    //nothing is needed here. xtimer interval is always 1000ms, the number of runs is provided by ttl value.
    //xTimerChangePeriod(this->ttltimer, pdMS_TO_TICKS(interval),0);
    //xTimerStop(this->ttltimer,0);      
  #else
      this->ticker_relay_ttl->interval(interval);
  #endif    
}

void Relay::setRelayTTA_Timer_Interval(uint32_t interval){
  #ifdef ESP32
     //nothing is needed here. xtimer interval is always 1000ms, the number of runs is provided by ttl value.
  #else
      this->ticker_relay_tta->interval(interval);
  #endif    
}

ksb_status_t Relay::TTLstate() {
  #ifdef ESP32
     if( xTimerIsTimerActive( this->ttltimer ) != pdFALSE )
     {
         return RUNNING_;
     }
     else
     {
        return STOPPED_;
     }
  #else
    	return this->ticker_relay_ttl->state();
  #endif    
}




boolean Relay::loadrelayparams(uint8_t rnb) {   //){

  char rfilename[20];
  mkRelayConfigName(rfilename, rnb);

    // const char* filename = "/config.json";

    if (!(SPIFFS.exists(rfilename))) {
         Serial.println(F("[INFO   ] Relay config file does not exist! ... building and rebooting...."));
         saveRelayDefaultConfig(rnb);
         return false;
     }

    File configFile = SPIFFS.open(rfilename, "r");
       if (!configFile) {
         saveRelayDefaultConfig(rnb);
         return false;
    }
         Serial.print("[rfilename:]" );   
         Serial.print(rfilename );     
         Serial.println(" " ); 


    size_t size = configFile.size();
    if (size > buffer_size) {
         Serial.printf("[INFO   ] %s is %u bytes, exceeds buffer_size=%d — rebuilding with this board's chip ID (%s)\n",
                       rfilename, (unsigned)size, (int)buffer_size, CID().c_str());
         configFile.close();
         saveRelayDefaultConfig(rnb);
         return false;
    }

  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.printf("[INFO   ] Failed to parse %s (%u bytes, buffer_size=%d): %s — rebuilding with this board's chip ID (%s)\n",
                  rfilename, (unsigned)size, (int)buffer_size, error.c_str(), CID().c_str());
    configFile.close();
    saveRelayDefaultConfig(rnb);
    return false;
  }

     // Use const char* to read strings from the StaticJsonDocument pool (zero heap temp).
     // String("field") = cstr assigns from pool pointer — one alloc for the persistent value.
     // Numeric fields use the | operator for null-safe defaults without any String creation.
     auto cstr = [&](const char* key, const char* def) -> const char* {
       const char* v = json[key].as<const char*>(); return (v && v[0]) ? v : def;
     };
     RelayConfParam->v_relaynb            = json["RELAYNB"]      | (uint8_t)0;
     RelayConfParam->v_PUB_TOPIC1         = cstr("PUB_TOPIC1",         "/none");
     RelayConfParam->v_TemperatureValue   = cstr("TemperatureValue",    "0");
     RelayConfParam->v_AlexaName          = cstr("AlexaName",           "0");
     RelayConfParam->v_ttl_PUB_TOPIC      = cstr("ttl_PUB_TOPIC",      "/ttlpubnone");
     RelayConfParam->v_i_ttl_PUB_TOPIC    = cstr("i_ttl_PUB_TOPIC",    "/ittlnone");
     RelayConfParam->v_ACS_AMPS           = cstr("ACS_AMPS",            "/none");
     RelayConfParam->v_CURR_TTL_PUB_TOPIC = cstr("CURR_TTL_PUB_TOPIC", "/ttlcnone");
     RelayConfParam->v_STATE_PUB_TOPIC    = cstr("STATE_PUB_TOPIC",     "/statenone");
     RelayConfParam->v_ACS_Sensor_Model   = cstr("ACS_Sensor_Model",    "10");
     RelayConfParam->v_ttl                = json["ttl"]          | (uint32_t)0;
     RelayConfParam->v_tta                = json["tta"]          | (uint32_t)0;
     RelayConfParam->v_Max_Current        = json["Max_Current"]  | (uint8_t)10;
     RelayConfParam->v_LWILL_TOPIC        = cstr("LWILL_TOPIC",         "/lwtnone");
     RelayConfParam->v_SUB_TOPIC1         = cstr("SUB_TOPIC1",          "/inone");
     RelayConfParam->v_ACS_Active         = (json["ACS_Active"] | 0) == 1;
     RelayConfParam->v_IN0_INPUTMODE      = MyConfParam.v_IN0_INPUTMODE;
     RelayConfParam->v_IN1_INPUTMODE      = MyConfParam.v_IN1_INPUTMODE;
     RelayConfParam->v_IN2_INPUTMODE      = MyConfParam.v_IN2_INPUTMODE;
     RelayConfParam->v_ACS_elasticity     = json["ACS_elasticity"] | (uint16_t)0;
    
     configFile.close();
     return true;
}


int Relay::readrelay (){
      return digitalRead(this->pin);
}

bool Relay::readPersistedrelay (){
  char rfilename[32];
  snprintf(rfilename, sizeof(rfilename), "/relayPersist%u.json",
           (unsigned)this->RelayConfParam->v_relaynb);

  File configFile = SPIFFS.open(rfilename, "r");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to open relayPersist file"));
    return false;
  }
  StaticJsonDocument<100> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("[INFO   ] Failed to read relayPersist file"));
    configFile.close();
    return false;
  }
  bool temp = (json["status"].as<uint8_t>() == 1);

  configFile.close();
  mdigitalWrite(this->getRelayPin(), temp);
  return temp;
}

bool Relay::savePersistedrelay(){
  // Throttle: skip write if value hasn't changed and less than 30 s have elapsed.
  uint8_t currentVal = (digitalRead(this->pin) == HIGH) ? 1 : 0;
  if (currentVal == persistLastVal)
    return true;
  persistLastVal = currentVal;

  StaticJsonDocument<100> json;

  char rfilename[32];
  snprintf(rfilename, sizeof(rfilename), "/relayPersist%u.json",
           (unsigned)this->RelayConfParam->v_relaynb);

  File configFile = SPIFFS.open(rfilename, "w");
    if (!configFile) {
      Serial.println(F("[INFO   ] Failed to open persist file for writing"));
      return FAILURE;
    }

    json[F("status")] = (digitalRead(this->pin)) == HIGH ? 1 : 0;
    Serial.println(F("\n[INFO   ] Serializing status"));

    if (serializeJsonPretty(json, configFile) == 0) {
      Serial.println(F("[INFO   ] Failed to write persist file"));
      configFile.close();
      return false;
    }
  configFile.print("\n");
  configFile.flush();
  configFile.close();

  return true;
}

void Relay::attachLoopfunc(fnptr_a GeneralLoopFunc){
      fgeneralinLoopFunc = GeneralLoopFunc;
}

uint8_t Relay::getRelayPin(){
      return this->pin;
}

void Relay::mdigitalWrite(uint8_t pn, uint8_t v)  {
    // M6: make the test-and-set of lockupdate atomic on dual-core ESP32.
    #ifdef ESP32
    bool proceed = false;
    portENTER_CRITICAL(&timerFlagsMux);
    if (!lockupdate) { lockupdate = true; proceed = true; }
    portEXIT_CRITICAL(&timerFlagsMux);
    if (!proceed) return;
    this->freelockreset();
    {
    #else
    if (!lockupdate){
      this->freelockreset();
      lockupdate = true;
    #endif
      // uint8_t sts = digitalRead(pn);
      // rchangedflag = (sts != v); // ksb0 this has been removed to correct the situation where the actual status of the io pin is ot in sync with the mqqt status
      // rchangedflag = true;
      // if (rchangedflag){

        digitalWrite(pn,v);

        if (this->hastimerrunning) {   
          this->timerpaused = (v==LOW); 
        }
        if (fonchangeInterruptService) {
          #ifdef ESP32
          // Defer to watch() on the Arduino loop task — mqttClient.publish()
          // is not safe to call from the AsyncTCP or timer-daemon tasks that
          // can also call mdigitalWrite().
          portENTER_CRITICAL(&timerFlagsMux);
          rchangedflag = true;
          portEXIT_CRITICAL(&timerFlagsMux);
          #else
          fonchangeInterruptService(this);
          #endif
        }
      // }
    }
}


Relay * getrelaybypin(uint8_t pn){
      Relay * rly = nullptr;
      Relay * rtemp = nullptr;
      for (std::vector<void *>::iterator it = relays.begin(); it != relays.end(); ++it)  {
        rtemp = static_cast<Relay *>(*it);
        if (rtemp && pn == rtemp->getRelayPin()) {
          rly = rtemp;
        }
      }
        /*for (int i=0; i<MAX_RELAYS; i++){
          rtemp = static_cast<Relay *>(mrelays[i]);
          if (rtemp->getRelayPin() == pn) rly = rtemp;
        }*/
    return rly;
}


Relay * getrelaybynumber(uint8_t nb){
  if (nb < relays.size()) {
    Relay * rly = static_cast<Relay *>(relays.at(nb));
    if (rly) {
      return rly;
    } else {
       return nullptr;
    }
  } else {
     return nullptr;
  }
}


void mkRelayConfigName(char name[], uint8_t rnb){
  snprintf(name, 16, "/relay%u.json", (unsigned)rnb);
}
