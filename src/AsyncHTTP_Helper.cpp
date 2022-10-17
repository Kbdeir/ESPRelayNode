#ifndef ESP32
  #include <arduino_homekit_server.h>
#endif
#include <AsyncHTTP_Helper.h> 
#include <SPIFFSEditor.h>
#include <MQTT_Processes.h>
#include <RelayClass.h>
#include <TimerClass.h>
#include <TempConfig.h>
#include <TLPressureSensor.h>
#include <AccelStepper.h>
#include <digitalClockDisplay.h>
#include <ConfigParams.h>

#ifdef ESP32
  #ifdef AppleHK
    #include <ESP32HAP.h>
  #endif 
#endif

#ifdef _ESP_ALEXA_
  #define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
  #include <Espalexa.h>
  extern Espalexa espalexa; 
#endif
#ifdef _ALEXA_
#include "fauxmoESP.h"
extern fauxmoESP fauxmo;
#endif

extern boolean CalendarNotInitiated ;
extern NodeTimer NTmr;
extern TempConfig PTempConfig;
#ifdef _pressureSensor_
extern TLPressureSensor TL136;
#endif 
extern float MCelcius;
extern float ACS_I_Current;
extern bool homekitNotInitialised;

extern void setupInputs();
extern void clearIRMap();

extern bool webing;


//extern std::vector<void *> relays ; // a list to hold all relays
//extern  int currentPosition;
//extern  int newPosition;  

#ifdef StepperMode
extern  AccelStepper shadeStepper;
extern bool steperrun;
#endif

uint8_t AppliedRelayNumber = 0;

// const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
// AsyncWebServer AsyncWeb_server(80);
#if defined _ESP_ALEXA_
AsyncWebServer AsyncWeb_server(80);
#else
// AsyncWebServer AsyncWeb_server(8080);
AsyncWebServer AsyncWeb_server(80);
#endif


// AsyncEventSource events("/events"); // https://randomnerdtutorials.com/esp32-web-server-sent-events-sse/
// AsyncWebSocket ws("/test");

bool restartRequired = false;  // Set this flag in the callbacks to restart ESP in the main loop
//File cf;


String timerprocessor(const String& var)
{
  if(var == F( "systemtime" ))          return digitalClockDisplay();
  if(var == F( "TNBT" ))                return  String(NTmr.id);
  if(var == F( "TRelay" ))              return  String(NTmr.relay);
  if(var == F( "Dfrom" ))               return  String(NTmr.spanDatefrom.c_str());
  if(var == F( "DTo" ))                 return String( NTmr.spanDateto.c_str());
  if(var == F( "TFrom" ))               return  String(NTmr.spantimefrom.c_str());
  if(var == F( "TTo" ))                 return String( NTmr.spantimeto.c_str());
  if(var == F( "CMonday" ))             { if (NTmr.weekdays->Monday)    return "1\" checked=\"\""; };
  if(var == F( "CTuesday" ))            { if (NTmr.weekdays->Tuesday )  return "1\" checked=\"\""; };
  if(var == F( "CWednesday" ))          { if (NTmr.weekdays->Wednesday) return "1\" checked=\"\""; };
  if(var == F( "CThursday" ))           { if (NTmr.weekdays->Thursday ) return "1\" checked=\"\""; };
  if(var == F( "CFriday" ))             { if (NTmr.weekdays->Friday )   return "1\" checked=\"\""; };
  if(var == F( "CSaturday" ))           { if (NTmr.weekdays->Saturday ) return "1\" checked=\"\""; };
  if(var == F( "CSunday" ))             { if (NTmr.weekdays->Sunday )   return "1\" checked=\"\""; };
  if(var == F( "CEnabled" ))            { if (NTmr.enabled)             return "1\" checked=\"\""; };
  if(var == F( "Mark_Hours" ))          return String(NTmr.Mark_Hours);
  if(var == F( "Mark_Minutes" ))        return String(NTmr.Mark_Minutes);
  if(var == F( "TMTYPEedit" ))          return String(NTmr.TM_type);
  /*if(var == F( "Testchar" ))            return [](){
        String s = String(NTmr.Testchar);
        s.replace('%', '-');
        return s;
      }();*/


  if(var == F( "TSTATE" ))              return
      [](){
        if (getrelaybynumber(NTmr.relay) != nullptr) {
          return (getrelaybynumber(NTmr.relay)->timerpaused == 1) ? "PAUSED" : "NOT PAUSED";
        } else return "NA";
      }();

  if(var == F( "RSTATE" ))              return
      [](){
        if (getrelaybynumber(NTmr.relay) != nullptr) {
          return (getrelaybynumber(NTmr.relay)->readrelay() == HIGH) ? "ON" : "OFF";
        } else return "NA";
      }();

  return String();
}


