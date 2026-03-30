#pragma once

#include "esp_err.h"

/// Initialise NVS, netif, event loop, WiFi STA and start connection.
/// Auto-reconnects on disconnect with backoff.
esp_err_t wifi_init();

/// Block until connected or timeout. Returns true if connected.
bool wifi_wait_connected(int timeout_ms = 15000);

/// True if WiFi STA currently has an IP.
bool wifi_is_connected();
