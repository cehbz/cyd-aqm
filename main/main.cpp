#include "display.hpp"
#include "sen55.hpp"
#include "ui.hpp"

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"

#include <cstdio>
#include <ctime>

namespace {
const char* TAG = "main";
constexpr auto kSen55Sda = GPIO_NUM_11;
constexpr auto kSen55Scl = GPIO_NUM_12;
} // anonymous namespace

extern "C" void app_main()
{
    // 1. Display + LVGL
    ESP_ERROR_CHECK(display::Init());

    // 2. SEN55 I2C bus
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = kSen55Sda,
        .scl_io_num = kSen55Scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };
    i2c_master_bus_handle_t sen55_bus{};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &sen55_bus));

    // 3. UI (static lifetime — outlives app_main)
    static Ui* ui = nullptr;
    if (lvgl_port_lock(0)) {
        static auto ui_obj = Ui();
        ui = &ui_obj;
        lvgl_port_unlock();
    }

    // 4. Sensor — starts measuring immediately, calls back at ~1 Hz
    static auto sensor = Sen55(sen55_bus, [](const Sen55::Measurement& m) {
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        char ts[32];
        std::snprintf(ts, sizeof(ts), "Updated %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (lvgl_port_lock(0)) {
            ui->UpdateMeasurements(m);
            ui->SetStatus(ts);
            lvgl_port_unlock();
        }
    });

    ESP_LOGI(TAG, "Air quality monitor running");
}
