
// #define USEPREF
#include <Arduino.h>
#include <string.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include "ACS712.h"
#include "OneButton.h"
#include "Debouncer.h"
#include <RelayClass.h>
#include <vector>
#include <ACS_Helper.h>
//#include <RelaysArray.h>

//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays

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
    /*
    extern "C" {
  	#include "freertos/FreeRTOS.h"
  	#include "freertos/timers.h"
    }*/
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
const char * EventNames[] = {
  "N/A", // just a placeholder, for indexing easily
  "Project Meeting   ",
  "Daily Workout     ",
  "Looong Meeting    ",
  "Yoga Class--uummm ",
  "New Year's eve!!!!",
  "** Anniv dinner **",
  "Council of Elders ",
  NULL
};

volatile byte interruptCounter = 0;
volatile byte interruptCounter2 = 0;
volatile byte SwitchButtonPin_interruptCounter=0;
volatile byte InputPin14_interruptCounter=0;
int numberOfInterrupts = 0;

//#include <KSBWebHelper.h>
/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
// #define FORMAT_SPIFFS_IF_FAILED true; // defined in FSFunctions

#define SERIAL_DEVICE     Serial

// a few defines to abstract away the Serial-specific stuff
#define PRINT(...)    SERIAL_DEVICE.print(__VA_ARGS__)
#define PRINTLN(...)  SERIAL_DEVICE.println(__VA_ARGS__)
#define LINE()    PRINTLN(' ')
#define LINES(n)  for (uint8_t _bl=0; _bl<n; _bl++) { PRINTLN(' '); }


#ifdef ESP32
    #ifdef USEPREF
    Preferences preferences;
    #endif
#endif
#define WIFI_AP_MODE 1
#define WIFI_CLT_MODE 0
#define DEFAULT_BOUNCE_TIME 100

Debouncer debouncer(DEFAULT_BOUNCE_TIME);

File file;

long timezone = 1;
byte daysavetime = 1;
int wifimode = WIFI_CLT_MODE;
const int led = 02;
String MAC;

//String PrefSSID, PrefPassword;  // used by preferences storage
unsigned long lastMillis = 0;

float trials = 0;
char buf[100];

int  WFstatus;
int UpCount = 0;
int32_t rssi;           // store WiFi signal strength here
String getSsid;
String getPass;

WiFiClient net;
//WiFiUDP wifiUdp;
//NTP ntp(wifiUdp);

String APssid = (String("Node-") +  CID()+"-");
const char* APpassword = "12345678";

int ledState = LOW;             					// ledState used to set the LED
unsigned long previousMillis = 0;         // will store last time LED was updated
long blinkInterval = 1000;           		      // blinkInterval at which to blink (milliseconds)
long APModetimer = 60*5;
long APModetimer_run_value = 0;
AsyncServer* MBserver = new AsyncServer(502); // start listening on tcp port 7050
ModbusIP mb;
const int LAMP1_COIL = 1;
const int LAMP2_COIL = 2;

//OneButton button(SwitchButtonPin, true);

float old_acs_value = 0;
float ACS_I_Current = 0;
ACS712 sensor(ACS712_20A, A0);

void ticker_relay_ttl_off (void* obj) ;
void ticker_relay_ttl_periodic_callback(void* obj);
void ticker_ACS712_func (void* obj);
void onRelaychangeInterruptSvc(void* t);
void ticker_ACS712_mqtt (void* obj);

//std::vector<Relay*> v; // a list to hold all clients

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

void ticker_relay_ttl_off (void* obj);

