/// public time server 129.6.15.28
// #define USEPREF n
// SW V 10.0 @ 25/09/2022


#include <defines.h>
#include <Wire.h>
#ifdef ESP32
#include "esp_adc_cal.h"
#endif

#ifdef DEBUG_ENABLED
 // #include <RemoteDebug.h>
 // #define HOST_NAME "remotedebug"
 // RemoteDebug Debug;
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
#include <ADS11x5Config.h>
#include <HSTConfig.h>

#ifdef ESP32
  #include <ESP32Ping.h>
#else
 // #include "AsyncPing.h"
#endif

#ifdef ESP32
#ifdef WaterFlowSensor
#include <WaterFlowSensor.h>;
#endif
#endif

#include "Pings.h"
#ifndef ESP32
extern AsyncPing Pings; 
#endif

#ifdef ESP_MESH
    #include <ksbMesh.h>
    extern Scheduler userScheduler;   // to control your personal task
    extern painlessMesh  mesh;    
#endif

#ifdef _ALEXA_
#include "fauxmoESP.h"
  fauxmoESP fauxmo;
  bool Alexa_initialised = false;
#endif

#ifdef INVERTERLINK
  #include <inverter.h> 
  #include <InverterHelper.h>
#endif

#include <AccelStepper.h>
#include <SimpleTimer.h>


OneButton MYbutton(ConfigInputPin, true);
DisplayActions CURRENT_Display_Action = ACTION_DISPALY_1; // DISPLAY SCREEN 1 ON STARTUP

#ifndef ESP32
  #ifndef ESP_MESH
  #include <TaskScheduler.h>
  #endif
  Scheduler Scheduler_ts;
#endif



#ifdef ESP32
  #ifndef ESP_MESH
  #include <TaskScheduler.h>
  #endif
    //  #define _TASK_SLEEP_ON_IDLE_RUN
    //  #define _TASK_PRIORITY
    //  #define _TASK_WDT_IDS
    //  #define _TASK_TIMECRITICAL  
  Scheduler Scheduler_ts;
#endif


#ifdef _HST_
  HSTConfig PAHSTConfig(1);
#endif  

#ifdef _ADS1X15_

  ADS11x5Config PADS11x5Config(1);
  #ifndef _ADS_ASYNC_
  #include <Adafruit_ADS1X15.h>
    #ifdef _ADS1015_
      Adafruit_ADS1015 ads;
    #else
      Adafruit_ADS1115 ads;
    #endif
  #else
    #include "ADS1X15.h"
    #ifdef  _ADS1015_
      ADS1015 ADS1(0x48);
    #else
      ADS1115 ADS1(0x48);
    #endif
  #endif

  #ifdef _ADS1X15_DC_Current_ /* variable used for DC current routine*/
          /* 0- General */

          int decimalPrecision = 2;                   // decimal places for all values shown in LED Display & Serial Monitor

          /* 1- AC Current Measurement */

          int currentAnalogInputPin = 1 ;             // USE PIN 1 on ADS for current
          int calibrationPin = 0;                    // USE ADS PIN 0 for REF
          float manualOffset = 0;                      // Key in value to manually offset the initial value
          float mVperAmpValue = 12.5;                 // If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT
                                                      // For example, a 20A Hall-Effect Current Transformer rated at 20A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 20A = 31.25mV/A 
                                                      // For example, a 50A Hall-Effect Current Transformer rated at 50A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 50A = 12.5 mV/A
          float supplyVoltage = 6144;                 // Analog input pin maximum supply voltage, Arduino Uno or Mega is 5000mV while Arduino Nano or Node MCU is 3300mV
          float offsetSampleRead = 0;                 /* to read the value of a sample for offset purpose later */
          float currentSampleRead  = 0;               /* to read the value of a sample including currentOffset1 value*/
          float currentLastSample  = 0;               /* to count time for each sample. Technically 1 milli second 1 sample is taken */
          float currentSampleSum   = 0;               /* accumulation of sample readings */
          float currentSampleCount = 0;               /* to count number of sample. */
          int maxsamplecount = 50;
          float currentMean ;                         /* to calculate the average value from all samples, in analog values*/ 
          float RMSCurrentMean ;                      /* square roof of currentMean, in analog values */   
          float FinalRMSCurrent ;                     /* the final RMS current reading*/

          float FinalDCCurrent ;                     /* the final DC current reading*/          
          float currentSampleSum_DC = 0;
          float currentMean_DC ;                     /* to calculate the average value from all samples, for DC mode, in analog values*/           
  #endif 

  void tskfn_ADSRead();
  Task tskADSRead(1000, TASK_FOREVER, &tskfn_ADSRead, &Scheduler_ts, false);   

#endif   


#if defined (_emonlib_)  || defined (_pressureSensor_) 
  #include "CT_ProcessPower.h"
  #ifdef _emonlib_
      #include "EmonLib.h"             // Include Emon Library
      #define CurrentPin A5
      #define VoltagePin A7 
      #ifdef ESP32_2RBoard
        #define CurrentPin A7
        #define VoltagePin A6
      #endif  

      void RelayCT_NormalConsumption_func (void* obj);
      void RelayCT_HighConsumption_func (void* obj);
      // CTPROCESSOR CT_1(CurrentPin,30,VoltagePin,382/2 ,1.7 , RelayCT_HighConsumption_func, RelayCT_NormalConsumption_func);
      CTPROCESSOR CT_1(CurrentPin ,30,VoltagePin,382/2 ,1.7 , RelayCT_HighConsumption_func, RelayCT_NormalConsumption_func);
       
  #endif    
  void tskfn_EmonPublisher();
  Task tskEmonPublisher(5000, TASK_FOREVER, &tskfn_EmonPublisher, &Scheduler_ts, false); 
  void tskfn_emon_reader();
  Task tskEmonReader(2000, TASK_FOREVER, &tskfn_emon_reader, &Scheduler_ts, false);  


  #ifdef _NEWMETHOD_
  void tskfn_PF();
  Task tskADSReadPF(1000, TASK_FOREVER, &tskfn_PF, &Scheduler_ts, false);   
  #endif  
#endif


#if defined  _ESP_ALEXA_ || defined _ALEXA_
  void tskfn_espAlexaStateUpdate();
  Task tskespAlexaStateUpdate(5000, TASK_FOREVER, &tskfn_espAlexaStateUpdate, &Scheduler_ts, false);   
#endif

#ifdef OLED_1306
  #include <SSD1306.h>
  void tskfn_OLEDUpdate();
  Task tskOLEDUpdate(1000, TASK_FOREVER, &tskfn_OLEDUpdate, &Scheduler_ts, false);   
#endif    


#ifdef OLED_ThingPulse
  #include <Wire.h>
  #include <SSD1306Wire.h>
  SSD1306Wire display(0x3c,SDA,SCL);
#endif


#ifdef blockingTime
  #include <KSBNTP.h>
#else
  #include <KSBAsyncNTP.h>
  AsyncUDP Audp;
#endif

bool esp_now_initiated = false;
bool mesh_active = false;

#ifdef ESP32
  #ifdef AppleHK
    extern     hap_char_t *hcx;
    extern     hap_char_t *hcx_temp;
  #endif
#endif
//#include <RelaysArray.h>
//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays
extern std::vector<void *> inputs ; // a list to hold all relays
extern void applyIRMAp(uint8_t Inpn, uint8_t rlyn);

extern bool started_in_confMode;

#ifdef _ESP_ALEXA_
  #define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
  #include <Espalexa.h>
  Espalexa espalexa;
  extern AsyncWebServer AsyncWeb_server;
  bool Alexa_initialised = false;

  void alphaChanged(EspalexaDevice* d) {
    if (d == nullptr) return; 

    //do what you need to do here
      for (auto it : relays)  {
        Relay * rtemp = static_cast<Relay *>(it);
          if (rtemp) {
                if ( (strcmp(d->getName().c_str(), rtemp->getRelayConfig()->v_AlexaName.c_str()) == 0) ) { 
                      if (d->getValue()) {
                      rtemp->mdigitalWrite(rtemp->getRelayPin(),HIGH);
                      } else {
                          rtemp->mdigitalWrite(rtemp->getRelayPin(),LOW);
                      }
              } 
          }  
      }  
  }  

#endif

#ifdef ESP32
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
	#include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"  
}
#endif

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

#include <KSBWiFiHelper.h>

long blinkInterval = blink_normal ;           // blinkInterval at which to blink (milliseconds)
Task blinker(blinkInterval, TASK_FOREVER, &blinkledTask, &Scheduler_ts, true); 
Task tskPinger(17000, TASK_FOREVER, &servicePings, &Scheduler_ts, false); 

