#pragma once

#include "apps/scorekeeper/scorekeeper.h"

// Saves the Score Keeper session (players, scores, sort mode, step size, add-up delay, last
// reset value) to NVS so it survives power loss. Uses the shared background writer in
// services/storage, which must be started for saves to happen.
namespace scorekeeper {

// Loads the saved session into `model`. Returns false, leaving `model` unchanged, when nothing
// is saved yet or the saved data is unreadable, from an unknown schema version, or invalid.
bool storage_load(ScoreKeeper &model);

// Queues a snapshot of `model` for saving, unless it's identical to what was last saved.
// Call from the thread that owns `model` (the LVGL task). Never blocks on flash.
void storage_save(const ScoreKeeper &model);

}  // namespace scorekeeper
