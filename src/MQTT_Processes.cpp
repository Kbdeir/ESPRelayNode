#include <defines.h>
#include <SerialLog.h>
#ifdef _REMOTE_SENSORS_
#include <RemoteSensorConfig.h>
#endif

#ifdef ESP32
#include <esp_task_wdt.h>
#endif

#include <MQTT_Processes.h>
#include <JSONConfig.h>
#include <RelayClass.h>
#include <InputClass.h>
#include <WireGuardManager.h>


#ifdef DEBUG_ENABLED
#include <RemoteDebug.h>
extern RemoteDebug Debug;
#endif
#ifdef OLED_1306
extern void oledWake();
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
  #ifdef InputPin01
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
static String mqttClientIdStorage;
static String mqttWillTopicStorage;
String mqttHealthTopicStorage;
static char mqttHealthPayloadStorage[32];

static const char* mqttDisconnectReasonText(AsyncMqttClientDisconnectReason reason) {
  switch (reason) {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
      return "TCP_DISCONNECTED";
    case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
      return "MQTT_UNACCEPTABLE_PROTOCOL_VERSION";
    case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
      return "MQTT_IDENTIFIER_REJECTED";
    case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
      return "MQTT_SERVER_UNAVAILABLE";
    case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
      return "MQTT_MALFORMED_CREDENTIALS";
    case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
      return "MQTT_NOT_AUTHORIZED";
    case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
      return "ESP8266_NOT_ENOUGH_SPACE";
    case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
      return "TLS_BAD_FINGERPRINT";
    default:
      return "UNKNOWN";
  }
}

static bool mqttCredentialIsSet(const String& value) {
  return !value.isEmpty() && !value.equalsIgnoreCase(F("null"));
}



// True while a TCP/MQTT handshake is in progress.
// Guards against sending a second CONNECT packet (which causes the broker to
// close the TCP socket -> TCP_DISCONNECTED) if the reconnect timer fires while
// a connection attempt is already underway.
volatile bool mqttConnecting = false;
volatile bool mqttStoppedForFirmwareUpdate = false;
static uint32_t mqttBackoffMs = 10000;
volatile bool pending_mqtt_subscribe = false;
volatile bool pending_input_status_publish = false;
volatile int  pending_relay_state_idx = -1;   // -1=idle, 0..N-1=next relay to publish
std::atomic<bool> suppressRetainedRelayOnAfterBrownout{false};
// Written from the AsyncTCP/MQTT callback task (onMqttMessage) and read/written
// from the Arduino loop task (mqttHealthCheck) — must be volatile so neither
// side caches a stale value.  Without volatile we saw spurious "Health echo
// missed" while the broker was actually echoing back fine.
static volatile bool mqttHealthAwaitingEcho = false;
static volatile uint8_t mqttHealthMisses = 0;
static uint32_t mqttLastHealthCheckMs = 0;
static uint32_t mqttLastReconnectAttemptMs = 0;
static uint32_t mqttConnectionStartedAtMs = 0;
static const uint8_t mqttHealthMaxMisses = 5;  // 5 × keepalive interval before disconnect; extra tolerance for VPN jitter
static bool mqttActiveRouteUsesVPN = false;
static bool mqttLastRouteDecisionUsesVPN = false;
static bool mqttRouteLogValid = false;
static bool mqttLoggedRouteUsesVPN = false;
static IPAddress mqttLoggedBrokerIp;

// Cached broker-IP resolution (WiFi DNS path) to avoid repeated blocking DNS calls.
static IPAddress mqttCachedBrokerIp;
static bool mqttBrokerIpCacheValid = false;
static String mqttCachedBrokerHost;

// One-time flag for AsyncMqttClient params that never change at runtime.
// File-scope (not static-local) so mqttInvalidateBrokerCache() can clear it
// if the user changes broker/credentials config at runtime.
static bool mqttClientParamsSet = false;

void mqttInvalidateBrokerCache() {
  mqttBrokerIpCacheValid = false;
  mqttCachedBrokerHost = "";
  mqttCachedBrokerIp = IPAddress(0, 0, 0, 0);
  mqttRouteLogValid = false;
  mqttClientParamsSet = false;  // force setServer/setCredentials re-apply on next connect
}

bool mqttIsEnabled() {
  return MyConfParam.v_MQTT_Active == 1;
}

void mqttStopForFirmwareUpdate() {
  if (mqttStoppedForFirmwareUpdate) return;

  mqttStoppedForFirmwareUpdate = true;
  mqttConnecting = false;
  mqttConnectionStartedAtMs = 0;
  pending_mqtt_subscribe = false;
  pending_input_status_publish = false;
  pending_relay_state_idx = -1;
  mqttHealthAwaitingEcho = false;
  mqttHealthMisses = 0;
  tiker_MQTT_CONNECT.stop();

  if (mqttClient.connected()) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Firmware update started - disconnecting MQTT");
    mqttClient.disconnect();
  } else {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Firmware update started - MQTT stopped");
  }
}

