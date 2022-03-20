// uncomment to select the bord model
// **********************************************************
#ifndef ESP32                           // if esp8266 based hardware
// #define HWver02                      // original board design; one relay, no inputs
 #define HWver03                        // V03 board design, one relay, input header
// #define HWver03_4R                   // AliExpress 4 relays board
// #define AppleHK
#else
 #define HWESP32                        // ESP32 based board. 1 relay, 6 inputs
#endif

// select feature type
// **********************************************************
  #define SR04                         // utrasonic sensor code 
// #define SolarHeaterControllerMode    // solar Water Heater Controller Mode. Relay on/off within temp sensors interval
// #define StepperMode
#define DEBUG_DISABLED
#define HK_name_len 30
//#define blockingTime
