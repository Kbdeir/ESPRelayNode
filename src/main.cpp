/// public time server 129.6.15.28
// #define USEPREF n
// SW V 10.0 @ 25/09/2022
// // esp_core_dump_image_erase(); uncomment to erase coredump

//High impact:

//Chronos (7 KB code + unknown rodata) — used only for the day/month/year tracking in CT_ProcessPower.cpp. You replaced it with just _nd, _nm, _ny, _nh from Chronos::DateTime::now(). You could swap the Chronos call for a direct NTP time struct read (time_t now = time(nullptr); struct tm *t = localtime(&now)) — that's already in the system, zero extra library cost.
//mDNS (23 KB) — check if it's actively used/needed. If not enabled in defines.h it shouldn't link, but worth verifying.


#include <FS.h>
#include <new>
#include <exception>

#include <defines.h>
#include <FirmwareUpdateState.h>

#ifdef USE_LittleFS
   #ifdef ESP32 
    #define SPIFFS LITTLEFS
    #else 
    #define SPIFFS LittleFS
   #endif
  #include <LittleFS.h>
#else
  #include <SPIFFS.h>
#endif 

#include <Wire.h>
#ifdef ESP32
#include "esp_adc_cal.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "SerialLog.h"
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


#ifdef ESP_NOW
  #include <espnow.h>
#endif 

#include <Arduino.h>
#include <string.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include "ACS712.h"
// #include "OneButton.h"
// #include <Bounce2.h>
#include <RelayClass.h>
#include <vector>
#include <ACS_Helper.h>
#include <InputClass.h>
#include <TimerClass.h>
#ifdef _AUTOMATION_RULES_
#include <AutomationRules.h>
#endif
#ifdef _REMOTE_SENSORS_
#include <RemoteSensorConfig.h>
#endif
#include <TempSensor.h>
#include <TempConfig.h>
#include <ADS11x5Config.h>

#ifdef _HST_
#include <HSTConfig.h>
#endif

#ifdef ESP32
  #include <ESP32Ping.h>
#else
 // #include "AsyncPing.h"
#endif

#ifdef ESP32
#ifdef WaterFlowSensor
#include <WaterFlowSensor.h>
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

//OneButton MYbutton(ConfigInputPin, true);
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
          float manualOffset = 0;                      // loaded from PAHSTConfig.manualOffset each task run
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

  float g_adsV2 = 0.0f, g_adsV3 = 0.0f;
  float g_adsMultiplier = 1.0f;
  #ifdef _ADS1X15_DC_Current_
  float g_adsFinalRMSCurrent = 0.0f, g_adsFinalDCCurrent = 0.0f;
  #endif

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
      CTPROCESSOR CT_1(CurrentPin, 30, VoltagePin, 191.0, 1.7, RelayCT_HighConsumption_func, RelayCT_NormalConsumption_func);

      // FreeRTOS task that runs calcVI() on core 0 so it never blocks the Arduino loop.
      // tskfn_emon_reader() triggers it via xTaskNotifyGive and reads results once ready.
      // ctMeasurementActive prevents notification accumulation: if tskfn_emon_reader fires
      // while calcVI is still running, it does NOT send another notification, avoiding the
      // back-to-back measurement loop that starves the core-0 IDLE task watchdog.
      static TaskHandle_t ctMeasurementTask_handle = nullptr;
      static volatile bool ctReadingReady    = false;
      static volatile bool ctMeasurementActive = false;

      void ctMeasurementTask(void* param) {
          for (;;) {
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              if (firmwareUpdateInProgress) {
                  ctReadingReady = false;
                  ctMeasurementActive = false;
                  continue;
              }
              ctMeasurementActive = true;
              if (firmwareUpdateInProgress) {
                  ctMeasurementActive = false;
                  continue;
              }
              CT_1.readPower(MyConfParam.v_CT_adjustment, MyConfParam.v_CT_saveThreshold);
              ctReadingReady = true;
              ctMeasurementActive = false;
              // Yield briefly so the core-0 IDLE task can run and reset the task watchdog.
              vTaskDelay(pdMS_TO_TICKS(20));
          }
      }
  #endif
  void tskfn_EmonPublisher();
  Task tskEmonPublisher(5000, TASK_FOREVER, &tskfn_EmonPublisher, &Scheduler_ts, false);
  void tskfn_emon_reader();
  Task tskEmonReader(2000, TASK_FOREVER, &tskfn_emon_reader, &Scheduler_ts, false);  

  void applyPowerTaskIntervals() {
    uint32_t readerInterval = MyConfParam.v_EmonReaderIntervalMs;
    uint32_t publisherInterval = MyConfParam.v_EmonPublisherIntervalMs;
    if (readerInterval < 500UL) readerInterval = 500UL;
    if (publisherInterval < 1000UL) publisherInterval = 1000UL;
    tskEmonReader.setInterval(readerInterval);
    #ifdef _emonlib_
    tskEmonPublisher.setInterval(publisherInterval);
    #endif
    Serial.printf("[CT     ] Power task intervals: reader=%lu ms, publisher=%lu ms\n",
                  static_cast<unsigned long>(readerInterval),
                  static_cast<unsigned long>(publisherInterval));
  }

  #ifdef _emonlib_
  // Re-applies CT calibration constants to the running EnergyMonitor instance
  // so VCal/PhaseCal/CT max current changes saved via the web UI take effect
  // immediately, without rebooting.
  void applyCTCalibration() {
    CT_1.emon1.current(CurrentPin, MyConfParam.v_CurrentTransformer_max_current);
    CT_1.emon1.voltage(VoltagePin, MyConfParam.v_calibration, MyConfParam.v_PhaseCal);
    Serial.printf("[CT     ] Calibration applied: ICAL=%u VCAL=%.2f PHASECAL=%.2f\n",
                  (unsigned)MyConfParam.v_CurrentTransformer_max_current,
                  MyConfParam.v_calibration, MyConfParam.v_PhaseCal);
  }
  #endif


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

  void applyOledTaskInterval() {
    uint32_t oledInterval = MyConfParam.v_OLEDUpdateIntervalMs;
    if (oledInterval < 500UL) oledInterval = 500UL;
    if (oledInterval > 4000UL) oledInterval = 4000UL;
    MyConfParam.v_OLEDUpdateIntervalMs = oledInterval;
    tskOLEDUpdate.setInterval(oledInterval);
    Serial.printf("[OLED   ] Screen update interval: %lu ms\n",
                  static_cast<unsigned long>(oledInterval));
  }
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
unsigned long lastMillis5000 = 600; // offset 600ms from 5s boundary to avoid CT/emon collision
bool tempConversionPending = false; // true while waiting 800ms for DS18B20 conversion
#ifdef HWESP32
bool tempSensorPin01Begun = false;
bool tempSensorPin02Begun = false;
#endif
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
#include <HADiscovery.h>
#include <WireGuardManager.h>
#include <AsyncHTTP_Helper.h>
#ifdef ESP32
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#endif

extern AsyncMqttClient mqttClient;

bool homekitNotInitialised = true;
bool HomespanInitiated = false;

time_t prevDisplay = 0; // when the digital clock was displayed
#include <Chronos.h>

NodeTimer NTmr(4);
#ifdef _AUTOMATION_RULES_
AutomationRule ARule(4);
#endif
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
#define DEFAULT_BOUNCE_TIME 100
#define CAL_MAX_NUM_EVENTS_TO_HOLD 28 // 4 timers × 7 weekdays = 28 max events

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
  TLPressureSensor TL136(35u, 0, 400, 150);
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

#ifdef OLED_1306
volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 400; // ms — suppresses bounce and prevents accidental double-cycle
volatile unsigned long buttonPressStartMs = 0;   // wall-clock time of last validated press-down
bool buttonLongPressHandled = false;              // true after long-press fires; suppresses short-press on release
unsigned long powerResetConfirmUntilMs = 0;       // show "power reset" confirmation until this timestamp
unsigned long rebootConfirmUntilMs = 0;           // show "rebooting" confirmation until this timestamp
static const unsigned long OLED_AUTO_SLEEP_MS = 30000UL;
static volatile bool oledWakePending = true;
static bool oledAwake = true;
static unsigned long oledLastWakeMs = 0;
static volatile bool oledWasAsleepOnPress = false; // set in ISR; suppresses page-cycle for wake-only press

#define GPIO0_BUTTON_PIN 0
volatile bool gpio0ButtonPressed = false;
volatile unsigned long gpio0LastInterruptTime = 0;
volatile unsigned long gpio0PressStartMs = 0; // wall-clock time of last validated GPIO0 press-down
bool gpio0LongPressHandled = false;            // true after long-press fires
const unsigned long GPIO0_LONG_PRESS_MS = 30000UL;   // hold time to trigger the current GPIO0 menu action
const long AP_CONFIG_MODE_MAX_SECONDS = 60 * 10;     // temporary config AP mode auto-closes after 10 minutes

// GPIO0 short-click menu: cycles through maintenance actions, mirroring the
// ConfigInputPin page menu (myClickFunction). NONE means GPIO0's overlay is
// not shown and the normal OLED page (CURRENT_Display_Action) is displayed.
typedef enum {
  GPIO0_ACTION_NONE,
  GPIO0_ACTION_AP_MODE,
  GPIO0_ACTION_CONFIG_RESET
} Gpio0MenuAction;
Gpio0MenuAction CURRENT_GPIO0_Action = GPIO0_ACTION_NONE;
unsigned long configResetConfirmUntilMs = 0; // show "Initializing..." confirmation until this timestamp
bool gpio0ApModeActive = false; // true while the temporary GPIO0-triggered config AP mode is running
#endif

long timezone     = 1;
byte xtries = 0;
byte daysavetime  = 1;
volatile bool pending_mqtt_connect = false;
unsigned long mqttSubscribedAt    = 0; // millis() when mqttSubscribeRelays() last ran
unsigned long relay_state_next_ms = 0; // when to send the next staggered relay-state pub
int wifimode      = WIFI_CLT_MODE;

double secondson = 0;

String MAC;
uint32_t trials   = 0;

// Heap largest-free-block sampled from loop() (no TCP buffers held).
// RTC_DATA_ATTR places these in RTC slow memory — zero DRAM BSS impact.
RTC_DATA_ATTR uint32_t g_heapMaxBlkCached = 0;
RTC_DATA_ATTR uint32_t g_heapNextSampleMs = 0;
int  WFstatus;
int UpCount       = 0;
WiFiClient net;
String APssid     = (String("Node-") +  CID() + "-");
const char* APpassword = "12345678";
       		      
long APModetimer  = 60*5;                     // max allowed time in AP mode, reset if exceeded
long APModetimer_run_value = 0;               // timer value to track the AP mode rining-timer value. reset board if it exceeds APModetimer
AsyncServer* MBserver = nullptr;              // Modbus TCP server — instantiate and call begin() to enable
ModbusIP mb;
const int LAMP1_COIL  = 0;
const int LAMP2_COIL  = 1;
float old_acs_value   = 0;
float ACS_I_Current   = 0;

float MCelcius;
float MCelcius2;

#if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
  #ifdef TempSensorPin
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
#ifdef OLED_1306
void oledWake();
#endif



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

            if (len >= 4 && memcmp(incomingData, "PING", 4) == 0) {
            Serial.println("[PING] Received — replying with PONG...");
            esp_now_send(mac, (uint8_t*)"PONG", strlen("PONG"));
            return;
        }
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
      unsigned long currentMillis_WFS=0;
      unsigned long previousMillis_WFS=0;
      unsigned long interval=5000;  // 5 s: reduces MQTT publish rate; flow calc uses elapsed ms so accuracy is unchanged
      volatile uint32_t pulseCount;
      volatile uint32_t pulseCountCumulative = 0;  // never reset; used by web API and OLED for per-second delta
      portMUX_TYPE pulseCountMux = portMUX_INITIALIZER_UNLOCKED;
      uint32_t pulse1Sec = 0;
      uint32_t wfsPulsesPerSec = 0;
      float flowRate;
      double flowMilliLitres;      // floating-point to avoid truncation drift
      uint64_t totalMilliLitres;
      unsigned long wfsPersistMs = 0; // timestamp of last totalMilliLitres save


void IRAM_ATTR pulseCounter() // Interupt function for WaterFlowSensor
{
  portENTER_CRITICAL_ISR(&pulseCountMux);
  pulseCount++;
  pulseCountCumulative++;
  portEXIT_CRITICAL_ISR(&pulseCountMux);
}
#endif
#endif


