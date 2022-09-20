#ifndef _INVERTERHELPER_
#define _INVERTERHELPER_

#include <Arduino.h>
#include <defines.h>
#include <MQTT_Processes.h>
#include <uptime_formatter.h>
#include <ArduinoJson.h>
#include <Settings.h>


extern AsyncMqttClient mqttClient;

Settings _settings;

  const byte MPI = 1;
  const byte PCM = 0;
  const byte PIP = 2;
  bool crcCheck = false; // Default CRC is off. 
  byte inverterType = PIP; // MPI; //And defaults in case...
  //String topic = "solar/";  //Default first part of topic. We will add device ID in setup
  //String st = "";
  int Led_Red    = 5;  //D1
  int Led_Green  = 4;  //D2  

  String topic = "solar/";

unsigned long mqtttimer = 0;
extern bool _allMessagesUpdated;
extern bool _otherMessagesUpdated;

extern QpiMessage _qpiMessage ;
extern QpigsMessage _qpigsMessage ;
extern QmodMessage _qmodMessage ;
extern QpiwsMessage _qpiwsMessage ;
extern QflagMessage _qflagMessage ;
extern QidMessage _qidMessage;
extern P003GSMessage _P003GSMessage ;
extern P003PSMessage _P003PSMessage ;
extern P006FPADJMessage _P006FPADJMessage;
extern String  _rawCommand;
extern String _otherBuffer;
extern String _nextCommandNeeded;
extern String _setCommand;
String st = "";

//---------- LEDS  
//int Led_Red = 5;    //D1
//int Led_Green = 4;  //D2

StaticJsonDocument<300> doc;  


int WifiGetRssiAsQuality(int rssi)  // THis part borrowed from Tasmota code
{
  int quality = 0;

  if (rssi <= -100) {
    quality = 0;
  } else if (rssi >= -50) {
    quality = 100;
  } else {
    quality = 2 * (rssi + 100);
  }
  return quality;
}


