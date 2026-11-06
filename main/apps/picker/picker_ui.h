#pragma once

#include "lvgl.h"

#include "apps/picker/picker.h"

namespace picker {

// Builds the Finger Picker screen into `screen`. Everyone holds a finger on the screen and each
// lights up in its own color; a circle counts down from 5 while streaks of light bounce between
// the fingers, then one finger is picked and stays lit. `round` must outlive the UI.
void ui_create(lv_obj_t *screen, Round &round);

}  // namespace picker
