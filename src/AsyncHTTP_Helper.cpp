#include <AsyncHTTP_Helper.h>
#include <KSBNTP.h>
#include <SPIFFSEditor.h>
#include <MQTT_Processes.h>
#include <RelayClass.h>
#include <TimerClass.h>

extern NodeTimer NTmr;

//const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
AsyncWebServer AsyncWeb_server(80);

bool restartRequired = false;  // Set this flag in the callbacks to restart ESP in the main loop
File cf;

String timerprocessor(const String& var)
{

  if(var == F( "TNBT" ))  return  String(NTmr.id);
  if(var == F( "Dfrom" ))  return  String(NTmr.spanDatefrom.c_str());
  if(var == F( "DTo" ))  return String( NTmr.spanDateto.c_str());
  if(var == F( "TFrom" ))  return  String(NTmr.spantimefrom.c_str());
  if(var == F( "TTo" ))  return String( NTmr.spantimeto.c_str());
  if(var == F( "CMonday" )) { if (NTmr.weekdays->Monday) return "1\" checked=\"\""; };
  if(var == F( "CTuesday" )) { if (NTmr.weekdays->Tuesday ) return "1\" checked=\"\""; };
  if(var == F( "CWednesday" )) { if (NTmr.weekdays->Wednesday) return "1\" checked=\"\""; };
  if(var == F( "CThursday" )) { if (NTmr.weekdays->Thursday ) return "1\" checked=\"\""; };
  if(var == F( "CFriday" )) { if (NTmr.weekdays->Friday ) return "1\" checked=\"\""; };
  if(var == F( "CSaturday" )) { if (NTmr.weekdays->Saturday ) return "1\" checked=\"\""; };
  if(var == F( "CSunday" )) { if (NTmr.weekdays->Sunday ) return "1\" checked=\"\""; };
  if(var == F( "CEnabled" )) { if (NTmr.enabled ) return "1\" checked=\"\""; };

  if(var == F( "Mark_Hours" ))  return  String(NTmr.Mark_Hours);
  if(var == F( "Mark_Minutes" ))  return  String(NTmr.Mark_Minutes);

  if(var == F( "TMTYPEedit" ))  return String(NTmr.TM_type);

  return String();
}