String TempConfProcessor(const String& var)
{

  if(var == F( "TNBT" ))                return String(PTempConfig.id);
  if(var == F( "TRelay" ))              return String(PTempConfig.relay);
  if(var == F( "spanTempfrom" ))        return String(PTempConfig.spanTempfrom);
  if(var == F( "spanTempto" ))          return String(PTempConfig.spanTempto);
  if(var == F( "spanBuffer" ))          return String(PTempConfig.spanBuffer);  
  if(var == F( "CEnabled" ))            { if (PTempConfig.enabled)             return "1\" checked=\"\""; };

  return String();
}

#ifdef _pressureSensor_
String TLProcessor(const String& var)
{
  if(var == F( "maSTopic" ))              return String(TL136.maSTopic);
  if(var == F( "maSHL" ))                 return String(TL136.max_sensor_measurment_capacity);
  if(var == F( "TankHeight" ))            return String(TL136.max_tank_capacity);  
  if(var == F( "maSLC" ))                 return String(TL136.maSLC);
  if(var == F( "maSHC" ))                 return String(TL136.maSHC);  
  if(var == F( "maBurdenResistor" ))      return String(TL136.BurdenResistorValue);  
  if(var == F( "systemtime" ))            return digitalClockDisplay();
  if(var == F( "ma_ADC" ))                return String(TL136.sampleI);  
  return String();
}
#endif



