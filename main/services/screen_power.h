#pragma once

#include <cstdint>

// Backlight brightness and inactivity dimming. After `dim_after_ms` without a touch the
// backlight drops low, and a minute later it turns off; a touch brings it back. A touch on a
// dark screen only wakes it and never reaches the button underneath. Uses LVGL: call with the
// LVGL lock held.
namespace screen_power {

void init(uint8_t brightness_percent, uint32_t dim_after_ms);
void set_brightness(uint8_t percent);
void set_dim_after(uint32_t ms);  // 0 = never dim

// While held (e.g. a timer is counting down), the screen may dim but never turns off.
void hold_awake(bool hold);

// Full brightness now, e.g. when an alarm goes off.
void wake();

// Backlight off now, as if the screen had timed out, and it stays off (even while held awake)
// until a touch or wake().
void sleep();

bool is_off();

}  // namespace screen_power
