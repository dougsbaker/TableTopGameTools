#pragma once

namespace ui {

// The board's two side buttons:
// - PWR turns the screen off, and on again. Held for 1.5 s, it puts up a power-off screen and
//   powers the board off when let go: `before_power_off` runs first, to queue any last saves,
//   and then everything queued is written. A press of PWR turns the board back on. Held on to
//   6 s, the power chip cuts the power by itself, saving nothing.
// - BOOT goes to the home screen from any app, cancelling an open confirmation. On a dark screen
//   it only wakes it, like a touch.
// Call once, with the LVGL lock held.
void side_buttons_start(void (*before_power_off)());

}  // namespace ui
