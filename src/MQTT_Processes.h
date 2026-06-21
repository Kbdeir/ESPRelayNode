
#ifndef MQTT_PROCESS_H
#define MQTT_PROCESS_H

  #include <AsyncMqttClient.h>
  #include <ConfigParams.h>
  #include <Scheduletimer.h>
  #include <atomic>
  #ifdef ESP32
  #include "lwip/tcpip.h"
  #include "esp_heap_caps.h"
  #endif

  extern AsyncMqttClient mqttClient;
  extern Schedule_timer ticker_relay_ttl;
  extern Schedule_timer tiker_MQTT_CONNECT;

  void onMqttPublish(uint16_t packetId);
  void connectToMqtt() ;
  void mqttConnectNow();
  bool mqttIsEnabled();
  bool mqttWantsVpnRoute();
  void applyMqttActiveState();
  void onMqttConnect(bool sessionPresent) ;
  void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) ;
  void onMqttSubscribe(uint16_t packetId, uint8_t qos);
  void onMqttUnsubscribe(uint16_t packetId);
  void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
  void tiker_MQTT_CONNECT_func (void* obj);
  void mqttpostinitstatusOfInputs(void* sender);
  void postInitialInputStatus();
  void mqttSubscribeRelays();
  void postRelayStates();
  void mqttHealthCheck();
  String mqttHealthTopic();
  const char* mqttWillTopic();  // "/home/Controller<CID>/Birth" - also used as HA discovery availability_topic
  const char* mqttGetClientId();  // returns the cached "ESP32-<CID>" client ID string
  const char* mqttStatusText();
  const char* mqttRouteText();
  extern String mqttHealthTopicStorage;
  void mqttStopForFirmwareUpdate();
  void mqttInvalidateBrokerCache();
  extern volatile bool pending_input_status_publish;
  extern volatile int  pending_relay_state_idx;   // -1=idle, 0..N-1=next relay to publish
  extern volatile bool pending_mqtt_subscribe;
  extern volatile bool mqttConnecting;
  extern volatile bool mqttStoppedForFirmwareUpdate;
  extern std::atomic<bool> suppressRetainedRelayOnAfterBrownout;

// Thread-safe publish wrapper.
// mqttClient.publish() calls AsyncTCP::write() -> tcp_write() (a lwIP function).
// If called from loopTask while async_tcp concurrently runs tcp_update_rcv_ann_wnd(),
// the TCP PCB is accessed without a lock → sequence-number corruption → lwIP assert.
// Holding the lwIP core lock serialises both accesses.
#ifdef ESP32
static inline uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length = 0) {
    // Guard against OOM: AsyncMqttClient queues packets into heap-allocated buffers.
    // If heap is critically low the internal std::vector grow throws std::bad_alloc.
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 4096) return 0;
    LOCK_TCPIP_CORE();
    uint16_t id = length ? mqttClient.publish(topic, qos, retain, payload, length)
                         : mqttClient.publish(topic, qos, retain, payload);
    UNLOCK_TCPIP_CORE();
    return id;
}
#else
static inline uint16_t mqttPublish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length = 0) {
    return length ? mqttClient.publish(topic, qos, retain, payload, length)
                  : mqttClient.publish(topic, qos, retain, payload);
}
#endif

#endif
