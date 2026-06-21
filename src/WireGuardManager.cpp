#include <WireGuardManager.h>

#ifdef ESP32

#include <ConfigParams.h>
#include <FirmwareUpdateState.h>
#include <SerialLog.h>
#include <MQTT_Processes.h>
#include <WireGuardClient.h>
#include <WireGuardConfig.h>

#ifdef ESP32
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

static WireGuardClient wireGuardClient;
static StoredWireGuardConfig wireGuardConfig;
static bool wireGuardLoaded = false;
static bool wireGuardStarted = false;
static bool wireGuardNetworkUp = false;
static bool wireGuardApplyRequested = false;
static uint32_t wireGuardStartedAt = 0;
static uint32_t wireGuardNextReconnectAt = 0;
// Maximum time we wait for the handshake to complete before tearing the
// interface down and restarting wg.begin().  Set generously (3 minutes) so we
// give a slow peer time to respond — the library handshake-retries internally
// every ~5 seconds, so anything under a minute interrupts useful retries.
static const uint32_t WIREGUARD_CONNECT_TIMEOUT_MS = 180000;
static const uint32_t WIREGUARD_RECONNECT_BACKOFF_MS = 30000;
static const uint32_t WIREGUARD_DNS_CHECK_TIMEOUT_MS = 8000;
static const uint8_t WIREGUARD_DNS_FAIL_THRESHOLD = 2;

bool wireGuardUseWifiRoute(const char *reason);
bool wireGuardUseVpnRoute(const char *reason);

#ifdef ESP32
static uint32_t wireGuardDnsCheckSentAt = 0;
static uint32_t wireGuardLastDnsCheckAt = 0;
static bool wireGuardDnsLastCheckValid = false;
static int wgDnsLiveSocket = -1;
static uint16_t wgDnsQueryId = 0;
static uint8_t wireGuardDnsConsecutiveFailures = 0;
static SemaphoreHandle_t wireGuardRouteMutex = nullptr;

static bool wireGuardTakeRouteMutex(TickType_t timeoutTicks)
{
  if (!wireGuardRouteMutex) {
    wireGuardRouteMutex = xSemaphoreCreateMutex();
  }
  return !wireGuardRouteMutex || xSemaphoreTake(wireGuardRouteMutex, timeoutTicks) == pdTRUE;
}

static void wireGuardGiveRouteMutex()
{
  if (wireGuardRouteMutex) {
    xSemaphoreGive(wireGuardRouteMutex);
  }
}

static struct netif *wireGuardCurrentDefaultNetif()
{
  struct netif *current = nullptr;
  LOCK_TCPIP_CORE();
  current = netif_default;
  UNLOCK_TCPIP_CORE();
  return current;
}

static void wireGuardRestoreDefaultNetif(struct netif *netifToRestore)
{
  if (!netifToRestore) return;
  LOCK_TCPIP_CORE();
  if (netif_default != netifToRestore) {
    netif_set_default(netifToRestore);
  }
  UNLOCK_TCPIP_CORE();
}

#endif

static void wireGuardScheduleReconnect(const __FlashStringHelper *reason)
{
  SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] ")); Serial.println(reason); }
  wireGuardStarted = false;
  wireGuardStartedAt = 0;
  wireGuardNetworkUp = false;
  wireGuardUseWifiRoute("WireGuard reconnect");
  wireGuardNextReconnectAt = millis() + WIREGUARD_RECONNECT_BACKOFF_MS;
}

#ifdef ESP32
static void wireGuardDnsCheckClose()
{
  if (wgDnsLiveSocket >= 0) {
    close(wgDnsLiveSocket);
    wgDnsLiveSocket = -1;
  }
}

