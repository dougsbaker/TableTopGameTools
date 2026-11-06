#pragma once

#include "lvgl.h"

namespace ui {

// A banner that slides down over the top of whatever screen is showing, stays for a few
// seconds, then slides away. Text is copied. Call with the LVGL lock held.
void toast_show(const char *text, lv_color_t color);

}  // namespace ui
