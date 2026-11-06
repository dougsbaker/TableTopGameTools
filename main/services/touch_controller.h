#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_lcd_touch.h"

// Direct access to the GT911 touch controller behind LVGL's touch input.
namespace touch_controller {

// How little a finger has to change the panel's signal to count as a touch. Raising it helps
// when the board isn't grounded (on battery, lying on a table), where a finger reads weaker.
enum class Sensitivity : uint8_t { kNormal, kHigh, kMax };
constexpr size_t kSensitivityCount = 3;

// The controller, or nullptr if it couldn't be found. Logs which it is the first time.
esp_lcd_touch_handle_t handle();

// Sets the controller's press and release thresholds for `level`; kNormal is its factory
// setting. The controller's configuration is written only when the thresholds differ, and is
// never written if it doesn't read back intact. Returns false if the change didn't take.
// Call with the LVGL lock held, so LVGL isn't reading touches meanwhile.
bool set_sensitivity(Sensitivity level);

}  // namespace touch_controller
