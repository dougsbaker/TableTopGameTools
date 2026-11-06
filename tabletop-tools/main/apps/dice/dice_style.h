#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"
#include "ui/theme.h"

namespace dice {

// The ROLL button, and the Dice tile on the home screen.
constexpr uint32_t kDiceColor = 0x7C3AED;

// Each die type gets its own color from the player palette: d4 red, d6 blue, d8 green,
// d10 amber, d12 cyan, d20 pink.
inline lv_color_t die_color(size_t type)
{
    constexpr uint8_t kPaletteSlots[] = {1, 0, 2, 3, 5, 6};
    return theme::player_color(kPaletteSlots[type % sizeof(kPaletteSlots)]);
}

}  // namespace dice
