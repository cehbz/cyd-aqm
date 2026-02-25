#pragma once

#include "esp_err.h"

#include <cstdint>

namespace esp32_8048s043 {

constexpr uint32_t kHRes = 800;
constexpr uint32_t kVRes = 480;

/**
 * Initialise the RGB LCD panel and LVGL port.
 * After this returns the LVGL task is running and the display is ready.
 */
[[nodiscard]] esp_err_t Init();

} // namespace esp32_8048s043