int ledState = LOW;             					    // ledState used to set the LED
unsigned long previousMillis = 0;             // will store last time LED was updated
unsigned long lastMillis = 0;
unsigned long lastMillis_1 = 0;
unsigned long lastMillis_2 = 0;
unsigned long lastMillis5000 = 0;
unsigned long lastMillis3000 = 0;


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

extern AsyncMqttClient mqttClient;

bool homekitNotInitialised = true;
bool HomespanInitiated = false;

time_t prevDisplay = 0; // when the digital clock was displayed
#include <Chronos.h>

NodeTimer NTmr(4);
TempConfig PTempConfig(1);

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

#ifdef SR04_SERIAL
#include "SR04_SERIAL.h"
extern SoftwareSerial myPort;
#endif

#ifdef _pressureSensor_
  #include "TLPressureSensor.h"
  TLPressureSensor TL136(35u , 400, 300, 51);
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

double secondson = 0;

String MAC;
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
  #if not defined _emonlib_ && not defined _pressureSensor_
   static TempSensor tempsensor(TempSensorPin);
  #endif  
  static TempSensor TempSensorSecond(SecondTempSensorPin);
/*
          // GPIO where the DS18B20 is connected to
          const int oneWireBus = 16;     

          // Setup a oneWire instance to communicate with any OneWire devices
          OneWire oneWire(oneWireBus);

          // Pass our oneWire reference to Dallas Temperature sensor 
          DallasTemperature sensors(&oneWire);
*/
#endif

#ifdef _ACS712_
ACS712 sensor(ACS712_30A, A0);  // todo: fix pin for ESP32 based boards
  #ifdef ESP32
       // ACS712 sensor(ACS712_30A, A0);  // todo: fix pin for ESP32 based boards, this is defined above for all boards, same pin A0
       #ifdef ESP32_2RBoard
        // ACS712 sensor_2(ACS712_30A, A3);  // todo: fix pin for ESP32 based boards, this is second sensor for 
       #endif  
  #endif
#endif

void ticker_relay_ttl_off (void* obj) ;
void ticker_relay_ttl_periodic_callback(void* obj);
void ticker_ACS712_func (void* obj);
void ticker_ACS712_mqtt (void* obj);
void onRelaychangeInterruptSvc(void* t);



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


#if defined (_emonlib_)  || defined (_pressureSensor_) 
void tskEmonPublisher_setEnableSts(bool sts){
  if (sts) tskEmonPublisher.enable();
  if (!sts) tskEmonPublisher.disable();
  
}
#endif


#ifdef ESP32
#ifdef WaterFlowSensor
      long currentMillis_WFS=0;
      long previousMillis_WFS=0;
      int interval=1000;
      volatile byte pulseCount;
      byte pulse1Sec = 0;
      float flowRate;
      unsigned int flowMilliLitres;
      unsigned long totalMilliLitres;


void IRAM_ATTR pulseCounter() // Interupt function for WaterFlowSensor
{
  pulseCount++;
}
#endif
#endif


void myClickFunction() {
  Serial.print(F("\n[INFO   ] >>>>> Conf Button Clicked <<<<< \n")); //MYbUTTON
  #ifdef OLED_1306
  display.clearDisplay();
  if (CURRENT_Display_Action == ACTION_DISPALY_1) {
    CURRENT_Display_Action = ACTION_DISPALY_2;
    display.setCursor(0,0);    
    display.println(">> Page 2 <<");
    Serial.print(F("\n[INFO   ] >>>>> PAGE 2 <<<<< \n")); 
  } else if (CURRENT_Display_Action == ACTION_DISPALY_2) {
    CURRENT_Display_Action = ACTION_DISPALY_3;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 3 <<<<< \n")); 
    display.setCursor(0,0);    
    display.println(">> Page 3 <<");    
  } else if (CURRENT_Display_Action == ACTION_DISPALY_3) {
    CURRENT_Display_Action = ACTION_DISPALY_4;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 4 <<<<< \n")); 
    display.setCursor(0,0);    
    display.println(">> Page 4 <<");    
  } else if (CURRENT_Display_Action == ACTION_DISPALY_4) {
    CURRENT_Display_Action = ACTION_DISPALY_1;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 1 <<<<< \n")); 
    display.setCursor(0,0);    
    display.println(">> Page 1 <<");    
  } 
  #endif
} 


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

  
  #ifdef ESP32_2RBoard
  
  Relay relay1( 
      Relay1Pin,
      ticker_relay_ttl_off,
      ticker_relay_ttl_periodic_callback,
      ticker_ACS712_func,
      ticker_ACS712_mqtt,
      onRelaychangeInterruptSvc,
      relayon
      
    );
    
  #endif

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
  #ifdef _ACS712_
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
            rly->setRelayTTA_Timer_Interval(rly->RelayConfParam->v_tta*1000); //      ticker_relay_tta->interval(rly->RelayConfParam->v_tta*1000);
            rly->start_tta_timer(); // ticker_relay_tta->start();  

        }
    }
  }
  #endif
}

void ticker_relay_ttl_periodic_callback(void *relaySender)
{
  if (relaySender != nullptr)
  {
    Relay *rly = static_cast<Relay *>(relaySender);
    uint32_t t = rly->getRelayTTLperiodscounter();
    Serial.printf("\n[INFO   ] TTL Countdown: %u ", t);  
    String st = String(t);  
    if (rly->readrelay() == HIGH)  
    {
        mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, st.c_str());
    }
  }
}

void ticker_relay_ttl_off(void *relaySender)  // this function is called when the timer expires 
{
  if (relaySender != nullptr)
  {
    Relay *rly = static_cast<Relay *>(relaySender);
    rly->mdigitalWrite(rly->getRelayPin(), LOW);
    // post OFF so to cancel the "retained ON" status on next restart of the board
    mqttClient.publish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF); 
    if (rly->RelayConfParam->v_ttl > 0)
    {
        mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
    }
  }
}

