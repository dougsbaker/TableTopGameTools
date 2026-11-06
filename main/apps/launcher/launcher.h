#pragma once

#include "lvgl.h"

namespace launcher {

// Builds the "Tabletop Tools" home screen: a grid of app tiles. Tiles for apps that aren't
// built yet are dimmed and marked COMING SOON instead of opening an empty screen.
void ui_create(lv_obj_t *screen);

}  // namespace launcher
