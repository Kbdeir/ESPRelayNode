#include "AsyncNtpClient.h"

#ifdef ESP32

#include <WiFi.h>
#include <ConfigParams.h>
#include <MQTT_Processes.h>
#include <WireGuardManager.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern boolean ftimesynced;

namespace
{
  constexpr uint16_t NTP_PORT = 123;
  constexpr uint8_t NTP_PACKET_SIZE = 48;
  constexpr uint32_t NTP_TO_UNIX_EPOCH = 2208988800UL;
  constexpr uint32_t REQUEST_RETRY_MS = 15000;
  constexpr uint32_t DEFAULT_SYNC_INTERVAL_MS = 300000;

  SemaphoreHandle_t ntpMutex = nullptr;

  SemaphoreHandle_t ensureNtpMutex()
  {
    if (!ntpMutex) {
      ntpMutex = xSemaphoreCreateMutex();
    }
    return ntpMutex;
  }

  bool takeNtpMutex(TickType_t timeoutTicks = 0)
  {
    SemaphoreHandle_t mutex = ensureNtpMutex();
    return !mutex || xSemaphoreTake(mutex, timeoutTicks) == pdTRUE;
  }

  void giveNtpMutex()
  {
    if (ntpMutex) {
      xSemaphoreGive(ntpMutex);
    }
  }

  uint32_t ipToU32(const IPAddress &ip)
  {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
  }

  bool ipIsOnWifiSubnet(const IPAddress &ip)
  {
    if (WiFi.status() != WL_CONNECTED || ip == IPAddress(0, 0, 0, 0)) return false;

    const uint32_t target = ipToU32(ip);
    const uint32_t local = ipToU32(WiFi.localIP());
    const uint32_t mask = ipToU32(WiFi.subnetMask());

    if (mask != 0 && mask != 0xFFFFFFFF && ((target & mask) == (local & mask))) {
      return true;
    }

    return ip[0] == WiFi.localIP()[0] &&
           ip[1] == WiFi.localIP()[1] &&
           ip[2] == WiFi.localIP()[2];
  }

  bool ipAddrToArduino(const ip_addr_t &addr, IPAddress &out)
  {
    if (!IP_IS_V4_VAL(addr)) return false;
    const ip4_addr_t *ip4 = ip_2_ip4(&addr);
    out = IPAddress(ip4_addr1(ip4), ip4_addr2(ip4), ip4_addr3(ip4), ip4_addr4(ip4));
    return true;
  }

  void arduinoToIpAddr(const IPAddress &in, ip_addr_t &out)
  {
    IP_ADDR4(&out, in[0], in[1], in[2], in[3]);
  }

  err_t resolveHostName(const char *name, ip_addr_t *address, dns_found_callback callback, void *arg)
  {
    LOCK_TCPIP_CORE();
    const err_t result = dns_gethostbyname(name, address, callback, arg);
    UNLOCK_TCPIP_CORE();
    return result;
  }
}

void AsyncNtpClient::begin(const String &server, uint16_t syncIntervalMinutes, bool syncEnabled)
{
  if (!takeNtpMutex(pdMS_TO_TICKS(1000))) return;

  ntpServer = server;
  ntpServer.trim();
  syncIntervalMs = syncIntervalMinutes > 0
                       ? static_cast<uint32_t>(syncIntervalMinutes) * 60000UL
                       : DEFAULT_SYNC_INTERVAL_MS;

  enabled = syncEnabled && ntpServer.length() > 0;
  connected = false;
  resolving = false;
  addressResolved = false;
  synced = false;
  routeLogValid = false;
  lastRequestAt = 0;
  lastSyncAt = 0;
  syncedEpoch = 0;
  serverAddress = {};

  resetSocket();

  if (!enabled) {
    Serial.println(syncEnabled ? F("[NTP    ] Disabled: no server configured") : F("[NTP    ] Disabled by configuration"));
    giveNtpMutex();
    return;
  }

  IPAddress numeric;
  if (numeric.fromString(ntpServer)) {
    arduinoToIpAddr(numeric, serverAddress);
    addressResolved = true;
  }

  Serial.print(F("[NTP    ] Async NTP configured: "));
  Serial.println(ntpServer);
  giveNtpMutex();
}