static uint32_t mqttIpToU32(const IPAddress& ip) {
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

static bool mqttIpIsOnWifiSubnet(const IPAddress& ip) {
  if (WiFi.status() != WL_CONNECTED) return false;

  const uint32_t target = mqttIpToU32(ip);
  const uint32_t local = mqttIpToU32(WiFi.localIP());
  const uint32_t mask = mqttIpToU32(WiFi.subnetMask());

  if (mask != 0 && mask != 0xFFFFFFFF && ((target & mask) == (local & mask))) {
    return true;
  }

  return ip[0] == WiFi.localIP()[0] &&
         ip[1] == WiFi.localIP()[1] &&
         ip[2] == WiFi.localIP()[2];
}

static bool mqttBrokerIsOnWifiSubnet(IPAddress& brokerIp) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Fast path: numeric IP in config — parse directly, no DNS needed.
  if (brokerIp.fromString(MyConfParam.v_MQTT_BROKER)) {
    return mqttIpIsOnWifiSubnet(brokerIp);
  }

  // Use cached resolution when the broker hostname hasn't changed.
  // The cache is invalidated by mqttInvalidateBrokerCache() on config changes.
  if (mqttBrokerIpCacheValid && mqttCachedBrokerHost == MyConfParam.v_MQTT_BROKER) {
    brokerIp = mqttCachedBrokerIp;
    return mqttIpIsOnWifiSubnet(brokerIp);
  }

  // Blocking DNS resolve — switch to WiFi route first so we use WiFi DNS.
  // WiFi.hostByName() can block up to ~8 s (lwIP DNS timeout). The ESP32 task
  // watchdog fires at 5 s, so we must reset it before and after the call.
  wireGuardUseWifiRoute("MQTT broker DNS");
  #ifdef ESP32
  esp_task_wdt_reset();
  #endif
  const bool resolved = WiFi.hostByName(MyConfParam.v_MQTT_BROKER.c_str(), brokerIp);
  #ifdef ESP32
  esp_task_wdt_reset();
  #endif
  if (!resolved) {
    SLOG_IF(SLOG_MQTT) { Serial.print(F("[MQTT   ] Could not resolve broker on WiFi route: ")); Serial.println(MyConfParam.v_MQTT_BROKER); }
    return false;
  }

  // Cache the result so subsequent calls skip the blocking resolve.
  mqttCachedBrokerHost = MyConfParam.v_MQTT_BROKER;
  mqttCachedBrokerIp = brokerIp;
  mqttBrokerIpCacheValid = true;

  return mqttIpIsOnWifiSubnet(brokerIp);
}

static bool mqttShouldUseVpnRoute(IPAddress& brokerIp) {
  if (MyConfParam.v_MQTT_BROKER.length() > 0) {
    brokerIp.fromString(MyConfParam.v_MQTT_BROKER);
  } else {
    brokerIp = IPAddress(0, 0, 0, 0);
  }

  if (MyConfParam.v_MQTT_UseVPN != 1) {
    return false;
  }

  return !mqttBrokerIsOnWifiSubnet(brokerIp);
}

bool mqttWantsVpnRoute() {
  if (!mqttIsEnabled()) return false;

  IPAddress brokerIp;
  return mqttShouldUseVpnRoute(brokerIp);
}

static void mqttLogRouteDecision(bool useVpn, const IPAddress& brokerIp) {
  if (mqttRouteLogValid && mqttLoggedRouteUsesVPN == useVpn && mqttLoggedBrokerIp == brokerIp) {
    return;
  }

  mqttRouteLogValid = true;
  mqttLoggedRouteUsesVPN = useVpn;
  mqttLoggedBrokerIp = brokerIp;

  SLOG_IF(SLOG_MQTT) {
    Serial.print(F("[MQTT   ] Broker route: "));
    Serial.print(useVpn ? F("VPN") : F("WiFi"));
    if (brokerIp != IPAddress(0, 0, 0, 0)) {
      Serial.print(F(" ("));
      Serial.print(brokerIp);
      Serial.print(F(")"));
    }
    Serial.println();
    if (useVpn) {
      Serial.print(F("[MQTT   ] WiFi IP/mask: "));
      Serial.print(WiFi.localIP());
      Serial.print(F(" / "));
      Serial.println(WiFi.subnetMask());
    }
  }
}

static bool mqttRouteIsReady() {
  IPAddress brokerIp;
  mqttLastRouteDecisionUsesVPN = mqttShouldUseVpnRoute(brokerIp);
  mqttLogRouteDecision(mqttLastRouteDecisionUsesVPN, brokerIp);

  if (!mqttLastRouteDecisionUsesVPN) {
    return wireGuardUseWifiRoute("MQTT");
  }

  if (!wireGuardIsEnabled()) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] VPN route requested but WireGuard tunnel is disabled");
    return false;
  }

  if (!wireGuardIsNetworkUp()) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Waiting for WireGuard tunnel before MQTT connection");
    wireGuardEnsureStarted();
    return false;
  }

  const bool routeReady = wireGuardUseVpnRoute("MQTT");
  if (routeReady) {
    wireGuardLogDefaultRoute("MQTT");
  }
  return routeReady;
}