String processor(const String& var)
{
  if(var == F( "MACADDR" ))  return (String(MAC.c_str()) + " - Chip id: " + CID());
  if(var == F( "ssid" ))  return  String(MyConfParam.v_ssid.c_str());
  if(var == F( "pass" ))  return String( MyConfParam.v_pass.c_str());
  if(var == F( "PhyLoc" ))  return String( MyConfParam.v_PhyLoc.c_str());
  if(var == F( "MQTT_BROKER" ))  return String( MyConfParam.v_MQTT_BROKER.c_str());
  if(var == F( "MQTT_B_PRT" ))  return String( MyConfParam.v_MQTT_B_PRT.c_str());
  if(var == F( "PUB_TOPIC1" ))  return String( MyConfParam.v_PUB_TOPIC1.c_str());
  if(var == F( "FRM_IP" ))  return String( MyConfParam.v_FRM_IP.c_str());
  if(var == F( "FRM_PRT" ))  return String( MyConfParam.v_FRM_PRT.c_str());
  if(var == F( "ACSmultiple" ))  return String( MyConfParam.v_ACSmultiple.c_str());
  if(var == F( "ACS_Sensor_Model" ))  return String( MyConfParam.v_ACS_Sensor_Model.c_str());
  if(var == F( "ttl" ))  return String( MyConfParam.v_ttl.c_str());
  if(var == F( "STATE_PUB_TOPIC" ))  return String( MyConfParam.v_STATE_PUB_TOPIC.c_str());
  if(var == F( "InputPin12_STATE_PUB_TOPIC" ))  return String( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str());
  if(var == F( "InputPin14_STATE_PUB_TOPIC" ))  return String( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str());
  if(var == F( "TTL_PUB_TOPIC" ))  return String( MyConfParam.v_ttl_PUB_TOPIC.c_str());
  if(var == F( "CURR_TTL_PUB_TOPIC" ))  return String( MyConfParam.v_CURR_TTL_PUB_TOPIC.c_str());
  if(var == F( "i_ttl_PUB_TOPIC" ))  return String( MyConfParam.v_i_ttl_PUB_TOPIC.c_str());
  if(var == F( "ACS_AMPS" ))  return String( MyConfParam.v_ACS_AMPS.c_str());
  if(var == F( "tta" ))  return String( MyConfParam.v_tta.c_str());
  if(var == F( "Max_Current" ))  return String( MyConfParam.v_Max_Current.c_str());
  if(var == F( "timeserver" ))  return String( MyConfParam.v_timeserver.c_str());
  if(var == F( "ntptz" ))  return String( MyConfParam.v_ntptz.c_str());
  if(var == F( "LWILL_TOPIC" ))  return String( MyConfParam.v_LWILL_TOPIC.c_str());
  if(var == F( "SUB_TOPIC1" ))  return String( MyConfParam.v_SUB_TOPIC1.c_str());
  if(var == F( "PIC_Active" )) { if (MyConfParam.v_PIC_Active == "1") return "1\" checked=\"\""; };
  if(var == F( "MQTT_Active" )) { if (MyConfParam.v_MQTT_Active == "1") return "1\" checked=\"\""; };
  if(var == F( "GPIO12_TOG" )) {  if (MyConfParam.v_GPIO12_TOG == "1") return "1\" checked=\"\"";	};
  if(var == F( "Copy_IO" )) { if (MyConfParam.v_Copy_IO == "1") return "1\" checked=\"\""; };
  if(var == F( "ACS_Active" )) { if (MyConfParam.v_ACS_Active == "1") return "1\" checked=\"\""; };
  if(var == F( "myppp" )) { if (MyConfParam.v_myppp == "1") return "1\" checked=\"\""; };
  if(var == F( "Update_now" )) { if (MyConfParam.v_Update_now == "1") return "1\" checked=\"\""; };
  if(var == F( "systemtime" ))  return digitalClockDisplay();
  return String();
}