static bool wireGuardIpIsOnWifiSubnet(const IPAddress &ip)
{
  if (ip == IPAddress(0, 0, 0, 0)) return false;

  const uint32_t rawIp = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
  const IPAddress wifiIp = WiFi.localIP();
  const IPAddress wifiMask = WiFi.subnetMask();
  const uint32_t rawWifi = ((uint32_t)wifiIp[0] << 24) | ((uint32_t)wifiIp[1] << 16) | ((uint32_t)wifiIp[2] << 8) | (uint32_t)wifiIp[3];
  const uint32_t rawMask = ((uint32_t)wifiMask[0] << 24) | ((uint32_t)wifiMask[1] << 16) | ((uint32_t)wifiMask[2] << 8) | (uint32_t)wifiMask[3];
  if (rawMask != 0 && rawMask != 0xFFFFFFFF && ((rawIp & rawMask) == (rawWifi & rawMask))) return true;

  return ip[0] == wifiIp[0] && ip[1] == wifiIp[1] && ip[2] == wifiIp[2];
}

static bool wireGuardAnyServiceNeedsVpnRoute()
{
  return MyConfParam.v_NTP_UseVPN == 1 || mqttWantsVpnRoute();
}

static bool wireGuardParseNextDnsToken(const char *&ptr, char *outBuf, size_t outBufSize)
{
  while (*ptr && (isspace(static_cast<unsigned char>(*ptr)) || *ptr == ',' || *ptr == ';')) {
    ++ptr;
  }

  if (!*ptr) return false;

  size_t i = 0;
  while (*ptr && *ptr != ',' && *ptr != ';' && !isspace(static_cast<unsigned char>(*ptr))) {
    if (i + 1 < outBufSize) outBuf[i++] = *ptr;
    ++ptr;
  }
  outBuf[i] = '\0';
  return i > 0;
}

static bool wireGuardHostIsNumericIp(const char *host)
{
  IPAddress parsed;
  return host && parsed.fromString(host);
}

static String wireGuardDnsProbeHost()
{
  String host = MyConfParam.v_MQTT_BROKER;
  host.trim();
  if (host.length() > 0 && !wireGuardHostIsNumericIp(host.c_str())) {
    return host;
  }

  host = MyConfParam.v_timeserver;
  if (host.length() > 0 && !wireGuardHostIsNumericIp(host.c_str())) {
    return host;
  }

  host = wireGuardConfig.peerEndpoint;
  const int colon = host.lastIndexOf(':');
  if (colon > 0) host = host.substring(0, colon);
  host.trim();
  if (host.length() > 0 && !wireGuardHostIsNumericIp(host.c_str())) {
    return host;
  }

  return String(F("pool.ntp.org"));
}

static void wireGuardDnsHandleFailure(uint32_t now)
{
  wireGuardDnsCheckClose();
  ++wireGuardDnsConsecutiveFailures;
  SLOG_IF(SLOG_WG) {
    Serial.print(F("[WG     ] DNS liveness check failed ("));
    Serial.print(wireGuardDnsConsecutiveFailures);
    Serial.print(F("/"));
    Serial.print(WIREGUARD_DNS_FAIL_THRESHOLD);
    Serial.println(F(")"));
  }

  if (wireGuardDnsConsecutiveFailures >= WIREGUARD_DNS_FAIL_THRESHOLD) {
    SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: tunnel declared down, reconnecting");
    wireGuardDnsConsecutiveFailures = 0;
    wireGuardClient.disconnect();
    wireGuardScheduleReconnect(F("DNS liveness reconnect scheduled"));
  }
}

// Parse the first numeric IP from wireGuardConfig.dnsServers into a sockaddr_in
// pointed at UDP port 53.  Returns false when no valid IP is configured.
static bool wgGetVpnDnsAddr(struct sockaddr_in &out)
{
  if (wireGuardConfig.dnsServers.length() == 0) return false;
  const char *ptr = wireGuardConfig.dnsServers.c_str();
  char token[48];
  if (!wireGuardParseNextDnsToken(ptr, token, sizeof(token))) return false;
  IPAddress ip;
  if (!ip.fromString(token) || ip == IPAddress(0, 0, 0, 0)) return false;
  memset(&out, 0, sizeof(out));
  out.sin_family = AF_INET;
  out.sin_port = htons(53);
  const uint32_t raw = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
                       ((uint32_t)ip[2] << 8)  | (uint32_t)ip[3];
  out.sin_addr.s_addr = htonl(raw);
  return true;
}

