// #define USEPREF n
#include <defines.h>

#ifndef DEBUG_DISABLED
  #include <RemoteDebug.h>
  #define HOST_NAME "remotedebug"
  RemoteDebug Debug;
#endif 

#ifdef AppleHK
#include <AppleHKSupport.h>
#endif

#include <Arduino.h>
#include <string.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include "ACS712.h"
#include "OneButton.h"
#include <Bounce2.h>
#include <RelayClass.h>
#include <vector>
#include <ACS_Helper.h>
#include <InputClass.h>
#include <TimerClass.h>
#include <TempSensor.h>
#include <TempConfig.h>
#include "AsyncPing.h"



//#include <AH_EasyDriver.h>
#include <AccelStepper.h>
#include <SimpleTimer.h>

#ifdef blockingTime
  #include <KSBNTP.h>
#else
  #include <KSBAsyncNTP.h>
  AsyncUDP Audp;
#endif

//#include <RelaysArray.h>
//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays
extern std::vector<void *> inputs ; // a list to hold all relays
extern void applyIRMAp(uint8_t Inpn, uint8_t rlyn);

extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
}
 


#ifdef ESP32
  #include <WiFi.h>
  #include <ESPmDNS.h>
    #ifdef USEPREF
        #include <Preferences.h>
        #include "KSBPreferences.h"
    #endif
  #include <FSFunctions.h>
  #include <AsyncTCP.h>
  #include <update.h>
  #include <esp_wifi.h>

#else
  #include <ESP8266WiFi.h>
  #include <ESP.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
#endif

#include <DNSServer.h>
//#include "NTP.h"
#include <time.h>
#include <MQTT_Processes.h>
#include <AsyncHTTP_Helper.h>

bool homekitNotInitialised = true;

time_t prevDisplay = 0; // when the digital clock was displayed
#include <Chronos.h>

NodeTimer NTmr(4);
TempConfig PTempConfig(1);

AsyncPing Pings; //Pings[1]; 
IPAddress addr;

const char * EventNames[] = {
  "N/A", // just a placeholder, for indexing easily
  "PM ",
  "DW ",
  "LM ",
  "YC ",
  "NY ",
  "AD ",
  "CE ",
  NULL
};

#define SERIAL_DEVICE     Serial
#define PRINT(...)    SERIAL_DEVICE.print(__VA_ARGS__)
#define PRINTLN(...)  SERIAL_DEVICE.println(__VA_ARGS__)
#define LINE()    PRINTLN(' ')
#define LINES(n)  for (uint8_t _bl=0; _bl<n; _bl++) { PRINTLN(' '); }
#define WIFI_AP_MODE 1
#define WIFI_CLT_MODE 0
#define DEFAULT_BOUNCE_TIME 100
#define CAL_MAX_NUM_EVENTS_TO_HOLD 10 // above 15 the system freezes, check why

#ifdef ESP32
    #ifdef USEPREF
    Preferences preferences;
    #endif
#endif

#ifdef SR04
      // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
      // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
      int trigPin = 13;    // Trigger
      int echoPin = 12;    // Echo
      long duration, cm, inches;

      #include <hcsr04.h>
      #define TRIG_PIN 14
      #define ECHO_PIN 12
#endif

#ifdef StepperMode
  //AH_EasyDriver(int RES, int DIR, int STEP, int MS1, int MS2, int SLP);
  //AH_EasyDriver shadeStepper(200,14,12,14,12,12,02);    // init w/o "enable" and "reset" functions
  //AH_EasyDriver shadeStepper(200,14,12,4,5,6,7,8);

  #define dirPin 14
  #define stepPin 12
  #define motorInterfaceType 1
  AccelStepper shadeStepper = AccelStepper(motorInterfaceType, stepPin, dirPin);
  //AH_EasyDriver shadeStepper(200,dirPin,stepPin);   // Initialisation

  SimpleTimer timer;
  //Global Variables
  const int stepsPerRevolution = 200; 
  bool steperrun = false;
  int currentPosition = 0;
  int newPosition = 0;  
  const int unrolled = 13; //number of full rotations from fully rolled to fully unrolled
#endif


long timezone     = 1;
byte daysavetime  = 1;
int wifimode      = WIFI_CLT_MODE;

String MAC;
unsigned long lastMillis = 0;
unsigned long lastMillis_1 = 0;
unsigned long lastMillis5000 = 0;
int restartRequired_counter = 0;

uint32_t trials   = 0;
int  WFstatus;
int UpCount       = 0;
WiFiClient net;
String APssid     = (String("Node-") +  CID()+"-");
const char* APpassword = "12345678";
int ledState      = LOW;             					// ledState used to set the LED
unsigned long previousMillis = 0;             // will store last time LED was updated
long blinkInterval = 1000;           		      // blinkInterval at which to blink (milliseconds)
long APModetimer  = 60*5;                     // max allowed time in AP mode, reset if exceeded
long APModetimer_run_value = 0;               // timer value to track the AP mode rining-timer value. reset board if it exceeds APModetimer
AsyncServer* MBserver = new AsyncServer(502); // start listening on tcp port 502
ModbusIP mb;
const int LAMP1_COIL  = 0;
const int LAMP2_COIL  = 1;
float old_acs_value   = 0;
float ACS_I_Current   = 0;

float MCelcius;
float MCelcius2;

static TempSensor tempsensor(TempSensorPin);
static TempSensor TempSensorSecond(SecondTempSensorPin);
extern float MCelcius;
extern float MCelcius_SecondTempSensor;

ACS712 sensor(ACS712_20A, A0);

void ticker_relay_ttl_off (void* obj) ;
void ticker_relay_ttl_periodic_callback(void* obj);
void ticker_ACS712_func (void* obj);
void onRelaychangeInterruptSvc(void* t);
void ticker_ACS712_mqtt (void* obj);

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;


int getpinMode(uint8_t pin) 
{
  if (pin >= NUM_DIGITAL_PINS) return (-1);

  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint32_t *reg = portModeRegister(port);
  if (*reg & bit) return (OUTPUT);

  volatile uint32_t *out = portOutputRegister(port);
  return ((*out & bit) ? INPUT_PULLUP : INPUT);
}


