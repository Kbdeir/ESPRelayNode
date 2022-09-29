#include <defines.h>

#include <MQTT_Processes.h>
#include <JSONConfig.h>
#include <RelayClass.h>
#include <InputClass.h>

#define DEBUG_DISABLED
#ifndef DEBUG_DISABLED
#include <RemoteDebug.h>
extern RemoteDebug Debug;
#endif 

//#include <RelaysArray.h>

//extern void *  mrelays[3];
extern std::vector<void *> relays ; // a list to hold all relays
extern String APssid;
//#define StepperMode


#ifndef StepperMode
  #ifndef HWESP32
  extern InputSensor Inputsnsr14;
  extern InputSensor Inputsnsr13;
  extern InputSensor Inputsnsr12;
  extern InputSensor Inputsnsr02;
  #endif

  #ifdef HWESP32
  #ifndef _emonlib_
  extern InputSensor Inputsnsr01;
  #endif
  #ifdef ESP32_2RBoard
  extern InputSensor Inputsnsr01;
  #endif  

  extern InputSensor Inputsnsr02;
  extern InputSensor Inputsnsr03;
  extern InputSensor Inputsnsr04;
  extern InputSensor Inputsnsr05;
  extern InputSensor Inputsnsr06;
  #endif
#endif 


#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

AsyncMqttClient mqttClient;

void connectToMqtt() {

  #ifndef DEBUG_DISABLED
  debugV("[MQTT] Connecting");
  #endif 
  mqttClient.disconnect();
  mqttClient.setCleanSession(true);
  mqttClient.setServer (MyConfParam.v_MQTT_BROKER.c_str(), MyConfParam.v_MQTT_B_PRT);
  Serial.print(F("\n[MQTT   ] Connecting to broker at "));
  Serial.print( MyConfParam.v_MQTT_BROKER);
  Serial.print(F(" port: "));
  Serial.println( MyConfParam.v_MQTT_B_PRT);  

  if (MyConfParam.v_mqttUser.c_str() != "") {
      mqttClient.setCredentials(MyConfParam.v_mqttUser.c_str(), MyConfParam.v_mqttPass.c_str());
      Serial.print(F("[MQTT   ] user: "));
      Serial.print(MyConfParam.v_mqttUser);
      Serial.print(F(" pass: "));
      Serial.println(MyConfParam.v_mqttPass);      
    }
  mqttClient.connect();
}

void tiker_MQTT_CONNECT_func (void* obj) {

  #ifndef DEBUG_DISABLED
  debugV("[MQTT] waiting for WIFI ");
  #endif
  if  (WiFi.status() == WL_CONNECTED)  {
    Serial.println("[MQTT   ] WIFI IS CONNECTED - CONNECTING TO MQTT ");
    #ifndef DEBUG_DISABLED
    debugV("[MQTT] WIFI CONNECTED - CONNECTING TO MQTT ");
    #endif
    connectToMqtt();
  }
}
Schedule_timer tiker_MQTT_CONNECT(tiker_MQTT_CONNECT_func,10000,0,MILLIS_);


boolean isValidNumber(String& str) {
   for(byte i=0;i<str.length();i++)  {
      if(isDigit(str.charAt(i))) return true;
   }
   return false;
}

/*
void mqttpostinitstatusOfInputs(void* sender){
  if (Inputsnsr12.fclickmode == INPUT_NORMAL) {
    mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, digitalRead(InputPin12) == HIGH ?  ON : OFF);
  }
  if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
    mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, digitalRead(InputPin14) == HIGH ?  ON : OFF);
  }
}
*/

void onMqttConnect(bool sessionPresent) {
  tiker_MQTT_CONNECT.stop();
  Serial.print(F("[MQTT   ] Connected to MQTT - "));
  Serial.print(F("Session present: "));
  Serial.println(sessionPresent);  
  #ifndef DEBUG_DISABLED
  debugV("[MQTT] Connected to MQTT");
  #endif
  String ControllerIDBirth = "/home/Controller" + CID() + "/Birth" ;
  mqttClient.publish( ControllerIDBirth.c_str(), QOS2, false,"online");


	//uint16_t packetIdSub = mqttClient.subscribe(relay0.RelayConfParam->v_SUB_TOPIC1.c_str(), 2);
  Relay * rtemp = nullptr;
  for (auto it : relays)  {
    rtemp = static_cast<Relay *>(it);
    if (rtemp) {
        uint16_t packetIdSub = mqttClient.subscribe(rtemp->RelayConfParam->v_SUB_TOPIC1.c_str(), 2);
    }
  }

  // mqttpostinitstatusOfInputs(NULL);

  #ifndef StepperMode
      
        [](){
          #if defined (HWver03) 
          if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin02) == HIGH ?  ON : OFF);
          }
          #endif

          #ifdef ESP32_2RBoard 
          
            if (Inputsnsr02.fclickmode == INPUT_NORMAL) { 
              mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }         
            
          #endif   

          #if defined (HWver03_4R)
            if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin14) == HIGH ?  ON : OFF);
            }          
            if (Inputsnsr13.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, // ksb change this later
                digitalRead(InputPin13) == HIGH ?  ON : OFF);
            }                  
          #endif
                    
          #if defined (HWver02)  || defined (HWver03)
          if (Inputsnsr12.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin12) == HIGH ?  ON : OFF);
          }
          if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin14) == HIGH ?  ON : OFF);
          }
          #endif

          #if defined (HWESP32)
            #ifndef _emonlib_
            if (Inputsnsr01.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin01_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin01) == HIGH ?  ON : OFF);
            }     
            #endif       
            
            #ifdef ESP32_2RBoard
            
            if (Inputsnsr01.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin01_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin01) == HIGH ?  ON : OFF);
            }    
            
            #endif

            if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin02_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr03.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin03_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
                digitalRead(InputPin03) == HIGH ?  ON : OFF);
            }          
            if (Inputsnsr04.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin04_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, 
                digitalRead(InputPin04) == HIGH ?  ON : OFF);
            }            
            if (Inputsnsr05.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin05_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, 
                digitalRead(InputPin05) == HIGH ?  ON : OFF);
            }       
            if (Inputsnsr06.fclickmode == INPUT_NORMAL) {
              mqttClient.publish( MyConfParam.v_InputPin06_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, 
                digitalRead(InputPin06) == HIGH ?  ON : OFF);
            }                                     
          #endif          

        }();

  #endif
}


