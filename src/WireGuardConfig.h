#pragma once

#include "Arduino.h"
#include <JSONConfig.h>

class StoredWireGuardConfig
{
  public:
    boolean enabled;
    String protocol;
    String privateKey;
    String localIp;
    String peerPublicKey;
    String peerEndpoint;
    String allowedIps;
    String dnsServers;
    boolean dnsLivenessCheck;
    uint16_t dnsLivenessInterval;

    StoredWireGuardConfig();
};

bool saveWireGuardConfig(AsyncWebServerRequest *request);
config_read_error_t loadWireGuardConfig(const char *filename, StoredWireGuardConfig &config);