Relay relay0(
    RelayPin,
    ticker_relay_ttl_off,
    ticker_relay_ttl_periodic_callback,
    ticker_ACS712_func,
    ticker_ACS712_mqtt,
    onRelaychangeInterruptSvc,
    relayon
  );

 #ifdef HWver03_4R    
  Relay relay1( 
      Relay1Pin,
      ticker_relay_ttl_off,
      ticker_relay_ttl_periodic_callback,
      ticker_ACS712_func,
      ticker_ACS712_mqtt,
      onRelaychangeInterruptSvc,
      relayon
    );

  Relay relay2(
      Relay2Pin,
      ticker_relay_ttl_off,
      ticker_relay_ttl_periodic_callback,
      ticker_ACS712_func,
      ticker_ACS712_mqtt,
      onRelaychangeInterruptSvc,
      relayon
    );   

  Relay relay3(
      Relay3Pin,
      ticker_relay_ttl_off,
      ticker_relay_ttl_periodic_callback,
      ticker_ACS712_func,
      ticker_ACS712_mqtt,
      onRelaychangeInterruptSvc,
      relayon
    );     
  #endif  


void ticker_ACS712_mqtt (void* relaySender) {
  if (relaySender != nullptr) {
  Relay * rly = static_cast<Relay *>(relaySender);
    if (rly->RelayConfParam->v_ACS_Active) {
        float variance =(abs(ACS_I_Current-old_acs_value));
        // Serial.printf("%6.*lf", 2, variance );
        if (true) {//(variance > 0.05){
           mqttClient.publish(rly->RelayConfParam->v_ACS_AMPS.c_str(), 2, true, String(ACS_I_Current).c_str());
          Serial.println(String("\n I = ") + ACS_I_Current + " A");
        }
        old_acs_value = ACS_I_Current;
  }
}
}


void ticker_ACS712_func (void* relaySender) {
  if (relaySender != nullptr) {
  Relay * rly = static_cast<Relay *>(relaySender);
    if (rly->RelayConfParam->v_ACS_Active) {
        ACS_I_Current = sensor.getCurrentAC();
        //Serial.println(String("ACS_I_Current = ") + ACS_I_Current + " A");
        //Serial.println(String("RelayConfParam->v_Max_Current = ") + rly->RelayConfParam->v_Max_Current);
        if (ACS_I_Current > rly->RelayConfParam->v_Max_Current) {
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
            Serial.println(F("ACS_I_Current > RelayConfParam->v_Max_Current"));
        }
    }
  }
}


void ticker_relay_ttl_periodic_callback(void* relaySender){
  Serial.print(F("\n TTL Countdown: "));
  //uint32_t t = ticker_relay_ttl.periodscounter();
  if (relaySender != nullptr) {
    Relay * rly = static_cast<Relay *>(relaySender);
    uint32_t t = rly->getRelayTTLperiodscounter();
    if (digitalRead(rly->getRelayPin() == HIGH)) {
      mqttClient.publish( rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, String(rly->RelayConfParam->v_ttl).c_str());
      mqttClient.publish( rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, (String(t).c_str()));
    }
  }
}


void ticker_relay_ttl_off (void* relaySender) {
  if (relaySender != nullptr) {
    Relay * rly = static_cast<Relay *>(relaySender);
    rly->mdigitalWrite(rly->getRelayPin(),LOW);
  }
}


void onRelaychangeInterruptSvc(void* relaySender){
  Relay * rly = static_cast<Relay *>(relaySender);
  uint16_t pack = 0;

  if (rly->rchangedflag ) {
      rly->rchangedflag = false;

      if (rly->readrelay() == HIGH) {
        Serial.print(F("\n\n[INFO] interrupt *ON* occurred."));
        if (rly->RelayConfParam->v_ttl > 0 ) {
          rly->start_ttl_timer();
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, ON);
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, ON);  
      }

      if (digitalRead(rly->getRelayPin()) == LOW) {
        Serial.print(F("\n\n[INFO] interrupt *OFF* occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl > 0 ) {
          mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF);
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, OFF);
      }
 
  } else {
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, digitalRead(rly->getRelayPin()) == HIGH ? ON : OFF);                
  }
  #ifdef AppleHK
    if (PIN_SWITCH == rly->getRelayPin()) {
    cha_switch_on.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
    homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);    
    }         
  #endif

}


void process_Input(void * inputSender, void * obj){
  Serial.print(F("\n process_Input"));
  if (inputSender != nullptr) {
      InputSensor * snsr = static_cast<InputSensor *>(inputSender);
    if (snsr->fclickmode == INPUT_NORMAL) {
      Serial.print(snsr->mqtt_topic.c_str());
      mqttClient.publish( snsr->mqtt_topic.c_str(), QOS2, RETAINED, digitalRead(snsr->pin) == HIGH ?  ON : OFF);
    }
    if (snsr->fclickmode == INPUT_TOGGLE) {
      mqttClient.publish( snsr->mqtt_topic.c_str(), QOS2, RETAINED, TOG);
    }
  }
}


void onchangeSwitchInterruptSvc(void* relaySender, void* inputSender){
  Relay * rly = static_cast<Relay *>(relaySender);
  InputSensor * input = static_cast<InputSensor *>(inputSender);
  // Serial.print("\n onchangeSwitchInterruptSvc");
  Serial.print("\n "); Serial.print(digitalRead(input->pin));
  rly->mdigitalWrite(rly->getRelayPin(),digitalRead(input->pin)); //rly->getRelaySwithbtnState());
  mqttClient.publish(input->mqtt_topic.c_str(), QOS2, RETAINED,digitalRead(input->pin) == HIGH ? ON : OFF);
}


// this function will be called when button is clicked.
void buttonclick(void* relaySender, void* inputSender) {
  Serial.print(F("\n buttonclick"));
  if (relaySender){
    Relay * rly = static_cast<Relay *>(relaySender);
    InputSensor * input;
    input = static_cast<InputSensor *>(inputSender);
      if (rly->readrelay() == HIGH) {
        rly->ticker_relay_tta->stop();
        rly->mdigitalWrite(rly->getRelayPin(), LOW);
        rly->stop_ttl_timer();
      } else {
      rly->ticker_relay_tta->interval(rly->RelayConfParam->v_tta*1000);
      rly->ticker_relay_tta->start();
      }
    mqttClient.publish(input->mqtt_topic.c_str(), QOS2, RETAINED,TOG);
  }
}

void TempertatureSensorEvent(int rlynb, float TSolarPanel, float TSolarTank) {
  Serial.print(F("\n[INFO] TempertatureSensorEvent"));
  Relay * rtmp =  getrelaybynumber(0);
  
  //mqttClient.publish(input->mqtt_topic.c_str(), QOS2, RETAINED,TOG);
  if ((PTempConfig.spanTempfrom != 0) && (PTempConfig.spanBuffer != 0)) {
    if (TSolarPanel > PTempConfig.spanTempfrom) {
      Serial.print(F("\n[INFO] TempertatureSensorEvent SOLAR PANEL > "));
      Serial.print (PTempConfig.spanTempfrom);
      if ((TSolarPanel - TSolarTank) > PTempConfig.spanBuffer) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
        Serial.print(F("\n[INFO] TempertatureSensorEvent SOLAR PANEL - TankTemp > "));
        Serial.print (PTempConfig.spanBuffer);
      }
      if ((TSolarPanel - TSolarTank) < PTempConfig.spanBuffer) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
        Serial.print(F("\n[INFO] TempertatureSensorEvent SOLAR PANEL - TankTemp < "));
        Serial.print (PTempConfig.spanBuffer);
      }    
    }

    if (TSolarPanel < PTempConfig.spanTempfrom) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
        Serial.print(F("\n[INFO] TempertatureSensorEvent SOLAR PANEL < "));
        Serial.print (PTempConfig.spanTempfrom);
    }
  }
    
  
}