void myClickFunction() {
  //Serial.print(F("\n[INFO   ] >>>>> Conf Button Clicked <<<<< \n")); //MYbUTTON
  #ifdef OLED_1306
 // display.dim(true);
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
    #ifdef _emonlib_
    CURRENT_Display_Action = ACTION_DISPALY_5;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 5 (Reset) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 5 <<");
    #elif defined(WaterFlowSensor)
    CURRENT_Display_Action = ACTION_DISPALY_6;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 6 (Flow) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 6 <<");
    #else
    CURRENT_Display_Action = ACTION_DISPALY_7;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 7 (Reboot) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 7 <<");
    #endif
  } else if (CURRENT_Display_Action == ACTION_DISPALY_4) {
    CURRENT_Display_Action = ACTION_DISPALY_1;
    display.setCursor(0,0);
    display.println(">> Page 1 <<");
  } else if (CURRENT_Display_Action == ACTION_DISPALY_5) {
    #ifdef WaterFlowSensor
    CURRENT_Display_Action = ACTION_DISPALY_6;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 6 (Flow) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 6 <<");
    #else
    CURRENT_Display_Action = ACTION_DISPALY_7;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 7 (Reboot) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 7 <<");
    #endif
#ifdef WaterFlowSensor
  } else if (CURRENT_Display_Action == ACTION_DISPALY_6) {
    CURRENT_Display_Action = ACTION_DISPALY_7;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 7 (Reboot) <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 7 <<");
#endif
  } else if (CURRENT_Display_Action == ACTION_DISPALY_7) {
    CURRENT_Display_Action = ACTION_DISPALY_1;
    Serial.print(F("\n[INFO   ] >>>>> PAGE 1 <<<<< \n"));
    display.setCursor(0,0);
    display.println(">> Page 1 <<");
  }
  display.println("");   
  display.println("");   
  display.println(">>    WAIT !!!   <<");  
  display.display();
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

static void forceAllRelayPinsOff()
{
  pinMode(RelayPin, OUTPUT);
  digitalWrite(RelayPin, LOW);

  #if defined(ESP32_2RBoard) || defined(HWver03_4R)
    pinMode(Relay1Pin, OUTPUT);
    digitalWrite(Relay1Pin, LOW);
  #endif

  #ifdef HWver03_4R
    pinMode(Relay2Pin, OUTPUT);
    digitalWrite(Relay2Pin, LOW);
    pinMode(Relay3Pin, OUTPUT);
    digitalWrite(Relay3Pin, LOW);
  #endif
}

static void persistRelayOff(uint8_t relayNumber)
{
  StaticJsonDocument<100> json;
  json[F("status")] = 0;

  char filename[24];
  snprintf(filename, sizeof(filename), "/relayPersist%u.json", relayNumber);

  File configFile = SPIFFS.open(filename, "w");
  if (!configFile) {
    Serial.print(F("[SYSTEM ] Failed to open relay persist file: "));
    Serial.println(filename);
    return;
  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.print(F("[SYSTEM ] Failed to write relay persist file: "));
    Serial.println(filename);
  }
  configFile.print("\n");
  configFile.close();
}

static void persistAllRelaysOff()
{
  persistRelayOff(0);

  #if defined(ESP32_2RBoard) || defined(HWver03_4R)
    persistRelayOff(1);
  #endif

  #ifdef HWver03_4R
    persistRelayOff(2);
    persistRelayOff(3);
  #endif
}


void ticker_ACS712_mqtt (void* relaySender) {
  if (relaySender != nullptr) {
    Relay * rly = static_cast<Relay *>(relaySender);
    if (rly->RelayConfParam->v_ACS_Active) {
        float variance =(abs(ACS_I_Current-old_acs_value));
        // Serial.printf("%6.*lf", 2, variance );
        if (true) {//(variance > 0.05){
           { char _ab[12]; snprintf(_ab, sizeof(_ab), "%.2f", (double)ACS_I_Current); mqttPublish(rly->RelayConfParam->v_ACS_AMPS.c_str(), 0, true, _ab); }
          Serial.printf("\n I = %.2f A\n", (double)ACS_I_Current);
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
    char st[12];
    snprintf(st, sizeof(st), "%lu", (unsigned long)t);
    if (rly->readrelay() == HIGH)
    {
        mqttPublish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, st);
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
    mqttPublish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF); 
    if (rly->RelayConfParam->v_ttl > 0)
    {
        mqttPublish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
    }
  }
}

void onRelaychangeInterruptSvc(void *relaySender)
{
  #ifdef OLED_1306
  oledWake();
  #endif
  Relay *rly = static_cast<Relay *>(relaySender);
  //  if (mqttClient.connected()) {
  //if (rly->rchangedflag)   {
    
    // post ON/OFF
    mqttPublish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, rly->readrelay() == HIGH ? ON : OFF );
    {
      // v_PUB_TOPIC1 is also the subscribed command topic, so the broker
      // echoes this publish back to us. Only (re)publish when the value
      // actually changes, and mark the echo as expected so onMqttMessage
      // can ignore it instead of re-applying it to the relay.
      int8_t curState = rly->readrelay() == HIGH ? 1 : 0;
      if (rly->lastPub1State != curState) {
        rly->lastPub1State = curState;
        rly->pendingEchoCount++;
        mqttPublish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, curState ? ON : OFF );
      }
    }
    #ifdef DEBUG_ENABLED
      debugV("* posting status: %s %s", rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), rly->readrelay() == HIGH ? ON : OFF );
    #endif

    if (rly->readrelay() == HIGH)
    {
        //Serial.println(F("[INFO   ] interrupt *ON* occurred."));
        if (rly->RelayConfParam->v_ttl > 0) {
            rly->start_ttl_timer();
            // post v_ttl value once on "on", post v_curr_ttl in the timer
            //Serial.println(F("[INFO   ] posting interrupt *on*"));
            { char ttlBuf[12]; snprintf(ttlBuf, sizeof(ttlBuf), "%d", rly->RelayConfParam->v_ttl); mqttPublish(rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, ttlBuf); }
            //Serial.println(F("[INFO   ] posting interrupt *on* done"));
        }
    }
    else
    { // (rly->readrelay() == LOW)
        // Serial.println(F("[INFO   ] interrupt *OFF* occurred."));
        rly->stop_ttl_timer();
        if (rly->RelayConfParam->v_ttl > 0) {
            mqttPublish(rly->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str(), QOS2, NOT_RETAINED, "0");
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
  #ifdef OLED_1306
  oledWake();
  #endif
  Serial.println(F("[INFO   ] processing Inputs"));
  if (inputSender != nullptr) {
      InputSensor * snsr = static_cast<InputSensor *>(inputSender);
    if (snsr->fclickmode == INPUT_NORMAL) {
      SLOG_IF(SLOG_MQTT) { Serial.println(("[MQTT   ]" + snsr->mqtt_topic).c_str()); }
      mqttPublish( snsr->mqtt_topic.c_str(), 0, RETAINED, digitalRead(snsr->pin) == HIGH ?  ON : OFF);
    }
    if (snsr->fclickmode == INPUT_TOGGLE) {
      mqttPublish( snsr->mqtt_topic.c_str(), 0, RETAINED, TOG);
    }
  }
}


void onchangeSwitchInterruptSvc(void* relaySender, void* inputSender){
  #ifdef OLED_1306
  oledWake();
  #endif
  Relay * rly = static_cast<Relay *>(relaySender);
  InputSensor * input = static_cast<InputSensor *>(inputSender);
  Serial.print("\n [INFO ] onchangeSwitchInterruptSvc");
  Serial.print("\n "); Serial.print(digitalRead(input->pin));
  rly->mdigitalWrite(rly->getRelayPin(),digitalRead(input->pin)); //rly->getRelaySwithbtnState());
  mqttPublish(input->mqtt_topic.c_str(), 0, RETAINED,digitalRead(input->pin) == HIGH ? ON : OFF);
}


// this function will be called when button is clicked.
void buttonclick(void* relaySender, void* inputSender) {
  #ifdef OLED_1306
  oledWake();
  #endif
  Serial.print(F("\n [INFO ] buttonclick"));
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
    mqttPublish(input->mqtt_topic.c_str(), 0, RETAINED,TOG);
  }
}

void TempertatureSensorEvent(int rlynb, float TSolarPanel, float TSolarTank) {
  Serial.print(F("\n[INFO   ] TempertatureSensorEvent"));
  Relay * rtmp = getrelaybynumber(rlynb);
  if (!rtmp) { Serial.println(F(" — invalid relay index, skipped")); return; }

  Serial.print(F("\n[INFO   ] TSolarPanel = ")); Serial.print(TSolarPanel);
  Serial.print(F("  TSolarTank = "));           Serial.print(TSolarTank);

  if (!PTempConfig.enabled) {
    Serial.println(F("\n[INFO   ] temperature control disabled — skipped"));
    return;
  }

  if ((PTempConfig.spanTempfrom != 0) && (PTempConfig.spanBuffer != 0)) {
    bool belowLow  = TSolarPanel < PTempConfig.spanTempfrom;
    bool aboveHigh = (PTempConfig.spanTempto != 0) && (TSolarPanel > PTempConfig.spanTempto);

    if (belowLow || aboveHigh) {
      rtmp->mdigitalWrite(rtmp->getRelayPin(), LOW);
      Serial.print(F("\n[INFO   ] relay OFF — panel out of ["));
      Serial.print(PTempConfig.spanTempfrom); Serial.print(F(", ")); Serial.print(PTempConfig.spanTempto); Serial.print(F("]"));
      return;
    }

    if ((TSolarPanel - TSolarTank) > PTempConfig.spanBuffer) {
      rtmp->mdigitalWrite(rtmp->getRelayPin(), HIGH);
      Serial.print(F("\n[INFO   ] relay ON  — panel–tank diff > buffer ("));
      Serial.print(PTempConfig.spanBuffer); Serial.print(F(")"));
    } else if ((TSolarPanel - TSolarTank) < PTempConfig.spanBuffer) {
      rtmp->mdigitalWrite(rtmp->getRelayPin(), LOW);
      Serial.print(F("\n[INFO   ] relay OFF — panel–tank diff < buffer ("));
      Serial.print(PTempConfig.spanBuffer); Serial.print(F(")"));
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
            // Disconnect first to flush driver state from the previous failed attempt.
            // This used to happen (as a side-effect) inside WiFiDisconnected, but that
            // caused a second disconnect event. Doing it here — just before WiFi.begin()
            // — is the correct place to clear stale state for a clean reconnect.
            WiFi.disconnect();
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
  #ifdef OLED_1306
  oledWake();
  #endif
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
  int calEventsAdded = 0;
  PRINTLN(F("[Events ] Starting up PointsEvents test"));
  Chronos::DateTime::setTime(year(), month(), day(), hour(), minute(), second()); 
  //Chronos::DateTime::setTime(2018, 12, 7, 18, 00, 00);

  uint8_t tcounter = 1;
  #ifndef ESP32 
  ESP.wdtFeed();
  #endif
  while(tcounter <= MAX_NUMBER_OF_TIMERS) { [&]()
    {
        #ifndef ESP32
        ESP.wdtFeed();
        #else
        esp_task_wdt_reset();
        #endif
        char timerfilename[20];
        snprintf(timerfilename, sizeof(timerfilename), "/timer%u.json", (unsigned)tcounter);

        config_read_error_t res = loadNodeTimer(timerfilename,NTmr);
        #ifndef ESP32
        ESP.wdtFeed();
        #else
        // Yield so the IDLE tasks (which feed their own TWDT entries) get
        // scheduled between flash reads — LittleFS I/O disables the cache on
        // both cores, and back-to-back reads here can starve IDLE0/IDLE1 long
        // enough to trip the watchdog (see Crash #7: abort in task_wdt_isr).
        vTaskDelay(1);
        #endif
        tcounter++;

        // A TIMER_NULL_RELAY timer exists purely as a schedule window (e.g. for
        // Automation Rules' time-gating, isNodeTimerActiveNow() — TimerClass.cpp:66)
        // — it must never enter the Chronos calendar: NTmr.relay also doubles as
        // the Chronos::Event id and the EventNames[] index (main.cpp:1582), and
        // 255 is out of bounds for both.
        if ((res == SUCCESS) && NTmr.enabled && NTmr.relay != TIMER_NULL_RELAY) {
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
            // Weekly recurring timers must always be added to the calendar regardless
            // of whether today falls within the date span — the Chronos Mark::Weekly
            // event handles day-of-week matching internally.
            {
            Serial.print(F("\n entered WEEKLY TIMER MODE eval 1"));

            uint32_t dailydiffsecs = timeDiff.totalSeconds() - (timeDiff.days() * 24 * 3600);
            if (NTmr.Mark_Hours + NTmr.Mark_Minutes > 0) {
              dailydiffsecs = (NTmr.Mark_Hours * 3600) +  (NTmr.Mark_Minutes * 60) ;
            }

#define CAL_ADD(event) do { \
  if (calEventsAdded < CAL_MAX_NUM_EVENTS_TO_HOLD) { MyCalendar.add(event); calEventsAdded++; } \
  else { Serial.println(F("[TIMERS ] Calendar full — event skipped!")); } \
} while(0)
              if (NTmr.weekdays->Sunday) {
                CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Sunday,Hour, Minute, 00),
                    Chronos::Span::Seconds(dailydiffsecs)));
              }
                if (NTmr.weekdays->Monday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Monday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
               }
                if (NTmr.weekdays->Tuesday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Tuesday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
                }
                if (NTmr.weekdays->Wednesday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Wednesday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
                 }
                 if (NTmr.weekdays->Thursday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Thursday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
                  }
                  if (NTmr.weekdays->Friday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Friday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
                   }
                   if (NTmr.weekdays->Saturday) {
                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Weekly(Chronos::Weekday::Saturday,Hour, Minute, 00),
                      Chronos::Span::Seconds(dailydiffsecs)));
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

                  CAL_ADD(Chronos::Event(NTmr.relay,Chronos::Mark::Daily(Hour, Minute, 00),
                        Chronos::Span::Seconds(dailydiffsecs)));
                }
          }

          if (NTmr.TM_type == TimerType::TM_MONTHLY_SPAN) {
            if ((previous.startOfDay() <= Chronos::DateTime::now()) && (Chronos::DateTime::now() <= next.endOfDay())) {
              uint8_t mday = (NTmr.monthDay > 0 && NTmr.monthDay <= 31) ? NTmr.monthDay : (uint8_t)Day;
              uint32_t monthlydiffsecs;
              if (NTmr.Mark_Hours + NTmr.Mark_Minutes > 0) {
                monthlydiffsecs = (NTmr.Mark_Hours * 3600UL) + (NTmr.Mark_Minutes * 60UL);
              } else {
                monthlydiffsecs = timeDiff.totalSeconds() - ((uint32_t)timeDiff.days() * 24UL * 3600UL);
              }
              Serial.printf("[TIMERS ] Monthly timer: day %u of month, %02d:%02d, duration %lus\n",
                            (unsigned)mday, Hour, Minute, (unsigned long)monthlydiffsecs);
              CAL_ADD(Chronos::Event(NTmr.relay,
                  Chronos::Mark::Monthly(mday, Hour, Minute, 0),
                  Chronos::Span::Seconds(monthlydiffsecs)));
            }
          }

          if (NTmr.TM_type == TimerType::TM_FULL_SPAN) {
            if (NTmr.Mark_Hours + NTmr.Mark_Minutes > 0) {
              uint32_t dursecs = (NTmr.Mark_Hours * 3600UL) + (NTmr.Mark_Minutes * 60UL);
              Chronos::DateTime computedNext = previous + (Chronos::EpochTime)dursecs;
              CAL_ADD(Chronos::Event(NTmr.relay, previous, computedNext));
            } else {
              CAL_ADD(Chronos::Event(NTmr.relay, previous, next));
            }
          }

        } // if SUCCESS
    } (); // anonym. func.
  } // while loop

