#include "apps/calculator/calculator_ui.h"

#include <cstdio>
#include <cstdlib>

#include "ui/components/header_bar.h"
#include "ui/icons.h"
#include "ui/motion.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace calculator {
namespace {

constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kUndoW = 140;
constexpr int32_t kTapeH = 96;
constexpr int32_t kDisplayH = 80;
constexpr int32_t kTotalW = 210;
constexpr int32_t kTapePad = 12;
constexpr int32_t kCellPad = 10;
constexpr int32_t kScrollbarW = 4;
constexpr uint32_t kCountMs = 300;
constexpr int32_t kBumpPx = 6;
constexpr uint32_t kBumpMs = 150;
constexpr lv_opa_t kAccentPressedShade = 60;
constexpr lv_opa_t kCaptionOpa = LV_OPA_70;
constexpr lv_opa_t kEmptyOpa = LV_OPA_40;

enum class Action : uint8_t { kDigit, kBackspace, kAdd, kSubtract, kClear };

struct Key {
    const char *text;
    Action action;
    uint8_t digit;
    uint8_t col;
    uint8_t row;
    uint8_t col_span;
    uint8_t row_span;
};

// 4x4 keypad; + is two rows tall and 0 two columns wide.
const Key kKeys[] = {
    {"7", Action::kDigit, 7, 0, 0, 1, 1},
    {"8", Action::kDigit, 8, 1, 0, 1, 1},
    {"9", Action::kDigit, 9, 2, 0, 1, 1},
    {LV_SYMBOL_BACKSPACE, Action::kBackspace, 0, 3, 0, 1, 1},
    {"4", Action::kDigit, 4, 0, 1, 1, 1},
    {"5", Action::kDigit, 5, 1, 1, 1, 1},
    {"6", Action::kDigit, 6, 2, 1, 1, 1},
    {LV_SYMBOL_MINUS, Action::kSubtract, 0, 3, 1, 1, 1},
    {"1", Action::kDigit, 1, 0, 2, 1, 1},
    {"2", Action::kDigit, 2, 1, 2, 1, 1},
    {"3", Action::kDigit, 3, 2, 2, 1, 1},
    {LV_SYMBOL_PLUS, Action::kAdd, 0, 3, 2, 1, 2},
    {"CLEAR", Action::kClear, 0, 0, 3, 1, 1},
    {"0", Action::kDigit, 0, 1, 3, 2, 1},
};

const int32_t kGridCols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
const int32_t kGridRows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

Tally *s_model = nullptr;
lv_obj_t *s_undo_button = nullptr;
lv_obj_t *s_tape = nullptr;
lv_obj_t *s_tape_label = nullptr;
lv_obj_t *s_entry_label = nullptr;
lv_obj_t *s_total_label = nullptr;
int32_t s_shown_total = 0;
char s_tape_text[1024];

void show_total(lv_obj_t *label, int32_t total)
{
    char text[12];
    int len = std::snprintf(text, sizeof(text), "%d", static_cast<int>(total));
    lv_label_set_text(label, text);
    const lv_font_t *font = len <= 5 ? &lv_font_montserrat_48 : len <= 7 ? &lv_font_montserrat_36 : &lv_font_montserrat_28;
    lv_obj_set_style_text_font(label, font, 0);
}

void exec_total(void *label, int32_t total)
{
    show_total(static_cast<lv_obj_t *>(label), total);
}

// "12 + 24 - 7 + 30": every term on the tape, in the order it was added.
void format_tape()
{
    size_t len = 0;
    s_tape_text[0] = '\0';
    for (size_t i = 0; i < s_model->count(); ++i) {
        int term = static_cast<int>(s_model->term(i));
        size_t room = sizeof(s_tape_text) - len;
        int written = i == 0 ? std::snprintf(s_tape_text + len, room, "%d", term)
                             : std::snprintf(s_tape_text + len, room, term < 0 ? " - %d" : " + %d", std::abs(term));
        if (written < 0 || static_cast<size_t>(written) >= room) {
            break;
        }
        len += static_cast<size_t>(written);
    }
}

void refresh()
{
    if (s_model->count() == 0) {
        lv_label_set_text(s_tape_label, "Type a number, then + to add it.");
        lv_obj_set_style_text_font(s_tape_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(s_tape_label, lv_color_hex(theme::kMutedText), 0);
    } else {
        format_tape();
        lv_label_set_text(s_tape_label, s_tape_text);
        lv_obj_set_style_text_font(s_tape_label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(s_tape_label, lv_color_hex(theme::kText), 0);
    }
    // Keep the newest terms in view; the tape can still be dragged back to review.
    lv_obj_update_layout(s_tape);
    lv_obj_scroll_by_bounded(s_tape, 0, -lv_obj_get_scroll_bottom(s_tape), LV_ANIM_ON);

    if (s_model->has_entry()) {
        lv_label_set_text_fmt(s_entry_label, "%d", static_cast<int>(s_model->entry()));
        lv_obj_set_style_text_opa(s_entry_label, LV_OPA_COVER, 0);
    } else {
        lv_label_set_text(s_entry_label, "0");
        lv_obj_set_style_text_opa(s_entry_label, kEmptyOpa, 0);
    }

    int32_t total = s_model->total();
    if (total != s_shown_total) {
        ui::animate(s_total_label, exec_total, s_shown_total, total, kCountMs, lv_anim_path_ease_out);
        s_shown_total = total;
    }
    ui::header_button_set_enabled(s_undo_button, s_model->can_undo());
}

void on_key(lv_event_t *e)
{
    const auto *key = static_cast<const Key *>(lv_event_get_user_data(e));
    switch (key->action) {
    case Action::kDigit:
        if (s_model->type_digit(key->digit)) {
            ui::animate(s_entry_label, ui::exec_translate_y, kBumpPx, 0, kBumpMs, lv_anim_path_ease_out);
        }
        break;
    case Action::kBackspace:
        s_model->backspace();
        break;
    case Action::kAdd:
    case Action::kSubtract:
        s_model->commit(key->action == Action::kSubtract);
        break;
    case Action::kClear:
        s_model->clear();
        break;
    }
    refresh();
}

void on_undo(lv_event_t *)
{
    s_model->undo();
    refresh();
}

void on_home(lv_event_t *)
{
    nav::go_home();
}

void build_header(lv_obj_t *screen)
{
    lv_obj_t *bar = ui::header_bar_create(screen);
    lv_obj_t *waffle = ui::header_button_create(bar, kHeaderIconW, nullptr, on_home, nullptr);
    lv_obj_center(ui::icon_waffle(waffle, lv_color_hex(theme::kText)));
    lv_obj_t *title = ui::header_button_create(bar, 0, "CALCULATOR", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    s_undo_button = ui::header_button_create(bar, kUndoW, LV_SYMBOL_LEFT " UNDO", on_undo, nullptr);
}

void build_tape(lv_obj_t *body)
{
    s_tape = theme::make_box(body);
    lv_obj_set_size(s_tape, LV_PCT(100), kTapeH);
    theme::set_bg(s_tape, lv_color_hex(theme::kSurface));
    lv_obj_set_style_pad_all(s_tape, kTapePad, 0);
    lv_obj_add_flag(s_tape, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_tape, LV_DIR_VER);
    lv_obj_set_style_width(s_tape, kScrollbarW, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_tape, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(s_tape, lv_color_hex(theme::kMutedText), LV_PART_SCROLLBAR);

    s_tape_label = lv_label_create(s_tape);
    lv_label_set_long_mode(s_tape_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(s_tape_label, LV_PCT(100));
}

lv_obj_t *make_display_cell(lv_obj_t *row, int32_t width, lv_color_t color, const char *caption)
{
    lv_obj_t *cell = theme::make_box(row);
    lv_obj_set_height(cell, LV_PCT(100));
    if (width > 0) {
        lv_obj_set_width(cell, width);
    } else {
        lv_obj_set_flex_grow(cell, 1);
    }
    theme::set_bg(cell, color);
    lv_obj_set_style_pad_all(cell, kCellPad, 0);

    lv_obj_t *caption_label = lv_label_create(cell);
    lv_label_set_text(caption_label, caption);
    lv_obj_set_style_text_font(caption_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(caption_label, kCaptionOpa, 0);
    lv_obj_align(caption_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *value = lv_label_create(cell);
    lv_obj_align(value, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    return value;
}

void build_display(lv_obj_t *body)
{
    lv_obj_t *row = theme::make_box(body);
    lv_obj_set_size(row, LV_PCT(100), kDisplayH);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, theme::kGap, 0);

    s_entry_label = make_display_cell(row, 0, lv_color_hex(theme::kSurface), "ENTRY");
    lv_obj_set_style_text_font(s_entry_label, &lv_font_montserrat_36, 0);
    s_total_label = make_display_cell(row, kTotalW, lv_color_hex(theme::kAccent), "TOTAL");
    show_total(s_total_label, s_shown_total);
}

void build_keypad(lv_obj_t *body)
{
    lv_obj_t *pad = theme::make_box(body);
    lv_obj_set_width(pad, LV_PCT(100));
    lv_obj_set_flex_grow(pad, 1);
    lv_obj_set_style_pad_row(pad, theme::kGap, 0);
    lv_obj_set_style_pad_column(pad, theme::kGap, 0);
    lv_obj_set_grid_dsc_array(pad, kGridCols, kGridRows);

    lv_color_t key_color = lv_color_hex(theme::kSurfaceHigh);
    lv_color_t key_pressed = lv_color_hex(theme::kSurfacePressed);
    lv_color_t accent = lv_color_hex(theme::kAccent);
    for (const Key &key : kKeys) {
        lv_obj_t *btn = theme::make_box(pad);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, key.col, key.col_span, LV_GRID_ALIGN_STRETCH, key.row,
                             key.row_span);
        if (key.action == Action::kAdd) {
            theme::make_pressable(btn, accent, theme::shade(accent, kAccentPressedShade));
        } else {
            theme::make_pressable(btn, key_color, key_pressed);
        }
        void *user_data = const_cast<Key *>(&key);
        lv_obj_add_event_cb(btn, on_key, LV_EVENT_CLICKED, user_data);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, key.text);
        lv_obj_center(label);
        if (key.action == Action::kClear) {
            lv_obj_set_style_text_color(label, lv_color_hex(theme::kDangerText), 0);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
        }
    }
}

}  // namespace

void ui_create(lv_obj_t *screen, Tally &model)
{
    s_model = &model;
    s_shown_total = model.total();
    build_header(screen);

    lv_obj_t *body = theme::make_box(screen);
    lv_obj_set_size(body, theme::kScreenW, theme::kScreenH - theme::kHeaderH);
    lv_obj_set_y(body, theme::kHeaderH);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(body, theme::kGap, 0);
    lv_obj_set_style_pad_row(body, theme::kGap, 0);

    build_tape(body);
    build_display(body);
    build_keypad(body);
    refresh();
}

}  // namespace calculator
