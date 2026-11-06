#pragma once

#include <cstdint>

#include "lvgl.h"

// Shared design system: flat palette, metrics, motion timings and sharp-cornered building blocks.
namespace theme {

constexpr int32_t kScreenW = 480;
constexpr int32_t kScreenH = 480;
constexpr int32_t kHeaderH = 50;
constexpr int32_t kGap = 3;  // Background-colored gap between tiles.

constexpr uint32_t kBackground = 0x0B0E14;
constexpr uint32_t kSurface = 0x161B24;
constexpr uint32_t kSurfaceHigh = 0x232B37;
constexpr uint32_t kSurfacePressed = 0x323C4B;
constexpr uint32_t kText = 0xF5F7FA;
constexpr uint32_t kMutedText = 0x8A94A6;
constexpr uint32_t kAccent = 0x2563EB;
constexpr uint32_t kDanger = 0xDC2626;
constexpr uint32_t kDangerText = 0xF87171;

// Motion durations in ms.
constexpr uint32_t kPressMs = 80;
constexpr uint32_t kReleaseMs = 220;
constexpr uint32_t kMoveMs = 320;  // rows sliding and resizing
constexpr uint32_t kOverlayInMs = 260;
constexpr uint32_t kOverlayOutMs = 200;
constexpr uint32_t kScreenMs = 300;  // switching between home and apps

constexpr uint8_t kPlayerColorCount = 8;

// Flat player color for slot `index` (wraps past kPlayerColorCount). All take white text.
lv_color_t player_color(uint8_t index);

// Darker copy of `c`; `amount` is 0 (unchanged) to 255 (black).
lv_color_t shade(lv_color_t c, lv_opa_t amount);

// Sets up the shared transitions. Call once before building any screen.
void init();

// Gives `screen` the app background and the inherited default text color and font.
void style_screen(lv_obj_t *screen);

// Plain rectangular container with every theme style stripped: no radius, border, shadow,
// outline or padding, and not scrollable. All UI is built from these so nothing is rounded.
lv_obj_t *make_box(lv_obj_t *parent);

// Flat opaque background for the given style selector.
void set_bg(lv_obj_t *obj, lv_color_t color, lv_style_selector_t selector = 0);

// Flat background that fades to `pressed` while touched and back on release.
void make_pressable(lv_obj_t *obj, lv_color_t normal, lv_color_t pressed);

// Smooth transition for background color and border width, for use with custom states.
const lv_style_transition_dsc_t *state_transition();

}  // namespace theme
