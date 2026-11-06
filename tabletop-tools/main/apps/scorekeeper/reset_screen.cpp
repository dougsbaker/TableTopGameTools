#include "apps/scorekeeper/reset_screen.h"

#include <cstdio>
#include <cstring>

#include "ui/motion.h"
#include "ui/theme.h"

namespace scorekeeper {
namespace {

constexpr int32_t kLeftW = 240;
constexpr int32_t kLeftPadX = 6;
constexpr int32_t kControlH = 80;
constexpr int32_t kStepW = 56;
constexpr int32_t kCaptionPadBottom = 10;
constexpr int32_t kPreviewScoreW = 76;
constexpr int32_t kPreviewPadX = 12;
constexpr int32_t kCompactRowH = 72;  // Preview rows shorter than this use the compact fonts.
constexpr uint32_t kFastAfterRepeats = 20;  // About 2 s of holding at LVGL's repeat rate.
constexpr int32_t kFastStep = 10;
constexpr int32_t kBumpPx = 8;
constexpr uint32_t kBumpMs = 180;
constexpr lv_opa_t kAccentPressedShade = 60;

struct ResetScreen {
    ScoreKeeper *model = nullptr;
    ResetDoneCb on_done = nullptr;
    int32_t value = 0;
    uint32_t repeats = 0;
    bool compact = false;
    lv_obj_t *root = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *preview_scores[kMaxPlayers] = {};
};

ResetScreen s_reset;

const lv_font_t *value_font(size_t len)
{
    return len <= 3 ? &lv_font_montserrat_48 : len <= 5 ? &lv_font_montserrat_36 : &lv_font_montserrat_28;
}

const lv_font_t *preview_font(size_t len)
{
    if (len <= 3) {
        return s_reset.compact ? &lv_font_montserrat_28 : &lv_font_montserrat_36;
    }
    return len <= 4 ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
}

void refresh()
{
    char text[12];
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(s_reset.value));
    size_t len = std::strlen(text);

    lv_label_set_text(s_reset.value_label, text);
    lv_obj_set_style_text_font(s_reset.value_label, value_font(len), 0);
    for (size_t i = 0; i < s_reset.model->count(); ++i) {
        lv_label_set_text(s_reset.preview_scores[i], text);
        lv_obj_set_style_text_font(s_reset.preview_scores[i], preview_font(len), 0);
    }
}

// Slides away and deletes itself afterwards, so it's safe inside a child's event callback.
void close()
{
    ui::overlay_slide_out(s_reset.root);
    s_reset.root = nullptr;
}

// Tap = 1; hold = 1 at long-press, then repeats of 1 that speed up to 10 after ~2 s.
void on_step(lv_event_t *e)
{
    int32_t sign = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    lv_event_code_t code = lv_event_get_code(e);
    int32_t step = 1;
    if (code == LV_EVENT_LONG_PRESSED) {
        s_reset.repeats = 0;
    } else if (code == LV_EVENT_LONG_PRESSED_REPEAT && ++s_reset.repeats >= kFastAfterRepeats) {
        step = kFastStep;
    }
    s_reset.value = clamp_score(int64_t{s_reset.value} + sign * step);
    refresh();
    ui::animate(s_reset.value_label, ui::exec_translate_y, sign * kBumpPx, 0, kBumpMs, lv_anim_path_ease_out);
}

void on_cancel(lv_event_t *)
{
    if (s_reset.root != nullptr) {
        close();
    }
}

void on_confirm(lv_event_t *)
{
    if (s_reset.root == nullptr) {
        return;
    }
    s_reset.model->reset_scores(s_reset.value);
    ResetDoneCb on_done = s_reset.on_done;
    close();
    if (on_done != nullptr) {
        on_done();
    }
}

lv_obj_t *make_cell(lv_obj_t *parent, int32_t width, lv_color_t color)
{
    lv_obj_t *cell = theme::make_box(parent);
    lv_obj_set_height(cell, LV_PCT(100));
    if (width > 0) {
        lv_obj_set_width(cell, width);
    } else {
        lv_obj_set_flex_grow(cell, 1);
    }
    theme::set_bg(cell, color);
    return cell;
}

lv_obj_t *make_button(lv_obj_t *parent, int32_t width, const char *symbol, lv_color_t color, lv_color_t pressed)
{
    lv_obj_t *btn = make_cell(parent, width, color);
    theme::make_pressable(btn, color, pressed);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *make_neutral_button(lv_obj_t *parent, int32_t width, const char *symbol)
{
    return make_button(parent, width, symbol, lv_color_hex(theme::kSurfaceHigh),
                       lv_color_hex(theme::kSurfacePressed));
}

void make_step_button(lv_obj_t *parent, const char *symbol, int32_t sign)
{
    lv_obj_t *btn = make_neutral_button(parent, kStepW, symbol);
    void *user_data = reinterpret_cast<void *>(intptr_t{sign});
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_SHORT_CLICKED, user_data);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED, user_data);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED_REPEAT, user_data);
}

