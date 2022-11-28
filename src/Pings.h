#ifndef _PINGS_
#define _PINGS_

#include "Arduino.h";
#include "ConfigParams.h"

#ifdef ESP32
  #include <ESP32Ping.h>
#else
  #include "AsyncPing.h"
  
#endif

#ifndef ESP32
void definePingsCallbacks() ;
#endif
void servicePings();
#endif    