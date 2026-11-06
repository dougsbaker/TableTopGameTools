#include "apps/timer/time_editor.h"

#include "apps/timer/countdown.h"
#include "apps/timer/timer_style.h"
#include "ui/components/header_bar.h"
#include "ui/fonts.h"
#include "ui/motion.h"
#include "ui/theme.h"

namespace timer {
namespace {

constexpr int32_t kBarButtonW = 150;
constexpr int32_t kSpinnersH = 262;
constexpr int32_t kStepH = 64;
constexpr int32_t kCaptionPadBottom = 8;
constexpr uint32_t kMaxMinutes = kMaxSeconds / 60;
constexpr int32_t kBumpPx = 8;
constexpr uint32_t kBumpMs = 160;

struct Preset {
    const char *label;
    uint32_t seconds;
};

const Preset kPresets[] = {
    {"30s", 30}, {"45s", 45}, {"1m", 60}, {"2m", 120}, {"3m", 180}, {"5m", 300}, {"10m", 600}, {"15m", 900},
};
constexpr size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

const int32_t kPresetCols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
const int32_t kPresetRows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

enum class Field : uint8_t { kMinutes, kSeconds };

struct StepTarget {
    Field field;
    int8_t sign;
};

const StepTarget kStepTargets[] = {
    {Field::kMinutes, +1}, {Field::kMinutes, -1}, {Field::kSeconds, +1}, {Field::kSeconds, -1},
};

struct Editor {
    EditDoneCb on_done = nullptr;
    uint32_t minutes = 0;
    uint32_t seconds = 0;
    lv_obj_t *root = nullptr;
    lv_obj_t *minutes_label = nullptr;
    lv_obj_t *seconds_label = nullptr;
    lv_obj_t *presets[kPresetCount] = {};
};

Editor s_editor;

void refresh()
{
    lv_label_set_text_fmt(s_editor.minutes_label, "%u", static_cast<unsigned>(s_editor.minutes));
    lv_label_set_text_fmt(s_editor.seconds_label, "%02u", static_cast<unsigned>(s_editor.seconds));
    uint32_t total = s_editor.minutes * 60 + s_editor.seconds;
    for (size_t i = 0; i < kPresetCount; ++i) {
        lv_obj_set_state(s_editor.presets[i], LV_STATE_CHECKED, kPresets[i].seconds == total);
    }
}

void bump(lv_obj_t *label, int32_t sign)
{
    ui::animate(label, ui::exec_translate_y, sign * kBumpPx, 0, kBumpMs, lv_anim_path_ease_out);
}

// Minutes stop at 0 and 99; seconds wrap around (59 -> 0), independent of the minutes.
void on_step(lv_event_t *e)
{
    const auto *target = static_cast<const StepTarget *>(lv_event_get_user_data(e));
    if (target->field == Field::kMinutes) {
        if ((target->sign > 0 && s_editor.minutes >= kMaxMinutes) || (target->sign < 0 && s_editor.minutes == 0)) {
            return;
        }
        s_editor.minutes = target->sign > 0 ? s_editor.minutes + 1 : s_editor.minutes - 1;
        bump(s_editor.minutes_label, target->sign);
    } else {
        s_editor.seconds = (s_editor.seconds + 60 + target->sign) % 60;
        bump(s_editor.seconds_label, target->sign);
    }
    refresh();
}

void on_preset(lv_event_t *e)
{
    const auto *preset = static_cast<const Preset *>(lv_event_get_user_data(e));
    s_editor.minutes = preset->seconds / 60;
    s_editor.seconds = preset->seconds % 60;
    bump(s_editor.minutes_label, 1);
    bump(s_editor.seconds_label, 1);
    refresh();
}

// Slides away and deletes itself afterwards, so it's safe inside a child's event callback.
void close()
{
    ui::overlay_slide_out(s_editor.root);
    s_editor.root = nullptr;
}

void on_cancel_clicked(lv_event_t *)
{
    if (s_editor.root != nullptr) {
        close();
    }
}

void on_done_clicked(lv_event_t *)
{
    if (s_editor.root == nullptr) {
        return;
    }
    EditDoneCb done = s_editor.on_done;
    uint32_t total = s_editor.minutes * 60 + s_editor.seconds;
    close();
    done(total);
}

lv_obj_t *make_button(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = theme::make_box(parent);
    theme::make_pressable(btn, lv_color_hex(theme::kSurfaceHigh), lv_color_hex(theme::kSurfacePressed));
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return label;
}

void make_step_button(lv_obj_t *column, const char *symbol, const StepTarget *target)
{
    lv_obj_t *label = make_button(column, symbol);
    lv_obj_t *btn = lv_obj_get_parent(label);
    lv_obj_set_size(btn, LV_PCT(100), kStepH);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    void *user_data = const_cast<StepTarget *>(target);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_SHORT_CLICKED, user_data);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED, user_data);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED_REPEAT, user_data);
}

