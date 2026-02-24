#include "display.hpp"
#include "sen55.hpp"
#include "ui.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <cstdio>
#include <ctime>

namespace {

const char* TAG = "main";

/* ── Configuration ───────────────────────────────────────────────────── */

constexpr auto kSen55Sda = GPIO_NUM_11;
constexpr auto kSen55Scl = GPIO_NUM_12;
constexpr int kSampleIntervalMs = 5 * 60 * 1000;  // 5 minutes
constexpr int kWarmupMs = 30'000;                  // PM stabilisation
constexpr int kDataReadyTimeoutMs = 5'000;

constexpr EventBits_t kEvtSensorRead = BIT0;

/* ── Shared state ────────────────────────────────────────────────────── */

EventGroupHandle_t g_sensor_evt{};

/* ── Callbacks ───────────────────────────────────────────────────────── */

void periodic_timer_cb(void* /*arg*/)
{
    xEventGroupSetBits(g_sensor_evt, kEvtSensorRead);
}

/* ── Sensor task ─────────────────────────────────────────────────────── */

struct SensorTaskArgs {
    Sen55* sensor;
    Ui* ui;
};

void sensor_task(void* arg)
{
    auto& ctx = *static_cast<SensorTaskArgs*>(arg);
    Sen55::Measurement meas{};

    // Trigger an immediate first reading
    xEventGroupSetBits(g_sensor_evt, kEvtSensorRead);

    for (;;) {
        xEventGroupWaitBits(g_sensor_evt, kEvtSensorRead,
                            pdTRUE, pdFALSE, portMAX_DELAY);

        // 1. Start measurement
        if (auto err = ctx.sensor->start_measurement(); err != ESP_OK) {
            ESP_LOGE(TAG, "start_measurement failed: %s", esp_err_to_name(err));
            if (lvgl_port_lock(0)) {
                ctx.ui->set_status("Sensor start error");
                lvgl_port_unlock();
            }
            continue;
        }

        if (lvgl_port_lock(0)) {
            ctx.ui->set_status("Warming up...");
            lvgl_port_unlock();
        }
        ESP_LOGI(TAG, "SEN55 warming up (%d s)...", kWarmupMs / 1000);
        vTaskDelay(pdMS_TO_TICKS(kWarmupMs));

        // 2. Poll data-ready
        bool ready = false;
        for (int elapsed = 0; !ready && elapsed < kDataReadyTimeoutMs; elapsed += 100) {
            if (auto err = ctx.sensor->read_data_ready(ready); err != ESP_OK) {
                ESP_LOGE(TAG, "data-ready read failed: %s", esp_err_to_name(err));
                break;
            }
            if (!ready) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        if (!ready) {
            ESP_LOGW(TAG, "SEN55 data not ready within timeout");
            (void)ctx.sensor->stop_measurement();
            if (lvgl_port_lock(0)) {
                ctx.ui->set_status("Sensor not ready");
                lvgl_port_unlock();
            }
            continue;
        }

        // 3. Read, then stop
        auto err = ctx.sensor->read_measured_values(meas);
        (void)ctx.sensor->stop_measurement();

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
            if (lvgl_port_lock(0)) {
                ctx.ui->set_status("Read error");
                lvgl_port_unlock();
            }
            continue;
        }

        ESP_LOGI(TAG, "PM2.5=%.1f  T=%.1f  RH=%.1f  VOC=%.0f  NOx=%.0f",
                 meas.pm2_5, meas.temperature, meas.humidity,
                 meas.voc_index, meas.nox_index);

        // 4. Format timestamp and update UI
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        char ts[32];
        std::snprintf(ts, sizeof(ts), "Updated %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (lvgl_port_lock(0)) {
            ctx.ui->update_measurements(meas);
            ctx.ui->set_status(ts);
            lvgl_port_unlock();
        }
    }
}

} // anonymous namespace

/* ── Entry point ─────────────────────────────────────────────────────── */

extern "C" void app_main()
{
    g_sensor_evt = xEventGroupCreate();

    // 1. Display + LVGL
    ESP_ERROR_CHECK(display::init());

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

    // 3. Sensor + UI (static lifetime — these outlive app_main)
    static auto sensor = Sen55(sen55_bus);
    static Ui* ui_ptr = nullptr;

    if (lvgl_port_lock(0)) {
        static auto ui = Ui();
        ui_ptr = &ui;
        ui.on_refresh([] {
            xEventGroupSetBits(g_sensor_evt, kEvtSensorRead);
        });
        lvgl_port_unlock();
    }

    // 4. Periodic timer
    const esp_timer_create_args_t timer_args = {
        .callback = periodic_timer_cb,
        .name = "sensor_timer",
    };
    esp_timer_handle_t timer{};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer,
        static_cast<uint64_t>(kSampleIntervalMs) * 1000));

    // 5. Sensor task
    static SensorTaskArgs task_args = {&sensor, ui_ptr};
    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096,
                            &task_args, 5, nullptr, tskNO_AFFINITY);

    ESP_LOGI(TAG, "Air quality monitor running");
}