// Build a minimal DNS A-record query packet.  Returns packet length, or -1 on error.
static int wgBuildDnsQuery(uint8_t *buf, int bufSize, const char *host, uint16_t id)
{
  if (bufSize < 18) return -1;
  buf[0] = (uint8_t)(id >> 8); buf[1] = (uint8_t)(id & 0xFF);
  buf[2] = 0x01; buf[3] = 0x00;   // flags: recursion desired
  buf[4] = 0x00; buf[5] = 0x01;   // QDCOUNT = 1
  for (int i = 6; i < 12; i++) buf[i] = 0; // ANCOUNT, NSCOUNT, ARCOUNT = 0
  int pos = 12;
  const char *p = host;
  while (*p) {
    const char *dot = strchr(p, '.');
    const int labelLen = dot ? (int)(dot - p) : (int)strlen(p);
    if (pos + 1 + labelLen + 5 > bufSize) return -1;
    buf[pos++] = (uint8_t)labelLen;
    memcpy(buf + pos, p, labelLen);
    pos += labelLen;
    if (!dot) break;
    p = dot + 1;
  }
  buf[pos++] = 0x00;
  buf[pos++] = 0x00; buf[pos++] = 0x01;  // QTYPE = A
  buf[pos++] = 0x00; buf[pos++] = 0x01;  // QCLASS = IN
  return pos;
}

