#pragma once

#include "lvgl.h"

namespace ui {

// 96 px Montserrat for big readouts such as the timer digits. The built-in fonts stop at 48 px,
// so this one is rendered at runtime (Tiny TTF) from the embedded Montserrat-Medium.ttf and
// cached per glyph. Created on first use; call with the LVGL lock held. Falls back to the 48 px
// built-in font if it can't be created.
const lv_font_t *font_display();

}  // namespace ui
