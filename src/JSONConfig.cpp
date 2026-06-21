#include <JSONConfig.h>
#include <RelayClass.h>
#include <InputClass.h>
#include <defines.h>
#ifdef ESP32
#include "esp_heap_caps.h"
#endif

#include <FS.h>
#ifdef USE_LittleFS
   #ifdef ESP32
    #define SPIFFS LITTLEFS
    #else
    #define SPIFFS LittleFS
   #endif
  #include <LittleFS.h>
#else
  #include <SPIFFS.h>
#endif



#ifdef INVERTERLINK
  #include <Settings.h>
  extern Settings _settings;
#endif

#include <vector>
extern std::vector<void *> relays;

const char* filename      = "/config.json";
const char* IRMapfilename = "/IRMAP.json";

#ifndef ESP32
#define buffer_size_2  1900
#endif
#ifdef ESP32
#define buffer_size_2  3000
#endif
#define buffer_size_IR  1000

extern void applyIRMap(int8_t Inpn, int8_t rlyn);


config_read_error_t loadConfig(TConfigParams &ConfParam) {
Serial.println(F("[INFO   ] loading SPIFFS"));
/*  if(SPIFFS.begin())
  {
    Serial.println(F("[INFO   ] SPIFFS Initialize....ok"));
  }
  else
  {
    Serial.println(F("[INFO   ] SPIFFS Initialization...failed"));
  }
  */

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

      Serial.println(F("[INFO   ] Allocating mem to open config.json"));
      StaticJsonDocument<3072> json;
      Serial.println(F("[INFO   ] opening config.json after allocation"));



  // Deserialize the JSON document
  size_t configFileSize = configFile.size();
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.printf("[INFO   ] Failed to read config.json (%u bytes, buffer_size_2=%d): %s — rebuilding with this board's chip ID (%s)\n",
                  (unsigned)configFileSize, (int)buffer_size_2, error.c_str(), CID().c_str());
    configFile.close();
    saveDefaultConfig();
    return JSONCONFIG_CORRUPTED;
  }

  ConfParam.v_ssid                = (json["ssid"].as<String>()!="") ? json["ssid"].as<String>() : String(F("ksba"));
  ConfParam.v_pass                = (json["pass"].as<String>()!="") ? json["pass"].as<String>() : String(F("samsam12"));

  ConfParam.v_mqttUser            = (json["mqttUser"].as<String>()!="") ? json["mqttUser"].as<String>() : String(F(""));
  ConfParam.v_mqttPass            = (json["mqttPass"].as<String>()!="") ? json["mqttPass"].as<String>() : String(F(""));
  if (ConfParam.v_mqttUser.equalsIgnoreCase(F("null"))) ConfParam.v_mqttUser = "";
  if (ConfParam.v_mqttPass.equalsIgnoreCase(F("null"))) ConfParam.v_mqttPass = "";

  ConfParam.v_PhyLoc              = (json["PhyLoc"].as<String>()!="") ? json["PhyLoc"].as<String>() : String(F("Not configured yet"));
  ConfParam.v_MQTT_BROKER         = (json["MQTT_BROKER"].as<String>()!="") ? json["MQTT_BROKER"].as<String>() : String(F("192.168.1.1"));

  ConfParam.v_timeserver = (json["timeserver"].as<String>()!="") ? json["timeserver"].as<String>() : String(F("192.168.50.1"));
  ConfParam.v_timeserver.trim();
  ConfParam.v_NTP_UseVPN = (json["NTP_UseVPN"].as<String>()!="") ? json["NTP_UseVPN"].as<uint8_t>() == 1 : false;

  if (json["Pingserver"].as<String>()!="") {
      ConfParam.v_Pingserver.fromString(json["Pingserver"].as<String>());} else
      { ConfParam.v_Pingserver.fromString("192.168.50.1");}      

      

  ConfParam.v_MQTT_Active         = (json["MQTT_Active"].as<String>()!="") ? json["MQTT_Active"].as<uint8_t>() ==1 : false;
    ConfParam.v_MQTT_UseVPN = (json["MQTT_UseVPN"].as<String>()!="") ? json["MQTT_UseVPN"].as<uint8_t>() ==1 : false;
  ConfParam.v_ntptz               = (json["ntptz"].as<String>()!="") ? json["ntptz"].as<signed char>() : 2;

  /*if (json["MQTT_BROKER"].as<String>()!="") {
      ConfParam.v_MQTT_BROKER.fromString(json["MQTT_BROKER"].as<String>());} else
      { ConfParam.v_MQTT_BROKER.fromString("192.168.1.1");}
      */

  ConfParam.v_MQTT_B_PRT          = (json["MQTT_B_PRT"].as<String>()!="") ? json["MQTT_B_PRT"].as<uint16_t>() : 1883;
  ConfParam.v_MQTT_KeepAliveSeconds = (json["MQTT_KeepAliveSeconds"].as<String>()!="") ? json["MQTT_KeepAliveSeconds"].as<uint16_t>() : 30;
  if (ConfParam.v_MQTT_KeepAliveSeconds < 5) ConfParam.v_MQTT_KeepAliveSeconds = 5;
  ConfParam.v_Update_now          = (json["Update_now"].as<String>()!="") ? json["Update_now"].as<uint8_t>() == 1 : false;
  ConfParam.v_TOGGLE_BTN_PUB_TOPIC= (json["TOGGLE_BTN_PUB_TOPIC"].as<String>()!="") ? json["TOGGLE_BTN_PUB_TOPIC"].as<String>() : String(F("/none"));

  ConfParam.v_IN0_INPUTMODE       =  json["I0MODE"].as<uint8_t>();
  ConfParam.v_IN1_INPUTMODE       =  json["I1MODE"].as<uint8_t>();
  ConfParam.v_IN2_INPUTMODE       =  json["I2MODE"].as<uint8_t>();
  ConfParam.v_IN3_INPUTMODE       =  json["I3MODE"].as<uint8_t>();
  ConfParam.v_IN4_INPUTMODE       =  json["I4MODE"].as<uint8_t>();
  ConfParam.v_IN5_INPUTMODE       =  json["I5MODE"].as<uint8_t>();
  ConfParam.v_IN6_INPUTMODE       =  json["I6MODE"].as<uint8_t>();

  #ifndef HWESP32
    ConfParam.v_InputPin12_STATE_PUB_TOPIC = (json["I12_STS_PTP"].as<String>()!="") ? json["I12_STS_PTP"].as<String>() : String(F("/none"));
    ConfParam.v_InputPin13_STATE_PUB_TOPIC = (json["I13_STS_PTP"].as<String>()!="") ? json["I13_STS_PTP"].as<String>() : String(F("/none"));
    ConfParam.v_InputPin14_STATE_PUB_TOPIC = (json["I14_STS_PTP"].as<String>()!="") ? json["I14_STS_PTP"].as<String>() : String(F("/none"));
  #endif
  #ifdef HWESP32
    // Load the legacy keys first — the web form and all existing saved configs
    // use I12_STS_PTP / I14_STS_PTP as the field names (displayed via processor()
    // as "I12_STS_PTP" / "I14_STS_PTP" template variables).
    ConfParam.v_InputPin12_STATE_PUB_TOPIC = (json["I12_STS_PTP"].as<String>()!="") ? json["I12_STS_PTP"].as<String>() : String(F("/none"));
    ConfParam.v_InputPin14_STATE_PUB_TOPIC = (json["I14_STS_PTP"].as<String>()!="") ? json["I14_STS_PTP"].as<String>() : String(F("/none"));
    // Map the dedicated IN1/IN2 fields — prefer the dedicated key if present
    // (written by saveConfig(TConfigParams&)), otherwise fall back to the legacy
    // key so existing config.json files keep working without re-flashing.
    String _i01 = json["I01_STS_PTP"].as<String>();
    ConfParam.v_InputPin01_STATE_PUB_TOPIC = (_i01!="" && _i01!="null") ? _i01 : ConfParam.v_InputPin12_STATE_PUB_TOPIC;
    String _i02 = json["I02_STS_PTP"].as<String>();
    ConfParam.v_InputPin02_STATE_PUB_TOPIC = (_i02!="" && _i02!="null") ? _i02 : ConfParam.v_InputPin14_STATE_PUB_TOPIC;
    // Temperature pins are configured through the IN1/IN2 input mode combos.
    // Keep the old TempSensorPinXXActive keys as a migration path for existing config.json files.
    if (json["TempSensorPin01Active"].as<uint8_t>() == 1) ConfParam.v_IN1_INPUTMODE = INPUT_TEMPERATURE;
    if (json["TempSensorPin02Active"].as<uint8_t>() == 1) ConfParam.v_IN2_INPUTMODE = INPUT_TEMPERATURE;
    if (ConfParam.v_IN0_INPUTMODE == INPUT_TEMPERATURE) ConfParam.v_IN0_INPUTMODE = INPUT_NORMAL;
    ConfParam.v_TempSensorPin01_Active = (ConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE) ? 1 : 0;
    ConfParam.v_TempSensorPin02_Active = (ConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE) ? 1 : 0;
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
  ConfParam.v_Reboot_on_WIFI_Disconnection  =  json["RWD"].as<uint16_t>();
  ConfParam.v_PRST                           = json["PRST"].as<uint8_t>();
  ConfParam.v_DailyRestartEnabled = json.containsKey("DailyRestartEnabled") ? json["DailyRestartEnabled"].as<uint8_t>() : 0;
  ConfParam.v_DailyRestartTime    = (json["DailyRestartTime"].as<String>()!="") ? json["DailyRestartTime"].as<String>() : String(F("03:00"));
  ConfParam.v_CurrentTransformer_max_current = (json["CurrentTransformer_max_current"].as<String>()!="") ? json["CurrentTransformer_max_current"].as<uint8_t>() : 0;
  ConfParam.v_calibration                    = (json["calibration"].as<String>()!="") ? json["calibration"].as<double>() : 1908;
  ConfParam.v_PhaseCal                        = (json["PhaseCal"].as<String>()!="") ? json["PhaseCal"].as<double>() : 1.7;
  ConfParam.v_VCal_Vp                        = json.containsKey("VCal_Vp") ? json["VCal_Vp"].as<float>() : 0.0f;
  ConfParam.v_VCal_Vs                        = json.containsKey("VCal_Vs") ? json["VCal_Vs"].as<float>() : 0.0f;
  ConfParam.v_VCal_R1                        = json.containsKey("VCal_R1") ? json["VCal_R1"].as<float>() : 0.0f;
  ConfParam.v_VCal_R2                        = json.containsKey("VCal_R2") ? json["VCal_R2"].as<float>() : 0.0f;
  ConfParam.v_CurrentTransformerTopic        = (json["CurrentTransformerTopic"].as<String>()!="") ? json["CurrentTransformerTopic"].as<String>() : String(F(""));  
  ConfParam.v_ToleranceOffTime               = (json["ToleranceOffTime"].as<String>()!="") ? json["ToleranceOffTime"].as<uint16_t>() : 10;
  ConfParam.v_ToleranceOnTime                = (json["ToleranceOnTime"].as<String>()!="") ? json["ToleranceOnTime"].as<uint16_t>() : 30;  
  ConfParam.v_CT_MaxAllowed_current          = (json["CT_MaxAllowed_current"].as<String>()!="") ? json["CT_MaxAllowed_current"].as<uint16_t>() : 30;  
  ConfParam.v_CT_adjustment                  = (json["CT_adjustment"].as<String>()!="") ? json["CT_adjustment"].as<float_t>() : 0.05;
  ConfParam.v_CT_saveThreshold               = json.containsKey("CT_saveThreshold") ? json["CT_saveThreshold"].as<uint16_t>() : 10;
  ConfParam.v_CT_ZeroLoadCurrentMax          = (json["CT_ZeroLoadCurrentMax"].as<String>()!="") ? json["CT_ZeroLoadCurrentMax"].as<float>() : 0.2f;
  ConfParam.v_CT_ZeroLoadPowerMin            = (json["CT_ZeroLoadPowerMin"].as<String>()!="") ? json["CT_ZeroLoadPowerMin"].as<float>() : -30.0f;
  ConfParam.v_CT_ZeroLoadPowerMax            = (json["CT_ZeroLoadPowerMax"].as<String>()!="") ? json["CT_ZeroLoadPowerMax"].as<float>() : 40.0f;
  ConfParam.v_EmonReaderIntervalMs           = json.containsKey("EmonReaderIntervalMs")    ? json["EmonReaderIntervalMs"].as<uint32_t>()    : 2000UL;
  ConfParam.v_EmonPublisherIntervalMs        = json.containsKey("EmonPublisherIntervalMs") ? json["EmonPublisherIntervalMs"].as<uint32_t>() : 5000UL;
  ConfParam.v_OLEDUpdateIntervalMs           = json.containsKey("OLEDUpdateIntervalMs")    ? json["OLEDUpdateIntervalMs"].as<uint32_t>()    : 1000UL;
  ConfParam.v_OLEDAlwaysOn                   = json.containsKey("OLEDAlwaysOn")            ? json["OLEDAlwaysOn"].as<uint8_t>()             : 1;
  ConfParam.v_OLED_HW_Active                 = json.containsKey("OLED_HW_Active")          ? json["OLED_HW_Active"].as<uint8_t>()           : 1;
  ConfParam.v_ADS_HW_Active                  = json.containsKey("ADS_HW_Active")           ? json["ADS_HW_Active"].as<uint8_t>()            : 1;
  if (ConfParam.v_EmonReaderIntervalMs < 500UL) ConfParam.v_EmonReaderIntervalMs = 500UL;
  if (ConfParam.v_EmonPublisherIntervalMs < 1000UL) ConfParam.v_EmonPublisherIntervalMs = 1000UL;
  if (ConfParam.v_OLEDUpdateIntervalMs < 500UL) ConfParam.v_OLEDUpdateIntervalMs = 500UL;
  if (ConfParam.v_OLEDUpdateIntervalMs > 4000UL) ConfParam.v_OLEDUpdateIntervalMs = 4000UL;
  ConfParam.v_OLEDAlwaysOn = ConfParam.v_OLEDAlwaysOn ? 1 : 0;
  ConfParam.v_Screen_orientation             = (json["Screen_orientation"].as<String>()!="") ? json["Screen_orientation"].as<uint8_t>() : 0; 


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

      Serial.println(F("[INFO   ] Allocating mem to open config.json"));
  DynamicJsonDocument json(buffer_size_2);
      Serial.println(F("[INFO   ] opening config.json"));
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
      json[F("I13_STS_PTP")]=ConfParam.v_InputPin13_STATE_PUB_TOPIC;
      json[F("I14_STS_PTP")]=ConfParam.v_InputPin14_STATE_PUB_TOPIC;
      json[F("I0MODE")]=ConfParam.v_IN0_INPUTMODE;
      json[F("I1MODE")]=ConfParam.v_IN1_INPUTMODE;
      json[F("I2MODE")]=ConfParam.v_IN2_INPUTMODE;
    #endif
     #ifdef HWESP32
      json[F("I12_STS_PTP")]=ConfParam.v_InputPin01_STATE_PUB_TOPIC;
      json[F("I14_STS_PTP")]=ConfParam.v_InputPin02_STATE_PUB_TOPIC;
      json[F("I01_STS_PTP")]=ConfParam.v_InputPin01_STATE_PUB_TOPIC;
      json[F("I02_STS_PTP")]=ConfParam.v_InputPin02_STATE_PUB_TOPIC;
      json[F("I0MODE")]=ConfParam.v_IN0_INPUTMODE;
      json[F("I1MODE")]=ConfParam.v_IN1_INPUTMODE;
      json[F("I2MODE")]=ConfParam.v_IN2_INPUTMODE;
      json[F("TempSensorPin01Active")]=(ConfParam.v_IN1_INPUTMODE == INPUT_TEMPERATURE) ? 1 : 0;
      json[F("TempSensorPin02Active")]=(ConfParam.v_IN2_INPUTMODE == INPUT_TEMPERATURE) ? 1 : 0;
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

    json[F("TOGGLE_BTN_PUB_TOPIC")]=ConfParam.v_TOGGLE_BTN_PUB_TOPIC;
    json[F("timeserver")]=ConfParam.v_timeserver; 
    json[F("NTP_UseVPN")]=ConfParam.v_NTP_UseVPN;
    json[F("Pingserver")]=ConfParam.v_Pingserver.toString();     
    json[F("MQTT_Active")]=ConfParam.v_MQTT_Active;
    json[F("MQTT_UseVPN")]=ConfParam.v_MQTT_UseVPN;
    json[F("MQTT_KeepAliveSeconds")]=ConfParam.v_MQTT_KeepAliveSeconds;
    json[F("ntptz")]=ConfParam.v_ntptz;
    json[F("Update_now")]=ConfParam.v_Update_now;
    json[F("Sonar_distance")]=ConfParam.v_Sonar_distance;
    json[F("Sonar_distance_max")]=ConfParam.v_Sonar_distance_max; 
    json[F("RWD")]  = ConfParam.v_Reboot_on_WIFI_Disconnection;
    json[F("PRST")] = ConfParam.v_PRST;
    json[F("DailyRestartEnabled")] = ConfParam.v_DailyRestartEnabled;
    json[F("DailyRestartTime")]    = ConfParam.v_DailyRestartTime;

    json[F("CurrentTransformer_max_current")] = ConfParam.v_CurrentTransformer_max_current;
    json[F("calibration")]                    = ConfParam.v_calibration;
    json[F("PhaseCal")]                       = ConfParam.v_PhaseCal;
    json[F("VCal_Vp")]                        = ConfParam.v_VCal_Vp;
    json[F("VCal_Vs")]                        = ConfParam.v_VCal_Vs;
    json[F("VCal_R1")]                        = ConfParam.v_VCal_R1;
    json[F("VCal_R2")]                        = ConfParam.v_VCal_R2;
    json[F("CurrentTransformerTopic")]        = ConfParam.v_CurrentTransformerTopic;        
    json[F("ToleranceOnTime")]                = ConfParam.v_ToleranceOnTime; 
    json[F("ToleranceOffTime")]               = ConfParam.v_ToleranceOffTime;        
    json[F("CT_adjustment")]                  = ConfParam.v_CT_adjustment;       
    json[F("CT_saveThreshold")]               = ConfParam.v_CT_saveThreshold;        
    json[F("CT_ZeroLoadCurrentMax")]          = ConfParam.v_CT_ZeroLoadCurrentMax;
    json[F("CT_ZeroLoadPowerMin")]            = ConfParam.v_CT_ZeroLoadPowerMin;
    json[F("CT_ZeroLoadPowerMax")]            = ConfParam.v_CT_ZeroLoadPowerMax;
    json[F("EmonReaderIntervalMs")]           = ConfParam.v_EmonReaderIntervalMs;
    json[F("EmonPublisherIntervalMs")]        = ConfParam.v_EmonPublisherIntervalMs;
    json[F("OLEDUpdateIntervalMs")]           = ConfParam.v_OLEDUpdateIntervalMs;
    json[F("OLEDAlwaysOn")]                   = ConfParam.v_OLEDAlwaysOn;
    json[F("Screen_orientation")]             = ConfParam.v_Screen_orientation;
    json[F("OLED_HW_Active")]                 = ConfParam.v_OLED_HW_Active;
    json[F("ADS_HW_Active")]                  = ConfParam.v_ADS_HW_Active;

    json[F("CT_MaxAllowed_current")]          = ConfParam.v_CT_MaxAllowed_current;

    if (serializeJsonPretty(json, configFile) == 0) {
      Serial.println(F("[INFO   ] Failed to write to file"));
      configFile.close();
      return false;
    }
  configFile.print("\n");
  configFile.flush();
  configFile.close();

  return true;
}


