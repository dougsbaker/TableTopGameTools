#include "apps/calculator/calculator.h"

#include <cstring>

namespace calculator {

bool Tally::type_digit(uint8_t d)
{
    if (d > 9) {
        return false;
    }
    if (entry_digits_ == 1 && entry_ == 0) {
        entry_ = d;  // No leading zeros.
        return true;
    }
    if (entry_digits_ >= kMaxEntryDigits) {
        return false;
    }
    entry_ = entry_ * 10 + d;
    ++entry_digits_;
    return true;
}

void Tally::backspace()
{
    if (entry_digits_ == 0) {
        return;
    }
    entry_ /= 10;
    --entry_digits_;
}

bool Tally::commit(bool subtract)
{
    if (!has_entry() || count_ >= kMaxTerms) {
        return false;
    }
    terms_[count_++] = subtract ? -entry_ : entry_;
    entry_ = 0;
    entry_digits_ = 0;
    cleared_count_ = 0;  // A new series has started, so a cleared one can't come back.
    return true;
}

void Tally::clear()
{
    if (count_ > 0) {
        std::memcpy(cleared_, terms_, count_ * sizeof(terms_[0]));
        cleared_count_ = count_;
    }
    count_ = 0;
    entry_ = 0;
    entry_digits_ = 0;
}

void Tally::undo()
{
    if (count_ > 0) {
        --count_;
    } else if (cleared_count_ > 0) {
        std::memcpy(terms_, cleared_, cleared_count_ * sizeof(terms_[0]));
        count_ = cleared_count_;
        cleared_count_ = 0;
    }
}

int32_t Tally::total() const
{
    // At most 99 terms of at most 99999, so this can't overflow.
    int32_t sum = 0;
    for (size_t i = 0; i < count_; ++i) {
        sum += terms_[i];
    }
    return sum;
}

}  // namespace calculator