lv_obj_t *make_control_row(lv_obj_t *parent)
{
    lv_obj_t *row = theme::make_box(parent);
    lv_obj_set_size(row, LV_PCT(100), kControlH);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, theme::kGap, 0);
    return row;
}

void build_controls()
{
    lv_obj_t *left = theme::make_box(s_reset.root);
    lv_obj_set_size(left, kLeftW, LV_PCT(100));
    lv_obj_set_style_pad_hor(left, kLeftPadX, 0);
    lv_obj_set_style_pad_row(left, theme::kGap, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *caption = lv_label_create(left);
    lv_label_set_text(caption, "RESET ALL TO");
    lv_obj_set_style_text_color(caption, lv_color_hex(theme::kMutedText), 0);
    lv_obj_set_style_pad_bottom(caption, kCaptionPadBottom, 0);

    lv_obj_t *stepper = make_control_row(left);
    make_step_button(stepper, LV_SYMBOL_MINUS, -1);
    lv_obj_t *value_cell = make_cell(stepper, 0, lv_color_hex(theme::kSurface));
    s_reset.value_label = lv_label_create(value_cell);
    lv_obj_center(s_reset.value_label);
    make_step_button(stepper, LV_SYMBOL_PLUS, +1);

    lv_obj_t *actions = make_control_row(left);
    lv_obj_add_event_cb(make_neutral_button(actions, 0, LV_SYMBOL_CLOSE), on_cancel, LV_EVENT_CLICKED, nullptr);
    lv_color_t accent = lv_color_hex(theme::kAccent);
    lv_obj_t *confirm = make_button(actions, 0, LV_SYMBOL_OK, accent, theme::shade(accent, kAccentPressedShade));
    lv_obj_add_event_cb(confirm, on_confirm, LV_EVENT_CLICKED, nullptr);
}

// Players in the order they were added, which is also how they sort once all scores tie.
void build_preview()
{
    lv_obj_t *right = theme::make_box(s_reset.root);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, theme::kGap, 0);

    const ScoreKeeper &model = *s_reset.model;
    int32_t n = static_cast<int32_t>(model.count());
    s_reset.compact = n > 0 && (theme::kScreenH - theme::kGap * (n - 1)) / n < kCompactRowH;

    for (size_t i = 0; i < model.count(); ++i) {
        const Player &p = model.player(i);
        lv_color_t color = theme::player_color(p.color);

        lv_obj_t *row = theme::make_box(right);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_flex_grow(row, 1);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, theme::kGap, 0);

        lv_obj_t *name_cell = make_cell(row, 0, color);
        lv_obj_set_style_pad_hor(name_cell, kPreviewPadX, 0);
        lv_obj_t *name = lv_label_create(name_cell);
        lv_label_set_text(name, p.name);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(name, LV_PCT(100));
        lv_obj_set_style_text_font(name, s_reset.compact ? &lv_font_montserrat_20 : &lv_font_montserrat_28, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *score_cell = make_cell(row, kPreviewScoreW, color);
        s_reset.preview_scores[i] = lv_label_create(score_cell);
        lv_obj_center(s_reset.preview_scores[i]);
    }
}

}  // namespace

void reset_screen_open(lv_obj_t *parent, ScoreKeeper &model, ResetDoneCb on_done)
{
    if (s_reset.root != nullptr) {
        return;
    }
    s_reset = ResetScreen{};
    s_reset.model = &model;
    s_reset.on_done = on_done;
    s_reset.value = model.starting_score();

    // Opaque and clickable, so it fully covers (and blocks touches to) the Score Keeper.
    s_reset.root = theme::make_box(parent);
    lv_obj_set_size(s_reset.root, theme::kScreenW, theme::kScreenH);
    theme::set_bg(s_reset.root, lv_color_hex(theme::kBackground));
    lv_obj_set_flex_flow(s_reset.root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_reset.root, theme::kGap, 0);

    build_controls();
    build_preview();
    refresh();
    ui::overlay_slide_in(s_reset.root);
}

}  // namespace scorekeeper