static void mqttResetStuckConnectionAttempt(const __FlashStringHelper *reason) {
  if (!mqttConnecting) return;
  SLOG_IF(SLOG_MQTT) { Serial.print(F("[MQTT   ] ")); Serial.println(reason); }
  mqttClient.disconnect(true);
  mqttConnecting = false;
  mqttConnectionStartedAtMs = 0;
  pending_mqtt_subscribe = false;
  pending_input_status_publish = false;
  mqttHealthAwaitingEcho = false;
}

// Initialize all per-board MQTT topic Strings from CID() exactly once.
// CID() is derived from the hardware eFuse MAC and never changes at runtime,
// so recomputing and reallocating these on every reconnect attempt is wasted
// heap churn that fragments the allocator over a long uptime.
static void mqttEnsureTopicsInitialized() {
  if (!mqttClientIdStorage.isEmpty()) return;
  String cid = CID();
  mqttClientIdStorage    = "ESP32-" + cid;
  mqttWillTopicStorage   = "/home/Controller" + cid + "/Birth";
  mqttHealthTopicStorage = "/home/Controller" + cid + "/Health";
}

String mqttHealthTopic() {
  mqttEnsureTopicsInitialized();
  return mqttHealthTopicStorage;
}

// Birth/LWT topic ("/home/Controller<CID>/Birth"), doubling as the
// availability_topic for HA discovery: "online" is published retained on
// connect, "offline" is the broker-issued LWT on disconnect.
const char* mqttWillTopic() {
  mqttEnsureTopicsInitialized();
  return mqttWillTopicStorage.c_str();
}

const char* mqttGetClientId() {
  mqttEnsureTopicsInitialized();
  return mqttClientIdStorage.c_str();
}

const char* mqttStatusText() {
  if (!mqttIsEnabled()) return "Disabled";
  if (mqttStoppedForFirmwareUpdate) return "Firmware Update";
  if (mqttClient.connected()) return "Connected";
  if (mqttConnecting) return "Connecting";
  return "Disconnected";
}

const char* mqttRouteText() {
  if (!mqttIsEnabled()) return "Off";
  if (mqttStoppedForFirmwareUpdate) return "Stopped";
  if (mqttClient.connected()) return mqttActiveRouteUsesVPN ? "VPN" : "Local WiFi";
  IPAddress brokerIp;
  const bool useVpn = mqttShouldUseVpnRoute(brokerIp);
  mqttLastRouteDecisionUsesVPN = useVpn;
  return useVpn ? "VPN" : "Local WiFi";
}

void applyMqttActiveState() {
  if (mqttStoppedForFirmwareUpdate) return;

  // Flush the broker-IP cache so the next connection re-resolves with fresh config.
  mqttInvalidateBrokerCache();

  if (!mqttIsEnabled()) {
    pending_mqtt_subscribe = false;
    pending_input_status_publish = false;
    pending_relay_state_idx = -1;
    mqttHealthAwaitingEcho = false;
    mqttHealthMisses = 0;
    mqttConnecting = false;
    tiker_MQTT_CONNECT.stop();
    if (mqttClient.connected()) {
      SLOGLNP(SLOG_MQTT, "[MQTT   ] Disabled by config - disconnecting");
      mqttClient.disconnect();
    } else {
      SLOGLNP(SLOG_MQTT, "[MQTT   ] Disabled by config");
    }
    return;
  }

  IPAddress brokerIp;
  if (mqttClient.connected() && mqttActiveRouteUsesVPN != mqttShouldUseVpnRoute(brokerIp)) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Route setting changed - reconnecting");
    mqttClient.disconnect();
    mqttBackoffMs = 10000;
    tiker_MQTT_CONNECT.interval(mqttBackoffMs);
    tiker_MQTT_CONNECT.start();
    return;
  }

  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected() && !mqttConnecting) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Enabled by config - starting reconnect timer");
    mqttBackoffMs = 10000;
    tiker_MQTT_CONNECT.interval(mqttBackoffMs);
    tiker_MQTT_CONNECT.start();
  }
}

