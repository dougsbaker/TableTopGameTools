#pragma once

#include <cstddef>

#include "lvgl.h"

namespace ui {

using ChoiceChangedCb = void (*)(size_t index, void *user_data);

// Segmented control: equal-width rectangular options, the selected one filled with `accent`.
// `on_change` runs when a different option is tapped. Grows to fill the remaining width of a
// flex-row parent. `labels` must outlive the row.
lv_obj_t *choice_row_create(lv_obj_t *parent, const char *const *labels, size_t count, size_t selected,
                            lv_color_t accent, ChoiceChangedCb on_change, void *user_data);

void choice_row_set(lv_obj_t *row, size_t selected);

}  // namespace ui