static void wireGuardUpdateDnsLivenessCheck(uint32_t now)
{
  if (firmwareUpdateInProgress) {
    wireGuardDnsCheckClose();
    return;
  }

  if (!wireGuardConfig.dnsLivenessCheck || !wireGuardNetworkUp) {
    wireGuardDnsCheckClose();
    return;
  }

  // --- Poll an in-flight probe ---
  if (wgDnsLiveSocket >= 0) {
    uint8_t resp[64];
    const ssize_t n = recv(wgDnsLiveSocket, resp, sizeof(resp), 0);
    // Valid DNS response: matches our query ID and has QR=1 in flags
    if (n >= 4 &&
        resp[0] == (uint8_t)(wgDnsQueryId >> 8) &&
        resp[1] == (uint8_t)(wgDnsQueryId & 0xFF) &&
        (resp[2] & 0x80)) {
      SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: VPN tunnel OK");
      wireGuardDnsCheckClose();
      wireGuardDnsConsecutiveFailures = 0;
      return;
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      wireGuardDnsCheckClose();
      // ENETDOWN/ENETUNREACH during a re-handshake window is not a real failure.
      if (!wireGuardClient.isSessionFresh()) {
        SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: recv error during re-handshake, not counting");
        return;
      }
      SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] DNS liveness: recv error errno=")); Serial.println(errno); }
      wireGuardDnsHandleFailure(now);
      return;
    }
    if (static_cast<int32_t>(now - wireGuardDnsCheckSentAt) >=
        static_cast<int32_t>(WIREGUARD_DNS_CHECK_TIMEOUT_MS)) {
      if (!wireGuardClient.isSessionFresh()) {
        wireGuardDnsCheckClose();
        SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: probe timed out during re-handshake, not counting");
        return;
      }
      SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: probe timed out");
      wireGuardDnsHandleFailure(now);
    }
    return;
  }

  // --- Check whether it is time for a new probe ---
  uint32_t checkIntervalMs = static_cast<uint32_t>(wireGuardConfig.dnsLivenessInterval) * 1000UL;
  if (checkIntervalMs < 5000UL) checkIntervalMs = 5000UL;
  if (wireGuardDnsLastCheckValid &&
      static_cast<int32_t>(now - wireGuardLastDnsCheckAt) < static_cast<int32_t>(checkIntervalMs)) {
    return;
  }
  wireGuardLastDnsCheckAt = now;
  wireGuardDnsLastCheckValid = true;

  // Do not probe while the session is re-handshaking — send() will fail with
  // ENETDOWN and falsely count as a liveness failure.  The re-handshake window
  // is a few seconds; the next probe interval will catch a genuinely dead tunnel.
  if (!wireGuardClient.isSessionFresh()) {
    SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: session re-handshaking, skipping probe");
    return;
  }

  // --- Open a UDP socket and send the DNS query through the VPN interface ---
  struct sockaddr_in dnsAddr;
  if (!wgGetVpnDnsAddr(dnsAddr)) {
    // No VPN DNS server configured — liveness check cannot run.
    SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: no VPN DNS server configured");
    return;
  }

  // Route must be VPN so connect() binds the socket to the WireGuard netif.
  struct netif *savedNetif = wireGuardCurrentDefaultNetif();
  if (!wireGuardUseVpnRoute("WG DNS liveness")) {
    SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: VPN route unavailable");
    wireGuardRestoreDefaultNetif(savedNetif);
    return;
  }

  const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] DNS liveness: socket() failed errno=")); Serial.println(errno); }
    wireGuardRestoreDefaultNetif(savedNetif);
    return;
  }

  // connect() fixes the destination and captures the outgoing interface at this moment.
  if (connect(sock, reinterpret_cast<struct sockaddr *>(&dnsAddr), sizeof(dnsAddr)) < 0) {
    SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] DNS liveness: connect() failed errno=")); Serial.println(errno); }
    close(sock);
    wireGuardRestoreDefaultNetif(savedNetif);
    return;
  }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
    SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] DNS liveness: fcntl() failed errno=")); Serial.println(errno); }
    close(sock);
    wireGuardRestoreDefaultNetif(savedNetif);
    return;
  }

  static String probeHostStorage;
  probeHostStorage = wireGuardDnsProbeHost();
  ++wgDnsQueryId;
  uint8_t query[64];
  const int qlen = wgBuildDnsQuery(query, sizeof(query), probeHostStorage.c_str(), wgDnsQueryId);
  if (qlen <= 0) {
    close(sock);
    wireGuardRestoreDefaultNetif(savedNetif);
    return;
  }

  // send() uses the interface bound at connect() time (WireGuard).
  const ssize_t sent = send(sock, query, static_cast<size_t>(qlen), 0);
  wireGuardRestoreDefaultNetif(savedNetif);

  if (sent <= 0) {
    close(sock);
    // ENETDOWN (128) during a re-handshake is transient — do not count it.
    if (!wireGuardClient.isSessionFresh()) {
      SLOGLNP(SLOG_WG, "[WG     ] DNS liveness: send() failed during re-handshake, not counting");
      return;
    }
    SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] DNS liveness: send() failed errno=")); Serial.println(errno); }
    wireGuardDnsHandleFailure(now);
    return;
  }

  wgDnsLiveSocket = sock;
  wireGuardDnsCheckSentAt = now;

  const uint32_t ipNet = ntohl(dnsAddr.sin_addr.s_addr);
  SLOG(SLOG_WG, "[WG     ] DNS liveness: probe sent to %u.%u.%u.%u via VPN\n",
    (ipNet >> 24) & 0xFF, (ipNet >> 16) & 0xFF, (ipNet >> 8) & 0xFF, ipNet & 0xFF);
}
#endif

static bool wireGuardLoadConfig()
{
  config_read_error_t res = loadWireGuardConfig("/WireGuardConfig.json", wireGuardConfig);
  if (res != SUCCESS && res != FILE_NOT_FOUND) {
    SLOGLNP(SLOG_WG, "[WG     ] Could not load WireGuard configuration");
  }
  wireGuardLoaded = true;
  return res == SUCCESS;
}

bool wireGuardIsEnabled()
{
  if (!wireGuardLoaded) wireGuardLoadConfig();
  return wireGuardConfig.enabled && wireGuardConfig.privateKey.length() > 0 &&
         wireGuardConfig.peerPublicKey.length() > 0 && wireGuardConfig.peerEndpoint.length() > 0;
}