void connectToMqtt() {
  if (mqttStoppedForFirmwareUpdate) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Firmware update in progress - skipping connect");
    return;
  }

  if (!mqttIsEnabled()) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Disabled - skipping connect");
    applyMqttActiveState();
    return;
  }

  if (!mqttRouteIsReady()) {
    // Record the attempt time so mqttHealthCheck() doesn't flood on every loop
    // iteration while the VPN tunnel is still coming up.
    mqttLastReconnectAttemptMs = millis();
    mqttBackoffMs = min(mqttBackoffMs * 2, (uint32_t)300000);
    tiker_MQTT_CONNECT.interval(mqttBackoffMs);
    tiker_MQTT_CONNECT.start();
    return;
  }

  if (mqttClient.connected()) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Already connected - skipping");
    mqttConnecting = false;
    mqttConnectionStartedAtMs = 0;
    tiker_MQTT_CONNECT.stop();
    return;
  }
  if (mqttConnecting && mqttConnectionStartedAtMs != 0 && millis() - mqttConnectionStartedAtMs > 60000UL) {
    mqttResetStuckConnectionAttempt(F("Previous connection attempt timed out - closing socket"));
  }
  if (mqttConnecting) {
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Connection already in progress - skipping");
    return;
  }

  #ifdef DEBUG_ENABLED
  debugV("[MQTT] Connecting");
  #endif

  mqttConnecting = true;
  mqttConnectionStartedAtMs = millis();
  mqttLastReconnectAttemptMs = mqttConnectionStartedAtMs;
  mqttActiveRouteUsesVPN = mqttLastRouteDecisionUsesVPN;

  // Set all AsyncMqttClient params once per config epoch.
  // setClientId/setWill/setCleanSession/setServer/setCredentials each touch the
  // library's internal heap buffers.  Calling them on every reconnect attempt
  // causes repeated alloc+free cycles that fragment the heap and can eventually
  // starve the library's packet-assembly buffers, causing TCP_DISCONNECTED.
  // mqttClientParamsSet is cleared by mqttInvalidateBrokerCache() if config changes.
  mqttEnsureTopicsInitialized();
  if (!mqttClientParamsSet) {
    mqttClientParamsSet = true;
    mqttClient.setClientId(mqttClientIdStorage.c_str());
    mqttClient.setWill(mqttWillTopicStorage.c_str(), 1, true, "offline");
    mqttClient.setCleanSession(true);
    mqttClient.setServer(MyConfParam.v_MQTT_BROKER.c_str(), MyConfParam.v_MQTT_B_PRT);
    if (mqttCredentialIsSet(MyConfParam.v_mqttUser)) {
      mqttClient.setCredentials(MyConfParam.v_mqttUser.c_str(), MyConfParam.v_mqttPass.c_str());
      SLOG_IF(SLOG_MQTT) { Serial.print(F("[MQTT   ] user: ")); Serial.println(MyConfParam.v_mqttUser); }
    } else {
      mqttClient.setCredentials(nullptr, nullptr);
      SLOGLNP(SLOG_MQTT, "[MQTT   ] anonymous login");
    }
  }
  mqttClient.setKeepAlive(MyConfParam.v_MQTT_KeepAliveSeconds);

  SLOG(SLOG_MQTT, "\n[MQTT   ] Connecting to %s:%d via %s (WiFi: %s/%s) clientId: %s\n",
    MyConfParam.v_MQTT_BROKER.c_str(),
    MyConfParam.v_MQTT_B_PRT,
    mqttLastRouteDecisionUsesVPN ? "VPN" : "WiFi",
    WiFi.localIP().toString().c_str(),
    WiFi.subnetMask().toString().c_str(),
    mqttClientIdStorage.c_str());
  wireGuardLogDefaultRoute("MQTT connect");
  mqttClient.connect();
}

void mqttConnectNow() {
  if (mqttStoppedForFirmwareUpdate) return;

  if (!mqttIsEnabled()) {
    applyMqttActiveState();
    return;
  }

  if (!mqttRouteIsReady()) {
    return;
  }

  if (mqttClient.connected()) {
    mqttActiveRouteUsesVPN = mqttLastRouteDecisionUsesVPN;
    mqttConnecting = false;
    mqttConnectionStartedAtMs = 0;
    tiker_MQTT_CONNECT.stop();
    return;
  }

  if (mqttConnecting) {
    return;
  }

  mqttBackoffMs = 10000;
  tiker_MQTT_CONNECT.stop();
  tiker_MQTT_CONNECT.interval(mqttBackoffMs);
  connectToMqtt();
}

