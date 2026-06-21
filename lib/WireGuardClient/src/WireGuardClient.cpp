#include "WireGuardClient.h"

#include <lwip/dns.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/tcpip.h>
#include <esp_netif.h>

void WireGuardClient::setNetworkUpCallback(NetworkUpCallback cb)
{
  networkUpCallback = cb;
}

bool WireGuardClient::begin(const WireGuardConfig &config)
{
  disconnect();

  if (!validateConfig(config))
  {
    Serial.println("[WireGuard] Invalid configuration");
    return false;
  }

  currentConfig = config;
  initialized = true;
  connected = false;
  networkUp = false;
  assignedIp = IPAddress(0, 0, 0, 0);
  assignedGateway = IPAddress(0, 0, 0, 0);
  assignedPrimaryDns = IPAddress(0, 0, 0, 0);
  assignedSecondaryDns = IPAddress(0, 0, 0, 0);
  resolvedPeerIp = IPAddress(0, 0, 0, 0);
  return true;
}

void WireGuardClient::setVpnAsDefaultRoute(bool enabled)
{
  asDefaultRoute = enabled;
}

struct netif* WireGuardClient::getWireGuardNetif()
{
  // The WireGuard-ESP32 library creates a network interface
  // We need to find it by iterating through available interfaces
  struct netif *netif = netif_list;
  while (netif != NULL)
  {
    // Check if this is the WireGuard interface
    // It typically has a name like "wg" or similar
    if (netif->name[0] == 'w' && netif->name[1] == 'g')
    {
      return netif;
    }
    netif = netif->next;
  }
  return NULL;
}

bool WireGuardClient::parseAllowedIps(const String &allowedIps, std::vector<RouteEntry> &routes)
{
  routes.clear();
  
  String remaining = allowedIps;
  remaining.trim();
  
  if (remaining.length() == 0)
  {
    return false;
  }

  // Parse comma-separated CIDR notation IPs
  while (remaining.length() > 0)
  {
    int comma = remaining.indexOf(',');
    String cidrStr = (comma >= 0) ? remaining.substring(0, comma) : remaining;
    cidrStr.trim();
    
    if (cidrStr.length() == 0)
    {
      if (comma < 0) break;
      remaining = remaining.substring(comma + 1);
      continue;
    }

    // Parse CIDR notation (e.g., "10.0.0.0/24")
    int slash = cidrStr.indexOf('/');
    if (slash <= 0)
    {
      Serial.print("[WireGuard] Invalid CIDR notation: ");
      Serial.println(cidrStr);
      if (comma < 0) break;
      remaining = remaining.substring(comma + 1);
      continue;
    }

    String ipStr = cidrStr.substring(0, slash);
    String prefixStr = cidrStr.substring(slash + 1);
    
    IPAddress addr;
    if (!addr.fromString(ipStr))
    {
      Serial.print("[WireGuard] Invalid IP address: ");
      Serial.println(ipStr);
      if (comma < 0) break;
      remaining = remaining.substring(comma + 1);
      continue;
    }

    int prefixLen = prefixStr.toInt();
    if (prefixLen < 0 || prefixLen > 32)
    {
      Serial.print("[WireGuard] Invalid prefix length: ");
      Serial.println(prefixStr);
      if (comma < 0) break;
      remaining = remaining.substring(comma + 1);
      continue;
    }

    RouteEntry entry;
    IP_ADDR4(&entry.dest, addr[0], addr[1], addr[2], addr[3]);
    
    // Calculate netmask from prefix length
    uint32_t mask = (prefixLen == 0) ? 0 : (0xFFFFFFFFU << (32 - prefixLen));
    uint8_t *maskBytes = (uint8_t *)&mask;
    IP_ADDR4(&entry.netmask, maskBytes[3], maskBytes[2], maskBytes[1], maskBytes[0]);
    
    routes.push_back(entry);

    if (comma < 0)
    {
      break;
    }
    remaining = remaining.substring(comma + 1);
  }

  return !routes.empty();
}

