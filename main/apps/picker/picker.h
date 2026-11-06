#pragma once

#include <cstddef>
#include <cstdint>

// Finger Picker rules, independent of the UI: everyone holds a finger on the screen, a countdown
// runs while at least two are down, and then one finger is picked at random. Times are in ms from
// any monotonic clock (the UI passes lv_tick_get()).
namespace picker {

constexpr size_t kMaxFingers = 5;  // the touch controller tracks up to five
constexpr size_t kMinFingers = 2;
constexpr uint8_t kColorCount = 8;  // player palette slots to choose from (theme::kPlayerColorCount)
constexpr uint32_t kCountdownMs = 5000;
constexpr uint32_t kSettleMs = 650;     // between choosing the winner and showing it
constexpr uint32_t kLiftGraceMs = 120;  // a finger missing for less than this hasn't lifted

struct TouchPoint {
    uint8_t id;  // the touch controller's track id, stable while the finger stays down
    int16_t x;
    int16_t y;
};

struct Finger {
    uint8_t id;
    int16_t x;
    int16_t y;
    uint8_t color;  // player palette slot, unique among the fingers in this round
    bool down;
    uint32_t last_seen;
};

enum class Phase : uint8_t {
    kWaiting,   // fewer than kMinFingers
    kCounting,  // counting down; starts over whenever a finger joins or leaves
    kSettling,  // winner chosen, about to be shown; fingers can no longer join or leave
    kPicked,    // winner shown until every finger has lifted and a new touch starts over
};

class Round {
public:
    // Feeds the fingers on the screen now.
    void update(const TouchPoint *points, size_t count, uint32_t now);
    void reset();

    Phase phase() const { return phase_; }
    uint32_t phase_started() const { return phase_start_; }
    size_t count() const { return count_; }
    const Finger &finger(size_t index) const { return fingers_[index]; }
    const Finger *find(uint8_t id) const;

    // Countdown left in kCounting, 0 otherwise.
    uint32_t remaining_ms(uint32_t now) const;

    // The chosen finger's id; valid in kSettling and kPicked.
    uint8_t winner_id() const { return winner_id_; }

    // In kPicked: every finger has lifted, so the next touch starts a new round.
    bool ready_for_next() const { return phase_ == Phase::kPicked && armed_; }

private:
    void enter(Phase phase, uint32_t now);
    int index_of(uint8_t id) const;  // -1 if no finger has that id
    bool add(const TouchPoint &point, uint32_t now);
    uint8_t free_color() const;

    Finger fingers_[kMaxFingers] = {};
    size_t count_ = 0;
    Phase phase_ = Phase::kWaiting;
    uint32_t phase_start_ = 0;
    uint8_t winner_id_ = 0;
    bool armed_ = false;
};

}  // namespace picker