bool saveConfig(TConfigParams &ConfParam, AsyncWebServerRequest *request){

  StaticJsonDocument<3000> doc;

  // Seed doc with existing config so fields not in this form are preserved.
  File existing = SPIFFS.open(filename, "r");
  if (existing) {
    deserializeJson(doc, existing);
    existing.close();
  }

    int args = request->args();
    for(int i=0;i<args;i++){
      if (request->arg(i) != NULL) {
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      doc[request->argName(i)] =  request->arg(i);
      }
    }

  (request->hasParam("PIC_Active"))  ? doc["PIC_Active"]  = 1 : doc["PIC_Active"]  = 0; 
  (request->hasParam("MQTT_Active")) ? doc["MQTT_Active"] = 1 : doc["MQTT_Active"] = 0;
  (request->hasParam("MQTT_UseVPN")) ? doc["MQTT_UseVPN"] = 1 : doc["MQTT_UseVPN"] = 0;
  (request->hasParam("NTP_UseVPN"))  ? doc["NTP_UseVPN"]  = 1 : doc["NTP_UseVPN"]  = 0;
  (request->hasParam("ACS_Active"))  ? doc["ACS_Active"]  = 1 : doc["ACS_Active"]  = 0;
  (request->hasParam("Update_now"))  ? doc["Update_now"]  = 1 : doc["Update_now"]  = 0;
  (request->hasParam("OLEDAlwaysOn"))      ? doc["OLEDAlwaysOn"]      = 1 : doc["OLEDAlwaysOn"]      = 0;
  (request->hasParam("OLED_HW_Active"))   ? doc["OLED_HW_Active"]   = 1 : doc["OLED_HW_Active"]   = 0;
  (request->hasParam("ADS_HW_Active"))    ? doc["ADS_HW_Active"]    = 1 : doc["ADS_HW_Active"]    = 0;
  (request->hasParam("DailyRestartEnabled")) ? doc["DailyRestartEnabled"] = 1 : doc["DailyRestartEnabled"] = 0;

  uint32_t oledUpdateInterval = 1000UL;
  if (doc.containsKey("OLEDUpdateIntervalMs") && doc["OLEDUpdateIntervalMs"].as<String>() != "") {
    oledUpdateInterval = doc["OLEDUpdateIntervalMs"].as<uint32_t>();
  }
  if (oledUpdateInterval < 500UL) oledUpdateInterval = 500UL;
  if (oledUpdateInterval > 4000UL) oledUpdateInterval = 4000UL;
  doc["OLEDUpdateIntervalMs"] = oledUpdateInterval;

  // Opening with "w" truncates the existing file atomically on LittleFS.
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
  configFile.print("\n");
  configFile.flush();
  configFile.close();

  return true;
}