bool sendtoMQTT() {
    if (millis() < (mqtttimer + 3000)) 
    { 
    return false;
    }
    mqtttimer = millis();
    if (!mqttClient.connected()) {
      digitalWrite(Led_Green, LOW); 
          SERIAL_DBG.println(F("CANT CONNECT TO MQTT"));
          SERIAL_DBG.println(_settings._mqttUser.c_str());
              if (mqttClient.connect()) {
                  SERIAL_DBG.println(F("Reconnected to MQTT SERVER"));
                  mqttClient.publish((topic + String("/Info")).c_str(), ("{\"Status\":\"Im alive!\", \"DeviceType\": \"" + _settings._deviceType + "\",\"IP \":\"" + WiFi.localIP().toString() + "\"}" ).c_str());
                  mqttClient.subscribe((topic + String("/code")).c_str());
                  mqttClient.subscribe((topic + String("/code")).c_str());
                } else {
                  SERIAL_DBG.println(F("CANT CONNECT TO MQTT"));
                  SERIAL_DBG.println(_settings._mqttUser.c_str());
                  digitalWrite(Led_Green, LOW); 
                  return false; // Exit if we couldnt connect to MQTT brooker
                }
    } 
    
    mqttClient.publish((topic + String("/uptime")).c_str(), QOS2, RETAINED,String("{\"human\":\"" + String(uptime_formatter::getUptime()) + "\", \"seconds\":" + String(millis()/1000) + "}").c_str()    );
    mqttClient.publish((topic + String("/wifi")).c_str()  , QOS2, RETAINED,(String("{ \"FreeRam\": ") + String(ESP.getFreeHeap()) + String(", \"rssi\": ") + String(WiFi.RSSI()) + String(", \"dbm\": ") + String(WifiGetRssiAsQuality(WiFi.RSSI())) + String("}")).c_str());  
  
    SERIAL_DBG.print(F("Data sent to MQTT SERver"));
    SERIAL_DBG.print(F(" - up: "));
    SERIAL_DBG.println(uptime_formatter::getUptime());
    digitalWrite(Led_Green, HIGH);
    
 
  if (!_allMessagesUpdated) return false;
  
  _allMessagesUpdated = false; // Lets reset messages and process them
  
  if (inverterType == PCM) { //PCM
     mqttClient.publish((String(topic) + String("/battv")).c_str(), QOS2, RETAINED,String(_qpigsMessage.battV).c_str());
     mqttClient.publish((String(topic) + String("/solarv")).c_str(), QOS2, RETAINED,String(_qpigsMessage.solarV).c_str());
     mqttClient.publish((String(topic) + String("/batta")).c_str(), QOS2, RETAINED,String(_qpigsMessage.battChargeA).c_str());
     mqttClient.publish((String(topic) + String("/wattage")).c_str(), QOS2, RETAINED,String(_qpigsMessage.wattage).c_str());
     mqttClient.publish((String(topic) + String("/solara")).c_str(), QOS2, RETAINED,String(_qpigsMessage.solarA).c_str());

     

    doc.clear();
    doc["battv"] =  _qpigsMessage.battV;
    doc["solarv"] = _qpigsMessage.solarV;
    doc["batta"] =  _qpigsMessage.battChargeA;
    doc["wattage"] =_qpigsMessage.wattage;
    doc["solara"] = _qpigsMessage.solarA;
    st = "";
    serializeJson(doc,st);
    mqttClient.publish((String(topic) + String("/status")).c_str(), QOS2, RETAINED,st.c_str() );

     
  }
   if (inverterType == PIP) { //PIP
   
     mqttClient.publish((String(topic) + String("/GridV")).c_str(), QOS2, RETAINED,String(_qpigsMessage.GridV).c_str());
     mqttClient.publish((String(topic) + String("/GridF")).c_str(), QOS2, RETAINED,String(_qpigsMessage.GridF).c_str());
     mqttClient.publish((String(topic) + String("/AcV")).c_str(), QOS2, RETAINED,String(_qpigsMessage.AcV).c_str());
     mqttClient.publish((String(topic) + String("/AcF")).c_str(), QOS2, RETAINED,String(_qpigsMessage.AcF).c_str());
     mqttClient.publish((String(topic) + String("/AcVA")).c_str(),QOS2, RETAINED, String(_qpigsMessage.AcVA).c_str());
     mqttClient.publish((String(topic) + String("/AcW")).c_str(), QOS2, RETAINED,String(_qpigsMessage.AcW).c_str());
     mqttClient.publish((String(topic) + String("/Load")).c_str(),QOS2, RETAINED, String(_qpigsMessage.Load).c_str());
     //mqttClient.publish((String(topic) + String("/BusV")).c_str(), String(_qpigsMessage.BusV).c_str());
     mqttClient.publish((String(topic) + String("/BattV")).c_str(),QOS2, RETAINED, String(_qpigsMessage.BattV).c_str());
     mqttClient.publish((String(topic) + String("/ChgeA")).c_str(), QOS2, RETAINED,String(_qpigsMessage.ChgeA).c_str());
     mqttClient.publish((String(topic) + String("/BattC")).c_str(),QOS2, RETAINED, String(_qpigsMessage.BattC).c_str());
     mqttClient.publish((String(topic) + String("/Temp")).c_str(), QOS2, RETAINED,String(_qpigsMessage.Temp).c_str());
     mqttClient.publish((String(topic) + String("/PVbattA")).c_str(), QOS2, RETAINED,String(_qpigsMessage.PVbattA).c_str());
     mqttClient.publish((String(topic) + String("/PVV")).c_str(), QOS2, RETAINED,String(_qpigsMessage.PVV).c_str());
     //mqttClient.publish((String(topic) + String("/BattSCC")).c_str(), String(_qpigsMessage.BattSCC).c_str());
     mqttClient.publish((String(topic) + String("/BattDisA")).c_str(),QOS2, RETAINED, String(_qpigsMessage.BattDisA).c_str());
     mqttClient.publish((String(topic) + String("/Stat")).c_str(), QOS2, RETAINED,String(_qpigsMessage.Stat).c_str());
     //mqttClient.publish((String(topic) + String("/BattOffs")).c_str(), String(_qpigsMessage.BattOffs).c_str());
     //mqttClient.publish((String(topic) + String("/Eeprom")).c_str(), String(_qpigsMessage.Eeprom).c_str());
     mqttClient.publish((String(topic) + String("/PVchW")).c_str(), QOS2, RETAINED,String(_qpigsMessage.PVchW).c_str());
     //mqttClient.publish((String(topic) + String("/b10")).c_str(), String(_qpigsMessage.b10).c_str());
     //mqttClient.publish((String(topic) + String("/b9")).c_str(), String(_qpigsMessage.b9).c_str());
     //mqttClient.publish((String(topic) + String("/b8")).c_str(), String(_qpigsMessage.b8).c_str());
     //mqttClient.publish((String(topic) + String("/reserved")).c_str(), String(_qpigsMessage.reserved).c_str());

    doc.clear();
    doc["GridV"] =  _qpigsMessage.GridV;
    doc["GridF"] = _qpigsMessage.GridF;
    doc["AcV"] =  _qpigsMessage.AcV;
    doc["AcF"] =_qpigsMessage.AcF;
    doc["AcVA"] = _qpigsMessage.AcVA;
    doc["AcW"] = _qpigsMessage.AcW;
    doc["Load"] = _qpigsMessage.Load;
    //doc["BusV"] = _qpigsMessage.BusV;
    doc["BattV"] = _qpigsMessage.BattV;
    doc["ChgeA"] = _qpigsMessage.ChgeA;
    doc["BattC"] = _qpigsMessage.BattC;
    doc["Temp"] = _qpigsMessage.Temp;
    doc["PVbattA"] = _qpigsMessage.PVbattA;
    doc["PVV"] = _qpigsMessage.PVV;
   //doc["BattSCC"] = _qpigsMessage.BattSCC;
    doc["BattDisA"] = _qpigsMessage.BattDisA;
    doc["Stat"] = _qpigsMessage.Stat;
    //doc["BattOffs"] = _qpigsMessage.BattOffs;
    //doc["Eeprom"] = _qpigsMessage.Eeprom;
    doc["PVchW"] = _qpigsMessage.PVchW;
    //doc["b10"] = _qpigsMessage.b10;
    //doc["b9"] = _qpigsMessage.b9;
    //doc["b8"] = _qpigsMessage.b8;
    //doc["reserved"] = _qpigsMessage.reserved;
    st = "";
    serializeJson(doc,st);
    mqttClient.publish((String(topic) + String("/status")).c_str(),QOS2, RETAINED, st.c_str() );
  }
  
  if (inverterType == MPI) { //IF MPI
    mqttClient.publish((String(topic) + String("/solar1w")).c_str(),QOS2, RETAINED, String(_P003GSMessage.solarInputV1*_P003GSMessage.solarInputA1).c_str());
    mqttClient.publish((String(topic) + String("/solar2w")).c_str(),QOS2, RETAINED, String(_P003GSMessage.solarInputV2*_P003GSMessage.solarInputA2).c_str());
    mqttClient.publish((String(topic) + String("/solarInputV1")).c_str(), QOS2, RETAINED,String(_P003GSMessage.solarInputV1).c_str());
    mqttClient.publish((String(topic) + String("/solarInputV2")).c_str(),QOS2, RETAINED, String(_P003GSMessage.solarInputV2).c_str());
    mqttClient.publish((String(topic) + String("/solarInputA1")).c_str(), QOS2, RETAINED,String(_P003GSMessage.solarInputA1).c_str());
    mqttClient.publish((String(topic) + String("/solarInputA2")).c_str(), QOS2, RETAINED,String(_P003GSMessage.solarInputA2).c_str()); 
    mqttClient.publish((String(topic) + String("/battV")).c_str(),QOS2, RETAINED, String(_P003GSMessage.battV).c_str());
    mqttClient.publish((String(topic) + String("/battA")).c_str(), QOS2, RETAINED,String(_P003GSMessage.battA).c_str());

    mqttClient.publish((String(topic) + String("/acInputVoltageR")).c_str(),QOS2, RETAINED, String(_P003GSMessage.acInputVoltageR).c_str());   
    mqttClient.publish((String(topic) + String("/acInputVoltageS")).c_str(),QOS2, RETAINED, String(_P003GSMessage.acInputVoltageS).c_str());
    mqttClient.publish((String(topic) + String("/acInputVoltageT")).c_str(),QOS2, RETAINED, String(_P003GSMessage.acInputVoltageT).c_str());

    mqttClient.publish((String(topic) + String("/acInputCurrentR")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acInputCurrentR).c_str());   
    mqttClient.publish((String(topic) + String("/acInputCurrentS")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acInputCurrentS).c_str());
    mqttClient.publish((String(topic) + String("/acInputCurrentT")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acInputCurrentT).c_str());

    mqttClient.publish((String(topic) + String("/acOutputCurrentR")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acOutputCurrentR).c_str());   
    mqttClient.publish((String(topic) + String("/acOutputCurrentS")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acOutputCurrentS).c_str());
    mqttClient.publish((String(topic) + String("/acOutputCurrentT")).c_str(), QOS2, RETAINED,String(_P003GSMessage.acOutputCurrentT).c_str());

    mqttClient.publish((String(topic) + String("/innerTemp")).c_str(), QOS2, RETAINED,String(_P003GSMessage.innerTemp).c_str());
    mqttClient.publish((String(topic) + String("/componentTemp")).c_str(), QOS2, RETAINED,String(_P003GSMessage.componentTemp).c_str());

    mqttClient.publish((String(topic) + String("/acWattageR")).c_str(), QOS2, RETAINED,String(_P003PSMessage.w_r).c_str());
    mqttClient.publish((String(topic) + String("/acWattageS")).c_str(), QOS2, RETAINED,String(_P003PSMessage.w_s).c_str());
    mqttClient.publish((String(topic) + String("/acWattageT")).c_str(), QOS2, RETAINED,String(_P003PSMessage.w_t).c_str());
    mqttClient.publish((String(topic) + String("/acWattageTotal")).c_str(), QOS2, RETAINED,String(_P003PSMessage.w_total).c_str());
    mqttClient.publish((String(topic) + String("/ac_output_procent")).c_str(), QOS2, RETAINED,String(_P003PSMessage.ac_output_procent).c_str());

    mqttClient.publish((String(topic) + String("/feedingGridDirectionR")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.feedingGridDirectionR).c_str());
    mqttClient.publish((String(topic) + String("/calibrationWattR")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.calibrationWattR).c_str());
    mqttClient.publish((String(topic) + String("/feedingGridDirectionS")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.feedingGridDirectionS).c_str());
    mqttClient.publish((String(topic) + String("/calibrationWattS")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.calibrationWattS).c_str());
    mqttClient.publish((String(topic) + String("/feedingGridDirectionT")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.feedingGridDirectionT).c_str());
    mqttClient.publish((String(topic) + String("/calibrationWattT")).c_str(), QOS2, RETAINED,String(_P006FPADJMessage.calibrationWattT).c_str());
    
    doc.clear();
    doc["solar1w"] =  _P003GSMessage.solarInputV1*_P003GSMessage.solarInputA1;
    doc["solar2w"] =  _P003GSMessage.solarInputV2*_P003GSMessage.solarInputA2;
    doc["solarInputV1"] =    _P003GSMessage.solarInputV1;
    doc["solarInputV2"] =    _P003GSMessage.solarInputV2;
    doc["solarInputA1"] =    _P003GSMessage.solarInputA1;
    doc["solarInputA2"] =    _P003GSMessage.solarInputA2;
    doc["battV"] = _P003GSMessage.battV;
    doc["battA"] = _P003GSMessage.battA;
    doc["acInputVoltageR"] = _P003GSMessage.acInputVoltageR;
    doc["acInputVoltageS"] = _P003GSMessage.acInputVoltageS;
    doc["acInputVoltageT"] = _P003GSMessage.acInputVoltageT;
    doc["acInputCurrentR"] = _P003GSMessage.acInputCurrentR;
    doc["acInputCurrentS"] = _P003GSMessage.acInputCurrentS;
    doc["acInputCurrentT"] = _P003GSMessage.acInputCurrentT;
    doc["acOutputCurrentR"] = _P003GSMessage.acOutputCurrentR;
    doc["acOutputCurrentS"] = _P003GSMessage.acOutputCurrentS;
    doc["acOutputCurrentT"] = _P003GSMessage.acOutputCurrentT;
    doc["acWattageR"] = _P003PSMessage.w_r;
    doc["acWattageS"] = _P003PSMessage.w_s;
    doc["acWattageT"] = _P003PSMessage.w_t;
    doc["acWattageTotal"] = _P003PSMessage.w_total;
    doc["acOutputProcentage"] = _P003PSMessage.ac_output_procent;
    doc["feedingGridDirectionR"] = _P006FPADJMessage.feedingGridDirectionR;
    doc["feedingGridDirectionS"] = _P006FPADJMessage.feedingGridDirectionS;
    doc["feedingGridDirectionT"] = _P006FPADJMessage.feedingGridDirectionT;
    doc["calibrationWattR"] = _P006FPADJMessage.calibrationWattR;
    doc["calibrationWattS"] = _P006FPADJMessage.calibrationWattS;
    doc["calibrationWattT"] = _P006FPADJMessage.calibrationWattT;
    doc["innerTemp"] = _P003GSMessage.innerTemp;
    doc["componentTemp"] = _P003GSMessage.componentTemp;
    st = "";
    serializeJson(doc,st);
    mqttClient.publish((String(topic) + String("/status")).c_str(), QOS2, RETAINED, st.c_str() );
  }

  return true;
}