// One column: + above the value, - below it.
lv_obj_t *build_spinner(lv_obj_t *row, const char *caption, const StepTarget *plus, const StepTarget *minus)
{
    lv_obj_t *column = theme::make_box(row);
    lv_obj_set_height(column, LV_PCT(100));
    lv_obj_set_flex_grow(column, 1);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, theme::kGap, 0);

    make_step_button(column, LV_SYMBOL_PLUS, plus);

    lv_obj_t *cell = theme::make_box(column);
    lv_obj_set_width(cell, LV_PCT(100));
    lv_obj_set_flex_grow(cell, 1);
    theme::set_bg(cell, lv_color_hex(theme::kSurface));
    lv_obj_t *value = lv_label_create(cell);
    lv_obj_set_style_text_font(value, ui::font_display(), 0);
    lv_obj_center(value);
    lv_obj_t *caption_label = lv_label_create(cell);
    lv_label_set_text(caption_label, caption);
    lv_obj_set_style_text_font(caption_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(caption_label, lv_color_hex(theme::kMutedText), 0);
    lv_obj_align(caption_label, LV_ALIGN_BOTTOM_MID, 0, -kCaptionPadBottom);

    make_step_button(column, LV_SYMBOL_MINUS, minus);
    return value;
}

void build_presets(lv_obj_t *root)
{
    lv_obj_t *grid = theme::make_box(root);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_pad_row(grid, theme::kGap, 0);
    lv_obj_set_style_pad_column(grid, theme::kGap, 0);
    lv_obj_set_grid_dsc_array(grid, kPresetCols, kPresetRows);

    lv_color_t accent = lv_color_hex(kTimerColor);
    for (size_t i = 0; i < kPresetCount; ++i) {
        lv_obj_t *btn = lv_obj_get_parent(make_button(grid, kPresets[i].label));
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(i % 4), 1, LV_GRID_ALIGN_STRETCH,
                             static_cast<int32_t>(i / 4), 1);
        lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), &lv_font_montserrat_28, 0);
        // The preset matching the current time is highlighted.
        theme::set_bg(btn, accent, LV_STATE_CHECKED);
        lv_obj_set_style_transition(btn, theme::state_transition(), LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn, on_preset, LV_EVENT_CLICKED, const_cast<Preset *>(&kPresets[i]));
        s_editor.presets[i] = btn;
    }
}

}  // namespace

void time_editor_open(lv_obj_t *parent, uint32_t seconds, EditDoneCb on_done)
{
    if (s_editor.root != nullptr) {
        return;
    }
    s_editor = Editor{};
    s_editor.on_done = on_done;
    s_editor.minutes = seconds / 60;
    s_editor.seconds = seconds % 60;

    // Opaque and clickable, so it fully covers (and blocks touches to) the timer.
    s_editor.root = theme::make_box(parent);
    lv_obj_set_size(s_editor.root, theme::kScreenW, theme::kScreenH);
    theme::set_bg(s_editor.root, lv_color_hex(theme::kBackground));
    lv_obj_set_flex_flow(s_editor.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_editor.root, theme::kGap, 0);

    lv_obj_t *bar = ui::header_bar_create(s_editor.root);
    ui::header_button_create(bar, kBarButtonW, LV_SYMBOL_CLOSE " CANCEL", on_cancel_clicked, nullptr);
    lv_obj_t *title = ui::header_button_create(bar, 0, "SET TIME", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    ui::header_button_create(bar, kBarButtonW, LV_SYMBOL_OK " DONE", on_done_clicked, nullptr);

    lv_obj_t *spinners = theme::make_box(s_editor.root);
    lv_obj_set_size(spinners, LV_PCT(100), kSpinnersH);
    lv_obj_set_flex_flow(spinners, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(spinners, theme::kGap, 0);
    s_editor.minutes_label = build_spinner(spinners, "MIN", &kStepTargets[0], &kStepTargets[1]);
    s_editor.seconds_label = build_spinner(spinners, "SEC", &kStepTargets[2], &kStepTargets[3]);

    build_presets(s_editor.root);
    refresh();
    ui::overlay_slide_in(s_editor.root);
}

}  // namespace timer
