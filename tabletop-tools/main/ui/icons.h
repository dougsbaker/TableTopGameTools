#pragma once

#include "lvgl.h"

// Simple icons drawn from rectangles (the built-in font has no matching glyphs). No part of an
// icon is clickable, so taps reach the tile or button underneath.
namespace ui {

// 3x3 grid ("waffle") icon used for returning to the home screen.
lv_obj_t *icon_waffle(lv_obj_t *parent, lv_color_t color);

// Three score rows in player colors.
lv_obj_t *icon_scorekeeper(lv_obj_t *parent);

// Display bar over a 3x3 keypad.
lv_obj_t *icon_calculator(lv_obj_t *parent, lv_color_t color);

// Ring of square ticks, most of them lit in `color`, like the timer's dial.
lv_obj_t *icon_timer(lv_obj_t *parent, lv_color_t color);

// Square die showing five pips.
lv_obj_t *icon_dice(lv_obj_t *parent, lv_color_t color);

// Three round finger spots, one lit (the Finger Picker; round on purpose, like its screen).
lv_obj_t *icon_picker(lv_obj_t *parent, lv_color_t color);

}  // namespace ui
