#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <functional>
#include <vector>

#include <WireGuard-ESP32.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <esp_netif.h>

struct WireGuardConfig
{
  String privateKey;
  String localIp;
  String peerPublicKey;
  String peerEndpoint;
  String allowedIps;
  String dnsServers;

  uint32_t connectTimeoutMs = 15000;
  uint32_t keepaliveIntervalSeconds = 25;
};

struct RouteEntry
{
  ip_addr_t dest;
  ip_addr_t netmask;
};

class WireGuardClient
{
public:
  using NetworkUpCallback = std::function<void(IPAddress assignedIp, IPAddress gateway, IPAddress primaryDns, IPAddress secondaryDns)>;

  void setNetworkUpCallback(NetworkUpCallback cb);

  bool begin(const WireGuardConfig &config);
  void setVpnAsDefaultRoute(bool enabled);

  bool connectTunnel();
  void disconnect();

  bool isConnected() const;
  bool isInterfaceUp() const;
  bool isNetworkUp() const;
  bool isPeerUp() const;
  bool isSessionFresh() const;
  uint32_t sessionAgeMs() const;
  String localIp() const;
  String peerEndpoint() const;
  String resolvedEndpointIp() const;
  String peerIp() const;
  uint16_t peerPort() const;

  void poll();

  static bool validateConfig(const WireGuardConfig &config);

private:
  struct Endpoint
  {
    String host;
    uint16_t port = 0;
  };

  static bool parseEndpoint(const String &endpoint, Endpoint &parsed);
  static bool resolveEndpointHost(const String &host, IPAddress &address);
  static bool parseDnsServers(const String &dnsServers, IPAddress &primaryDns, IPAddress &secondaryDns);
  static bool parseAllowedIps(const String &allowedIps, std::vector<RouteEntry> &routes);
  
  void setupDefaultRoute();
  void cleanupRoutes();
  void saveCurrentDns();
  void restoreSavedDns();
  
  struct netif* getWireGuardNetif();

  WireGuard wg;
  WireGuardConfig currentConfig;
  NetworkUpCallback networkUpCallback = nullptr;
  std::vector<RouteEntry> addedRoutes;
  IPAddress assignedIp;
  IPAddress assignedGateway;
  IPAddress assignedPrimaryDns;
  IPAddress assignedSecondaryDns;
  IPAddress resolvedPeerIp;
  bool initialized = false;
  bool connected = false;
  bool networkUp = false;
  bool asDefaultRoute = true;
  bool routesConfigured = false;
  bool dnsSaved = false;
  ip_addr_t savedDns0 = {};
  ip_addr_t savedDns1 = {};

  static constexpr uint32_t SESSION_STALE_MS = 130000;
};
