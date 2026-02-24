#include "display.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include <cstdint>

namespace display {
namespace {

const char* TAG = "display";

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

// GT911 touch (I2C_NUM_0)
constexpr auto kPinTouchSda = GPIO_NUM_19;
constexpr auto kPinTouchScl = GPIO_NUM_20;
constexpr auto kPinTouchRst = GPIO_NUM_38;
constexpr auto kPinTouchInt = GPIO_NUM_18;

constexpr uint32_t kPixelClkHz = 18'000'000;
constexpr uint32_t kTouchI2cClkHz = 400'000;

/*
 * GT911 raw coordinate range on this board.
 * The touch firmware reports in its own coordinate space, not the panel
 * resolution.  Values determined empirically (limpens repo).
 */
constexpr uint16_t kTouchRawXMax = 477;
constexpr uint16_t kTouchRawYMax = 269;

/* ── Helpers ─────────────────────────────────────────────────────────── */

uint16_t map_range(uint16_t val, uint16_t in_max, uint16_t out_max)
{
    return static_cast<uint16_t>(
        static_cast<uint32_t>(val) * out_max / in_max);
}

void touch_process_coordinates(esp_lcd_touch_handle_t /*tp*/,
                               uint16_t* x, uint16_t* y,
                               uint16_t* /*strength*/,
                               uint8_t* /*point_num*/,
                               uint8_t /*max_point_num*/)
{
    *x = map_range(*x, kTouchRawXMax, kHRes);
    *y = map_range(*y, kTouchRawYMax, kVRes);
}

/* ── Initialisation helpers ──────────────────────────────────────────── */

esp_err_t init_touch(esp_lcd_touch_handle_t& out_touch)
{
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = kPinTouchSda,
        .scl_io_num = kPinTouchScl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = true},
    };
    i2c_master_bus_handle_t i2c_bus{};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &i2c_bus),
                        TAG, "I2C bus init failed");

    const esp_lcd_panel_io_i2c_config_t tp_io_cfg = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 16,
        .lcd_param_bits = 0,
        .flags = {.disable_control_phase = 1},
        .scl_speed_hz = kTouchI2cClkHz,
    };
    esp_lcd_panel_io_handle_t tp_io{};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io),
                        TAG, "Touch panel IO init failed");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = static_cast<uint16_t>(kHRes),
        .y_max = static_cast<uint16_t>(kVRes),
        .rst_gpio_num = kPinTouchRst,
        .int_gpio_num = kPinTouchInt,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
        .process_coordinates = touch_process_coordinates,
        .interrupt_callback = nullptr,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &out_touch),
                        TAG, "GT911 init failed");

    ESP_LOGI(TAG, "GT911 touch initialised");
    return ESP_OK;
}

esp_err_t init_lcd(esp_lcd_panel_handle_t& out_panel)
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
        .num_fbs = 2,
        .bounce_buffer_size_px = kHRes * 10,
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

void backlight_on()
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

esp_err_t init()
{
    esp_lcd_panel_handle_t lcd_panel{};
    ESP_RETURN_ON_ERROR(init_lcd(lcd_panel), TAG, "LCD init failed");

    esp_lcd_touch_handle_t touch{};
    ESP_RETURN_ON_ERROR(init_touch(touch), TAG, "Touch init failed");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = lcd_panel,
        .buffer_size = kHRes * 100,
        .double_buffer = false,
        .hres = kHRes,
        .vres = kVRes,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
            .swap_bytes = false,
            .direct_mode = true,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        },
    };
    auto* disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL port");
        return ESP_FAIL;
    }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    if (!lvgl_port_add_touch(&touch_cfg)) {
        ESP_LOGE(TAG, "Failed to add touch input to LVGL port");
        return ESP_FAIL;
    }

    backlight_on();
    ESP_LOGI(TAG, "Display fully initialised");
    return ESP_OK;
}

} // namespace display
