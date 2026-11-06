#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

// Switches between the home screen and the apps. Each screen is built the first time it's
// shown and then kept, so an app keeps its state (and any open views) while you're elsewhere.
namespace nav {

enum class Screen : uint8_t {
    kHome,
    kScoreKeeper,
    kCalculator,
    kTimer,
    kSettings,
    kDice,
    kPicker,
};
constexpr size_t kScreenCount = 7;

using BuildFn = void (*)(lv_obj_t *screen);

// Registers how to build screen `id`. Call before the first show().
void register_screen(Screen id, BuildFn build);

// Builds every registered screen now rather than on first show, so opening an app never
// stalls. Optional; call with the LVGL lock held.
void build_all();

// Shows screen `id`. Apps slide in from the right; going home slides back to the right.
// Call with the LVGL lock held (event callbacks already have it).
void show(Screen id);

inline void go_home()
{
    show(Screen::kHome);
}

}  // namespace nav