void tiker_MQTT_CONNECT_func (void* obj) {
  if (mqttStoppedForFirmwareUpdate) {
    tiker_MQTT_CONNECT.stop();
    return;
  }

  if (!mqttIsEnabled()) {
    applyMqttActiveState();
    return;
  }
  if (mqttClient.connected()) {
    mqttConnecting = false;
    mqttConnectionStartedAtMs = 0;
    tiker_MQTT_CONNECT.stop();
    return;
  }
  #ifdef DEBUG_ENABLED
  debugV("[MQTT] waiting for WIFI ");
  #endif
  if (WiFi.status() == WL_CONNECTED) {
    SLOG(SLOG_MQTT, "\n[MQTT   ] WIFI connected - connecting to MQTT (backoff %ums)\n", mqttBackoffMs);
    #ifdef DEBUG_ENABLED
    debugV("[MQTT] WIFI CONNECTED - CONNECTING TO MQTT ");
    #endif
    connectToMqtt();
    // Backoff doubling is centralised:
    //   - connectToMqtt() doubles on route-not-ready (VPN down)
    //   - onMqttDisconnect() doubles on a failed TCP/MQTT handshake
    // Doubling here would triple-double the backoff in a single cycle when this
    // ticker, mqttHealthCheck() and connectToMqtt() all run during the same loop.
  }
}
Schedule_timer tiker_MQTT_CONNECT(tiker_MQTT_CONNECT_func,10000,0,MILLIS_);


boolean isValidNumber(String& str) {
   if (str.length() == 0) return false;
   for(byte i=0;i<str.length();i++)  {
      if(!isDigit(str.charAt(i))) return false;
   }
   return true;
}

/*
void mqttpostinitstatusOfInputs(void* sender){
  if (Inputsnsr12.fclickmode == INPUT_NORMAL) {
    mqttPublish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), 0, RETAINED, digitalRead(InputPin12) == HIGH ?  ON : OFF);
  }
  if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
    mqttPublish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), 0, RETAINED, digitalRead(InputPin14) == HIGH ?  ON : OFF);
  }
}
*/

// Set by onMqttConnect; consumed in loop() to subscribe and publish initial state
// from the Arduino task, not from the AsyncMqttClient callback task.
void onMqttConnect(bool sessionPresent) {
  #ifdef OLED_1306
  oledWake();
  #endif
  if (mqttStoppedForFirmwareUpdate) {
    mqttConnecting = false;
    mqttClient.disconnect();
    return;
  }

  if (!mqttIsEnabled()) {
    mqttConnecting = false;
    applyMqttActiveState();
    return;
  }
  mqttConnecting = false;
  mqttConnectionStartedAtMs = 0;
  tiker_MQTT_CONNECT.stop();
  mqttBackoffMs = 10000;
  tiker_MQTT_CONNECT.interval(mqttBackoffMs);
  mqttHealthAwaitingEcho = false;
  mqttHealthMisses = 0;
  mqttLastHealthCheckMs = millis();

  SLOG_IF(SLOG_MQTT) { Serial.print(F("[MQTT   ] Connected to MQTT - session present: ")); Serial.println(sessionPresent); }
  #ifdef DEBUG_ENABLED
  debugV("[MQTT] Connected to MQTT");
  #endif

  // Defer subscriptions to loop() - processed by mqttSubscribeRelays() on the
  // Arduino task after the broker has settled. Do NOT publish initial input
  // states here: that burst of 6+ retained QoS-1 publishes immediately after
  // CONNACK overwhelms simple embedded brokers and causes TCP_DISCONNECTED.
  // Input and relay state retained messages are maintained continuously during
  // normal operation, so the broker already has current values.
  pending_mqtt_subscribe = true;
  pending_relay_state_idx = 0;
}

void mqttSubscribeRelays() {
  if (!mqttIsEnabled() || !mqttClient.connected()) return;
  // Birth message: QoS 0 so the broker stores it (retained=true) without
  // needing to send a PUBACK - removes one packet exchange from the burst.
  // Reuse mqttWillTopicStorage (same topic) — avoids a heap allocation.
  mqttEnsureTopicsInitialized();
  mqttPublish(mqttWillTopicStorage.c_str(), 0, true, "online");

  // mqttHealthTopicStorage was already filled by mqttEnsureTopicsInitialized().
  mqttClient.subscribe(mqttHealthTopicStorage.c_str(), 1);

  Relay * rtemp = nullptr;
  for (auto it : relays) {
    rtemp = static_cast<Relay *>(it);
    if (rtemp != nullptr) {
      mqttClient.subscribe(rtemp->RelayConfParam->v_SUB_TOPIC1.c_str(), 1);
    }
  }
#ifdef _REMOTE_SENSORS_
  // Remote sensor subscriptions are QoS 0: we only need the latest retained
  // value, not guaranteed delivery, and QoS 0 avoids PUBACK round-trips.
  remoteSensorsMqttSubscribe();
#endif
}

