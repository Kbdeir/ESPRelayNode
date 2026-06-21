#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <lwip/ip_addr.h>

class AsyncNtpClient
{
public:
  void begin(const String &server, uint16_t syncIntervalMinutes, bool syncEnabled);
  void stop();
  void update();
  bool needsNetworkWork() const;
  bool isSynced() const;
  uint32_t lastSyncEpoch() const;
  uint32_t lastSyncMillis() const;

private:
  static void handleDnsResult(const char *name, const ip_addr_t *ipaddr, void *arg);
  bool selectRoute();
  void restorePrimaryRoute();
  bool serverAddressIsOnWifiSubnet() const;
  bool shouldUseVpnRoute() const;
  void startResolve(uint32_t now);
  void sendRequest(uint32_t now);
  void handlePacket(const uint8_t *data, size_t length);
  void pollResponse();
  bool openSocket();
  void resetSocket();

  String ntpServer;
  ip_addr_t serverAddress = {};
  int udpSocket = -1;
  bool enabled = false;
  bool connected = false;
  bool resolving = false;
  bool addressResolved = false;
  bool synced = false;
  bool routeLogValid = false;
  bool loggedUseVpn = false;
  uint32_t lastRequestAt = 0;
  uint32_t lastSyncAt = 0;
  uint32_t syncedEpoch = 0;
  uint32_t syncIntervalMs = 300000;
};