bool wireGuardIsNetworkUp()
{
  return wireGuardNetworkUp && wireGuardClient.isNetworkUp();
}

WireGuardStatus wireGuardGetStatus()
{
  if (!wireGuardLoaded) wireGuardLoadConfig();

  WireGuardStatus status;
  status.enabled = wireGuardIsEnabled();
  status.started = wireGuardStarted;
  status.connected = wireGuardClient.isPeerUp() && wireGuardClient.isSessionFresh();
  status.networkUp = wireGuardIsNetworkUp();
  // True during the backoff window after a failed connection attempt.
  const uint32_t now32 = millis();
  status.reconnecting = !wireGuardStarted && wireGuardNextReconnectAt != 0 &&
                        static_cast<int32_t>(wireGuardNextReconnectAt - now32) > 0;
  status.localIp = status.networkUp ? wireGuardClient.localIp() : wireGuardConfig.localIp;
  status.configuredEndpoint = wireGuardConfig.peerEndpoint;
  status.resolvedEndpointIp = wireGuardClient.resolvedEndpointIp();
  status.peerIp = wireGuardClient.peerIp();
  status.peerPort = wireGuardClient.peerPort();
  status.dnsServers = wireGuardConfig.dnsServers;
  status.dnsLivenessCheck = wireGuardConfig.dnsLivenessCheck;
  status.dnsLivenessInterval = wireGuardConfig.dnsLivenessInterval;
#ifdef ESP32
  status.dnsLivenessFailures = wireGuardDnsConsecutiveFailures;
#endif
  return status;
}

bool wireGuardUseWifiRoute(const char *reason)
{
  (void)reason;
#ifdef ESP32
  if (!wireGuardTakeRouteMutex(pdMS_TO_TICKS(1000))) return false;
  esp_netif_t *staEspNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!staEspNetif) {
    wireGuardGiveRouteMutex();
    return false;
  }

  void *staNetifImpl = esp_netif_get_netif_impl(staEspNetif);
  if (!staNetifImpl) {
    wireGuardGiveRouteMutex();
    return false;
  }

  struct netif *wifiNetif = static_cast<struct netif *>(staNetifImpl);
  LOCK_TCPIP_CORE();
  if (netif_default != wifiNetif) {
    netif_set_default(wifiNetif);
  }
  UNLOCK_TCPIP_CORE();
  wireGuardGiveRouteMutex();
  return true;
#else
  return true;
#endif
}

bool wireGuardUseVpnRoute(const char *reason)
{
  (void)reason;
  if (!wireGuardIsNetworkUp()) return false;

#ifdef ESP32
  if (!wireGuardTakeRouteMutex(pdMS_TO_TICKS(1000))) return false;
  struct netif *vpnNetif = nullptr;
  LOCK_TCPIP_CORE();
  for (struct netif *candidate = netif_list; candidate != nullptr; candidate = candidate->next) {
    if (candidate->name[0] == 'w' && candidate->name[1] == 'g') {
      vpnNetif = candidate;
      break;
    }
  }

  if (vpnNetif && netif_default != vpnNetif) {
    netif_set_default(vpnNetif);
  }
  UNLOCK_TCPIP_CORE();

  if (!vpnNetif) {
    wireGuardGiveRouteMutex();
    SLOGLNP(SLOG_WG, "[WG     ] Could not select WireGuard route");
    return false;
  }

  wireGuardGiveRouteMutex();
  return true;
#else
  return false;
#endif
}

