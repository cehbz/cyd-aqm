#include "wifi.hpp"
#include "credentials.h"

#include <cstring>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

namespace {

const char *TAG = "wifi";

constexpr int CONNECTED_BIT = BIT0;
constexpr int FAIL_BIT      = BIT1;

EventGroupHandle_t s_events{};
bool s_connected{false};
int  s_retry_count{0};

// Backoff: immediate, 1s, 5s, 10s, 30s cap
int retry_delay_ms()
{
    constexpr int delays[] = {0, 1000, 5000, 10000, 30000};
    constexpr int n = sizeof(delays) / sizeof(delays[0]);
    int idx = std::min(s_retry_count, n - 1);
    return delays[idx];
}

void event_handler(void * /*arg*/, esp_event_base_t base,
                   int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        int delay = retry_delay_ms();
        s_retry_count++;
        ESP_LOGW(TAG, "Disconnected — reconnecting in %d ms (attempt %d)",
                 delay, s_retry_count);
        if (delay > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
        esp_wifi_connect();
        xEventGroupSetBits(s_events, FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "Connected — IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        s_retry_count = 0;
        xEventGroupSetBits(s_events, CONNECTED_BIT);
    }
}

} // namespace

esp_err_t wifi_init()
{
    // NVS (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr, nullptr));

    wifi_config_t wifi_cfg{};
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid),
                 WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password),
                 WIFI_PASS, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to %s ...", WIFI_SSID);
    return ESP_OK;
}

bool wifi_wait_connected(int timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(
        s_events,
        CONNECTED_BIT | FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    return (bits & CONNECTED_BIT) != 0;
}

bool wifi_is_connected()
{
    return s_connected;
}
