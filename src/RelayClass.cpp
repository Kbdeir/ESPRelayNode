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
  RelayConfParam = new TConfigParams;

  lockupdate = false;
  freeinterval = 200;
  r_in_mode = 1;
  //fMQTT_Update_Topic = "/none";
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
  ticker_ACS712 = new Schedule_timer (fticker_ACS712_func,100,0,MILLIS_);
  ticker_ACS_MQTT = new Schedule_timer (fticker_ACS712_mqtt_func,1000,0,MILLIS_);
  ticker_relay_tta = new Schedule_timer(fttacallback,0,0,MILLIS_);
//  btn_debouncer = new Bounce();

//  fbutton = nullptr;
//  fonclick = nullptr;
//  fon_associatedbtn_change = nullptr;

  rchangedflag = false;

}

Relay::~Relay(){
      delete RelayConfParam ;
      delete ticker_relay_ttl;
      delete ticker_ACS712;
      delete ticker_ACS_MQTT;
      delete ticker_relay_tta;
//      delete fbutton;
//      delete btn_debouncer;
    }


void Relay::watch(){
   if (this->ticker_relay_ttl) this->ticker_relay_ttl->update(this);
   if (this->ticker_ACS712) this->ticker_ACS712->update(this);
   if (this->ticker_ACS_MQTT) this->ticker_ACS_MQTT->update(this);
   if (this->ticker_relay_tta) this->ticker_relay_tta->update(this);
   if (this->freelock) this->freelock->update(this);
   freelockfunc();
/*
   if (fbutton) fbutton->tick(this);
   if (fon_associatedbtn_change){
     btn_debouncer->update();
     if (btn_debouncer->fell() || btn_debouncer->rose()) {
         fon_associatedbtn_change(this);
     }
   }
   */
   if (fgeneralinLoopFunc) fgeneralinLoopFunc(this);
}

String Relay::getRelayPubTopic() {
      return RelayConfParam->v_PUB_TOPIC1;
   }

void Relay::setRelayConfig(TConfigParams * RelayConf) {
      RelayConfParam = RelayConf;
   }