void postInitialInputStatus() {
  if (!mqttIsEnabled() || !mqttClient.connected()) return;
  #ifndef StepperMode
          #if defined (HWver03)
            Serial.print(F("\n[INFO   ]  MQTT posting initial status of Inputs"));
            if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }
          #endif

          #if defined (HWver03_4R)
            if (Inputsnsr02.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin14) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr13.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin13) == HIGH ?  ON : OFF);
            }
          #endif

          #if defined (HWver02)  || defined (HWver03)
          if (Inputsnsr12.fclickmode == INPUT_NORMAL) {
            mqttPublish( MyConfParam.v_InputPin12_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
              digitalRead(InputPin12) == HIGH ?  ON : OFF);
          }
          #if defined (MQTTPostInitStatus)
          if (Inputsnsr14.fclickmode == INPUT_NORMAL) {
            mqttPublish( MyConfParam.v_InputPin14_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
              digitalRead(InputPin14) == HIGH ?  ON : OFF);
          }
          #endif
          #endif

          #if defined (HWESP32)
            #ifdef InputPin01
            if (MyConfParam.v_IN1_INPUTMODE != INPUT_TEMPERATURE && Inputsnsr01.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin01_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin01) == HIGH ?  ON : OFF);
            }
            #endif

            if (MyConfParam.v_IN2_INPUTMODE != INPUT_TEMPERATURE && Inputsnsr02.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin02_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin02) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr03.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin03_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin03) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr04.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin04_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin04) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr05.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin05_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin05) == HIGH ?  ON : OFF);
            }
            if (Inputsnsr06.fclickmode == INPUT_NORMAL) {
              mqttPublish( MyConfParam.v_InputPin06_STATE_PUB_TOPIC.c_str(), 0, RETAINED,
                digitalRead(InputPin06) == HIGH ?  ON : OFF);
            }
          #endif
  #endif
}

void postRelayStates() {
  if (!mqttIsEnabled() || !mqttClient.connected()) return;
  for (auto it : relays) {
    Relay* rly = static_cast<Relay*>(it);
    if (!rly) continue;
    const bool on = rly->readrelay() == HIGH;
    mqttPublish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, on ? ON : OFF);
    mqttPublish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(),      QOS2, RETAINED, on ? ON : OFF);
    if (rly->RelayConfParam->v_ttl > 0) {
      char ttlBuf[12];
      snprintf(ttlBuf, sizeof(ttlBuf), "%d", rly->RelayConfParam->v_ttl);
      mqttPublish(rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), QOS2, RETAINED, ttlBuf);
    }
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  #ifdef OLED_1306
  oledWake();
  #endif
  mqttConnecting = false;        // reset so the next attempt is allowed
  mqttConnectionStartedAtMs = 0;
  pending_mqtt_subscribe = false;
  pending_input_status_publish = false;
  pending_relay_state_idx = -1;
  mqttHealthAwaitingEcho = false;
  mqttHealthMisses = 0;
  SLOG_IF(SLOG_MQTT) {
    Serial.print(F("[MQTT   ] * Disconnected * reason: "));
    Serial.print(mqttDisconnectReasonText(reason));
    Serial.print(F(" ("));
    Serial.print(static_cast<uint8_t>(reason));
    Serial.println(F(")"));
  }
  if (mqttStoppedForFirmwareUpdate) {
    return;
  }

  if (mqttIsEnabled() && WiFi.isConnected()) {
    // A real failed attempt — advance the backoff once here.
    mqttBackoffMs = min(mqttBackoffMs * 2, (uint32_t)300000);
    SLOG(SLOG_MQTT, "[MQTT   ] Reconnect scheduled in %ums (backoff)\n", mqttBackoffMs);
    tiker_MQTT_CONNECT.interval(mqttBackoffMs);
    tiker_MQTT_CONNECT.start();
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  SLOG_IF(SLOG_MQTT) {
    Serial.print(F("\n[MQTT   ] Subscribe ack"));
    Serial.print(F("  packetId: "));
    Serial.print(packetId);
    Serial.print(F("  qos: "));
    Serial.print(qos);
  }
}

