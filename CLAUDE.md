# CYD Air Quality Monitor

ESP-IDF firmware for an air quality monitor using an ESP32-8048S043 "CYD" and Sensirion SEN55 sensor.

## Hardware

| Component | Detail |
|-----------|--------|
| Board | ESP32-8048S043 (ESP32-S3, 16MB flash, 8MB octal PSRAM) |
| Display | 4.3" 800x480 RGB565, ST7262, 18 MHz pixel clock |
| Sensor | Sensirion SEN55, I2C @ 0x69, GPIO 11 (SDA) / 12 (SCL) via P2 header |
| Sensor power | 5V from P1 5V pin, GND from P1 GND, SEL pin pulled to GND for I2C mode |
| USB | USB-A to USB-C cable required (board's CC resistors don't support C-to-C) |

### Header pinout

| Header | Pins |
|--------|------|
| P1 | GND, RXD (GPIO 44), TXD (GPIO 43), 5V |
| P2 | GPIO 19, 11, 12, 13 |
| P4 | GPIO 18, 17, 3.3V, GND |

### I2C notes

- SEN55 has no internal pull-ups; ESP32-S3 internal pull-ups (~45 kΩ) are enabled and work at 10 kHz
- Sensirion I2C protocol requires a 20 ms delay between command write and data read (no repeated-start)
- SEN55 I2C is 3.3V compatible despite 5V VDD

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
  main.cpp                – app_main (extern "C"), constructs Sen55 + Ui
  esp32_8048s043.hpp/cpp  – Board-specific RGB LCD + LVGL port init
  ui.hpp/cpp              – Ui class: 8 measurement cards, status line
  sen55.hpp/cpp           – Sen55 class: self-managing sensor (pimpl, owns polling task)
```

### Sensor

Sen55 is a self-managing domain object. Construction starts measurement and spawns a polling task; destruction stops it. The only interface is a callback that fires with each new `Measurement`. All I2C protocol details are hidden behind pimpl. The header has no ESP-IDF dependencies.

### Display (esp32_8048s043)

Board-specific init for the ESP32-8048S043's 4.3" ST7262 RGB panel. Uses `esp_lvgl_port` with a simple single-framebuffer configuration (no bounce buffer, no direct mode, no avoid-tearing).

### LVGL fonts

Built-in Montserrat fonts lack µ (U+00B5) and ³ (U+00B3). Unit strings use ASCII fallbacks: `ug/m3` instead of `µg/m³`. The degree symbol (U+00B0) is included and works.

### Dependencies (managed via idf_component.yml)

- `lvgl/lvgl ^9`
- `espressif/esp_lvgl_port ^2.4`

## Conventions

- Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these project-specific exceptions:
  - File extensions: `.hpp` / `.cpp` (not `.h` / `.cc`)
- C++ (gnu++2b), modern idioms: classes, constexpr, std::array, std::string_view, std::function, RAII, pimpl
- ESP-IDF C APIs wrapped in C++ classes/namespaces; `app_main` is `extern "C"`
- All LVGL calls must be wrapped in `lvgl_port_lock(0)` / `lvgl_port_unlock()`
- No exceptions; error handling via `esp_err_t` returns