void ticker_ACS712_mqtt (void* obj) {
  if (obj != NULL) {
  Relay * rly;
  rly = static_cast<Relay *>(obj);
    if (rly->RelayConfParam->v_ACS_Active == "1") {
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

void ticker_ACS712_func (void* obj) {
  if (obj != NULL) {
  Relay * rly;
  rly = static_cast<Relay *>(obj);
    if (rly->RelayConfParam->v_ACS_Active == "1") {
        ACS_I_Current = sensor.getCurrentAC();
        // Serial.println(String("I = ") + ACS_I_Current + " A");
        if (ACS_I_Current > rly->RelayConfParam->v_Max_Current.toInt()) {
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
            // uint16_t packetIdPub = mqttClient.publish(MyConfParam.v_STATE_PUB_TOPIC.c_str(), 2, true, OFF); moved to interrupt
        }
    }
  }
}


void ticker_relay_ttl_periodic_callback(void* obj){
  Serial.print(F("\n TTL Countdown: "));
  //uint32_t t = ticker_relay_ttl.periodscounter();
  if (obj != NULL) {
    Relay * rly;
    rly = static_cast<Relay *>(obj);
    uint32_t t = rly->getRelayTTLperiodscounter();
    if (digitalRead(rly->getRelayPin() == HIGH)) {
      mqttClient.publish( rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, rly->RelayConfParam->v_ttl.c_str());
      mqttClient.publish( rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, false, (String(t).c_str()));
    }
  }
}


void ticker_relay_ttl_off (void* obj) {
  if (obj != NULL) {
    Relay * rly;
    rly = static_cast<Relay *>(obj);
    rly->mdigitalWrite(rly->getRelayPin(),LOW);
  }
}


void onRelaychangeInterruptSvc(void* t){
/*  if((interruptCounter > 0) or (interruptCounter2 > 0)){
      if (interruptCounter > 0) interruptCounter--;
      if (interruptCounter2 > 0) interruptCounter2--;
      */

      Relay * rly;
      rly = static_cast<Relay *>(t);

      if (rly->rchangedflag ) {
        rly->rchangedflag = false;

      if (rly->readrelay() == HIGH) {
        Serial.print(F("\n\n An interrupt *ON* has occurred."));
        if (rly->RelayConfParam->v_ttl.toInt() > 0 ) {
          rly->start_ttl_timer();
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, "on");
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, "on");
      }

      if (digitalRead(rly->getRelayPin()) == LOW) {
        Serial.print(F("\n\n An interrupt *OFF* has occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl.toInt() > 0 ) {
          mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
        }
        mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, "off");
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, "off");
      }
  }
}


void onchangeSwitchInterruptSvc(void* t){
if (true){ //}(SwitchButtonPin_interruptCounter > 0) {
  //SwitchButtonPin_interruptCounter--;
  Relay * rly;
  rly = static_cast<Relay *>(t);
  if ((rly->RelayConfParam->v_GPIO12_TOG == "0") && (rly->RelayConfParam->v_Copy_IO == "0")) {
    char* msg;
      rly->getRelaySwithbtnState() == HIGH ? msg = ON : msg = OFF;
      mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, msg);
      Serial.print("\npublished1");
      mqttClient.publish(MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, msg);
      Serial.print("\npublished2");
  }
  if ((rly->RelayConfParam->v_Copy_IO == "1")  && (rly->RelayConfParam->v_GPIO12_TOG == "0")) {
      rly->mdigitalWrite(rly->getRelayPin(),rly->getRelaySwithbtnState());
  }
}
}


// this function will be called when the button was pressed.
void buttonclick(void* sender) {
  if (sender){
  Relay * rly;
  rly = static_cast<Relay *>(sender);

  if ((rly->RelayConfParam->v_GPIO12_TOG == "1") && (rly->RelayConfParam->v_Copy_IO == "0"))  {
    if (rly->readrelay() == HIGH) {
      rly->ticker_relay_tta->stop();
      rly->mdigitalWrite(rly->getRelayPin(), LOW);
      rly->stop_ttl_timer();
    } else {
    rly->ticker_relay_tta->interval(rly->RelayConfParam->v_tta.toInt()*1000);
    rly->ticker_relay_tta->start();
    }
  }
}
}

void relayloopservicefunc(void* sender){
if (sender){
  Relay * rly;
  rly = static_cast<Relay *>(sender);
    if (rly->TTLstate() != RUNNING_) {
      if (digitalRead(rly->getRelayPin()) == HIGH) {
        if (rly->RelayConfParam->v_ttl.toInt() > 0 ) rly->start_ttl_timer(); // ticker_relay_ttl.start();
      }
    }
  }
}