void AsyncNtpClient::stop()
{
  if (!takeNtpMutex(pdMS_TO_TICKS(1000))) return;

  enabled = false;
  connected = false;
  resolving = false;
  addressResolved = false;
  synced = false;
  routeLogValid = false;
  lastRequestAt = 0;
  lastSyncAt = 0;
  syncedEpoch = 0;
  resetSocket();

  Serial.println(F("[NTP    ] Stopped for firmware update"));
  giveNtpMutex();
}

void AsyncNtpClient::update()
{
  if (!enabled || WiFi.status() != WL_CONNECTED || mqttConnecting) {
    return;
  }

  const uint32_t now = millis();
  if (synced && now - lastSyncAt < syncIntervalMs) {
    return;
  }

  if (!takeNtpMutex()) {
    return;
  }

  pollResponse();
  if (synced && now - lastSyncAt < syncIntervalMs) {
    giveNtpMutex();
    return;
  }

  if (lastRequestAt != 0 && now - lastRequestAt < REQUEST_RETRY_MS) {
    giveNtpMutex();
    return;
  }

  if (!addressResolved) {
    startResolve(now);
    giveNtpMutex();
    return;
  }

  sendRequest(now);
  giveNtpMutex();
}

bool AsyncNtpClient::needsNetworkWork() const
{
  if (!enabled || WiFi.status() != WL_CONNECTED || mqttConnecting) {
    return false;
  }

  const uint32_t now = millis();
  if (synced && now - lastSyncAt < syncIntervalMs) {
    return false;
  }

  if (lastRequestAt != 0 && now - lastRequestAt < REQUEST_RETRY_MS) {
    return false;
  }

  return !resolving;
}

bool AsyncNtpClient::isSynced() const
{
  return synced;
}

uint32_t AsyncNtpClient::lastSyncEpoch() const
{
  return syncedEpoch;
}

uint32_t AsyncNtpClient::lastSyncMillis() const
{
  return lastSyncAt;
}

void AsyncNtpClient::handleDnsResult(const char *name, const ip_addr_t *ipaddr, void *arg)
{
  AsyncNtpClient *client = static_cast<AsyncNtpClient *>(arg);
  if (!client || !takeNtpMutex(pdMS_TO_TICKS(1000))) {
    return;
  }

  client->resolving = false;

  if (!ipaddr) {
    client->lastRequestAt = millis();
    Serial.print(F("[NTP    ] Failed to resolve NTP server: "));
    Serial.println(name ? name : client->ntpServer.c_str());
    giveNtpMutex();
    return;
  }

  client->serverAddress = *ipaddr;
  client->addressResolved = true;
  client->connected = false;
  client->resetSocket();

  char addressText[48] = {};
  ipaddr_ntoa_r(ipaddr, addressText, sizeof(addressText));
  Serial.print(F("[NTP    ] Resolved NTP server "));
  Serial.print(name ? name : client->ntpServer.c_str());
  Serial.print(F(" -> "));
  Serial.println(addressText);
  giveNtpMutex();
}

bool AsyncNtpClient::serverAddressIsOnWifiSubnet() const
{
  IPAddress serverIp;
  return ipAddrToArduino(serverAddress, serverIp) && ipIsOnWifiSubnet(serverIp);
}

bool AsyncNtpClient::shouldUseVpnRoute() const
{
  if (MyConfParam.v_NTP_UseVPN != 1) return false;
  if (addressResolved && serverAddressIsOnWifiSubnet()) return false;
  return true;
}

bool AsyncNtpClient::selectRoute()
{
  const bool useVpn = shouldUseVpnRoute();
  if (!routeLogValid || loggedUseVpn != useVpn) {
    routeLogValid = true;
    loggedUseVpn = useVpn;
    Serial.print(F("[NTP    ] Using "));
    Serial.print(useVpn ? F("VPN") : F("WiFi"));
    Serial.print(F(" route to time server: "));
    Serial.println(ntpServer);
  }

  if (!useVpn) {
    return wireGuardUseWifiRoute("NTP");
  }

  if (!wireGuardIsEnabled()) {
    Serial.println(F("[NTP    ] VPN route requested but WireGuard tunnel is disabled"));
    return false;
  }

  if (!wireGuardIsNetworkUp()) {
    Serial.println(F("[NTP    ] Waiting for WireGuard tunnel before NTP request"));
    wireGuardEnsureStarted();
    return false;
  }

  return wireGuardUseVpnRoute("NTP");
}