void wireGuardLogDefaultRoute(const char *reason)
{
#ifdef ESP32
  char name[8] = "(none)";
  char ipText[24] = "0.0.0.0";
  char reasonText[32] = "";
  bool isDefault = false;

  if (reason && reason[0]) {
    strncpy(reasonText, reason, sizeof(reasonText) - 1);
    reasonText[sizeof(reasonText) - 1] = '\0';
  }

  LOCK_TCPIP_CORE();
  struct netif *current = netif_default;
  if (current) {
    snprintf(name, sizeof(name), "%c%c%d", current->name[0], current->name[1], current->num);
    ip4addr_ntoa_r(netif_ip4_addr(current), ipText, sizeof(ipText));
    isDefault = true;
  }
  UNLOCK_TCPIP_CORE();

  static bool lastRouteLogValid = false;
  static char lastReason[32] = "";
  static char lastName[8] = "";
  static char lastIpText[24] = "";

  if (lastRouteLogValid &&
      strcmp(lastReason, reasonText) == 0 &&
      strcmp(lastName, name) == 0 &&
      strcmp(lastIpText, ipText) == 0) {
    return;
  }

  lastRouteLogValid = true;
  strncpy(lastReason, reasonText, sizeof(lastReason) - 1);
  lastReason[sizeof(lastReason) - 1] = '\0';
  strncpy(lastName, name, sizeof(lastName) - 1);
  lastName[sizeof(lastName) - 1] = '\0';
  strncpy(lastIpText, ipText, sizeof(lastIpText) - 1);
  lastIpText[sizeof(lastIpText) - 1] = '\0';

  Serial.print(F("[ROUTE  ] Default route"));
  if (reasonText[0]) {
    Serial.print(F(" for "));
    Serial.print(reasonText);
  }
  Serial.print(F(": "));
  Serial.print(name);
  if (isDefault) {
    Serial.print(F(" ip "));
    Serial.print(ipText);
  }
  Serial.println();
#else
  (void)reason;
#endif
}

bool wireGuardEnsureStarted()
{
  if (!wireGuardLoaded) wireGuardLoadConfig();

  if (!wireGuardConfig.enabled) {
    wireGuardDisconnect();
    return false;
  }

  if (wireGuardIsNetworkUp()) return true;
  if (wireGuardStarted) return false;
  if (wireGuardNextReconnectAt != 0 && millis() < wireGuardNextReconnectAt) return false;

#ifdef ESP32
  if (WiFi.status() != WL_CONNECTED) return false;
#endif

  WireGuardConfig config;
  config.privateKey = wireGuardConfig.privateKey;
  config.localIp = wireGuardConfig.localIp;
  config.peerPublicKey = wireGuardConfig.peerPublicKey;
  config.peerEndpoint = wireGuardConfig.peerEndpoint;
  config.allowedIps = wireGuardConfig.allowedIps;
  config.dnsServers = wireGuardConfig.dnsServers;

  if (!wireGuardClient.begin(config)) {
    SLOGLNP(SLOG_WG, "[WG     ] begin() failed");
    wireGuardStarted = false;
    wireGuardStartedAt = 0;
    wireGuardNextReconnectAt = millis() + WIREGUARD_RECONNECT_BACKOFF_MS;
    return false;
  }

  const bool routeAllTrafficViaVpn = mqttWantsVpnRoute();
  wireGuardClient.setVpnAsDefaultRoute(routeAllTrafficViaVpn);
  wireGuardClient.setNetworkUpCallback([](IPAddress assignedIp, IPAddress gateway, IPAddress primaryDns, IPAddress secondaryDns) {
    wireGuardNetworkUp = true;
    SLOG_IF(SLOG_WG) {
      Serial.print(F("[WG     ] Network up, IP: "));
      Serial.print(assignedIp);
      Serial.print(F(" gateway: "));
      Serial.print(gateway);
      Serial.print(F(" dns: "));
      Serial.print(primaryDns);
      Serial.print(F(" "));
      Serial.println(secondaryDns);
    }

    if (mqttIsEnabled()) {
      mqttConnectNow();
      if (!mqttWantsVpnRoute()) {
        wireGuardUseWifiRoute("WireGuard network up");
      }
    } else {
      wireGuardUseWifiRoute("WireGuard network up");
    }
  });

  SLOG_IF(SLOG_WG) { Serial.print(F("[WG     ] Connecting to ")); Serial.println(config.peerEndpoint); }

  wireGuardStarted = true;
  wireGuardStartedAt = millis();
  if (!wireGuardClient.connectTunnel()) {
    SLOGLNP(SLOG_WG, "[WG     ] Tunnel connection failed");
    wireGuardStarted = false;
    wireGuardStartedAt = 0;
    wireGuardNetworkUp = false;
    wireGuardUseWifiRoute("WireGuard failed");
    wireGuardNextReconnectAt = millis() + WIREGUARD_RECONNECT_BACKOFF_MS;
    return false;
  }

  SLOG(SLOG_WG, "[WG     ] connectTunnel() returned OK; awaiting handshake (timeout %us)\n",
    (unsigned)(WIREGUARD_CONNECT_TIMEOUT_MS / 1000));
  wireGuardNextReconnectAt = 0;
  return wireGuardClient.isNetworkUp();
}

