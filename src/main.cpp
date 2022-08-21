// public time server 129.6.15.28
// #define USEPREF n
// V 1.0
// ESP 8266 and ESP32 arduino libraries and examples : https://github.com/espressif/arduino-esp32/tree/master/libraries

#include <defines.h>

#ifndef DEBUG_DISABLED
  #include <RemoteDebug.h>
  #define HOST_NAME "remotedebug"
  RemoteDebug Debug;
#endif 

#ifdef AppleHK
  #ifndef ESP32
    #include <AppleHKSupport.h>
  #endif
  #ifdef ESP32
    #include <ESP32HomeKit.h>
    #include <ESP32HAP.h>
  #endif
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
#ifdef ESP32
  #include <ESP32Ping.h>
#else
  #include "AsyncPing.h"
#endif

#ifdef ESP_MESH
    #include <ksbMesh.h>
    extern Scheduler userScheduler;   // to control your personal task
    extern painlessMesh  mesh;    
#endif


#ifdef INVERTERLINK
  #include <inverter.h> 
  #include <InverterHelper.h>
#endif

//#include <AH_EasyDriver.h>
#include <AccelStepper.h>
#include <SimpleTimer.h>


#ifdef emonlib
#include "EmonLib.h"             // Include Emon Library
  EnergyMonitor emon1;             // Create an instance  
  float realPower;
  float apparentPower;
  float powerFActor ;
  float supplyVoltage;
  float Irms ;  
#endif  

#ifdef OLED_1306
#include <SSD1306.h>
#endif

#ifdef blockingTime
  #include <KSBNTP.h>
#else
  #include <KSBAsyncNTP.h>
  AsyncUDP Audp;
#endif

bool esp_now_initiated = false;
bool mesh_active = false;

//#include <RelaysArray.h>
//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays
extern std::vector<void *> inputs ; // a list to hold all relays
extern void applyIRMAp(uint8_t Inpn, uint8_t rlyn);

extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
	// #include "freertos/FreeRTOS.h"
	// #include "freertos/timers.h"  
}

// TimerHandle_t mqttReconnectTimer;
// TimerHandle_t wifiReconnectTimer;

char HAName_Bridge[HK_name_len]  = "MyBridge_____\0";
char HAName_SW[HK_name_len]      = "MySwitch_____\0";

#ifdef HWver03_4R
  char HAName_SW1[HK_name_len]      = "MySwitch1_____\0";
  char HAName_SW2[HK_name_len]      = "MySwitch2_____\0";
  char HAName_SW3[HK_name_len]      = "MySwitch3_____\0";
#endif

#ifndef ESP32
extern "C"
{
  #include <homekit/types.h> // needed for icmp packet definitions
}
#endif
/*
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
*/

#include <KSBWiFiHelper.h>

long blinkInterval ;                          // blinkInterval at which to blink (milliseconds)
unsigned long lastMillis_2;
int ledState = LOW;             					    // ledState used to set the LED
unsigned long previousMillis = 0;             // will store last time LED was updated
unsigned long lastMillis = 0;
unsigned long lastMillis_1 = 0;
unsigned long lastMillis5000 = 0;

unsigned long switch_millis = 0;

unsigned long mqttstartUpdatemillis = 0;

#ifdef ESP32
  #include <ESPmDNS.h>
    #ifdef USEPREF
        #include <Preferences.h>
        #include "KSBPreferences.h"
    #endif
  #include <FSFunctions.h>
  #include <AsyncTCP.h>
  #include <update.h>
#else
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
#endif

#include <DNSServer.h>
//#include "NTP.h"
#include <time.h>
#include <MQTT_Processes.h>
#include <AsyncHTTP_Helper.h>

bool homekitNotInitialised = true;
bool HomespanInitiated = false;

time_t prevDisplay = 0; // when the digital clock was displayed
#include <Chronos.h>

NodeTimer NTmr(4);
TempConfig PTempConfig(1);

#ifndef ESP32
AsyncPing Pings; //Pings[1]; 
#endif

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
      long duration, cm, inches;
      double emptypercent,fullpercent;         
      #include <NewPing.h>
      #define MAX_DISTANCE 250 
      NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);  
      
      //unsigned int pingSpeed = 50; // How frequently are we going to send out a ping (in milliseconds). 50ms would be 20 times a second.
      //unsigned long pingTimer;     // Holds the next ping time.        
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
byte xtries = 0;
byte daysavetime  = 1;
int wifimode      = WIFI_CLT_MODE;

String MAC;

int restartRequired_counter = 0;

uint32_t trials   = 0;
int  WFstatus;
int UpCount       = 0;
WiFiClient net;
String APssid     = (String("Node-") +  CID() + "-");
const char* APpassword = "12345678";
       		      
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

#if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
  static TempSensor tempsensor(TempSensorPin);
  static TempSensor TempSensorSecond(SecondTempSensorPin);
#endif

ACS712 sensor(ACS712_20A, A0);