#undef CAL_ADD
  Serial.printf("[TIMERS ] %d calendar event(s) loaded (cap %d)\n", calEventsAdded, CAL_MAX_NUM_EVENTS_TO_HOLD);
  // Populate the in-memory timer schedule cache so /api/live can serve
  // schedule data without opening LittleFS files on every poll.
  refreshAllTimerCache();
  Serial.println(F("[TIMERS ] Timer schedule cache refreshed"));
  #ifdef _AUTOMATION_RULES_
  refreshAllAutomationRuleCache();
  Serial.println(F("[AUTOMAT] Automation rule cache refreshed"));
  #endif
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


void chronosevaluatetimers(Calendar& MyCalendar) {
  if (ftimesynced){
  // static: keeps this off the stack (avoids stack overflow with larger calendar sizes)
  static Chronos::Event::Occurrence occurrenceList[CAL_MAX_NUM_EVENTS_TO_HOLD];
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
          if ((nowTime == occurrenceList[i].finish - 1 )) {
            // OFF takes priority — checked first so a relay is never turned ON
            // and immediately OFF in the same pass at the last second.
            LINE();
            rly->lockupdate = false;
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
            PRINTLN(F("[INFO   ] turning relay OFF... event is done"));
            rly->timerpaused = false;
            rly->hastimerrunning = false;
          } else if ((nowTime > occurrenceList[i].start + 1)) {
            rly->hastimerrunning = true;
            PRINTLN(F("\n[INFO   ] turning relay [ON]... event is Starting"));
            if (!digitalRead(rly->getRelayPin())){
              if (!rly->timerpaused) {
                rly->mdigitalWrite(rly->getRelayPin(),HIGH);
              }
            }
          }
        } // if rly !=nullptr
//      } // if rly in relys vector

    } // for loop
  } // if numongoing > 0
  else {
    // No events ongoing — release any relay that a timer left ON (handles missed finish-1 on reboot)
    for (auto it : relays) {
      Relay *rly = static_cast<Relay*>(it);
      if (rly && rly->hastimerrunning) {
        rly->lockupdate = false;
        rly->mdigitalWrite(rly->getRelayPin(), LOW);
        rly->hastimerrunning = false;
        rly->timerpaused    = false;
        PRINTLN(F("[TIMERS ] No ongoing events — releasing timer-held relay"));
      }
    }
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

    #ifdef ESP32
    // Some routers hand out a DNS server that doesn't resolve external names
    // (or none at all), which kills NTP / MQTT-broker lookups when VPN is off.
    // Log what DHCP gave us and install a public fallback into the secondary
    // slot if it's empty so lwIP has somewhere to fall back to.  We only fill
    // empty slots so a configured WireGuard DNS (set later by the tunnel) is
    // not clobbered.
    {
      LOCK_TCPIP_CORE();
      const ip_addr_t *dns0 = dns_getserver(0);
      const ip_addr_t *dns1 = dns_getserver(1);
      const bool dns0Empty = (!dns0) || ip_addr_isany(dns0);
      const bool dns1Empty = (!dns1) || ip_addr_isany(dns1);
      char dns0Txt[24] = "(none)";
      char dns1Txt[24] = "(none)";
      if (dns0 && !ip_addr_isany(dns0)) ipaddr_ntoa_r(dns0, dns0Txt, sizeof(dns0Txt));
      if (dns1 && !ip_addr_isany(dns1)) ipaddr_ntoa_r(dns1, dns1Txt, sizeof(dns1Txt));
      Serial.print(F("[WIFI   ] DHCP DNS: "));
      Serial.print(dns0Txt);
      Serial.print(F(" / "));
      Serial.println(dns1Txt);

      if (dns0Empty) {
        ip_addr_t fb;
        IP_ADDR4(&fb, 8, 8, 8, 8);
        dns_setserver(0, &fb);
        Serial.println(F("[WIFI   ] DNS primary empty; installed fallback 8.8.8.8"));
      }
      if (dns1Empty) {
        ip_addr_t fb;
        IP_ADDR4(&fb, 1, 1, 1, 1);
        dns_setserver(1, &fb);
        Serial.println(F("[WIFI   ] DNS secondary empty; installed fallback 1.1.1.1"));
      }
      UNLOCK_TCPIP_CORE();
    }
    #endif

    Serial.println(F("[INFO   ] Starting UDP"));

    // #ifdef ESP8266
    #ifdef blockingTime
      Udp.begin(localPort);
    #else
      // AsyncNTP
      Serial.print(F("[NTP    ] >> Time server: "));
      Serial.println(MyConfParam.v_timeserver);
      ntpBegin();
    #endif

    static bool mdnsStarted = false;
    if (!mdnsStarted) {
      Serial.println(F("[INFO   ] starting MDNS"));
      if (!MDNS.begin((MyConfParam.v_PhyLoc).c_str())) {
        Serial.println(F("[INFO   ] Error setting up MDNS responder!"));
      } else {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
        Serial.println(F("[INFO   ] MDNS responder started"));
      }
    }

    // MDNS.addServiceTxt("http", "tcp","MQTT server", MyConfParam.v_MQTT_BROKER.toString().c_str());
    // MDNS.addServiceTxt("http", "tcp","Chip", String(MAC.c_str()) + " - Chip id: " + CID());

    #ifdef blockingTime
      setSyncProvider(getNtpTime);
    #else
      setSyncProvider(timeprovider); // [time]
      setSyncInterval(10); 
    #endif

    #ifdef DEBUG_ENABLED
      debugV("* MQTT active flag: %u", MyConfParam.v_MQTT_Active);
    #endif

    SetAsyncHTTP();

    // Set flag so connectToMqtt() is called from loop() (Arduino task),
    // not from the WiFi event callback (system event task on ESP32).
    pending_mqtt_connect = MyConfParam.v_MQTT_Active == 1;

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
      Inputsnsr13.mqtt_topic = MyConfParam.v_InputPin13_STATE_PUB_TOPIC;
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
      #ifdef InputPin01
      if (MyConfParam.v_IN1_INPUTMODE != INPUT_TEMPERATURE) {
        Inputsnsr01.initialize(InputPin01,process_Input,INPUT_NONE,PULLMODE_);
        Inputsnsr01.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
        Inputsnsr01.onInputClick_RelayServiceRoutine = buttonclick;
        Inputsnsr01.post_mqtt = true;
        Inputsnsr01.mqtt_topic = MyConfParam.v_InputPin01_STATE_PUB_TOPIC; //+ "_1"; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
        Inputsnsr01.fclickmode = static_cast <input_mode>(MyConfParam.v_IN1_INPUTMODE);  
      }
      #endif

      if (MyConfParam.v_IN2_INPUTMODE != INPUT_TEMPERATURE) {
      Inputsnsr02.initialize(InputPin02,process_Input,INPUT_NONE,PULLMODE_);
      Inputsnsr02.onInputChange_RelayServiceRoutine = onchangeSwitchInterruptSvc;
      Inputsnsr02.onInputClick_RelayServiceRoutine = buttonclick;
      Inputsnsr02.post_mqtt = true;
      Inputsnsr02.mqtt_topic = MyConfParam.v_InputPin02_STATE_PUB_TOPIC; // currently it posts to the same as InputPin12, reads its config from IN1, same as input12
      Inputsnsr02.fclickmode = static_cast <input_mode>(MyConfParam.v_IN2_INPUTMODE);
      }

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
        pulseCountCumulative = 0;
        wfsPulsesPerSec = 0;
        flowRate = 0.0;
        flowMilliLitres = 0;
        totalMilliLitres = 0;
        // Restore cumulative total from flash so a power cycle doesn't lose volume data.
        {
          File wfFile = SPIFFS.open("/wfs_total.dat", "r");
          if (wfFile && wfFile.size() == sizeof(totalMilliLitres)) {
            wfFile.read((uint8_t*)&totalMilliLitres, sizeof(totalMilliLitres));
          }
          if (wfFile) wfFile.close();
        }
        previousMillis_WFS = 0;
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

#ifdef OLED_1306
void oledWake()
{
    oledWakePending = true;
}

static void oledSetDisplayOn(bool on)
{
    display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    oledAwake = on;
}

static bool oledReadyForDraw()
{
    const unsigned long now = millis();
    const bool alwaysOn = (MyConfParam.v_OLEDAlwaysOn == 1) || firmwareUpdateInProgress;

    if (oledWakePending) {
        oledWakePending = false;
        oledLastWakeMs = now;
        if (!oledAwake) oledSetDisplayOn(true);
    }

    if (alwaysOn) {
        oledLastWakeMs = now;
        if (!oledAwake) oledSetDisplayOn(true);
        return true;
    }

    if (oledLastWakeMs == 0) oledLastWakeMs = now;

    if (oledAwake && (now - oledLastWakeMs >= OLED_AUTO_SLEEP_MS)) {
        display.clearDisplay();
        display.display();
        oledSetDisplayOn(false);
        return false;
    }

    return oledAwake;
}

static void drawCenteredOledText(const char *line, int16_t y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(line, 0, y, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, y);
    display.print(line);
}

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > debounceDelay) {
    lastInterruptTime = currentTime;
    if (!oledAwake) oledWasAsleepOnPress = true;
    oledWake();
    buttonPressed = true;
    buttonPressStartMs = currentTime;
  }
}

