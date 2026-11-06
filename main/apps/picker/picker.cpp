#include "apps/picker/picker.h"

#include "esp_random.h"

namespace picker {
namespace {

// Uniform in [0, n), without modulo bias.
uint32_t random_below(uint32_t n)
{
    const uint32_t limit = UINT32_MAX - UINT32_MAX % n;
    uint32_t value;
    do {
        value = esp_random();
    } while (value >= limit);
    return value % n;
}

}  // namespace

void Round::reset()
{
    count_ = 0;
    phase_ = Phase::kWaiting;
    winner_id_ = 0;
    armed_ = false;
}

int Round::index_of(uint8_t id) const
{
    for (size_t i = 0; i < count_; ++i) {
        if (fingers_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Finger *Round::find(uint8_t id) const
{
    int index = index_of(id);
    return index < 0 ? nullptr : &fingers_[index];
}

uint32_t Round::remaining_ms(uint32_t now) const
{
    if (phase_ != Phase::kCounting) {
        return 0;
    }
    uint32_t elapsed = now - phase_start_;
    return elapsed >= kCountdownMs ? 0 : kCountdownMs - elapsed;
}

void Round::enter(Phase phase, uint32_t now)
{
    phase_ = phase;
    phase_start_ = now;
}

// A random color no other finger has, so every finger in a round looks different.
uint8_t Round::free_color() const
{
    uint8_t unused[kColorCount];
    uint32_t n = 0;
    for (uint8_t color = 0; color < kColorCount; ++color) {
        bool taken = false;
        for (size_t i = 0; i < count_; ++i) {
            taken = taken || fingers_[i].color == color;
        }
        if (!taken) {
            unused[n++] = color;
        }
    }
    return n == 0 ? 0 : unused[random_below(n)];
}

bool Round::add(const TouchPoint &point, uint32_t now)
{
    if (count_ >= kMaxFingers) {
        return false;
    }
    fingers_[count_] = {point.id, point.x, point.y, free_color(), true, now};
    ++count_;
    return true;
}

void Round::update(const TouchPoint *points, size_t count, uint32_t now)
{
    if (ready_for_next() && count > 0) {
        reset();
    }
    bool locked = phase_ == Phase::kSettling || phase_ == Phase::kPicked;
    bool changed = false;

    for (size_t p = 0; p < count; ++p) {
        int index = index_of(points[p].id);
        if (index >= 0) {
            Finger &finger = fingers_[index];
            finger.x = points[p].x;
            finger.y = points[p].y;
            finger.down = true;
            finger.last_seen = now;
        } else if (!locked) {
            changed = add(points[p], now) || changed;
        }
    }

    // Fingers that have been gone a moment have lifted. Once the winner is chosen they stay in
    // the round, so the winner stays lit after everyone lets go.
    bool any_down = false;
    for (size_t i = 0; i < count_;) {
        Finger &finger = fingers_[i];
        if (now - finger.last_seen <= kLiftGraceMs) {
            any_down = true;
            ++i;
        } else if (locked) {
            finger.down = false;
            ++i;
        } else {
            fingers_[i] = fingers_[count_ - 1];
            --count_;
            changed = true;
        }
    }

    switch (phase_) {
    case Phase::kWaiting:
        if (count_ >= kMinFingers) {
            enter(Phase::kCounting, now);
        }
        break;
    case Phase::kCounting:
        if (count_ < kMinFingers) {
            enter(Phase::kWaiting, now);
        } else if (changed) {
            phase_start_ = now;  // someone joined or let go: count down again
        } else if (now - phase_start_ >= kCountdownMs) {
            winner_id_ = fingers_[random_below(count_)].id;
            enter(Phase::kSettling, now);
        }
        break;
    case Phase::kSettling:
        if (now - phase_start_ >= kSettleMs) {
            enter(Phase::kPicked, now);
        }
        break;
    case Phase::kPicked:
        armed_ = armed_ || !any_down;
        break;
    }
}

}  // namespace picker
