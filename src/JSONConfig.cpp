#include <JSONConfig.h>
#include <RelayClass.h>

const char* filename      = "/config.json";
const char* IRMapfilename = "/IRMAP.json";
#define buffer_size  2000

extern void applyIRMap(int8_t Inpn, int8_t rlyn);


config_read_error_t loadConfig(TConfigParams &ConfParam) {

  if(SPIFFS.begin())
  {
    Serial.println(F("SPIFFS Initialize....ok"));
  }
  else
  {
    Serial.println(F("SPIFFS Initialization...failed"));
  }

  if (! SPIFFS.exists(filename)) {
    Serial.println(F("config file does not exist! ... building and rebooting...."));
    saveDefaultConfig();
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file"));
    saveDefaultConfig();
    return ERROR_OPENING_FILE;
  }

  size_t size = configFile.size();
  if (size > buffer_size) {
    Serial.println(F("Config file size is too large, rebuilding."));
    saveDefaultConfig();
    return ERROR_OPENING_FILE;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<buffer_size> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println(F("Failed to parse config file"));
    saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  // ACS_AMPS v_InputPin12_STATE_PUB_TOPIC

  ConfParam.v_ssid                = (json["ssid"].as<String>()!="") ? json["ssid"].as<String>() : String(F("ssid"));
  ConfParam.v_pass                = (json["pass"].as<String>()!="") ? json["pass"].as<String>() : String(F("passpass12"));
  ConfParam.v_PhyLoc              = (json["PhyLoc"].as<String>()!="") ? json["PhyLoc"].as<String>() : String(F("Not configured yet"));
  //ConfParam.v_MQTT_BROKER         = (json["MQTT_BROKER"].as<String>()!="") ? json["MQTT_BROKER"].as<String>() : String(F("192.168.1.1"));
  if (json["timeserver"].as<String>()!="") {
      ConfParam.v_timeserver.fromString(json["timeserver"].as<String>());} else { ConfParam.v_timeserver.fromString("192.168.1.1");}
  ConfParam.v_MQTT_Active         = (json["MQTT_Active"].as<String>()!="") ? json["MQTT_Active"].as<uint8_t>() ==1 : false;
  ConfParam.v_ntptz               = (json["ntptz"].as<String>()!="") ? json["ntptz"].as<signed char>() : 2;
  if (json["MQTT_BROKER"].as<String>()!="") {
      ConfParam.v_MQTT_BROKER.fromString(json["MQTT_BROKER"].as<String>());} else { ConfParam.v_MQTT_BROKER.fromString("192.168.1.1");}
  ConfParam.v_MQTT_B_PRT          = (json["MQTT_B_PRT"].as<String>()!="") ? json["MQTT_B_PRT"].as<uint16_t>() : 1883;

  /*
  ConfParam.v_PUB_TOPIC1          = (json["PUB_TOPIC1"].as<String>()!="") ? json["PUB_TOPIC1"].as<String>() : String(F("/none"));
  ConfParam.v_ttl_PUB_TOPIC       = (json["ttl_PUB_TOPIC"].as<String>()!="") ? json["ttl_PUB_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_i_ttl_PUB_TOPIC     = (json["i_ttl_PUB_TOPIC"].as<String>()!="") ? json["i_ttl_PUB_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_ACS_AMPS            = (json["ACS_AMPS"].as<String>()!="") ? json["ACS_AMPS"].as<String>() : String(F("/none"));
  ConfParam.v_CURR_TTL_PUB_TOPIC  = (json["CURR_TTL_PUB_TOPIC"].as<String>()!="") ? json["CURR_TTL_PUB_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_STATE_PUB_TOPIC     = (json["STATE_PUB_TOPIC"].as<String>()!="") ? json["STATE_PUB_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_ACS_Sensor_Model    = (json["ACS_Sensor_Model"].as<String>()!="") ? json["ACS_Sensor_Model"].as<String>() : String(F("10"));
  ConfParam.v_ttl                 = (json["ttl"].as<String>()!="") ? json["ttl"].as<uint32_t>() : 0;
  ConfParam.v_tta                 = (json["tta"].as<String>()!="") ? json["tta"].as<uint32_t>() : 0;
  ConfParam.v_Max_Current         = (json["Max_Current"].as<String>()!="") ? json["Max_Current"].as<uint8_t>() : 10;
  ConfParam.v_LWILL_TOPIC         = (json["LWILL_TOPIC"].as<String>()!="") ? json["LWILL_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_SUB_TOPIC1          = (json["SUB_TOPIC1"].as<String>()!="") ? json["SUB_TOPIC1"].as<String>() : String(F("/none"));
  ConfParam.v_ACS_Active          = (json["ACS_Active"].as<String>()!="") ? json["ACS_Active"].as<uint8_t>() == 1 : false;
  */

  ConfParam.v_Update_now          = (json["Update_now"].as<String>()!="") ? json["Update_now"].as<uint8_t>() == 1 : false;
  ConfParam.v_TOGGLE_BTN_PUB_TOPIC= (json["TOGGLE_BTN_PUB_TOPIC"].as<String>()!="") ? json["TOGGLE_BTN_PUB_TOPIC"].as<String>() : String(F("/none"));
  ConfParam.v_IN0_INPUTMODE       =  json["I0MODE"].as<uint8_t>();
  ConfParam.v_IN1_INPUTMODE       =  json["I1MODE"].as<uint8_t>();
  ConfParam.v_IN2_INPUTMODE       =  json["I2MODE"].as<uint8_t>();

  ConfParam.v_InputPin12_STATE_PUB_TOPIC = (json["I12_STS_PTP"].as<String>()!="") ? json["I12_STS_PTP"].as<String>() : String(F("/none"));
  ConfParam.v_InputPin14_STATE_PUB_TOPIC = (json["I14_STS_PTP"].as<String>()!="") ? json["I14_STS_PTP"].as<String>() : String(F("/none"));

  //ConfParam.v_FRM_IP              = (json["FRM_IP"].as<String>()!="") ? json["FRM_IP"].as<String>() : String(F("192.168.1.1"));

  if (json["FRM_IP"].as<String>()!="") {
      ConfParam.v_FRM_IP.fromString(json["FRM_IP"].as<String>());} else { ConfParam.v_FRM_IP.fromString("192.168.1.1");}


  ConfParam.v_FRM_PRT             = (json["FRM_PRT"].as<String>()!="") ? json["FRM_PRT"].as<uint16_t>() : 83;

  Serial.print(F("\n will connect to: ")); Serial.print(ConfParam.v_ssid);
  Serial.print(F("\n with pass: ")); Serial.print(ConfParam.v_pass);
  Serial.print(F("\n PhyLoc:")); Serial.print(ConfParam.v_PhyLoc);
  Serial.print(F("\n timeserver:")); Serial.print(ConfParam.v_timeserver);
  Serial.print(F("\n MQTT_Active:")); Serial.print(ConfParam.v_MQTT_Active);
  Serial.print(F("\n MQTT_BROKER:")); Serial.print(ConfParam.v_MQTT_BROKER);
  Serial.print(F("\n MQTT_B_PRT:")); Serial.print(ConfParam.v_MQTT_B_PRT);
  /*
  Serial.print(F("\n PUB_TOPIC1:")); Serial.print(ConfParam.v_PUB_TOPIC1);
  Serial.print(F("\n ttl:")); Serial.print(ConfParam.v_STATE_PUB_TOPIC);
  Serial.print(F("\n Input TTL Topic:")); Serial.print(ConfParam.v_i_ttl_PUB_TOPIC);
  Serial.print(F("\n ACS_AMPS:")); Serial.print(ConfParam.v_ACS_AMPS);
  Serial.print(F("\n LWILL_TOPIC:")); Serial.print(ConfParam.v_LWILL_TOPIC);
  Serial.print(F("\n SUB_TOPIC1:")); Serial.print(ConfParam.v_SUB_TOPIC1);
  Serial.print(F("\n ACS_Active:")); Serial.print(ConfParam.v_ACS_Active);
  Serial.print(F("\n ASC_Sensor_Model:")); Serial.print(ConfParam.v_ACS_Sensor_Model);
  Serial.print(F("\n Max_Current:")); Serial.print(ConfParam.v_Max_Current);
  Serial.print(F("\n ttl :")); Serial.print(ConfParam.v_ttl);
  Serial.print(F("\n tta:")); Serial.print(ConfParam.v_tta);
  */
  Serial.print(F("\n FRM_PRT:")); Serial.print(ConfParam.v_FRM_PRT);
  Serial.print(F("\n ntptz:")); Serial.print(ConfParam.v_ntptz);
  Serial.print(F("\n Update_now:")); Serial.print(ConfParam.v_Update_now);
  Serial.print(F("\n v_IN0_INPUTMODE:")); Serial.print(ConfParam.v_IN0_INPUTMODE);
  Serial.print(F("\n v_IN1_INPUTMODE:")); Serial.print(ConfParam.v_IN1_INPUTMODE);
  Serial.print(F("\n v_IN2_INPUTMODE:")); Serial.print(ConfParam.v_IN2_INPUTMODE);
  //relay1.loadrelayparams();
  return SUCCESS;
}


bool saveConfig(TConfigParams &ConfParam){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["ssid"]= ConfParam.v_ssid ;
    json["pass"]=ConfParam.v_pass;
    json["PhyLoc"]=ConfParam.v_PhyLoc;
    json["MQTT_BROKER"]=ConfParam.v_MQTT_BROKER.toString();
    json["MQTT_B_PRT"]=ConfParam.v_MQTT_B_PRT;
    json["FRM_IP"]= MyConfParam.v_FRM_IP.toString();

    json["FRM_PRT"]=ConfParam.v_FRM_PRT;
    /*
    json["PUB_TOPIC1"]=ConfParam.v_PUB_TOPIC1;
    json["ACS_Sensor_Model"] = ConfParam.v_ACS_Sensor_Model;
    json["ttl"]=ConfParam.v_ttl;
    json["ttl_PUB_TOPIC"]=ConfParam.v_ttl_PUB_TOPIC;
    json["i_ttl_PUB_TOPIC"]=ConfParam.v_i_ttl_PUB_TOPIC;
    json["ACS_AMPS"]=ConfParam.v_ACS_AMPS;
    json["CURR_TTL_PUB_TOPIC"]=ConfParam.v_CURR_TTL_PUB_TOPIC;
    json["STATE_PUB_TOPIC"]=ConfParam.v_STATE_PUB_TOPIC;
    json["tta"]=ConfParam.v_tta;
    json["Max_Current"]=ConfParam.v_Max_Current;
    json["LWILL_TOPIC"]=ConfParam.v_LWILL_TOPIC;
    json["SUB_TOPIC1"]=ConfParam.v_SUB_TOPIC1;
    json["ACS_Active"]=ConfParam.v_ACS_Active;
    */

    json["I12_STS_PTP"]=ConfParam.v_InputPin12_STATE_PUB_TOPIC;
    json["I14_STS_PTP"]=ConfParam.v_InputPin14_STATE_PUB_TOPIC;
    json["TOGGLE_BTN_PUB_TOPIC"]=ConfParam.v_TOGGLE_BTN_PUB_TOPIC;
    json["I0MODE"]=ConfParam.v_IN0_INPUTMODE;
    json["I1MODE"]=ConfParam.v_IN1_INPUTMODE;
    json["I2MODE"]=ConfParam.v_IN2_INPUTMODE;


    json["timeserver"]=ConfParam.v_timeserver.toString();
    json["MQTT_Active"]=ConfParam.v_MQTT_Active;
    json["ntptz"]=ConfParam.v_ntptz;

    json["Update_now"]=ConfParam.v_Update_now;

    File configFile = SPIFFS.open(filename, "w");
    if (!configFile) {
      Serial.println(F("Failed to open config file for writing"));
      return false;
    }

    json.printTo(configFile);
    configFile.flush();
    configFile.close();
    return true;
}

bool saveConfig(TConfigParams &ConfParam, AsyncWebServerRequest *request){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["PIC_Active"]    =  0 ;
    json["MQTT_Active"]   =  0 ;
    json["ACS_Active"]    =  0 ;
    json["Update_now"]    =  0 ;

    int args = request->args();
    for(int i=0;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }

    if(request->hasParam("PIC_Active")) json["PIC_Active"]      =  1;
    if(request->hasParam("MQTT_Active")) json["MQTT_Active"]    =  1;
    if(request->hasParam("ACS_Active")) json["ACS_Active"]      =  1;
    if(request->hasParam("Update_now")) json["Update_now"]      =  1;

    File configFile = SPIFFS.open(filename, "w");
    if (!configFile) {
      Serial.println(F("Failed to open config file for writing"));
      return false;
    }

    json.printTo(configFile);
    configFile.flush();
    configFile.close();
    return true;
}



bool saveDefaultConfig(){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

  Serial.print("\n writing default parameters");
  json["ssid"]="ksba";
  json["pass"]="samsam12";
  json["PhyLoc"]="Not configured yet";
  json["timeserver"]="194.97.156.5";
  json["ntptz"]=2;
  json["PIC_Active"]=0;
  json["MQTT_Active"]=0;

  json["MQTT_BROKER"]="192.168.1.1";
  json["MQTT_B_PRT"]=1883;

  json["PUB_TOPIC1"]="/home/Controller" + CID() + "/Coils/C1" ;
  json["STATE_PUB_TOPIC"]="/home/Controller" + CID() + "/Coils/State/C1";
  json["ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/VTTL";
  json["i_ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/i/TTL";
  json["CURR_TTL_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/CURRVTTL";
  json["LWILL_TOPIC"]="/home/Controller" + CID() + "/LWT";
  json["SUB_TOPIC1"]= "/home/Controller" + CID() +  "/#";
  json["ACS_AMPS"]="/home/Controller" + CID() + "/Coils/C1/Amps";
  json["ttl"]=0;
  json["tta"]=0;
  json["ACS_Active"]=0;
  json["tta"]="0";
  json["ACS_Sensor_Model"] = "10";
  json["Max_Current"]=10;

  json["TOGGLE_BTN_PUB_TOPIC"]="/home/Controller" + CID() + "/Coils/C1" ;
  json["I12_STS_PTP"]="/home/Controller" + CID() + "/INS/sts/IN1";
  json["I14_STS_PTP"]="/home/Controller" + CID() + "/INS/sts/IN2";
  json["I0MODE"]=1;
  json["I1MODE"]=1;
  json["I2MODE"]=1;

  json["FRM_IP"]="192.168.1.1";
  json["FRM_PRT"]=83;
  json["Update_now"]=0;
  //json["GPIO12_TOG"]="0";
  //json["Copy_IO"]="0";

  SPIFFS.remove("/config.json");
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("\n Failed to write default config file"));
    return false;
  }

  json.printTo(configFile);
    Serial.println(F("\n Saved default config"));
    configFile.flush();
    configFile.close();
  return true;
}



bool saveDefaultIRMapConfig(){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
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

  File configFile = SPIFFS.open(IRMapfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to write default config file"));
    return false;
  }

  json.printTo(configFile);
    Serial.println(F("Saved default IRMap config"));
    configFile.flush();
    configFile.close();
  return true;
}

bool saveIRMapConfig(AsyncWebServerRequest *request){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    int args = request->args();

    for(int i=0;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }

    SPIFFS.remove(IRMapfilename);
    File configFile = SPIFFS.open(IRMapfilename, "w");
    if (!configFile) {
      Serial.println(F("Failed to open config file for writing"));
      return false;
    }
    json.printTo(configFile);
    configFile.flush();
    configFile.close();
    return true;
}


config_read_error_t loadIRMapConfig(TIRMap &IRMap) {
    if(SPIFFS.begin())
    {
      Serial.println(F("SPIFFS Initialize....ok"));
    }
    else
    {
      Serial.println(F("SPIFFS Initialization...failed"));
    }

    if (! SPIFFS.exists(IRMapfilename)) {
      Serial.println(F("IRMAP config file does not exist! ... building and rebooting...."));
      while (!saveDefaultIRMapConfig()){

      };
      return FILE_NOT_FOUND;
    }

    File configFile = SPIFFS.open(IRMapfilename, "r");
    if (!configFile) {
      Serial.println(F("Failed to open IRMAP config file"));
      saveDefaultIRMapConfig();
      return ERROR_OPENING_FILE;
    }

    size_t size = configFile.size();
    if (size > buffer_size) {
      Serial.println(F("IRMAP Config file size is too large, rebuilding."));
      saveDefaultIRMapConfig();
      return ERROR_OPENING_FILE;
    }

    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    // We don't use String here because ArduinoJson library requires the input
    // buffer to be mutable. If you don't use ArduinoJson, you may as well
    // use configFile.readString instead.
    configFile.readBytes(buf.get(), size);

    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    if (!json.success()) {
      Serial.println(F("Failed to parse config file"));
      saveDefaultIRMapConfig();
      return JSONCONFIG_CORRUPTED;
    }

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
    return SUCCESS;
}






bool saveRelayDefaultConfig(){
  char  relayfilename[20] = "/relay";
  strcat(relayfilename, "01");
  strcat(relayfilename, ".json");

    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

  json["PhyLoc"]="Not configured yet";
  json["PUB_TOPIC1"]="/home/Controller" + CID() + "/Coils/C1" ;
  json["STATE_PUB_TOPIC"]="/home/Controller" + CID() + "/Coils/State/C1";
  json["ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/VTTL";
  json["i_ttl_PUB_TOPIC"]="/home/Controller" + CID() + "/i/TTL";
  json["CURR_TTL_PUB_TOPIC"]="/home/Controller" + CID() + "/sts/CURRVTTL";
  json["LWILL_TOPIC"]="/home/Controller" + CID() + "/LWT";
  json["SUB_TOPIC1"]= "/home/Controller" + CID() +  "/#";
  json["ACS_AMPS"]="/home/Controller" + CID() + "/Coils/C1/Amps";
  json["ttl"]=0;
  json["tta"]=0;
  json["ACS_Active"]=0;
  json["tta"]="0";
  json["ACS_Sensor_Model"] = "10";
  json["Max_Current"]=10;

  json["I0MODE"]=1;
  json["I1MODE"]=1;
  json["I2MODE"]=1;

  SPIFFS.remove(relayfilename);
  File configFile = SPIFFS.open(relayfilename, "w");
  if (!configFile) {
    Serial.println(F("Failed to write default relay config file"));
    return false;
  }

  json.printTo(configFile);
    Serial.println(F("Saved default relay config"));
  //  configFile.flush();
    configFile.close();
  return true;
}



bool saveRelayConfig(Trelayconf * ConfParam, AsyncWebServerRequest *request){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["ACS_Active"]    =  0 ;

    int args = request->args();
    for(int i=0;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }

    if(request->hasParam("ACS_Active")) json["ACS_Active"]      =  1;

    char  relayfilename[20] = "/relay";
    strcat(relayfilename, "01");
    strcat(relayfilename, ".json");

    SPIFFS.remove(relayfilename);
    File configFile = SPIFFS.open(relayfilename, "w");
    if (!configFile) {
      Serial.println(F("Failed to open config file for writing"));
      return false;
    }

    json.printTo(configFile);
    configFile.flush();
    configFile.close();
    return true;
}

bool saveRelayConfig(Trelayconf * ConfParam){
    StaticJsonBuffer<buffer_size> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["PhyLoc"]=ConfParam->v_PhyLoc;
    json["PUB_TOPIC1"]=ConfParam->v_PUB_TOPIC1;
    json["ACS_Sensor_Model"] = ConfParam->v_ACS_Sensor_Model;
    json["ttl"]=ConfParam->v_ttl;
    json["ttl_PUB_TOPIC"]=ConfParam->v_ttl_PUB_TOPIC;
    json["i_ttl_PUB_TOPIC"]=ConfParam->v_i_ttl_PUB_TOPIC;
    json["ACS_AMPS"]=ConfParam->v_ACS_AMPS;
    json["CURR_TTL_PUB_TOPIC"]=ConfParam->v_CURR_TTL_PUB_TOPIC;
    json["STATE_PUB_TOPIC"]=ConfParam->v_STATE_PUB_TOPIC;
    json["I0MODE"]=ConfParam->v_IN0_INPUTMODE;
    json["I1MODE"]=ConfParam->v_IN1_INPUTMODE;
    json["I2MODE"]=ConfParam->v_IN2_INPUTMODE;
    json["tta"]=ConfParam->v_tta;
    json["Max_Current"]=ConfParam->v_Max_Current;
    json["LWILL_TOPIC"]=ConfParam->v_LWILL_TOPIC;
    json["SUB_TOPIC1"]=ConfParam->v_SUB_TOPIC1;
    json["ACS_Active"]=ConfParam->v_ACS_Active;

    char  relayfilename[20] = "/relay";
    strcat(relayfilename, "01");
    strcat(relayfilename, ".json");

    SPIFFS.remove(relayfilename);
    File configFile = SPIFFS.open(relayfilename, "w");
    if (!configFile) {
      Serial.println(F("Failed to open config file for writing"));
      return false;
    }
  }
