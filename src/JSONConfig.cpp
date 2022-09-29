#include <JSONConfig.h>
#include <RelayClass.h>

#ifdef INVERTERLINK
  #include <Settings.h>
  extern Settings _settings;

#endif

const char* filename      = "/config.json";
const char* IRMapfilename = "/IRMAP.json";
#ifndef ESP32
#define buffer_size_2  1800 // 2048
#endif
#ifdef ESP32
#define buffer_size_2  2400
#endif
#define buffer_size_IR  1000

extern void applyIRMap(int8_t Inpn, int8_t rlyn);


config_read_error_t loadConfig(TConfigParams &ConfParam) {
Serial.println(F("[INFO   ] loading SPIFFS"));
  if(SPIFFS.begin())
  {
    Serial.println(F("[INFO   ] SPIFFS Initialize....ok"));
  }
  else
  {
    Serial.println(F("[INFO   ] SPIFFS Initialization...failed"));
  }

  Serial.println(F("[INFO   ] Opening config.json"));
  if (! SPIFFS.exists(filename)) {
    Serial.println(F("[INFO   ] config file does not exist! ... building and rebooting...."));
    saveDefaultConfig();
    return FILE_NOT_FOUND;
  }

  Serial.println(F("[INFO   ] Starting config.json parsing"));
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to open config file"));
    saveDefaultConfig();
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<buffer_size_2> json;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("[INFO   ] Failed to read file, using default configuration"));
    saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;    
  }

  ConfParam.v_ssid                = (json["ssid"].as<String>()!="") ? json["ssid"].as<String>() : String(F("ksba"));
  ConfParam.v_pass                = (json["pass"].as<String>()!="") ? json["pass"].as<String>() : String(F("samsam12"));

  ConfParam.v_mqttUser            = (json["mqttUser"].as<String>()!="") ? json["mqttUser"].as<String>() : String(F(""));
  ConfParam.v_mqttPass            = (json["mqttPass"].as<String>()!="") ? json["mqttPass"].as<String>() : String(F(""));

  ConfParam.v_PhyLoc              = (json["PhyLoc"].as<String>()!="") ? json["PhyLoc"].as<String>() : String(F("Not configured yet"));
  ConfParam.v_MQTT_BROKER         = (json["MQTT_BROKER"].as<String>()!="") ? json["MQTT_BROKER"].as<String>() : String(F("192.168.1.1"));

  if (json["timeserver"].as<String>()!="") {
      ConfParam.v_timeserver.fromString(json["timeserver"].as<String>());} else
      { ConfParam.v_timeserver.fromString("192.168.50.1");}

  if (json["Pingserver"].as<String>()!="") {
      ConfParam.v_Pingserver.fromString(json["Pingserver"].as<String>());} else
      { ConfParam.v_Pingserver.fromString("192.168.50.1");}      

      

  ConfParam.v_MQTT_Active         = (json["MQTT_Active"].as<String>()!="") ? json["MQTT_Active"].as<uint8_t>() ==1 : false;
  ConfParam.v_ntptz               = (json["ntptz"].as<String>()!="") ? json["ntptz"].as<signed char>() : 2;

  /*if (json["MQTT_BROKER"].as<String>()!="") {
      ConfParam.v_MQTT_BROKER.fromString(json["MQTT_BROKER"].as<String>());} else
      { ConfParam.v_MQTT_BROKER.fromString("192.168.1.1");}
      */

  ConfParam.v_MQTT_B_PRT          = (json["MQTT_B_PRT"].as<String>()!="") ? json["MQTT_B_PRT"].as<uint16_t>() : 1883;
  ConfParam.v_Update_now          = (json["Update_now"].as<String>()!="") ? json["Update_now"].as<uint8_t>() == 1 : false;
  ConfParam.v_TOGGLE_BTN_PUB_TOPIC= (json["TOGGLE_BTN_PUB_TOPIC"].as<String>()!="") ? json["TOGGLE_BTN_PUB_TOPIC"].as<String>() : String(F("/none"));

  ConfParam.v_IN0_INPUTMODE       =  json["I0MODE"].as<uint8_t>();
  ConfParam.v_IN1_INPUTMODE       =  json["I1MODE"].as<uint8_t>();
  ConfParam.v_IN2_INPUTMODE       =  json["I2MODE"].as<uint8_t>();
  ConfParam.v_IN3_INPUTMODE       =  json["I3MODE"].as<uint8_t>();
  ConfParam.v_IN4_INPUTMODE       =  json["I4MODE"].as<uint8_t>();
  ConfParam.v_IN5_INPUTMODE       =  json["I5MODE"].as<uint8_t>();
  ConfParam.v_IN6_INPUTMODE       =  json["I6MODE"].as<uint8_t>();

  //#ifndef HWESP32
    ConfParam.v_InputPin12_STATE_PUB_TOPIC = (json["I12_STS_PTP"].as<String>()!="") ? json["I12_STS_PTP"].as<String>() : String(F("/none"));
    ConfParam.v_InputPin14_STATE_PUB_TOPIC = (json["I14_STS_PTP"].as<String>()!="") ? json["I14_STS_PTP"].as<String>() : String(F("/none"));
  //#endif  
  #ifdef HWESP32
  //fixit
    ConfParam.v_InputPin01_STATE_PUB_TOPIC = ConfParam.v_InputPin12_STATE_PUB_TOPIC;
    ConfParam.v_InputPin02_STATE_PUB_TOPIC = ConfParam.v_InputPin14_STATE_PUB_TOPIC; 
  #endif

   ConfParam.v_InputPin03_STATE_PUB_TOPIC = (json["I03_STS_PTP"].as<String>()!="") ? json["I03_STS_PTP"].as<String>() : String(F("/none"));
   ConfParam.v_InputPin04_STATE_PUB_TOPIC = (json["I04_STS_PTP"].as<String>()!="") ? json["I04_STS_PTP"].as<String>() : String(F("/none"));
   ConfParam.v_InputPin05_STATE_PUB_TOPIC = (json["I05_STS_PTP"].as<String>()!="") ? json["I05_STS_PTP"].as<String>() : String(F("/none"));
   ConfParam.v_InputPin06_STATE_PUB_TOPIC = (json["I06_STS_PTP"].as<String>()!="") ? json["I06_STS_PTP"].as<String>() : String(F("/none"));     

  //ConfParam.v_FRM_IP              = (json["FRM_IP"].as<String>()!="") ? json["FRM_IP"].as<String>() : String(F("192.168.1.1"));

  if (json["FRM_IP"].as<String>()!="") {
      ConfParam.v_FRM_IP.fromString(json["FRM_IP"].as<String>());} else
      { ConfParam.v_FRM_IP.fromString("192.168.1.1");}


  ConfParam.v_FRM_PRT             = (json["FRM_PRT"].as<String>()!="") ? json["FRM_PRT"].as<uint16_t>() : 83;
  ConfParam.v_Sonar_distance      = (json["Sonar_distance"].as<String>()!="") ? json["Sonar_distance"].as<String>() : String(F("0"));
  ConfParam.v_Sonar_distance_max  =  json["Sonar_distance_max"].as<uint16_t>();
  ConfParam.v_pass                = (json["pass"].as<String>()!="") ? json["pass"].as<String>() : String(F("samsam12"));  
  ConfParam.v_Reboot_on_WIFI_Disconnection  =  json["RWD"].as<uint16_t>();  
  ConfParam.v_CurrentTransformer_max_current = (json["CurrentTransformer_max_current"].as<String>()!="") ? json["CurrentTransformer_max_current"].as<uint8_t>() : 0;
  ConfParam.v_calibration                    = (json["calibration"].as<String>()!="") ? json["calibration"].as<uint16_t>() : 1908;
  ConfParam.v_CurrentTransformerTopic        = (json["CurrentTransformerTopic"].as<String>()!="") ? json["CurrentTransformerTopic"].as<String>() : String(F(""));  
  ConfParam.v_ToleranceOffTime               = (json["ToleranceOffTime"].as<String>()!="") ? json["ToleranceOffTime"].as<uint16_t>() : 10;
  ConfParam.v_ToleranceOnTime                = (json["ToleranceOnTime"].as<String>()!="") ? json["ToleranceOnTime"].as<uint16_t>() : 30;  
  ConfParam.v_CT_MaxAllowed_current          = (json["CT_MaxAllowed_current"].as<String>()!="") ? json["CT_MaxAllowed_current"].as<uint16_t>() : 30;  
  ConfParam.v_CT_adjustment                  = (json["CT_adjustment"].as<String>()!="") ? json["CT_adjustment"].as<float_t>() : 10;
  configFile.close();

  #ifdef INVERTERLINK
            _settings._wifiSsid = ConfParam.v_ssid;
            _settings._wifiPass = ConfParam.v_pass;
            _settings._deviceType = "PIP";
            _settings._deviceName = ConfParam.v_PhyLoc;
            _settings._mqttServer = ConfParam.v_MQTT_BROKER;
            _settings._mqttUser = ConfParam.v_mqttUser;
            _settings._mqttPassword = ConfParam.v_mqttPass;
            _settings._mqttPort = ConfParam.v_MQTT_B_PRT  ;       
  #endif
  //json = NILL;
  return SUCCESS;
}