void AsyncNtpClient::restorePrimaryRoute()
{
  if (mqttIsEnabled() && mqttWantsVpnRoute() && wireGuardIsNetworkUp()) {
    wireGuardUseVpnRoute("MQTT restore after NTP");
    return;
  }

  wireGuardUseWifiRoute("NTP complete");
}

void AsyncNtpClient::startResolve(uint32_t now)
{
  if (resolving) return;

  if (!selectRoute()) {
    lastRequestAt = now;
    return;
  }

  ip_addr_t resolvedAddress = {};
  const err_t result = resolveHostName(ntpServer.c_str(), &resolvedAddress, handleDnsResult, this);
  lastRequestAt = now;
  restorePrimaryRoute();

  if (result == ERR_OK) {
    serverAddress = resolvedAddress;
    addressResolved = true;
    connected = false;
    resetSocket();

    char addressText[48] = {};
    ipaddr_ntoa_r(&resolvedAddress, addressText, sizeof(addressText));
    Serial.print(F("[NTP    ] Resolved NTP server "));
    Serial.print(ntpServer);
    Serial.print(F(" -> "));
    Serial.println(addressText);
    return;
  }

  if (result == ERR_INPROGRESS) {
    resolving = true;
    Serial.print(F("[NTP    ] Resolving NTP server: "));
    Serial.println(ntpServer);
    return;
  }

  Serial.print(F("[NTP    ] Failed to start NTP DNS resolve for "));
  Serial.print(ntpServer);
  Serial.print(F(", err="));
  Serial.println(static_cast<int>(result));
}

void AsyncNtpClient::sendRequest(uint32_t now)
{
  if (!selectRoute()) {
    lastRequestAt = now;
    return;
  }

  if (!connected) {
    if (!openSocket()) {
      lastRequestAt = now;
      Serial.print(F("[NTP    ] Failed to connect async UDP socket to "));
      Serial.println(ntpServer);
      restorePrimaryRoute();
      return;
    }

    Serial.print(F("[NTP    ] Async NTP server: "));
    Serial.println(ntpServer);
  }

  uint8_t request[NTP_PACKET_SIZE] = {};
  request[0] = 0x23;

  const ssize_t written = send(udpSocket, request, sizeof(request), 0);
  lastRequestAt = now;
  Serial.print(F("[NTP    ] Async NTP request sent, bytes="));
  Serial.println(written > 0 ? static_cast<size_t>(written) : 0);
  restorePrimaryRoute();
}

void AsyncNtpClient::handlePacket(const uint8_t *data, size_t length)
{
  Serial.print(F("[NTP    ] Async NTP response received, bytes="));
  Serial.println(length);

  if (length < NTP_PACKET_SIZE) {
    return;
  }

  if ((data[0] & 0x07) != 4) {
    Serial.println(F("[NTP    ] Ignoring NTP packet with unexpected mode"));
    return;
  }

  const uint32_t secondsSince1900 =
      (static_cast<uint32_t>(data[40]) << 24) |
      (static_cast<uint32_t>(data[41]) << 16) |
      (static_cast<uint32_t>(data[42]) << 8) |
      static_cast<uint32_t>(data[43]);

  if (secondsSince1900 < NTP_TO_UNIX_EPOCH) {
    Serial.println(F("[NTP    ] Ignoring invalid NTP response"));
    return;
  }

  syncedEpoch = secondsSince1900 - NTP_TO_UNIX_EPOCH;
  synced = true;
  lastSyncAt = millis();
  ftimesynced = true;

  Serial.print(F("[NTP    ] Clock synchronized, epoch="));
  Serial.println(syncedEpoch);
}

