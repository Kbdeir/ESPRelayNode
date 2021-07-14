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

//#define StepperMode


#ifndef StepperMode
extern InputSensor Inputsnsr14;
extern InputSensor Inputsnsr12;
extern InputSensor Inputsnsr02;
#endif 



#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

AsyncMqttClient mqttClient;

void connectToMqtt() {
  Serial.println(F("\n[MQTT] Connecting"));
  #ifndef DEBUG_DISABLED
  debugV("[MQTT] Connecting");
  #endif 
  mqttClient.setCleanSession(true);
  mqttClient.connect();
}

void tiker_MQTT_CONNECT_func (void* obj) {
  Serial.print("[MQTT] waiting for WIFI ");
  #ifndef DEBUG_DISABLED
  debugV("[MQTT] waiting for WIFI ");
  #endif
  if  (WiFi.status() == WL_CONNECTED)  {
    Serial.print("[MQTT] WIFI CONNECTED - CONNECTING TO MQTT ");
    #ifndef DEBUG_DISABLED
    debugV("[MQTT] WIFI CONNECTED - CONNECTING TO MQTT ");
    #endif
    connectToMqtt();
  }
}
Schedule_timer tiker_MQTT_CONNECT(tiker_MQTT_CONNECT_func,5000,0,MILLIS_);


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
  Serial.println(F("[MQTT] Connected to MQTT."));
  Serial.print(F("[MQTT] Session present: "));
  #ifndef DEBUG_DISABLED
  debugV("[MQTT] Connected to MQTT");
  #endif
  Serial.println(sessionPresent);

	//uint16_t packetIdSub = mqttClient.subscribe(relay0.RelayConfParam->v_SUB_TOPIC1.c_str(), 2);
  Relay * rtemp = nullptr;
  for (auto it : relays)  {
    rtemp = static_cast<Relay *>(it);
    if (rtemp) {
        uint16_t packetIdSub = mqttClient.subscribe(rtemp->RelayConfParam->v_SUB_TOPIC1.c_str(), 2);
    }
  }


	Serial.print(F("[MQTT] Subscribing at QoS 2, packetId: "));
  // Serial.println(packetIdSub);
  // mqttpostinitstatusOfInputs(NULL);

  #ifndef StepperMode
      #if defined (HWver02)  || defined (HWver03)
        [](){
          #if defined (HWver03)
          if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin02) == HIGH ?  ON : OFF);
          }
          #endif

          if (Inputsnsr12.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin12) == HIGH ?  ON : OFF);
          }
          if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
            mqttClient.publish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED,
              digitalRead(InputPin14) == HIGH ?  ON : OFF);
          }

        }();
      #endif
  #endif
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println(F("\n[MQTT] Disconnected"));
  if (WiFi.isConnected()) {
    tiker_MQTT_CONNECT.start(); // recoonect to mqtt server
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println(F("\n[MQTT] Subscribe ack"));
  Serial.print(F("  packetId: "));
  Serial.println(packetId);
  Serial.print(F("  qos: "));
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println(F("\n[MQTT] Unsubscribe ack"));
  Serial.print(F("  packetId: "));
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  Serial.print(F("\n[MQTT] Received"));
  Serial.print(F("  topic: "));
  Serial.print(topic);
	Serial.print(F("  payload: "));
  Serial.print(payload);
  Serial.print(F("  qos: "));
  Serial.print(properties.qos);
  Serial.print(F("  dup: "));
  Serial.print(properties.dup);
  Serial.print(F("  retain: "));
  Serial.print(properties.retain);
  Serial.print(F("  len: "));
  Serial.print(len);

  #ifndef DEBUG_DISABLED
  debugV("[MQTT] received  topic: %s - Payload: %s" , topic, payload);
  #endif

  String temp = String(payload).substring(0,len);
  String pld = String(payload);
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


  /*  for (int i=0; i<MAX_RELAYS; i++){
      rtemp = static_cast<Relay *>(mrelays[i]);
      //if (rtemp) {
        //Serial.println("");
        //Serial.println(rtemp->RelayConfParam->v_PUB_TOPIC1);
      //}
      if (rtemp->RelayConfParam->v_PUB_TOPIC1 == tp) {
        rly = rtemp;
        break;
      }
    }*/

  if (rly) {
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
      // sync mqtt state to actual pin state
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
    }

    if (tp == rly->RelayConfParam->v_i_ttl_PUB_TOPIC) {
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
  Serial.print(F("\n[MQTT] Publish ack"));
  Serial.print(F("  packetId: "));
  Serial.print(packetId);
}