void ticker_relay_ttl_off (void* obj) ;
void ticker_relay_ttl_periodic_callback(void* obj);
void ticker_ACS712_func (void* obj);
void onRelaychangeInterruptSvc(void* t);
void ticker_ACS712_mqtt (void* obj);


#ifdef ESP_NOW
//************** ESP_NOW ******************* 
// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char a[100];
    bool SwitchState;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback function that will be executed when data is received
    #ifndef ESP32
        void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
          memcpy(&myData, incomingData, sizeof(myData));
          Serial.print("\n[ESP_NOW] Bytes received: ");
          Serial.print(len);
          Serial.print(" Char: ");
          Serial.print(myData.a);
          Serial.print(" SwitchState: ");
          Serial.print(myData.SwitchState);
          Serial.println();
          Serial.println("[ESP_NOW] posting update");
            Relay * rtmp =  getrelaybynumber(0);
            if (rtmp) rtmp->mdigitalWrite(rtmp->getRelayPin(),myData.SwitchState);
        }
    #endif
    #ifdef ESP32
        void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
          memcpy(&myData, incomingData, sizeof(myData));
          Serial.print("\n[ESP_NOW] Bytes received: ");
          Serial.print(len);
          Serial.print(" Char: ");
          Serial.print(myData.a);
          Serial.print(" SwitchState: ");
          Serial.print(myData.SwitchState);
          Serial.println();
          Serial.println("[ESP_NOW] posting update");
            Relay * rtmp =  getrelaybynumber(0);
            if (rtmp) rtmp->mdigitalWrite(rtmp->getRelayPin(),myData.SwitchState);
        }
    #endif

 #endif

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
            Serial.println(F("Reactivating relay after TTA time"));
            rly->ticker_relay_tta->interval(rly->RelayConfParam->v_tta*1000);
            rly->ticker_relay_tta->start();  

        }
    }
  }
}


void ticker_relay_ttl_periodic_callback(void* relaySender){

  //uint32_t t = ticker_relay_ttl.periodscounter();
  if (relaySender != nullptr) {
    Relay * rly = static_cast<Relay *>(relaySender);
    uint32_t t = rly->getRelayTTLperiodscounter();
    Serial.print(F("[INFO   ] TTL Countdown: "));    
    Serial.println(t);    
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
  // uint16_t pack = 0;
  //if (mqttClient.connected()) { xxxx
  if (rly->rchangedflag ) {
      rly->rchangedflag = false;

      if (rly->readrelay() == HIGH) {
        Serial.println(F("[INFO   ] interrupt *ON* occurred."));
        if (rly->RelayConfParam->v_ttl > 0 ) {
          rly->start_ttl_timer();
        }
        //mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, ON);   //xxx check why this is needed
        // Serial.print(rly->RelayConfParam->v_STATE_PUB_TOPIC);
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, ON);         
      }

      if (digitalRead(rly->getRelayPin()) == LOW) {
        Serial.println(F("[INFO   ] interrupt *OFF* occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl > 0 ) {
          mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
        }
        //mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF);   //xxx check why this is needed
        // Serial.print(rly->RelayConfParam->v_STATE_PUB_TOPIC);        
        mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, OFF);        
      }

  } else {
       mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, digitalRead(rly->getRelayPin()) == HIGH ? ON : OFF);                
  }
  //}
  #ifdef AppleHK
    #ifndef ESP32
      //if (HomeKitt_PIN_SWITCH == rly->getRelayPin()) {
        rly->savePersistedrelay();
        switch (rly->getRelayPin()) {
          case RelayPin:
            cha_switch_on.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
            homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);    
            break;
          #ifdef HWver03_4R  
            case Relay1Pin:
              cha_switch_on1.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
              homekit_characteristic_notify(&cha_switch_on1, cha_switch_on1.value);    
              break;
            case Relay2Pin:
              cha_switch_on2.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
              homekit_characteristic_notify(&cha_switch_on2, cha_switch_on2.value);   
              break;
            case Relay3Pin:
              cha_switch_on3.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
              homekit_characteristic_notify(&cha_switch_on3, cha_switch_on3.value);   
              break;   
          #endif        
        }
    #endif
    #ifdef ESP32

    #endif     
  #endif

}


