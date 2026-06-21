#pragma once

#include <Arduino.h>

struct WireGuardStatus
{
  bool enabled = false;
  bool started = false;
  bool connected = false;
  bool networkUp = false;
  bool reconnecting = false;
  String localIp;
  String configuredEndpoint;
  String resolvedEndpointIp;
  String peerIp;
  uint16_t peerPort = 0;
  String dnsServers;
  bool dnsLivenessCheck = false;
  uint16_t dnsLivenessInterval = 0;
  uint8_t dnsLivenessFailures = 0;
};

void wireGuardApplyConfig();
void wireGuardRequestApplyConfig();
void wireGuardLoop();
void wireGuardDisconnect();
bool wireGuardIsEnabled();
bool wireGuardIsNetworkUp();
WireGuardStatus wireGuardGetStatus();
bool wireGuardEnsureStarted();
bool wireGuardUseVpnRoute(const char *reason);
bool wireGuardUseWifiRoute(const char *reason);
void wireGuardLogDefaultRoute(const char *reason);