String processor(const String& var)
{
  Relay * AppliedRelay = getrelaybynumber(AppliedRelayNumber);

  // Serial.println(String("AppliedRelayNumber = ") + AppliedRelayNumber + " \n");

  if(var == F( "MACADDR" ))             return (String(MAC.c_str()) + " - Chip id: " + CID());
  if(var == F( "ssid" ))                return  String(MyConfParam.v_ssid.c_str());
  if(var == F( "pass" ))                return String( MyConfParam.v_pass.c_str());

  if(var == F( "mqttUser" ))            return  String(MyConfParam.v_mqttUser.c_str());
  if(var == F( "mqttPass" ))            return String( MyConfParam.v_mqttPass.c_str());

  if(var == F( "RWD" ))  return String( MyConfParam.v_Reboot_on_WIFI_Disconnection);

  if(var == F( "PhyLoc" ))              return String( MyConfParam.v_PhyLoc.c_str());
  if(var == F( "MQTT_BROKER" ))         return String(MyConfParam.v_MQTT_BROKER.c_str()) ; // MyConfParam.v_MQTT_BROKER.toString();
  if(var == F( "MQTT_B_PRT" ))          return String( MyConfParam.v_MQTT_B_PRT);

  if (AppliedRelay != nullptr) {
    if(var == F( "ACS_elasticity" ))    return String( AppliedRelay->RelayConfParam->v_ACS_elasticity); 
    if(var == F( "RELAYNB" ))             return String( AppliedRelay->RelayConfParam->v_relaynb);
    if(var == F( "PUB_TOPIC1" ))          return String( AppliedRelay->RelayConfParam->v_PUB_TOPIC1.c_str());
    if(var == F( "TemperatureValue" ))    return String( AppliedRelay->RelayConfParam->v_TemperatureValue.c_str());

    if(var == F( "AlexaName" ))    return String( AppliedRelay->RelayConfParam->v_AlexaName.c_str());

    if(var == F( "ACS_Sensor_Model" ))    return String( AppliedRelay->RelayConfParam->v_ACS_Sensor_Model.c_str());
    if(var == F( "ttl" ))                 return String( AppliedRelay->RelayConfParam->v_ttl);
    if(var == F( "STATE_PUB_TOPIC" ))     return String( AppliedRelay->RelayConfParam->v_STATE_PUB_TOPIC.c_str());
    if(var == F( "TTL_PUB_TOPIC" ))       return String( AppliedRelay->RelayConfParam->v_ttl_PUB_TOPIC.c_str());
    if(var == F( "CURR_TTL_PUB_TOPIC" ))  return String( AppliedRelay->RelayConfParam->v_CURR_TTL_PUB_TOPIC.c_str());
    if(var == F( "i_ttl_PUB_TOPIC" ))     return String( AppliedRelay->RelayConfParam->v_i_ttl_PUB_TOPIC.c_str());
    if(var == F( "ACS_AMPS" ))            return String( AppliedRelay->RelayConfParam->v_ACS_AMPS.c_str());
    if(var == F( "tta" ))                 return String( AppliedRelay->RelayConfParam->v_tta);
    if(var == F( "Max_Current" ))         return String( AppliedRelay->RelayConfParam->v_Max_Current);
    if(var == F( "LWILL_TOPIC" ))         return String( AppliedRelay->RelayConfParam->v_LWILL_TOPIC.c_str());
    if(var == F( "SUB_TOPIC1" ))          return String( AppliedRelay->RelayConfParam->v_SUB_TOPIC1.c_str());
    if(var == F( "ACS_Active" ))          { if (AppliedRelay->RelayConfParam->v_ACS_Active) return "1\" checked=\"\""; }; 
  }

  if(var == F( "FRM_IP" ))              return MyConfParam.v_FRM_IP.toString();
  if(var == F( "FRM_PRT" ))             return String( MyConfParam.v_FRM_PRT);

  if(var == F( "I12_STS_PTP"))          return String( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str());
  if(var == F( "I14_STS_PTP"))          return String( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str());

  #ifdef ESP32
    if(var == F( "I01_STS_PTP"))          return String( MyConfParam.v_InputPin01_STATE_PUB_TOPIC.c_str());
    if(var == F( "I02_STS_PTP"))          return String( MyConfParam.v_InputPin02_STATE_PUB_TOPIC.c_str());
  #endif

  if(var == F( "I03_STS_PTP"))          return String( MyConfParam.v_InputPin03_STATE_PUB_TOPIC.c_str());
  if(var == F( "I04_STS_PTP"))          return String( MyConfParam.v_InputPin04_STATE_PUB_TOPIC.c_str());
  if(var == F( "I05_STS_PTP"))          return String( MyConfParam.v_InputPin05_STATE_PUB_TOPIC.c_str());
  if(var == F( "I06_STS_PTP"))          return String( MyConfParam.v_InputPin06_STATE_PUB_TOPIC.c_str()); 

  if(var == F( "timeserver" ))          return MyConfParam.v_timeserver.toString() ;
  if(var == F( "Pingserver" ))          return MyConfParam.v_Pingserver.toString() ;
  if(var == F( "ntptz" ))               return String( MyConfParam.v_ntptz);
  if(var == F( "MQTT_Active"))         { if (MyConfParam.v_MQTT_Active) return "1\" checked=\"\""; };
  if(var == F( "Update_now" ))         { if (MyConfParam.v_Update_now)  return "1\" checked=\"\""; };
  if(var == F( "systemtime" ))          return digitalClockDisplay();
  if(var == F( "heap" ))                return String(ESP.getFreeHeap());

  if(var == F( "ATEMP" ))               return [](){
    //char tmp[10];
    //itoa(MCelcius,tmp,10);
    return String(MCelcius);
  }();

  if(var == F( "ACS" ))                return String(ACS_I_Current);
  //String(MCelcius);
  if(var == F( "TOGGLE_BTN_PUB_TOPIC" ))  return String( MyConfParam.v_TOGGLE_BTN_PUB_TOPIC.c_str());
  if(var == F( "I0MODE" ))                return String( MyConfParam.v_IN0_INPUTMODE);
  if(var == F( "I1MODE" ))                return String( MyConfParam.v_IN1_INPUTMODE);
  if(var == F( "I2MODE" ))                return String( MyConfParam.v_IN2_INPUTMODE);

  if(var == F( "I3MODE" ))              return String( MyConfParam.v_IN3_INPUTMODE);
  if(var == F( "I4MODE" ))              return String( MyConfParam.v_IN4_INPUTMODE);
  if(var == F( "I5MODE" ))              return String( MyConfParam.v_IN5_INPUTMODE);
  if(var == F( "I6MODE" ))              return String( MyConfParam.v_IN6_INPUTMODE);    

  if(var == F( "RSTATE0" ))               return (getrelaybynumber(AppliedRelayNumber)->readrelay() == HIGH) ? "ON" : "OFF";
  if(var == F( "RSTATE1" ))               return [](){
    if (getrelaybynumber(AppliedRelayNumber) != nullptr) {
      return (getrelaybynumber(AppliedRelayNumber)->readrelay() == HIGH) ? "ON" : "OFF";
    } else return "NA";
  }();
  
  if(var == F( "Sonar_distance" ))      return String( MyConfParam.v_Sonar_distance.c_str());
  if(var == F( "Sonar_distance_max" ))  return String( MyConfParam.v_Sonar_distance_max);

  if(var == F( "CurrentTransformer_max_current" ))  return String( MyConfParam.v_CurrentTransformer_max_current);
  if(var == F( "calibration" ))  return String( MyConfParam.v_calibration);    
  if(var == F( "CurrentTransformerTopic" ))  return String( MyConfParam.v_CurrentTransformerTopic);   
  if(var == F( "ToleranceOffTime" ))  return String( MyConfParam.v_ToleranceOffTime);   
  if(var == F( "CT_adjustment" ))  return String( MyConfParam.v_CT_adjustment);     
  if(var == F( "ToleranceOnTime" ))  return String( MyConfParam.v_ToleranceOnTime);   
  if(var == F( "CT_MaxAllowed_current" ))  return String( MyConfParam.v_CT_MaxAllowed_current);   
  if(var == F( "CT_saveThreshold" ))  return String( MyConfParam.v_CT_saveThreshold);         
  
  if(var == F( "Screen_orientation" ))  return String( MyConfParam.v_Screen_orientation);              


  return String();
}