void onMqttUnsubscribe(uint16_t packetId) {
  SLOG_IF(SLOG_MQTT) {
    Serial.print(F("\n[MQTT   ] Unsubscribe ack"));
    Serial.print(F("  packetId: "));
    Serial.println(packetId);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (mqttStoppedForFirmwareUpdate) return;
  if (!mqttIsEnabled()) return;

  // H1: avoid heap String allocations on every MQTT message — use C-string
  // comparisons for topic and strncmp/len for payload.

  #ifdef DEBUG_ENABLED
  //  debugV("[MQTT] received  topic: %s - Payload: %s" , topic, payload);
  #endif

  if (mqttHealthTopicStorage == topic) {
    // Any message echoed on /home/Controller<CID>/Health proves the broker is
    // alive and forwarding our own publishes back to us.  Don't compare against
    // mqttHealthPayloadStorage — the loop task overwrites that buffer when it
    // emits the next health check, so a slightly-late echo for the previous
    // publish would otherwise look like a miss even though the broker echoed
    // fine.  The topic is private to this controller, so any echo is genuine.
    SLOG(SLOG_MQTT, "[MQTT   ] Health echo received (payload=%.*s)\n", (int)len, payload);
    mqttHealthAwaitingEcho = false;
    mqttHealthMisses = 0;
    return;
  }

#ifdef _REMOTE_SENSORS_
  // Update cached value for any remote sensor slot whose topic matches.
  // Runs on the async_tcp task — no heap allocation (see RemoteSensorConfig.cpp).
  remoteSensorsOnMqttMessage(topic, payload, len);
#endif

  Relay * rly = nullptr;
  Relay * rtemp = nullptr;

  //Relay * it = NULL;
  //for (std::vector<void *>::iterator it = relays.begin(); it != relays.end(); ++it)
  for (auto it : relays)  {
    rtemp = static_cast<Relay *>(it);
    if (rtemp != nullptr) {
      if ((rtemp->RelayConfParam->v_STATE_PUB_TOPIC == topic) || (rtemp->RelayConfParam->v_CURR_TTL_PUB_TOPIC == topic)) {
          return;
      }
      if ((rtemp->RelayConfParam->v_PUB_TOPIC1 == topic) || (rtemp->RelayConfParam->v_i_ttl_PUB_TOPIC == topic)) {
          rly = rtemp;
          break;
      }
    }
  }

  // Payload comparisons without heap String — payload is NOT null-terminated.
  const bool payloadIsOn  = (len == strlen(ON)  && strncmp(payload, ON,  len) == 0);
  const bool payloadIsOff = (len == strlen(OFF) && strncmp(payload, OFF, len) == 0);
  const bool payloadIsTog = (len == strlen(TOG) && strncmp(payload, TOG, len) == 0);

  if (rly) {
      if (rly->RelayConfParam->v_PUB_TOPIC1 == topic) {
        // v_PUB_TOPIC1 is also our own command topic, so the broker echoes
        // back whatever we just published on a relay state change. Swallow
        // as many ON/OFF echoes as we've published - otherwise an echo
        // re-applies to the relay, re-triggers the state-change handler, and
        // the relay cycles on/off forever (especially under rapid
        // consecutive toggles, where echoes can arrive out of order).
        if ((payloadIsOn || payloadIsOff) && rly->pendingEchoCount > 0) {
          rly->pendingEchoCount--;
          return;
        }
        if (payloadIsOn) {
            #ifdef OLED_1306
            oledWake();
            #endif
            if (suppressRetainedRelayOnAfterBrownout.load(std::memory_order_acquire) && properties.retain) {
              SLOGLNP(SLOG_MQTT, "[MQTT   ] Ignoring retained relay ON after brownout reset");
              rly->mdigitalWrite(rly->getRelayPin(), LOW);
              mqttPublish(rly->RelayConfParam->v_PUB_TOPIC1.c_str(), QOS2, RETAINED, OFF);
              mqttPublish(rly->RelayConfParam->v_STATE_PUB_TOPIC.c_str(), QOS2, RETAINED, OFF);
              return;
            }
            rly->mdigitalWrite(rly->getRelayPin(),HIGH);
        } else
        if (payloadIsOff) {
            #ifdef OLED_1306
            oledWake();
            #endif
            rly->stop_ttl_timer();
            rly->mdigitalWrite(rly->getRelayPin(),LOW);
        } else
        if (payloadIsTog) {
            #ifdef OLED_1306
            oledWake();
            #endif
            rly->mdigitalWrite(rly->getRelayPin(),!rly->readrelay());
        }
      }

      if (rly->RelayConfParam->v_i_ttl_PUB_TOPIC == topic) {
        char pldBuf[16];
        size_t pldLen = (len < sizeof(pldBuf) - 1) ? len : sizeof(pldBuf) - 1;
        memcpy(pldBuf, payload, pldLen);
        pldBuf[pldLen] = '\0';
        // Validate digits without constructing a heap-allocated String.
        bool valid = (pldLen > 0);
        for (size_t _i = 0; _i < pldLen && valid; _i++) if (!isDigit((unsigned char)pldBuf[_i])) valid = false;
        if (valid) {
          int newTtl = atoi(pldBuf);
          if (rly->RelayConfParam->v_ttl != newTtl) {
            rly->RelayConfParam->v_ttl = newTtl;
            rly->ticker_relay_ttl->interval(rly->RelayConfParam->v_ttl*1000);
            #ifdef ESP32
            // Also update the running FreeRTOS timer period - on ESP32 the
            // Schedule_timer interval() call above is a no-op for the xTimer.
            xTimerChangePeriod(rly->ttltimer,
                               pdMS_TO_TICKS(rly->RelayConfParam->v_ttl * 1000), 0);
            #endif
            saveRelayConfig(rly->RelayConfParam);
            { char ttlBuf[12]; snprintf(ttlBuf, sizeof(ttlBuf), "%d", rly->RelayConfParam->v_ttl); mqttPublish(rly->RelayConfParam->v_ttl_PUB_TOPIC.c_str(), 2, RETAINED, ttlBuf); }
          }      
        }
      }

  }
}


void onMqttPublish(uint16_t packetId) {
  //Serial.println(F("[MQTT   ] Publish ack"));
  //Serial.print(F("  packetId: "));
  //Serial.println(packetId);
}

void mqttHealthCheck() {
  if (mqttStoppedForFirmwareUpdate) {
    mqttConnecting = false;
    pending_mqtt_subscribe = false;
    pending_input_status_publish = false;
    pending_relay_state_idx = -1;
    mqttHealthAwaitingEcho = false;
    mqttHealthMisses = 0;
    tiker_MQTT_CONNECT.stop();
    return;
  }

  if (!mqttIsEnabled()) {
    mqttHealthAwaitingEcho = false;
    mqttHealthMisses = 0;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    mqttHealthAwaitingEcho = false;
    mqttHealthMisses = 0;
    return;
  }

  if (!mqttClient.connected()) {
    mqttHealthAwaitingEcho = false;
    const uint32_t now = millis();
    if (mqttConnecting && mqttConnectionStartedAtMs != 0 && now - mqttConnectionStartedAtMs > 60000UL) {
      mqttResetStuckConnectionAttempt(F("Connection attempt timed out - closing socket and scheduling reconnect"));
    }
    if (!mqttConnecting) {
      // CRITICAL: Schedule_timer::start() resets lastTime = millis().  If we
      // called start() every loop iteration while disconnected, the timer
      // could NEVER reach its interval — the countdown was perpetually being
      // moved forward and the ticker never fired.  Only (re)start it when it
      // is currently stopped, so a previously armed countdown is preserved.
      if (tiker_MQTT_CONNECT.state() != RUNNING_) {
        tiker_MQTT_CONNECT.interval(mqttBackoffMs);
        tiker_MQTT_CONNECT.start();
      }
    }
    return;
  }

  tiker_MQTT_CONNECT.stop();
  mqttConnecting = false;
  mqttConnectionStartedAtMs = 0;

  // Do NOT call mqttRouteIsReady() here.  The TCP socket is already bound to
  // the interface it connected through; re-validating the route every health
  // tick would skip the ping during transient wireGuardIsNetworkUp() flickers
  // (e.g. WireGuard session renewal) and the user sees nothing happening.

  const uint32_t now = millis();
  // Fire at 2/3 of the keepalive interval so the health PUBLISH always arrives at
  // the broker well before its 1.5× keepalive timeout — even when the broker uses
  // a shorter effective keepalive than what we advertised in the CONNECT packet
  // (common with broker-side max_keepalive settings), and even with VPN latency.
  // Example: keepalive=30s → ping every 20s → broker tolerates up to 45s.
  uint32_t mqttHealthIntervalMs = (uint32_t)MyConfParam.v_MQTT_KeepAliveSeconds * 667UL;
  if (mqttHealthIntervalMs < 5000UL) mqttHealthIntervalMs = 5000UL;
  if (now - mqttLastHealthCheckMs < mqttHealthIntervalMs) {
    return;
  }
  mqttLastHealthCheckMs = now;

  if (mqttHealthAwaitingEcho) {
    mqttHealthMisses++;
    SLOG(SLOG_MQTT, "\n[MQTT   ] Health echo missed %u/%u\n", mqttHealthMisses, mqttHealthMaxMisses);
    if (mqttHealthMisses >= mqttHealthMaxMisses) {
      SLOGLNP(SLOG_MQTT, "[MQTT   ] Health check failed - disconnecting and scheduling reconnect");
      mqttHealthAwaitingEcho = false;
      mqttHealthMisses = 0;
      mqttConnecting = false;
      pending_mqtt_subscribe = false;
      pending_input_status_publish = false;
      mqttClient.disconnect();
      mqttBackoffMs = 10000;
      tiker_MQTT_CONNECT.interval(mqttBackoffMs);
      tiker_MQTT_CONNECT.start();
      return;
    }
  }

  // M3: topic is pre-computed at connect time; payload uses snprintf, not 3 String temporaries.
  snprintf(mqttHealthPayloadStorage, sizeof(mqttHealthPayloadStorage),
           "%lu-%u", (unsigned long)now, (unsigned)mqttHealthMisses);
  const uint16_t pktId = mqttPublish(mqttHealthTopicStorage.c_str(), 0, false, mqttHealthPayloadStorage);
  if (pktId == 0) {
    // Publish failed (OOM guard or queue full) — do NOT set awaiting flag.
    // A ping that was never sent can't be echoed back; marking it as awaiting
    // would count it as a miss and eventually trigger a spurious force-disconnect.
    SLOGLNP(SLOG_MQTT, "[MQTT   ] Health ping blocked (heap < 4KB or queue full) - skipping");
  } else {
    mqttHealthAwaitingEcho = true;
    SLOG(SLOG_MQTT, "[MQTT   ] Health ping sent (pkt=%u, payload=%s, every %us)\n",
      pktId, mqttHealthPayloadStorage, (unsigned)(mqttHealthIntervalMs / 1000));
  }
}