void IRAM_ATTR handleGpio0Interrupt() {
  unsigned long currentTime = millis();
  if (currentTime - gpio0LastInterruptTime > debounceDelay) {
    gpio0LastInterruptTime = currentTime;
    oledWake();
    gpio0ButtonPressed = true;
    gpio0PressStartMs = currentTime;
    gpio0LongPressHandled = false;
  }
}

#endif

// ── setup() helper functions ──────────────────────────────────────────────────

static bool s_brownoutBoot = false;  // set by setupHardware(), read by setupFilesystem()

static void setupHardware() {
  std::set_terminate([]() {
    Serial.println("FATAL: uncaught C++ exception (likely std::bad_alloc / OOM) -- restarting");
    Serial.flush();
    ESP.restart();
  });

  Serial.begin(115200);
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  {
    esp_core_dump_summary_t summary;
    if (esp_core_dump_get_summary(&summary) == ESP_OK) {
      Serial.printf("=== CRASH DUMP FOUND ===\n");
      Serial.printf("Crashed task: %s\n", summary.exc_task);
      Serial.printf("Exception cause: %lu\n", summary.ex_info.exc_cause);
      Serial.printf("EPC1: 0x%08lX\n", summary.ex_info.epcx[0]);
      Serial.printf("========================\n");

      const esp_partition_t *cdPart = esp_partition_find_first(
          ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
      if (cdPart) {
        uint32_t cdSize = 0;
        esp_partition_read(cdPart, 0, &cdSize, sizeof(cdSize));
        if (cdSize > 0 && cdSize != 0xFFFFFFFF && cdSize <= cdPart->size) {
          Serial.println("================= CORE DUMP START =================");
          const size_t CHUNK = 192;
          uint8_t buf[CHUNK];
          uint8_t b64[260];
          size_t offset = 0;
          while (offset < cdSize) {
            size_t toRead = min((uint32_t)CHUNK, cdSize - offset);
            esp_partition_read(cdPart, offset, buf, toRead);
            size_t outLen = 0;
            mbedtls_base64_encode(b64, sizeof(b64), &outLen, buf, toRead);
            b64[outLen] = '\0';
            Serial.println((char *)b64);
            offset += toRead;
          }
          Serial.println("================= CORE DUMP END ===================");
        }
      }
    } else {
      Serial.printf("No crash dump (clean boot)\n");
    }
  }

  #ifdef ESP32
    s_brownoutBoot = (esp_reset_reason() == ESP_RST_BROWNOUT);
    if (s_brownoutBoot) {
      forceAllRelayPinsOff();
      suppressRetainedRelayOnAfterBrownout.store(true, std::memory_order_release);
      Serial.println(F("[SYSTEM ] Brownout reset detected; forcing relays OFF for fail-safe boot"));
    }
  #endif

  #ifndef ESP32
    definePingsCallbacks();
    ESP.wdtDisable();
  #endif

  #if defined(OLED_1306) || defined(_ADS1X15_)
  Wire.begin();
  #endif

  #ifdef OLED_ThingPulse
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Hello world");
    display.display();
  #endif

  #ifdef INVERTERLINK
  SERIAL_INVERTER.begin(2400);
  #endif

  blinkInterval = blink_normal;

  #ifdef StepperMode
    shadeStepper.setMaxSpeed(1000);
    shadeStepper.setAcceleration(200);
    shadeStepper.setSpeed(50);
  #endif

  pinMode(led, OUTPUT);
  pinMode(ConfigInputPin, INPUT_PULLUP);

  #ifndef StepperMode
    #if defined(HWver02) || defined(HWver03)
      pinMode(InputPin12, INPUT_PULLUP);
      pinMode(InputPin14, INPUT_PULLUP);
    #endif
    #ifdef HWver03
    pinMode(InputPin02, INPUT_PULLUP);
    #endif
    #ifdef HWESP32
      #ifdef InputPin01
      if (MyConfParam.v_IN1_INPUTMODE != INPUT_TEMPERATURE) pinMode(InputPin01, INPUT_PULLUP);
      #endif
      if (MyConfParam.v_IN2_INPUTMODE != INPUT_TEMPERATURE) pinMode(InputPin02, INPUT_PULLUP);
      pinMode(InputPin03, INPUT_PULLUP);
      pinMode(InputPin04, INPUT_PULLUP);
      pinMode(InputPin05, INPUT_PULLUP);
      pinMode(InputPin06, INPUT_PULLUP);
    #endif
  #endif
}

// Returns false if LittleFS mount fails — setup() must return early.
static bool setupFilesystem() {
  #ifdef ESP32
  if (!SPIFFS.begin(true)) {
  #else
  if (!SPIFFS.begin()) {
  #endif
    Serial.println("❌ LittleFS mount failed!");
    return false;
  }
  Serial.println("✅ LittleFS mounted successfully.");

  if (s_brownoutBoot) persistAllRelaysOff();

  #ifdef ESP32
  listDir(SPIFFS, "/", 0);
  #endif

  #ifdef USEPREF
    ReadParams(MyConfParam, preferences);
  #endif

  while (loadConfig(MyConfParam) != SUCCESS) {
    delay(2000);
    ESP.restart();
  }
  serialLogLoad();

  #ifdef _ADS1X15_
  if (MyConfParam.v_ADS_HW_Active) {
    // Probe I2C bus before init — if chip is absent, disable and save config.
    Wire.beginTransmission(0x48);
    bool adsPresent = (Wire.endTransmission() == 0);
    if (!adsPresent) {
      Serial.println(F("[ADS    ] ADS1X15 not found on I2C (addr 0x48) — disabling and saving config"));
      MyConfParam.v_ADS_HW_Active = 0;
      saveConfig(MyConfParam);
    } else {
      #ifndef _ADS_ASYNC_
        #ifdef _ADS1015_
          ads.setDataRate(RATE_ADS1015_3300SPS);
        #else
          ads.setDataRate(RATE_ADS1115_860SPS);
        #endif
        ads.setGain(GAIN_TWOTHIRDS);
        ads.begin();
      #else
        ADS1.setGain(0);
        ADS1.setDataRate(7);
        ADS1.setMode(1);
        ADS1.begin();
      #endif
    }
  }
  #endif

  #ifdef OLED_1306
  if (MyConfParam.v_OLED_HW_Active) {
    // Probe I2C bus before init — if screen is absent, disable and save config.
    Wire.beginTransmission(SCREEN_ADDRESS);
    bool oledPresent = (Wire.endTransmission() == 0);
    if (!oledPresent) {
      Serial.println(F("[OLED   ] SSD1306 not found on I2C (addr 0x3C) — disabling and saving config"));
      MyConfParam.v_OLED_HW_Active = 0;
      saveConfig(MyConfParam);
    } else {
      SSD_1306();
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      drawCenteredOledText("Booting...", 22);
      drawCenteredOledText("Please Wait...", 34);
      display.display();
      #ifdef _emonlib_
        loadCTReadings(CT_1.saved_Wh, CT_1.saved_MTD_Wh, CT_1.saved_YTD_Wh,
                       CT_1._last_reset_day, CT_1._last_reset_month, CT_1._last_reset_year);
        CT_1.wh         = CT_1.saved_Wh;
        CT_1.MTD_Wh     = CT_1.saved_MTD_Wh;
        CT_1.YTD_Wh     = CT_1.saved_YTD_Wh;
        CT_1.PreviousWh = CT_1.saved_Wh;
        CT_1.CTSaveThreshold = 0;
      #endif
    }
  }
  #endif

  #ifdef ESP32
  #ifdef WaterFlowSensor
    loadconfigWFS("/WaterFlowSensorConfig.json");
  #endif
  #endif

  return true;
}

static void setupNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);

  if (digitalRead(ConfigInputPin) == LOW) {
    started_in_confMode = true;
    Serial.print(F("\n[WIFI   ] >>>>> Sarting WIFI in AP mode <<<<< \n"));
    WiFi.mode(WIFI_AP_STA);
    startsoftAP();
  }

  #ifndef ESP32
    #ifndef ESP_MESH
    if (!started_in_confMode) {
      Serial.println(F("[WIFI   ] Starting WIFI in Station mode"));
      WiFi.begin(MyConfParam.v_ssid.c_str(), MyConfParam.v_pass.c_str());
    }
    #else
      Serial.println(F("[INFO   ] calling setup_mesh()"));
      setup_mesh();
    #endif
  #else
    if (!started_in_confMode) {
      Serial.println(F("[WIFI   ] assigning WIFI Events"));
      WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
      WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
      #ifndef ESP_MESH
        WiFi.begin(MyConfParam.v_ssid.c_str(), MyConfParam.v_pass.c_str());
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
  mqttClient.setServer(MyConfParam.v_MQTT_BROKER.c_str(), MyConfParam.v_MQTT_B_PRT);
  mqttClient.setKeepAlive(10);

  mb.addCoil(LAMP1_COIL);
  mb.addCoil(LAMP2_COIL);

  #ifdef DEBUG_ENABLED
    Debug.begin(HOST_NAME);
    Debug.setResetCmdEnabled(true);
    Debug.showProfiler(true);
    Debug.showColors(true);
  #endif
}

static void setupInputsAndRelays() {
  #ifndef StepperMode
    setupInputs();
    #if defined(HWver02) || defined(HWver03)
      inputs.push_back(&Inputsnsr13);
      inputs.push_back(&Inputsnsr12);
      inputs.push_back(&Inputsnsr14);
    #endif
    #ifdef HWver03
    inputs.push_back(&Inputsnsr02);
    #endif
    #ifdef HWver03_4R
      inputs.push_back(&Inputsnsr02);
      inputs.push_back(&Inputsnsr14);
      inputs.push_back(&Inputsnsr13);
    #endif
    #ifdef HWESP32
      #ifdef InputPin01
      if (MyConfParam.v_IN1_INPUTMODE != INPUT_TEMPERATURE) inputs.push_back(&Inputsnsr01);
      #endif
      if (MyConfParam.v_IN2_INPUTMODE != INPUT_TEMPERATURE) inputs.push_back(&Inputsnsr02);
      inputs.push_back(&Inputsnsr03);
      inputs.push_back(&Inputsnsr04);
      inputs.push_back(&Inputsnsr05);
      #ifndef WaterFlowSensor
      inputs.push_back(&Inputsnsr06);
      #endif
    #endif
  #endif

  while (relay0.loadrelayparams(0) != true) { delay(2000); ESP.restart(); }
  relay0.attachLoopfunc(relayloopservicefunc);
  relay0.stop_ttl_timer();
  relay0.setRelayTTL_Timer_Interval(relay0.RelayConfParam->v_ttl * 1000);

  #ifdef ESP32_2RBoard
    while (relay1.loadrelayparams(1) != true) { delay(2000); ESP.restart(); }
    relay1.attachLoopfunc(relayloopservicefunc);
    relay1.stop_ttl_timer();
    relay1.setRelayTTL_Timer_Interval(relay1.RelayConfParam->v_ttl * 1000);
  #endif

  #ifndef ESP32
  ESP.wdtFeed();
  #endif

  #ifdef HWver03_4R
    while (relay1.loadrelayparams(1) != true) { delay(2000); ESP.restart(); }
    relay1.attachLoopfunc(relayloopservicefunc);
    relay1.stop_ttl_timer();
    relay1.setRelayTTL_Timer_Interval(relay1.RelayConfParam->v_ttl * 1000);

    while (relay2.loadrelayparams(2) != true) { delay(2000); ESP.restart(); }
    relay2.attachLoopfunc(relayloopservicefunc);
    relay2.stop_ttl_timer();
    relay2.setRelayTTL_Timer_Interval(relay2.RelayConfParam->v_ttl * 1000);

    while (relay3.loadrelayparams(3) != true) { delay(2000); ESP.restart(); }
    relay3.attachLoopfunc(relayloopservicefunc);
    relay3.stop_ttl_timer();
    relay3.setRelayTTL_Timer_Interval(relay3.RelayConfParam->v_ttl * 1000);
  #endif

  relays.push_back(&relay0);
  #ifdef ESP32_2RBoard
    relays.push_back(&relay1);
  #endif
  #ifdef HWver03_4R
    relays.push_back(&relay1);
    relays.push_back(&relay2);
    relays.push_back(&relay3);
  #endif

  #ifndef ESP32
  ESP.wdtFeed();
  #endif

  while (loadIRMapConfig(myIRMap) != SUCCESS) { ESP.restart(); }

  #ifndef ESP32
  ESP.wdtFeed();
  #endif

  Serial.println(F("[INFO TP] opening /tempconfig.json file"));
  if (relay0.RelayConfParam->v_TemperatureValue != "0") {
    loadTempConfig("/tempconfig.json", PTempConfig);
  } else {
    Serial.println(F("[INFO TP] v_TemperatureValue = 0; disabling temperature processing"));
  }

  #ifdef _ACS712_
  ACS_Calibrate_Start(relay0, sensor);
  #endif

  #ifdef StepperMode
  #define stepperenablepin InputPin02
    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    #ifdef HWver03
    pinMode(stepperenablepin, OUTPUT);
    #endif
    digitalWrite(stepPin, LOW);
  #endif

  #ifdef SR04_SERIAL
  delay(2000);
  beginSerialSR04();
  #endif
}

static void setupScheduler() {
  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0 && !started_in_confMode)
    tskPinger.enable();

  #ifdef _emonlib_
  if (!started_in_confMode) {
    xTimerStop(CT_1.ThresholdCossHighTimer, 0);
    xTimerStop(CT_1.ThresholdCossLowTimer, 0);
    CT_1.emon1.current(CurrentPin, MyConfParam.v_CurrentTransformer_max_current);
    CT_1.emon1.voltage(VoltagePin, MyConfParam.v_calibration, MyConfParam.v_PhaseCal);
    applyPowerTaskIntervals();
    xTaskCreatePinnedToCore(ctMeasurementTask, "CTMeasure", 4096, NULL, 1,
                            &ctMeasurementTask_handle, 0);
    tskEmonReader.enable();
    tskEmonPublisher.enableDelayed(2500);
  }
  #endif

  #ifdef _pressureSensor_
  applyPowerTaskIntervals();
  tskEmonReader.enable();
  #endif

  #if defined(_ESP_ALEXA_) || defined(_ALEXA_)
  if (!started_in_confMode) tskespAlexaStateUpdate.enable();
  #endif

  #ifdef OLED_1306
  if (MyConfParam.v_OLED_HW_Active) {
    applyOledTaskInterval();
    tskOLEDUpdate.enable();
  }
  #endif

  #ifdef _ADS1X15_
  if (MyConfParam.v_ADS_HW_Active) {
    loadADS11x5Config("/ADS11x5Config.json", PADS11x5Config);
    tskADSRead.enable();
  }
  #endif

  #ifdef _HST_
  loadHSTConfig("/HSTConfig.json", PAHSTConfig);
  #endif

  #ifdef _REMOTE_SENSORS_
  loadRemoteSensors();
  #endif

  #ifdef _NEWMETHOD_
  tskADSReadPF.enable();
  #endif

  if (relay0.RelayConfParam->v_TemperatureValue != "0") {
    #ifdef HWESP32
      #ifdef TempSensorPin
      if (MyConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE) { tempsensor.tempbegin(); tempSensorPin01Begun = true; }
      #endif
      if (MyConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE) { TempSensorSecond.tempbegin(); tempSensorPin02Begun = true; }
    #else
      #if not defined _emonlib_ && not defined _pressureSensor_
      tempsensor.tempbegin();
      #endif
      TempSensorSecond.tempbegin();
    #endif
  }

  #ifdef OLED_1306
  if (MyConfParam.v_OLED_HW_Active) {
    #ifdef ESP32
      pinMode(ConfigInputPin, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(ConfigInputPin), handleButtonInterrupt, FALLING);
      pinMode(GPIO0_BUTTON_PIN, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(GPIO0_BUTTON_PIN), handleGpio0Interrupt, FALLING);
    #endif
  }
  #endif

  #ifdef _ALEXA_
  fauxmo.begin();
  #endif

  #ifdef AppleHKHomeSpan
  homeSpan.begin(Category::Other, "HomeSpan", "1.0.0");
  HomespanInitiated = true;
  #endif
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  setupHardware();
  if (!setupFilesystem()) return;
  setupNetwork();
  setupInputsAndRelays();
  setupScheduler();
  Serial.println(F("[SYSTEM ] Setup completed"));
} // setup


