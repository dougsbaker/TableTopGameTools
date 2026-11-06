#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ui {

using LevelChangedCb = void (*)(int32_t level, void *user_data);

// A row of `segments` square tiles lit up to the current level (0..segments), like the timer's
// dial. Tap or drag across it to set the level; it never goes below `min_level`. `on_change`
// runs whenever the level changes. Grows to fill the remaining width of a flex-row parent.
lv_obj_t *level_bar_create(lv_obj_t *parent, int32_t segments, int32_t min_level, int32_t level,
                           lv_color_t lit, LevelChangedCb on_change, void *user_data);

void level_bar_set(lv_obj_t *bar, int32_t level);

}  // namespace ui