void relayloopservicefunc(void* relaySender){
if (relaySender){
  Relay * rly = static_cast<Relay *>(relaySender);
    if (rly->TTLstate() != RUNNING_) {
      if (digitalRead(rly->getRelayPin()) == HIGH) {
        if (rly->RelayConfParam->v_ttl > 0 ) rly->start_ttl_timer(); // ticker_relay_ttl.start();
      }
    }
  }
}


void IP_info()
{
  // int32_t rssi;           // store WiFi signal strength here
  // String getSsid;
  // String getPass;
  // String getSsid = WiFi.SSID();
  // String getPass = WiFi.psk();
   #ifdef ESP32
     wifi_config_t conf;
     esp_wifi_get_config(WIFI_IF_STA, &conf);  // load wifi settings to struct comf
   #endif

      MAC = WiFi.macAddress();
      Serial.printf( "\n\n\tSSID\t%s, ",  WiFi.SSID().c_str() );
      //Serial.print( rssi);
			//Serial.printf(" dBm\n" );  // printf??
      Serial.printf( "\tPass:\t %s\n",  WiFi.psk().c_str() );
      Serial.print( F("\n\n\tIP address:\t") );
			Serial.print(WiFi.localIP() );
      /*
      Serial.print(F(" / "));
      Serial.println( WiFi.subnetMask() );
      Serial.print( F("\tGateway IP:\t") );  Serial.println( WiFi.gatewayIP() );
      Serial.print( F("\t1st DNS:\t") );     Serial.println( WiFi.dnsIP() );
      */
      Serial.printf( "\tMAC:\t\t%s\n", MAC.c_str());
}


void blinkled(){
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= blinkInterval) {
		previousMillis = currentMillis;
		if (ledState == LOW) {
			ledState = HIGH;
		} else {
			ledState = LOW;
		}
		digitalWrite(led, ledState);
	}
}

void blinkledtimed(void* obj){
		if (ledState == LOW) {
			ledState = HIGH;
		} else {
			ledState = LOW;
		}
		digitalWrite(led, ledState);
      #ifdef HWver03_4R
    		// digitalWrite(led2, ledState);
      #endif  
}


extern Schedule_timer Wifireconnecttimer;

void tiker_WIFI_CONNECT_func (void* obj) {
        Serial.print(F("\n[WIFI] WIFI timer active"));
        // Access Point mode configuration jumper is set
        if (digitalRead(ConfigInputPin) == LOW){ 
                  Serial.print(F("\n[WIFI] Access Point mode configuration jumper is set"));                               
          Wifireconnecttimer.stop();
          WiFi.mode(WIFI_AP_STA);
        } else {
          // Station mode, try to connect with saved SSID & PASS
          WiFi.mode(WIFI_STA);
          WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); 
        }
}
Schedule_timer Wifireconnecttimer(tiker_WIFI_CONNECT_func,10000,0,MILLIS_);


void startsoftAP(){
  delay(1000);
  Serial.println(F("** softAP **"));
  wifimode = WIFI_AP_MODE;
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(100);
  String t = APssid + String(MyConfParam.v_PhyLoc);
  WiFi.softAP(t.c_str(), APpassword);
  Serial.println(F("starting softAP mode - Connect to: "));
  Serial.println(t);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.softAPIP());
	trials = 0;
	SetAsyncHTTP();
	blinkInterval = 100;
	APModetimer_run_value = 0;
}


//------------modbus-------------------------------------------------------------------------------------------------
/* clients events */
/*static void handleError(void* arg, AsyncClient* client, int8_t error) {
 Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}*/

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
  /*
  Serial.printf("\n Modbus query received from client %s \n", client->remoteIP().toString().c_str());
  Serial.print("\n Modbusfunction: ");
  Serial.print(fn);
  Serial.print("\n");
  */
  uint8_t fn = mb.task(client, data, len, false); // first time, process to determine function - don't respond
  if (fn==1){
    Relay * t = nullptr;
    t = getrelaybypin(RelayPin);
    if (t) {
      mb.Coil(LAMP1_COIL, t->readrelay());  // set internal coil value in the mb system to the actual value of the relay
    }
  }

  if (fn==5) {
     Relay * t = nullptr;
     t = getrelaybypin(RelayPin);
     if (t) t->mdigitalWrite(RelayPin, mb.Coil(LAMP1_COIL));
  }

  mb.task(client, data, len, true);
}

/*
static void handleDisconnect(void* arg, AsyncClient* client) {
 Serial.printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
 Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}*/

/* server events */
static void handleNewClient(void* arg, AsyncClient* client) {
   Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());
   // register events
   client->onData(&handleData, NULL);
   //client->onError(&handleError, NULL);
   //client->onDisconnect(&handleDisconnect, NULL);
   //client->onTimeout(&handleTimeOut, NULL);
}
//-------------------------------------------------------------------------------------------------------------

DefineCalendarType(Calendar, CAL_MAX_NUM_EVENTS_TO_HOLD);
Calendar MyCalendar;

