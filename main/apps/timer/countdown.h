#pragma once

#include <cstdint>

// Countdown timer model. Time comes from a monotonic microsecond clock passed in by the caller
// (esp_timer_get_time()), never from the UI frame rate. No LVGL here.
namespace timer {

constexpr uint32_t kMaxSeconds = 99 * 60 + 59;

enum class State : uint8_t {
    kIdle,      // showing the set duration; can be edited
    kRunning,
    kPaused,
    kFinished,  // reached zero; waiting for START or RESET
};

class Countdown {
public:
    State state() const { return state_; }
    uint32_t duration_s() const { return duration_s_; }

    // Sets the duration (clamped to kMaxSeconds). Only takes effect while idle.
    void set_duration(uint32_t seconds);

    // From idle or finished: counts down the full duration. Does nothing if it's 0.
    void start(int64_t now_us);
    void pause(int64_t now_us);
    void resume(int64_t now_us);
    // Back to idle with the same duration.
    void reset();

    // Call regularly. Returns true exactly once, when a running countdown reaches zero.
    bool update(int64_t now_us);

    int64_t remaining_us(int64_t now_us) const;

private:
    State state_ = State::kIdle;
    uint32_t duration_s_ = 60;
    int64_t deadline_us_ = 0;     // while running
    int64_t paused_left_us_ = 0;  // while paused
};

// Whole seconds still to go, rounded up, so the display reaches 0 exactly when time is up.
uint32_t seconds_left(int64_t remaining_us);

// "14:25", "9:30", then "60s" ... "1s", "0s": only the characters the time needs.
void format_time(char *out, unsigned size, uint32_t seconds);

}  // namespace timer