void wireGuardApplyConfig()
{
  wireGuardApplyRequested = false;
  wireGuardLoaded = false;
  wireGuardLoadConfig();
  wireGuardDisconnect();
  wireGuardEnsureStarted();
}

void wireGuardRequestApplyConfig()
{
  wireGuardApplyRequested = true;
}

void wireGuardDisconnect()
{
  if (wireGuardStarted || wireGuardClient.isConnected()) {
    wireGuardClient.disconnect();
  }
#ifdef ESP32
  wireGuardDnsCheckClose();
  wireGuardDnsConsecutiveFailures = 0;
  wireGuardDnsLastCheckValid = false;
#endif
  wireGuardStarted = false;
  wireGuardStartedAt = 0;
  wireGuardNetworkUp = false;
  wireGuardNextReconnectAt = 0;
  wireGuardUseWifiRoute("WireGuard disconnect");
}

// Print a one-line handshake-state snapshot when something has changed or every
// HANDSHAKE_LOG_INTERVAL_MS while we're still waiting for the first peer-up
// transition.  Helps diagnose "Why is the tunnel taking forever?" silences.
static void wireGuardLogHandshakeProgress(uint32_t now)
{
#ifdef ESP32
  static const uint32_t HANDSHAKE_LOG_INTERVAL_MS = 5000;
  static uint32_t lastLogAt = 0;
  static int8_t lastPeerUp = -1;
  static int8_t lastNetworkUp = -1;
  static uint8_t lastSessionFresh = 2;

  const bool peerUp = wireGuardClient.isPeerUp();
  const bool sessionFresh = wireGuardClient.isSessionFresh();
  const bool netUp = wireGuardNetworkUp;
  const uint32_t sessionAge = wireGuardClient.sessionAgeMs();
  const uint32_t startedAgo = wireGuardStartedAt ? (now - wireGuardStartedAt) : 0;

  const bool changed = (int8_t)peerUp != lastPeerUp ||
                       (int8_t)netUp != lastNetworkUp ||
                       (uint8_t)sessionFresh != lastSessionFresh;

  // Suppress periodic logs once we're fully up — only log state changes after that.
  const bool tick = !netUp && (lastLogAt == 0 || now - lastLogAt >= HANDSHAKE_LOG_INTERVAL_MS);

  if (!changed && !tick) return;

  lastLogAt = now;
  lastPeerUp = peerUp;
  lastNetworkUp = netUp;
  lastSessionFresh = sessionFresh;

  SLOG(SLOG_WG, "[WG     ] state: started=%lus peer_up=%d session_fresh=%d session_age=%s network_up=%d peer=%s:%u\n",
    (unsigned long)(startedAgo / 1000),
    peerUp ? 1 : 0,
    sessionFresh ? 1 : 0,
    sessionAge == UINT32_MAX ? "n/a" : String((unsigned long)sessionAge).c_str(),
    netUp ? 1 : 0,
    wireGuardClient.peerIp().c_str(),
    (unsigned)wireGuardClient.peerPort());
#else
  (void)now;
#endif
}

