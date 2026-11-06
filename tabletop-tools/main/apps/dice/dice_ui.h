#pragma once

#include "lvgl.h"

#include "apps/dice/dice.h"

namespace dice {

// Builds the Dice screen into `screen`: a round ROLL button ringed by d4-d20 tiles. Tap a tile
// to add one of that die, hold it to take one away; ROLL replaces the button's label with the
// total. `model` must outlive the UI.
void ui_create(lv_obj_t *screen, DiceSet &model);

}  // namespace dice