void IP_info()
{
   getSsid = WiFi.SSID();
   getPass = WiFi.psk();
   #ifdef ESP32
     wifi_config_t conf;
     esp_wifi_get_config(WIFI_IF_STA, &conf);  // load wifi settings to struct comf
   #endif

      MAC = WiFi.macAddress();
      Serial.printf( "\n\n\tSSID\t%s, ", getSsid.c_str() );
      Serial.print( rssi);
			Serial.printf(" dBm\n" );  // printf??
      Serial.printf( "\tPass:\t %s\n", getPass.c_str() );
      Serial.print( F("\n\n\tIP address:\t") );
			Serial.print(WiFi.localIP() );
      Serial.print(F(" / "));
      Serial.println( WiFi.subnetMask() );
      Serial.print( F("\tGateway IP:\t") );  Serial.println( WiFi.gatewayIP() );
      Serial.print( F("\t1st DNS:\t") );     Serial.println( WiFi.dnsIP() );
      Serial.printf( "\tMAC:\t\t%s\n", MAC.c_str() );
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
static void handleError(void* arg, AsyncClient* client, int8_t error) {
 Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
 Serial.printf("\n Modbus query received from client %s \n", client->remoteIP().toString().c_str());
 mb.task(client, data, len);
 Relay * t = NULL;
 t = getrelaybypin(RelayPin);
 if (t) t->mdigitalWrite(RelayPin, mb.Coil(LAMP1_COIL));
 String pld = (mb.Coil(LAMP1_COIL) == 0) ? OFF : ON;
}

static void handleDisconnect(void* arg, AsyncClient* client) {
 Serial.printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
 Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

/* server events */
static void handleNewClient(void* arg, AsyncClient* client) {
   Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());
   // register events
   client->onData(&handleData, NULL);
   client->onError(&handleError, NULL);
   client->onDisconnect(&handleDisconnect, NULL);
   client->onTimeout(&handleTimeOut, NULL);
}
//-------------------------------------------------------------------------------------------------------------

DefineCalendarType(Calendar, 10);
Calendar MyCalendar;

void chronosInit() {
  PRINTLN(F("Starting up PointsEvents test"));
  Chronos::DateTime::setTime(year(), month(), day(), hour(), minute(), second());
  //Chronos::DateTime::setTime(2018, 11, 1, 10, 00, 00);

  MyCalendar.add(
      Chronos::Event(4,Chronos::Mark::Weekly(Chronos::Weekday::Thursday, 10, 00, 05), Chronos::Span::Seconds(15))
    );
  LINE();

  PRINTLN(F(" **** presumably got NTP time **** :"));
  Chronos::DateTime::now().printTo(SERIAL_DEVICE);
  LINE();
  Chronos::DateTime nowTime(Chronos::DateTime::now());
  PRINT(F("Right \"now\" it's: "));

  nowTime.printTo(SERIAL_DEVICE);
  LINES(2);

}

void chronosevaluatetimers(Calendar MyCalendar) {
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
    PRINTLN(F("**** Some things are going on this very minute! ****"));
    for (int i = 0; i < numOngoing; i++) {
      PRINT(F("**** Event: "));
      PRINT((int )occurrenceList[i].id);
      PRINT('\t');
      PRINT(EventNames[occurrenceList[i].id]);
      PRINT(F("\tends in: "));
      (nowTime - occurrenceList[i].finish).printTo(SERIAL_DEVICE);

      if ((nowTime == occurrenceList[i].start + 1)) {
        LINE();
        PRINTLN(F(" *** truning relay ON... event is Starting *** "));
      }
      if ((nowTime == occurrenceList[i].finish - 1)) {
        LINE();
        PRINTLN(F(" *** truning relay OFF... event is done ***"));
      }

    }
  } else {
  //  PRINTLN(F("Looks like we're free for the moment..."));
  }

}



void Wifi_connect() {
  Serial.println(F("Starting WiFi"));
  #ifdef USEPREF
    ReadParams(MyConfParam, preferences);
  #endif
  while (loadConfig(MyConfParam) != SUCCESS){
    delay(2000);
    ESP.restart();
  };
	String getSsid = MyConfParam.v_ssid;
  String getPass = MyConfParam.v_pass;

  relay1.loadrelayparams();
  //relay2.loadrelayparams();

  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  delay(100);

    if (digitalRead(ConfigInputPin) == HIGH) {
                WiFi.begin( getSsid.c_str() , getPass.c_str() ); // try to connect with saved SSID & PASS
                //  WiFi.begin( "ksbb" , "samsam12" ); // try to connect with saved SSID & PASS
                trials = 0;
              	 blinkInterval = 50;
                while ((WiFi.status() != WL_CONNECTED) & (trials < MaxWifiTrials)){
                //  while ((WiFi.waitForConnectResult() != WL_CONNECTED) & (trials < MaxWifiTrials)){
                    delay(250);
                		blinkled();
                    trials++;
                    Serial.print(".");
                    // yield();
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
                    MDNS.addService("http", "tcp", 80); // Announce esp tcp service on port 8080
                    MDNS.addServiceTxt("http", "tcp",F("Pub Coil"), MyConfParam.v_PUB_TOPIC1.c_str());
                    MDNS.addServiceTxt("http", "tcp",F("Pub Coil Status"), MyConfParam.v_STATE_PUB_TOPIC.c_str());
                    MDNS.addServiceTxt("http", "tcp",F("MQTT server"), MyConfParam.v_MQTT_BROKER.c_str());
                    MDNS.addServiceTxt("http", "tcp",F("TTL"), MyConfParam.v_ttl.c_str());
                    MDNS.addServiceTxt("http", "tcp",F("Max Allowed Current"), MyConfParam.v_Max_Current.c_str());

                    //setSyncInterval(10);
                    setSyncProvider(getNtpTime);

                    chronosInit();

                    trials = 0;
                    relay1.stop_ttl_timer();
                    relay1.setRelayTTT_Timer_Interval(relay1.RelayConfParam->v_ttl.toInt()*1000);
                    ACS_Calibrate_Start(relay1,sensor);

                    //relay2.stop_ttl_timer();
                    //relay2.setRelayTTT_Timer_Interval(relay2.RelayConfParam->v_ttl.toInt()*1000);

                    connectToMqtt();

                		MBserver->begin();
                		MBserver->onClient(&handleNewClient, MBserver);

                    APModetimer_run_value = 0;

                }  // wifi is connected
} // if (digitalRead(ConfigInputPin) == HIGH)

if (digitalRead(ConfigInputPin) == LOW){
	if ((WiFi.status() != WL_CONNECTED))	{
		startsoftAP();
	}
}
}

