#pragma once

#include <cstdint>

#include "lvgl.h"

namespace timer {

using EditDoneCb = void (*)(uint32_t seconds);

// Opens the full-screen SET TIME editor over `parent`, starting at `seconds`: independent
// +/- for minutes and seconds, with preset times below. DONE passes the new time to `on_done`;
// CANCEL changes nothing. The caller must hold the LVGL lock.
void time_editor_open(lv_obj_t *parent, uint32_t seconds, EditDoneCb on_done);

}  // namespace timer