void onRelaychangeInterruptSvc(void *relaySender)
{
  Relay *rly = static_cast<Relay *>(relaySender);
  //  if (mqttClient.connected()) {
  //if (rly->rchangedflag)   {
    
    // post ON/OFF 
    mqttClient.publish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, rly->readrelay() == HIGH ? ON : OFF );
    #ifdef DEBUG_ENABLED
      debugV("* posting status: %s %s", rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), rly->readrelay() == HIGH ? ON : OFF );
    #endif    

    if (rly->readrelay() == HIGH)
    {
        Serial.println(F("[INFO   ] interrupt *ON* occurred."));
        if (rly->RelayConfParam->v_ttl > 0) {
            rly->start_ttl_timer();
            // post v_ttl value once on "on", post v_curr_ttl in the timer
            Serial.println(F("[INFO   ] posting interrupt *on*"));
            mqttClient.publish(rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, String(rly->RelayConfParam->v_ttl).c_str()); 
            Serial.println(F("[INFO   ] posting interrupt *on* done"));
        }
    }
    else
    { // (rly->readrelay() == LOW)
        Serial.println(F("[INFO   ] interrupt *OFF* occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl > 0) {
            mqttClient.publish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
        }
    }
  
      //}


  #ifdef AppleHK
  #ifndef ESP32
    // if (HomeKitt_PIN_SWITCH == rly->getRelayPin()) {
    rly->savePersistedrelay();
    switch (rly->getRelayPin())
    {
    case RelayPin:
      cha_switch_on.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false; // sync the value
      homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
      break;
  #ifdef HWver03_4R
    case Relay1Pin:
      cha_switch_on1.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false; // sync the value
      homekit_characteristic_notify(&cha_switch_on1, cha_switch_on1.value);
      break;
    case Relay2Pin:
      cha_switch_on2.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false; // sync the value
      homekit_characteristic_notify(&cha_switch_on2, cha_switch_on2.value);
      break;
    case Relay3Pin:
      cha_switch_on3.value.bool_value = digitalRead(rly->getRelayPin()) == HIGH ? true : false; // sync the value
      homekit_characteristic_notify(&cha_switch_on3, cha_switch_on3.value);
      break;
  #endif
    }
  #endif
  #ifdef ESP32
    hap_val_t new_val;
    new_val.i = digitalRead(rly->getRelayPin()) == HIGH ? 1 : 0;
    hap_char_update_val(hcx, &new_val);
  #endif
  #endif
//  rchangedflag = false; 
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
        rly->stop_tta_timer(); // ticker_relay_tta->stop();
        rly->mdigitalWrite(rly->getRelayPin(), LOW);
        rly->stop_ttl_timer();
      } else {
      rly->setRelayTTA_Timer_Interval(rly->RelayConfParam->v_tta*1000); //   ticker_relay_tta->interval(rly->RelayConfParam->v_tta*1000);
      rly->start_tta_timer();  //ticker_relay_tta->start();
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
        if (rly->RelayConfParam->v_ttl > 0 ) rly->start_ttl_timer(); 
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
          started_in_confMode = true;

        } else {
          // Station mode, try to connect with saved SSID & PASS
          #ifndef ESP_MESH
            WiFi.mode(WIFI_STA); 
            WiFi.setSleep(WIFI_PS_NONE);
            Serial.print(F("\n[WIFI   ] Begining WiFi connection"));  
            blinker.setInterval(50);
            blinker.enable();
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
  Serial.println(F("starting softAP mode - Connect to: WIFI AP:"));
  Serial.println(t);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.softAPIP());

  #ifdef OLED_1306 
    display.clearDisplay();
    display.setCursor(0,0);    
    display.println("*** soft AP mode ***\nConnect to:");
    display.println("");
    display.println(t);       
    display.println("");
    display.print(WiFi.softAPIP());               
  #endif

	trials = 0;
	SetAsyncHTTP();
	blinkInterval = blink_APMode;
  blinker.setInterval(blink_APMode);     
	APModetimer_run_value = 0;
}

  #ifdef _emonlib_   // CT Normal current processing
    void RelayCT_NormalConsumption_func (void* obj) {  
        if (CT_1.CT_RelayForceOff){
          Serial.printf("\n[CT     ] Consumption is normal, setting relay back [%u] in %u seconds", relay0.relay_previous_state, CT_1.ThresholdCossLowTimerCounter);   
          if (CT_1.ThresholdCossLowTimerCounter >= MyConfParam.v_ToleranceOnTime) {
            relay0.mdigitalWrite(relay0.getRelayPin(),relay0.relay_previous_state == 1 ? HIGH : LOW); //HIGH);
            CT_1.CT_RelayForceOff = false;
          }
        }
    }
    void RelayCT_HighConsumption_func (void* obj) {   // CT over current processing 
          Serial.printf("\n[CT     ] consumption HIGH, setting relay off after %u seconds", CT_1.ThresholdCossHighTimerCounter);   
          Serial.printf("\n[CT *   ] checking relay status @ count  %u, saved %u ", CT_1.ThresholdCossHighTimerCounter, relay0.relay_previous_state);    
          if ((CT_1.ThresholdCossHighTimerCounter == 1)) {
            Serial.printf("\n[CT**   ] saving relay status %u ", relay0.readrelay());    
            relay0.relay_previous_state = relay0.readrelay();
          }         
        if (CT_1.ThresholdCossHighTimerCounter >= MyConfParam.v_ToleranceOffTime) {
          relay0.mdigitalWrite(relay0.getRelayPin(),LOW);
          CT_1.ThresholdCossHighTimerStop();
          CT_1.CT_RelayForceOff = true;              
        }
    }
  #endif 


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
  #if not defined _emonlib_ && not defined _pressureSensor_
  #ifndef ESP32_2RBoard
  InputSensor Inputsnsr01(InputPin01,process_Input,INPUT_NONE);
  #endif 
  #endif

  #ifdef ESP32_2RBoard
  InputSensor Inputsnsr01(InputPin01,process_Input,INPUT_NONE);
  #endif  
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

    blinkInterval = blink_normal;
    //#ifndef ESP32
    blinker.setInterval(blink_normal);  
    blinker.enable();
    //#endif    
    Serial.print(F("[WIFI   ] IP assigned by DHCP = "));
    Serial.println(WiFi.localIP());   
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
      setSyncProvider(timeprovider); // [time]
      setSyncInterval(10); 
    #endif

    #ifdef DEBUG_ENABLED
      debugV("* Starting MQTT connection timer, 5 seconds period");
    #endif

    // tiker_MQTT_CONNECT.start(); // timer will retry to connect every 5s.  
    SetAsyncHTTP();

    #ifdef AppleHK
      #ifdef ESP32
        connectToMqtt();
      #endif
    #endif

    #ifndef AppleHK
    connectToMqtt();
    #endif

    // MBserver->begin();
    // MBserver->onClient(&handleNewClient, MBserver);

    homekitNotInitialised = true;

    #ifdef _ESP_ALEXA_
    //our callback functions
    if (!Alexa_initialised) {
        Alexa_initialised = true;
        for (auto it : relays)  {
          Serial.printf("[MAIN] Adding relays to ESPAlexa");      
          Relay * rtemp = static_cast<Relay *>(it);
            if (rtemp) {
              if (rtemp->getRelayConfig()->v_AlexaName !="null") {
              espalexa.addDevice(rtemp->getRelayConfig()->v_AlexaName.c_str(), alphaChanged);
              }
            }  
        }   
        // EspalexaDevice* device3; //this is optional
        espalexa.begin(&AsyncWeb_server); //give espalexa a pointer to your server object so it can use your server instead of creating its own
    }
    #endif

    #ifdef _ALEXA_
        Serial.println("[ALEXA  ] >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  Starting Alexa server");
      if (!Alexa_initialised) {
        Alexa_initialised = true;
        Serial.println("[ALEXA  ] Starting Alexa server");
        // mySwitch.enableReceive(RF_RECEIVER);  // Receiver on interrupt 0 => that is pin #2

        // By default, fauxmoESP creates it's own webserver on the defined port
        // The TCP port must be 80 for gen3 devices (default is 1901)
        // This has to be done before the call to enable()
        // fauxmo.createServer(true); // not needed, this is the default value
        fauxmo.setPort(80); // This is required for gen3 devices

        // You have to call enable(true) once you have a WiFi connection
        // You can enable or disable the library at any moment
        // Disabling it will prevent the devices from being discovered and switched
        fauxmo.enable(true);
        // You can use different ways to invoke alexa to modify the devices state:
        // "Alexa, turn lamp two on"

        for (auto it : relays)  {
          Serial.printf("[MAIN   ] Adding relays to fauxmo\n");      
          Relay * rtemp = static_cast<Relay *>(it);
            if (rtemp) {
              if (rtemp->getRelayConfig()->v_AlexaName != "null") {
              fauxmo.addDevice(rtemp->getRelayConfig()->v_AlexaName.c_str());
              Serial.printf("[MAIN   ] Added relay:[ %s ]:to fauxmo" , rtemp->getRelayConfig()->v_AlexaName.c_str());
              } else {
                // fauxmo.enable(false);
                }
            }  
        }     

        fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
          // Callback when a command from Alexa is received. 
          // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
          // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
          // Just remember not to delay too much here, this is a callback, exit as soon as possible.
          // If you have to do something more involved here set a flag and process it in your main loop.
                  
          for (auto it : relays)  {
            // if ( (strcmp(device_name, relay0.getRelayConfig()->v_AlexaName.c_str()) == 0) ) {      
              Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);      
                Relay * rtemp = static_cast<Relay *>(it);
                if (rtemp) {
                  if ( (strcmp(device_name, rtemp->getRelayConfig()->v_AlexaName.c_str()) == 0) ) { 
                    rtemp->mdigitalWrite(rtemp->getRelayPin(),state ? HIGH : LOW );
                  } 
                }  
          } 
        } );
      }
    #endif    

} // thingsTODO_on_WIFI_Connected


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
  #ifdef _pressureSensor_
    config_read_error_t res = TLloadconfig("/PressureSensorConfig.json", TL136);
    TL136.preparejsontemplate();
  #endif 
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
      #if not defined _emonlib_ && not defined _pressureSensor_
        Inputsnsr01.initialize(InputPin01,process_Input,INPUT_NONE,PULLMODE_);
        Inputsnsr01.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
        Inputsnsr01.onInputClick_RelayServiceRoutine = buttonclick;
        Inputsnsr01.post_mqtt = true;
        Inputsnsr01.mqtt_topic = MyConfParam.v_InputPin01_STATE_PUB_TOPIC; //+ "_1"; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
        Inputsnsr01.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);  
      #endif

      #ifdef ESP32_2RBoard  
        Inputsnsr01.initialize(InputPin01,process_Input,INPUT_NONE,PULLMODE_);
        Inputsnsr01.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
        Inputsnsr01.onInputClick_RelayServiceRoutine = buttonclick;
        Inputsnsr01.post_mqtt = true;
        Inputsnsr01.mqtt_topic = MyConfParam.v_InputPin01_STATE_PUB_TOPIC; //+ "_1"; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
        Inputsnsr01.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);  
      #endif

      Inputsnsr02.initialize(InputPin02,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr02.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr02.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr02.post_mqtt = true;
      Inputsnsr02.mqtt_topic = MyConfParam.v_InputPin02_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr02.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);

      Inputsnsr03.initialize(InputPin03,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr03.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr03.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr03.post_mqtt = true;
      Inputsnsr03.mqtt_topic = MyConfParam.v_InputPin03_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr03.fclickmode = static_cast <input_mode>(MyConfParam.v_IN3_INPUTMODE);

      Inputsnsr04.initialize(InputPin04,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr04.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr04.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr04.post_mqtt = true;
      Inputsnsr04.mqtt_topic = MyConfParam.v_InputPin04_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr04.fclickmode = static_cast <input_mode>(MyConfParam.v_IN4_INPUTMODE);

      Inputsnsr05.initialize(InputPin05,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr05.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr05.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr05.post_mqtt = true;
      Inputsnsr05.mqtt_topic = MyConfParam.v_InputPin05_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr05.fclickmode = static_cast <input_mode>(MyConfParam.v_IN5_INPUTMODE);

      #ifndef WaterFlowSensor
      Inputsnsr06.initialize(InputPin06,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr06.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr06.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr06.post_mqtt = true;
      Inputsnsr06.mqtt_topic = MyConfParam.v_InputPin06_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr06.fclickmode = static_cast <input_mode>(MyConfParam.v_IN6_INPUTMODE);
      #endif
      #ifdef WaterFlowSensor
        pinMode(InputPin06, INPUT_PULLUP);
        pulseCount = 0;
        flowRate = 0.0;
        flowMilliLitres = 0;
        totalMilliLitres = 0;
        previousMillis = 0;
        attachInterrupt(InputPin06, pulseCounter, FALLING);
      #endif


    #endif//HWESP32
  #endif //StepperMode
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
  // strcpy(topic, "checkIn/");
  // strcat(topic, mqtt_client_id);
  // client.publish(topic, "OK"); 
  }
#endif
*/

#ifdef ESP32
/*
void toggleLED(void * parameter){
  for(;;){ // infinite loop
    digitalWrite(2, HIGH);
    vTaskDelay(blinkInterval / portTICK_PERIOD_MS);
    digitalWrite(2, LOW);
    vTaskDelay(blinkInterval / portTICK_PERIOD_MS);
  }
}
*/
#endif

void setup() {
/*
#ifdef ESP32
  xTaskCreate(
    toggleLED,        // Function that should be called
    "Toggle LED",     // Name of the task (for debugging)
    1000,             // Stack size (bytes)
    NULL,             // Parameter to pass
    1,                // Task priority
    NULL              // Task handle
  );
#endif
*/



  #ifndef ESP32
    definePingsCallbacks();
  #endif  

  #ifndef ESP32
  ESP.wdtDisable();
  #endif

  #ifdef _ADS1X15_
  #ifndef _ADS_ASYNC_
    #ifdef _ADS1015_
      ads.setDataRate(RATE_ADS1015_3300SPS);
    #else
      ads.setDataRate(RATE_ADS1115_860SPS); 
    #endif
    ads.setGain(GAIN_TWOTHIRDS);
    //ads.setGain(GAIN_ONE);
    ads.begin();
  #else
    ADS1.setGain(0);        // 6.144 volt
    ADS1.setDataRate(7);    // medium
    // single shot mode
    // ADS1.setMode(0); //for ADS1115
    ADS1.setMode(1); ///for ADS1115
    ADS1.begin();
    // ADS1.setDataRate(7);    // medium
  #endif  
    
  #endif

  Serial.begin(115200);

  #ifdef OLED_ThingPulse
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_16);
    // Display some text
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Hello world");
    display.display();
  #endif

  #ifdef INVERTERLINK
  SERIAL_INVERTER.begin(2400);     // Using UART0 for comm with inverter. IE cant be connected during flashing
  #endif

  blinkInterval = blink_normal; 

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
        #if not defined _emonlib_ && not defined _pressureSensor_
        pinMode ( InputPin01, INPUT_PULLUP );
        #endif
        pinMode ( InputPin02, INPUT_PULLUP );
        pinMode ( InputPin03, INPUT_PULLUP );
        pinMode ( InputPin04, INPUT_PULLUP );
        pinMode ( InputPin05, INPUT_PULLUP );
        pinMode ( InputPin06, INPUT_PULLUP );                                        
        #endif        
    #endif    

  
  /*
  uint32_t Freq = 0;
  setCpuFrequencyMhz(240);
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
  

        #ifdef OLED_1306
              SSD_1306();
              DSS1306_text(0,0,1,"booting...");
              #ifdef _emonlib_
                DSS1306_text(0,10,1,"loading CT Readings...");
                loadCTReadings(CT_1.saved_Wh,CT_1.saved_MTD_Wh,CT_1.saved_YTD_Wh);
                Serial.printf ("[CT     ] Loaded CT Values %f KWh - %f MTD_KWh - %f YTD_KWh \n",
                CT_1.saved_Wh, CT_1.saved_MTD_Wh, CT_1.saved_YTD_Wh );              
                CT_1.wh         = CT_1.saved_Wh;
                CT_1.MTD_Wh     = CT_1.saved_MTD_Wh;
                CT_1.YTD_Wh     = CT_1.saved_YTD_Wh;
                CT_1.PreviousWh = CT_1.saved_Wh; 
                CT_1.CTSaveThreshold = 0;
              #else

              #endif
              
        #endif

         
        /*
          only need to format SPIFFS the first time we run a
          test or else use the SPIFFS plugin to create a partition
          https://github.com/me-no-dev/arduino-esp32fs-plugin
        */        
        #ifdef ESP32
        // SPIFFS.format();
          if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
            Serial.println(F("SPIFFS Mount Failed"));
           if(SPIFFS.format()) { Serial.println("File System Formated"); }
            return;
          }
          listDir(SPIFFS, "/", 0);  
        #else
          // SPIFFS.format();
          SPIFFS.begin();
        //  if(!SPIFFS.begin()){
        //    if(SPIFFS.format()) { Serial.println("File System Formated"); }
        //      else { Serial.println("File System Formatting Error"); } 
        //  }
        #endif
        //wifimode = WIFI_CLT_MODE;

        #ifdef USEPREF
          ReadParams(MyConfParam, preferences);
        #endif

        while (loadConfig(MyConfParam) != SUCCESS) {
          delay(2000);
          ESP.restart();
        };

        #ifdef ESP32
        #ifdef WaterFlowSensor
          loadconfigWFS("/WaterFlowSensorConfig.json"); 
        #endif
        #endif
        
        WiFi.mode(WIFI_STA); 
        WiFi.setSleep(WIFI_PS_NONE);
            
        if (digitalRead(ConfigInputPin) == LOW){
          started_in_confMode = true;
          Serial.print(F("\n[WIFI   ] >>>>> Sarting WIFI in AP mode <<<<< \n"));
          WiFi.mode(WIFI_AP_STA);
          startsoftAP();
        }

      //   String WiFiHostname = "ESP_" ; //+ CID();
      //   WiFiHostname += MyConfParam.v_PhyLoc.substring(0,28);
      //   WiFiHostname.replace(" ","");
      //   Serial.print(F("[WIFI   ] Setting WiFi Hostname to "));
      //   Serial.println(WiFiHostname);
      //   WiFi.setHostname(WiFiHostname.c_str());

          #ifndef ESP32
            #ifndef ESP_MESH 
            if (!started_in_confMode) {
              Serial.println(F("[WIFI   ] Starting WIFI in Station mode"));              
              WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); 
              //WiFi.begin( "ksba" , "samsam12" ); 
            }  
            #else
                Serial.println(F("[INFO   ] calling setup_mesh()"));                  
                setup_mesh();
            #endif   
          #else
            if (!started_in_confMode) {
              Serial.println(F("[WIFI   ] assigning WIFI Events"));            
              WiFi.onEvent(WiFiGotIP, WiFiEvent_t::
              ARDUINO_EVENT_WIFI_STA_GOT_IP);
              WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);  
              #ifndef ESP_MESH      
                WiFi.begin( MyConfParam.v_ssid.c_str() , MyConfParam.v_pass.c_str() ); 
                WiFi.printDiag(Serial); 
              #endif
              #ifdef ESP_MESH
                    setup_mesh();
              #endif   
            }       
          #endif

        mqttClient.setClientId(APssid.c_str());
        mqttClient.setCleanSession(true);
        mqttClient.setMaxTopicLength(512);
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

        #ifdef DEBUG_ENABLED
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
         #if not defined _emonlib_ && not defined _pressureSensor_
        inputs.push_back(&Inputsnsr01); 
        #endif
        #ifdef ESP32_2RBoard
        inputs.push_back(&Inputsnsr01); 
        #endif
        inputs.push_back(&Inputsnsr02);      
        inputs.push_back(&Inputsnsr03);    
        inputs.push_back(&Inputsnsr04); 
        inputs.push_back(&Inputsnsr05);      
        inputs.push_back(&Inputsnsr06);                
        #endif     

    #endif    

       while (relay0.loadrelayparams(0) != true){
          delay(2000);
          ESP.restart();
        };
        relay0.attachLoopfunc(relayloopservicefunc);
        relay0.stop_ttl_timer();
        relay0.setRelayTTL_Timer_Interval(relay0.RelayConfParam->v_ttl*1000);

        #ifdef ESP32_2RBoard
        
          while (relay1.loadrelayparams(1) != true){
            delay(2000);
            ESP.restart();
          };
          relay1.attachLoopfunc(relayloopservicefunc);
          relay1.stop_ttl_timer();
          relay1.setRelayTTL_Timer_Interval(relay1.RelayConfParam->v_ttl*1000);
          
        #endif


        #ifndef ESP32 
        ESP.wdtFeed();
        #endif
        #ifdef HWver03_4R   
          while (relay1.loadrelayparams(1) != true){
            delay(2000);
            ESP.restart();
          };
          relay1.attachLoopfunc(relayloopservicefunc);
          relay1.stop_ttl_timer();
          relay1.setRelayTTL_Timer_Interval(relay1.RelayConfParam->v_ttl*1000);

          while (relay2.loadrelayparams(2) != true){
            delay(2000);
            ESP.restart();
          };
          relay2.attachLoopfunc(relayloopservicefunc);
          relay2.stop_ttl_timer();
          relay2.setRelayTTL_Timer_Interval(relay2.RelayConfParam->v_ttl*1000);

          while (relay3.loadrelayparams(3) != true){
            delay(2000);
            ESP.restart();
          };
          relay3.attachLoopfunc(relayloopservicefunc);
          relay3.stop_ttl_timer();
          relay3.setRelayTTL_Timer_Interval(relay3.RelayConfParam->v_ttl*1000);    
        #endif            
        

        relays.push_back(&relay0); // order is important
        #ifdef ESP32_2RBoard
          relays.push_back(&relay1);
        #endif

        #ifdef HWver03_4R  
        // order is important
          relays.push_back(&relay1);
          relays.push_back(&relay2);
          relays.push_back(&relay3);   
        #endif     
        #ifndef ESP32
        ESP.wdtFeed();
        #endif
        while (loadIRMapConfig(myIRMap) != SUCCESS){
          ESP.restart();
        };
        #ifndef ESP32
          ESP.wdtFeed();
        #endif
        Serial.println(F("[INFO TP] opening /tempconfig.json file"));
        if (relay0.RelayConfParam->v_TemperatureValue != "0") {
            loadTempConfig("/tempconfig.json",PTempConfig);      
        } else {
            Serial.println(F("[INFO TP] v_TemperatureValue = 0; disabling temperature processing"));
        }

       #ifdef _ACS712_
       ACS_Calibrate_Start(relay0,sensor);
       #endif

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


  #ifdef SR04_SERIAL
   delay(2000);
   beginSerialSR04();
  #endif

  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
    if (!started_in_confMode){
    tskPinger.enable();
    }
  }

  #ifdef _emonlib_
    if (!started_in_confMode){
      xTimerStop(CT_1.ThresholdCossHighTimer,0);
      xTimerStop(CT_1.ThresholdCossLowTimer,0);
      CT_1.emon1.current(CurrentPin, MyConfParam.v_CurrentTransformer_max_current);
      CT_1.emon1.voltage(VoltagePin, MyConfParam.v_calibration, MyConfParam.v_PhaseCal);

      tskEmonReader.enable();    
      tskEmonPublisher.enable();
    }
  #endif