void chronosInit() {
  MyCalendar.clear();
  PRINTLN(F("Starting up PointsEvents test"));
  Chronos::DateTime::setTime(year(), month(), day(), hour(), minute(), second()); 
  //Chronos::DateTime::setTime(2018, 12, 7, 18, 00, 00);

  uint8_t tcounter = 0;

  while(tcounter <= MAX_NUMBER_OF_TIMERS) { [&tcounter]() 
    {
        ESP.wdtFeed();
        char  timerfilename[30] = "";
        strcpy(timerfilename, "/timer");
        strcat(timerfilename, String(tcounter).c_str());
        strcat(timerfilename, ".json");
        Serial.print (F("\n[[TIMERS ]"));
        config_read_error_t res = loadNodeTimer(timerfilename,NTmr);
       
        tcounter++;

        if ((res == SUCCESS) && NTmr.enabled) {
          //loadNodeTimer("/timer.json",NTmr);
          //Serial.print(F("\n\n\nBEGIN TIMER DEBUG ************"));
          int Year, Month, Day, Hour, Minute, Second ;
          int TYear, TMonth, TDay, THour, TMinute, TSecond ;

          String DF = NTmr.spanDatefrom + " " + NTmr.spantimefrom;
          String DT = NTmr.spanDateto + " " + NTmr.spantimeto;

          sscanf(DF.c_str(), "%d-%d-%d %d:%d:%d", &Year, &Month, &Day, &Hour, &Minute, &Second);
          sscanf(DT.c_str(), "%d-%d-%d %d:%d:%d", &TYear, &TMonth, &TDay, &THour, &TMinute, &TSecond);

          Chronos::DateTime previous(Year, Month, Day, Hour, Minute, 0);
          Chronos::DateTime next(TYear, TMonth, TDay, THour, TMinute, 0);
          Chronos::Span::Absolute timeDiff = next-previous;

          /*  
          PRINTLN(F("\nFrom timer values: "));
          PRINT(NTmr.spanDatefrom); PRINT(" "); PRINT(NTmr.spantimefrom);
          PRINT(" = ");
          PRINT(Year);
          PRINT("-");
          PRINT(Month);
          PRINT("-");
          PRINT(Day);
          PRINT(" ");
          PRINT(Hour);
          PRINT(":");
          PRINT(Minute);

          PRINTLN(F("\n\nTo timer values: "));
          PRINT(NTmr.spanDateto); PRINT(" ");
          PRINT(NTmr.spantimeto);
          PRINT(" = ");

          PRINT(TYear);
          PRINT("-");
          PRINT(TMonth);
          PRINT("-");
          PRINT(TDay);
          PRINT(" ");
          PRINT(THour);
          PRINT(":");
          PRINT(TMinute);

          PRINTLN(F("\n\ntimeDiff values: "));
          PRINT(F("There are "));
          PRINT(timeDiff.days());
          PRINT(F(" days, "));
          PRINT(timeDiff.hours());
          PRINT(F(" hours, "));
          PRINT(timeDiff.minutes());
          PRINT(F(" minutes and  "));
          PRINT(timeDiff.seconds());
          PRINT(F(" seconds between DF & DT."));
          PRINTLN(F("\n\nTotalSeconds: "));
          PRINT(timeDiff.totalSeconds());

          Serial.print(F("\nTimer type: "));
          Serial.print(NTmr.TM_type);
          Serial.print(F(" - FULL SPAN timer type: "));
          Serial.print(TimerType::TM_FULL_SPAN);
          Serial.print(F("\n\n\n END TIMER DEBUGG ************\n\n\n"));
          */

          if (NTmr.TM_type == TimerType::TM_WEEKDAY_SPAN) {

            Serial.print(F("\n entered WEEKLY TIMER MODE eval 0"));
            if ((previous.startOfDay() <= Chronos::DateTime::now()) && (Chronos::DateTime::now() <= next.endOfDay())) {
            Serial.print(F("\n entered WEEKLY TIMER MODE eval 1"));

            uint32_t dailydiffsecs = timeDiff.totalSeconds() - (timeDiff.days() * 24 * 3600);
            if (NTmr.Mark_Hours + NTmr.Mark_Minutes > 0) {
              dailydiffsecs = (NTmr.Mark_Hours * 3600) +  (NTmr.Mark_Minutes * 60) ;
            }

              if (NTmr.weekdays->Sunday) {
                MyCalendar.add(
                    Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Sunday,Hour, Minute, 00),
                    Chronos::Span::Seconds(dailydiffsecs))
                  );
              }
                if (NTmr.weekdays->Monday) {
                 MyCalendar.add(
                     Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Monday,Hour, Minute, 00),
                     Chronos::Span::Seconds(dailydiffsecs))
                   );
               }
                if (NTmr.weekdays->Tuesday) {
                  MyCalendar.add(
                      Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Tuesday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs))
                    );
                }
                if (NTmr.weekdays->Wednesday) {
                   MyCalendar.add(
                       Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Wednesday,Hour, Minute, 00),
                       Chronos::Span::Seconds(dailydiffsecs))
                     );
                 }
                 if (NTmr.weekdays->Thursday) {
                    MyCalendar.add(
                        Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Thursday,Hour, Minute, 00),
                        Chronos::Span::Seconds(dailydiffsecs))
                      );
                  }
                  if (NTmr.weekdays->Friday) {
                     MyCalendar.add(
                         Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Friday,Hour, Minute, 00),
                         Chronos::Span::Seconds(dailydiffsecs))
                       );
                   }
                   if (NTmr.weekdays->Saturday) {
                      MyCalendar.add(
                          Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Saturday,Hour, Minute, 00),
                          Chronos::Span::Seconds(dailydiffsecs))
                        );
                    }
                }
          }

          if (NTmr.TM_type == TimerType::TM_DAILY_SPAN) {
                if ((previous.startOfDay() <= Chronos::DateTime::now()) && (Chronos::DateTime::now() <= next.endOfDay())) {
                  uint32_t dailydiffsecs = 0;
                  if (NTmr.Mark_Hours + NTmr.Mark_Minutes == 0) {
                    dailydiffsecs = timeDiff.totalSeconds() - (timeDiff.days() * 24 * 3600);
                  }
                  if (NTmr.Mark_Hours + NTmr.Mark_Minutes > 0) {
                    dailydiffsecs = (NTmr.Mark_Hours * 3600) +  (NTmr.Mark_Minutes * 60) ;
                  }

                    MyCalendar.add(
                        Chronos::Event(NTmr.relay,Chronos::Mark::Daily(Hour, Minute, 00),
                        Chronos::Span::Seconds(dailydiffsecs))
                      );
                }
          }

          if (NTmr.TM_type == TimerType::TM_FULL_SPAN) {
                    MyCalendar.add(
                        Chronos::Event(NTmr.relay,previous,next)
                      );
          }

        } // if SUCCESS
    } (); // anonym. func.
  } // while loop

  LINE();
  PRINT(F("[NTP ] Presumably got NTP time,"));
  PRINT(F(" right \"now\" it's: "));
  Chronos::DateTime::now().printTo(SERIAL_DEVICE);
  Chronos::DateTime nowTime(Chronos::DateTime::now());
   //nowTime.printTo(SERIAL_DEVICE);


  #ifdef AppleHK
    //homekit_storage_reset();   
    if (homekitNotInitialised) {
      homekitNotInitialised = false;
      my_homekit_setup();
    }
  #endif

  LINE();
}