#ifdef OLED_1306
void tskfn_OLEDUpdate() {
    if (!oledReadyForDraw()) return;

    if (firmwareUpdateInProgress) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        drawCenteredOledText("Firmware Update", 22);
        drawCenteredOledText("in Progress", 34);
        display.display();
        return;
    }

    // Temporary config AP mode active — shown until it auto-closes and the board restarts
    if (wifimode == WIFI_AP_MODE) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        drawCenteredOledText("** AP Mode **", 0);
        drawCenteredOledText("Connect to:", 10);
        // SSID can exceed the ~21 chars that fit on one line at text size 1
        // (128px / 6px per char) — split it across two lines.
        String apName = APssid + String(MyConfParam.v_PhyLoc);
        const size_t maxCharsPerLine = 21;
        String apNameLine1 = apName;
        String apNameLine2 = "";
        if (apName.length() > maxCharsPerLine) {
          apNameLine1 = apName.substring(0, maxCharsPerLine);
          apNameLine2 = apName.substring(maxCharsPerLine);
        }
        drawCenteredOledText(apNameLine1.c_str(), 20);
        drawCenteredOledText(apNameLine2.c_str(), 30);
        String apIp = WiFi.softAPIP().toString();
        drawCenteredOledText(apIp.c_str(), 42);
        long remainingSec = APModetimer - APModetimer_run_value;
        if (remainingSec < 0) remainingSec = 0;
        char remBuf[24];
        snprintf(remBuf, sizeof(remBuf), "Closes in %ld:%02ld", remainingSec / 60, remainingSec % 60);
        drawCenteredOledText(remBuf, 54);
        display.display();
        return;
    }

    // GPIO0 menu — page A: hold 30s to enter temporary config AP mode
    if (CURRENT_GPIO0_Action == GPIO0_ACTION_AP_MODE) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);

        bool holding = (gpio0PressStartMs != 0 && !gpio0LongPressHandled && digitalRead(GPIO0_BUTTON_PIN) == LOW);
        if (holding) {
            unsigned long heldMs = millis() - gpio0PressStartMs;
            unsigned long heldSec = heldMs / 1000UL;
            drawCenteredOledText("Holding...", 10);
            String prog = String(heldSec) + " / 30 sec";
            drawCenteredOledText(prog.c_str(), 22);
            // Progress bar (96 px wide, centred)
            int barX = 16, barY = 34, barW = 96, barH = 6;
            display.drawRect(barX, barY, barW, barH, WHITE);
            int fill = (int)(heldMs * barW / GPIO0_LONG_PRESS_MS);
            if (fill > 0) display.fillRect(barX, barY, fill, barH, WHITE);
        } else {
            drawCenteredOledText("Hold 30s for", 8);
            drawCenteredOledText("AP Config Mode", 18);
            drawCenteredOledText("Click to skip", 38);
        }
        display.display();
        return;
    }

    // GPIO0 menu — page B: hold 30s to create any missing config JSON files
    if (CURRENT_GPIO0_Action == GPIO0_ACTION_CONFIG_RESET) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);

        if (configResetConfirmUntilMs != 0 && millis() < configResetConfirmUntilMs) {
            drawCenteredOledText("Initializing", 16);
            drawCenteredOledText("Config Files...", 28);
        } else {
            if (configResetConfirmUntilMs != 0) configResetConfirmUntilMs = 0;
            bool holding = (gpio0PressStartMs != 0 && !gpio0LongPressHandled && digitalRead(GPIO0_BUTTON_PIN) == LOW);
            if (holding) {
                unsigned long heldMs = millis() - gpio0PressStartMs;
                unsigned long heldSec = heldMs / 1000UL;
                drawCenteredOledText("Holding...", 10);
                String prog = String(heldSec) + " / 30 sec";
                drawCenteredOledText(prog.c_str(), 22);
                // Progress bar (96 px wide, centred)
                int barX = 16, barY = 34, barW = 96, barH = 6;
                display.drawRect(barX, barY, barW, barH, WHITE);
                int fill = (int)(heldMs * barW / GPIO0_LONG_PRESS_MS);
                if (fill > 0) display.fillRect(barX, barY, fill, barH, WHITE);
            } else {
                drawCenteredOledText("Hold 30s to", 8);
                drawCenteredOledText("Init/Reset Config", 18);
                drawCenteredOledText("Click to skip", 38);
            }
        }
        display.display();
        return;
    }

    // Page 5: power reset page — only compiled and reachable when _emonlib_ is active
    #ifdef _emonlib_
    if (CURRENT_Display_Action == ACTION_DISPALY_5) {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);

      if (powerResetConfirmUntilMs != 0 && millis() < powerResetConfirmUntilMs) {
        // Show "done" after successful reset
        drawCenteredOledText("Done!", 16);
        drawCenteredOledText("Readings cleared", 28);
      } else {
        if (powerResetConfirmUntilMs != 0) powerResetConfirmUntilMs = 0;
        bool holding = (buttonPressStartMs != 0 && digitalRead(ConfigInputPin) == LOW);
        if (holding) {
          unsigned long heldSec = (millis() - buttonPressStartMs) / 1000UL;
          drawCenteredOledText("Holding...", 10);
          String prog = String(heldSec) + " / 10 sec";
          drawCenteredOledText(prog.c_str(), 22);
          // Progress bar (96 px wide, centred)
          int barX = 16, barY = 34, barW = 96, barH = 6;
          display.drawRect(barX, barY, barW, barH, WHITE);
          int fill = (int)(heldSec * barW / 10);
          if (fill > 0) display.fillRect(barX, barY, fill, barH, WHITE);
        } else {
          drawCenteredOledText("Hold button 10s", 8);
          drawCenteredOledText("to reset power", 18);
          drawCenteredOledText("readings", 28);
        }
      }
      // fall through to status bar + display.display() below
    }
    #endif // _emonlib_

    // Page 7: board reboot page — always compiled and reachable.
    if (CURRENT_Display_Action == ACTION_DISPALY_7) {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);

      if (rebootConfirmUntilMs != 0 && millis() < rebootConfirmUntilMs) {
        drawCenteredOledText("Rebooting...", 22);
      } else {
        if (rebootConfirmUntilMs != 0) rebootConfirmUntilMs = 0;
        bool holding = (buttonPressStartMs != 0 && digitalRead(ConfigInputPin) == LOW);
        if (holding) {
          unsigned long heldSec = (millis() - buttonPressStartMs) / 1000UL;
          drawCenteredOledText("Holding...", 10);
          String prog = String(heldSec) + " / 30 sec";
          drawCenteredOledText(prog.c_str(), 22);
          // Progress bar (96 px wide, centred)
          int barX = 16, barY = 34, barW = 96, barH = 6;
          display.drawRect(barX, barY, barW, barH, WHITE);
          int fill = (int)(heldSec * barW / 30);
          if (fill > 0) display.fillRect(barX, barY, fill, barH, WHITE);
        } else {
          drawCenteredOledText("Hold button 30s", 8);
          drawCenteredOledText("to reboot", 18);
          drawCenteredOledText("the board", 28);
        }
      }
      // fall through to status bar + display.display() below
    }

    #ifdef WaterFlowSensor
    if (CURRENT_Display_Action == ACTION_DISPALY_6) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        drawCenteredOledText("Flow Sensor", 0);
        display.setCursor(0, 14);
        display.print(F("Flow: "));
        display.print(flowRate, 1);
        display.print(F(" L/min"));
        display.setCursor(0, 26);
        display.print(F("PPS:  "));
        display.print(wfsPulsesPerSec);
        display.setCursor(0, 38);
        display.print(F("Total:"));
        display.print((double)totalMilliLitres / 1000.0, 2);
        display.print(F(" L"));
    }
    #endif // WaterFlowSensor

    // Pages 1 & 2: clear + draw CT content here (single owner of clearDisplay).
    // Pages 3 & 4: content task already wrote to the buffer; just overlay status bar.
    #if defined(_emonlib_) && !defined(_pressureSensor_)
    if ((CURRENT_Display_Action == ACTION_DISPALY_1) || (CURRENT_Display_Action == ACTION_DISPALY_2)) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.dim(false);
        CT_1.DisplayPower(display, mqttClient, MyConfParam.v_Screen_orientation);
    }
    #endif

    // Status line — overlaid on top of whatever content is already in the buffer
    display.setCursor(1, 54);
    display.setTextColor(BLACK, WHITE);
    display.print(WiFi.localIP().toString());
    display.setTextColor(WHITE);
    display.setCursor(100, 54);
    display.print((WiFi.status() == WL_CONNECTED) ? F("*") : F("x"));
    display.setCursor(110, 54);
    display.print(mqttClient.connected() ? F("M") : F("m"));
    display.setCursor(120, 54);
    if (!wireGuardIsEnabled())       display.print(F("_"));
    else if (wireGuardIsNetworkUp()) display.print(F("V"));
    else                             display.print(F("v"));

    display.display();  // single SPI push per tick — no flicker
}
#endif   