#ifdef _pressureSensor_
  tskEmonReader.enable();    
#endif



  #if defined  _ESP_ALEXA_ || defined _ALEXA_
    if (!started_in_confMode){
    tskespAlexaStateUpdate.enable(); 
    }
  #endif

  #ifdef OLED_1306
    tskOLEDUpdate.enable();
  #endif   

  #ifdef _ADS1X15_
    loadADS11x5Config("/ADS11x5Config.json",PADS11x5Config);  
    tskADSRead.enable();
  #endif       

  #ifdef _HST_
    loadHSTConfig("/HSTConfig.json",PAHSTConfig);  
  #endif         

  #ifdef _NEWMETHOD_
  tskADSReadPF.enable();
  #endif

  // link the myClickFunction function to be called on a click event.   
  // MYbutton.setClickTicks(500);
  MYbutton.attachClick(myClickFunction); 

  if (relay0.RelayConfParam->v_TemperatureValue != "0") {
    #if not defined _emonlib_ && not defined _pressureSensor_
    tempsensor.tempbegin();
    #endif
    TempSensorSecond.tempbegin();
  }
}


void loop() {

  MYbutton.tick(&MYbutton); 

  #ifdef _ALEXA_
    fauxmo.handle();
  #endif

  Scheduler_ts.execute();

  for (auto it : relays)  {
    Relay * rtemp = static_cast<Relay *>(it);
    if (rtemp != nullptr) {
        rtemp->watch();
    }
  }
  


    
  #ifdef _ESP_ALEXA_
   espalexa.loop();
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

  #ifdef DEBUG_ENABLED
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

   if (restartRequired){
      Serial.println(F("\n[SYSTEM ] Restarting\n\r"));
      restartRequired = false;
            delay(4000);
               if (MyConfParam.v_PRST == 1) {
                   Serial.println(F("\n[SYSTEM ] Restarting with Toggle\n\r"));
                   Relay * rtmp =  getrelaybynumber(0) ;         
                   digitalWrite(rtmp->getRelayPin(),LOW);                     
                }
            delay(1000);
      ESP.restart();
   }


  #ifdef AppleHK
  #ifndef ESP32
	  my_homekit_loop();
  #endif  
  #endif   

  #ifndef ESP_MESH
    tiker_MQTT_CONNECT.update(nullptr);  
    Wifireconnecttimer.update(nullptr);
  #else
   #ifdef ESP_MESH_ROOT
     tiker_MQTT_CONNECT.update(nullptr);  
     #endif
  #endif

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
     #if not defined _emonlib_ && not defined _pressureSensor_
      Inputsnsr01.watch();
     #endif  
     #ifdef ESP32_2RBoard
      Inputsnsr01.watch();
     #endif
      Inputsnsr02.watch();     
      Inputsnsr03.watch();           
      Inputsnsr04.watch();
      Inputsnsr05.watch();    
      #ifndef WaterFlowSensor 
      Inputsnsr06.watch();         
      #endif

      #ifdef WaterFlowSensor
          currentMillis_WFS = millis();
          if (currentMillis_WFS - previousMillis_WFS > interval) {
            pulse1Sec = pulseCount;
            pulseCount = 0;
            // Because this loop may not complete in exactly 1 second intervals we calculate
            // the number of milliseconds that have passed since the last execution and use
            // that to scale the output. We also apply the calibrationFactor to scale the output
            // based on the number of pulses per second per units of measure (litres/minute in
            // this case) coming from the sensor.
            flowRate = ((1000.0 / (millis() - previousMillis_WFS)) * pulse1Sec) / calibrationFactor;
            previousMillis_WFS = millis();
            // Divide the flow rate in litres/minute by 60 to determine how many litres have
            // passed through the sensor in this 1 second interval, then multiply by 1000 to
            // convert to millilitres.
            flowMilliLitres = (flowRate / 60) * 1000;
            // Add the millilitres passed in this second to the cumulative total
            totalMilliLitres += flowMilliLitres;
            // Print the flow rate for this second in litres / minute
            Serial.print("Flow rate: ");
            Serial.print(int(flowRate));  // Print the integer part of the variable
            Serial.print("L/min");
            Serial.print("\t");       // Print tab space
            // Print the cumulative total of litres flowed since starting
            Serial.print("Output Liquid Quantity: ");
            Serial.print(totalMilliLitres);
            Serial.print("mL / ");
            double totalLiters;
            totalLiters = totalMilliLitres / 1000;
            Serial.print(totalLiters);
            Serial.println("L");
            char res[8]; 
            char res2[8]; 
            dtostrf(flowRate, 6, 1, res); // Leave room for too large numbers!
            mqttClient.publish(WaterFlowSensor_Topic.c_str(), QOS2, RETAINED, res); // String(TSolarPanel).c_str());   
            dtostrf(totalLiters, 6, 1, res2); // Leave room for too large numbers!
            mqttClient.publish("TotalLiters", QOS2, RETAINED, res2); // String(TSolarPanel).c_str());   
          }
      #endif

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
  if (!started_in_confMode) {

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
                      connectToMqtt();
                      homekitUpdateBootStatus();
                  }     
                #endif  

                  #ifdef ESP32
                  if((timeStatus() == timeSet) && homekitNotInitialised) {    
                    Serial.println("[HOMEKIT] starting homekit");            
                    setupHAP();
                    homekitNotInitialised = false;
                  }
                  #endif    
              #endif 
    }


    if (millis() - lastMillis > 1000) {       // things to do every 1 second
      lastMillis = millis();
      secondson++;

      #ifdef SR04_SERIAL
      if (secondson > 5) {
        myPort.print("."); // send any char to trigger measurment
        //double cm = readSerialUltrasoundSensor();
        char res[8];
        dtostrf(readSerialUltrasoundSensor(), 6, 1, res);  
        Serial.print(PSTR("Result SR04 Serial :"));
        Serial.println(res);          
        if (mqttClient.connected()) {
          mqttClient.publish(MyConfParam.v_Sonar_distance.c_str(), QOS2, RETAINED, res );
          }      
      }
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
  }
      
  #if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
    #ifndef SR04 // have to stop it because it uses the same pins as the temp sensors
    if (relay0.RelayConfParam->v_TemperatureValue != "0") {

      /*
      sensors.requestTemperatures(); 
      float temperatureC = sensors.getTempCByIndex(0);
      Serial.print(temperatureC);
      Serial.println("ºC");
      */

    
      #if not defined _emonlib_ && not defined _pressureSensor_
      if (millis() - lastMillis5000 > 5000) {
      //  pinMode(TempSensorPin,  INPUT_PULLUP );  
      //  pinMode(SecondTempSensorPin,  INPUT_PULLUP );        
        lastMillis5000 = millis();
        
        pinMode ( TempSensorPin, INPUT_PULLUP);
        pinMode ( SecondTempSensorPin, INPUT_PULLUP);
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
        #ifdef DEBUG_ENABLED
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
        #ifdef DEBUG_ENABLED
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

        #ifdef OLED_1306
        if (CURRENT_Display_Action == ACTION_DISPALY_4){
              display.clearDisplay();
              int8_t StartRow = 22;
              int8_t rect_width = 60;
              display.drawRect(0,StartRow,rect_width,30, WHITE);
              display.drawRect(rect_width + 2 ,StartRow,rect_width,30, WHITE);

              display.setCursor(8, StartRow + 2);
              display.println(F("TEMP 1"));
              display.setTextColor(SSD1306_WHITE);
              display.setCursor(10,StartRow + 16);       
              display.setTextSize(1.5); 
              display.print(String(MCelcius));
              display.setTextSize(1);        
      

              display.setCursor(rect_width + 10 ,StartRow + 2 );    
              display.println(F("TEMP 2"));
              display.setCursor(rect_width + 8,StartRow + 16 );   
              display.setTextSize(1.5);                  
              display.print(String(MCelcius2));
              display.setTextSize(1);              
              display.setTextColor(WHITE);
        }

        #endif


      }
      #endif //emonlib
      
    }
    #endif
  #endif 
 #endif // ifndef StepperMode

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