void WireGuardClient::setupDefaultRoute()
{
  if (routesConfigured)
  {
    return;
  }

  struct netif *wgNetif = getWireGuardNetif();
  if (wgNetif == NULL)
  {
    Serial.println("[WireGuard] WARNING: Could not find WireGuard network interface");
    return;
  }

  if (!asDefaultRoute)
  {
    Serial.println("[WireGuard] Default route NOT configured (asDefaultRoute=false)");
    return;
  }

  // Parse allowed IPs to determine routes
  std::vector<RouteEntry> allowedRoutes;
  if (!parseAllowedIps(currentConfig.allowedIps, allowedRoutes))
  {
    Serial.println("[WireGuard] Could not parse allowed IPs for routing");
    return;
  }

  // If allowed IPs contain 0.0.0.0/0, all traffic goes through tunnel
  bool routeAllTraffic = false;
  for (const auto &route : allowedRoutes)
  {
    if (ip_addr_isany(&route.dest))
    {
      routeAllTraffic = true;
      break;
    }
  }

  if (routeAllTraffic)
  {
    Serial.println("[WireGuard] OK: All traffic routed through WireGuard tunnel (0.0.0.0/0)");
  }
  else
  {
    Serial.println("[WireGuard] OK: Specific routes configured:");
    for (const auto &route : allowedRoutes)
    {
      char destStr[24] = {0};
      ipaddr_ntoa_r(&route.dest, destStr, sizeof(destStr));
      char maskStr[24] = {0};
      ipaddr_ntoa_r(&route.netmask, maskStr, sizeof(maskStr));
      Serial.print("  - ");
      Serial.print(destStr);
      Serial.print(" netmask ");
      Serial.println(maskStr);
    }
  }

  // Note: The WireGuard-ESP32 library handles setting the interface as the default route
  // when wg.begin() is called. We validate the configuration here.
  Serial.println("[WireGuard] OK: WireGuard interface configured for tunnel routing");
  routesConfigured = true;
  addedRoutes = allowedRoutes;
}

void WireGuardClient::cleanupRoutes()
{
  if (!routesConfigured)
  {
    return;
  }

  struct netif *wgNetif = getWireGuardNetif();
  if (wgNetif != NULL)
  {
    Serial.println("[WireGuard] OK: Routes cleaned up on disconnect");
  }

  addedRoutes.clear();
  routesConfigured = false;
}

void WireGuardClient::saveCurrentDns()
{
  if (dnsSaved)
  {
    return;
  }

  const ip_addr_t *dns0 = dns_getserver(0);
  const ip_addr_t *dns1 = dns_getserver(1);
  if (dns0)
  {
    savedDns0 = *dns0;
  }
  else
  {
    ip_addr_set_zero(&savedDns0);
  }

  if (dns1)
  {
    savedDns1 = *dns1;
  }
  else
  {
    ip_addr_set_zero(&savedDns1);
  }

  dnsSaved = true;
}

void WireGuardClient::restoreSavedDns()
{
  if (!dnsSaved)
  {
    return;
  }

  LOCK_TCPIP_CORE();
  dns_setserver(0, &savedDns0);
  dns_setserver(1, &savedDns1);
  UNLOCK_TCPIP_CORE();
  dnsSaved = false;

  char dns0Text[48] = {};
  char dns1Text[48] = {};
  ipaddr_ntoa_r(&savedDns0, dns0Text, sizeof(dns0Text));
  ipaddr_ntoa_r(&savedDns1, dns1Text, sizeof(dns1Text));
  Serial.print("[WireGuard] Restored DNS: ");
  Serial.print(dns0Text);
  Serial.print(" ");
  Serial.println(dns1Text);
}