void schedulerStopForFirmwareUpdate() {
  Scheduler_ts.disableAll();
}

// Re-creates any missing per-submodule JSON config files using this board's
// CHIPID-based defaults — these are the same loaders setup() calls, and each
// one writes its defaults (via CID()) only when its file is absent or corrupt.
// A restart is required afterwards so the freshly written files are loaded
// through the normal boot sequence.
void initializeMissingConfigFiles() {
  Serial.println(F("[SYSTEM ] Initializing missing configuration files..."));

  loadConfig(MyConfParam);
  loadIRMapConfig(myIRMap);

  for (size_t i = 0; i < relays.size(); i++) {
    Relay* rly = static_cast<Relay*>(relays[i]);
    if (rly) rly->loadrelayparams((uint8_t)i);
  }

  loadTempConfig("/tempconfig.json", PTempConfig);

  #ifdef _ADS1X15_
  if (MyConfParam.v_ADS_HW_Active)
    loadADS11x5Config("/ADS11x5Config.json", PADS11x5Config);
  #endif

  #ifdef _HST_
  loadHSTConfig("/HSTConfig.json", PAHSTConfig);
  #endif

  #ifdef WaterFlowSensor
  loadconfigWFS("/WaterFlowSensorConfig.json");
  #endif

  Serial.println(F("[SYSTEM ] Configuration initialization complete."));
}

// Fires once per day at MyConfParam.v_DailyRestartTime ("HH:MM") if enabled,
// but only when the heap is fragmented enough to matter — i.e. the largest
// free block is below 90 KB. Otherwise the day is skipped and re-evaluated
// the next time the clock hits that minute.
void checkDailyRestart() {
  if (!MyConfParam.v_DailyRestartEnabled) return;
  if (timeStatus() == timeNotSet) return;

  int targetH = -1, targetM = -1;
  if (sscanf(MyConfParam.v_DailyRestartTime.c_str(), "%d:%d", &targetH, &targetM) != 2) return;

  static int lastCheckedDay = -1;
  int curDay = day();

  if (hour() == targetH && minute() == targetM) {
    if (lastCheckedDay != curDay) {
      lastCheckedDay = curDay;
      size_t maxBlk = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      if (maxBlk < (90 * 1024)) {
        Serial.printf("[SYSTEM ] Daily restart at %02d:%02d — max alloc heap %u B < 90 KB, restarting.\n",
                       targetH, targetM, (unsigned)maxBlk);
        restartRequired = true;
      } else {
        Serial.printf("[SYSTEM ] Daily restart at %02d:%02d skipped — max alloc heap %u B is healthy.\n",
                       targetH, targetM, (unsigned)maxBlk);
      }
    }
  }
}

// ── loop() service functions ──────────────────────────────────────────────────

#ifdef ESP32
static void servicePendingWifiConnected() {
  if (!pending_wifi_connected) return;
  pending_wifi_connected = false;
  thingsTODO_on_WIFI_Connected();
  #if defined (_emonlib_)  || defined (_pressureSensor_)
  tskEmonPublisher_setEnableSts(true);
  #endif
}
#endif

#ifdef OLED_1306
static void serviceOledOtaDraw() {
  if (!MyConfParam.v_OLED_HW_Active || !firmwareUpdateInProgress) return;
  static unsigned long lastDrawMs = 0;
  if (millis() - lastDrawMs >= 1000) {
    lastDrawMs = millis();
    tskfn_OLEDUpdate();
  }
}

static void serviceOledNetworkWatcher() {
  if (!MyConfParam.v_OLED_HW_Active) return;
  static bool primed = false;
  static bool lastWifi = false, lastMqtt = false, lastVpnEnabled = false, lastVpnUp = false;
  const bool wifi = (WiFi.status() == WL_CONNECTED);
  const bool mqtt = mqttClient.connected();
  const bool vpnE = wireGuardIsEnabled();
  const bool vpnU = wireGuardIsNetworkUp();
  if (!primed) {
    primed = true;
    lastWifi = wifi; lastMqtt = mqtt; lastVpnEnabled = vpnE; lastVpnUp = vpnU;
    return;
  }
  if (lastWifi != wifi || lastMqtt != mqtt || lastVpnEnabled != vpnE || lastVpnUp != vpnU) {
    oledWake();
    lastWifi = wifi; lastMqtt = mqtt; lastVpnEnabled = vpnE; lastVpnUp = vpnU;
  }
}

static void serviceOledCfgButton() {
  if (!MyConfParam.v_OLED_HW_Active) return;
  const bool buttonHeld = (digitalRead(ConfigInputPin) == LOW);
  static bool prevButtonHeld = false;

  if (buttonPressStartMs != 0 && buttonHeld && !buttonLongPressHandled &&
      CURRENT_Display_Action == ACTION_DISPALY_5 &&
      millis() - buttonPressStartMs >= 10000UL) {
    buttonLongPressHandled = true;
    buttonPressed = false;
    #ifdef _emonlib_
    CT_1.wh = 0; CT_1.MTD_Wh = 0; CT_1.YTD_Wh = 0; CT_1.CTSaveThreshold = 0;
    saveCTReadings(0, 0, 0, CT_1._last_reset_day, CT_1._last_reset_month, CT_1._last_reset_year);
    Serial.println(F("[CT     ] Long press on page 5: power accumulators reset to zero."));
    #endif
    powerResetConfirmUntilMs = millis() + 3000UL;
  }
  if (buttonPressStartMs != 0 && buttonHeld && !buttonLongPressHandled &&
      CURRENT_Display_Action == ACTION_DISPALY_7 &&
      millis() - buttonPressStartMs >= 30000UL) {
    buttonLongPressHandled = true;
    buttonPressed = false;
    Serial.println(F("[SYSTEM ] Long press on page 7: rebooting the board."));
    rebootConfirmUntilMs = millis() + 3000UL;
    tskfn_OLEDUpdate();
    restartRequired = true;
  }
  if (prevButtonHeld && !buttonHeld) {
    if (buttonPressed && !buttonLongPressHandled) {
      if (oledWasAsleepOnPress) CURRENT_Display_Action = ACTION_DISPALY_1;
      else                      myClickFunction();
    }
    oledWasAsleepOnPress = false;
    buttonPressed = false;
    buttonPressStartMs = 0;
    buttonLongPressHandled = false;
  }
  prevButtonHeld = buttonHeld;
}

static void serviceGpio0Button() {
  const bool gpio0Held = (digitalRead(GPIO0_BUTTON_PIN) == LOW);
  static bool prevGpio0Held = false;

  if (gpio0PressStartMs != 0 && gpio0Held && !gpio0LongPressHandled &&
      millis() - gpio0PressStartMs >= GPIO0_LONG_PRESS_MS) {
    if (CURRENT_GPIO0_Action == GPIO0_ACTION_AP_MODE) {
      gpio0LongPressHandled = true;
      Serial.println(F("[GPIO0  ] Long press: activating temporary config AP mode."));
      APModetimer = AP_CONFIG_MODE_MAX_SECONDS;
      WiFi.mode(WIFI_AP_STA);
      startsoftAP();
      gpio0ApModeActive = true;
      tskfn_OLEDUpdate();
    } else if (CURRENT_GPIO0_Action == GPIO0_ACTION_CONFIG_RESET) {
      gpio0LongPressHandled = true;
      Serial.println(F("[GPIO0  ] Long press: initializing missing configuration files."));
      initializeMissingConfigFiles();
      configResetConfirmUntilMs = millis() + 3000UL;
      tskfn_OLEDUpdate();
      restartRequired = true;
    }
  }
  if (prevGpio0Held && !gpio0Held) {
    if (gpio0ButtonPressed && !gpio0LongPressHandled) {
      if (gpio0ApModeActive) {
        Serial.println(F("[GPIO0  ] Press: cancelling temporary config AP mode, restarting."));
        restartRequired = true;
      } else {
        switch (CURRENT_GPIO0_Action) {
          case GPIO0_ACTION_NONE:         CURRENT_GPIO0_Action = GPIO0_ACTION_AP_MODE;      break;
          case GPIO0_ACTION_AP_MODE:      CURRENT_GPIO0_Action = GPIO0_ACTION_CONFIG_RESET; break;
          case GPIO0_ACTION_CONFIG_RESET: CURRENT_GPIO0_Action = GPIO0_ACTION_NONE;         break;
        }
        Serial.printf("[GPIO0  ] Menu page -> %d\n", (int)CURRENT_GPIO0_Action);
        tskfn_OLEDUpdate();
      }
    }
    gpio0ButtonPressed = false;
    gpio0PressStartMs = 0;
    gpio0LongPressHandled = false;
  }
  prevGpio0Held = gpio0Held;
}
#endif  // OLED_1306

static void serviceMqttPending() {
  if (!mqttIsEnabled()) {
    pending_mqtt_subscribe = false;
    pending_input_status_publish = false;
    pending_relay_state_idx = -1;
  }
  if (pending_mqtt_connect) {
    pending_mqtt_connect = false;
    if (!firmwareUpdateInProgress && mqttIsEnabled()) connectToMqtt();
    else applyMqttActiveState();
  }
  if (pending_mqtt_subscribe && mqttIsEnabled() && mqttClient.connected()) {
    pending_mqtt_subscribe = false;
    mqttSubscribeRelays();
    mqttSubscribedAt    = millis();
    relay_state_next_ms = mqttSubscribedAt + 5000;
    haDiscoveryBegin(mqttSubscribedAt + 8000);
  }
  if (pending_input_status_publish &&
      mqttIsEnabled() && mqttClient.connected() &&
      mqttSubscribedAt != 0 && millis() - mqttSubscribedAt >= 5000) {
    pending_input_status_publish = false;
    postInitialInputStatus();
  }
  if (pending_relay_state_idx >= 0 &&
      mqttIsEnabled() && mqttClient.connected() &&
      millis() >= relay_state_next_ms) {
    if ((size_t)pending_relay_state_idx < relays.size()) {
      Relay* rly = static_cast<Relay*>(relays[pending_relay_state_idx]);
      if (rly) {
        const bool on = rly->readrelay() == HIGH;
        mqttPublish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, on ? ON : OFF);
        mqttPublish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(),      QOS2, RETAINED, on ? ON : OFF);
        if (rly->RelayConfParam->v_ttl > 0) {
          char buf[12];
          snprintf(buf, sizeof(buf), "%d", rly->RelayConfParam->v_ttl);
          mqttPublish(rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, buf);
        }
      }
      pending_relay_state_idx++;
      relay_state_next_ms = millis() + 200;
    } else {
      pending_relay_state_idx = -1;
      relay_state_next_ms = 0;
    }
  }
  haDiscoveryService();
  if (mqttSubscribedAt != 0 && !pending_input_status_publish && pending_relay_state_idx < 0)
    mqttSubscribedAt = 0;
}

static void serviceRestartRequired() {
  if (!restartRequired) return;
  Serial.println(F("\n[SYSTEM ] Restarting\n\r"));
  restartRequired = false;
  delay(4000);
  if (MyConfParam.v_PRST == 1) {
    Serial.println(F("\n[SYSTEM ] Restarting with Toggle\n\r"));
    Relay* rtmp = getrelaybynumber(0);
    if (rtmp) {
      digitalWrite(rtmp->getRelayPin(), LOW);
      rtmp->savePersistedrelay();
    }
  }
  delay(1000);
  ESP.restart();
}