bool saveConfig(TConfigParams &ConfParam){

  StaticJsonDocument<buffer_size_2> json;

    File configFile = SPIFFS.open(filename, "w");
    if (!configFile) {
      Serial.println(F("[INFO   ] Failed to open config file for writing"));
      return FAILURE;
    }
    
    json[F("ssid")]= ConfParam.v_ssid ;
    json[F("pass")]=ConfParam.v_pass;

    json[F("mqttUser")]= ConfParam.v_mqttUser ;
    json[F("mqttPass")]=ConfParam.v_mqttPass;

    json[F("PhyLoc")]=ConfParam.v_PhyLoc;
    json[F("MQTT_BROKER")]=ConfParam.v_MQTT_BROKER; // .toString();
    json[F("MQTT_B_PRT")]=ConfParam.v_MQTT_B_PRT;
    json[F("FRM_IP")]= MyConfParam.v_FRM_IP.toString();
    json[F("FRM_PRT")]=ConfParam.v_FRM_PRT;
    #ifndef HWESP32
      json[F("I12_STS_PTP")]=ConfParam.v_InputPin12_STATE_PUB_TOPIC;
      json[F("I14_STS_PTP")]=ConfParam.v_InputPin14_STATE_PUB_TOPIC;
      json[F("I0MODE")]=ConfParam.v_IN0_INPUTMODE;
      json[F("I1MODE")]=ConfParam.v_IN1_INPUTMODE;
      json[F("I2MODE")]=ConfParam.v_IN2_INPUTMODE;    
    #endif
     #ifdef HWESP32 // fixit
      json[F("I12_STS_PTP")]=ConfParam.v_InputPin01_STATE_PUB_TOPIC;
      json[F("I14_STS_PTP")]=ConfParam.v_InputPin02_STATE_PUB_TOPIC;

      json[F("I01_STS_PTP")]=ConfParam.v_InputPin01_STATE_PUB_TOPIC;
      json[F("I02_STS_PTP")]=ConfParam.v_InputPin02_STATE_PUB_TOPIC;
     #endif   
      json[F("I03_STS_PTP")]=ConfParam.v_InputPin03_STATE_PUB_TOPIC;
      json[F("I04_STS_PTP")]=ConfParam.v_InputPin04_STATE_PUB_TOPIC;
      json[F("I05_STS_PTP")]=ConfParam.v_InputPin05_STATE_PUB_TOPIC;
      json[F("I06_STS_PTP")]=ConfParam.v_InputPin06_STATE_PUB_TOPIC;

      /*json[F("I0MODE")]=ConfParam.v_IN0_INPUTMODE;
      json[F("I1MODE")]=ConfParam.v_IN1_INPUTMODE;
      json[F("I2MODE")]=ConfParam.v_IN2_INPUTMODE;   */
      
      json[F("I3MODE")]=ConfParam.v_IN3_INPUTMODE;
      json[F("I4MODE")]=ConfParam.v_IN4_INPUTMODE;
      json[F("I5MODE")]=ConfParam.v_IN5_INPUTMODE;   
      json[F("I6MODE")]=ConfParam.v_IN6_INPUTMODE;             
    /* #endif    */

      json[F("I03_STS_PTP")]=ConfParam.v_InputPin03_STATE_PUB_TOPIC;
      json[F("I04_STS_PTP")]=ConfParam.v_InputPin04_STATE_PUB_TOPIC;
      json[F("I05_STS_PTP")]=ConfParam.v_InputPin05_STATE_PUB_TOPIC;
      json[F("I06_STS_PTP")]=ConfParam.v_InputPin06_STATE_PUB_TOPIC;
      json[F("I3MODE")]=ConfParam.v_IN3_INPUTMODE;
      json[F("I4MODE")]=ConfParam.v_IN4_INPUTMODE;
      json[F("I5MODE")]=ConfParam.v_IN5_INPUTMODE;   
      json[F("I6MODE")]=ConfParam.v_IN6_INPUTMODE;        

    json[F("TOGGLE_BTN_PUB_TOPIC")]=ConfParam.v_TOGGLE_BTN_PUB_TOPIC;
    json[F("timeserver")]=ConfParam.v_timeserver.toString(); 
    json[F("Pingserver")]=ConfParam.v_Pingserver.toString();     
    json[F("MQTT_Active")]=ConfParam.v_MQTT_Active;
    json[F("ntptz")]=ConfParam.v_ntptz;
    json[F("Update_now")]=ConfParam.v_Update_now;
    json[F("Sonar_distance")]=ConfParam.v_Sonar_distance;
    json[F("Sonar_distance_max")]=ConfParam.v_Sonar_distance_max; 
    json[F("RWD")]=ConfParam.v_Reboot_on_WIFI_Disconnection;    

    json[F("CurrentTransformer_max_current")] = ConfParam.v_CurrentTransformer_max_current;
    json[F("calibration")]                    = ConfParam.v_calibration; 
    json[F("CurrentTransformerTopic")]        = ConfParam.v_CurrentTransformerTopic;        
    json[F("ToleranceOnTime")]                = ConfParam.v_ToleranceOnTime; 
    json[F("ToleranceOffTime")]               = ConfParam.v_ToleranceOffTime;        
    json[F("CT_adjustment")]                  = ConfParam.v_CT_adjustment;            
    json[F("CT_MaxAllowed_current")]          = ConfParam.v_CT_MaxAllowed_current;

    if (serializeJsonPretty(json, configFile) == 0) {
      Serial.println(F("[INFO   ] Failed to write to file"));
      configFile.close();
      return false;
    }
  configFile.println("\n\n");
  configFile.close();

  return true;
}