// Check if we have pending raw messages to send to MQTT. Then send it. 
void sendRaw() {
  if (_otherMessagesUpdated) {
    _otherMessagesUpdated = false;
    SERIAL_DBG.print(F("MQTT RAW SEND: Command sent: "));
    SERIAL_DBG.print(_rawCommand);
    SERIAL_DBG.print(" Raw data recieved: ");
    SERIAL_DBG.println(_otherBuffer);
    mqttClient.publish((String(topic) + String("/debug/recieved")).c_str(), QOS2, RETAINED,String(_otherBuffer).c_str() );
    
    doc.clear();
    doc["sentMessage"] = _rawCommand;
    doc["recievedMessage"] = _otherBuffer;
    st = "";
    serializeJson(doc,st);
    mqttClient.publish((String(topic) + String("/debug/json")).c_str(), QOS2, RETAINED,st.c_str() );
    _rawCommand = "";
  }
}

// TESTING MQTT SEND
void callback(char* top, byte* payload, unsigned int length) {
  SERIAL_DBG.println(F("Callback done"));
  if (strcmp(top,"pir1Status")==0){
    // whatever you want for this topic
  }
 
  String st ="";
  for (int i = 0; i < length; i++) {
    st += String((char)payload[i]);
  }
  st.trim(); // Remove any whitespaces!!

  
  mqttClient.publish((String(topic) + String("/debug/sent")).c_str(), QOS2, RETAINED,String(st).c_str() );
  SERIAL_DBG.print(F("Current command: "));
  SERIAL_DBG.print(_nextCommandNeeded);
  SERIAL_DBG.print(F(" Setting next command to : "));
  SERIAL_DBG.println(st);
  _setCommand = st;
  _rawCommand = st;
  // Add code to put the call into the queue but verify it firstly. Then send the result back to debug/new window?
}


#endif