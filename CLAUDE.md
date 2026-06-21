# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Development Commands

```bash
# Build firmware
pio run -e arduino-esp32

# Upload firmware
pio run -e arduino-esp32 --target upload

# Upload filesystem (web UI and config defaults in data/)
pio run -e arduino-esp32 --target uploadfs

# Serial monitor
pio device monitor --baud 115200

# Clean build artifacts
pio run -e arduino-esp32 --target clean

# Flash memory backup/restore (custom scripts)
pio run -e arduino-esp32 --target dump_flash
pio run -e arduino-esp32 --target restore_flash
```

There are no unit tests — validation is done via serial monitor output and MQTT state messages on real hardware.

## Architecture Overview

This is an ESP32 (NodeMCU-32S) IoT relay controller firmware for smart home/industrial automation. The entire application lives in `src/main.cpp` (~3,200 lines) which calls into support modules in `src/` and `include/`.

**Control interfaces:** MQTT (primary), Modbus TCP (port 502), Apple HomeKit, Alexa/Fauxmo, and a web UI served from LittleFS (`data/`).

**Storage:** Configuration is persisted as JSON files in LittleFS (`/config.json`, `/Relayconfig.json`, `/timer0.json`–`/timerN.json`, `/IRMAP.json`). The web UI pages (`data/*.html`) are flashed separately via `uploadfs`.

### Core execution model

`setup()` mounts LittleFS, loads JSON configs, connects WiFi, starts MQTT/Modbus/HomeKit, registers input callbacks, and starts the TaskScheduler. `loop()` drives `Scheduler_ts.execute()`, relay watch, input polling, MQTT keep-alive, and optional mesh/HomeKit/inverter servicing.

### Key modules

| Module | Files | Responsibility |
|---|---|---|
| Relay control | `RelayClass.h/cpp` | GPIO relay with TTL (auto-off) and TTA (delayed-on) timers; syncs HomeKit + MQTT on state change |
| Input sensing | `InputClass.h/cpp` | Debounced digital inputs (Bounce2); modes: normal, toggle, relay-copy |
| MQTT | `MQTT_Processes.h/cpp` | AsyncMqttClient; subscribe/publish; parses incoming JSON commands |
| Configuration | `JSONConfig.h/cpp`, `ConfigParams.h/cpp` | Load/save typed config structs from LittleFS JSON |
| Schedule timers | `Scheduletimer.h/cpp` | Microsecond/millisecond software timers; drives calendar-based relay automation |
| WiFi | `KSBWiFiHelper.h/cpp` | STA/AP mode, reconnection, event handling |
| Modbus TCP | `Modbus.h/cpp`, `ModbusIP.h/cpp` | Async TCP Modbus server; coil-mapped relay control |
| Power monitoring | `CT_ProcessPower.h/cpp`, `ADS11x5Config.h/cpp` | EmonLib CT current sensing via ADS1X15 ADC |
| Temperature | `TempSensor.h/cpp`, `TempConfig.h/cpp` | DS18B20 OneWire; dual-sensor solar heater control |
| Water flow | `WaterFlowSensor` (in main + config) | Interrupt-driven pulse counter; flow rate + cumulative volume |
| OTA | `ExecOTA.h/cpp` | Web-based firmware upload handler |
| Mesh | `ksbMesh.h/cpp` | painlessMesh WiFi mesh networking |
| Web UI | `AsyncHTTP_Helper.h/cpp` | ESPAsyncWebServer route registration and file serving |

### Feature flag system

All optional features are enabled/disabled via `#define` in `include/defines.h`. This is the single most important file for understanding what is compiled in. Key flags:

- **Hardware variant:** `HWESP32`, `ESP32_2RBoard`, `ESP32_4RBoard` (sets relay/input count and GPIO mapping)
- **Sensing:** `_emonlib_`, `_ADS1X15_`, `_ADS1015_`, `_pressureSensor_`, `WaterFlowSensor`, `_HST_`
- **Smart home:** `AppleHK`, `_ALEXA_`, `_ESP_ALEXA_`
- **Networking:** `ESP_MESH`, `ESP_MESH_ROOT`, `INVERTERLINK`
- **Display:** `OLED_1306`

Conditional compilation via these flags is pervasive throughout `main.cpp` and the support modules — always check `defines.h` before concluding a feature is absent.

### Configuration structures

- `TConfigParams` (in `ConfigParams.h`) — system-wide: WiFi creds, MQTT broker/topics, NTP, CT calibration, display settings
- `Trelayconf` — per-relay: TTL/TTA values, topic names, input bindings, temperature thresholds
- `TIRMap` — input-to-relay mapping vectors

JSON serialization/deserialization for all structures is in `JSONConfig.cpp`. When adding new config fields, update both the struct and the JSON load/save functions there.

### MQTT topic convention

Topics follow a `<location>/<component>/<action>` pattern defined in relay configuration. State changes are published as retained messages. TTL countdowns are published on a separate status topic during auto-off timers.

### Branch context

The active development branch `WithWaterFlowSensor` adds water flow sensor support (`WaterFlowSensor` define, `WaterFlowSensorConfig.json`, `data/WaterFlowSensor.html`).


Think before acting — identify root cause and state done criteria before touching any code

Keep it simple

Only change what was asked — no unrequested refactoring, renaming, or cleanup

Define done — explicit success criteria stated upfront, each verified before reporting complete
Minimize heap fragmentation — baseline 110K / 18.9% RAM protected; DynamicJsonDocument and unguarded String+= loops forbidden on hot paths; all template processor String builders must reserve() upfron
