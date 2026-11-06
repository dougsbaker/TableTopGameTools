#pragma once

#include <cstddef>
#include <cstdint>

// Adding-machine model for tallying, such as summing domino pips: a tape of signed terms plus
// the number being typed. No LVGL here, so the UI stays separate.
namespace calculator {

constexpr size_t kMaxTerms = 99;
constexpr uint8_t kMaxEntryDigits = 5;

class Tally {
public:
    // Appends digit `d` (0-9) to the entry. Returns false if the entry is already full.
    bool type_digit(uint8_t d);
    void backspace();

    // Adds the entry to the tape as +entry, or -entry when `subtract`, and empties the entry.
    // Returns false if nothing is typed or the tape is full.
    bool commit(bool subtract);

    // Empties the entry and the tape. undo() straight afterwards brings the tape back.
    void clear();

    // Removes the last term, or restores the tape erased by clear().
    void undo();
    bool can_undo() const { return count_ > 0 || cleared_count_ > 0; }

    int32_t total() const;
    size_t count() const { return count_; }
    int32_t term(size_t index) const { return terms_[index]; }
    bool has_entry() const { return entry_digits_ > 0; }
    int32_t entry() const { return entry_; }

private:
    int32_t terms_[kMaxTerms] = {};
    size_t count_ = 0;
    int32_t cleared_[kMaxTerms] = {};
    size_t cleared_count_ = 0;
    int32_t entry_ = 0;
    uint8_t entry_digits_ = 0;
};

}  // namespace calculator
