#pragma once

#include "lvgl.h"

#include "apps/timer/countdown.h"

namespace timer {

// Builds the Timer screen into `screen`: a 60-tick dial around the remaining time, which you
// drag to set up to 5 minutes, plus START/PAUSE/RESUME and RESET. `model` must outlive the UI.
// The countdown keeps running, and the alarm still sounds, while other screens are showing.
void ui_create(lv_obj_t *screen, Countdown &model);

}  // namespace timer