void chronosevaluatetimers(Calendar MyCalendar) {
  if (ftimesynced){
  // create an array of Event::Occurrence objects, to hold replies from the calendar
  Chronos::Event::Occurrence occurrenceList[CAL_MAX_NUM_EVENTS_TO_HOLD];
  // listOngoing: get events that are happening at specified datetime.  Called with
  // listOngoing(MAX_NUMBER_OF_EVENTS, INTO_THIS_ARRAY, AT_DATETIME)
  // It will return the number of events set in the INTO_THIS_ARRAY array.
  Chronos::DateTime nowTime(Chronos::DateTime::now());
  int numOngoing = MyCalendar.listOngoing(CAL_MAX_NUM_EVENTS_TO_HOLD, occurrenceList, nowTime);
  if (numOngoing) {
    // At least one event is happening at "nowTime"...
    LINE();
    //PRINTLN(F("**** Some things are going on this very minute! ****"));
    for (int i = 0; i < numOngoing; i++) {
      PRINT(F("[INFO] Running Event: "));
      PRINT((int )occurrenceList[i].id);
      PRINT('\t');
      PRINT(EventNames[occurrenceList[i].id]);
      PRINT(F("\t ends in: "));
      (nowTime - occurrenceList[i].finish).printTo(SERIAL_DEVICE);


//      if (occurrenceList[i].id < relays.size()) {
        Relay * rly = getrelaybynumber(occurrenceList[i].id); //static_cast<Relay *>(relays.at(occurrenceList[i].id));
        if (rly) {
          if ((nowTime > occurrenceList[i].start) && (nowTime < occurrenceList[i].start + 5)) {
            rly->timerpaused = false;
            rly->hastimerrunning = true;
          }
          if ((nowTime > occurrenceList[i].start + 1)) {
            rly->hastimerrunning = true;
            // LINE();
            PRINTLN(F("\n[INFO] turning relay [ON]... event is Starting - TimerPaused value"));
            // PRINT(rly->timerpaused);
            if (!digitalRead(rly->getRelayPin())){
              if (!rly->timerpaused) {
                rly->mdigitalWrite(rly->getRelayPin(),HIGH);
              }
            }
          }
          if ((nowTime == occurrenceList[i].finish - 1 )) {
            LINE();
            rly->lockupdate = false;
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
            PRINTLN(F("[INFO]truning relay OFF... event is done - TimerPaused value"));
            // PRINT(rly->timerpaused);
            rly->timerpaused = false;
            rly->hastimerrunning = false;
          }
        } // if rly !=nullptr
//      } // if rly in relys vector

    } // for loop
  } // if numongoing > 0
   else {
  //  PRINTLN(F("Looks like we're free for the moment..."));
    }
  }
}


 #ifndef StepperMode
  //inputs
  #if defined (HWver02)  || defined (HWver03)
  InputSensor Inputsnsr12(InputPin12,process_Input,INPUT_NONE);
  InputSensor Inputsnsr13(SwitchButtonPin2,process_Input,INPUT_NONE);
  //InputSensor Inputsnsr14(Relay2Pin,process_Input,INPUT_NONE); // just moved to make room for connecting the ds18 temp sensor to  InputPin14
  InputSensor Inputsnsr14(InputPin14,process_Input,INPUT_NONE);
  #endif

  #ifdef HWver03
  InputSensor Inputsnsr02(InputPin02,process_Input,INPUT_NONE);
  #endif

  #ifdef HWver03_4R
  InputSensor Inputsnsr02(InputPin02,process_Input,INPUT_NONE);
  InputSensor Inputsnsr14(InputPin14,process_Input,INPUT_NONE); 
  InputSensor Inputsnsr13(InputPin13,process_Input,INPUT_NONE);   
  #endif
#endif

//InputSensor Inputsnsr14(InputPin14,process_Input,INPUT_NONE);




void thingsTODO_on_WIFI_Connected() {
            Serial.println(F("WiFi Connected."));
            IP_info();

            SetAsyncHTTP();
        		blinkInterval = 1000;
            Serial.print(F("\n[WIFI]IP number assigned by DHCP is "));
            Serial.println(WiFi.localIP());
            Serial.println(F("\n[INFO] Starting UDP"));


            #ifdef ESP8266

                // AsyncNTP
                #ifdef blockingTime
                      Udp.begin(localPort);
                #else
                if (Audp.connect(MyConfParam.v_timeserver, NTP_REQUEST_PORT))
                {
                  Serial.println(F("Time server connected \n"));
                  Audp.onPacket([](AsyncUDPPacket packet)
                  {
                    parsePacket(packet);
                  });
                }
                #endif
                

               /* if(Audp.listen(1234)) {
                    Serial.print("UDP Listening on IP: ");
                    Serial.println(WiFi.localIP());
                    Audp.onPacket([](AsyncUDPPacket packet) {
                        Serial.print("UDP Packet Type: ");
                        Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
                        Serial.print(", From: ");
                        Serial.print(packet.remoteIP());
                        Serial.print(":");
                        Serial.print(packet.remotePort());
                        Serial.print(", To: ");
                        Serial.print(packet.localIP());
                        Serial.print(":");
                        Serial.print(packet.localPort());
                        Serial.print(", Length: ");
                        Serial.print(packet.length());
                        Serial.print(", Data: ");
                        Serial.write(packet.data(), packet.length());
                        Serial.println();
                        //reply to the client
                        packet.printf("Got %u bytes of data", packet.length());
                        parsePacket(packet);
                    });
                } */        

            #endif


            Serial.println(F("[INFO] starting mdns"));
            if (!MDNS.begin((MyConfParam.v_PhyLoc).c_str())) {
              Serial.println(F("[INFO] Error setting up MDNS responder!"));
            }
            Serial.println(F("[INFO] mDNS responder started"));
            MDNS.addService("http","tcp", 80); // Announce esp tcp service on port 8080
            MDNS.addServiceTxt("http", "tcp","MQTT server", MyConfParam.v_MQTT_BROKER.toString().c_str());
            MDNS.addServiceTxt("http", "tcp","Chip", String(MAC.c_str()) + " - Chip id: " + CID());

            
            #ifdef blockingTime
             setSyncProvider(getNtpTime);
            #else
             setSyncProvider(timeprovider);
             setSyncInterval(2); 
            #endif 

            #ifndef DEBUG_DISABLED  
            debugV("* Starting MQTT connection timer, 5 seconds period"); 
            #endif 
            tiker_MQTT_CONNECT.start();  // timer will retry to connect every 5s. 
        		MBserver->begin();
        		MBserver->onClient(&handleNewClient, MBserver);

}