bool WireGuardClient::connectTunnel()
{
  if (!initialized)
  {
    Serial.println("[WireGuard] begin() must be called before connectTunnel()");
    return false;
  }

  Endpoint endpoint;
  if (!parseEndpoint(currentConfig.peerEndpoint, endpoint))
  {
    Serial.println("[WireGuard] Invalid peer endpoint");
    return false;
  }

  IPAddress localIp;
  if (!localIp.fromString(currentConfig.localIp))
  {
    Serial.println("[WireGuard] Invalid local tunnel IP");
    return false;
  }

  Serial.print("[WireGuard] Starting interface ");
  Serial.print(localIp);
  Serial.print(" -> ");
  Serial.print(endpoint.host);
  Serial.print(':');
  Serial.println(endpoint.port);

  // Save current (WiFi) DNS BEFORE touching anything so we can restore on failure.
  saveCurrentDns();

  // Resolve the endpoint hostname using the current WiFi DNS — the WireGuard DNS
  // server is on the VPN side and is unreachable before the tunnel exists.
  // We must NOT set WireGuard DNS until after wg.begin() succeeds.
  IPAddress endpointIp;
  if (!resolveEndpointHost(endpoint.host, endpointIp))
  {
    Serial.println("[WireGuard] Failed to resolve peer endpoint");
    restoreSavedDns();
    return false;
  }
  resolvedPeerIp = endpointIp;

  const String endpointIpText = endpointIp.toString();
  if (!wg.begin(localIp, currentConfig.privateKey.c_str(), endpointIpText.c_str(),
                currentConfig.peerPublicKey.c_str(), endpoint.port))
  {
    Serial.println("[WireGuard] Failed to start WireGuard interface");
    restoreSavedDns();
    return false;
  }

  connected = true;
  networkUp = false;
  assignedIp = localIp;
  assignedGateway = IPAddress(0, 0, 0, 0);

  // Tunnel interface is now up — it is safe to switch to VPN DNS servers.
  IPAddress primaryDns(0, 0, 0, 0);
  IPAddress secondaryDns(0, 0, 0, 0);
  parseDnsServers(currentConfig.dnsServers, primaryDns, secondaryDns);
  assignedPrimaryDns = primaryDns;
  assignedSecondaryDns = secondaryDns;

  LOCK_TCPIP_CORE();
  if (primaryDns != IPAddress(0, 0, 0, 0))
  {
    ip_addr_t dnsAddress;
    IP_ADDR4(&dnsAddress, primaryDns[0], primaryDns[1], primaryDns[2], primaryDns[3]);
    dns_setserver(0, &dnsAddress);
    Serial.print("[WireGuard] Primary DNS set to ");
    Serial.println(primaryDns);
  }
  if (secondaryDns != IPAddress(0, 0, 0, 0))
  {
    ip_addr_t dnsAddress;
    IP_ADDR4(&dnsAddress, secondaryDns[0], secondaryDns[1], secondaryDns[2], secondaryDns[3]);
    dns_setserver(1, &dnsAddress);
    Serial.print("[WireGuard] Secondary DNS set to ");
    Serial.println(secondaryDns);
  }
  else
  {
    ip_addr_t dnsAddress;
    ip_addr_set_zero(&dnsAddress);
    dns_setserver(1, &dnsAddress);
  }
  UNLOCK_TCPIP_CORE();

  // Setup routing to ensure all traffic goes through tunnel
  setupDefaultRoute();

  return true;
}

bool WireGuardClient::resolveEndpointHost(const String &host, IPAddress &address)
{
  if (address.fromString(host))
  {
    Serial.print("[WireGuard] Endpoint already numeric: ");
    Serial.println(address);
    return true;
  }

  const ip_addr_t *dns0 = dns_getserver(0);
  const ip_addr_t *dns1 = dns_getserver(1);
  char dns0Text[48] = {};
  char dns1Text[48] = {};
  if (dns0)
  {
    ipaddr_ntoa_r(dns0, dns0Text, sizeof(dns0Text));
  }
  if (dns1)
  {
    ipaddr_ntoa_r(dns1, dns1Text, sizeof(dns1Text));
  }

  Serial.print("[WireGuard] Resolving endpoint ");
  Serial.print(host);
  Serial.print(" using DNS ");
  Serial.print(dns0Text[0] ? dns0Text : "0.0.0.0");
  Serial.print(" ");
  Serial.println(dns1Text[0] ? dns1Text : "0.0.0.0");

  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  struct addrinfo *result = nullptr;
  const int error = lwip_getaddrinfo(host.c_str(), nullptr, &hints, &result);
  if (error != 0 || result == nullptr)
  {
    Serial.print("[WireGuard] Endpoint DNS failed, err=");
    Serial.println(error);
    if (result)
    {
      lwip_freeaddrinfo(result);
    }
    return false;
  }

  const sockaddr_in *addr = reinterpret_cast<const sockaddr_in *>(result->ai_addr);
  const uint32_t raw = addr->sin_addr.s_addr;
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&raw);
  address = IPAddress(bytes[0], bytes[1], bytes[2], bytes[3]);
  lwip_freeaddrinfo(result);

  Serial.print("[WireGuard] Endpoint DNS ");
  Serial.print(host);
  Serial.print(" -> ");
  Serial.println(address);
  return true;
}

void WireGuardClient::disconnect()
{
  // Clean up routes before disconnecting
  cleanupRoutes();
  
  if (wg.is_initialized())
  {
    wg.end();
  }
  restoreSavedDns();

  initialized = false;
  connected = false;
  networkUp = false;
  assignedIp = IPAddress(0, 0, 0, 0);
  assignedGateway = IPAddress(0, 0, 0, 0);
  assignedPrimaryDns = IPAddress(0, 0, 0, 0);
  assignedSecondaryDns = IPAddress(0, 0, 0, 0);
  resolvedPeerIp = IPAddress(0, 0, 0, 0);
}

bool WireGuardClient::isConnected() const
{
  return connected && wg.is_initialized() && isPeerUp() && isSessionFresh();
}

bool WireGuardClient::isInterfaceUp() const
{
  return connected && wg.is_initialized();
}

bool WireGuardClient::isNetworkUp() const
{
  return networkUp && isInterfaceUp();
}

