#pragma once

#include <cstdint>

// Speaker output through the board's ES8311 codec (Waveshare BSP). Tones are generated on a
// low-priority background task, so callers never block on audio.
namespace sound {

// Starts the speaker and the playback task. Returns false if audio isn't available, in which
// case the other calls do nothing.
bool init();

// 0-100. Applied before the next sound plays.
void set_volume(uint8_t percent);

// While enabled, the amplifier stays on so tap clicks play instantly (it otherwise powers up
// only for alarms and test beeps, which avoids idle hiss).
void set_clicks_enabled(bool enabled);

// Plays the timer alarm: a few rounds of three short beeps, about 5 s in all. Restarts it if
// it's already playing.
void play_alarm();

// One short beep, e.g. to try a volume setting.
void play_test();

// A soft tick, e.g. each second of a countdown.
void play_tick();

// A short rising chime, e.g. when the Finger Picker picks.
void play_chime();

// A very short tick for touch feedback. Ignored while the alarm or a beep is playing, and
// when clicks are disabled.
void play_click();

// Stops the alarm if it's playing.
void stop();

}  // namespace sound
