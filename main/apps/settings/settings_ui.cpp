#include "apps/settings/settings_ui.h"

#include <cstdio>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "services/power.h"
#include "services/settings.h"
#include "services/sound.h"
#include "services/storage.h"
#include "ui/battery_status.h"
#include "ui/components/choice_row.h"
#include "ui/components/confirm_dialog.h"
#include "ui/components/header_bar.h"
#include "ui/components/level_bar.h"
#include "ui/components/toast.h"
#include "ui/icons.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace settings_app {
namespace {

constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kRowH = 64;
constexpr int32_t kRowPadX = 12;
constexpr int32_t kRowPadY = 8;
constexpr int32_t kTitleW = 150;
constexpr int32_t kTestW = 90;
constexpr int32_t kSectionPadTop = 14;
constexpr int32_t kSectionPadBottom = 4;
constexpr int32_t kScrollbarW = 4;
constexpr int32_t kLevelSegments = 10;  // brightness and volume in 10% steps
constexpr uint32_t kRefreshMs = 2000;

const char *const kDimLabels[] = {"OFF", "1m", "2m", "5m", "10m"};
static_assert(sizeof(kDimLabels) / sizeof(kDimLabels[0]) == settings::kDimOptionCount, "one label per option");
const char *const kOnOff[] = {"ON", "OFF"};
const char *const kTouchLabels[] = {"NORMAL", "HIGH", "MAX"};
static_assert(sizeof(kTouchLabels) / sizeof(kTouchLabels[0]) == touch_controller::kSensitivityCount,
              "one label per level");

lv_obj_t *s_screen = nullptr;
lv_obj_t *s_alarm_row = nullptr;
lv_obj_t *s_clicks_row = nullptr;
lv_obj_t *s_touch_row = nullptr;
lv_obj_t *s_battery_label = nullptr;
lv_obj_t *s_battery_detail = nullptr;
lv_obj_t *s_memory_label = nullptr;

void on_home(lv_event_t *)
{
    nav::go_home();
}

void refresh_status()
{
    power::Status status = power::status();
    char text[64];
    if (!status.available) {
        lv_label_set_text(s_battery_label, "POWER CHIP NOT RESPONDING");
        lv_label_set_text(s_battery_detail, "");
    } else {
        char indicator[24];
        ui::battery_indicator_text(status, indicator, sizeof(indicator));
        const char *state = status.battery_present ? (status.charging ? "CHARGING" : "ON BATTERY")
                                                   : "NO BATTERY - RUNNING ON USB";
        std::snprintf(text, sizeof(text), "%s   %s", indicator, state);
        lv_label_set_text(s_battery_label, text);
        lv_obj_set_style_text_color(s_battery_label,
                                    lv_color_hex(ui::battery_low(status) ? theme::kDangerText : theme::kText), 0);
        std::snprintf(text, sizeof(text), "Battery %u.%02u V   USB %u.%02u V",
                      static_cast<unsigned>(status.battery_mv / 1000), static_cast<unsigned>(status.battery_mv % 1000 / 10),
                      static_cast<unsigned>(status.usb_mv / 1000), static_cast<unsigned>(status.usb_mv % 1000 / 10));
        lv_label_set_text(s_battery_detail, text);
    }
    std::snprintf(text, sizeof(text), "Free memory: %u KB  +  %u KB PSRAM",
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    lv_label_set_text(s_memory_label, text);
}

void on_refresh(lv_timer_t *)
{
    if (lv_screen_active() == s_screen) {
        refresh_status();
    }
}

// The timer's mute button changes the alarm setting too, so re-read it whenever we're shown.
void on_screen_load(lv_event_t *)
{
    const settings::Values &values = settings::get();
    ui::choice_row_set(s_alarm_row, values.alarm_sound ? 0 : 1);
    ui::choice_row_set(s_clicks_row, values.tap_clicks ? 0 : 1);
    refresh_status();
}

void on_brightness(int32_t level, void *)
{
    settings::set_brightness(static_cast<uint8_t>(level * 100 / kLevelSegments));
}

void on_dim(size_t index, void *)
{
    settings::set_dim_index(static_cast<uint8_t>(index));
}

void on_volume(int32_t level, void *)
{
    settings::set_volume(static_cast<uint8_t>(level * 100 / kLevelSegments));
}

void on_test(lv_event_t *)
{
    sound::play_test();
}

void on_alarm(size_t index, void *)
{
    settings::set_alarm_sound(index == 0);
}

void on_clicks(size_t index, void *)
{
    settings::set_tap_clicks(index == 0);
}

void on_touch(size_t index, void *)
{
    if (!settings::set_touch_sensitivity(static_cast<touch_controller::Sensitivity>(index))) {
        ui::choice_row_set(s_touch_row, static_cast<size_t>(settings::get().touch));
        ui::toast_show("COULDN'T CHANGE TOUCH SENSITIVITY", lv_color_hex(theme::kDanger));
    }
}

void on_reset_confirmed(void *)
{
    storage::erase_all();
    esp_restart();
}

void on_reset_all(lv_event_t *)
{
    ui::confirm_dialog_show("RESET ALL DATA?",
                            "Players, scores and settings go back to their defaults, then the device restarts.",
                            LV_SYMBOL_TRASH " RESET", on_reset_confirmed, nullptr);
}

void add_section(lv_obj_t *list, const char *title)
{
    lv_obj_t *label = lv_label_create(list);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(theme::kMutedText), 0);
    lv_obj_set_style_pad_left(label, kRowPadX, 0);
    lv_obj_set_style_pad_top(label, kSectionPadTop, 0);
    lv_obj_set_style_pad_bottom(label, kSectionPadBottom, 0);
}

// A setting tile: its title on the left; the caller adds the control, which fills the rest.
lv_obj_t *add_row(lv_obj_t *list, const char *title)
{
    lv_obj_t *row = theme::make_box(list);
    lv_obj_set_size(row, LV_PCT(100), kRowH);
    theme::set_bg(row, lv_color_hex(theme::kSurface));
    lv_obj_set_style_pad_hor(row, kRowPadX, 0);
    lv_obj_set_style_pad_ver(row, kRowPadY, 0);
    lv_obj_set_style_pad_column(row, kRowPadX, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, title);
    lv_obj_set_width(label, kTitleW);
    return row;
}

// A tile with a main line and a muted detail line underneath.
lv_obj_t *add_info(lv_obj_t *list, lv_obj_t **detail)
{
    lv_obj_t *tile = theme::make_box(list);
    lv_obj_set_size(tile, LV_PCT(100), LV_SIZE_CONTENT);
    theme::set_bg(tile, lv_color_hex(theme::kSurface));
    lv_obj_set_style_pad_hor(tile, kRowPadX, 0);
    lv_obj_set_style_pad_ver(tile, kRowPadY + 4, 0);
    lv_obj_set_style_pad_row(tile, 6, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *main_label = lv_label_create(tile);
    *detail = lv_label_create(tile);
    lv_obj_set_style_text_font(*detail, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*detail, lv_color_hex(theme::kMutedText), 0);
    return main_label;
}

void build_display(lv_obj_t *list, const settings::Values &values)
{
    add_section(list, "DISPLAY");
    ui::level_bar_create(add_row(list, "BRIGHTNESS"), kLevelSegments, 1, values.brightness * kLevelSegments / 100,
                         lv_color_hex(theme::kAccent), on_brightness, nullptr);
    ui::choice_row_create(add_row(list, "AUTO-DIM"), kDimLabels, settings::kDimOptionCount, values.dim_index,
                          lv_color_hex(theme::kAccent), on_dim, nullptr);
}

void build_touch(lv_obj_t *list, const settings::Values &values)
{
    add_section(list, "TOUCH - RAISE IT IF TAPS ARE MISSED ON BATTERY");
    s_touch_row = ui::choice_row_create(add_row(list, "SENSITIVITY"), kTouchLabels, touch_controller::kSensitivityCount,
                                        static_cast<size_t>(values.touch), lv_color_hex(theme::kAccent), on_touch,
                                        nullptr);
}

void build_sound(lv_obj_t *list, const settings::Values &values)
{
    add_section(list, "SOUND");
    lv_obj_t *volume = add_row(list, "VOLUME");
    ui::level_bar_create(volume, kLevelSegments, 0, values.volume * kLevelSegments / 100,
                         lv_color_hex(theme::kAccent), on_volume, nullptr);
    lv_obj_t *test = theme::make_box(volume);
    lv_obj_set_size(test, kTestW, LV_PCT(100));
    theme::make_pressable(test, lv_color_hex(theme::kSurfaceHigh), lv_color_hex(theme::kSurfacePressed));
    lv_obj_add_event_cb(test, on_test, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *test_label = lv_label_create(test);
    lv_label_set_text(test_label, "TEST");
    lv_obj_center(test_label);

    s_alarm_row = ui::choice_row_create(add_row(list, "TIMER ALARM"), kOnOff, 2, values.alarm_sound ? 0 : 1,
                                        lv_color_hex(theme::kAccent), on_alarm, nullptr);
    s_clicks_row = ui::choice_row_create(add_row(list, "TAP CLICKS"), kOnOff, 2, values.tap_clicks ? 0 : 1,
                                         lv_color_hex(theme::kAccent), on_clicks, nullptr);
}

void build_battery(lv_obj_t *list)
{
    add_section(list, "BATTERY");
    s_battery_label = add_info(list, &s_battery_detail);
}

void build_system(lv_obj_t *list)
{
    add_section(list, "SYSTEM");
    lv_obj_t *about = add_info(list, &s_memory_label);
    char text[64];
    std::snprintf(text, sizeof(text), "TABLETOP TOOLS %s", esp_app_get_description()->version);
    lv_label_set_text(about, text);

    lv_obj_t *about_detail = lv_label_create(lv_obj_get_parent(about));
    std::snprintf(text, sizeof(text), "ESP-IDF %s   LVGL %d.%d.%d", esp_get_idf_version(), LVGL_VERSION_MAJOR,
                  LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_label_set_text(about_detail, text);
    lv_obj_set_style_text_font(about_detail, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(about_detail, lv_color_hex(theme::kMutedText), 0);
    lv_obj_move_to_index(about_detail, 1);  // versions above the memory line

    lv_obj_t *reset = theme::make_box(list);
    lv_obj_set_size(reset, LV_PCT(100), kRowH);
    theme::make_pressable(reset, lv_color_hex(theme::kSurface), lv_color_hex(theme::kSurfaceHigh));
    lv_obj_add_event_cb(reset, on_reset_all, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(reset);
    lv_label_set_text(label, LV_SYMBOL_TRASH "  RESET ALL DATA");
    lv_obj_set_style_text_color(label, lv_color_hex(theme::kDangerText), 0);
    lv_obj_center(label);
}

}  // namespace

void ui_create(lv_obj_t *screen)
{
    s_screen = screen;
    lv_obj_t *bar = ui::header_bar_create(screen);
    lv_obj_t *waffle = ui::header_button_create(bar, kHeaderIconW, nullptr, on_home, nullptr);
    lv_obj_center(ui::icon_waffle(waffle, lv_color_hex(theme::kText)));
    lv_obj_t *title = ui::header_button_create(bar, 0, "SETTINGS", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);

    // A scrolling list of setting tiles below the header.
    lv_obj_t *list = theme::make_box(screen);
    lv_obj_set_size(list, theme::kScreenW, theme::kScreenH - theme::kHeaderH);
    lv_obj_set_y(list, theme::kHeaderH);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, theme::kGap, 0);
    lv_obj_set_style_pad_bottom(list, kSectionPadTop, 0);
    lv_obj_set_style_width(list, kScrollbarW, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, lv_color_hex(theme::kMutedText), LV_PART_SCROLLBAR);

    const settings::Values &values = settings::get();
    build_display(list, values);
    build_touch(list, values);
    build_sound(list, values);
    build_battery(list);
    build_system(list);

    lv_obj_add_event_cb(screen, on_screen_load, LV_EVENT_SCREEN_LOAD_START, nullptr);
    lv_timer_create(on_refresh, kRefreshMs, nullptr);
    refresh_status();
}

}  // namespace settings_app