/*
void Wifi_connect() {

  Serial.println(F("[INFO] Starting WiFi"));
 // WiFi.softAPdisconnect();
 // WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  delay(100);

    if (digitalRead(ConfigInputPin) == HIGH) {
        WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); // try to connect with saved SSID & PASS
        Serial.print("\n[WIFI] ssid: "); Serial.print(MyConfParam.v_ssid.c_str());
        Serial.print("\n[WIFI] pass: "); Serial.print(MyConfParam.v_pass.c_str());Serial.print("\n ");
        trials = 0;
      	blinkInterval = 50;
        // LedBlinker.update(nullptr);
        /* while ((WiFi.status() != WL_CONNECTED) & (trials < MaxWifiTrials*2)){
        //  while ((WiFi.waitForConnectResult() != WL_CONNECTED) & (trials < MaxWifiTrials)){
            delay(50);
        		blinkled();
            trials++;
            Serial.print(F("-"));
        }*/
 /*       if  (WiFi.status() == WL_CONNECTED)   {
            thingsTODO_on_WIFI_Connected();
            trials = 0;
            APModetimer_run_value = 0;
        }  // wifi is connected
    } // if (digitalRead(ConfigInputPin) == HIGH)

    if (digitalRead(ConfigInputPin) == LOW){
      Serial.println(F("[WIFI] Starting AP_STA mode"));
      WiFi.mode(WIFI_AP_STA);
    	if ((WiFi.status() != WL_CONNECTED))	{
    		startsoftAP();
    	}
    }
}
*/

void setupInputs(){



#ifndef StepperMode
  #if defined (HWver02)  || defined (HWver03)
  Inputsnsr14.initialize(InputPin14,process_Input,INPUT_NONE);
  Inputsnsr12.initialize(InputPin12,process_Input,INPUT_NONE);
  Inputsnsr13.initialize(SwitchButtonPin2,process_Input,INPUT_NONE);  
  #endif
#ifdef HWver03_4R
  Inputsnsr02.initialize(InputPin02,process_Input,INPUT_NONE);
  Inputsnsr02.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr02.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr02.post_mqtt = true;
  Inputsnsr02.mqtt_topic = MyConfParam.v_InputPin12_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr02.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);

  Inputsnsr14.initialize(InputPin14,process_Input,INPUT_NONE);
  Inputsnsr14.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr14.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr14.post_mqtt = true;
  Inputsnsr14.mqtt_topic = MyConfParam.v_InputPin14_STATE_PUB_TOPIC;
  Inputsnsr14.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);  

  Inputsnsr13.initialize(InputPin13,process_Input,INPUT_NONE);
  Inputsnsr13.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr13.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr13.post_mqtt = true;
  Inputsnsr13.mqtt_topic = MyConfParam.v_InputPin14_STATE_PUB_TOPIC;  // change this later KSB
  Inputsnsr13.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);    
#endif
  #ifdef HWver03
  Inputsnsr02.initialize(InputPin02,process_Input,INPUT_NONE);
  Inputsnsr02.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr02.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr02.post_mqtt = true;
  Inputsnsr02.mqtt_topic = MyConfParam.v_InputPin12_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr02.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);
  #endif

  #if defined (HWver02)  || defined (HWver03)
  Inputsnsr12.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr12.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr12.post_mqtt = true;
  Inputsnsr12.mqtt_topic = MyConfParam.v_InputPin12_STATE_PUB_TOPIC;
  Inputsnsr12.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);

  Inputsnsr13.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr13.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr13.post_mqtt = true;
  Inputsnsr13.mqtt_topic = MyConfParam.v_TOGGLE_BTN_PUB_TOPIC;
  Inputsnsr13.fclickmode = static_cast <input_mode>(MyConfParam.v_IN0_INPUTMODE);

  Inputsnsr14.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr14.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr14.post_mqtt = true;
  Inputsnsr14.mqtt_topic = MyConfParam.v_InputPin14_STATE_PUB_TOPIC;
  Inputsnsr14.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);
  //Inputsnsr14.SetInputSensorPin(InputPin14);
  #endif
  #endif
}


/*
#ifdef StepperMode

void processStepper()
{
  if (newPosition > currentPosition)
  {
    shadeStepper.sleepON();
    shadeStepper.move(4144, BACKWARD);
    currentPosition++;
  }
  if (newPosition < currentPosition)
  {
    shadeStepper.sleepON();
    shadeStepper.move(4144, FORWARD);
    currentPosition--;
  }
  if (newPosition == currentPosition)
  {
    if (currentPosition == 0 || currentPosition == unrolled)
    {
      shadeStepper.sleepOFF();
    }
  }

              Serial.print(F("\n[INFO] NEW Position: "));
              Serial.println(newPosition);
              Serial.print(F("\n[INFO] Current Position: "));
              Serial.println(currentPosition);
              
}


void checkIn()
{
 // char topic[40];
//  strcpy(topic, "checkIn/");
 // strcat(topic, mqtt_client_id);
 // client.publish(topic, "OK"); 
}
#endif
*/

  

