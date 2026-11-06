#pragma once

#include "lvgl.h"

#include "apps/calculator/calculator.h"

namespace calculator {

// Builds the Calculator screen into `screen`: a scrolling tape of everything added so far,
// the entry and running total, and a keypad. `model` must outlive the UI.
void ui_create(lv_obj_t *screen, Tally &model);

}  // namespace calculator
