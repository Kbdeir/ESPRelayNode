#include <RelayClass.h>
#include <RelaysArray.h>


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

Relay::Relay(uint8_t p,
              fnptr_a ttlcallback,
              fnptr_a ttlupdatecallback,
              fnptr_a ACSfunc,
              fnptr_a ACSfunc_mqtt,
              fnptr_a onchangeInterruptService,
              fnptr_a ttacallback ) {

  pin = p;
  pinMode ( pin, OUTPUT);
  RelayConfParam = new Trelayconf;

  lockupdate = false;
  freeinterval = 100; // was 200
  r_in_mode = 1;
  timerpaused = false;
  hastimerrunning = false;

  // tickers callback functions for ttl, acs, tta
  fttlcallback = ttlcallback;
  fttlupdatecallback = ttlupdatecallback;
  fticker_ACS712_func = ACSfunc;
  fticker_ACS712_mqtt_func = ACSfunc_mqtt;
  fttacallback = ttacallback;
  fonchangeInterruptService = onchangeInterruptService;
  fgeneralinLoopFunc = NULL;

  ticker_relay_ttl = new Schedule_timer(fttlcallback,0,0,MILLIS_,fttlupdatecallback, SECONDS_);
  ticker_ACS712 = new Schedule_timer (fticker_ACS712_func,1000,0,MILLIS_);
  ticker_ACS_MQTT = new Schedule_timer (fticker_ACS712_mqtt_func,1000,0,MILLIS_);
  ticker_relay_tta = new Schedule_timer(fttacallback,0,0,MILLIS_);
  rchangedflag = false;
}

Relay::~Relay(){
      delete RelayConfParam ;
      delete ticker_relay_ttl;
      delete ticker_ACS712;
      delete ticker_ACS_MQTT;
      delete ticker_relay_tta;
    }


void Relay::watch(){
   if (this->ticker_relay_ttl) this->ticker_relay_ttl->update(this);
   if (this->ticker_ACS712) this->ticker_ACS712->update(this);
   if (this->ticker_ACS_MQTT) this->ticker_ACS_MQTT->update(this);
   if (this->ticker_relay_tta) this->ticker_relay_tta->update(this);
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


boolean Relay::loadrelayparams(uint8_t rnb) {   //){

  char rfilename[20];
  mkRelayConfigName(rfilename, rnb);

    if(SPIFFS.begin()) { Serial.println(F("SPIFFS Initialize....ok")); }
      else {Serial.println(F("SPIFFS Initialization...failed")); }

    // const char* filename = "/config.json";

    if (!(SPIFFS.exists(rfilename))) {
         Serial.println(F("Relay config file does not exist! ... building and rebooting...."));
         saveRelayDefaultConfig(rnb);
         return false;
     }

    File configFile = SPIFFS.open(rfilename, "r");
       if (!configFile) {
         saveRelayDefaultConfig(rnb);
         return false;
    }
         Serial.print(rfilename );     
         Serial.println(" " ); 


    size_t size = configFile.size();
    if (size > buffer_size) {
         Serial.println(F("Relay Config file size is too large, rebuilding."));
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

     /*
        Serial.println(String("*RelayConfParam->v_relaynb  = ") + RelayConfParam->v_relaynb + " \n");     
        Serial.println(String("*RelayConfParam->v_PUB_TOPIC1  = ") + RelayConfParam->v_PUB_TOPIC1 + " \n");   
        Serial.println(String("*RelayConfParam->v_TemperatureValue = ") + RelayConfParam->v_TemperatureValue + " \n");   
        Serial.println(String("*RelayConfParam->v_ttl_PUB_TOPIC   = ") + RelayConfParam->v_ttl_PUB_TOPIC  + " \n");   
        Serial.println(String("*RelayConfParam->v_i_ttl_PUB_TOPIC  = ") + RelayConfParam->v_i_ttl_PUB_TOPIC + " \n");   
        Serial.println(String("*RelayConfParam->v_CURR_TTL_PUB_TOPIC    = ") + RelayConfParam->v_CURR_TTL_PUB_TOPIC   + " \n");   
        Serial.println(String("*RelayConfParam->v_STATE_PUB_TOPIC = ") + RelayConfParam->v_STATE_PUB_TOPIC + " \n");   
        Serial.println(String("*RelayConfParam->v_LWILL_TOPIC  = ") + RelayConfParam->v_LWILL_TOPIC + " \n");   
        Serial.println(String("*RelayConfParam->v_SUB_TOPIC1 = ") + RelayConfParam->v_SUB_TOPIC1 + " \n");   
      */
    
     configFile.close();
     return true;
}


void Relay::start_ttl_timer() {
      this->ticker_relay_ttl->start();
}

void Relay::stop_ttl_timer() {
      this->ticker_relay_ttl->stop();
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
      return this->ticker_relay_ttl->periodscounter();
}

void Relay::setRelayTTT_Timer_Interval(uint32_t interval){
      this->ticker_relay_ttl->interval(interval);
}

ksb_status_t Relay::TTLstate() {
    	return this->ticker_relay_ttl->state();
}

int Relay::readrelay (){
      return digitalRead(this->pin);
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
      uint8_t sts = digitalRead(pn);
      rchangedflag = (sts != v);
      if (rchangedflag){
        digitalWrite(pn,v);
        Relay * rl;
        rl = getrelaybypin(pn);
        if (rl!=nullptr) {
         if (rl->hastimerrunning) { rl->timerpaused = (v==LOW); }
        }
              if (fonchangeInterruptService) fonchangeInterruptService(this);
      }
     // if (fonchangeInterruptService) fonchangeInterruptService(this);
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
