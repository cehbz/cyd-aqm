#pragma once

/// Publish MQTT Discovery config for the local SEN55 sensor (8 entities).
/// Call on every MQTT connect (including reconnects).
void ha_discovery_publish_sen55(const char *device_id);
