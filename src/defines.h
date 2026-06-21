// ============================================================
// BUILD PROFILE SELECTOR
// Uncomment exactly ONE profile below, comment out the others.
// ============================================================

// #define PROFILE_SINGLE_RELAY        // HWESP32 · 1 relay · ACS712 · no extra hardware
// #define PROFILE_SINGLE_RELAY_AUTO   // HWESP32 · 1 relay · ACS712 · Automation Rules + Remote Sensors
 #define PROFILE_WATER_PRESSURE      // HWESP32 + ESP32_2RBoard · pressure sensor · water flow · OLED
// #define PROFILE_SOLAR_VOLTMETER     // HWESP32 + ESP32_2RBoard · emonlib · ADS1X15 · solar heater · water flow

// ============================================================

#if defined(PROFILE_SINGLE_RELAY)
  #include "defines-template-Single Relay ESP32.h"
#elif defined(PROFILE_SINGLE_RELAY_AUTO)
  #include "defines-template-Single Relay ESP32 Auto.h"
#elif defined(PROFILE_WATER_PRESSURE)
  #include "defines template water pressure sensor.h"
#elif defined(PROFILE_SOLAR_VOLTMETER)
  #include "defines-template-SolarVoltmeter.h"
#else
  #error "No build profile selected. Uncomment exactly one PROFILE_xxx line in defines.h."
#endif