#ifndef StepperMode
static void serviceInputWatchers() {
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
    #ifdef InputPin01
    if (MyConfParam.v_IN1_INPUTMODE != INPUT_TEMPERATURE) Inputsnsr01.watch();
    #endif
    if (MyConfParam.v_IN2_INPUTMODE != INPUT_TEMPERATURE) Inputsnsr02.watch();
    Inputsnsr03.watch();
    Inputsnsr04.watch();
    Inputsnsr05.watch();
    #ifndef WaterFlowSensor
    Inputsnsr06.watch();
    #endif
  #endif
}

#ifdef ESP32
#ifdef WaterFlowSensor
static void serviceWaterFlowSensor() {
  currentMillis_WFS = millis();
  if (currentMillis_WFS - previousMillis_WFS <= interval) return;
  unsigned long elapsed_ms = currentMillis_WFS - previousMillis_WFS;
  portENTER_CRITICAL(&pulseCountMux);
  pulse1Sec = pulseCount;
  pulseCount = 0;
  portEXIT_CRITICAL(&pulseCountMux);
  flowRate = ((1000.0f / elapsed_ms) * pulse1Sec) / calibrationFactor;
  wfsPulsesPerSec = (uint32_t)((pulse1Sec * 1000UL) / elapsed_ms);
  previousMillis_WFS = currentMillis_WFS;
  flowMilliLitres = (double)pulse1Sec * 1000.0 / (calibrationFactor * 60.0);
  totalMilliLitres += (uint64_t)flowMilliLitres;
  if (millis() - wfsPersistMs >= 300000UL) {
    File wfFile = SPIFFS.open("/wfs_total.dat", "w");
    if (wfFile) {
      wfFile.write((const uint8_t*)&totalMilliLitres, sizeof(totalMilliLitres));
      wfFile.close();
    }
    wfsPersistMs = millis();
  }
  double totalLiters = (double)totalMilliLitres / 1000.0;
  SLOG(SLOG_WFS, "[WFS    ] Flow: %dL/min  Total: %lluml / %.2fL\n",
                int(flowRate), (unsigned long long)totalMilliLitres, totalLiters);
  char res[12], res2[12];
  dtostrf(flowRate, 6, 1, res);
  mqttPublish(WaterFlowSensor_Topic.c_str(), 0, true, res);
  dtostrf(totalLiters, 6, 1, res2);
  {
    char wfsTotalTopic[72];
    int _sl = WaterFlowSensor_Topic.lastIndexOf('/');
    if (_sl >= 0)
      snprintf(wfsTotalTopic, sizeof(wfsTotalTopic), "%.*sTotalLiters", _sl + 1, WaterFlowSensor_Topic.c_str());
    else
      strlcpy(wfsTotalTopic, "TotalLiters", sizeof(wfsTotalTopic));
    mqttPublish(wfsTotalTopic, 0, true, res2);
  }
}
#endif  // WaterFlowSensor
#endif  // ESP32

static void serviceCalendar() {
  if (timeStatus() != timeNotSet && now() != prevDisplay) {
    prevDisplay = now();
    chronosevaluatetimers(MyCalendar);
    checkDailyRestart();
  }
  if ((timeStatus() == timeSet) && CalendarNotInitiated) {
    chronosInit();
    CalendarNotInitiated = false;
  } else if (CalendarNotInitiated) {
    static uint32_t lastNtpWarn = 0;
    if (millis() - lastNtpWarn > 60000) {
      lastNtpWarn = millis();
      Serial.println(F("[TIMERS ] Waiting for NTP sync — calendar not loaded yet"));
    }
  }
}

static void serviceHomeKitDeferredInit() {
  if (millis() - lastMillis_2 <= 2000) return;
  lastMillis_2 = millis();
  #ifdef AppleHK
    #ifndef ESP32
    if ((timeStatus() == timeSet) && homekitNotInitialised) {
      homekitNotInitialised = false;
      String s = "Bridge_" + MyConfParam.v_PhyLoc;
      s.toCharArray(HAName_Bridge, HK_name_len);
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
    if ((timeStatus() == timeSet) && homekitNotInitialised) {
      Serial.println("[HOMEKIT] starting homekit");
      setupHAP();
      homekitNotInitialised = false;
    }
    #endif
  #endif
}

static void servicePerSecond() {
  if (millis() - lastMillis <= 1000) return;
  lastMillis = millis();
  secondson++;

  #ifdef _AUTOMATION_RULES_
  evaluateAutomationRules();
  #endif

  #ifdef SR04_SERIAL
  if (secondson > 5) {
    myPort.print(".");
    char res[8];
    dtostrf(readSerialUltrasoundSensor(), 6, 1, res);
    Serial.print(PSTR("Result SR04 Serial :"));
    Serial.println(res);
    if (mqttClient.connected())
      mqttPublish(MyConfParam.v_Sonar_distance.c_str(), 0, RETAINED, res);
  }
  #endif

  #ifdef SR04
  if (MyConfParam.v_Sonar_distance != "0") {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT_PULLUP);
    cm = sonar.convert_cm(sonar.ping_median(10, 300));
    Serial.print("[SONAR  ] Ping: "); Serial.print(cm); Serial.println("cm");
    char res[8];
    dtostrf(cm, 6, 1, res);
    if (mqttClient.connected()) mqttPublish(MyConfParam.v_Sonar_distance.c_str(), 0, RETAINED, res);
  }
  #endif

  if (wifimode == WIFI_AP_MODE) {
    APModetimer_run_value++;
    Serial.print(F("\n[WIFI   ] ApMode will restart after (seconds): "));
    Serial.print(APModetimer - APModetimer_run_value);
    if (APModetimer_run_value >= APModetimer) {
      APModetimer_run_value = 0;
      ESP.restart();
    }
  }
}

#if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
#ifndef SR04
static void serviceTemperatureSensors() {
  if (relay0.RelayConfParam->v_TemperatureValue == "0") return;
  #if defined(TempSensorPin) || defined(SecondTempSensorPin)
  if (millis() - lastMillis5000 > 5000) {
    lastMillis5000 = millis();
    #ifdef HWESP32
      #ifdef TempSensorPin
      if (MyConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE) {
        if (!tempSensorPin01Begun) { tempsensor.tempbegin(); tempSensorPin01Begun = true; }
        pinMode(TempSensorPin, INPUT_PULLUP);
        tempsensor.requestTemp();
      } else {
        tempSensorPin01Begun = false;
      }
      #endif
      if (MyConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE) {
        if (!tempSensorPin02Begun) { TempSensorSecond.tempbegin(); tempSensorPin02Begun = true; }
        pinMode(SecondTempSensorPin, INPUT_PULLUP);
        TempSensorSecond.requestTemp();
      } else {
        tempSensorPin02Begun = false;
      }
      tempConversionPending = (MyConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE);
      #ifdef TempSensorPin
      tempConversionPending = tempConversionPending || (MyConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE);
      #endif
    #else
      pinMode(TempSensorPin, INPUT_PULLUP);
      pinMode(SecondTempSensorPin, INPUT_PULLUP);
      tempsensor.requestTemp();
      TempSensorSecond.requestTemp();
      tempConversionPending = true;
    #endif
  }
  if (tempConversionPending && millis() - lastMillis5000 >= 800) {
    tempConversionPending = false;
    #ifdef HWESP32
      #ifdef TempSensorPin
      bool tempPin01Active = (MyConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE);
      #else
      bool tempPin01Active = false;
      #endif
      bool tempPin02Active = (MyConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE);
      if (tempPin01Active) { tempsensor.getCurrentTemp(0); MCelcius = tempsensor.Celcius; }
      if (tempPin02Active) { TempSensorSecond.getCurrentTemp(0); MCelcius2 = TempSensorSecond.Celcius; }
    #else
      tempsensor.getCurrentTemp(0);
      TempSensorSecond.getCurrentTemp(0);
      MCelcius  = tempsensor.Celcius;
      MCelcius2 = TempSensorSecond.Celcius;
      bool tempPin01Active = true;
      bool tempPin02Active = true;
    #endif
    float TSolarTank = roundf(MCelcius);
    if (tempPin01Active) {
      SLOG_IF(SLOG_INFO) { Serial.print(F("[INFO   ] Temperature Sensor #1: ")); Serial.println(MCelcius); }
      #ifdef DEBUG_ENABLED
        debugV("[INFO   ] TempSensor1 %.2f C ", TSolarTank);
      #endif
      char res[12];
      dtostrf(MCelcius, 6, 1, res);
      mqttPublish(relay0.RelayConfParam->v_TemperatureValue.c_str(), 0, RETAINED, res);
    }
    #ifdef AppleHK
      #ifndef ESP32
      if (tempPin01Active) {
        if (MCelcius < 0) MCelcius = 99;
        homekit_characteristic_notify(&cha_temperature, HOMEKIT_FLOAT(MCelcius));
      }
      #endif
    #endif
    float TSolarPanel = roundf(MCelcius2);
    if (tempPin02Active) {
      SLOG_IF(SLOG_INFO) { Serial.print(F("[INFO   ] Temperature Sensor #2: ")); Serial.println(MCelcius2); }
      #ifdef DEBUG_ENABLED
        debugV("[INFO   ] TempSensor2 %.2f C ", TSolarPanel);
      #endif
      char res[12];
      dtostrf(MCelcius2, 6, 1, res);
      char tempTopic2[80];
      snprintf(tempTopic2, sizeof(tempTopic2), "%s_2", relay0.RelayConfParam->v_TemperatureValue.c_str());
      mqttPublish(tempTopic2, 0, RETAINED, res);
    }
    #ifdef SolarHeaterControllerMode
    if (tempPin01Active && tempPin02Active && TSolarPanel > -100 && TSolarTank > -100)
      TempertatureSensorEvent(0, TSolarPanel, TSolarTank);
    #endif
    #ifdef OLED_1306
    if (CURRENT_Display_Action == ACTION_DISPALY_4) {
      display.clearDisplay();
      int8_t StartRow = 22;
      int8_t rect_width = 60;
      display.drawRect(0, StartRow, rect_width, 30, WHITE);
      display.drawRect(rect_width + 2, StartRow, rect_width, 30, WHITE);
      display.setCursor(8, StartRow + 2);
      display.println(F("TEMP 1"));
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, StartRow + 16);
      display.setTextSize(1);
      display.print(String(MCelcius));
      display.setTextSize(1);
      display.setCursor(rect_width + 10, StartRow + 2);
      display.println(F("TEMP 2"));
      display.setCursor(rect_width + 8, StartRow + 16);
      display.setTextSize(1);
      display.print(String(MCelcius2));
      display.setTextSize(1);
      display.setTextColor(WHITE);
    }
    #endif
  }
  #endif  // TempSensorPin || SecondTempSensorPin
}
#endif  // !SR04
#endif  // HWver02 || HWver03 || HWESP32
#endif  // !StepperMode

#ifdef StepperMode
static void serviceStepperMode() {
  if (!steperrun) return;
  shadeStepper.setSpeed(400);
  digitalWrite(stepperenablepin, false);
  while (shadeStepper.currentPosition() != 800) {
    shadeStepper.runSpeed();
    ESP.wdtFeed();
  }
  steperrun = false;
  digitalWrite(stepperenablepin, true);
  shadeStepper.stop();
}
#endif

// ─────────────────────────────────────────────────────────────────────────────

