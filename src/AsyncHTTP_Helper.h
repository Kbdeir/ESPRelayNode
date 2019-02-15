//#ifndef __HTTP_
//#define __HTTP_
#pragma once



#include <Arduino.h>
#include <string.h>

#include <ESPAsyncWebServer.h>
#include <ConfigParams.h>
#include <ExecOTA.h>
#include <JSONConfig.h>
#include "KSBNTP.h"
#include <SPIFFSEditor.h>
#include <MQTT_Processes.h>
#include <RelayClass.h>
#include <TimerClass.h>


extern bool restartRequired;

//String processor(const String& var);
void SetAsyncHTTP();


//#endif
