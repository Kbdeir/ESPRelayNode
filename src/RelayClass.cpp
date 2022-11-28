#include <RelayClass.h>
#include <RelaysArray.h>
#include <JSONConfig.h>

#ifdef ESP32

void fnTTA_CallBack(TimerHandle_t xTimer, void* obj) { // called when the TTA is over
// to be implemented
    Relay * rly = static_cast<Relay *>(pvTimerGetTimerID( xTimer ));
    if (rly) {
      rly->running_TTA++;   
      if (rly->running_TTA == rly->RelayConfParam->v_tta){
        rly->fttacallback(rly);
        rly->stop_tta_timer();
      }
    }
}

void fnTTL_CallBack(TimerHandle_t xTimer, void* obj) { // called when the TTL is runing and over
    Relay * rly = static_cast<Relay *>(pvTimerGetTimerID( xTimer ));
    if (rly) {
      rly->running_TTL++;       
      //Serial.printf ("\n fnTTL_Callback called %u", rly->running_TTL) ; 
      if (rly->running_TTL > 2) rly->fttlupdatecallback(rly);
      if (rly->running_TTL == rly->RelayConfParam->v_ttl){
          // Serial.println (F("TTL last run")) ; 
          rly->fttlcallback(rly);
          rly->stop_ttl_timer();
      } 
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
  freeinterval = 10; // was 200
  r_in_mode = 1;
  timerpaused = false;
  hastimerrunning = false;

  // tickers callback functions for ttl, acs, tta
  fttlupdatecallback        = ttlupdatecallback;
  fticker_ACS712_func       = ACSfunc;
  fticker_ACS712_mqtt_func  = ACSfunc_mqtt;
  fttlcallback              = ttlcallback;
  fttacallback              = ttacallback;

  // timers
  ticker_relay_ttl = new Schedule_timer(fttlcallback,0,0,MILLIS_,fttlupdatecallback, SECONDS_);
  ticker_relay_tta = new Schedule_timer(fttacallback,0,0,MILLIS_);  
  ticker_ACS712 = new Schedule_timer (fticker_ACS712_func,1000,0,MILLIS_);
  ticker_ACS_MQTT = new Schedule_timer (fticker_ACS712_mqtt_func,1000,0,MILLIS_);


  #ifdef ESP32
    ttltimer = xTimerCreate("ttltimer", pdMS_TO_TICKS(1000), pdTRUE, (void*)  this, reinterpret_cast<TimerCallbackFunction_t>(fnTTL_CallBack));
    ttatimer = xTimerCreate("ttatimer", pdMS_TO_TICKS(1000), pdTRUE, (void*)  this, reinterpret_cast<TimerCallbackFunction_t>(fnTTA_CallBack));      
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

  //#ifndef ESP32
   if (this->ticker_relay_ttl)  this->ticker_relay_ttl->update(this);
   if (this->ticker_relay_tta)  this->ticker_relay_tta->update(this);
  //#endif   
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
      RelayConfParam = RelayConf;
   }

Trelayconf * Relay::getRelayConfig() {
      return RelayConfParam;
   }

void Relay::setRelayTag(char *Relayt)
   { strcpy (RelayTag, Relayt); }

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

    if(SPIFFS.begin()) { Serial.print(F("[INFO   ] SPIFFS Initialize....ok")); }
      else {Serial.println(F("[INFO   ] SPIFFS Initialization...failed")); }

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
         Serial.println(F("[INFO   ] Relay Config file size is too large, rebuilding."));
         saveRelayDefaultConfig(rnb);
         return false;
    }

  StaticJsonDocument<buffer_size> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));  
  }

     RelayConfParam->v_relaynb            = (json["RELAYNB"].as<String>()!="") ? json["RELAYNB"].as<uint8_t>() : 0;
     RelayConfParam->v_PUB_TOPIC1         = (json["PUB_TOPIC1"].as<String>()!="") ? json["PUB_TOPIC1"].as<String>() : String("/none");
     RelayConfParam->v_TemperatureValue   = (json["TemperatureValue"].as<String>()!="") ? json["TemperatureValue"].as<String>() : String("0");
     RelayConfParam->v_AlexaName          = (json["AlexaName"].as<String>()!="") ? json["AlexaName"].as<String>() : String("0");
     RelayConfParam->v_ttl_PUB_TOPIC      = (json["ttl_PUB_TOPIC"].as<String>()!="") ? json["ttl_PUB_TOPIC"].as<String>() : String("/ttlpubnone");
     RelayConfParam->v_i_ttl_PUB_TOPIC    = (json["i_ttl_PUB_TOPIC"].as<String>()!="") ? json["i_ttl_PUB_TOPIC"].as<String>() : String("/ittlnone");
     RelayConfParam->v_ACS_AMPS           = (json["ACS_AMPS"].as<String>()!="") ? json["ACS_AMPS"].as<String>() : String("/none");
     RelayConfParam->v_CURR_TTL_PUB_TOPIC = (json["CURR_TTL_PUB_TOPIC"].as<String>()!="") ? json["CURR_TTL_PUB_TOPIC"].as<String>() : String("/ttlcnone");
     RelayConfParam->v_STATE_PUB_TOPIC    = (json["STATE_PUB_TOPIC"].as<String>()!="") ? json["STATE_PUB_TOPIC"].as<String>() : String("/statenone");
     RelayConfParam->v_ACS_Sensor_Model   = (json["ACS_Sensor_Model"].as<String>()!="") ? json["ACS_Sensor_Model"].as<String>() : String("10");
     RelayConfParam->v_ttl                = (json["ttl"].as<String>()!="") ? json["ttl"].as<uint32_t>() : 0;
     RelayConfParam->v_tta                = (json["tta"].as<String>()!="") ? json["tta"].as<uint32_t>() : 0;
     RelayConfParam->v_Max_Current        = (json["Max_Current"].as<String>()!="") ? json["Max_Current"].as<uint8_t>() : 10;
     RelayConfParam->v_LWILL_TOPIC        = (json["LWILL_TOPIC"].as<String>()!="") ? json["LWILL_TOPIC"].as<String>() : String("/lwtnone");
     RelayConfParam->v_SUB_TOPIC1         = (json["SUB_TOPIC1"].as<String>()!="") ? json["SUB_TOPIC1"].as<String>() : String("/inone");
     RelayConfParam->v_ACS_Active         = (json["ACS_Active"].as<String>()!="") ? json["ACS_Active"].as<uint8_t>() == 1 : false;
     RelayConfParam->v_IN0_INPUTMODE       =  MyConfParam.v_IN0_INPUTMODE; //json["I0MODE"].as<uint8_t>();
     RelayConfParam->v_IN1_INPUTMODE       =  MyConfParam.v_IN1_INPUTMODE; //json["I1MODE"].as<uint8_t>();
     RelayConfParam->v_IN2_INPUTMODE       =  MyConfParam.v_IN2_INPUTMODE; //json["I2MODE"].as<uint8_t>();
     RelayConfParam->v_ACS_elasticity      = (json["ACS_elasticity"].as<String>()!="") ? json["ACS_elasticity"].as<uint16_t>() : 0;
    
     configFile.close();
     return true;
}