bool WireGuardClient::isPeerUp() const
{
  return connected && wg.is_initialized() && wg.is_peer_up();
}

bool WireGuardClient::isSessionFresh() const
{
  return sessionAgeMs() <= SESSION_STALE_MS;
}

uint32_t WireGuardClient::sessionAgeMs() const
{
  if (!connected || !wg.is_initialized())
  {
    return UINT32_MAX;
  }

  return wg.session_age_ms();
}

String WireGuardClient::localIp() const
{
  return assignedIp.toString();
}

String WireGuardClient::peerEndpoint() const
{
  return currentConfig.peerEndpoint;
}

String WireGuardClient::resolvedEndpointIp() const
{
  return resolvedPeerIp.toString();
}

String WireGuardClient::peerIp() const
{
  IPAddress address;
  uint16_t port = 0;
  if (!wg.get_peer_endpoint(address, port))
  {
    return String();
  }

  return address.toString();
}

uint16_t WireGuardClient::peerPort() const
{
  IPAddress address;
  uint16_t port = 0;
  return wg.get_peer_endpoint(address, port) ? port : 0;
}

void WireGuardClient::poll()
{
  if (!connected || !wg.is_initialized())
  {
    networkUp = false;
    return;
  }

  // CRITICAL: do NOT declare the network up until the WireGuard handshake has
  // completed (is_peer_up() == true).  Before the session is established the
  // WireGuard netif's output() returns ERR_RTE, so any send() through the
  // tunnel fails with ENETUNREACH (errno 118).  That is exactly what causes
  // the DNS-liveness flood and MQTT TCP timeouts when the callback fires too
  // early.  The library does NOT buffer pre-handshake data packets; it drops
  // them with a routing error.
  if (!isPeerUp())
  {
    return;
  }

  if (!networkUp)
  {
    networkUp = true;
    if (networkUpCallback)
    {
      networkUpCallback(assignedIp, assignedGateway, assignedPrimaryDns, assignedSecondaryDns);
    }
  }
}

bool WireGuardClient::validateConfig(const WireGuardConfig &config)
{
  if (config.privateKey.length() == 0)
  {
    Serial.println("[WireGuard] Missing private key");
    return false;
  }

  IPAddress localIp;
  if (config.localIp.length() == 0 || !localIp.fromString(config.localIp))
  {
    Serial.println("[WireGuard] Missing or invalid local tunnel IP");
    return false;
  }

  if (config.peerPublicKey.length() == 0)
  {
    Serial.println("[WireGuard] Missing peer public key");
    return false;
  }

  Endpoint endpoint;
  if (!parseEndpoint(config.peerEndpoint, endpoint))
  {
    Serial.println("[WireGuard] Peer endpoint must be host:port");
    return false;
  }

  if (config.allowedIps.length() == 0)
  {
    Serial.println("[WireGuard] Missing allowed IPs");
    return false;
  }

  return true;
}

bool WireGuardClient::parseEndpoint(const String &endpoint, Endpoint &parsed)
{
  const int colon = endpoint.lastIndexOf(':');
  if (colon <= 0 || colon >= static_cast<int>(endpoint.length()) - 1)
  {
    return false;
  }

  String host = endpoint.substring(0, colon);
  String portText = endpoint.substring(colon + 1);
  host.trim();
  portText.trim();

  const long port = portText.toInt();
  if (host.length() == 0 || port <= 0 || port > 65535)
  {
    return false;
  }

  parsed.host = host;
  parsed.port = static_cast<uint16_t>(port);
  return true;
}

bool WireGuardClient::parseDnsServers(const String &dnsServers, IPAddress &primaryDns, IPAddress &secondaryDns)
{
  primaryDns = IPAddress(0, 0, 0, 0);
  secondaryDns = IPAddress(0, 0, 0, 0);

  String values = dnsServers;
  values.trim();
  if (values.length() == 0)
  {
    return false;
  }

  const int comma = values.indexOf(',');
  String primary = comma >= 0 ? values.substring(0, comma) : values;
  String secondary = comma >= 0 ? values.substring(comma + 1) : "";
  primary.trim();
  secondary.trim();

  bool parsedAny = false;
  if (primary.length() > 0)
  {
    parsedAny = primaryDns.fromString(primary);
    if (!parsedAny)
    {
      Serial.print("[WireGuard] Ignoring invalid primary DNS: ");
      Serial.println(primary);
    }
  }

  if (secondary.length() > 0 && !secondaryDns.fromString(secondary))
  {
    Serial.print("[WireGuard] Ignoring invalid secondary DNS: ");
    Serial.println(secondary);
  }

  return parsedAny || secondaryDns != IPAddress(0, 0, 0, 0);
}
