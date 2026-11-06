#pragma once

#include <cstddef>
#include <cstdint>

#include "services/touch_controller.h"

// Device-wide settings, saved to NVS and applied to the screen and speaker as they change.
namespace settings {

// Auto-dim choices: off, 1, 2, 5 and 10 minutes.
constexpr uint32_t kDimAfterMs[] = {0, 60000, 120000, 300000, 600000};
constexpr size_t kDimOptionCount = sizeof(kDimAfterMs) / sizeof(kDimAfterMs[0]);

struct Values {
    uint8_t brightness;  // 10-100 %
    uint8_t dim_index;   // into kDimAfterMs
    uint8_t volume;      // 0-100 %
    bool alarm_sound;    // timer alarm
    bool tap_clicks;     // tick on every touch
    touch_controller::Sensitivity touch;
};

// Loads saved settings (defaults if there are none) and applies them. Call once, with the
// LVGL lock held, after sound::init().
void load_and_apply();

const Values &get();

void set_brightness(uint8_t percent);
void set_dim_index(uint8_t index);
void set_volume(uint8_t percent);
void set_alarm_sound(bool on);
void set_tap_clicks(bool on);
// Returns false, leaving the setting as it was, if the touch controller didn't take the change.
bool set_touch_sensitivity(touch_controller::Sensitivity level);

}  // namespace settings
