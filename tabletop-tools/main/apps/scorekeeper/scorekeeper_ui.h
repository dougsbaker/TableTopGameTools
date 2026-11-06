#pragma once

#include "lvgl.h"

#include "apps/scorekeeper/scorekeeper.h"

namespace scorekeeper {

// Builds the Score Keeper screen (header + player rows) into `parent`.
// The caller must hold the LVGL lock. `model` must outlive the UI.
void ui_create(lv_obj_t *parent, ScoreKeeper &model);

}  // namespace scorekeeper