void AsyncNtpClient::pollResponse()
{
  if (udpSocket < 0) return;

  uint8_t response[NTP_PACKET_SIZE] = {};
  while (true) {
    const ssize_t received = recv(udpSocket, response, sizeof(response), 0);
    if (received > 0) {
      handlePacket(response, static_cast<size_t>(received));
      continue;
    }

    if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      Serial.print(F("[NTP    ] UDP receive failed, errno="));
      Serial.println(errno);
      resetSocket();
    }
    return;
  }
}

bool AsyncNtpClient::openSocket()
{
  if (udpSocket >= 0) {
    connected = true;
    return true;
  }

  if (!IP_IS_V4_VAL(serverAddress)) {
    Serial.println(F("[NTP    ] Only IPv4 NTP servers are supported"));
    return false;
  }

  udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpSocket < 0) {
    Serial.print(F("[NTP    ] UDP socket() failed, errno="));
    Serial.println(errno);
    return false;
  }

  if (fcntl(udpSocket, F_SETFL, O_NONBLOCK) < 0) {
    Serial.print(F("[NTP    ] UDP fcntl() failed, errno="));
    Serial.println(errno);
    resetSocket();
    return false;
  }

  sockaddr_in server = {};
  server.sin_family = AF_INET;
  server.sin_port = htons(NTP_PORT);
  server.sin_addr.s_addr = ip_2_ip4(&serverAddress)->addr;

  if (connect(udpSocket, reinterpret_cast<sockaddr *>(&server), sizeof(server)) < 0) {
    Serial.print(F("[NTP    ] UDP connect() failed, errno="));
    Serial.println(errno);
    resetSocket();
    return false;
  }

  connected = true;
  return true;
}

void AsyncNtpClient::resetSocket()
{
  if (udpSocket >= 0) {
    close(udpSocket);
    udpSocket = -1;
  }
  connected = false;
}

#else

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ConfigParams.h>
#include <MQTT_Processes.h>

extern boolean ftimesynced;

namespace
{
  constexpr uint16_t NTP_PORT = 123;
  constexpr uint8_t NTP_PACKET_SIZE = 48;
  constexpr uint32_t NTP_TO_UNIX_EPOCH = 2208988800UL;
  constexpr uint32_t REQUEST_RETRY_MS = 15000;
  constexpr uint32_t DEFAULT_SYNC_INTERVAL_MS = 300000;

  WiFiUDP ntpUdp;
  IPAddress ntpServerIp;
  uint8_t ntpPacket[NTP_PACKET_SIZE] = {};
}

void AsyncNtpClient::begin(const String &server, uint16_t syncIntervalMinutes, bool syncEnabled)
{
  ntpServer = server;
  ntpServer.trim();
  syncIntervalMs = syncIntervalMinutes > 0
                       ? static_cast<uint32_t>(syncIntervalMinutes) * 60000UL
                       : DEFAULT_SYNC_INTERVAL_MS;

  enabled = syncEnabled && ntpServer.length() > 0;
  connected = false;
  resolving = false;
  addressResolved = false;
  synced = false;
  lastRequestAt = 0;
  lastSyncAt = 0;
  syncedEpoch = 0;
  resetSocket();

  if (!enabled) {
    Serial.println(syncEnabled ? F("[NTP    ] Disabled: no server configured") : F("[NTP    ] Disabled by configuration"));
    return;
  }

  if (ntpServerIp.fromString(ntpServer)) {
    addressResolved = true;
  }

  Serial.print(F("[NTP    ] Async NTP configured: "));
  Serial.println(ntpServer);
}

void AsyncNtpClient::stop()
{
  enabled = false;
  connected = false;
  resolving = false;
  addressResolved = false;
  synced = false;
  lastRequestAt = 0;
  lastSyncAt = 0;
  syncedEpoch = 0;
  resetSocket();
  Serial.println(F("[NTP    ] Stopped for firmware update"));
}

void AsyncNtpClient::update()
{
  if (!enabled || WiFi.status() != WL_CONNECTED || mqttConnecting) {
    return;
  }

  const uint32_t now = millis();
  pollResponse();

  if (synced && now - lastSyncAt < syncIntervalMs) {
    return;
  }

  if (lastRequestAt != 0 && now - lastRequestAt < REQUEST_RETRY_MS) {
    return;
  }

  if (!addressResolved) {
    startResolve(now);
    return;
  }

  sendRequest(now);
}

