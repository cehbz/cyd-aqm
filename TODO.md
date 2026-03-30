# CYD Gateway TODO

The CYD (ESP32-S3, SEN55, 4.3" display) becomes the ESP-NOW gateway for Home Assistant integration. Three independent roles: display (unchanged), SEN55 → MQTT publisher, ESP-NOW gateway.

## Phase 1: WiFi + MQTT + SEN55 Publishing

- [ ] Add `wifi.hpp/cpp` — WiFi STA with auto-reconnect
- [ ] Add `mqtt.hpp/cpp` — esp-mqtt client, LWT, publish/subscribe helpers
- [ ] Add `device_id.hpp/cpp` — MAC-derived unique device ID
- [ ] Add `ha_discovery.hpp/cpp` — SEN55 discovery payloads (8 entities)
- [ ] Add `credentials.h.template` — WiFi SSID/pass, MQTT broker URI
- [ ] Update `main.cpp` — init WiFi, MQTT; hook MQTT publish into Sen55 callback
- [ ] Update `idf_component.yml` — add espressif/mqtt, espressif/cjson
- [ ] Update `CMakeLists.txt` — add new source files
- [ ] Verify: display unchanged, 8 SEN55 entities appear in HA

## Phase 2: ESP-NOW Gateway

- [ ] Add `espnow_protocol.h` — shared message definitions
- [ ] Add `espnow_bridge.hpp/cpp` — ESP-NOW recv → MQTT; MQTT cmd → ESP-NOW send
- [ ] Add `device_registry.hpp/cpp` — track remote devices by MAC, heartbeat watchdog
- [ ] Update `ha_discovery.hpp/cpp` — add PMS5003 + PWM12V discovery templates
- [ ] Update `main.cpp` — init ESP-NOW
- [ ] Verify: gateway receives ESP-NOW, publishes to MQTT, HA discovers remote devices
