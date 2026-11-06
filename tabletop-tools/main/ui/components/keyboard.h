#pragma once

#include <cstddef>

#include "lvgl.h"

namespace ui {

using TextChangedCb = void (*)(void *user_data);

// Rectangular A-Z keyboard with SPACE, backspace and CLEAR. It grows to fill the remaining
// height of a flex-column `parent`. Edits the NUL-terminated `text` in place (at most
// `max_len` characters, no leading or double spaces) and calls `on_change` after each edit.
// `text` must outlive the keyboard.
lv_obj_t *keyboard_create(lv_obj_t *parent, char *text, size_t max_len, TextChangedCb on_change,
                          void *user_data);

}  // namespace ui