void loop() {

  #ifdef ESP32
  servicePendingWifiConnected();
  #endif

  wireGuardLoop();

  #ifdef OLED_1306
  serviceOledOtaDraw();
  #endif

  if (!firmwareUpdateInProgress) {
    #ifdef OLED_1306
    serviceOledNetworkWatcher();
    #endif
    wifiScanLoop();

    if (!firmwareUpdateInProgress &&
        ((mqttIsEnabled() && MyConfParam.v_MQTT_UseVPN == 1) ||
         MyConfParam.v_NTP_UseVPN == 1)) {
      wireGuardEnsureStarted();
    }

    serviceMqttPending();

    if (!firmwareUpdateInProgress) {
      mqttHealthCheck();
      ntpLoop();
    }

    #ifdef ESP32
    if (pending_softap) { pending_softap = false; startsoftAP(); }
    #endif

    #ifdef OLED_1306
    serviceOledCfgButton();
    serviceGpio0Button();
    #endif

  #ifdef _ALEXA_
    fauxmo.handle();
  #endif

  Scheduler_ts.execute();

  // Sample largest free block from loop() — no HTTP/MQTT TCP buffers held here,
  // so this reflects the true idle heap, not a transient per-connection drop.
  { uint32_t now = millis(); if (now >= g_heapNextSampleMs) { g_heapMaxBlkCached = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); g_heapNextSampleMs = now + 5000; } }

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

  } // !firmwareUpdateInProgress

   firmwareOtaWatchdog();

  serviceRestartRequired();

  if (!firmwareUpdateInProgress) {

  #ifdef AppleHK
  #ifndef ESP32
	  my_homekit_loop();
  #endif  
  #endif   

  #ifndef ESP_MESH
    if (mqttIsEnabled()) tiker_MQTT_CONNECT.update(nullptr);
    Wifireconnecttimer.update(nullptr);
  #else
   #ifdef ESP_MESH_ROOT
     if (mqttIsEnabled()) tiker_MQTT_CONNECT.update(nullptr);
     #endif
  #endif

  #ifndef StepperMode
    serviceInputWatchers();
    #ifdef ESP32
    #ifdef WaterFlowSensor
    serviceWaterFlowSensor();
    #endif
    #endif
  #endif

  serviceCalendar();

 
  #ifndef StepperMode
  if (!started_in_confMode) {
    serviceHomeKitDeferredInit();
    servicePerSecond();
  }
  #if defined (HWver02)  || defined (HWver03) || defined (HWESP32)
    #ifndef SR04
    serviceTemperatureSensors();
    #endif
  #endif
  #endif  // !StepperMode

  #ifdef StepperMode
    serviceStepperMode();
  #endif

  } // !firmwareUpdateInProgress

} // end main loop()




// In-place replacement for char[] jsonPost buffers (CT_ProcessPower uses char[512],
// not Arduino String, to avoid heap fragmentation on the CT task path).
static void jsonbuf_replace(char* buf, size_t bufsz, const char* from, const char* to) {
    char* pos = strstr(buf, from);
    if (!pos) return;
    size_t flen = strlen(from), tlen = strlen(to);
    size_t tail = strlen(pos + flen) + 1;
    if ((size_t)(pos - buf) + tlen + tail > bufsz) return;
    memmove(pos + tlen, pos + flen, tail);
    memcpy(pos, to, tlen);
}

#if defined (_emonlib_)  || defined (_pressureSensor_)
    #ifdef _emonlib_
    void tskfn_EmonPublisher() {
        if (firmwareUpdateInProgress) return;

        if (!started_in_confMode){
          // Guard with both WiFi and MQTT state: mqttClient.connected() stays true
          // for several seconds after a hard WiFi drop because AsyncMqttClient has
          // no way to know the link died until the TCP keepalive fires. During that
          // window publish() writes to a dead socket and can corrupt the AsyncTCP
          // send buffer. Checking WiFi.isConnected() cuts the race window to the
          // same ~100 ms that the WiFi driver takes to set WL_DISCONNECTED.
          if (WiFi.isConnected() && mqttClient.connected()) {
            // Copy jsonPost inside the mutex, then publish outside it.
            // publish() can block on the TCP send buffer; holding the mutex across
            // it would let the Core-0 writer time out (50 ms) and overwrite jsonPost
            // while Core-1 still holds a pointer into the old buffer → heap corruption.
            static char publishBuf[512];
            bool ready = false;
            if (CT_1.jsonPostMutex &&
                xSemaphoreTake(CT_1.jsonPostMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
              ready = (strstr(CT_1.jsonPost, "[VOLTS2]") == nullptr);
              if (ready) {
                strncpy(publishBuf, CT_1.jsonPost, sizeof(publishBuf) - 1);
                publishBuf[sizeof(publishBuf) - 1] = '\0';
              }
              xSemaphoreGive(CT_1.jsonPostMutex);
            }
            if (ready) {
              mqttPublish((MyConfParam.v_CurrentTransformerTopic).c_str(), 0, RETAINED, publishBuf);
            }
          }
      }
    }
    #endif
    #ifdef _pressureSensor_
    void tskfn_EmonPublisher() {
      if (firmwareUpdateInProgress) return;
      if (!mqttClient.connected()) return;
      if (MyConfParam.maSTopic == "0") return;
      mqttPublish(TL136.maSTopic.c_str(), 0, true, TL136.jsonPost);
    }
    #endif

    void tskfn_emon_reader(){
        if (firmwareUpdateInProgress) return;

        #ifdef _emonlib_
            #ifndef _pressureSensor_
              if (!MyConfParam.v_CurrentTransformerTopic.startsWith("disabled:")) {
                // Trigger a new CT measurement only when the previous one has finished.
                // Sending notifications while ctMeasurementActive is set would cause
                // them to queue up, running calcVI back-to-back and starving the core-0
                // IDLE task, which triggers the task watchdog.
                if (ctMeasurementTask_handle && !ctMeasurementActive && !ctReadingReady) {
                    xTaskNotifyGive(ctMeasurementTask_handle);
                }
                if (!ctReadingReady) return; // results not ready yet - check next tick
                ctReadingReady = false;
                
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
          // CT136 publish moved to tskfn_EmonPublisher (runs at EmonPublisherIntervalMs, not every read)
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
            Relay * rtemp = static_cast<Relay *>(it);
            // Only increment n for relays that were actually registered with Espalexa
            // (those with v_AlexaName != "null"). Incrementing for all relays caused
            // index misalignment when some relays are skipped during addDevice().
            if (rtemp && rtemp->getRelayConfig()->v_AlexaName != "null") {
              EspalexaDevice* dev = espalexa.getDevice(n);
              if (dev) {
                dev->setState(digitalRead(rtemp->getRelayPin()) == HIGH);
                Serial.printf("\n [ALEXA   ] updating ALEXA %s to %d", dev->getName().c_str(), digitalRead(rtemp->getRelayPin()) == HIGH);
              }
              n++;
            }
          }
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
      g_adsMultiplier = mutiplyer;
      #ifdef _ADS1X15_DC_Current_
      manualOffset = (float)PAHSTConfig.manualOffset;
      #endif
      
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
        g_adsV2 = Voltage2;
        Serial.printf("\nADS 1115 Volatge 2 = %f, %u, %.2f", Voltage2, adc2, (Voltage2 * mutiplyer));

        adc3 = ads.readADC_SingleEnded(3);
        Voltage3 = (adc3 * ADSStep)/1000;
        g_adsV3 = Voltage3;
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
        g_adsV2 = Voltage2;
        //Serial.printf("\nADS 1115 Volatge 2 = %f, %u, %.2f, ADS1.toVoltage %f", Voltage2, val_2, (Voltage2 * mutiplyer), ADS1.toVoltage(ADS1.readADC(2)) * mutiplyer);
        Voltage3 = (val_3 * ADSStep)/1000;
        g_adsV3 = Voltage3;
        #ifdef _emonlib_
        if (CT_1.jsonPostMutex &&
            xSemaphoreTake(CT_1.jsonPostMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          char vbuf[24];
          snprintf(vbuf, sizeof(vbuf), "%.4f", Voltage2 * mutiplyer);
          jsonbuf_replace(CT_1.jsonPost, sizeof(CT_1.jsonPost), "[VOLTS2]", vbuf);
          snprintf(vbuf, sizeof(vbuf), "%.4f", Voltage3 * mutiplyer);
          jsonbuf_replace(CT_1.jsonPost, sizeof(CT_1.jsonPost), "[VOLTS3]", vbuf);
          xSemaphoreGive(CT_1.jsonPostMutex);
        }
        #endif
        //Serial.printf("\nADS 1115 ADS1.toVoltage 3 = %f, %u, %.2f, ADS1.toVoltage %f", Voltage3, val_3, (Voltage3 * mutiplyer), ADS1.toVoltage(ADS1.readADC(3)) * mutiplyer);

        #ifdef _ADS1X15_DC_Current_   /* read DC current */
          #ifndef _ADS1X15_CURRENT_   /* make sure pin is not used for _ADS1X5_CURRENT_*/
                mVperAmpValue = PAHSTConfig.AmpsVoltsRatio; 
                while (currentSampleCount < maxsamplecount )
                  {
                    // unsigned int tt = micros();
                    esp_task_wdt_reset(); // each I2C read can take ~8 ms; 50 samples = ~400 ms on loop task
                    currentSampleRead = ADS1.readADC_Differential_0_1();     //ADS1.readADC(currentAnalogInputPin);                                       /* read the sample value including offset value*/
                    // delay(1);
                    // float callibrationvalue = ADS1.readADC(calibrationPin)-1;
                    // Serial.printf("\n Actual value %f, vref %f", currentSampleRead, callibrationvalue);
                    // currentSampleRead -=  callibrationvalue;
                    // Serial.printf ("\n2 ADC measurment time = %u", micros() - tt);
                    // SerialDebug.print(" currentAnalogInputPin value is: "); SerialDebug.println(analogRead(currentAnalogInputPin));
                    // SerialDebug.print(" calibrationPin value is: "); SerialDebug.print(analogRead(calibrationPin));
                    // SerialDebug.println(" currentSampleRead value is: "); SerialDebug.print(analogRead(currentSampleRead));
                    float currentSampleReadmV = ADS1.toVoltage(currentSampleRead) * 1000.0f;
                    float csq = currentSampleReadmV * currentSampleReadmV;
                    currentSampleSum += csq ;                                                         /* accumulate total analog values for each sample readings*/
                    currentSampleSum_DC += currentSampleReadmV;
                    currentSampleCount += 1;                                                        /* to count and move on to the next following count */  
                  }

                  if (currentSampleCount == maxsamplecount){
                    currentMean = currentSampleSum/currentSampleCount;                                                /* average accumulated squared millivolt values*/
                    RMSCurrentMean = sqrt(currentMean);                                                             

                    currentMean_DC = currentSampleSum_DC /currentSampleCount;                                         /* average accumulated millivolt values*/

                    // Serial.printf("\n >>>>> Sample msqt value is: %f", RMSCurrentMean);
                    // Serial.printf("\n >>>>> Sample msqt DCvalue is: %f", currentMean_DC);
                    if (mVperAmpValue > 0.0f) {
                      FinalRMSCurrent = (RMSCurrentMean / mVperAmpValue) - manualOffset;                             /* calculate the final RMS current */
                      FinalDCCurrent =  (currentMean_DC / mVperAmpValue) - manualOffset;                             /* calculate the final DC current */
                      if(FinalRMSCurrent <= (625/mVperAmpValue/100))                                                  /* if the current detected is less than or up to 1%, set current value to 0A*/
                      { FinalRMSCurrent =0; }
                    } else {
                      FinalRMSCurrent = 0;
                      FinalDCCurrent = 0;
                    }

                    // Serial.printf(" \nThe Current RMS value is: %f", FinalRMSCurrent);
                    // Serial.printf(" \nThe Current DC value is: %f", FinalDCCurrent);

                    currentSampleSum = 0;
                    currentSampleSum_DC = 0;
                    currentSampleCount = 0;

                    g_adsFinalRMSCurrent = FinalRMSCurrent;
                    g_adsFinalDCCurrent  = FinalDCCurrent;

                    #ifdef _emonlib_
                    if (CT_1.jsonPostMutex &&
                        xSemaphoreTake(CT_1.jsonPostMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                      char vbuf[24];
                      snprintf(vbuf, sizeof(vbuf), "%.4f", FinalRMSCurrent);
                      jsonbuf_replace(CT_1.jsonPost, sizeof(CT_1.jsonPost), "[HST_AMPS]", vbuf);
                      snprintf(vbuf, sizeof(vbuf), "%.4f", FinalDCCurrent);
                      jsonbuf_replace(CT_1.jsonPost, sizeof(CT_1.jsonPost), "[DCAMPS]", vbuf);
                      xSemaphoreGive(CT_1.jsonPostMutex);
                    }
                    #endif
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
              display.setTextSize(1); 
              display.print(String(ADS1.toVoltage(ADS1.readADC(2)) * mutiplyer));
              display.setTextSize(1);        
      

              display.setCursor(rect_width + 10 ,StartRow + 2 );    
              display.println(F("ADC 4"));
              display.setCursor(rect_width + 8,StartRow + 16 );   
              display.setTextSize(1);                  
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

