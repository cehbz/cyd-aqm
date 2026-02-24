# CYD Air Quality Monitor

ESP-IDF firmware for an air quality monitor using an ESP32-8048S043 "CYD" and Sensirion SEN55 sensor.

## Hardware

| Component | Detail |
|-----------|--------|
| Board | ESP32-8048S043 (ESP32-S3, 16MB flash, 8MB octal PSRAM) |
| Display | 4.3" 800x480 RGB565, ST7262, 18 MHz pixel clock |
| Touch | GT911 capacitive, I2C @ 0x5D, GPIO 19 (SDA) / 20 (SCL) / 38 (RST) / 18 (INT) |
| Sensor | Sensirion SEN55, I2C @ 0x69, GPIO 11 (SDA) / 12 (SCL) via P2 header |
| Sensor power | 5V + GND from P1 header, SEL pin pulled to GND for I2C mode |
| I2C pull-ups | External 10 kΩ from SDA/SCL to 3.3V (SEN55 has no internal pull-ups) |

## Build

Requires ESP-IDF v5.4. The `.envrc` configures the Python venv via `layout python3` and sources ESP-IDF via direnv.

```sh
direnv allow
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

A wrapper at `~/.local/bin/idf.py` sources `export.sh` automatically, so `idf.py build` works directly in any shell (including Claude Code's Bash tool).

## Architecture

```
main/
  main.cpp        – app_main (extern "C"), sensor task, 5-min esp_timer
  display.hpp/cpp – display::init() — RGB LCD panel + LVGL port + GT911 touch
  ui.hpp/cpp      – Ui class: 8 measurement cards, refresh button, status bar
  sen55.hpp/cpp   – Sen55 class: I2C driver with CRC8 verification
```

### Sensor lifecycle

The SEN55 is started and stopped each measurement cycle to extend fan/sensor lifespan. Each cycle: start → warm-up (~30 s) → read → stop. This reduces cumulative fan runtime by ~90% compared to continuous operation.

### Display

Uses `esp_lvgl_port` to manage the LVGL task, display registration (double framebuffer + bounce buffer in PSRAM, direct mode, avoid-tearing), and touch input. The GT911 `process_coordinates` callback maps from the sensor's native coordinate space (0–477 × 0–269) to display resolution (800 × 480).

### Dependencies (managed via idf_component.yml)

- `lvgl/lvgl ^9`
- `espressif/esp_lvgl_port ^2.4`
- `espressif/esp_lcd_touch_gt911 ^1.2`

## Conventions

- C++ (gnu++2b), modern idioms: classes, constexpr, std::array, std::string_view, std::function, RAII
- ESP-IDF C APIs wrapped in C++ classes/namespaces; `app_main` is `extern "C"`
- All LVGL calls must be wrapped in `lvgl_port_lock(0)` / `lvgl_port_unlock()`
- SEN55 driver uses the new `i2c_master` API (not the legacy `driver/i2c.h`)
- Constants use `constexpr` with `kCamelCase` naming
- No exceptions; error handling via `esp_err_t` returns
- Headers use `.hpp` extension; sources use `.cpp`