void process_Input(void * inputSender, void * obj){
  Serial.println(F("[INFO   ] processing Inputs"));
  if (inputSender != nullptr) {
      InputSensor * snsr = static_cast<InputSensor *>(inputSender);
    if (snsr->fclickmode == INPUT_NORMAL) {
      Serial.println(("[MQTT   ]" + snsr->mqtt_topic).c_str());
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
  Serial.print(F("\n[INFO   ] TempertatureSensorEvent"));
  Relay * rtmp =  getrelaybynumber(0);
      Serial.print(F("\n[INFO  ********* ] TempertatureSensorEvent TSolarPanel = "));
      Serial.print (TSolarPanel);

      Serial.print(F("\n[INFO  ********* ] TempertatureSensorEvent TSolarTank = "));
      Serial.print (TSolarTank);

  //mqttClient.publish(input->mqtt_topic.c_str(), QOS2, RETAINED,TOG);
  if ((PTempConfig.spanTempfrom != 0) && (PTempConfig.spanBuffer != 0)) {
    if (TSolarPanel > PTempConfig.spanTempfrom) {
      Serial.print(F("\n[INFO   ] TempertatureSensorEvent SOLAR PANEL > "));
      Serial.print (PTempConfig.spanTempfrom);
      if ((TSolarPanel - TSolarTank) > PTempConfig.spanBuffer) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
          Serial.print(F("\n[INFO   ] TempertatureSensorEvent SOLAR PANEL - TankTemp > "));
          Serial.print (PTempConfig.spanBuffer);
      }
      if ((TSolarPanel - TSolarTank) < PTempConfig.spanBuffer) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
          Serial.print(F("\n[INFO   ] TempertatureSensorEvent SOLAR PANEL - TankTemp < "));
          Serial.print (PTempConfig.spanBuffer);
      }    
    }

    if (TSolarPanel < PTempConfig.spanTempfrom) {
        rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
          Serial.print(F("\n[INFO   ] TempertatureSensorEvent SOLAR PANEL < "));
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

      Serial.printf( "[WIFI   ] SSID: %s, ",  WiFi.SSID().c_str() );
      Serial.printf( "PASS: %s",  WiFi.psk().c_str() );
      Serial.print( F(" * IP address:") );
			Serial.print(WiFi.localIP() );
      /*
      Serial.print(F(" / "));
      Serial.println( WiFi.subnetMask() );
      Serial.print( F("\tGateway IP:\t") );  Serial.println( WiFi.gatewayIP() );
      Serial.print( F("\t1st DNS:\t") );     Serial.println( WiFi.dnsIP() );
      */
      long rssi = WiFi.RSSI();
      Serial.print(" * RSSI:");
      Serial.print(rssi);               
      Serial.printf( " * MAC:%s\n", MAC.c_str());
}


extern Schedule_timer Wifireconnecttimer;
extern void connectToMqtt();

void tiker_WIFI_CONNECT_func (void* obj) {
        Serial.println(F("\n[WIFI] WIFI timer active"));
        // Access Point mode configuration jumper is set
        if (digitalRead(ConfigInputPin) == LOW){ 
          Serial.print(F("\n[WIFI] Access Point mode configuration jumper is set"));                               
          Wifireconnecttimer.stop();
          WiFi.mode(WIFI_AP_STA);
     
        } else {
          // Station mode, try to connect with saved SSID & PASS
          // WiFi.reconnect();
          #ifndef ESP_MESH
            WiFi.mode(WIFI_STA); 
            WiFi.setSleep(WIFI_PS_NONE);
            //WiFi.disconnect(true);
            WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); 
            WiFi.printDiag(Serial); 
          #endif 
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

#ifdef AppleHK
#ifndef ESP32
void homekitUpdateBootStatus(void) {

 Relay * rly = nullptr;

  for (auto it : relays)  {
    rly = static_cast<Relay *>(it);
    if (rly) {
      switch (rly->getRelayPin()) {
        case RelayPin:
          cha_switch_on.value.bool_value = rly->readPersistedrelay(); //digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
          homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);    
          break;
        #ifdef HWver03_4R  
          case Relay1Pin:
            cha_switch_on1.value.bool_value = rly->readPersistedrelay() ; //digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
            homekit_characteristic_notify(&cha_switch_on1, cha_switch_on1.value);    
            break;
          case Relay2Pin:
            cha_switch_on2.value.bool_value = rly->readPersistedrelay(); // digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
            homekit_characteristic_notify(&cha_switch_on2, cha_switch_on2.value);   
            break;
          case Relay3Pin:
            cha_switch_on3.value.bool_value = rly->readPersistedrelay(); //digitalRead(rly->getRelayPin()) == HIGH ? true : false;	  //sync the value
            homekit_characteristic_notify(&cha_switch_on3, cha_switch_on3.value);   
            break;   
         #endif        
      }

    }
  }
}  
#endif
#endif


DefineCalendarType(Calendar, CAL_MAX_NUM_EVENTS_TO_HOLD);
Calendar MyCalendar;

void chronosInit() {
  MyCalendar.clear();
  PRINTLN(F("[Events ] Starting up PointsEvents test"));
  Chronos::DateTime::setTime(year(), month(), day(), hour(), minute(), second()); 
  //Chronos::DateTime::setTime(2018, 12, 7, 18, 00, 00);

  uint8_t tcounter = 1;
  #ifndef ESP32 
  ESP.wdtFeed();
  #endif
  while(tcounter <= MAX_NUMBER_OF_TIMERS) { [&tcounter]() 
    {
        #ifndef ESP32 
        ESP.wdtFeed();
        #endif
        char  timerfilename[30] = "";
        strcpy(timerfilename, "/timer");
        strcat(timerfilename, String(tcounter).c_str());
        strcat(timerfilename, ".json");

        config_read_error_t res = loadNodeTimer(timerfilename,NTmr);
        #ifndef ESP32 
        ESP.wdtFeed();
        #endif
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
  PRINT(F("[NTP    ] Presumably got NTP time,"));
  PRINT(F(" right \"now\" it's: "));
  Chronos::DateTime::now().printTo(SERIAL_DEVICE);
      
      #ifndef ESP32
      ESP.wdtFeed();
     #endif 
  Chronos::DateTime nowTime(Chronos::DateTime::now());
   //nowTime.printTo(SERIAL_DEVICE);

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
      PRINT(F("[INFO   ] Running Event: "));
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
            PRINTLN(F("\n[INFO   ] turning relay [ON]... event is Starting - TimerPaused value"));
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
            PRINTLN(F("[INFO   ]truning relay OFF... event is done - TimerPaused value"));
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

  #ifdef HWESP32
  InputSensor Inputsnsr01(InputPin01,process_Input,INPUT_NONE);
  InputSensor Inputsnsr02(InputPin02,process_Input,INPUT_NONE);     
  InputSensor Inputsnsr03(InputPin03,process_Input,INPUT_NONE); 
  InputSensor Inputsnsr04(InputPin04,process_Input,INPUT_NONE);
  InputSensor Inputsnsr05(InputPin05,process_Input,INPUT_NONE);     
  InputSensor Inputsnsr06(InputPin06,process_Input,INPUT_NONE); 
  #endif
#endif

//InputSensor Inputsnsr14(InputPin14,process_Input,INPUT_NONE);


void thingsTODO_on_WIFI_Connected() {
    Serial.println(F("\n[WIFI   ] WiFi Connected"));
    IP_info();

    blinkInterval = 1000;
    Serial.print(F("[WIFI   ] IP assigned by DHCP = "));
    Serial.println(WiFi.localIP());   

#ifdef OLED_1306
    display.println(WiFi.localIP().toString());
    display.display();
#endif    

    Serial.print(F("[WIFI   ] Channel: "));
    Serial.println(WiFi.channel());
    Serial.println(F("[INFO   ] Starting UDP"));

    // #ifdef ESP8266
    #ifdef blockingTime
      Udp.begin(localPort);
    #else
      // AsyncNTP
      Serial.print(F("[NTP    ] >> Time server: "));
      Serial.println(MyConfParam.v_timeserver);
      if (Audp.connect(MyConfParam.v_timeserver, NTP_REQUEST_PORT))
      {
        Serial.println(F("[NTP    ] >> Time server connected"));
        Audp.onPacket([](AsyncUDPPacket packet)
                      { parsePacket(packet); });
      }
    #endif

    Serial.println(F("[INFO   ] starting MDNS"));
    if (!MDNS.begin((MyConfParam.v_PhyLoc).c_str()))
    {
      Serial.println(F("[INFO   ] Error setting up MDNS responder!"));
    }

    Serial.println(F("[INFO   ] MDNS responder started"));
    MDNS.addService("http", "tcp", 80); // Announce esp tcp service on port 80

    // MDNS.addServiceTxt("http", "tcp","MQTT server", MyConfParam.v_MQTT_BROKER.toString().c_str());
    // MDNS.addServiceTxt("http", "tcp","Chip", String(MAC.c_str()) + " - Chip id: " + CID());

  #ifdef blockingTime
    setSyncProvider(getNtpTime);
  #else
    setSyncProvider(timeprovider);
    setSyncInterval(30);
  #endif

  #ifndef DEBUG_DISABLED
    debugV("* Starting MQTT connection timer, 5 seconds period");
  #endif

    // tiker_MQTT_CONNECT.start(); // timer will retry to connect every 5s.  
    SetAsyncHTTP();
    connectToMqtt();

    // MBserver->begin();
    // MBserver->onClient(&handleNewClient, MBserver);

    homekitNotInitialised = true;



}


#ifdef _commented_
void Wifi_connect()
{
  Serial.println(F("[INFO   ] Starting WiFi"));
  // WiFi.softAPdisconnect();
  // WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  if (digitalRead(ConfigInputPin) == HIGH)
  {
    WiFi.begin(MyConfParam.v_ssid.c_str(), MyConfParam.v_pass.c_str()); // try to connect with saved SSID & PASS
    Serial.print("[WIFI] ssid : ");
    Serial.print(MyConfParam.v_ssid.c_str());
    Serial.print(" - pass : ");
    Serial.println(MyConfParam.v_pass.c_str());

    trials = 0;
    blinkInterval = 50;
    // ksbwifi: WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() );
    while ((WiFi.status() != WL_CONNECTED) & (trials < MaxWifiTrials * 2))
    {
      //  while ((WiFi.waitForConnectResult() != WL_CONNECTED) & (trials < MaxWifiTrials)){
      delay(50);
      blinkled();
      trials++;
      Serial.print(F("|"));
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      thingsTODO_on_WIFI_Connected();
      trials = 0;
      APModetimer_run_value = 0;
    } // wifi is connected
  }   // if (digitalRead(ConfigInputPin) == HIGH)

  if (digitalRead(ConfigInputPin) == LOW)
  {
    Serial.println(F("[WIFI] Starting AP_STA mode"));
    WiFi.mode(WIFI_AP_STA);
    if ((WiFi.status() != WL_CONNECTED))
    {
      startsoftAP();
    }
  }
}
#endif

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

  #ifdef HWESP32
  Inputsnsr01.initialize(InputPin01,process_Input,INPUT_NONE);
  Inputsnsr01.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr01.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr01.post_mqtt = true;
  Inputsnsr01.mqtt_topic = MyConfParam.v_InputPin01_STATE_PUB_TOPIC; //+ "_1"; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr01.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);

  Inputsnsr02.initialize(InputPin02,process_Input,INPUT_NONE);
  Inputsnsr02.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr02.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr02.post_mqtt = true;
  Inputsnsr02.mqtt_topic = MyConfParam.v_InputPin02_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr02.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);

  Inputsnsr03.initialize(InputPin03,process_Input,INPUT_NONE);
  Inputsnsr03.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr03.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr03.post_mqtt = true;
  Inputsnsr03.mqtt_topic = MyConfParam.v_InputPin03_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr03.fclickmode = static_cast <input_mode>(MyConfParam.v_IN3_INPUTMODE);

  Inputsnsr04.initialize(InputPin04,process_Input,INPUT_NONE);
  Inputsnsr04.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr04.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr04.post_mqtt = true;
  Inputsnsr04.mqtt_topic = MyConfParam.v_InputPin04_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr04.fclickmode = static_cast <input_mode>(MyConfParam.v_IN4_INPUTMODE);

  Inputsnsr05.initialize(InputPin05,process_Input,INPUT_NONE);
  Inputsnsr05.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr05.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr05.post_mqtt = true;
  Inputsnsr05.mqtt_topic = MyConfParam.v_InputPin05_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr05.fclickmode = static_cast <input_mode>(MyConfParam.v_IN5_INPUTMODE);

  Inputsnsr06.initialize(InputPin06,process_Input,INPUT_NONE);
  Inputsnsr06.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
  Inputsnsr06.onInputClick_RelayServiceRoutine = buttonclick;
  Inputsnsr06.post_mqtt = true;
  Inputsnsr06.mqtt_topic = MyConfParam.v_InputPin06_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
  Inputsnsr06.fclickmode = static_cast <input_mode>(MyConfParam.v_IN6_INPUTMODE);

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

              Serial.print(F("\n[INFO   ] NEW Position: "));
              Serial.println(newPosition);
              Serial.print(F("\n[INFO   ] Current Position: "));
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

//  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
//  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

#ifdef emonlib
analogReadResolution()
  // emon1.voltage(A1, 382, 1.7);  // Voltage: input pin, calibration, phase_shift ** voltage constant = alternating mains voltage ÷ alternating voltage at ADC input pin (alternating voltage at ADC input pin = voltage at the middle point of the resistor voltage divider)
  emon1.current(A0, 30);       // Current: input pin, calibration.  *** the current constant is the value of current you want to read when 1 V is produced at the analogue input
#endif

    blinkInterval = 1000; 
    lastMillis_2 = 0;
    #ifndef ESP32
          Pings.on(true,[](const AsyncPingResponse& response){
            IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
            if (response.answer)
              Serial.printf("\n[PING   ] %d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n", response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
            else
              Serial.printf("\n[PING   ] no answer yet for %s icmp_seq=%d\n", addr.toString().c_str(), response.icmp_seq);
            return false; //do not stop
          });
          Pings.on(false,[](const AsyncPingResponse& response){
            IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
            Serial.printf("\n[PING   ] total answer from %s sent %d recevied %d time %d ms\n",addr.toString().c_str(),response.total_sent,response.total_recv,response.total_time);
            if (response.total_recv > 0) {
              restartRequired_counter = 0;
            }  else
            {
                  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
                    restartRequired_counter++;
                    //Serial.printf("\n\nPinging failure count: %i \n\n", restartRequired_counter);
                    Serial.print(F("\n[PING   ] Pinging failed count: "));
                    Serial.print(restartRequired_counter);
                    Serial.print("\n");
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
    #else

    #endif


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
        #ifdef HWESP32
        pinMode ( InputPin01, INPUT_PULLUP );
        pinMode ( InputPin02, INPUT_PULLUP );
        pinMode ( InputPin03, INPUT_PULLUP );
        pinMode ( InputPin04, INPUT_PULLUP );
        pinMode ( InputPin05, INPUT_PULLUP );
        pinMode ( InputPin06, INPUT_PULLUP );                                        
        #endif        
    #endif    

  Serial.begin(115200);
  #ifdef INVERTERLINK
  SERIAL_INVERTER.begin(2400);     // Using UART0 for comm with inverter. IE cant be connected during flashing
  #endif
  /*
  uint32_t Freq = 0;
  Freq = getCpuFrequencyMhz();
  Serial.print("CPU Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getXtalFrequencyMhz();
  Serial.print("XTAL Freq = ");
  Serial.print(Freq);
  Serial.println(" MHz");
  Freq = getApbFrequency();
  Serial.print("APB Freq = ");
  Serial.print(Freq);
  Serial.println(" Hz");    
  */

        // LedBlinker.start();

        /*
          only need to format SPIFFS the first time we run a
          test or else use the SPIFFS plugin to create a partition
          https://github.com/me-no-dev/arduino-esp32fs-plugin
        */
#ifdef OLED_1306
       SSD_1306();
       DSS1306_clear_text(0,0,1,"booting...");
#endif       

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

        while (loadConfig(MyConfParam) != SUCCESS) {
          delay(2000);
          ESP.restart();
        };
        
        //ScanMyWiFi();
        //delay(2000);

        WiFi.mode(WIFI_STA); 
        WiFi.setSleep(WIFI_PS_NONE);
            
        if (digitalRead(ConfigInputPin) == LOW){
          Serial.print(F("\n[WIFI   ] Sarting WIFI in AP mode \n"));
          WiFi.mode(WIFI_AP_STA);
        }

      //   String WiFiHostname = "ESP_" ; //+ CID();
      //   WiFiHostname += MyConfParam.v_PhyLoc.substring(0,28);
      //   WiFiHostname.replace(" ","");
      //   Serial.print(F("[WIFI   ] Setting WiFi Hostname to "));
      //   Serial.println(WiFiHostname);
      //   WiFi.setHostname(WiFiHostname.c_str());

        Serial.println(F("[WIFI   ] Starting WIFI in Station mode"));

        #ifndef ESP32
          #ifndef ESP_MESH 
            WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); // try to connect with saved SSID & PASS
          #else
              setup_mesh();
          #endif   
        #else
          Serial.println(F("[WIFI   ] assigning WIFI Events"));
          WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
          WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);  
          #ifndef ESP_MESH      
            WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); 
            WiFi.printDiag(Serial); 
          #endif
          #ifdef ESP_MESH
                setup_mesh();
          #endif          
        #endif

        mqttClient.setClientId(APssid.c_str());
        mqttClient.setCleanSession(true);
        mqttClient.setMaxTopicLength(256);
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onSubscribe(onMqttSubscribe);
        mqttClient.onUnsubscribe(onMqttUnsubscribe);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.onPublish(onMqttPublish);
         // mqttClient.setServer("test.mosquitto.org",1883);//  (MQTT_HOST, MQTT_PORT);
        mqttClient.setServer(MyConfParam.v_MQTT_BROKER.c_str(), MyConfParam.v_MQTT_B_PRT);
        // Serial.print("[MQTT   ] setting mqtt Will to: ");
        // static String ControllerIDWill =  relay0.RelayConfParam->v_LWILL_TOPIC; // "/home/Controller" + CID() +"/Will" ;
        // static String ControllerIDWill =  "/home/Controller" + CID() +"/Will" ;   
        // Serial.println(ControllerIDWill);
        // mqttClient.setWill(ControllerIDWill.c_str(), QOS2, false,"offline");
        mqttClient.setKeepAlive(10);
      
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

        #ifdef HWESP32 // order is important
        inputs.push_back(&Inputsnsr01); 
        inputs.push_back(&Inputsnsr02);      
        inputs.push_back(&Inputsnsr03);    
        inputs.push_back(&Inputsnsr04); 
        inputs.push_back(&Inputsnsr05);      
        inputs.push_back(&Inputsnsr06);                
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

  #ifdef emonlib
  emon1.calcVI(20,5000);         // Calculate all. No.of wavelengths, time-out
  emon1.serialprint();            

   realPower       = emon1.realPower;        //extract Real Power into variable
   apparentPower   = emon1.apparentPower;    //extract Apparent Power into variable
   powerFActor     = emon1.powerFactor;      //extract Power Factor into Variable
   supplyVoltage   = emon1.Vrms;             //extract Vrms into Variable
   Irms            = emon1.Irms;             //extract Irms into Variable
  #ifdef OLED_1306
  display.clearDisplay();
  DSS1306_clear_text(0,10,1,"current:");
  DSS1306_clear_text(48,10,1,Irms);
  DSS1306_clear_text(0,20,1,"voltage:");
  DSS1306_clear_text(48,20,1,supplyVoltage);
  display.display();  
  #endif

  #endif

  #ifdef ESP_MESH
  if ( mesh_active) {
    mesh.update();
  }
  #endif

  #ifdef AppleHKHomeSpan
  if (HomespanInitiated) {
    homeSpan.poll();
  }
  #endif 

  #ifndef DEBUG_DISABLED
  Debug.handle();
  yield();
  #endif

  #ifdef INVERTERLINK
    serviceInverter();
  #endif


    #ifndef ESP32
    ESP.wdtFeed();
    MDNS.update();
    #endif
    pinMode (ConfigInputPin, INPUT_PULLUP );

    if (restartRequired){
      Serial.println(F("\n[SYSTEM ] Restarting\n\r"));
      restartRequired = false;
      delay(2500);
      ESP.restart();
    }

  #ifdef ESP32
    if  (WiFi.status() != WL_CONNECTED)  {
      // if (APModetimer_run_value == 0) Wifi_connect();
      // Serial.println(F("[WIFI ] Disconnected"));
    }
  #endif

  #ifdef AppleHK
  #ifndef ESP32
	  my_homekit_loop();
  #endif  
  #endif   
  // delay(10);

  blinkled();


   #ifndef ESP_MESH
    tiker_MQTT_CONNECT.update(nullptr);  
    Wifireconnecttimer.update(nullptr);
   #else
    #ifdef ESP_MESH_ROOT
    tiker_MQTT_CONNECT.update(nullptr);  
    #endif
   #endif
  // LedBlinker.update(nullptr);

  for (auto it : relays)  {
    Relay * rtemp = static_cast<Relay *>(it);
    if (rtemp) {
        rtemp->watch();
    }
  }

  #ifndef StepperMode
    #if defined (HWver02)  || defined (HWver03)
    #if  defined (SolarHeaterControllerMode) || defined (SR04)
    #else
      Inputsnsr14.watch();
      Inputsnsr12.watch();
    #endif 
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

    #ifdef HWESP32
      Inputsnsr01.watch();
      Inputsnsr02.watch();     
      Inputsnsr03.watch();           
      Inputsnsr04.watch();
      Inputsnsr05.watch();     
      Inputsnsr06.watch();         
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

  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
    if (millis() - lastMillis_1 > 17000) {
      
      lastMillis_1 = millis();
      #ifndef ESP32
        Pings.begin(MyConfParam.v_Pingserver,1);
      #else
          bool success = Ping.ping(MyConfParam.v_Pingserver, 1);
          if(!success){
          Serial.println(F("[PING   ] Ping failed"));
                if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
                  restartRequired_counter++;
                  //Serial.printf("\n\nPinging failure count: %i \n\n", restartRequired_counter);
                  Serial.print(F("[PING   ] Pinging failed count: "));
                  Serial.println(restartRequired_counter);
                  if (restartRequired_counter > MyConfParam.v_Reboot_on_WIFI_Disconnection)  {
                    restartRequired = true;
                  }     
                }        
          } 
          else {
          Serial.println(F("[PING   ] Ping successful"));
          restartRequired_counter = 0;
          } 
      #endif
    }
  }

  if (millis() - lastMillis_2 > 2000) {
    lastMillis_2 = millis();
            #ifdef AppleHK
              #ifndef ESP32
                if((timeStatus() == timeSet) && homekitNotInitialised) {
                    // homekit_storage_reset();   
                    homekitNotInitialised = false;
                    String s = "Bridge_" + MyConfParam.v_PhyLoc;      
                    s.toCharArray(HAName_Bridge, 32);
                    MyConfParam.v_PhyLoc.toCharArray(HAName_SW, HK_name_len);   
                    #ifdef HWver03_4R
                      ((MyConfParam.v_PhyLoc) + "1").toCharArray(HAName_SW1, HK_name_len);  
                      ((MyConfParam.v_PhyLoc) + "2").toCharArray(HAName_SW2, HK_name_len);  
                      ((MyConfParam.v_PhyLoc) + "3").toCharArray(HAName_SW3, HK_name_len);  
                    #endif
                    my_homekit_setup();
                    homekitUpdateBootStatus();
                }     
              #endif  

                #ifdef ESP32
                if((timeStatus() == timeSet) && homekitNotInitialised) {                
                  setupHAP();
                  homekitNotInitialised = false;
                }
                #endif
        
            #endif 
  }


  if (millis() - lastMillis > 1000) {       // things to do every 1 second
    lastMillis = millis();

#ifdef OLED_1306
      display.display();
#endif      
		
    #ifdef SR04
    if (MyConfParam. v_Sonar_distance != "0") {
      pinMode(TRIG_PIN, OUTPUT);
      pinMode(ECHO_PIN, INPUT_PULLUP);
      cm = sonar.convert_cm(sonar.ping_median(10,300));
      Serial.print("[SONAR  ] Ping: ");
      Serial.print(cm); // Send ping, get distance in cm and print result (0 = outside set distance range)
      Serial.println("cm");


      // old method
          /*
          int trigPin = TRIG_PIN;    // Trigger
          int echoPin = ECHO_PIN;    // Echo
        
          pinMode(trigPin, OUTPUT);
          pinMode(echoPin, INPUT);

          digitalWrite(trigPin, LOW);
          delayMicroseconds(5);
          digitalWrite(trigPin, HIGH);
          delayMicroseconds(15);
          digitalWrite(trigPin, LOW);
    
          duration = pulseIn(echoPin, HIGH);
          // Serial.print(duration);
          pinMode(TRIG_PIN, INPUT_PULLUP);
          pinMode(ECHO_PIN, INPUT_PULLUP);      

          cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343
         // if (cm > MyConfParam.v_Sonar_distance_max) { cm = -1; }
          inches = (duration/2) / 74;   // Divide by 74 or multiply by 0.0135
          Serial.print("[Sonar  ------------------------------------------------------------------------------->>> ] Sonar distance: ");
          Serial.print(inches);
          Serial.print(F(" inches, "));
          Serial.print(cm);
          Serial.println(F(" cm"));
          */
         char res[8]; // Buffer big enough for 7-character float

         /*
         // uncomment this section if you want to post percentage full and percentage empty to mqtt broker
          emptypercent = 0;
          if (MyConfParam.v_Sonar_distance_max > 0) {
          emptypercent = (cm*100) / MyConfParam.v_Sonar_distance_max;
          } 
          fullpercent = (100 - emptypercent) ;

          
          dtostrf(emptypercent, 6, 1, res); // Leave room for too large numbers!     

          if (mqttClient.connected()) { mqttClient.publish((MyConfParam.v_Sonar_distance +"_empty%").c_str(), QOS2, RETAINED, res );}             

          dtostrf(fullpercent, 6, 1, res); // Leave room for too large numbers!   
          if (mqttClient.connected()) {mqttClient.publish((MyConfParam.v_Sonar_distance +"_full%").c_str(), QOS2, RETAINED, res );}         
        */

          dtostrf(cm, 6, 1, res); // Leave room for too large numbers!   
          if (mqttClient.connected()) {mqttClient.publish(MyConfParam.v_Sonar_distance.c_str(), QOS2, RETAINED, res );}
          
    }
    #endif 

    if (wifimode == WIFI_AP_MODE) {
  		APModetimer_run_value++;
      Serial.print(F("\n[WIFI   ] ApMode will restart after (seconds): "));
      Serial.print(APModetimer-APModetimer_run_value);
  		if (APModetimer_run_value == APModetimer) {
        APModetimer_run_value = 0;
        ESP.restart();
      }
    }
  }
  #endif


 #ifndef StepperMode
  #if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
  #ifndef SR04 // have to stop it because it uses the same pins as the temp sensors
  if (relay0.RelayConfParam->v_TemperatureValue != "0") {
    if (millis() - lastMillis5000 > 5000) {
    //  pinMode(TempSensorPin,  INPUT_PULLUP );  
    //  pinMode(SecondTempSensorPin,  INPUT_PULLUP );        
      lastMillis5000 = millis();
      tempsensor.getCurrentTemp(0); // this is assigned to tank
      xtries = 0;
      while ((xtries < 4) && (tempsensor.Celcius < 0)) {
        // delayMicroseconds(500);
        tempsensor.getCurrentTemp(0); // this is assigned to tank
        xtries ++; 
      }
      TempSensorSecond.getCurrentTemp(0);
      MCelcius = tempsensor.Celcius;      
      MCelcius2 = TempSensorSecond.Celcius; // this is assigned to panels

      float TSolarTank = roundf(tempsensor.Celcius);

      Serial.print(F("[INFO   ] Temperature Sensor #1: "));
      Serial.println(MCelcius); // tempsensor.getCurrentTemp(0));
      #ifndef DEBUG_DISABLED
        debugV("[INFO   ] TempSensor1 %.2f C ", TSolarTank);
      #endif
     
      char res[8]; // Buffer big enough for 7-character float
      dtostrf(MCelcius, 6, 1, res); // Leave room for too large numbers!
      mqttClient.publish(relay0.RelayConfParam->v_TemperatureValue.c_str(), QOS2, RETAINED,res); //String(MCelcius).c_str());

      #ifdef AppleHK
      #ifndef ESP32
        if (MCelcius < 0 ) { MCelcius = 99; }; 
        //cha_temperature.value.float_value = MCelcius;
        homekit_characteristic_notify(&cha_temperature, HOMEKIT_FLOAT(MCelcius)) ;// cha_temperature.value);
      #endif  
      #endif       

      float TSolarPanel = roundf(TempSensorSecond.Celcius);
      Serial.print(F("[INFO   ] Temperature Sensor #2: "));
      Serial.println(MCelcius2); //TempSensorSecond.getCurrentTemp(0)); 
      #ifndef DEBUG_DISABLED
        debugV("[INFO   ] TempSensor2 %.2f C ", TSolarPanel);
      #endif
      dtostrf(TSolarPanel, 6, 1, res); // Leave room for too large numbers!
      mqttClient.publish((relay0.RelayConfParam->v_TemperatureValue + "_2").c_str(), QOS2, RETAINED, res); // String(TSolarPanel).c_str());       

      #ifdef SolarHeaterControllerMode
      if (TSolarPanel > -100) {
        if (TSolarTank > -100) {
            TempertatureSensorEvent(0,TSolarPanel,TSolarTank);
        }    
      }      
      #else
      #endif

    }
  }
  #endif
  #endif 
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
