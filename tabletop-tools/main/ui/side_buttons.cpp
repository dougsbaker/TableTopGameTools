#include "ui/side_buttons.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "services/power.h"
#include "services/screen_power.h"
#include "services/sound.h"
#include "services/storage.h"
#include "ui/components/confirm_dialog.h"
#include "ui/components/toast.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace ui {
namespace {

const char *TAG = "side_buttons";

// BOOT pulls GPIO0 low. It's a strapping pin, so holding it while the board starts enters
// flashing mode; once running it's an ordinary input.
constexpr gpio_num_t kBootPin = GPIO_NUM_0;
constexpr uint32_t kPollMs = 20;
constexpr uint8_t kStableReads = 2;  // a change must hold for this many polls
constexpr uint32_t kStillOnMs = 2000;  // still running this long after switching off means it failed
constexpr int32_t kPowerRowGap = 14;

enum class PowerOff : uint8_t {
    kNone,
    kWaitingForRelease,  // PWR was held: the power-off screen is up until it's let go
    kSwitchingOff,       // the power chip has been told to cut the power
};

bool s_boot_down = false;
uint8_t s_boot_changed_reads = 0;
PowerOff s_power_off = PowerOff::kNone;
lv_obj_t *s_power_screen = nullptr;
void (*s_before_power_off)() = nullptr;

void on_power_key()
{
    sound::play_click();
    if (screen_power::is_off()) {
        screen_power::wake();
    } else {
        screen_power::sleep();
    }
}

// Covers every screen, so nothing can be touched while the board is going off.
void show_power_screen()
{
    s_power_screen = theme::make_box(lv_layer_top());
    lv_obj_set_size(s_power_screen, theme::kScreenW, theme::kScreenH);
    theme::set_bg(s_power_screen, lv_color_hex(theme::kBackground));
    lv_obj_add_flag(s_power_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(s_power_screen, lv_color_hex(theme::kText), 0);
    lv_obj_set_style_text_font(s_power_screen, &lv_font_montserrat_20, 0);
    lv_obj_set_flex_flow(s_power_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_power_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_power_screen, kPowerRowGap, 0);

    lv_obj_t *icon = lv_label_create(s_power_screen);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(theme::kDangerText), 0);

    lv_obj_t *title = lv_label_create(s_power_screen);
    lv_label_set_text(title, "POWERING OFF");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    lv_obj_t *hint = lv_label_create(s_power_screen);
    lv_label_set_text(hint, "Let go of the button");
    lv_obj_set_style_text_color(hint, lv_color_hex(theme::kMutedText), 0);
}

// Back to normal after the power chip didn't switch off.
void cancel_power_off()
{
    s_power_off = PowerOff::kNone;
    if (s_power_screen != nullptr) {
        lv_obj_delete(s_power_screen);
        s_power_screen = nullptr;
    }
    screen_power::wake();
    toast_show("COULDN'T POWER OFF", lv_color_hex(theme::kDanger));
}

void on_still_on(lv_timer_t *)
{
    ESP_LOGW(TAG, "still running %u ms after powering off", static_cast<unsigned>(kStillOnMs));
    cancel_power_off();
}

void on_power_hold()
{
    sound::play_click();
    screen_power::wake();
    show_power_screen();
    s_power_off = PowerOff::kWaitingForRelease;
}

// Only once the button is let go: one still held as the power goes could count as the press
// that turns the board back on.
void on_power_release()
{
    screen_power::sleep();  // dark at once, rather than showing the panel lose power
    if (s_before_power_off != nullptr) {
        s_before_power_off();
    }
    storage::flush();
    if (!power::shutdown()) {
        cancel_power_off();
        return;
    }
    s_power_off = PowerOff::kSwitchingOff;
    lv_timer_t *timer = lv_timer_create(on_still_on, kStillOnMs, nullptr);
    lv_timer_set_repeat_count(timer, 1);
}

void on_boot_key()
{
    sound::play_click();
    bool was_off = screen_power::is_off();
    screen_power::wake();
    if (was_off) {
        return;
    }
    confirm_dialog_cancel();  // it lives above every screen, so it would follow us home
    nav::go_home();
}

void on_poll(lv_timer_t *)
{
    bool pressed = power::take_key_press();
    switch (s_power_off) {
    case PowerOff::kNone:
        if (power::take_key_hold()) {
            on_power_hold();
        } else if (pressed) {
            on_power_key();
        }
        break;
    case PowerOff::kWaitingForRelease:
        if (power::take_key_release()) {
            on_power_release();
        }
        break;
    case PowerOff::kSwitchingOff:
        break;
    }

    bool down = gpio_get_level(kBootPin) == 0;
    if (down == s_boot_down) {
        s_boot_changed_reads = 0;
        return;
    }
    if (++s_boot_changed_reads < kStableReads) {
        return;
    }
    s_boot_changed_reads = 0;
    s_boot_down = down;
    if (down && s_power_off == PowerOff::kNone) {
        on_boot_key();
    }
}

}  // namespace

void side_buttons_start(void (*before_power_off)())
{
    s_before_power_off = before_power_off;
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << kBootPin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&config);
    lv_timer_create(on_poll, kPollMs, nullptr);
}

}  // namespace ui
