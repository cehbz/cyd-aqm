#include "esp32_8048s043.hpp"
#include "sen55.hpp"
#include "ui.hpp"
#include "device_id.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"
#include "ha_discovery.hpp"

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"

#include <cstdio>
#include <ctime>

namespace {

const char *TAG = "main";
constexpr auto kSen55Sda = GPIO_NUM_11;
constexpr auto kSen55Scl = GPIO_NUM_12;

// --- MQTT publishing for SEN55 readings ---

void publish_sen55(const Sen55::Measurement &m)
{
    if (!mqtt_is_connected()) return;

    const char *id = device_id_get();
    char topic[64];
    char value[16];

    struct { const char *entity; float val; int decimals; } fields[] = {
        {"pm1_0",    m.pm1_0,       1},
        {"pm2_5",    m.pm2_5,       1},
        {"pm4_0",    m.pm4_0,       1},
        {"pm10",     m.pm10,        1},
        {"temp",     m.temperature, 1},
        {"humidity", m.humidity,    1},
        {"voc",      m.voc_index,   0},
        {"nox",      m.nox_index,   0},
    };

    for (const auto &f : fields) {
        std::snprintf(topic, sizeof(topic), "aqm/%s/sensor/%s", id, f.entity);
        if (f.decimals == 0) {
            std::snprintf(value, sizeof(value), "%d", static_cast<int>(f.val));
        } else {
            std::snprintf(value, sizeof(value), "%.1f", f.val);
        }
        mqtt_publish(topic, value);
    }
}

// --- MQTT callbacks ---

void on_mqtt_connect()
{
    ha_discovery_publish_sen55(device_id_get());
}

void on_mqtt_data(const char * /*topic*/, int /*topic_len*/,
                  const char * /*data*/, int /*data_len*/)
{
    // No subscriptions in Phase 1 — will be used by ESP-NOW gateway in Phase 2
}

} // namespace

extern "C" void app_main()
{
    // 1. Display + LVGL
    ESP_ERROR_CHECK(esp32_8048s043::Init());

    // 2. Device identity
    device_id_init();
    ESP_LOGI(TAG, "Device ID: %s", device_id_get());

    // 3. SEN55 I2C bus
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_1;
    bus_cfg.sda_io_num = kSen55Sda;
    bus_cfg.scl_io_num = kSen55Scl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t sen55_bus{};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &sen55_bus));

    // 4. UI (static lifetime — outlives app_main)
    static Ui *ui = nullptr;
    if (lvgl_port_lock(0)) {
        static auto ui_obj = Ui();
        ui = &ui_obj;
        lvgl_port_unlock();
    }

    // 5. Sensor — starts measuring immediately, calls back at ~1 Hz
    static auto sensor = Sen55(sen55_bus, [](const Sen55::Measurement &m) {
        auto now = std::time(nullptr);
        auto *tm = std::localtime(&now);
        char ts[32];
        std::snprintf(ts, sizeof(ts), "Updated %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);

        // Update display
        if (lvgl_port_lock(0)) {
            ui->UpdateMeasurements(m);
            ui->SetStatus(ts);
            lvgl_port_unlock();
        }

        // Publish to MQTT
        publish_sen55(m);
    });

    // 6. WiFi + MQTT
    wifi_init();
    if (wifi_wait_connected(15000)) {
        ESP_LOGI(TAG, "WiFi connected");
    } else {
        ESP_LOGW(TAG, "WiFi not connected yet — will auto-reconnect");
    }

    mqtt_init(device_id_get(), on_mqtt_connect, on_mqtt_data);

    ESP_LOGI(TAG, "Air quality monitor + gateway running");
}
