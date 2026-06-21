// uncomment to select the board model
// **********************************************************
/*
Feature	         Code symbol	                     GPIO
Temp sensor 1	   TempSensorPin / InputPin01	      GPIO33
Temp sensor 2	   SecondTempSensorPin / InputPin02	GPIO16
EmonLib current	CurrentPin A5	                  GPIO33
EmonLib voltage	VoltagePin A7	                  GPIO35
Pressure sensor	TLPressureSensor TL136(35u, ...)	GPIO35

Conflict:
For plain HWESP32 without ESP32_2RBoard:

Temp sensor 1: GPIO33
Temp sensor 2: GPIO16
_emonlib_: not allowed now
_pressureSensor_: not allowed now
So for plain HWESP32, there is no active conflict because EmonLib and pressure sensor cannot be enabled.

For HWESP32 + ESP32_2RBoard:

Temp sensor 1: GPIO33
Temp sensor 2: GPIO16
EmonLib current: A7 = GPIO35
EmonLib voltage: A6 = GPIO34
Pressure sensor: GPIO35
Conflicts in ESP32_2RBoard mode:

Temp sensors do not conflict with EmonLib.
Temp sensors do not conflict with pressure sensor.
EmonLib current and pressure sensor do conflict with each other on GPIO35, so do not enable _emonlib_ and _pressureSensor_ together unless one of those pins is changed.

*/

#define USE_LittleFS

#ifndef ESP32                           // if esp8266 based hardware
    // #define HWver02                  // original board design; one relay, no inputs
     #define HWver03                  // V03 board design, one relay, input header
    // #define HWver03_4R               // AliExpress 4 relays board
    
#else
    #define HWESP32                  // ESP32 based board. 1 relay, 6 inputs
    // #define ESP32_2RBoard            // for 2 relays based boards
    // #define ESP32_3RBoard            // for 3 relays based boards
    // #define ESP32_4RBoard            // for 4 relays based boards
    
    // #define OLED_1306                // OLED only available on ESP32
    // #define OLED_ThingPulse
    // #define _emonlib_                // ESP32_2RBoard only; uses CT/voltage ADC pins
    // #define _pressureSensor_         // ESP32_2RBoard only; uses pressure ADC pin
    //   #define _ADS1X15_                // uncomment to activate a default to ADS1115, ADS1115 is a 16-bit ADC, offering higher precision, while the ADS1015 is a 12-bit ADC with a higher maximum sample rate
    //   #define _ADS1015_                // uncomment to if using ADS1015
   //    #define _ADS_ASYNC_              // uncomment to use lib
     //#define _ADS1X15_CURRENT_        // uncomment to use ADS to measure current
     //#define _ADS1X15_VOLTAGE_        // uncomment to use ADS to measure voltage
   //  #define _ADS1X15_DC_Current_     // uncomment to use ADS to measure DC current on same pin used for _ADS1X15_CURRENT_ - these are exclusive
   //  #define _HST_                    // Uncomment to use DC Hall sensor
    // #define WaterFlowSensor

    // #define _NEWMETHOD_

    #define PULLMODE_ INPUT_PULLUP
    // #define PULLMODE_ INPUT_PULLDOWN

    // #define DEBUG_ENABLED               // enable debug messages theough TELNET port 23
#endif

// select feature type
// *******************
// #define MQTTPostInitStatus             // until further granual dev. this allows/diallows posting of initial status of Inputsnsr02
// #define SR04                         // utrasonic sensor code
// #define SR04_SERIAL
// #define SolarHeaterControllerMode    // solar Water Heater Controller Mode. Relay on/off within temp sensors interval
// #define StepperMode
// #define blockingTime                 // better use blocking for ESP8266
// #define _AUTOMATION_RULES_           // Sensor-condition-driven relay automation (Automation.html). No hardware required.
// #define _REMOTE_SENSORS_             // MQTT-sourced remote board sensor values for use in automation conditions (RemoteSensors.html). Requires MQTT.


#if defined(_ADS1X15_DC_Current_) && defined(_ADS1X15_CURRENT_)
#error "_ADS1X15_DC_Current_ and _ADS1X15_CURRENT_ both use ADS current inputs and must not be enabled together."
#endif

#if defined(_ADS1X15_DC_Current_) && !defined(_ADS_ASYNC_)
#error "_ADS1X15_DC_Current_ currently requires _ADS_ASYNC_ because the DC current routine uses the ADS1X15 async library API."
#endif

#define HK_name_len 30
// #define ESP_NOW

// #define ESP_MESH
// #define ESP_MESH_ROOT

// #define AppleHK
// #define _ALEXA_ 
// #define _ESP_ALEXA_

// #define INVERTERLINK
// #define WEBSOCKETS
   #define _ACS712_

#if defined(HWESP32) && !defined(ESP32_2RBoard) && (defined(_emonlib_) || defined(_pressureSensor_))
  #error "_emonlib_ and _pressureSensor_ are only supported when ESP32_2RBoard is enabled. Disable them for plain HWESP32."
#endif

#if defined(ESP32_2RBoard) && defined(_emonlib_) && defined(_pressureSensor_)
  #error "_emonlib_ and _pressureSensor_ both use GPIO35 in ESP32_2RBoard mode. Enable only one or change one sensor pin."
#endif

#ifdef INVERTERLINK
  #ifdef ESP32
    #define SERIAL_DBG Serial
    #define SERIAL_INVERTER Serial2
  #else
    #define SERIAL_DBG Serial
    #define SERIAL_INVERTER Serial 
  #endif    
#else
  #define SERIAL_DBG Serial
  #define SERIAL_INVERTER Serial 
#endif






