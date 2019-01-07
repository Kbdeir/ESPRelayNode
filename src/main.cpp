
// #define USEPREF
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


//#include <RelaysArray.h>
//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays
extern std::vector<void *> inputs ; // a list to hold all relays

extern void applyIRMAp(uint8_t Inpn, uint8_t rlyn);

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

#include <KSBNTP.h>
time_t prevDisplay = 0; // when the digital clock was displayed
#include <Chronos.h>

NodeTimer NTmr(4);

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

#ifdef ESP32
    #ifdef USEPREF
    Preferences preferences;
    #endif
#endif


long timezone     = 1;
byte daysavetime  = 1;
int wifimode      = WIFI_CLT_MODE;
const int led     = 02;
String MAC;
unsigned long lastMillis = 0;
uint32_t trials   = 0;
int  WFstatus;
int UpCount       = 0;
WiFiClient net;
String APssid     = (String("Node-") +  CID()+"-");
const char* APpassword = "12345678";
int ledState      = LOW;             					// ledState used to set the LED
unsigned long previousMillis = 0;         // will store last time LED was updated
long blinkInterval = 1000;           		      // blinkInterval at which to blink (milliseconds)
long APModetimer  = 60*5;
long APModetimer_run_value = 0;
AsyncServer* MBserver = new AsyncServer(502); // start listening on tcp port 7050
ModbusIP mb;
const int LAMP1_COIL  = 1;
const int LAMP2_COIL  = 2;
float old_acs_value   = 0;
float ACS_I_Current   = 0;

ACS712 sensor(ACS712_20A, A0);

void ticker_relay_ttl_off (void* obj) ;
void ticker_relay_ttl_periodic_callback(void* obj);
void ticker_ACS712_func (void* obj);
void onRelaychangeInterruptSvc(void* t);
void ticker_ACS712_mqtt (void* obj);

Relay relay1(
    RelayPin,
    ticker_relay_ttl_off,
    ticker_relay_ttl_periodic_callback,
    ticker_ACS712_func,
    ticker_ACS712_mqtt,
    onRelaychangeInterruptSvc,
    relayon
  );

  /*Relay relay2(
      Relay2Pin,
      ticker_relay_ttl_off,
      ticker_relay_ttl_periodic_callback,
      ticker_ACS712_func,
      ticker_ACS712_mqtt,
      onRelaychangeInterruptSvc,
      relayon
    );
*/


void ticker_ACS712_mqtt (void* relaySender) {
  if (relaySender != nullptr) {
  Relay * rly;
  rly = static_cast<Relay *>(relaySender);
    if (rly->RelayConfParam->v_ACS_Active) {
        float variance =(abs(ACS_I_Current-old_acs_value));
        // Serial.printf("%6.*lf", 2, variance );
        if (true) {//(variance > 0.05){
           mqttClient.publish(rly->RelayConfParam->v_ACS_AMPS.c_str(), 2, true, String(ACS_I_Current).c_str());
        //  Serial.println(String("I = ") + ACS_I_Current + " A");
        }
        old_acs_value = ACS_I_Current;
  }
}
}


void ticker_ACS712_func (void* relaySender) {
  if (relaySender != nullptr) {
  Relay * rly;
  rly = static_cast<Relay *>(relaySender);
    if (rly->RelayConfParam->v_ACS_Active) {
        ACS_I_Current = sensor.getCurrentAC();
        // Serial.println(String("I = ") + ACS_I_Current + " A");
        if (ACS_I_Current > rly->RelayConfParam->v_Max_Current) {
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
        }
    }
  }
}


void ticker_relay_ttl_periodic_callback(void* relaySender){
  Serial.print(F("\n TTL Countdown: "));
  //uint32_t t = ticker_relay_ttl.periodscounter();
  if (relaySender != nullptr) {
    Relay * rly;
    rly = static_cast<Relay *>(relaySender);
    uint32_t t = rly->getRelayTTLperiodscounter();
    if (digitalRead(rly->getRelayPin() == HIGH)) {
      mqttClient.publish( rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, String(rly->RelayConfParam->v_ttl).c_str());
      mqttClient.publish( rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, (String(t).c_str()));
    }
  }
}


void ticker_relay_ttl_off (void* relaySender) {
  if (relaySender != nullptr) {
    Relay * rly;
    rly = static_cast<Relay *>(relaySender);
    rly->mdigitalWrite(rly->getRelayPin(),LOW);
  }
}