void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println(F("[MQTT   ] * Disconnected * "));
  if (WiFi.isConnected()) {
    tiker_MQTT_CONNECT.start(); // recoonect to mqtt server
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.print(F("[MQTT   ] Subscribe ack"));
  Serial.print(F("  packetId: "));
  Serial.print(packetId);
  Serial.print(F("  qos: "));
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.print(F("\n[MQTT   ] Unsubscribe ack"));
  Serial.print(F("  packetId: "));
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  String temp = String(payload).substring(0,len);
  Serial.print(F("[MQTT   ] Received"));
  Serial.print(F("  topic: "));
  Serial.print(topic);
	Serial.print(F("  payload: "));
  Serial.println(temp);

  /*
  Serial.print(F("  qos: "));
  Serial.print(properties.qos);
  Serial.print(F("  dup: "));
  Serial.print(properties.dup);
  Serial.print(F("  retain: "));
  Serial.print(properties.retain);
  Serial.print(F("  len: "));
  Serial.print(len);
  Serial.print(F("\n"));
  */

  #ifndef DEBUG_DISABLED
    debugV("[MQTT] received  topic: %s - Payload: %s" , topic, payload);
  #endif


  String tp = String(topic);

  Relay * rly = nullptr;
  Relay * rtemp = nullptr;

  //Relay * it = NULL;
  //for (std::vector<void *>::iterator it = relays.begin(); it != relays.end(); ++it)
  for (auto it : relays)  {
    rtemp = static_cast<Relay *>(it);
    if (rtemp) {
      if ((rtemp->RelayConfParam->v_PUB_TOPIC1 == tp) ||
          (rtemp->RelayConfParam->v_i_ttl_PUB_TOPIC == tp)) {
            rly = rtemp;
            break;
      }
    }
  }


  /*  
    for (int i=0; i<MAX_RELAYS; i++){
      rtemp = static_cast<Relay *>(mrelays[i]);
      //if (rtemp) {
        //Serial.println("");
        //Serial.println(rtemp->RelayConfParam->v_PUB_TOPIC1);
      //}
      if (rtemp->RelayConfParam->v_PUB_TOPIC1 == tp) {
        rly = rtemp;
        break;
      }
    }
  */

  if (rly) {
    // String temp = String(payload).substring(0,len);
    if (tp == rly->RelayConfParam->v_PUB_TOPIC1) {
      if (temp == ON) {
          rly->mdigitalWrite(rly->getRelayPin(),HIGH);
      } else if (temp == OFF) {
          rly->mdigitalWrite(rly->getRelayPin(),LOW);
      } else if (temp = TOG) {
          rly->mdigitalWrite(rly->getRelayPin(),!rly->readrelay());
      }
    }
    else
    {
      // sync mqtt state to the actual pin state
      mqttClient.publish( rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, (digitalRead(rly->getRelayPin()) == HIGH) ? ON : OFF); 

      // sync mqtt state to the actual pin state
      /*
      if (digitalRead(rly->getRelayPin()) == HIGH) {
          if (temp == OFF) {
            mqttClient.publish( rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, ON);
          }
      }
      if (digitalRead(rly->getRelayPin()) == LOW) {
          if (temp == ON) {
            mqttClient.publish( rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, OFF);
          }
      }
      */
    }

    if (tp == rly->RelayConfParam->v_i_ttl_PUB_TOPIC) {
      String pld = String(payload);
      if (isValidNumber(pld)) {
        //String ttl = pld.substring(0,len);
        rly->RelayConfParam->v_ttl = pld.substring(0,len).toInt();
        rly->ticker_relay_ttl->interval(rly->RelayConfParam->v_ttl*1000);
        saveRelayConfig(rly->RelayConfParam);
        mqttClient.publish( rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), 2, RETAINED,
            String(rly->RelayConfParam->v_ttl).c_str());

           
      }
    }
  }
}


void onMqttPublish(uint16_t packetId) {
  //Serial.println(F("[MQTT   ] Publish ack"));
  //Serial.print(F("  packetId: "));
  //Serial.println(packetId);
}
