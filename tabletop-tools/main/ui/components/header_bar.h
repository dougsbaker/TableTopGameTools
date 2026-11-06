#pragma once

#include <cstdint>

#include "lvgl.h"

// Full-width top bar made of rectangular cells separated by dark dividers.
namespace ui {

lv_obj_t *header_bar_create(lv_obj_t *parent);

// Adds a cell to `bar`. `width` <= 0 shares the remaining width with other growing cells.
// `text` may be null (e.g. for an icon cell). Without `on_click` the cell is an inert,
// muted placeholder.
lv_obj_t *header_button_create(lv_obj_t *bar, int32_t width, const char *text,
                               lv_event_cb_t on_click, void *user_data);

void header_button_set_text(lv_obj_t *button, const char *text);

// Disabled buttons ignore touches and show muted text.
void header_button_set_enabled(lv_obj_t *button, bool enabled);

}  // namespace ui