#if defined (_emonlib_)  || defined (_pressureSensor_) 
    #ifdef _emonlib_
    void tskfn_EmonPublisher() {
        if (!started_in_confMode){
          if (mqttClient.connected()) {
            mqttClient.publish((MyConfParam.v_CurrentTransformerTopic).c_str(), QOS2, RETAINED, CT_1.jsonPost.c_str());        
          }  
    }
    }
    #endif
    #ifdef _pressureSensor_
    void tskfn_EmonPublisher() {

    }
    #endif    

    void tskfn_emon_reader(){
        #ifdef _emonlib_ 
            #ifndef _pressureSensor_
              if (!MyConfParam.v_CurrentTransformerTopic.startsWith("disabled:")) {
                CT_1.readPower(MyConfParam.v_CT_adjustment, MyConfParam.v_CT_saveThreshold);
                #if not defined _pressureSensor_
                if ((CURRENT_Display_Action == ACTION_DISPALY_1) || (CURRENT_Display_Action == ACTION_DISPALY_2)) {
                  #ifdef OLED_1306
                  CT_1.DisplayPower(display, mqttClient, MyConfParam.v_Screen_orientation);
                  #endif
                  }
                #endif 
                
                #ifdef AppleHK
                  // this is still experimental, post volatge as dim value!
                  hap_val_t new_val;
                  new_val.f = CT_1.amps;
                  hap_char_update_val(hcx_temp, &new_val);
                #endif   
                
                if (CT_1.amps > MyConfParam.v_CT_MaxAllowed_current) {
                  #ifdef ESP32
                  #else
                    if (RelayCToff.enabled == false) {
                      Serial.println(F("[CT     ] timer off runing - High consumption detected"));
                      RelayCToff.start(); 
                    }
                  #endif
                }        
              }
            #else
              CT_1.readVoltage(); 
            #endif
        #endif 

        #ifdef _pressureSensor_
          TL136.read(500);
          #ifdef OLED_1306
            if (CURRENT_Display_Action == ACTION_DISPALY_1) {
              TL136.DisplayLevel(display, WiFi.isConnected(), mqttClient.connected(),MyConfParam.v_Screen_orientation);
            }
            #ifdef _emonlib_
              display.setCursor(90,10);    
              display.print(String(CT_1.supplyVoltage,0) + " v");
            #endif    
          #endif
          if (MyConfParam.maSTopic != "0") {
            if (mqttClient.connected()) {
              mqttClient.publish(TL136.maSTopic.c_str(), QOS2, RETAINED, TL136.jsonPost.c_str()); 
            }
          }
        #endif
    }
