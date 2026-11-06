#include "ui/components/confirm_dialog.h"

#include "ui/motion.h"
#include "ui/theme.h"

namespace ui {
namespace {

constexpr int32_t kPanelW = 420;
constexpr int32_t kTitleH = 56;
constexpr int32_t kButtonsH = 72;
constexpr int32_t kTextPad = 18;
constexpr lv_opa_t kBackdropOpa = 180;
constexpr int32_t kRisePx = 24;
constexpr uint32_t kFadeInMs = 220;
constexpr uint32_t kFadeOutMs = 160;

struct DialogState {
    lv_obj_t *backdrop;
    lv_obj_t *panel;
    ConfirmCb on_confirm;
    void *user_data;
    bool closed;
};

DialogState *s_open = nullptr;

// Fades out, then the backdrop (and everything on it) deletes itself.
void close(DialogState *state)
{
    state->closed = true;
    if (s_open == state) {
        s_open = nullptr;
    }
    make_inert(state->backdrop);
    animate(state->panel, exec_translate_y, 0, kRisePx, kFadeOutMs, lv_anim_path_ease_in);
    animate(state->backdrop, exec_bg_opa, kBackdropOpa, LV_OPA_TRANSP, kFadeOutMs, lv_anim_path_ease_in, 0,
            lv_obj_delete_anim_completed_cb);
}

void on_cancel_clicked(lv_event_t *e)
{
    auto *state = static_cast<DialogState *>(lv_event_get_user_data(e));
    if (!state->closed) {
        close(state);
    }
}

void on_confirm_clicked(lv_event_t *e)
{
    auto *state = static_cast<DialogState *>(lv_event_get_user_data(e));
    if (state->closed) {
        return;
    }
    close(state);
    state->on_confirm(state->user_data);
}

void on_delete(lv_event_t *e)
{
    auto *state = static_cast<DialogState *>(lv_event_get_user_data(e));
    if (s_open == state) {
        s_open = nullptr;
    }
    delete state;
}

void make_button(lv_obj_t *row, const char *text, lv_color_t color, lv_color_t pressed, lv_event_cb_t cb,
                 DialogState *state)
{
    lv_obj_t *btn = theme::make_box(row);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    theme::make_pressable(btn, color, pressed);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, state);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

}  // namespace

void confirm_dialog_show(const char *title, const char *message, const char *confirm_text,
                         ConfirmCb on_confirm, void *user_data)
{
    // The backdrop is clickable, so it swallows touches meant for the screen underneath.
    lv_obj_t *backdrop = theme::make_box(lv_layer_top());
    lv_obj_set_size(backdrop, theme::kScreenW, theme::kScreenH);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);

    lv_obj_t *panel = theme::make_box(backdrop);
    lv_obj_set_width(panel, kPanelW);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    theme::set_bg(panel, lv_color_hex(theme::kBackground));
    lv_obj_set_style_pad_row(panel, theme::kGap, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    auto *state = new DialogState{backdrop, panel, on_confirm, user_data, false};
    lv_obj_add_event_cb(backdrop, on_delete, LV_EVENT_DELETE, state);
    s_open = state;

    lv_obj_t *title_cell = theme::make_box(panel);
    lv_obj_set_size(title_cell, LV_PCT(100), kTitleH);
    lv_obj_set_style_pad_hor(title_cell, kTextPad, 0);
    theme::set_bg(title_cell, lv_color_hex(theme::kSurface));
    lv_obj_t *title_label = lv_label_create(title_cell);
    lv_label_set_text(title_label, title);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(title_label, LV_PCT(100));
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_center(title_label);

    lv_obj_t *message_cell = theme::make_box(panel);
    lv_obj_set_size(message_cell, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(message_cell, kTextPad, 0);
    theme::set_bg(message_cell, lv_color_hex(theme::kSurface));
    lv_obj_t *message_label = lv_label_create(message_cell);
    lv_label_set_text(message_label, message);
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(message_label, LV_PCT(100));
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(message_label, lv_color_hex(theme::kMutedText), 0);

    lv_obj_t *buttons = theme::make_box(panel);
    lv_obj_set_size(buttons, LV_PCT(100), kButtonsH);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, theme::kGap, 0);
    make_button(buttons, LV_SYMBOL_CLOSE " CANCEL", lv_color_hex(theme::kSurfaceHigh),
                lv_color_hex(theme::kSurfacePressed), on_cancel_clicked, state);
    lv_color_t danger = lv_color_hex(theme::kDanger);
    make_button(buttons, confirm_text, danger, theme::shade(danger, 60), on_confirm_clicked, state);

    animate(backdrop, exec_bg_opa, LV_OPA_TRANSP, kBackdropOpa, kFadeInMs, lv_anim_path_ease_out);
    animate(panel, exec_translate_y, kRisePx, 0, kFadeInMs, lv_anim_path_ease_out);
}

void confirm_dialog_cancel()
{
    if (s_open != nullptr) {
        close(s_open);
    }
}

}  // namespace ui