void setup() {

      Pings.on(true,[](const AsyncPingResponse& response){
        IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
        if (response.answer)
          Serial.printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n", response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
        else
          Serial.printf("no answer yet for %s icmp_seq=%d\n", addr.toString().c_str(), response.icmp_seq);
        return false; //do not stop
      });
      Pings.on(false,[](const AsyncPingResponse& response){
        IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
        Serial.printf("total answer from %s sent %d recevied %d time %d ms\n",addr.toString().c_str(),response.total_sent,response.total_recv,response.total_time);
         if (response.total_recv > 0) {
           restartRequired_counter = 0;
         }  else
         {
              if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
                restartRequired_counter++;
                //Serial.printf("\n\nPinging failure count: %i \n\n", restartRequired_counter);
                Serial.print(F("\n\nPinging failed count: "));
                Serial.print(restartRequired_counter);
                Serial.print("\n\n");
                if (restartRequired_counter > MyConfParam.v_Reboot_on_WIFI_Disconnection)  {
                  restartRequired = true;
                }     
              }
         }      
        if (response.mac) {
          // Serial.printf("detected eth address " MACSTR "\n",MAC2STR(response.mac->addr));
        }
        return true;
      });


    #ifdef StepperMode
      //  AH_EasyDriver(int RES, int DIR, int STEP, int MS1, int MS2, int SLP);
      //  shadeStepper.setMicrostepping(0);            // 0 -> Full Step                                
      //  shadeStepper.setSpeedRPM(100);               // set speed in RPM, rotations per minute
      //  shadeStepper.sleepOFF();                     // set Sleep mode OFF  
      //  timer.setInterval(800, processStepper);   
      //  timer.setInterval(90000, checkIn);

      shadeStepper.setMaxSpeed(1000);
      shadeStepper.setAcceleration(200);
      shadeStepper.setSpeed(50);     
    #endif

    pinMode ( led, OUTPUT );
    #ifdef HWver03_4R
     // pinMode ( led2, OUTPUT );
    #endif
    pinMode ( ConfigInputPin, INPUT_PULLUP );

    #ifndef StepperMode
        #if defined (HWver02)  || defined (HWver03)
          pinMode ( InputPin12, INPUT_PULLUP );
          pinMode ( InputPin14, INPUT_PULLUP );
        #endif  
        #ifdef HWver03
        pinMode ( InputPin02, INPUT_PULLUP );
        #endif
    #endif    

    Serial.begin(115200);

        // LedBlinker.start();

        /*
          only need to format SPIFFS the first time we run a
          test or else use the SPIFFS plugin to create a partition
          https://github.com/me-no-dev/arduino-esp32fs-plugin
        */

        #ifdef ESP32
          if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
            Serial.println(F("SPIFFS Mount Failed"));
            return;
          }
          listDir(SPIFFS, "/", 0);
        #else
        /*  if(SPIFFS.format()) { Serial.println("File System Formated"); }
            else { Serial.println("File System Formatting Error"); } */
        #endif
        //wifimode = WIFI_CLT_MODE;

        #ifdef USEPREF
          ReadParams(MyConfParam, preferences);
        #endif

      while (loadConfig(MyConfParam) != SUCCESS){
        delay(2000);
        ESP.restart();
        };

        gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event)
        {
          Serial.print(F("\n[WIFI] Station connected, IP: "));
          Serial.println(WiFi.localIP());
          thingsTODO_on_WIFI_Connected();
          blinkInterval = 1000;
          Wifireconnecttimer.stop();
        });

        disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
        {
          Serial.println(F("\n[WIFI] Station disconnected\n"));
          // Wifi_connect();
          if (blinkInterval > 50) {
            Wifireconnecttimer.start();
            Serial.println(F("[WIFI] Starting reconnection timer\n"));
          }
          blinkInterval = 50;

          if (digitalRead(ConfigInputPin) == LOW){
            Wifireconnecttimer.stop();
            Serial.println(F("[WIFI] Starting AP_STA mode\n"));
            WiFi.mode(WIFI_AP_STA);
            if ((WiFi.status() != WL_CONNECTED))	{
              startsoftAP();
            }
          }      
        });

        
        WiFi.mode(WIFI_STA);
        if (digitalRead(ConfigInputPin) == LOW){
          Serial.print(F("\n[WIFI] Sarting WIFI in AP mode \n"));
          WiFi.mode(WIFI_AP_STA);
        }
        WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); // try to connect with saved SSID & PASS

        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onSubscribe(onMqttSubscribe);
        mqttClient.onUnsubscribe(onMqttUnsubscribe);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.onPublish(onMqttPublish);
        mqttClient.setServer(MyConfParam.v_MQTT_BROKER, MyConfParam.v_MQTT_B_PRT);
        mqttClient.setKeepAlive(5000);
      
        mb.addCoil(LAMP1_COIL);
        mb.addCoil(LAMP2_COIL);

        #ifndef DEBUG_DISABLED
	      Debug.begin(HOST_NAME); // Initialize the WiFi server
        Debug.setResetCmdEnabled(true); // Enable the reset command
      	Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
      	Debug.showColors(true); // Colors
        #endif

    #ifndef StepperMode
        setupInputs();
        // Add inputs to vector. the order is important.
        #if defined (HWver02)  || defined (HWver03)
        inputs.push_back(&Inputsnsr13); // this is input 0, CONF pin on board
        inputs.push_back(&Inputsnsr12); // this is input 1, second input on the board
        inputs.push_back(&Inputsnsr14); // this is input 2, first input on the board
        #endif
        #ifdef HWver03
        inputs.push_back(&Inputsnsr02); // this is input 3, third pin on board
        #endif

        #ifdef HWver03_4R // order is important
        inputs.push_back(&Inputsnsr02); 
        inputs.push_back(&Inputsnsr14);      
        inputs.push_back(&Inputsnsr13);          
        #endif        
    #endif    


     //while (relay0.loadrelayparams(0) != true){
       while (relay0.loadrelayparams(0) != true){
          delay(2000);
          ESP.restart();
        };
        relay0.attachLoopfunc(relayloopservicefunc);
        relay0.stop_ttl_timer();
        relay0.setRelayTTT_Timer_Interval(relay0.RelayConfParam->v_ttl*1000);
   
        #ifdef HWver03_4R   
          while (relay1.loadrelayparams(1) != true){
            delay(2000);
            ESP.restart();
          };
          relay1.attachLoopfunc(relayloopservicefunc);
          relay1.stop_ttl_timer();
          relay1.setRelayTTT_Timer_Interval(relay1.RelayConfParam->v_ttl*1000);

          while (relay2.loadrelayparams(2) != true){
            delay(2000);
            ESP.restart();
          };
          relay2.attachLoopfunc(relayloopservicefunc);
          relay2.stop_ttl_timer();
          relay2.setRelayTTT_Timer_Interval(relay2.RelayConfParam->v_ttl*1000);

          while (relay3.loadrelayparams(3) != true){
            delay(2000);
            ESP.restart();
          };
          relay3.attachLoopfunc(relayloopservicefunc);
          relay3.stop_ttl_timer();
          relay3.setRelayTTT_Timer_Interval(relay3.RelayConfParam->v_ttl*1000);    
        #endif            
        

        relays.push_back(&relay0); // order is important
        #ifdef HWver03_4R  
        // order is important
          relays.push_back(&relay1);
          relays.push_back(&relay2);
          relays.push_back(&relay3);   
        #endif     

        while (loadIRMapConfig(myIRMap) != SUCCESS){
          delay(2000);
          ESP.restart();
        };

        if (relay0.RelayConfParam->v_TemperatureValue != "0") {
            config_read_error_t res = loadTempConfig("/tempconfig.json",PTempConfig);      
        }

       ACS_Calibrate_Start(relay0,sensor);

        // mrelays[0]=&relay0;
        // attachInterrupt(digitalPinToInterrupt(relay2.getRelayPin()), handleInterrupt2, RISING );
        /*
        relay2.attachSwithchButton(SwitchButtonPin2, onchangeSwitchInterruptSvc, buttonclick);
        relay2.attachLoopfunc(relayloopservicefunc);
        relays.push_back(&relay2);
        */
        //mrelays[1]=&relay2;
        // attachInterrupt(digitalPinToInterrupt(InputPin14), InputPin14_handleInterrupt, CHANGE );



    #ifdef StepperMode
    #define stepperenablepin InputPin02
        pinMode(dirPin, OUTPUT);   
        pinMode(stepPin,  OUTPUT);  
        #ifdef HWver03
        pinMode ( stepperenablepin, OUTPUT );
        #endif    
        digitalWrite(stepPin, LOW); 
    #endif
}