String IRMAPprocessor(const String& var)
{
  if(var == F( "I1" ))        return String( myIRMap.I1);
  if(var == F( "I2" ))        return String( myIRMap.I2);
  if(var == F( "I3" ))        return String( myIRMap.I3);
  if(var == F( "I4" ))        return String( myIRMap.I4);
  if(var == F( "I5" ))        return String( myIRMap.I5);
  if(var == F( "I6" ))        return String( myIRMap.I6);
  if(var == F( "I7" ))        return String( myIRMap.I7);
  if(var == F( "I8" ))        return String( myIRMap.I8);
  if(var == F( "I9" ))        return String( myIRMap.I9);
  if(var == F( "I10" ))       return String( myIRMap.I10);
  if(var == F( "R1" ))        return String( myIRMap.R1);
  if(var == F( "R2" ))        return String( myIRMap.R2);
  if(var == F( "R3" ))        return String( myIRMap.R3);
  if(var == F( "R4" ))        return String( myIRMap.R4);
  if(var == F( "R5" ))        return String( myIRMap.R5);
  if(var == F( "R6" ))        return String( myIRMap.R6);
  if(var == F( "R7" ))        return String( myIRMap.R7);
  if(var == F( "R8" ))        return String( myIRMap.R8);
  if(var == F( "R9" ))        return String( myIRMap.R9);
  if(var == F( "R10" ))       return String( myIRMap.R10);

  return String();
}

/*
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.println("Websocket client connection received");
  } else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");
  } else if(type == WS_EVT_DATA){
    Serial.println("Data received: ");
    for(int i=0; i < len; i++) {
          Serial.print((char) data[i]);
    }
    Serial.println();
    String msg = "";

    for(size_t i=0; i < len; i++) {
      msg += (char) data[i];
    }

    if (msg == "on") {
      client->text("on command executed");
      Relay * rtmp =  getrelaybynumber(0);
      rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
    }
    if (msg == "off") {
      client->text("off command executed");
      Relay * rtmp =  getrelaybynumber(0);
      rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
    }
  }
}
*/