void SetAsyncHTTP(){

  #ifdef ESP32
    AsyncWeb_server.addHandler(new SPIFFSEditor(SPIFFS,"user","pass"));
  #else
    AsyncWeb_server.addHandler(new SPIFFSEditor("user","pass"));
  #endif

  AsyncWeb_server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  AsyncWeb_server.on("/JConfig", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate("user", "pass")) return request->requestAuthentication();
    request->send(SPIFFS, "/config.json");
      // int args = request->args();
    });

  AsyncWeb_server.on("/Timer1", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      //request->send(SPIFFS, "/Timer1.html");

        request->send(SPIFFS, "/Timer1.html", String(), false, timerprocessor);

        // int args = request->args();
    });

  AsyncWeb_server.on("/savetimer.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/savetimer.html");
            saveNodeTimer(request);
            loadNodeTimer("/timer.json",NTmr);
            //loadConfig(MyConfParam);
            //uint16_t packetIdPub2 = mqttClient.publish( MyConfParam.v_i_ttl_PUB_TOPIC.c_str(), 2, true, MyConfParam.v_ttl.c_str());
            //uint16_t packetIdPub3 = mqttClient.publish( MyConfParam.v_ttl_PUB_TOPIC.c_str(), 2, true, MyConfParam.v_ttl.c_str());
      });

  AsyncWeb_server.on("/Apply.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate("user", "pass")) return request->requestAuthentication();
    request->send(SPIFFS, "/Apply.html");
      // int args = request->args();
          #ifdef USEPREF
          SaveParams(MyConfParam,preferences,request);
          #endif
          saveConfig(MyConfParam, request);
          loadConfig(MyConfParam);
          uint16_t packetIdPub2 = mqttClient.publish( MyConfParam.v_i_ttl_PUB_TOPIC.c_str(), 2, true, MyConfParam.v_ttl.c_str());
          uint16_t packetIdPub3 = mqttClient.publish( MyConfParam.v_ttl_PUB_TOPIC.c_str(), 2, true, MyConfParam.v_ttl.c_str());
    });

    /*
      AsyncWeb_server.on("/updatepage", HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", serverIndex);
      response->addHeader("Connection", "close");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);
    });
    */

  AsyncWeb_server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
      // the request handler is triggered after the upload has finished...
      // create the response, add header, and send response
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError())?"FAIL":"OK... Update Successful");
      response->addHeader("Connection", "close");
      response->addHeader("Access-Control-Allow-Origin", "*");
      restartRequired = true;  // Tell the main loop to restart the ESP
      request->send(response);
        },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
                            //Upload handler chunks in data

                            if(!index){ // if index == 0 then this is the first frame of data
                              Serial.printf("\n UploadStart: %s\n", filename.c_str());
                              // Serial.setDebugOutput(true);

                              #ifdef ESP32
                                  Update.begin();
                              #else
                                //   calculate sketch space required for the update
                                  uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                                  if(!Update.begin(maxSketchSpace)){//start with max available size
                                    Update.printError(Serial);
                                  }
                                  Update.runAsync(true); // tell the updaterClass to run in async mode
                              #endif
                            }

                            //Write chunked data to the free sketch space
                            if(Update.write(data, len) != len){
                                Update.printError(Serial);
                            }

                            if(final){ // if the final flag is set then this is the last frame of data
                              if(Update.end(true)){ //true to set the size to the current progress
                                  Serial.printf("Update Success: %u B\nRebooting...\n", index+len);
                                } else {
                                  Update.printError(Serial);
                                }
                                // Serial.setDebugOutput(false);
                            }
                          });

      /*AsyncWeb_server.on("/Config2.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/Config2.html", String(), false, processor);
      });*/


  AsyncWeb_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/Config.html", String(), false, processor);
    });

      /*AsyncWeb_server.on("/firmware.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          request->send(SPIFFS, "/firmware.html", String(), false, processor);
      		MyConfParam.v_Update_now =  MyConfParam.v_Update_now;
          execOTA(MyConfParam.v_FRM_IP,MyConfParam.v_FRM_PRT.toInt());
          });
          */

	AsyncWeb_server.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
	      request->send(SPIFFS, "/Reset.html");
				Serial.println(F("rebooting.."));
        restartRequired = true; // main function will take care of restarting
	      });

        /*
        AsyncWeb_server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
          // the request handler is triggered after the upload has finished...
          // create the response, add header, and send response
          AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (true)?"OK... upload Successful":"OK... upload Successful");
          response->addHeader("Connection", "close");
          response->addHeader("Access-Control-Allow-Origin", "*");
          restartRequired = false;  // Tell the main loop to restart the ESP
          request->send(response);
            },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){

                                if (!index) {

                                  Serial.print (index);
                                //  cf = SPIFFS.open("/Config1.html", "w");
                                String temp = "/" + filename;
                                Serial.printf("\n saving to file --->%s\n ",temp.c_str());
                                  cf = SPIFFS.open(temp, "w");
                                  if (!cf) {
                                    Serial.println(F("can not create file "));
                                    //return ERROR_OPENING_FILE;
                                  } else {
                                    Serial.println(F("file created"));
                                  }
                                  Serial.printf("\n UploadStart: %s\n", filename.c_str());
                                  Serial.printf("\n first frame: %s\n", filename.c_str());
                                  Serial.setDebugOutput(true);
                                }

                                  cf.write(data,len);
                                  printf("%s", (const char*)data);
                                  Serial.print(index);

                                if(final) {
                                  Serial.printf("\n UploadEnd: %s (%u)\n", filename.c_str(), index+len);
                                  cf.close();
                                }
                              });
        */
	Serial.println(F("Starting HTTP server"));
  AsyncWeb_server.begin();
}