void loop() {

  #ifndef DEBUG_DISABLED
  Debug.handle();
  yield();
  #endif

    ESP.wdtFeed();
    MDNS.update();
    pinMode (ConfigInputPin, INPUT_PULLUP );

    if (restartRequired){
      Serial.println(F("\n[SYSTEM] Restarting\n\r"));
      restartRequired = false;
      delay(2500);
      ESP.restart();
    }

 // if  (WiFi.status() != WL_CONNECTED)  {
    //if (APModetimer_run_value == 0) Wifi_connect();
 // }

  #ifdef AppleHK
	  my_homekit_loop();
  #endif   
  // delay(10);
 	blinkled();

  tiker_MQTT_CONNECT.update(nullptr);
  Wifireconnecttimer.update(nullptr);
  // LedBlinker.update(nullptr);

  for (auto it : relays)  {
    Relay * rtemp = static_cast<Relay *>(it);
    if (rtemp) {
        rtemp->watch();
    }
  }

  #ifndef StepperMode
    #if defined (HWver02)  || defined (HWver03)
      Inputsnsr14.watch();
      Inputsnsr12.watch();
      Inputsnsr13.watch();
    #endif
    #ifdef HWver03
      Inputsnsr02.watch();
    #endif

    #ifdef HWver03_4R
      Inputsnsr02.watch();
      Inputsnsr14.watch();     
      Inputsnsr13.watch();            
    #endif

  #endif

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {                   //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      chronosevaluatetimers(MyCalendar);
    }
  }

  if((timeStatus() == timeSet) && CalendarNotInitiated) {
    chronosInit();
    CalendarNotInitiated = false;
  }


 #ifndef StepperMode

  if (millis() - lastMillis_1 > 10000) {
    lastMillis_1 = millis();
    Pings.begin(MyConfParam.v_Pingserver);
  }

  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
		
    #ifdef SR04
    if (MyConfParam. v_Sonar_distance != "0") {
          pinMode(TRIG_PIN, INPUT_PULLUP);
          pinMode(ECHO_PIN, INPUT_PULLUP);
          //HCSR04 hcsr04(TRIG_PIN, ECHO_PIN, 20, 4000);
          //Serial.println(hcsr04.distanceInMillimeters());
          int trigPin = TRIG_PIN;    // Trigger
          int echoPin = ECHO_PIN;    // Echo
          long duration, cm, inches;
          // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
          // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
          pinMode(trigPin, OUTPUT);
          digitalWrite(trigPin, LOW);
          delayMicroseconds(5);
          digitalWrite(trigPin, HIGH);
          delayMicroseconds(15);
          digitalWrite(trigPin, LOW);
          // Read the signal from the sensor: a HIGH pulse whose
          // duration is the time (in microseconds) from the sending
          // of the ping to the reception of its echo off of an object.
          pinMode(echoPin, INPUT);
          duration = pulseIn(echoPin, HIGH);
          pinMode(TRIG_PIN, INPUT_PULLUP);
          pinMode(ECHO_PIN, INPUT_PULLUP);

          cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343
          if (cm > MyConfParam.v_Sonar_distance_max) { cm = -1; }
          inches = (duration/2) / 74;   // Divide by 74 or multiply by 0.0135
          Serial.print(inches);
          Serial.print("in, ");
          Serial.print(cm);
          Serial.print("cm");
          Serial.println();

          mqttClient.publish(MyConfParam.v_Sonar_distance.c_str(), QOS2, RETAINED, [cm](){
                char tmp[10];
                itoa(cm,tmp,10);
                return tmp; //
              }()
            );
            /*

            mqttClient.publish((MyConfParam.v_Sonar_distance+"_rem").c_str(), QOS2, RETAINED, [cm](){
                  char tmp[10];
                  itoa(MyConfParam.v_Sonar_distance_max - cm,tmp,10);
                  return tmp; //
                }()
              );
              */
    }
    #endif 

    if (wifimode == WIFI_AP_MODE) {
  		APModetimer_run_value++;
      Serial.print(F("\n[WIFI] ApMode will restart after (seconds): "));
      Serial.print(APModetimer-APModetimer_run_value);
  		if (APModetimer_run_value == APModetimer) {
        APModetimer_run_value = 0;
        ESP.restart();
      }
    }
  }
  #endif

 #ifndef StepperMode
  if (relay0.RelayConfParam->v_TemperatureValue != "0") {
    if (millis() - lastMillis5000 > 5000) {
  //    pinMode(TempSensorPin,  INPUT_PULLUP );  
      lastMillis5000 = millis();
      tempsensor.getCurrentTemp(0);
      MCelcius = tempsensor.Celcius;
      TempSensorSecond.getCurrentTemp(0);
      MCelcius2 = TempSensorSecond.Celcius;

      float rtmp = roundf(tempsensor.Celcius);
      float TSolarTank = rtmp;

      Serial.print("\n[INFO] Temperature: ");
      Serial.print(tempsensor.getCurrentTemp(0));
      #ifndef DEBUG_DISABLED
      debugV("[INFO] TempSensor1 %.2f C ", TSolarTank);
      #endif

      
      mqttClient.publish(relay0.RelayConfParam->v_TemperatureValue.c_str(), QOS2, RETAINED, [rtmp](){
            char tmp[10];
            itoa(rtmp,tmp,10);
            return tmp; //
          // return String(round(MCelcius)).c_str();
          }()
        );

      rtmp = roundf(TempSensorSecond.Celcius);
      float TSolarPanel = rtmp;
      Serial.print(F("\n[INFO] Temperature_Sensor2: "));
      Serial.print(TempSensorSecond.getCurrentTemp(0)); 
      #ifndef DEBUG_DISABLED
      debugV("[INFO] TempSensor2 %.2f C ", TSolarPanel);
      #endif

      mqttClient.publish((relay0.RelayConfParam->v_TemperatureValue + "_2").c_str(), QOS2, RETAINED, [rtmp](){
            char tmp[10];
            itoa(rtmp,tmp,10);
            return tmp; //
          // return String(round(MCelcius)).c_str();
          }()
        );

      #ifdef SolarHeaterControllerMode
            TempertatureSensorEvent(0,TSolarPanel,TSolarTank);
      #else
      #endif

    }
  }
 #endif 

  #ifdef StepperMode
    //shadeStepper.move(1600);               // move 1600 steps
    //shadeStepper.move(-1600);              // move 1600 steps
    //shadeStepper.revolve(2.0);             // revolve 2 times
    //shadeStepper.rotate(180.0);            // rotate 180° 
    if (steperrun) {
        shadeStepper.setSpeed(400);
        digitalWrite(stepperenablepin,false);
        while(shadeStepper.currentPosition() != 800)
        {
          shadeStepper.runSpeed();
          ESP.wdtFeed();
        }
        steperrun = ! steperrun;
        digitalWrite(stepperenablepin,true);
        shadeStepper.stop();
    }
  #endif  

}