#endif

#if defined  _ESP_ALEXA_ || defined _ALEXA_
void tskfn_espAlexaStateUpdate(){
  Serial.println(F("[ALEXA   ] notifying ALEXA"));
  #ifdef  _ESP_ALEXA_
        if (Alexa_initialised) {
          int n = 0;
          for (auto it : relays)  {
            Relay * rtemp = nullptr;
            rtemp = static_cast<Relay *>(it);
            if (rtemp) {
                if ( (strcmp(espalexa.getDevice(n)->getName().c_str(), rtemp->getRelayConfig()->v_AlexaName.c_str()) == 0) ) { 
                  espalexa.getDevice(n)->setState(digitalRead(rtemp->getRelayPin()) == HIGH);
                  Serial.printf("\n [ALEXA   ] updating ALEXA %s to %d",espalexa.getDevice(n)->getName(),digitalRead(rtemp->getRelayPin()) == HIGH );
                }
            }
            n++;
          }
          n = 0;
        }

  #endif 
  #ifdef _ALEXA_
    // notify all relays status
    Relay *rtemp = nullptr;
    for (auto it : relays)
    {
      rtemp = static_cast<Relay *>(it);
      if (rtemp)
      {
          fauxmo.setState(rtemp->RelayConfParam->v_AlexaName.c_str(), digitalRead(rtemp->getRelayPin()) == HIGH, 1);
      }
    }
  #endif 
}    
#endif


#ifdef OLED_1306
void tskfn_OLEDUpdate() {
    display.setCursor(1,54);  // col,row      
    display.setTextColor(BLACK,WHITE);
    display.print(WiFi.localIP().toString());
    display.setTextColor(WHITE);      
    display.setCursor(100,54);  // col,row    
    if ((WiFi.status() == WL_CONNECTED))  display.print(F("*"));   
    if ((WiFi.status() != WL_CONNECTED)) display.print(F("x"));  
    display.setCursor(110,54);  // col,row   
    if (mqttClient.connected()) display.print(F("M"));   
    if (!mqttClient.connected()) display.print(F("m"));   

    MYbutton.tick(&MYbutton); 
    display.display();
}  
#endif    