void onRelaychangeInterruptSvc(void* relaySender){
  Relay * rly;
  rly = static_cast<Relay *>(relaySender);

  if (rly->rchangedflag ) {
      rly->rchangedflag = false;

      if (rly->readrelay() == HIGH) {
        Serial.print(F("\n\n An interrupt *ON* has occurred."));
        if (rly->RelayConfParam->v_ttl > 0 ) {
          rly->start_ttl_timer();
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, ON);
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, ON);
      }

      if (digitalRead(rly->getRelayPin()) == LOW) {
        Serial.print(F("\n\n An interrupt *OFF* has occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl > 0 ) {
          mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF);
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, OFF);
      }
  } else {
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                            digitalRead(rly->getRelayPin()) == HIGH ? ON : OFF);
  }
}


void process_Input(void * inputSender, void * obj){
  Serial.print("\n process_Input");
  if (inputSender != nullptr) {
      InputSensor * snsr;
      snsr = static_cast<InputSensor *>(inputSender);
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
  Serial.print("\n onchangeSwitchInterruptSvc");
  Relay * rly;
  rly = static_cast<Relay *>(relaySender);
  InputSensor * input;
  input = static_cast<InputSensor *>(inputSender);
  //if (rly->r_in_mode == INPUT_NORMAL) {} v_InputPin12_STATE_PUB_TOPIC
  Serial.print("\n onchangeSwitchInterruptSvc");
  //if (rly->r_in_mode == INPUT_COPY_TO_RELAY) {
  rly->mdigitalWrite(rly->getRelayPin(),digitalRead(input->pin)); //rly->getRelaySwithbtnState());
  //}
  mqttClient.publish(input->mqtt_topic.c_str(), QOS2, RETAINED,digitalRead(input->pin) == HIGH ? ON : OFF);
}


// this function will be called when button is clicked.
void buttonclick(void* relaySender, void* inputSender) {
  Serial.print("\n buttonclick");
  if (relaySender){
    Relay * rly;
    rly = static_cast<Relay *>(relaySender);
    InputSensor * input;
    input = static_cast<InputSensor *>(inputSender);
    Serial.print("\n buttonclick");
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


void relayloopservicefunc(void* relaySender){
if (relaySender){
  Relay * rly;
  rly = static_cast<Relay *>(relaySender);
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

DefineCalendarType(Calendar, 10);
Calendar MyCalendar;

void chronosInit() {
  MyCalendar.clear();
  PRINTLN(F("Starting up PointsEvents test"));
  Chronos::DateTime::setTime(year(), month(), day(), hour(), minute(), second());
  //Chronos::DateTime::setTime(2018, 12, 7, 18, 00, 00);

  uint8_t tcounter = 1;
  while(tcounter <= MAX_NUMBER_OF_TIMERS){ [&tcounter]() {

        config_read_error_t res = loadNodeTimer(
          [tcounter](){
            char  timerfilename[30] = "";
            strcpy(timerfilename, "/timer");
            strcat(timerfilename, String(tcounter).c_str());
            strcat(timerfilename, ".json");
            Serial.print (F("\n--------------------------"));
            Serial.print  (timerfilename);
            Serial.print("\n");
            return timerfilename;
          }()
        ,NTmr);

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

        /*  PRINTLN(F("\nFrom timer values: "));
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
    }();
  } // while loop

  LINE();
  PRINTLN(F("**** presumably got NTP time **** :"));
  Chronos::DateTime::now().printTo(SERIAL_DEVICE);
  LINE();
  Chronos::DateTime nowTime(Chronos::DateTime::now());
  PRINT(F("Right \"now\" it's: "));
  nowTime.printTo(SERIAL_DEVICE);
  LINES(2);
}

void chronosevaluatetimers(Calendar MyCalendar) {
  if (ftimesynced){
  // create an array of Event::Occurrence objects, to hold replies from the calendar
  Chronos::Event::Occurrence occurrenceList[10];
  // listOngoing: get events that are happening at specified datetime.  Called with
  // listOngoing(MAX_NUMBER_OF_EVENTS, INTO_THIS_ARRAY, AT_DATETIME)
  // It will return the number of events set in the INTO_THIS_ARRAY array.
  Chronos::DateTime nowTime(Chronos::DateTime::now());
  int numOngoing = MyCalendar.listOngoing(10, occurrenceList, nowTime);
  if (numOngoing) {
    // At least one event is happening at "nowTime"...
    LINE();
    //PRINTLN(F("**** Some things are going on this very minute! ****"));
    for (int i = 0; i < numOngoing; i++) {
      PRINT(F("**** Running Event: "));
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
            LINE();
            PRINTLN(F(" *** truning relay ON... event is Starting - TimerPaused value: *** "));
            PRINT(rly->timerpaused);
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
            PRINTLN(F(" *** truning relay OFF... event is done - TimerPaused value: ***"));
            PRINT(rly->timerpaused);
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


InputSensor Inputsnsr14(InputPin14,process_Input,INPUT_NONE);
InputSensor Inputsnsr12(InputPin12,process_Input,INPUT_NONE);
InputSensor Inputsnsr13(SwitchButtonPin2,process_Input,INPUT_NONE);

void Wifi_connect() {
  Serial.println(F("Starting WiFi"));
  relay1.loadrelayparams();

  Inputsnsr12.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);
  Inputsnsr14.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);
  Inputsnsr13.fclickmode = static_cast <input_mode>(MyConfParam.v_IN0_INPUTMODE);
  //relay2.loadrelayparams();

  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);

  delay(100);

    if (digitalRead(ConfigInputPin) == HIGH) {
                //WiFi.begin( getSsid.c_str() , getPass.c_str() ); // try to connect with saved SSID & PASS
                WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); // try to connect with saved SSID & PASS
                Serial.print("\n ssid: "); Serial.print(MyConfParam.v_ssid.c_str());
                Serial.print("\n pass: "); Serial.print(MyConfParam.v_pass.c_str());Serial.print("\n ");
                //  WiFi.begin( "ksbb" , "samsam12" ); // try to connect with saved SSID & PASS
                trials = 0;
              	 blinkInterval = 50;
                while ((WiFi.status() != WL_CONNECTED) & (trials < MaxWifiTrials*2)){
                //  while ((WiFi.waitForConnectResult() != WL_CONNECTED) & (trials < MaxWifiTrials)){
                    delay(50);
                		blinkled();
                    trials++;
                    Serial.print(F("-"));

                }
                if  (WiFi.status() == WL_CONNECTED)   {
                  Serial.println(F("WiFi Connected."));
                  IP_info();
                  SetAsyncHTTP();
                  // get time
                		blinkInterval = 1000;
                    Serial.print(F("IP number assigned by DHCP is "));
                    Serial.println(WiFi.localIP());
                    Serial.println(F("Starting UDP"));
                    Udp.begin(localPort);

                    #ifdef ESP8266
                      Serial.print(F("Local port: "));
                      Serial.println(Udp.localPort());
                      Serial.println(F("waiting for sync"));
                    #endif

                    if (!MDNS.begin((APssid+MyConfParam.v_PhyLoc).c_str())) {
                      Serial.println(F("Error setting up MDNS responder!"));
                    }
                    Serial.println(F("mDNS responder started"));
                    MDNS.addService(F("http"), F("tcp"), 80); // Announce esp tcp service on port 8080
                    MDNS.addServiceTxt(F("http"), F("tcp"),F("Pub Coil"), MyConfParam.v_PUB_TOPIC1.c_str());
                    MDNS.addServiceTxt(F("http"), F("tcp"),F("Pub Coil Status"), MyConfParam.v_STATE_PUB_TOPIC.c_str());
                    MDNS.addServiceTxt(F("http"), F("tcp"),F("MQTT server"), MyConfParam.v_MQTT_BROKER.toString().c_str());
                    MDNS.addServiceTxt(F("http"), F("tcp"),F("TTL"), String(MyConfParam.v_ttl).c_str());
                    MDNS.addServiceTxt(F("http"), F("tcp"),F("Max Allowed Current"), String(MyConfParam.v_Max_Current).c_str());

                    //setSyncInterval(10);
                    setSyncProvider(getNtpTime);

                    trials = 0;
                    relay1.stop_ttl_timer();
                    relay1.setRelayTTT_Timer_Interval(relay1.RelayConfParam->v_ttl*1000);
                    ACS_Calibrate_Start(relay1,sensor);

                    //relay2.stop_ttl_timer();
                    //relay2.setRelayTTT_Timer_Interval(relay2.RelayConfParam->v_ttl.toInt()*1000);

                    connectToMqtt();

                		MBserver->begin();
                		MBserver->onClient(&handleNewClient, MBserver);

                    APModetimer_run_value = 0;

                    Inputsnsr14.post_mqtt = true;
                    Inputsnsr14.mqtt_topic = MyConfParam.v_InputPin14_STATE_PUB_TOPIC;

                    Inputsnsr12.post_mqtt = true;
                    Inputsnsr12.mqtt_topic = MyConfParam.v_InputPin12_STATE_PUB_TOPIC;

                    Inputsnsr13.post_mqtt = true;
                    Inputsnsr13.mqtt_topic = MyConfParam.v_TOGGLE_BTN_PUB_TOPIC;

                }  // wifi is connected
} // if (digitalRead(ConfigInputPin) == HIGH)

if (digitalRead(ConfigInputPin) == LOW){
	if ((WiFi.status() != WL_CONNECTED))	{
		startsoftAP();
	}
}
}


void setup() {
    pinMode ( led, OUTPUT );
    pinMode ( ConfigInputPin, INPUT_PULLUP );
    pinMode ( InputPin12, INPUT_PULLUP );
    pinMode ( InputPin14, INPUT_PULLUP );
    Serial.begin(115200);

    /*
       You only need to format SPIFFS the first time you run a
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

    WiFi.mode(WIFI_AP_STA);

		mqttClient.onConnect(onMqttConnect);
		mqttClient.onDisconnect(onMqttDisconnect);
		mqttClient.onSubscribe(onMqttSubscribe);
		mqttClient.onUnsubscribe(onMqttUnsubscribe);
		mqttClient.onMessage(onMqttMessage);
		mqttClient.onPublish(onMqttPublish);
		mqttClient.setServer(MyConfParam.v_MQTT_BROKER, MyConfParam.v_MQTT_B_PRT);

    mb.addCoil(LAMP1_COIL);
    mb.addCoil(LAMP2_COIL);

    Inputsnsr13.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
    Inputsnsr13.onInputClick_RelayServiceRoutine = buttonclick;

    Inputsnsr12.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
    Inputsnsr12.onInputClick_RelayServiceRoutine = buttonclick;

    Inputsnsr14.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
    Inputsnsr14.onInputClick_RelayServiceRoutine = buttonclick;
    //Inputsnsr13.addrelay(&relay1);

    inputs.push_back(&Inputsnsr13);
    inputs.push_back(&Inputsnsr12);
    inputs.push_back(&Inputsnsr14);

    /*
    InputSensor * t;
    t = static_cast<InputSensor *>(inputs.at(0));
    t->addrelay(&relay1);
    t = static_cast<InputSensor *>(inputs.at(1));
    t->addrelay(&relay1);
    */

    relay1.attachLoopfunc(relayloopservicefunc);
    relays.push_back(&relay1);

  //  applyIRMAp(0,0);
  //  applyIRMAp(1,0);

  while (loadIRMapConfig(myIRMap) != SUCCESS){
    delay(2000);
    ESP.restart();
  };

    // mrelays[0]=&relay1;
    // attachInterrupt(digitalPinToInterrupt(relay2.getRelayPin()), handleInterrupt2, RISING );
    /*
    relay2.attachSwithchButton(SwitchButtonPin2, onchangeSwitchInterruptSvc, buttonclick);
    relay2.attachLoopfunc(relayloopservicefunc);
    relays.push_back(&relay2);
    */
    //mrelays[1]=&relay2;
    // attachInterrupt(digitalPinToInterrupt(InputPin14), InputPin14_handleInterrupt, CHANGE );
}


void loop() {

  if  (WiFi.status() != WL_CONNECTED)  {
    if (APModetimer_run_value == 0) Wifi_connect();
  }

 	blinkled();
  tiker_MQTT_CONNECT.update(nullptr);
  relay1.watch();
  //relay2.watch();

  Inputsnsr14.watch();
  Inputsnsr12.watch();
  Inputsnsr13.watch();

  if (restartRequired){
    Serial.printf("Restarting ESP\n\r");
    restartRequired = false;
    delay(2500);
    ESP.restart();
  }

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

  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    if (wifimode == WIFI_AP_MODE) {
  		APModetimer_run_value++;
  		if (APModetimer_run_value == APModetimer) {
        APModetimer_run_value = 0;
        ESP.restart();
      }
    }
  }

}
