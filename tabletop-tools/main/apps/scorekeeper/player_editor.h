#pragma once

#include <cstdint>

#include "lvgl.h"

#include "apps/scorekeeper/scorekeeper.h"

namespace scorekeeper {

using EditorDoneCb = void (*)();

// Opens a full-screen name + color editor over `parent`. `player_id` 0 creates a new player.
// `on_done` runs after a player is saved or removed; CANCEL changes nothing.
// The caller must hold the LVGL lock. Only one editor can be open at a time.
void player_editor_open(lv_obj_t *parent, ScoreKeeper &model, uint8_t player_id, EditorDoneCb on_done);

}  // namespace scorekeeper
