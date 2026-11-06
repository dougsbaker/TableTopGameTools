#pragma once

#include <cstddef>
#include <cstdint>

// Dice roller model: how many of each die are chosen, and the last roll. No LVGL here.
namespace dice {

constexpr uint8_t kSides[] = {4, 6, 8, 10, 12, 20};
constexpr size_t kTypeCount = sizeof(kSides) / sizeof(kSides[0]);
constexpr uint8_t kMaxPerType = 20;

// A fair roll of one die with `sides` faces, from the hardware random number generator.
uint8_t roll_die(uint8_t sides);

class DiceSet {
public:
    // Change how many of die type `type` (index into kSides) will be rolled. They return false
    // when nothing changed (already at kMaxPerType or 0). Any change clears the last result.
    bool add(size_t type);
    bool remove(size_t type);
    void clear();

    uint8_t count(size_t type) const { return counts_[type]; }
    uint32_t dice_count() const;

    // Rolls every chosen die.
    void roll();
    bool has_result() const { return has_result_; }
    uint8_t result(size_t type, size_t index) const { return results_[type][index]; }
    uint32_t total() const;

private:
    uint8_t counts_[kTypeCount] = {};
    uint8_t results_[kTypeCount][kMaxPerType] = {};
    bool has_result_ = false;
};

}  // namespace dice