bool AsyncNtpClient::needsNetworkWork() const
{
  if (!enabled || WiFi.status() != WL_CONNECTED || mqttConnecting) {
    return false;
  }

  const uint32_t now = millis();
  if (synced && now - lastSyncAt < syncIntervalMs) {
    return false;
  }

  if (lastRequestAt != 0 && now - lastRequestAt < REQUEST_RETRY_MS) {
    return false;
  }

  return true;
}

bool AsyncNtpClient::isSynced() const
{
  return synced;
}

uint32_t AsyncNtpClient::lastSyncEpoch() const
{
  return syncedEpoch;
}

uint32_t AsyncNtpClient::lastSyncMillis() const
{
  return lastSyncAt;
}

void AsyncNtpClient::handleDnsResult(const char *, const ip_addr_t *, void *)
{
}

bool AsyncNtpClient::serverAddressIsOnWifiSubnet() const
{
  return false;
}

bool AsyncNtpClient::shouldUseVpnRoute() const
{
  return false;
}

bool AsyncNtpClient::selectRoute()
{
  return true;
}

void AsyncNtpClient::restorePrimaryRoute()
{
}

void AsyncNtpClient::startResolve(uint32_t now)
{
  lastRequestAt = now;
  IPAddress resolvedIp;
  if (!WiFi.hostByName(ntpServer.c_str(), resolvedIp)) {
    Serial.print(F("[NTP    ] Failed to resolve NTP server: "));
    Serial.println(ntpServer);
    return;
  }

  ntpServerIp = resolvedIp;
  addressResolved = true;
  connected = false;

  Serial.print(F("[NTP    ] Resolved NTP server "));
  Serial.print(ntpServer);
  Serial.print(F(" -> "));
  Serial.println(ntpServerIp);
}

void AsyncNtpClient::sendRequest(uint32_t now)
{
  if (!connected && !openSocket()) {
    lastRequestAt = now;
    Serial.print(F("[NTP    ] Failed to start UDP socket for "));
    Serial.println(ntpServer);
    return;
  }

  memset(ntpPacket, 0, NTP_PACKET_SIZE);
  ntpPacket[0] = 0x23;

  ntpUdp.beginPacket(ntpServerIp, NTP_PORT);
  ntpUdp.write(ntpPacket, NTP_PACKET_SIZE);
  ntpUdp.endPacket();

  lastRequestAt = now;
  Serial.print(F("[NTP    ] Async NTP request sent to "));
  Serial.println(ntpServerIp);
}

void AsyncNtpClient::handlePacket(const uint8_t *data, size_t length)
{
  Serial.print(F("[NTP    ] Async NTP response received, bytes="));
  Serial.println(length);

  if (length < NTP_PACKET_SIZE) {
    return;
  }

  const uint32_t secondsSince1900 =
      (static_cast<uint32_t>(data[40]) << 24) |
      (static_cast<uint32_t>(data[41]) << 16) |
      (static_cast<uint32_t>(data[42]) << 8) |
      static_cast<uint32_t>(data[43]);

  if (secondsSince1900 < NTP_TO_UNIX_EPOCH) {
    Serial.println(F("[NTP    ] Ignoring invalid NTP response"));
    return;
  }

  syncedEpoch = secondsSince1900 - NTP_TO_UNIX_EPOCH;
  synced = true;
  lastSyncAt = millis();
  ftimesynced = true;

  Serial.print(F("[NTP    ] Clock synchronized, epoch="));
  Serial.println(syncedEpoch);
}

void AsyncNtpClient::pollResponse()
{
  if (!connected) return;

  const int packetSize = ntpUdp.parsePacket();
  if (packetSize <= 0) return;

  uint8_t response[NTP_PACKET_SIZE] = {};
  const int received = ntpUdp.read(response, NTP_PACKET_SIZE);
  if (received > 0) {
    handlePacket(response, static_cast<size_t>(received));
  }
}

bool AsyncNtpClient::openSocket()
{
  ntpUdp.stop();
  connected = ntpUdp.begin(0) == 1;
  return connected;
}

void AsyncNtpClient::resetSocket()
{
  ntpUdp.stop();
  connected = false;
}

#endif
