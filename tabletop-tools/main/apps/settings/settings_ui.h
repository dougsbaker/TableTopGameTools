#pragma once

#include "lvgl.h"

namespace settings_app {

// Builds the Settings screen into `screen`: display (brightness, auto-dim), sound (volume and
// test beep, timer alarm, tap clicks), battery status, and system info with "reset all data".
void ui_create(lv_obj_t *screen);

}  // namespace settings_app