#ifdef _ADS1X15_
  void tskfn_ADSRead() {  
    
      int16_t adc0, adc1, adc2, adc3;
      float Voltage0 = 0.0;
      float Voltage1 = 0.0;
      float Voltage2 = 0.0;
      float Voltage3 = 0.0;

      float R1 = 100000;
      float R2 = 3300;
      float mutiplyer = PADS11x5Config.ResMultiplier; // 31.37709; //(R2/(R1+R2)) * 1000;
      
      // Serial.println("Getting single-ended readings from AIN0..3");
      // Serial.println("ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)");
      // https://microcontrollerslab.com/wp-content/uploads/2019/05/ADS1115-external-ADC-with-ESP32.jpg
      // https://microcontrollerslab.com/ads1115-external-adc-with-esp32/


    #ifndef _ADS_ASYNC_
        float ADSStep = 0;
        #ifdef _ADS1015_
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) ADSStep = 2;
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) ADSStep = 3;
        #else
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) ADSStep = 0.125;
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) ADSStep = 0.1875;
        #endif
  
        adc0 = ads.readADC_SingleEnded(0);
        Voltage0 = (adc0 * ADSStep)/1000;  
        Serial.printf("\nADS 1115 Volatge 0 = %f, %u", Voltage0, adc0);

        adc1 = ads.readADC_SingleEnded(1);
        Voltage1 = (adc1 * ADSStep)/1000;  
        Serial.printf("\nADS 1115 Volatge 1 = %f, %u, %.2f", Voltage1, adc1);
  
        adc2 = ads.readADC_SingleEnded(2);
        Voltage2 = (adc2 * ADSStep)/1000;  
        Serial.printf("\nADS 1115 Volatge 2 = %f, %u, %.2f", Voltage2, adc2, (Voltage2 * mutiplyer));

        adc3 = ads.readADC_SingleEnded(3);
        Voltage3 = (adc3 * ADSStep)/1000;  
        Serial.printf("\nADS 1115 Volatge 3 = %f, %u, %.2f", Voltage3, adc3, (Voltage3 * mutiplyer));
      
    #else
        //int16_t val_0 = ADS1.readADC(0);  
        //int16_t val_1 = ADS1.readADC(1);  
        int16_t val_2 = ADS1.readADC(2);  
        int16_t val_3 = ADS1.readADC(3);  
        float ADSStep = 0;
        #ifdef _ADS1015_
        ADSStep = 3; 
        #else
        ADSStep = 0.1875;
        #endif
        //Voltage0 = (val_0 * ADSStep)/1000;  
        //Serial.printf("\nADS 1115 Volatge 0 = %f, %u, %.2f, ADS1.toVoltage %f", Voltage0, val_0, (Voltage0 * mutiplyer), ADS1.toVoltage(ADS1.readADC(0)) * mutiplyer);
        
        Voltage2 = (val_2 * ADSStep)/1000;  
        #ifdef _emonlib_
        CT_1.jsonPost.replace("[VOLTS2]", String(Voltage2 * mutiplyer));
        #endif
        //Serial.printf("\nADS 1115 Volatge 2 = %f, %u, %.2f, ADS1.toVoltage %f", Voltage2, val_2, (Voltage2 * mutiplyer), ADS1.toVoltage(ADS1.readADC(2)) * mutiplyer);
        Voltage3 = (val_3 * ADSStep)/1000;  
        #ifdef _emonlib_
        CT_1.jsonPost.replace("[VOLTS3]",String(Voltage3 * mutiplyer));
        #endif
        //Serial.printf("\nADS 1115 ADS1.toVoltage 3 = %f, %u, %.2f, ADS1.toVoltage %f", Voltage3, val_3, (Voltage3 * mutiplyer), ADS1.toVoltage(ADS1.readADC(3)) * mutiplyer);


        #ifdef _ADS1X15_DC_Current_   /* read DC current */
          #ifndef _ADS1X15_CURRENT_   /* make sure pin is not used for _ADS1X5_CURRENT_*/
                mVperAmpValue = PAHSTConfig.AmpsVoltsRatio; 
                while (currentSampleCount < maxsamplecount )                                           
                  { 
                //    unsigned int tt = micros();
                    currentSampleRead = ADS1.readADC_Differential_0_1();     //ADS1.readADC(currentAnalogInputPin);                                       /* read the sample value including offset value*/
                    //delay(1);
                 //   float callibrationvalue = ADS1.readADC(calibrationPin)-1;
                    //Serial.printf("\n Actual value %f, vref %f", currentSampleRead, callibrationvalue);
                 //  currentSampleRead -=  callibrationvalue;
                 //   Serial.printf ("\n2 ADC measurment time = %u", micros() - tt);
                    // SerialDebug.print(" currentAnalogInputPin value is: "); SerialDebug.println(analogRead(currentAnalogInputPin));
                    // SerialDebug.print(" calibrationPin value is: "); SerialDebug.print(analogRead(calibrationPin));
                    // SerialDebug.println(" currentSampleRead value is: "); SerialDebug.print(analogRead(currentSampleRead));
                    float csq = currentSampleRead * currentSampleRead;
                    currentSampleSum += csq ;                                                         /* accumulate total analog values for each sample readings*/
                    currentSampleSum_DC +=  currentSampleRead;
                    currentSampleCount += 1;                                                        /* to count and move on to the next following count */  
                  }

                  if (currentSampleCount == maxsamplecount){
                    currentMean = currentSampleSum/currentSampleCount;                                                /* average accumulated squared analog values*/
                    RMSCurrentMean = sqrt(currentMean);                                                             

                    currentMean_DC = currentSampleSum_DC /currentSampleCount;                                         /* average accumulated analog values*/

                    Serial.printf("\n >>>>> Sample msqt value is: %f", RMSCurrentMean);
                    Serial.printf("\n >>>>> Sample msqt DCvalue is: %f", currentMean_DC);
                    FinalRMSCurrent = (((RMSCurrentMean /2047) *supplyVoltage) /mVperAmpValue)- manualOffset;         /* calculate the final RMS current*/
                    FinalDCCurrent =  (((currentMean_DC /2047) *supplyVoltage) /mVperAmpValue)- manualOffset;         /* calculate the final RMS current*/
                    if(FinalRMSCurrent <= (625/mVperAmpValue/100))                                                    /* if the current detected is less than or up to 1%, set current value to 0A*/
                    { FinalRMSCurrent =0; } 

                    Serial.printf(" \nThe Current RMS value is: %f", FinalRMSCurrent);
                    Serial.printf(" \nThe Current DC value is: %f", FinalDCCurrent);

                    currentSampleSum = 0;
                    currentSampleSum_DC = 0;                    
                    currentSampleCount = 0;

                    CT_1.jsonPost.replace("[HST_AMPS]",String(FinalRMSCurrent));
                    CT_1.jsonPost.replace("[DCAMPS]",String(FinalDCCurrent));

                  }
  

          #endif
        #endif


            if (CURRENT_Display_Action == ACTION_DISPALY_3) {
              display.clearDisplay();
              int8_t StartRow = 22;
              int8_t rect_width = 60;
              display.drawRect(0,StartRow,rect_width,30, WHITE);
              display.drawRect(rect_width + 2 ,StartRow,rect_width,30, WHITE);

              #ifdef _ADS1X15_DC_Current_ 
                  display.setCursor(1, 1);
                  display.print(F("HST Amps: "));
                  display.println(String(FinalRMSCurrent));
                  display.print(F("DC Amps: "));
                  display.println(String(FinalDCCurrent));                                    
              #endif

              display.setCursor(8, StartRow + 2);
              display.println(F("ADC 3"));
              display.setTextColor(SSD1306_WHITE);
              display.setCursor(10,StartRow + 16);       
              display.setTextSize(1.5); 
              display.print(String(ADS1.toVoltage(ADS1.readADC(2)) * mutiplyer));
              display.setTextSize(1);        
      

              display.setCursor(rect_width + 10 ,StartRow + 2 );    
              display.println(F("ADC 4"));
              display.setCursor(rect_width + 8,StartRow + 16 );   
              display.setTextSize(1.5);                  
              display.print(String(ADS1.toVoltage(ADS1.readADC(3)) * mutiplyer));
              display.setTextSize(1);              
              display.setTextColor(WHITE);
             } 


      
    #endif
  }    
#endif

