#include "esp32_8048s043.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"

#include <cstdint>

// ESP-IDF config structs have many fields we intentionally leave defaulted.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace esp32_8048s043 {
namespace {

const char* TAG = "esp32_8048s043";

/* ── Pin assignments (ESP32-8048S043 board) ──────────────────────────── */

constexpr auto kPinBacklight = GPIO_NUM_2;

// RGB LCD control
constexpr auto kPinHsync = GPIO_NUM_39;
constexpr auto kPinVsync = GPIO_NUM_41;
constexpr auto kPinDE = GPIO_NUM_40;
constexpr auto kPinPclk = GPIO_NUM_42;

// RGB LCD 16-bit data bus (B5, G6, R5)
constexpr int kDataPins[] = {
    GPIO_NUM_8,   // B3
    GPIO_NUM_3,   // B4
    GPIO_NUM_46,  // B5
    GPIO_NUM_9,   // B6
    GPIO_NUM_1,   // B7
    GPIO_NUM_5,   // G2
    GPIO_NUM_6,   // G3
    GPIO_NUM_7,   // G4
    GPIO_NUM_15,  // G5
    GPIO_NUM_16,  // G6
    GPIO_NUM_4,   // G7
    GPIO_NUM_45,  // R3
    GPIO_NUM_48,  // R4
    GPIO_NUM_47,  // R5
    GPIO_NUM_21,  // R6
    GPIO_NUM_14,  // R7
};

constexpr uint32_t kPixelClkHz = 18'000'000;

/* ── Initialisation helpers ──────────────────────────────────────────── */

esp_err_t InitLcd(esp_lcd_panel_handle_t& out_panel)
{
    const esp_lcd_rgb_panel_config_t panel_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz            = kPixelClkHz,
            .h_res              = kHRes,
            .v_res              = kVRes,
            .hsync_pulse_width  = 4,
            .hsync_back_porch   = 8,
            .hsync_front_porch  = 8,
            .vsync_pulse_width  = 4,
            .vsync_back_porch   = 8,
            .vsync_front_porch  = 8,
            .flags = {.pclk_active_neg = true},
        },
        .data_width = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 0,
        .psram_trans_align = 64,
        .hsync_gpio_num = kPinHsync,
        .vsync_gpio_num = kPinVsync,
        .de_gpio_num = kPinDE,
        .pclk_gpio_num = kPinPclk,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            kDataPins[0],  kDataPins[1],  kDataPins[2],  kDataPins[3],
            kDataPins[4],  kDataPins[5],  kDataPins[6],  kDataPins[7],
            kDataPins[8],  kDataPins[9],  kDataPins[10], kDataPins[11],
            kDataPins[12], kDataPins[13], kDataPins[14], kDataPins[15],
        },
        .flags = {.fb_in_psram = true},
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_cfg, &out_panel),
                        TAG, "RGB panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(out_panel), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(out_panel), TAG, "Panel init failed");

    ESP_LOGI(TAG, "RGB LCD panel initialised (%lux%lu)", kHRes, kVRes);
    return ESP_OK;
}

void BacklightOn()
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << kPinBacklight,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(kPinBacklight, 1);
    ESP_LOGI(TAG, "Backlight on");
}

} // anonymous namespace

esp_err_t Init()
{
    esp_lcd_panel_handle_t lcd_panel{};
    ESP_RETURN_ON_ERROR(InitLcd(lcd_panel), TAG, "LCD init failed");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = lcd_panel,
        .buffer_size = kHRes * 10,
        .double_buffer = false,
        .hres = kHRes,
        .vres = kVRes,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
            .swap_bytes = false,
            .direct_mode = false,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = false,
            .avoid_tearing = false,
        },
    };
    auto* disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL port");
        return ESP_FAIL;
    }

    BacklightOn();
    ESP_LOGI(TAG, "Display fully initialised");
    return ESP_OK;
}

} // namespace esp32_8048s043
