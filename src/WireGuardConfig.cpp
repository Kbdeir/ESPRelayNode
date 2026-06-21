#include <WireGuardConfig.h>

#define WIREGUARD_CONFIG_BUFFER_SIZE 768

StoredWireGuardConfig::StoredWireGuardConfig()
{
  enabled = false;
  protocol = "WireGuard";
  privateKey = "";
  localIp = "10.0.0.2";
  peerPublicKey = "";
  peerEndpoint = "";
  allowedIps = "0.0.0.0/0";
  dnsServers = "";
  dnsLivenessCheck = false;
  dnsLivenessInterval = 30;
}

bool saveWireGuardConfig(AsyncWebServerRequest *request)
{
  StaticJsonDocument<WIREGUARD_CONFIG_BUFFER_SIZE> json;
  File configFile = SPIFFS.open("/WireGuardConfig.json", "w");
  if (!configFile) {
    Serial.println(F("Failed to open WireGuardConfig file for writing"));
    return false;
  }

  json["VPNEnabled"] = request->hasParam("VPNEnabled") ? "1" : "0";
  json["VPNProtocol"] = "WireGuard";
  json["WGPrivateKey"] = request->hasParam("WGPrivateKey") ? request->getParam("WGPrivateKey")->value() : "";
  json["WGLocalIP"] = request->hasParam("WGLocalIP") ? request->getParam("WGLocalIP")->value() : "10.0.0.2";
  json["WGPeerPublicKey"] = request->hasParam("WGPeerPublicKey") ? request->getParam("WGPeerPublicKey")->value() : "";
  json["WGPeerEndpoint"] = request->hasParam("WGPeerEndpoint") ? request->getParam("WGPeerEndpoint")->value() : "";
  json["WGAllowedIPs"] = request->hasParam("WGAllowedIPs") ? request->getParam("WGAllowedIPs")->value() : "0.0.0.0/0";
  json["WGDnsServers"] = request->hasParam("WGDnsServers") ? request->getParam("WGDnsServers")->value() : "";
  json["WGDnsLivenessCheck"] = request->hasParam("WGDnsLivenessCheck") ? "1" : "0";
  json["WGDnsLivenessInterval"] = request->hasParam("WGDnsLivenessInterval") ? request->getParam("WGDnsLivenessInterval")->value() : "30";

  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write WireGuardConfig file"));
    configFile.close();
    return false;
  }

  configFile.flush();
  configFile.close();
  return true;
}

bool saveWireGuardConfigFromJson(const String& body)
{
  StaticJsonDocument<WIREGUARD_CONFIG_BUFFER_SIZE> req;
  if (deserializeJson(req, body) != DeserializationError::Ok) return false;
  File f = SPIFFS.open("/WireGuardConfig.json", "w");
  if (!f) return false;
  bool ok = (serializeJson(req, f) > 0);
  f.close();
  return ok;
}

config_read_error_t loadWireGuardConfig(const char *filename, StoredWireGuardConfig &config)
{
  if (!SPIFFS.exists(filename)) {
    Serial.println(F("\n[INFO   ] WireGuardConfig file does not exist!:"));
    Serial.print(filename);
    return FILE_NOT_FOUND;
  }

  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.println(F("\n[INFO   ] Failed to open WireGuardConfig file"));
    return ERROR_OPENING_FILE;
  }

  if (configFile.size() > WIREGUARD_CONFIG_BUFFER_SIZE) {
    Serial.println(F("\n[INFO   ] WireGuardConfig file size is too large."));
    configFile.close();
    return ERROR_OPENING_FILE;
  }

  StaticJsonDocument<WIREGUARD_CONFIG_BUFFER_SIZE> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println(F("Failed to parse WireGuardConfig file"));
    configFile.close();
    return JSONCONFIG_CORRUPTED;
  }

  config.enabled = (json["VPNEnabled"].as<String>() == "1");
  config.protocol = "WireGuard";
  config.privateKey = json["WGPrivateKey"].as<String>();
  config.localIp = json["WGLocalIP"].as<String>() != "" ? json["WGLocalIP"].as<String>() : "10.0.0.2";
  config.peerPublicKey = json["WGPeerPublicKey"].as<String>();
  config.peerEndpoint = json["WGPeerEndpoint"].as<String>();
  config.allowedIps = json["WGAllowedIPs"].as<String>() != "" ? json["WGAllowedIPs"].as<String>() : "0.0.0.0/0";
  config.dnsServers = json["WGDnsServers"].as<String>();
  config.dnsLivenessCheck = (json["WGDnsLivenessCheck"].as<String>() == "1");
  config.dnsLivenessInterval = json["WGDnsLivenessInterval"].as<String>() != "" ? json["WGDnsLivenessInterval"].as<uint16_t>() : 30;

  configFile.close();
  return SUCCESS;
}
