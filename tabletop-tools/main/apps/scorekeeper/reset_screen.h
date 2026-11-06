#pragma once

#include "lvgl.h"

#include "apps/scorekeeper/scorekeeper.h"

namespace scorekeeper {

using ResetDoneCb = void (*)();

// Opens a full-screen "reset all scores" view over `parent`. On the left is a value stepper
// with cancel / confirm, and on the right a live preview of every player at that value.
// Confirming sets all scores to the value and keeps the players and colors, then calls
// `on_done`; cancel changes nothing. The caller must hold the LVGL lock.
void reset_screen_open(lv_obj_t *parent, ScoreKeeper &model, ResetDoneCb on_done);

}  // namespace scorekeeper
