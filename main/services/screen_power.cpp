#include "services/screen_power.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "ui/theme.h"

namespace screen_power {
namespace {

enum class Level : uint8_t { kAwake, kDimmed, kOff };

constexpr uint8_t kMinBrightness = 5;
constexpr uint8_t kDimmedBrightness = 10;
constexpr uint32_t kOffAfterDimMs = 60000;
constexpr uint32_t kPollMs = 250;

uint8_t s_brightness = 100;
uint32_t s_dim_after = 0;
bool s_hold = false;
bool s_asleep = false;  // turned off by sleep(); the inactivity timer leaves it alone
Level s_level = Level::kAwake;
lv_obj_t *s_blocker = nullptr;  // swallows the touch that wakes a dark screen
bool s_blocker_pressed = false;

void apply()
{
    int percent = s_brightness;
    if (s_level == Level::kDimmed) {
        percent = s_brightness < kDimmedBrightness ? s_brightness : kDimmedBrightness;
    } else if (s_level == Level::kOff) {
        percent = 0;
    }
    bsp_display_brightness_set(percent);
}

void set_level(Level level);

void on_blocker(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_blocker_pressed = true;
        set_level(Level::kAwake);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // Deleted asynchronously because this runs inside the blocker's own event.
        lv_obj_delete_async(s_blocker);
        s_blocker = nullptr;
        s_blocker_pressed = false;
    }
}

void set_level(Level level)
{
    if (level != Level::kOff) {
        s_asleep = false;
    }
    if (level == s_level) {
        return;
    }
    if (level == Level::kOff && s_blocker == nullptr) {
        // An invisible, clickable layer above everything, so the waking touch hits nothing else.
        s_blocker = theme::make_box(lv_layer_sys());
        lv_obj_set_size(s_blocker, theme::kScreenW, theme::kScreenH);
        lv_obj_add_event_cb(s_blocker, on_blocker, LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(s_blocker, on_blocker, LV_EVENT_RELEASED, nullptr);
        lv_obj_add_event_cb(s_blocker, on_blocker, LV_EVENT_PRESS_LOST, nullptr);
    } else if (level != Level::kOff && s_blocker != nullptr && !s_blocker_pressed) {
        lv_obj_delete(s_blocker);  // woken without a touch, e.g. by an alarm
        s_blocker = nullptr;
    }
    s_level = level;
    apply();
}

void on_poll(lv_timer_t *)
{
    if (s_asleep) {
        return;
    }
    uint32_t idle = lv_display_get_inactive_time(nullptr);
    if (s_dim_after == 0 || idle < s_dim_after) {
        set_level(Level::kAwake);
    } else if (!s_hold && idle >= s_dim_after + kOffAfterDimMs) {
        set_level(Level::kOff);
    } else {
        set_level(Level::kDimmed);
    }
}

}  // namespace

void init(uint8_t brightness_percent, uint32_t dim_after_ms)
{
    s_dim_after = dim_after_ms;
    set_brightness(brightness_percent);
    lv_timer_create(on_poll, kPollMs, nullptr);
}

void set_brightness(uint8_t percent)
{
    s_brightness = percent < kMinBrightness ? kMinBrightness : percent > 100 ? 100 : percent;
    apply();
}

void set_dim_after(uint32_t ms)
{
    s_dim_after = ms;
}

void hold_awake(bool hold)
{
    s_hold = hold;
}

void wake()
{
    lv_display_trigger_activity(nullptr);
    set_level(Level::kAwake);
}

void sleep()
{
    set_level(Level::kOff);
    s_asleep = true;
}

bool is_off()
{
    return s_level == Level::kOff;
}

}  // namespace screen_power
