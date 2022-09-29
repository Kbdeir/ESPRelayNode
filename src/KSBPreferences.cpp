#ifdef ESP32
#ifdef USEPREF
  #include <Preferences.h>
  #include <KSBPreferences.h>
  #include <Arduino.h>
  #include <ESPAsyncWebServer.h>

//void SaveWifi_to_Preferences(Preferences preferences, String ssid, String pass)
void SaveWifi_to_Preferences(Preferences preferences, String ssid, String pass)
{
  preferences.begin("Args_param", false);
  preferences.putString("ssid", ssid );
  preferences.putString("pass", pass );
  preferences.end();
}

void ReadParams(TConfigParams &ConfParam, Preferences preferences){
    preferences.begin("Args_param", false);
      ConfParam.v_ssid = preferences.getString("ssid");
      ConfParam.v_pass =  preferences.getString("pass");
  		ConfParam.v_PhyLoc =  preferences.getString("PhyLoc");
  		ConfParam.v_MQTT_BROKER =  preferences.getString("MQTT_BROKER");
  		ConfParam.v_MQTT_B_PRT =  preferences.getString("MQTT_B_PRT");
  		ConfParam.v_PUB_TOPIC1 =  preferences.getString("PUB_TOPIC1");
  		ConfParam.v_FRM_IP =  preferences.getString("FRM_IP");
  		ConfParam.v_FRM_PRT =  preferences.getString("FRM_PRT");
  		ConfParam.v_ASCmultiple =  preferences.getString("ACSmultiple");
  		ConfParam.v_ttl =  preferences.getString("ttl");
  		ConfParam.v_tta =  preferences.getString("tta");
  		ConfParam.v_Max_Current =  preferences.getString("Max_Current");
  		ConfParam.v_timeserver =  preferences.getString("timeserver"); 
  		ConfParam.v_Pingserver =  preferences.getString("Pingserver");       
  		//ConfParam.v_PIC_Active =  preferences.getString("PIC_Active");
  		ConfParam.v_MQTT_Active =  preferences.getString("MQTT_Active");
  		ConfParam.v_myppp =  preferences.getString("myppp");
  		ConfParam.v_ntptz =  preferences.getString("ntptz");
  		ConfParam.v_LWILL_TOPIC =  preferences.getString("LWILL_TOPIC");
  		ConfParam.v_SUB_TOPIC1 =  preferences.getString("SUB_TOPIC1");
  		ConfParam.v_GPIO12_TOG =  preferences.getString("GPIO12_TOG");
  		ConfParam.v_Copy_IO =  preferences.getString("Copy_IO");
  		ConfParam.v_ACS_Active =  preferences.getString("ACS_Active");
  		ConfParam.v_Update_now =  preferences.getString("Update_now");

      Serial.println("will connect to: " + preferences.getString("ssid"));
      Serial.println("with pass: " + preferences.getString("pass"));
      Serial.println("PhyLoc:" + preferences.getString("PhyLoc"));
      Serial.println("MQTT_BROKER:" + preferences.getString("MQTT_BROKER"));
      Serial.println("MQTT_B_PRT:" + preferences.getString("MQTT_B_PRT"));
      Serial.println("PUB_TOPIC1:" + preferences.getString("PUB_TOPIC1"));
      Serial.println("FRM_IP:" + preferences.getString("FRM_IP"));
      Serial.println("FRM_PRT:" + preferences.getString("FRM_PRT"));
      Serial.println("ASCmultiple:" + preferences.getString("ACSmultiple"));
      Serial.println("ttl:" + preferences.getString("ttl"));
      Serial.println("tta:" + preferences.getString("tta"));
      Serial.println("Max_Current:" + preferences.getString("Max_Current"));
      Serial.println("timeserver:" + preferences.getString("timeserver"));
      Serial.println("PIC_Active:" + preferences.getString("PIC_Active"));
      Serial.println("MQTT_Active:" + preferences.getString("MQTT_Active"));
      Serial.println("myppp:" + preferences.getString("myppp"));
      Serial.println("ntptz:" + preferences.getString("ntptz"));
      Serial.println("LWILL_TOPIC:" + preferences.getString("LWILL_TOPIC"));
      Serial.println("SUB_TOPIC1:" + preferences.getString("SUB_TOPIC1"));
      Serial.println("GPIO12_TOG:" + preferences.getString("GPIO12_TOG"));
      Serial.println("Copy_IO:" + preferences.getString("Copy_IO"));
      Serial.println("ACS_Active:" + preferences.getString("ACS_Active"));
      Serial.println("tta:" + preferences.getString("tta"));
      Serial.println("Update_now:" + preferences.getString("Update_now"));

  	preferences.end();
  }




  void SaveParams(TConfigParams &ConfParam, Preferences preferences, AsyncWebServerRequest *request){
    preferences.begin("Args_param", false);
      preferences.putString("PIC_Active",  "0" );
      preferences.putString("MQTT_Active",  "0" );
      preferences.putString("GPIO12_TOG",  "0" );
      preferences.putString("Copy_IO",  "0" );
      preferences.putString("ACS_Active",  "0" );
      preferences.putString("myppp",  "0" );
      preferences.putString("Update_now",  "0" );

      int args = request->args();

      for(int i=0;i<args;i++){
        Serial.printf("ARG>>>>>>>>>>>>>[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      }

      for(int i=0;i<args;i++){
        Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
        preferences.putString(request->argName(i).c_str(),  request->arg(i).c_str() );
      }

      if(request->hasParam("PIC_Active")) preferences.putString("PIC_Active",  "1" );
      if(request->hasParam("MQTT_Active")) preferences.putString("MQTT_Active",  "1" );
      if(request->hasParam("GPIO12_TOG")) preferences.putString("GPIO12_TOG",  "1" );
      if(request->hasParam("Copy_IO")) preferences.putString("Copy_IO",  "1" );
      if(request->hasParam("ACS_Active")) preferences.putString("ACS_Active",  "1" );
      if(request->hasParam("myppp")) preferences.putString("myppp",  "1" );
      if(request->hasParam("Update_now")) preferences.putString("Update_now",  "1" );

      ConfParam.v_ssid = preferences.getString("ssid");
      ConfParam.v_pass =  preferences.getString("pass");
  		ConfParam.v_PhyLoc =  preferences.getString("PhyLoc");
  		ConfParam.v_MQTT_BROKER =  preferences.getString("MQTT_BROKER");
  		ConfParam.v_MQTT_B_PRT =  preferences.getString("MQTT_B_PRT");
  		ConfParam.v_PUB_TOPIC1 =  preferences.getString("PUB_TOPIC1");
  		ConfParam.v_FRM_IP =  preferences.getString("FRM_IP");
  		ConfParam.v_FRM_PRT =  preferences.getString("FRM_PRT");
  		ConfParam.v_ASCmultiple =  preferences.getString("ACSmultiple");
  		ConfParam.v_ttl =  preferences.getString("ttl");
  		ConfParam.v_tta =  preferences.getString("tta");
  		ConfParam.v_Max_Current =  preferences.getString("Max_Current");
  		ConfParam.v_timeserver =  preferences.getString("timeserver");
  		ConfParam.v_Pingserver =  preferences.getString("Pingserver");      
  		//ConfParam.v_PIC_Active =  preferences.getString("PIC_Active");
  		ConfParam.v_MQTT_Active =  preferences.getString("MQTT_Active");
  		//ConfParam.v_myppp =  preferences.getString("myppp");
  		ConfParam.v_ntptz =  preferences.getString("ntptz");
  		ConfParam.v_LWILL_TOPIC =  preferences.getString("LWILL_TOPIC");
  		ConfParam.v_SUB_TOPIC1 =  preferences.getString("SUB_TOPIC1");
  		ConfParam.v_ACS_Active =  preferences.getString("ACS_Active");
  		ConfParam.v_Update_now =  preferences.getString("Update_now");

    preferences.end();
  }


  #endif
  #endif