/*
void handleInterrupt() {
  interruptCounter++;
}

void handleInterrupt2() {
  Serial.print("\n******  interrupt r2 ******");
  interruptCounter2++;
}
*/

void SwitchButtonPin_handleInterrupt() {
  SwitchButtonPin_interruptCounter++;
}

void InputPin14_handleInterrupt() {
    InputPin14_interruptCounter++;
}

void setup() {
    pinMode ( led, OUTPUT );
    pinMode ( ConfigInputPin, INPUT_PULLUP );
    pinMode ( InputPin12, INPUT_PULLUP );
    pinMode ( InputPin14, INPUT_PULLUP );
    Serial.begin(115200);

    /* You only need to format SPIFFS the first time you run a
       test or else use the SPIFFS plugin to create a partition
       https://github.com/me-no-dev/arduino-esp32fs-plugin */

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
    wifimode = WIFI_CLT_MODE;
    WiFi.mode(WIFI_AP_STA);

		mqttClient.onConnect(onMqttConnect);
		mqttClient.onDisconnect(onMqttDisconnect);
		mqttClient.onSubscribe(onMqttSubscribe);
		mqttClient.onUnsubscribe(onMqttUnsubscribe);
		mqttClient.onMessage(onMqttMessage);
		mqttClient.onPublish(onMqttPublish);
		mqttClient.setServer(MyConfParam.v_MQTT_BROKER.c_str(), MQTT_PORT);

    mb.addCoil(LAMP1_COIL);
    mb.addCoil(LAMP2_COIL);

  //  attachInterrupt(digitalPinToInterrupt(relay1.getRelayPin()), handleInterrupt, CHANGE );
    relay1.attachSwithchButton(SwitchButtonPin2,
                            //  SwitchButtonPin_handleInterrupt,
                              onchangeSwitchInterruptSvc,  // for input mode and copy to relay ,ode
                              buttonclick);                // for toggle mode

    relay1.attachLoopfunc(relayloopservicefunc);
    relays.push_back(&relay1);
    //mrelays[0]=&relay1;

  //  attachInterrupt(digitalPinToInterrupt(relay2.getRelayPin()), handleInterrupt2, RISING );
    /*
    relay2.attachSwithchButton(SwitchButtonPin2, SwitchButtonPin_handleInterrupt, onchangeSwitchInterruptSvc, buttonclick);
    relay2.attachLoopfunc(relayloopservicefunc);
    relays.push_back(&relay2);
    */
    //mrelays[1]=&relay2;

    attachInterrupt(digitalPinToInterrupt(InputPin14), InputPin14_handleInterrupt, CHANGE );
}


void loop() {

  if  (WiFi.status() != WL_CONNECTED)  {
    if (APModetimer_run_value == 0) Wifi_connect();
  }

 	blinkled();
  tiker_MQTT_CONNECT.update(NULL);
  relay1.watch();
  //relay2.watch();

  if (restartRequired){  // check the flag here to determine if a restart is required
    Serial.printf("Restarting ESP\n\r");
    restartRequired = false;
    delay(2500);
    ESP.restart();
  }

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      chronosevaluatetimers(MyCalendar);
    }
  }

  if (InputPin14_interruptCounter > 0) {
    InputPin14_interruptCounter--;
    char* msg;
    uint8_t  InputPin14State = digitalRead(InputPin14);
    bool stateChanged = debouncer.update(InputPin14State);
    if (stateChanged) {
      digitalRead(InputPin14) == HIGH ? msg = ON : msg = OFF;
      mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, msg);
    }
    numberOfInterrupts++;
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
