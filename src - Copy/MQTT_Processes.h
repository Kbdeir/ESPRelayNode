
#ifndef MQTT_PROCESS_H
#define MQTT_PROCESS_H

  #include <AsyncMqttClient.h>
  #include <ConfigParams.h>
  #include <Scheduletimer.h>

//#define MQTT_HOST IPAddress(192, 168, 30, 111)
  extern AsyncMqttClient mqttClient;
  extern Schedule_timer ticker_relay_ttl;
  extern Schedule_timer tiker_MQTT_CONNECT;

  void onMqttPublish(uint16_t packetId);
  void connectToMqtt() ;
  void onMqttConnect(bool sessionPresent) ;
  void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) ;
  void onMqttSubscribe(uint16_t packetId, uint8_t qos);
  void onMqttUnsubscribe(uint16_t packetId);
  void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
  void tiker_MQTT_CONNECT_func ();
  void mqttpostinitstatusOfInputs(void* sender);

#endif
