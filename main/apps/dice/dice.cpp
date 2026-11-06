#include "apps/dice/dice.h"

#include "esp_random.h"

namespace dice {

uint8_t roll_die(uint8_t sides)
{
    // Reject the top sliver of the range so every face is exactly equally likely.
    const uint32_t limit = UINT32_MAX - UINT32_MAX % sides;
    uint32_t r;
    do {
        r = esp_random();
    } while (r >= limit);
    return static_cast<uint8_t>(r % sides + 1);
}

bool DiceSet::add(size_t type)
{
    if (counts_[type] >= kMaxPerType) {
        return false;
    }
    ++counts_[type];
    has_result_ = false;
    return true;
}

bool DiceSet::remove(size_t type)
{
    if (counts_[type] == 0) {
        return false;
    }
    --counts_[type];
    has_result_ = false;
    return true;
}

void DiceSet::clear()
{
    for (uint8_t &count : counts_) {
        count = 0;
    }
    has_result_ = false;
}

uint32_t DiceSet::dice_count() const
{
    uint32_t n = 0;
    for (uint8_t count : counts_) {
        n += count;
    }
    return n;
}

void DiceSet::roll()
{
    for (size_t type = 0; type < kTypeCount; ++type) {
        for (uint8_t i = 0; i < counts_[type]; ++i) {
            results_[type][i] = roll_die(kSides[type]);
        }
    }
    has_result_ = dice_count() > 0;
}

uint32_t DiceSet::total() const
{
    uint32_t sum = 0;
    for (size_t type = 0; type < kTypeCount; ++type) {
        for (uint8_t i = 0; i < counts_[type]; ++i) {
            sum += results_[type][i];
        }
    }
    return sum;
}

}  // namespace dice