void wireGuardLoop()
{
  if (wireGuardApplyRequested) {
    wireGuardApplyConfig();
    return;
  }

  if (!wireGuardLoaded) wireGuardLoadConfig();
  if (!wireGuardConfig.enabled) return;

  if (!wireGuardStarted) {
    wireGuardEnsureStarted();
    return;
  }

  wireGuardClient.poll();
#ifdef ESP32
  wireGuardLogHandshakeProgress(millis());
  if (!firmwareUpdateInProgress) {
    wireGuardUpdateDnsLivenessCheck(millis());
  } else {
    wireGuardDnsCheckClose();
  }
#endif

  // Detect tunnel going down: interface no longer initialised.
  // Do NOT check isPeerUp()/isSessionFresh() here — those are for status display
  // and DNS liveness only.  With networkUp set immediately on wg.is_initialized(),
  // adding those checks caused an immediate teardown-reconnect loop because the
  // handshake hadn't completed yet when wireGuardNetworkUp first became true.
  if (wireGuardNetworkUp && !wireGuardClient.isNetworkUp()) {
    wireGuardClient.disconnect();
    wireGuardScheduleReconnect(F("Tunnel down; scheduling reconnect"));
  }

  // Initial-handshake stall path.
  //
  // We used to tear the tunnel down at WIREGUARD_CONNECT_TIMEOUT_MS and recreate
  // wg.begin() — but that destroys the underlying UDP socket and forces a NEW
  // home-router NAT mapping (different external source port).  MikroTik's
  // WireGuard caches the peer endpoint from the first session it saw and won't
  // re-point it to a new source port from a fresh initiation; combined with our
  // tear-down it created a deadlock where every retry cycle picked a new NAT
  // port the server was no longer talking to.
  //
  // The WireGuard library itself re-sends handshake initiations every 5 s on
  // the SAME UDP socket — that's the right behaviour because it keeps the NAT
  // mapping warm and the source port stable.  Don't disturb it: only emit a
  // periodic "still trying" log so the user can see the stall.  Recovery comes
  // from the server side (either PersistentKeepalive on the peer, or a fresh
  // session after MikroTik's REJECT_AFTER_TIME expires).
  static uint32_t lastStallLogAt = 0;
  if (!wireGuardNetworkUp && wireGuardStartedAt != 0 &&
      millis() - wireGuardStartedAt >= WIREGUARD_CONNECT_TIMEOUT_MS) {
    const uint32_t now = millis();
    if (lastStallLogAt == 0 || now - lastStallLogAt >= 60000) {
      lastStallLogAt = now;
      SLOG(SLOG_WG, "[WG     ] Handshake still pending after %lus — keeping UDP socket alive "
                    "(server cache may be stuck; set PersistentKeepalive on the peer)\n",
                    (unsigned long)((now - wireGuardStartedAt) / 1000));
    }
  }

  if (!wireGuardNetworkUp && wireGuardNextReconnectAt != 0 && millis() >= wireGuardNextReconnectAt) {
    wireGuardNextReconnectAt = 0;
    wireGuardEnsureStarted();
  }
}

#else

void wireGuardApplyConfig()
{
}

void wireGuardRequestApplyConfig()
{
}

void wireGuardLoop()
{
}

void wireGuardDisconnect()
{
}

bool wireGuardIsEnabled()
{
  return false;
}

bool wireGuardIsNetworkUp()
{
  return false;
}

WireGuardStatus wireGuardGetStatus()
{
  WireGuardStatus status;
  status.enabled = false;
  status.started = false;
  status.connected = false;
  status.networkUp = false;
  status.reconnecting = false;
  status.localIp = F("disabled");
  status.configuredEndpoint = F("-");
  status.resolvedEndpointIp = F("-");
  status.peerIp = F("-");
  status.peerPort = 0;
  status.dnsServers = F("-");
  status.dnsLivenessCheck = false;
  status.dnsLivenessInterval = 0;
  status.dnsLivenessFailures = 0;
  return status;
}

bool wireGuardEnsureStarted()
{
  return false;
}

bool wireGuardUseVpnRoute(const char *)
{
  return false;
}

bool wireGuardUseWifiRoute(const char *)
{
  return true;
}

void wireGuardLogDefaultRoute(const char *)
{
}

#endif
