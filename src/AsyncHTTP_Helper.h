#ifndef HTTP_
#define HTTP_

#include <ESPAsyncWebServer.h>
#include <ConfigParams.h>
#include <Arduino.h>
#include <string.h>
#include <ExecOTA.h>
#include <JSONConfig.h>


extern bool restartRequired;

String processor(const String& var);
void SetAsyncHTTP();


#endif