#ifdef _NEWMETHOD_
  void tskfn_PF() {  

      Serial.print("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> PF TASK: ");

      // Serial.println("Getting single-ended readings from AIN0..3");
      // Serial.println("ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)");
      // https://microcontrollerslab.com/wp-content/uploads/2019/05/ADS1115-external-ADC-with-ESP32.jpg
      // https://microcontrollerslab.com/ads1115-external-adc-with-esp32/


        /* 0- General */

        int decimalPrecision = 2;                   // decimal places for all values shown in LED Display & Serial Monitor


        /* 1 - Phase Angle, Frequency and Power Factor measurement */

        int expectedFrequency = 50;                 // Key in frequency for main grid (50 / 60 hz)
        int analogInputPin1PA = VoltagePin;                 // The input pin for analogRead1 sensor. Use voltage sensor as primary reference here.
        int analogInputPin2PA = CurrentPin;                 // The input pin for analogRead2 sensor. Use current or voltage as secondary reference.
        float voltageAnalogOffset =4;               // This is to offset analog value for analogInput1
        float currentAnalogOffset =-12;             // This is to offset analog value for analogInput2
        unsigned long startMicrosPA;                /* start counting time for Phase Angle and Period (in micro seconds)*/
        unsigned long vCurrentMicrosPA;             /* current time for analogInput1 (voltage) (in micro seconds). AnalogInput1 is used for reference for phase angle*/
        unsigned long iCurrentMicrosPA;             /* current time for analogInput2 (current/voltage) (in micro seconds).*/
        unsigned long periodMicrosPA;               /* current time for record period of wave */
        float vAnalogValue =0;                      /* is the analog value for voltage sensor / analogInput1 and center at 0 value */
        float iAnalogValue =0;                      /* is the analog value for current sensor / analogInput2 and center at 0 value */
        float previousValueV =0;                    /* use to record peak value for voltage sensor*/
        float previousValueI =0;                    /* use to record peak value for current sensro*/
        float previousphaseAngleSample=0;           /* previous sample reading to replace false value less than 100 micro seconds*/
        float phaseAngleSample =0;                  /* is the time difference between 2 sensor values (in micro seconds) */
        float phaseAngleAccumulate =0;              /* is the accumulate time difference for accumulate samples*/
        float periodSample=0;                       /* is the time difference for a period of wave for a sample (in micro seconds)*/
        float periodSampleAccumulate = 0;           /* is the accumulate time difference for accumulate samples */
        float phaseDifference =0;                   /* is the averaged set of time difference of 2 sensors*/
        float phaseAngle =0;                        /* is the phase angle in degree (out of 360)*/
        float frequency = 0;                        /* is the frequency of the voltage sensor wave*/
        float voltagePhaseAngle=0;                  /* is the time recorded from begining to reach peak value for analogInput1 in micro seconds*/
        float currentPhaseAngle=0;                  /* is the time recorded from begining to reach peak value for analogInput2 in micro seconds*/
        float averagePeriod =0;                     /* is the average set of time recorded for a period of wave */
        int sampleCount = 0;                        /* to count how many set of samples */
        float averageVp = 0 ;                       /* Average vp ksb*/
        int a = 3;                                  /* use for switching operation*/
        float powerFactor;                          /* to calculate power factor */

        float previousValueVAccumulate = 0;         /* Accumulated max Vp ksb*/
        float ADC_djuster = 3.3/4096;               /*3.3v over 4096 ADC steps = volts per step*/
        float resdividerFactor = 7500.0 / (7500.0 + 100000.0);
        float transformerRatio = (240.0 / 13.7);

      int t = millis();
      while((sampleCount != expectedFrequency)){

      MYbutton.tick(&MYbutton); 
      /* 1 - Phase Angle, Frequency and Power Factor measurement */
        
        vAnalogValue = analogRead(analogInputPin1PA) - 1975 + voltageAnalogOffset;       /* read analogInput1 with center adjusted*/
        //  Serial.print(vAnalogValue);
        //  Serial.print(";");
        iAnalogValue = analogRead(analogInputPin2PA) - 1930 + currentAnalogOffset;       /* read analogInput2 with center adjusted*/
        // Serial.print(iAnalogValue);
        if((vAnalogValue>0) && a == 3)                                          /* initial begining stage of measurement when analogInput1 wave larger than 0 */
        {
          a=0;                                                                  /* allow to change to the next stage */
        }
        
        if((vAnalogValue<=0) && a ==0)                                          /* when analog value of analogInput1 smaller or equal than 0*/
        {
          startMicrosPA = micros();                                             /* start counting time for all*/
          a=1;                                                                  /* allow to change to the next stage */
        }
        
        if((vAnalogValue>0) && a ==1)                                           /* when analog value of analogInput1 larger than 0*/
        {
          a = 2;                                                                /* allow to change to the next stage */
          previousValueV = 0;                                                   /* reset value. This value to be compared to measure peak value for analogInput1 */
          previousValueI = 0;                                                   /* reset value. This value to be compared to measure peak value for analogInput2 */
        }  
     
        if((vAnalogValue > previousValueV ) && a==2)                            /* if current measured value larger than previous peak value of analogInput1 */
        {
          previousValueV = vAnalogValue ;                                       /* record current measure value replace previous peak value */
          vCurrentMicrosPA = micros();                                          /* record current time for analogInput1 */
        }
        
        if((iAnalogValue > previousValueI) && a==2)                             /* if current measured value larger than previous peak value of analogInput2 */
        {
          previousValueI = iAnalogValue ;                                       /* record current measure value replace previous peak value */
          iCurrentMicrosPA = micros();                                          /* record current time for analogInput2 */
        }
  
        if((vAnalogValue <=0) && a==2)                                          /* when analog value of analogInput1 smaller or equal than 0*/
        {
          periodMicrosPA = micros();                                            /* record current time for 1 period */
          periodSample = periodMicrosPA - startMicrosPA;                        /* period wave is the current time minus the starting time (for 1 sample)*/
          periodSampleAccumulate = periodSampleAccumulate + periodSample;       /* accumulate or add up time for all sample readings of period wave */
          previousValueVAccumulate = previousValueVAccumulate + previousValueV; /* Accumulate Vp*/
          voltagePhaseAngle = vCurrentMicrosPA - startMicrosPA;                 /* time taken for analogInput1 from 0 (down wave) to peak value (up wave)*/
          currentPhaseAngle = iCurrentMicrosPA - startMicrosPA;                 /* time taken for analogInput2 from 0 (down wave) to peak value (up wave)*/
          phaseAngleSample = currentPhaseAngle - voltagePhaseAngle;             /* time difference between analogInput1 peak value and analogInput2 peak value*/
          if(phaseAngleSample>=100)                                             /* if time difference more than 100 micro seconds*/
          {
          previousphaseAngleSample = phaseAngleSample;                          /* replace previous value using new current value */ 
          }
          if(phaseAngleSample<100)                                              /* if time difference less than 100 micro seconds (might be noise or fake values)*/
          {
          phaseAngleSample = previousphaseAngleSample;                          /* take previous value instead using low value*/
          }
          phaseAngleAccumulate = phaseAngleAccumulate + phaseAngleSample;       /* accumulate or add up time for all sample readings of time difference */
          sampleCount = sampleCount + 1;                                        /* count sample number */
          startMicrosPA = periodMicrosPA;                                       /* reset begining time */
          a=1;                                                                  /* reset stage mode */
          previousValueV = 0;                                                   /* reset peak value for analogInput1 for next set */
          previousValueI = 0;                                                   /* reset peak value for analogInput2 for next set */
        }
      }

        if(sampleCount == expectedFrequency)                                          /* if number of total sample recorded equal 50 by default */
        {
          averageVp = previousValueVAccumulate/sampleCount;                           /* avverage Vp ksb*/
          averagePeriod = periodSampleAccumulate/sampleCount;                         /* average time for a period of wave from all the sample readings*/
          frequency = 1000000 / averagePeriod;                                        /* the calculated frequency value */
          phaseDifference = phaseAngleAccumulate / sampleCount;                       /* average time difference between 2 sensor peak values from all the sample readings */
          phaseAngle = ((phaseDifference*360) / averagePeriod);                       /* the calculated phase angle in degree (out of 360)*/
          powerFactor = cos(phaseAngle*0.017453292);                                  /* power factor. Cos is in radian, the formula on the left has converted the degree to rad. */

          float adjusted_Vp = averageVp * ADC_djuster * 0.707 ;
          adjusted_Vp = adjusted_Vp / resdividerFactor;
          adjusted_Vp = adjusted_Vp * transformerRatio;

          //float V_RATIO = ((0.57 * ((3300/1000.0) / (4096)))/resdivider) * transformerRatio; 
          float averageV = adjusted_Vp;// * 0.85713; // adjustment for component errors and deviations

          debugV ("\nPhase Angle : %f, Frequency : %f, Power Factor : %f Average Vpeak : %f, Voltage : %f" , phaseAngle, frequency , powerFactor, averageVp, averageV);   

          Serial.print("Time taken :");
          Serial.print(millis() - t);
          Serial.print("ms ");
          Serial.print("Phase Angle :");
          Serial.print(phaseAngle,decimalPrecision);
          Serial.print("Â°  ");
          Serial.print("Frequency :");
          Serial.print(frequency,decimalPrecision);
          Serial.print("Hz  ");
          Serial.print("Power Factor :");
          Serial.print(powerFactor,decimalPrecision);
          Serial.print(" Average Vpeak :");
          Serial.println(averageVp,decimalPrecision);          
          Serial.print(" Voltage :");
          Serial.println(averageV,decimalPrecision);   

          sampleCount = 0;                                                            /* reset the sample counting quantity */
          periodSampleAccumulate = 0;                                                 /* reset the accumulated time for period wave from all samples */
          phaseAngleAccumulate = 0;                                                   /* reset the accumulated time for time difference from all samples*/
          previousValueVAccumulate = 0;

            if (CURRENT_Display_Action == ACTION_DISPALY_4) {
              #ifdef OLED_1306
              display.clearDisplay();
              int8_t StartRow = 22;
              int8_t rect_width = 60;
              display.drawRect(0,StartRow,rect_width,30, WHITE);
              display.drawRect(rect_width + 2 ,StartRow,rect_width,30, WHITE);

              display.setCursor(8, StartRow + 2);
              display.println(F("PF"));
              display.setTextColor(SSD1306_WHITE);
              display.setCursor(10,StartRow + 16);       
              display.setTextSize(1.5); 
              display.print(String(powerFactor));
              display.setTextSize(1);        
      
              display.setCursor(rect_width + 10 ,StartRow + 2 );    
              display.println(F("Freq.HZ."));
              display.setCursor(rect_width + 8,StartRow + 16 );   
              display.setTextSize(1.5);                  
              display.print(String(frequency));
              display.setTextSize(1);              
              display.setTextColor(WHITE);
              #endif
             } 

        }   

  }    
#endif

