#pragma once

#include <cstddef>

#include "lvgl.h"
#include "services/power.h"

namespace ui {

constexpr int kLowBatteryPercent = 15;

// Short indicator text with a battery symbol: "87%", charging "87%" with a bolt, "USB" when
// running from USB with no battery, or "" if the power chip isn't available.
void battery_indicator_text(const power::Status &status, char *out, size_t size);

// Low and running on battery (not charging).
bool battery_low(const power::Status &status);

// Watches the battery and shows a toast when it falls to 15% and again at 5%.
// Call once, with the LVGL lock held.
void battery_alerts_start();

}  // namespace ui