bool saveConfig(TConfigParams &ConfParam, AsyncWebServerRequest *request){

  StaticJsonDocument<buffer_size_2> doc;
  // StaticJsonDocument<64> dummy;

    int args = request->args();
    for(int i=0;i<args;i++){
      if (request->arg(i) != NULL) {
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      doc[request->argName(i)] =  request->arg(i);
      }
    }

  (request->hasParam("PIC_Active"))  ? doc["PIC_Active"]  = 1 : doc["PIC_Active"]  = 0; 
  (request->hasParam("MQTT_Active")) ? doc["MQTT_Active"] = 1 : doc["MQTT_Active"] = 0;
  (request->hasParam("ACS_Active"))  ? doc["ACS_Active"]  = 1 : doc["ACS_Active"]  = 0;
  (request->hasParam("Update_now"))  ? doc["Update_now"]  = 1 : doc["Update_now"]  = 0;    

  // Serialize JSON to file
  SPIFFS.remove("/config.json");
  File configFile = SPIFFS.open(filename, "w");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to open config file for writing"));
    return false;
  }    


  if (serializeJsonPretty(doc, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;
  }
  configFile.println("\n\n");
  configFile.close();

  return true;
}



bool saveDefaultConfig(){
  StaticJsonDocument<buffer_size_2> json;

  Serial.print("\n writing default parameters");
  json[F("ssid")]="ksba";
  json[F("pass")]="samsam12";

  json[F("mqttUser")]="";
  json[F("mqttPass")]="";

  json[F("PhyLoc")]="Not configured yet";
  json[F("timeserver")]="194.97.156.5";
  json[F("ntptz")]=2;
  json[F("PIC_Active")]=0;
  json[F("MQTT_Active")]=0;
  json[F("MQTT_BROKER")]="192.168.1.1";
  json[F("MQTT_B_PRT")]=1883;

  json[F("TOGGLE_BTN_PUB_TOPIC")]="/home/Controller" + CID() + "/INS/sts/IN0" ;
  
  #ifndef HWESP32
  json[F("I12_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN1";
  json[F("I14_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN2";
  json[F("I0MODE")]=2;
  json[F("I1MODE")]=2;
  json[F("I2MODE")]=2;  
  #endif

 
    json[F("I12_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN1";
    json[F("I14_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN2";  
    json[F("I01_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN1";
    json[F("I02_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN2";
    json[F("I03_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN3";
    json[F("I04_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN4";  
    json[F("I05_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN5";
    json[F("I06_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN6";  
    json[F("I0MODE")]=2;
    json[F("I1MODE")]=2;
    json[F("I2MODE")]=2;  
    json[F("I3MODE")]=2;
    json[F("I4MODE")]=2;
    json[F("I5MODE")]=2;      
    json[F("I6MODE")]=2;      


  json[F("Sonar_distance")]=0;
  json[F("Sonar_distance_max")]=50; 

  json[F("FRM_IP")]="192.168.1.1";
  json[F("FRM_PRT")]=83;
  json[F("Update_now")]=0;

  json[F("CurrentTransformer_max_current")] = 50;
  json[F("calibration")]                    = 1906; 
  json[F("ToleranceOffTime")]               = 10; 
  json[F("ToleranceOnTime")]                = 30;
  json[F("CT_MaxAllowed_current")]          = 30;      
  json[F("CurrentTransformerTopic")]        = "/home/Controller" + CID() + "/CT";   
  json[F("CT_adjustment")]                  = 10;        

  //SPIFFS.remove("/config.json");


  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to write default config file"));
    return false;
  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;    
  }
  configFile.println("\n\n");
  configFile.close();

  Serial.println (F("[INFO   ] Printing out default settings"));
  File file2 = SPIFFS.open("/config.json","r");
  if(!file2){
      Serial.println("Failed to open file for reading");
      return false;
  }  

  while(file2.available()){
      Serial.write(file2.read());
  }

//  delay(2000);
//  ESP.restart();

  return true;
}



bool saveDefaultIRMapConfig(){
  StaticJsonDocument<buffer_size_IR> json;

  json["I1"]="-1";
  json["R1"]="-1";
  json["I2"]="-1";
  json["R2"]="-1";
  json["I3"]="-1";
  json["R3"]="-1";
  json["I4"]="-1";
  json["R4"]="-1";
  json["I5"]="-1";
  json["R5"]="-1";
  json["I6"]="-1";
  json["R6"]="-1";
  json["I7"]="-1";
  json["R7"]="-1";
  json["I8"]="-1";
  json["R8"]="-1";
  json["I9"]="-1";
  json["R9"]="-1";
  json["I10"]="-1";
  json["R10"]="-1";

  #ifdef ESP32

  #else
      ESP.wdtFeed();
  #endif    

  File configFile = SPIFFS.open(IRMapfilename, "w");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to write default config file"));
    return FAILURE;
  }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return FAILURE;    
  }
    configFile.println("\n\n");
    configFile.close();

  return SUCCESS;
}


bool saveIRMapConfig(AsyncWebServerRequest *request){

  StaticJsonDocument<buffer_size_IR> json;

  // Set the values in the document

    int args = request->args();

    for(int i=0;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }


    SPIFFS.remove(IRMapfilename);
    File configFile = SPIFFS.open(IRMapfilename, "w");
    if (!configFile) {
      Serial.println(F("[INFO   ] Failed to open config file for writing"));
      return false;
    }

  // Serialize JSON to file
  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;    
  }
  configFile.println("\n\n");
  configFile.close();
  return true;
}


config_read_error_t loadIRMapConfig(TIRMap &IRMap) {
    if(SPIFFS.begin())
    { 
       Serial.println(F("[INFO   ] SPIFFS Initialize....ok"));
    }  else {
      Serial.println(F("[INFO   ] SPIFFS Initialization...failed"));
    }

    if (! SPIFFS.exists(IRMapfilename)) {
      Serial.println(F("[INFO   ] IRMAP config file does not exist! ... building and rebooting...."));
      while (!saveDefaultIRMapConfig()){
      #ifndef ESP32
        ESP.wdtFeed();
      #endif  
      };
      return FILE_NOT_FOUND;
    }

    File configFile = SPIFFS.open(IRMapfilename, "r");
    if (!configFile) {
      Serial.println(F("[INFO   ] Failed to open IRMAP config file"));
      saveDefaultIRMapConfig();
      return ERROR_OPENING_FILE;
    }


  StaticJsonDocument<buffer_size_IR> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
    Serial.println(F("[INFO   ] Failed to read file, using default configuration"));

    myIRMap.I1   =  json["I1"].as<int8_t>();
    myIRMap.I2   =  json["I2"].as<int8_t>();
    myIRMap.I3   =  json["I3"].as<int8_t>();
    myIRMap.I4   =  json["I4"].as<int8_t>();
    myIRMap.I5   =  json["I5"].as<int8_t>();
    myIRMap.I6   =  json["I6"].as<int8_t>();
    myIRMap.I7   =  json["I7"].as<int8_t>();
    myIRMap.I8   =  json["I8"].as<int8_t>();
    myIRMap.I9   =  json["I9"].as<int8_t>();
    myIRMap.I10  =  json["I10"].as<int8_t>();

    myIRMap.R1   =  json["R1"].as<int8_t>();
    myIRMap.R2   =  json["R2"].as<int8_t>();
    myIRMap.R3   =  json["R3"].as<int8_t>();
    myIRMap.R4   =  json["R4"].as<int8_t>();
    myIRMap.R5   =  json["R5"].as<int8_t>();
    myIRMap.R6   =  json["R6"].as<int8_t>();
    myIRMap.R7   =  json["R7"].as<int8_t>();
    myIRMap.R8   =  json["R8"].as<int8_t>();
    myIRMap.R9   =  json["R9"].as<int8_t>();
    myIRMap.R10  =  json["R10"].as<int8_t>();

    applyIRMap(myIRMap.I1 , myIRMap.R1);
    applyIRMap(myIRMap.I2 , myIRMap.R2);
    applyIRMap(myIRMap.I3 , myIRMap.R3);
    applyIRMap(myIRMap.I4 , myIRMap.R4);
    applyIRMap(myIRMap.I5 , myIRMap.R5);
    applyIRMap(myIRMap.I6 , myIRMap.R6);
    applyIRMap(myIRMap.I7 , myIRMap.R7);
    applyIRMap(myIRMap.I8 , myIRMap.R8);
    applyIRMap(myIRMap.I9 , myIRMap.R9);
    applyIRMap(myIRMap.I10 , myIRMap.R10);

    configFile.close();

    return SUCCESS;
}



bool saveRelayDefaultConfig(uint8_t rnb){
  StaticJsonDocument<buffer_size_2> json;

   Serial.print(F("\n[INFO   ] Initializing default relay parameters"));

 
    json[F("RELAYNB")]=rnb;
    json[F("PhyLoc")]="NA";
    json[F("PUB_TOPIC1")]= "/home/Controller" + CID() + "/Coils/C" + String(rnb) ; //1";
    json[F("STATE_PUB_TOPIC")]="/home/Controller" + CID() + "/Coils/State/C" + String(rnb) ; //1";

    json[F("TemperatureValue")]="0";
    json[F("ACS_Sensor_Model")] = "30";
    json[F("ttl")] = "0";
    json[F("ttl_PUB_TOPIC")]="/home/Controller" + CID() + "/sts/VTTL" + String(rnb) ; //1";;
    json[F("i_ttl_PUB_TOPIC")]="/home/Controller" + CID() + "/i/TTL" + String(rnb) ; //1";;
    json[F("ACS_AMPS")]="/home/Controller" + CID() + "/Coils/C1/Amps" + String(rnb) ; //1";;
    json[F("CURR_TTL_PUB_TOPIC")]="/home/Controller" + CID() + "/sts/CURRVTTL" + String(rnb) ; //1";;

  //  json[F("I0MODE")]="0";
  //  json[F("I1MODE")]="0";
  //  json[F("I2MODE")]="0";

    json[F("tta")]="0";
    json[F("ACS_elasticity")]="0";    
    json[F("Max_Current")]="10";
    json[F("LWILL_TOPIC")]="/home/Controller" + CID() + "/LWT" + String(rnb) ; //1";;
    json[F("SUB_TOPIC1")]="/home/Controller" + CID() +  "/#";
    json[F("ACS_Active")]="0";


    
    char  relayfilename[20];
    mkRelayConfigName(relayfilename, json[F("RELAYNB")]);

    SPIFFS.remove(relayfilename);
    File configFile = SPIFFS.open(relayfilename, "w");
    if (!configFile) {
      Serial.println(F("\n[INFO   ] Failed to write relay config file"));
      return false;
    }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;      
  }
  configFile.println("\n\n");
  configFile.close();

  return true;

}


bool saveRelayConfig(AsyncWebServerRequest *request){
  StaticJsonDocument<buffer_size_2> json;

    json["ACS_Active"]    =  0 ;

    int args = request->args();
    for(int i=2;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }

    if(request->hasParam("ACS_Active")) json["ACS_Active"]  =  1;

    char  relayfilename[20];
    mkRelayConfigName(relayfilename, json["RELAYNB"]);

    SPIFFS.remove(relayfilename);
    File configFile = SPIFFS.open(relayfilename, "w");
    if (!configFile) {
      Serial.println(F("\n[INFO   ] Failed to write relay config file"));
      return false;
    }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;    
  }
  configFile.println("\n\n");
  configFile.close();

  return true;

}




bool saveRelayConfig(Trelayconf * RConfParam){
  StaticJsonDocument<buffer_size_2> json;

    json[F("RELAYNB")]=RConfParam->v_relaynb;
    json[F("PhyLoc")]=RConfParam->v_PhyLoc;
    json[F("PUB_TOPIC1")]=RConfParam->v_PUB_TOPIC1;
    json[F("TemperatureValue")]=RConfParam->v_TemperatureValue;
    json[F("ACS_Sensor_Model")] = RConfParam->v_ACS_Sensor_Model;
    json[F("ttl")]=RConfParam->v_ttl;
    json[F("ttl_PUB_TOPIC")]=RConfParam->v_ttl_PUB_TOPIC;
    json[F("i_ttl_PUB_TOPIC")]=RConfParam->v_i_ttl_PUB_TOPIC;
    json[F("ACS_AMPS")]=RConfParam->v_ACS_AMPS;
    json[F("CURR_TTL_PUB_TOPIC")]=RConfParam->v_CURR_TTL_PUB_TOPIC;
    json[F("STATE_PUB_TOPIC")]=RConfParam->v_STATE_PUB_TOPIC;
    json[F("I0MODE")]=RConfParam->v_IN0_INPUTMODE;
    json[F("I1MODE")]=RConfParam->v_IN1_INPUTMODE;
    json[F("I2MODE")]=RConfParam->v_IN2_INPUTMODE;
    json[F("tta")]=RConfParam->v_tta; 
    json[F("ACS_elasticity")]=RConfParam->v_ACS_elasticity;     
    json[F("Max_Current")]=RConfParam->v_Max_Current;
    json[F("LWILL_TOPIC")]=RConfParam->v_LWILL_TOPIC;
    json[F("SUB_TOPIC1")]=RConfParam->v_SUB_TOPIC1;
    json[F("ACS_Active")]=RConfParam->v_ACS_Active;

    char  relayfilename[20];
    mkRelayConfigName(relayfilename, RConfParam->v_relaynb);

    SPIFFS.remove(relayfilename);
    File configFile = SPIFFS.open(relayfilename, "w");
    if (!configFile) {
      Serial.println(F("\n[INFO   ] Failed to write relay config file"));
      return false;
    }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write to file"));
    configFile.close();
    return false;        
  }

  configFile.println("\n\n");
  /*StaticJsonDocument<64> dummy;
  dummy["end"]="0";
  if (serializeJsonPretty(dummy, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }*/
  configFile.close();

  return true;
  }



bool saveCTReadings(float KWh,  float MTD_KWh, float YTD_KWh){
  StaticJsonDocument<200> json;

    json[F("KWh")]=String(KWh);
    json[F("MTD_KWh")]=String(MTD_KWh);
    json[F("YTD_KWh")]=String(YTD_KWh);

    // SPIFFS.remove("/AccumulatedPower.json");
    File configFile = SPIFFS.open("/AccumulatedPower.json", "w");
    if (!configFile) {
      Serial.println(F("\n[INFO   ] Failed to write relay Power file"));
      return false;
    }

  if (serializeJsonPretty(json, configFile) == 0) {
    Serial.println(F("[INFO   ] Failed to write Power values to file"));
    configFile.close();
    return false;        
  }

  // configFile.println("\n\n");
  /*StaticJsonDocument<64> dummy;
  dummy["end"]="0";
  if (serializeJsonPretty(dummy, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }*/
  configFile.close();

  return true;
  }




bool loadCTReadings(float &KWh,  float &MTD_KWh, float &YTD_KWh) {
Serial.println(F("[INFO   ] loading SPIFFS"));
  if(SPIFFS.begin())
  {
    Serial.println(F("[INFO   ] SPIFFS Initialize....ok"));
  }
  else
  {
    Serial.println(F("[INFO   ] SPIFFS Initialization...failed"));
  }

  Serial.println(F("[INFO   ] Opening config.json"));
  if (! SPIFFS.exists("/AccumulatedPower.json")) {
    Serial.println(F("[INFO   ] AccumulatedPower file does not exist!"));
    KWh      =  0;
    MTD_KWh  =  0;
    YTD_KWh  =  0;
    return false;
  }

  Serial.println(F("[INFO   ] Starting config.json parsing"));
  File configFile = SPIFFS.open("/AccumulatedPower.json", "r");
  if (!configFile) {
    Serial.println(F("[INFO   ] Failed to open AccumulatedPower.json file"));
    KWh      =  0;
    MTD_KWh  =  0;
    YTD_KWh  =  0;
    return false;
  }

  StaticJsonDocument<200> json;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("[INFO   ] Failed to read AccumulatedPower.json file, using default configuration"));
    KWh      =  0;
    MTD_KWh  =  0;
    YTD_KWh  =  0;
    return false;    
  }

  KWh      = (json["KWh"].as<String>()!="")     ? json["KWh"].as<float>() : 0;
  MTD_KWh  = (json["MTD_KWh"].as<String>()!="") ? json["MTD_KWh"].as<float>() : 0;
  YTD_KWh  = (json["YTD_KWh"].as<String>()!="") ? json["YTD_KWh"].as<float>() : 0;

  return true;
}