void SetAsyncHTTP(){

  #ifdef ESP32
    AsyncWeb_server.addHandler(new SPIFFSEditor(SPIFFS,"user","pass"));
  #else
    AsyncWeb_server.addHandler(new SPIFFSEditor( "user","pass"));
  #endif

  //ws.onEvent(onWsEvent);
  //AsyncWeb_server.addHandler(&ws);

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
          if (request->hasParam("GetTimer")) {
            AsyncWebParameter * Para = request->getParam("GetTimer");
            String tmp = Para->value();
            char  timerfilename[20] = "";
            strcpy(timerfilename, "/timer");
            strcat(timerfilename, tmp.c_str());
            strcat(timerfilename, ".json");
            if (loadNodeTimer(timerfilename,NTmr)== SUCCESS) {
                  request->send(SPIFFS, "/Timer1.html", String(), false, timerprocessor);
            } else {
                  request->send(SPIFFS, "/Timer1.html");
            };
        } else {
          request->send(SPIFFS, "/Timer1.html");
        }
        // int args = request->args();
    });

    AsyncWeb_server.on("/AdvancedTemperatureConf.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        config_read_error_t res = loadTempConfig("/tempconfig.json",PTempConfig); 
        request->send(SPIFFS, "/AdvancedTemperatureConf.html", String(), false, TempConfProcessor);
      });

    AsyncWeb_server.on("/AdvancedTempSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/AdvancedTempSave.html");
            saveTempConfig(request);
            config_read_error_t res = loadTempConfig("/tempconfig.json",PTempConfig);  
    });

    AsyncWeb_server.on("/savetimer.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/savetimer.html");
            saveNodeTimer(request);
            CalendarNotInitiated = true;
    });
    
    #ifdef _pressureSensor_
    AsyncWeb_server.on("/PressureSensorConfig.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        config_read_error_t res = TLloadconfig("/PressureSensorConfig.json",TL136); 
        request->send(SPIFFS, "/PressureSensorConfig.html", String(), false, TLProcessor);
      });      

    AsyncWeb_server.on("/PressureSensorSave.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/PressureSensorSave.html");
            TLsaveconfig(request); 
            config_read_error_t res = TLloadconfig("/PressureSensorConfig.json",TL136);  
    });
    #endif

    AsyncWeb_server.on("/wscontrol.html", HTTP_GET, [](AsyncWebServerRequest *request){
          if (!request->authenticate("user", "pass")) return request->requestAuthentication();
          AppliedRelayNumber = 0;
          if (request->hasParam("GETRELAYNB")) {
              String t = request->getParam("GETRELAYNB")->value();
              AppliedRelayNumber = t.toInt();
          
          if (request->hasParam("RELAYACTION")) {
            String msg = request->getParam("RELAYACTION")->value();
            if (msg == "ON") {
              Relay * rtmp =  getrelaybynumber(AppliedRelayNumber);
              rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
            }
            if (msg == "OFF") {
              Relay * rtmp =  getrelaybynumber(AppliedRelayNumber);
              rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
            }
          }
          }
          request->send(SPIFFS, "/wscontrol.html", String(), false, processor);
    });

    AsyncWeb_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        AppliedRelayNumber = 0;
              webing = true;
        if (request->hasParam("GETRELAYNB")) {
            String t = request->getParam("GETRELAYNB")->value();
            AppliedRelayNumber = t.toInt();
            Relay * rtmp =  getrelaybynumber(AppliedRelayNumber);
            if (rtmp != nullptr ) {
              if (request->hasParam("RELAYACTION")) {
                String msg = request->getParam("RELAYACTION")->value();
                if (msg == "ON") {
                  rtmp->mdigitalWrite(rtmp->getRelayPin(),HIGH);
                  #ifdef StepperMode
                  steperrun = ! steperrun;
                  shadeStepper.setCurrentPosition(0);
                  #endif
                }
                if (msg == "OFF") {
                  rtmp->mdigitalWrite(rtmp->getRelayPin(),LOW);
                }
              }
            } else {
              AppliedRelayNumber = 0; // revert to relay 0 if relay n is not found 
            }
        } 
         request->send(SPIFFS, "/Config.html", String(), false, processor);
               webing = false;
    }
    );

    AsyncWeb_server.on("/Apply.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/Apply.html");
        // int args = request->args();
          #ifdef USEPREF
          SaveParams(MyConfParam,preferences,request);
          #endif
          saveConfig(MyConfParam, request);
          loadConfig(MyConfParam);
          setupInputs();
          clearIRMap();
          loadIRMapConfig(myIRMap);
    });

    AsyncWeb_server.on("/ResetHomeKit", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/ResetHomeKit.html");
        // int args = request->args();
      #ifdef AppleHK
        #ifndef ESP32
            homekit_storage_reset();   
            Serial.println(F("HomeKit Web Reset, rebooting.."));
            restartRequired = true; // main function will take care of restarting
        #endif    
      #endif

      #ifdef ESP32
        #ifdef AppleHK
              resetHap(); 
        #endif      
      #endif
    });

    AsyncWeb_server.on("/RelayCmdApplied.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/RelayCmdApplied.html");
    });    

    AsyncWeb_server.on("/ApplyRelay.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/ApplyRelay.html");

        if(request->hasParam("RELAYNB")) {
          auto tmp = request->getParam("RELAYNB")->value();
          saveRelayConfig(request);
          Relay * rtmp =  getrelaybynumber(AppliedRelayNumber);
          if (rtmp) {rtmp->loadrelayparams(tmp.toInt());
          //if (rtmp) {rtmp->loadrelayparams(0);

            uint16_t packetIdPub2 = mqttClient.publish( rtmp->RelayConfParam->v_i_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED,
              [](int i){
                char buffer [33];
                itoa (i,buffer,10);
                return buffer;
              }(rtmp->RelayConfParam->v_ttl));

            uint16_t packetIdPub3 = mqttClient.publish( rtmp->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED,
              [](int i){
                char buffer [33];
                itoa (i,buffer,10);
                return buffer;
              }(rtmp->RelayConfParam->v_ttl));
          }
        }

      }
    );


    AsyncWeb_server.on("/Input_Relays_Map", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->authenticate("user", "pass")) return request->requestAuthentication();
        request->send(SPIFFS, "/Input_Relays_Map.html", String(), false, IRMAPprocessor);
      });

    AsyncWeb_server.on("/ApplyIRMap.html", HTTP_GET, [](AsyncWebServerRequest *request){
      if (!request->authenticate("user", "pass")) return request->requestAuthentication();
      request->send(SPIFFS, "/ApplyIRMap.html");
        // int args = request->args();
          saveIRMapConfig(request);
          loadIRMapConfig(myIRMap);
    });


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
            Serial.print(F("\n[SYSTEM  ] firmware upload started (will reboot once done): "));
            Serial.print (filename.c_str());
            Serial.print(F("\n"));
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
                Serial.print(F("Update Success... \nRebooting..."));
                Serial.print(index+len);
              } else {
                Update.printError(Serial);
              }
              // Serial.setDebugOutput(false);
          }
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

    #ifdef _ESP_ALEXA_
    AsyncWeb_server.onNotFound([](AsyncWebServerRequest *request){
      if (!espalexa.handleAlexaApiCall(request)) //if you don't know the URI, ask espalexa whether it is an Alexa control request
      {
        //whatever you want to do with 404s
        request->send(404, "text/plain", "Not found");
      }
    });
    #endif
    #ifdef _ALEXA_
      // These two callbacks are required for gen1 and gen3 compatibility
      AsyncWeb_server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
          if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
          // Handle any other body request here...
      });
      AsyncWeb_server.onNotFound([](AsyncWebServerRequest *request) {
          String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
          if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
          // Handle not found request here...
      });
    #endif

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
	Serial.println(F("[INFO   ] Starting HTTP server"));
  AsyncWeb_server.begin();
}