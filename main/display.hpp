#pragma once

#include "esp_err.h"

#include <cstdint>

namespace display {

constexpr uint32_t kHRes = 800;
constexpr uint32_t kVRes = 480;

/**
 * Initialise the RGB LCD panel, GT911 touch, and LVGL port.
 * After this returns the LVGL task is running and the display is ready.
 */
[[nodiscard]] esp_err_t Init();

} // namespace display
