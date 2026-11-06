#include "apps/timer/countdown.h"

#include <cstdio>

namespace timer {
namespace {

constexpr int64_t kUsPerSecond = 1000000;

}  // namespace

void Countdown::set_duration(uint32_t seconds)
{
    if (state_ == State::kIdle) {
        duration_s_ = seconds > kMaxSeconds ? kMaxSeconds : seconds;
    }
}

void Countdown::start(int64_t now_us)
{
    if ((state_ == State::kIdle || state_ == State::kFinished) && duration_s_ > 0) {
        deadline_us_ = now_us + duration_s_ * kUsPerSecond;
        state_ = State::kRunning;
    }
}

void Countdown::pause(int64_t now_us)
{
    if (state_ == State::kRunning) {
        paused_left_us_ = remaining_us(now_us);
        state_ = State::kPaused;
    }
}

void Countdown::resume(int64_t now_us)
{
    if (state_ == State::kPaused) {
        deadline_us_ = now_us + paused_left_us_;
        state_ = State::kRunning;
    }
}

void Countdown::reset()
{
    state_ = State::kIdle;
}

bool Countdown::update(int64_t now_us)
{
    if (state_ == State::kRunning && now_us >= deadline_us_) {
        state_ = State::kFinished;
        return true;
    }
    return false;
}

int64_t Countdown::remaining_us(int64_t now_us) const
{
    switch (state_) {
    case State::kIdle:
        return duration_s_ * kUsPerSecond;
    case State::kRunning:
        return deadline_us_ > now_us ? deadline_us_ - now_us : 0;
    case State::kPaused:
        return paused_left_us_;
    case State::kFinished:
        break;
    }
    return 0;
}

uint32_t seconds_left(int64_t remaining_us)
{
    return static_cast<uint32_t>((remaining_us + kUsPerSecond - 1) / kUsPerSecond);
}

void format_time(char *out, unsigned size, uint32_t seconds)
{
    if (seconds > 60) {
        std::snprintf(out, size, "%u:%02u", static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
    } else {
        std::snprintf(out, size, "%us", static_cast<unsigned>(seconds));
    }
}

}  // namespace timer
