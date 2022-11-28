// uncomment to select the bord model
// **********************************************************
#ifndef ESP32                           // if esp8266 based hardware
// #define HWver02                      // original board design; one relay, no inputs
 #define HWver03                        // V03 board design, one relay, input header
 #define HWver03_4R                     // AliExpress 4 relays board
#else
 #define HWESP32                        // ESP32 based board. 1 relay, 6 inputs
 #define ESP32_2RBoard
// #define ESP32_3RBoard
// #define ESP32_4RBoard
 #define OLED_1306                    // OLED only available on ESP32
// #define OLED_ThingPulse
 #define _emonlib_  // https://learn.openenergymonitor.org/electricity-monitoring/ctac/ct-and-ac-power-adaptor-installation-and-calibration-theory   Available on ESP32 only
#define _pressureSensor_

#define PULLMODE_ INPUT_PULLUP

// #define PULLMODE_ INPUT_PULLDOWN
#endif

// select feature type
// *******************
// #define SR04                         // utrasonic sensor code 
// #define SR04_SERIAL 
// #define SolarHeaterControllerMode    // solar Water Heater Controller Mode. Relay on/off within temp sensors interval
// #define StepperMode
// #define blockingTime                 // better use blocking for ESP8266

// #define DEBUG_DISABLED
#define HK_name_len 30
// #define ESP_NOW
// #define ESP_MESH
// #define ESP_MESH_ROOT

// #define AppleHK
// #define _ALEXA_ 
// #define _ESP_ALEXA_

// #define INVERTERLINK
// #define WEBSOCKETS
// #define _ACS712_

//Ecobank Nigeria limited

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






