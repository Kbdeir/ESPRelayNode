#pragma once

#include <Arduino.h>
#include <defines.h>

// Home Assistant MQTT discovery: publishes retained config payloads to
// homeassistant/<component>/<node_id>/<object_id>/config so relays and the
// compiled-in sensors (temperature, water flow, ACS712, CT/Emon power)
// appear automatically in HA without manual entity setup.
//
// Entirely opt-in via HA_DISCOVERY in defines.h. When the flag is not set,
// the calls below compile to nothing.

#ifdef HA_DISCOVERY

// Schedule the staggered (re)publication of all discovery configs, with the
// first publish at the given millis() timestamp (subsequent entities follow
// ~250ms apart). Safe to call again on every MQTT reconnect: payloads are
// retained and idempotent, so republishing just keeps HA in sync after a
// relay/sensor topic is renamed via the web UI.
void haDiscoveryBegin(unsigned long startAtMs);

// Call once per loop() iteration. Publishes at most one entity per call
// while a publish is pending, MQTT is connected, and the schedule allows it.
void haDiscoveryService();

#else

inline void haDiscoveryBegin(unsigned long) {}
inline void haDiscoveryService() {}

#endif // HA_DISCOVERY
