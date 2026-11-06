#pragma once

#include <cstddef>

#include "apps/picker/picker.h"

namespace picker {

// Every finger on the screen as of LVGL's latest touch read (LVGL itself follows only the first).
// Fills up to `max` points in screen coordinates and returns how many. Call on the LVGL task.
size_t read_touch_points(TouchPoint *out, size_t max);

// False if the touch controller couldn't be reached directly, in which case only the first
// finger is seen. Logs which it is the first time.
bool multitouch_available();

}  // namespace picker