bool saveDefaultConfig(){
  DynamicJsonDocument json(buffer_size_2);

  Serial.print("\n writing default parameters");
  json[F("ssid")]="ksba";
  json[F("pass")]="samsam12";

  json[F("mqttUser")]="";
  json[F("mqttPass")]="";

  json[F("PhyLoc")]="Not configured yet";
  json[F("timeserver")]="194.97.156.5";
  json[F("NTP_UseVPN")]=0;
  json[F("ntptz")]=2;
  json[F("PIC_Active")]=0;
  json[F("MQTT_Active")]=0;
  json[F("MQTT_UseVPN")]=0;
  json[F("MQTT_BROKER")]="192.168.1.1";
  json[F("MQTT_B_PRT")]=1883;

  json[F("TOGGLE_BTN_PUB_TOPIC")]="/home/Controller" + CID() + "/INS/sts/IN0" ;
  
    json[F("I12_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN1";
    json[F("I13_STS_PTP")]="/home/Controller" + CID() + "/INS/sts/IN3";
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
    #ifdef HWESP32
    json[F("TempSensorPin01Active")]=0;
    json[F("TempSensorPin02Active")]=0;
    #endif


  json[F("Sonar_distance")]=0;
  json[F("Sonar_distance_max")]=50; 

  json[F("FRM_IP")]="192.168.1.1";
  json[F("FRM_PRT")]=83;
  json[F("Update_now")]=0;
  json[F("MQTT_KeepAliveSeconds")]=30;

  json[F("CurrentTransformer_max_current")] = 50;
  json[F("calibration")]                    = 185; 
  json[F("ToleranceOffTime")]               = 10; 
  json[F("ToleranceOnTime")]                = 30;
  json[F("CT_MaxAllowed_current")]          = 30;      
  json[F("CurrentTransformerTopic")]        = "/home/Controller" + CID() + "/CT";   
  json[F("CT_adjustment")]                  = 0;    
  json[F("CT_saveThreshold")]               = 10;      
  json[F("CT_ZeroLoadCurrentMax")]          = 0.2;
  json[F("CT_ZeroLoadPowerMin")]            = -30.0;
  json[F("CT_ZeroLoadPowerMax")]            = 40.0;
  json[F("EmonReaderIntervalMs")]           = 2000;
  json[F("EmonPublisherIntervalMs")]        = 5000;
  json[F("OLEDUpdateIntervalMs")]           = 1000;
  json[F("OLEDAlwaysOn")]                   = 1;
  json[F("Screen_orientation")]             = 0;
  json[F("OLED_HW_Active")]                 = 1;
  json[F("ADS_HW_Active")]                  = 1;
  json[F("PRST")]                           = 0;
  json[F("DailyRestartEnabled")]            = 0;
  json[F("DailyRestartTime")]               = "03:00";
  
      

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
  configFile.print("\n");
  configFile.flush();
  configFile.close();

  return true;
}



bool saveDefaultIRMapConfig(){
  DynamicJsonDocument json(buffer_size_IR);

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
    configFile.print("\n");
    configFile.flush();
    configFile.close();

  return SUCCESS;
}


bool saveIRMapConfig(AsyncWebServerRequest *request){

  StaticJsonDocument<1024> json;

  // Set the values in the document

    int args = request->args();

    for(int i=0;i<args;i++){
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      json[request->argName(i)] =  request->arg(i) ;
    }


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
  configFile.print("\n");
  configFile.flush();
  configFile.close();
  return true;
}


config_read_error_t loadIRMapConfig(TIRMap &IRMap) {
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


  StaticJsonDocument<1024> json;
  DeserializationError error = deserializeJson(json, configFile);
  configFile.close();
  if (error) {
    Serial.println(F("[INFO   ] Failed to parse IRMAP config file"));
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



bool saveRelayDefaultConfig(uint8_t rnb){
  DynamicJsonDocument json(buffer_size_2);

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
  configFile.print("\n");
  configFile.flush();
  configFile.close();

  return true;

}


bool saveRelayConfig(AsyncWebServerRequest *request){
  // Previous design used a temp-file atomic rename (write→temp, remove old, rename).
  // That caused 7–9 sequential LittleFS flash operations. On a 74%-full filesystem
  // each write can trigger garbage collection (1-3 s each). Stacked up they exceeded
  // the 5-second Task Watchdog Timer, crashing ipc0 on Core 0.
  //
  // New design: direct write to final file (4 flash ops total) + update in-memory
  // relay struct from request params so the caller does NOT need loadrelayparams().

  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 5120) {
    Serial.println(F("[INFO   ] saveRelayConfig: heap too low"));
    return false;
  }
  DynamicJsonDocument json(buffer_size_2);

  json["ACS_Active"] = 0;

  int args = request->args();
  for (int i = 0; i < args; i++) {
    const String& val = request->arg(i);
    if (val.length() > 64) {
      json[request->argName(i)] = val.substring(0, 64);
    } else {
      json[request->argName(i)] = val;
    }
  }
  if (request->hasParam("ACS_Active")) json["ACS_Active"] = 1;

  const String& rnbStr = request->hasParam("RELAYNB")
                       ? request->getParam("RELAYNB")->value()
                       : request->getParam("RelayNB")->value();
  uint8_t rnb = rnbStr.toInt();
  json["RELAYNB"] = rnb;
  json.remove("RelayNB");

  if (rnb >= relays.size()) {
    Serial.printf("[INFO   ] saveRelayConfig: bad RELAYNB %u\n", rnb);
    return false;
  }

  char relayfilename[20];
  mkRelayConfigName(relayfilename, rnb);

  // Op 1: open for write (overwrites existing file directly — no temp rename)
  File f = SPIFFS.open(relayfilename, "w");
  if (!f) {
    Serial.printf("[INFO   ] saveRelayConfig: cannot open %s\n", relayfilename);
    return false;
  }
  // Op 2: write JSON (compact)
  size_t written = serializeJson(json, f);
  // Op 3: flush + close
  f.flush();
  f.close();

  if (written == 0) {
    Serial.println(F("[INFO   ] saveRelayConfig: write failed"));
    return false;
  }

  Serial.printf("[INFO   ] Relay config saved: %s (%u B, 3 flash ops)\n", relayfilename, (unsigned)written);

  // Update the in-memory relay struct directly from the JSON we just built.
  // This replaces the loadrelayparams() call in the caller, saving 2 more flash ops.
  Relay *r = getrelaybynumber(rnb);
  if (r && r->RelayConfParam) {
    Trelayconf *rc = r->RelayConfParam;
    if (json.containsKey("PUB_TOPIC1"))        rc->v_PUB_TOPIC1        = json["PUB_TOPIC1"].as<String>();
    if (json.containsKey("STATE_PUB_TOPIC"))   rc->v_STATE_PUB_TOPIC   = json["STATE_PUB_TOPIC"].as<String>();
    if (json.containsKey("ttl_PUB_TOPIC"))     rc->v_ttl_PUB_TOPIC     = json["ttl_PUB_TOPIC"].as<String>();
    if (json.containsKey("CURR_TTL_PUB_TOPIC"))rc->v_CURR_TTL_PUB_TOPIC= json["CURR_TTL_PUB_TOPIC"].as<String>();
    if (json.containsKey("i_ttl_PUB_TOPIC"))   rc->v_i_ttl_PUB_TOPIC   = json["i_ttl_PUB_TOPIC"].as<String>();
    if (json.containsKey("ACS_AMPS"))          rc->v_ACS_AMPS          = json["ACS_AMPS"].as<String>();
    if (json.containsKey("LWILL_TOPIC"))       rc->v_LWILL_TOPIC       = json["LWILL_TOPIC"].as<String>();
    if (json.containsKey("SUB_TOPIC1"))        rc->v_SUB_TOPIC1        = json["SUB_TOPIC1"].as<String>();
    if (json.containsKey("TemperatureValue"))  rc->v_TemperatureValue  = json["TemperatureValue"].as<String>();
    if (json.containsKey("AlexaName"))         rc->v_AlexaName         = json["AlexaName"].as<String>();
    if (json.containsKey("ttl"))               rc->v_ttl               = json["ttl"].as<uint32_t>();
    if (json.containsKey("tta"))               rc->v_tta               = json["tta"].as<uint32_t>();
    if (json.containsKey("ACS_Active"))        rc->v_ACS_Active        = json["ACS_Active"].as<int>() != 0;
    if (json.containsKey("ACS_elasticity"))    rc->v_ACS_elasticity    = json["ACS_elasticity"].as<int>();
    if (json.containsKey("Max_Current"))       rc->v_Max_Current       = json["Max_Current"].as<float>();
  }

  return true;
}


bool saveRelayConfig(Trelayconf * RConfParam){
  StaticJsonDocument<1536> json;

    json[F("RELAYNB")]=RConfParam->v_relaynb;
    json[F("PhyLoc")]=RConfParam->v_PhyLoc;
    json[F("PUB_TOPIC1")]=RConfParam->v_PUB_TOPIC1;
    json[F("TemperatureValue")]=RConfParam->v_TemperatureValue;
    json[F("AlexaName")]=RConfParam->v_AlexaName;
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

  configFile.print("\n");
  /*StaticJsonDocument<64> dummy;
  dummy["end"]="0";
  if (serializeJsonPretty(dummy, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }*/
  configFile.flush();
  configFile.close();

  return true;
  }



void saveCTReadings(double KWh, double MTD_KWh, double YTD_KWh, int lastDay, int lastMonth, int lastYear){
  StaticJsonDocument<256> json;
  json[F("KWh")]     = KWh;
  json[F("MTD_KWh")] = MTD_KWh;
  json[F("YTD_KWh")] = YTD_KWh;
  // Persist the last-reset anchor dates so a reboot between midnight and 1 AM
  // does not cause the day/month/year reset to be skipped on the next boot.
  if (lastDay != -1) {
    json[F("lastDay")]   = lastDay;
    json[F("lastMonth")] = lastMonth;
    json[F("lastYear")]  = lastYear;
  }

  // Serialize to a String first — never open the file until we know the
  // payload is valid, so a serialization failure cannot truncate the file.
  String payload;
  if (serializeJsonPretty(json, payload) == 0 || payload.isEmpty()) {
    Serial.println(F("[CT     ] saveCTReadings: serialization failed — file not touched"));
    return;
  }

  File configFile = SPIFFS.open("/AccumulatedPower.json", "w");
  if (!configFile) {
    Serial.println(F("[CT     ] saveCTReadings: failed to open AccumulatedPower.json — values preserved in RAM"));
    return;
  }

  size_t written = configFile.print(payload);
  configFile.flush();
  configFile.close();

  if (written != payload.length()) {
    Serial.printf("[CT     ] saveCTReadings: incomplete write (%u / %u bytes)\n",
                  (unsigned)written, (unsigned)payload.length());
  }
}




void loadCTReadings(double &KWh, double &MTD_KWh, double &YTD_KWh, int &lastDay, int &lastMonth, int &lastYear) {
  Serial.println(F("[INFO   ] Starting AccumulatedPower.json parsing"));
  lastDay = lastMonth = lastYear = -1;
  File configFile = SPIFFS.open("/AccumulatedPower.json", "r");
  if (!configFile) {
    Serial.println(F("[CT     ] AccumulatedPower.json not found — starting from zero"));
    KWh = MTD_KWh = YTD_KWh = 0;
    return;
  }

  StaticJsonDocument<512> json;

  DeserializationError error = deserializeJson(json, configFile);
  configFile.close();
  if (error) {
    Serial.println(F("[INFO   ] Failed to parse AccumulatedPower.json, resetting to zero"));
    KWh = MTD_KWh = YTD_KWh = 0;
    return;
  }

  KWh     = json["KWh"]     | 0.0;
  MTD_KWh = json["MTD_KWh"] | 0.0;
  YTD_KWh = json["YTD_KWh"] | 0.0;
  lastDay   = json["lastDay"]   | -1;
  lastMonth = json["lastMonth"] | -1;
  lastYear  = json["lastYear"]  | -1;
  Serial.printf("[CT     ] Loaded: KWh=%.2f MTD=%.2f YTD=%.2f lastDay=%d lastMonth=%d lastYear=%d\n",
                KWh, MTD_KWh, YTD_KWh, lastDay, lastMonth, lastYear);
}

