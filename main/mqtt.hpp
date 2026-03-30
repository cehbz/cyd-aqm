#pragma once

#include "esp_err.h"

/// Callback for incoming MQTT data.
using mqtt_data_cb_t = void (*)(const char *topic, int topic_len,
                                const char *data, int data_len);

/// Callback invoked on every MQTT connect (including reconnects).
/// Use to publish discovery, subscribe to topics, etc.
using mqtt_connect_cb_t = void (*)();

/// Initialise MQTT client with LWT on the availability topic.
/// \param device_id  Used to build the availability topic: aqm/<device_id>/availability
/// \param on_connect Called on every MQTT_EVENT_CONNECTED
/// \param on_data    Called on every MQTT_EVENT_DATA
esp_err_t mqtt_init(const char *device_id,
                    mqtt_connect_cb_t on_connect,
                    mqtt_data_cb_t on_data);

/// True if the MQTT client is currently connected.
bool mqtt_is_connected();

/// Publish a message. Returns the message ID or -1 on error.
int mqtt_publish(const char *topic, const char *data,
                 int qos = 0, bool retain = false);

/// Subscribe to a topic. Returns the message ID or -1 on error.
int mqtt_subscribe(const char *topic, int qos = 0);