TConfigParams * Relay::getRelayConfig() {
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

boolean Relay::SaveDefautRelayParams(){
   StaticJsonBuffer<buffer_size> jsonBuffer;
   JsonObject& json = jsonBuffer.createObject();

   json["PUB_TOPIC1"]="/home/Controller" + CID() + "/Coils/C1" ;
   json["STATE_PUB_TOPIC"]="/home/Controller" + CID() + "/Coils/State/C1";
   json["ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/VTTL";
   json["i_ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/i/TTL";
   json["ACS_AMPS"]="/home/Controller" + CID() + "/Coils/C1/Amps";
   json["CURR_TTL_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/CURRVTTL";
   json["LWILL_TOPIC"]="/home/Controller" + CID() + "/LWT";
   json["SUB_TOPIC1"]= "/home/Controller" + CID() +  "/#";
   json["ASCmultiple"]="10";
   json["ACS_Sensor_Model"] = "10";
   json["ttl"]="0";
   json["tta"]="0";
   json["Max_Current"]="10";
   json["GPIO12_TOG"]="0";
   json["Copy_IO"]="0";
   json["ACS_Active"]="0";

   File configFile = SPIFFS.open(filename, "w");
   if (!configFile) {
     return false;
   }

   json.printTo(configFile);
   configFile.flush();
   configFile.close();
   return true;
   }

boolean Relay::loadrelayparams(){

    if(SPIFFS.begin()) { Serial.println(F("SPIFFS Initialize....ok")); }
       else {Serial.println(F("SPIFFS Initialization...failed")); }

    const char* filename = "/config.json";

    if (! SPIFFS.exists(filename)) {
         Serial.println(F("config file does not exist! ... building and rebooting...."));
         while (!saveDefaultConfig()){};
         return false;
     }

    File configFile = SPIFFS.open(filename, "r");
       if (!configFile) {
         Serial.println(F("Failed to open config file"));
         saveDefaultConfig();
         return false;
    }

    size_t size = configFile.size();
    if (size > buffer_size) {
         Serial.println(F("Config file size is too large, rebuilding."));
         saveDefaultConfig();
         return false;
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
         Serial.println(F("Failed to parse config file"));
         saveDefaultConfig();
         return false;
    }

     RelayConfParam->v_PUB_TOPIC1 = (json["PUB_TOPIC1"].as<String>()!="") ? json["PUB_TOPIC1"].as<String>() : String("/none");
     RelayConfParam->v_ttl_PUB_TOPIC  = (json["ttl_PUB_TOPIC"].as<String>()!="") ? json["ttl_PUB_TOPIC"].as<String>() : String("/none");
     RelayConfParam->v_i_ttl_PUB_TOPIC = (json["i_ttl_PUB_TOPIC"].as<String>()!="") ? json["i_ttl_PUB_TOPIC"].as<String>() : String("/none");
     RelayConfParam->v_ACS_AMPS  = (json["ACS_AMPS"].as<String>()!="") ? json["ACS_AMPS"].as<String>() : String("/none");
     RelayConfParam->v_CURR_TTL_PUB_TOPIC  = (json["CURR_TTL_PUB_TOPIC"].as<String>()!="") ? json["CURR_TTL_PUB_TOPIC"].as<String>() : String("/none");
     RelayConfParam->v_STATE_PUB_TOPIC = (json["STATE_PUB_TOPIC"].as<String>()!="") ? json["STATE_PUB_TOPIC"].as<String>() : String("/none");
     RelayConfParam->v_ACS_Sensor_Model = (json["ACS_Sensor_Model"].as<String>()!="") ? json["ACS_Sensor_Model"].as<String>() : String("10");
     RelayConfParam->v_ttl = (json["ttl"].as<String>()!="") ? json["ttl"].as<uint32_t>() : 0;
     RelayConfParam->v_tta = (json["tta"].as<String>()!="") ? json["tta"].as<uint32_t>() : 0;
     RelayConfParam->v_Max_Current = (json["Max_Current"].as<String>()!="") ? json["Max_Current"].as<uint8_t>() : 10;
     RelayConfParam->v_LWILL_TOPIC = (json["LWILL_TOPIC"].as<String>()!="") ? json["LWILL_TOPIC"].as<String>() : String("/none");
     RelayConfParam->v_SUB_TOPIC1 = (json["SUB_TOPIC1"].as<String>()!="") ? json["SUB_TOPIC1"].as<String>() : String("/none");
     RelayConfParam->v_ACS_Active = (json["ACS_Active"].as<String>()!="") ? json["ACS_Active"].as<uint8_t>() == 1 : false;

     RelayConfParam->v_IN0_INPUTMODE       =  json["I0MODE"].as<uint8_t>();
     RelayConfParam->v_IN1_INPUTMODE       =  json["I1MODE"].as<uint8_t>();
     RelayConfParam->v_IN2_INPUTMODE       =  json["I2MODE"].as<uint8_t>();


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

/*
  void Relay::attachSwithchButton (uint8_t switchbutton
                                  ,fnptr_a on_associatedbtn_change // on change for input or copy io mode
                                  ,fnptr_a onclick
                                  ,uint8_t im
                                  ,String& MQTT_Update_Topic
                                )
          {
            r_in_mode = im;
            fMQTT_Update_Topic = MQTT_Update_Topic;
            fswitchbutton = switchbutton;
            pinMode (fswitchbutton, INPUT_PULLUP );
            fon_associatedbtn_change = on_associatedbtn_change;
            fonclick = onclick;
            fbutton = new OneButton(fswitchbutton, true);
            btn_debouncer->attach(fswitchbutton,INPUT_PULLUP);
            btn_debouncer->interval(25); // interval in ms
            if (fonclick) fbutton->attachClick(fonclick);        // toggle mode function
          }
*/

  void Relay::attachLoopfunc(fnptr_a GeneralLoopFunc){
      fgeneralinLoopFunc = GeneralLoopFunc;
    }

/*
  uint8_t Relay::getRelaySwithbtn(){
      return this->fswitchbutton;
    }

    */
/*
  uint8_t Relay::getRelaySwithbtnState(){
      return digitalRead(this->fswitchbutton);
    }
    */

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
      }
      if (fonchangeInterruptService) fonchangeInterruptService(this);
    }
    }

  /*Relay * Relay::relayofpin(uint8_t pn){
      return this;
    }*/

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
    } else { return nullptr;}
  }   else { return nullptr;}
}
