#pragma once

#include "lvgl.h"

#include "apps/scorekeeper/scorekeeper.h"

namespace scorekeeper {

struct GameMenuActions {
    void (*on_options_changed)();  // step size or add-up delay changed in the model
    void (*on_new_game)();         // the user confirmed NEW GAME; the model isn't changed yet
};

// The Score Keeper's drop-down options panel (step size, add-up delay, new game). It slides
// down from under the header of `screen`; tapping outside it, CLOSE, or the gear again closes it.
// The caller must hold the LVGL lock.
void game_menu_toggle(lv_obj_t *screen, ScoreKeeper &model, const GameMenuActions &actions);
void game_menu_close();

}  // namespace scorekeeper