int Relay::readrelay (){
      return digitalRead(this->pin);
}

bool Relay::readPersistedrelay (){
  char rfilename[20];
  strcpy(rfilename,"/relayPersist");
  strcat(rfilename, String(this->RelayConfParam->v_relaynb).c_str());
  strcat(rfilename,".json");  

  File configFile = SPIFFS.open(rfilename, "r");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to open relayPersist file"));
    return ERROR_OPENING_FILE;
  }
  StaticJsonDocument<100> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("[INFO   ] Failed to read file, using default configuration"));
    saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;    
  }
  bool temp = false;
  temp =  (json["status"].as<uint8_t>() == 1);

  mdigitalWrite(this->getRelayPin(),temp);
  configFile.close();
  return temp;
}

bool Relay::savePersistedrelay(){
  StaticJsonDocument<100> json;
  
  char rfilename[20];
  strcpy(rfilename,"/relayPersist");
  strcat(rfilename, String(this->RelayConfParam->v_relaynb).c_str());
  strcat(rfilename,".json");

  File configFile = SPIFFS.open(rfilename, "w");
    if (!configFile) {
      Serial.println(F("[INFO   ] Failed to open persist file for writing"));
      return FAILURE;
    }

    json[F("status")] = (digitalRead(this->pin)) == HIGH ? "1" : "0";
    Serial.println(F("\n[INFO   ] Serializing status"));

    if (serializeJsonPretty(json, configFile) == 0) {
      Serial.println(F("[INFO   ] Failed to write persist file"));
      configFile.close();
      return false;
    }
  configFile.println("\n");
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
    this->freelockreset();
    if (!lockupdate){
      lockupdate = true;
      // uint8_t sts = digitalRead(pn);
      // rchangedflag = (sts != v); // ksb0 this has been removed to correct the situation where the actual status of the io pin is ot in sync with the mqqt status
      // rchangedflag = true;
      // if (rchangedflag){

        digitalWrite(pn,v);

        if (this->hastimerrunning) {   
          this->timerpaused = (v==LOW); 
        }
        if (fonchangeInterruptService)  {
          fonchangeInterruptService(this);
          // rchangedflag = false; 
        }
      // }
    }
}


Relay * getrelaybypin(uint8_t pn){
      Relay * rly = nullptr;
      Relay * rtemp = nullptr;
      for (std::vector<void *>::iterator it = relays.begin(); it != relays.end(); ++it)  {
        rtemp = static_cast<Relay *>(*it);
        if (pn == rtemp->getRelayPin()) {
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
  strcpy(name,"/relay");
  strcat(name, String(rnb).c_str());
  strcat(name,".json");
}
