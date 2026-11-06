#include "ui/components/keyboard.h"

#include <cstring>

#include "ui/theme.h"

namespace ui {
namespace {

const char *const kMap[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "CLEAR", "SPACE", "",
};

// Button ids follow kMap order, skipping the "\n" row breaks.
constexpr uint32_t kBackspaceId = 26;
constexpr uint32_t kClearId = 27;
constexpr uint32_t kSpaceId = 28;

struct KeyboardState {
    char *text;
    size_t max_len;
    TextChangedCb on_change;
    void *user_data;
};

void on_key(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target_obj(e);
    auto *state = static_cast<KeyboardState *>(lv_event_get_user_data(e));
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    char *text = state->text;
    size_t len = std::strlen(text);
    if (id == kBackspaceId) {
        if (len == 0) {
            return;
        }
        text[len - 1] = '\0';
    } else if (id == kClearId) {
        if (len == 0) {
            return;
        }
        text[0] = '\0';
    } else {
        char ch = id == kSpaceId ? ' ' : lv_buttonmatrix_get_button_text(kb, id)[0];
        bool bad_space = ch == ' ' && (len == 0 || text[len - 1] == ' ');
        if (len >= state->max_len || bad_space) {
            return;
        }
        text[len] = ch;
        text[len + 1] = '\0';
    }
    state->on_change(state->user_data);
}

void on_delete(lv_event_t *e)
{
    delete static_cast<KeyboardState *>(lv_event_get_user_data(e));
}

}  // namespace

lv_obj_t *keyboard_create(lv_obj_t *parent, char *text, size_t max_len, TextChangedCb on_change,
                          void *user_data)
{
    lv_obj_t *kb = lv_buttonmatrix_create(parent);
    lv_obj_remove_style_all(kb);
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_flex_grow(kb, 1);
    lv_obj_set_style_pad_row(kb, theme::kGap, 0);
    lv_obj_set_style_pad_column(kb, theme::kGap, 0);

    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(theme::kSurfaceHigh), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(theme::kAccent),
                              static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_hex(theme::kText), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_28, LV_PART_ITEMS);

    lv_buttonmatrix_set_map(kb, kMap);
    lv_buttonmatrix_set_button_width(kb, kBackspaceId, 3);
    lv_buttonmatrix_set_button_width(kb, kSpaceId, 3);
    lv_buttonmatrix_set_button_ctrl(kb, kClearId, LV_BUTTONMATRIX_CTRL_NO_REPEAT);

    auto *state = new KeyboardState{text, max_len, on_change, user_data};
    lv_obj_add_event_cb(kb, on_key, LV_EVENT_VALUE_CHANGED, state);
    lv_obj_add_event_cb(kb, on_delete, LV_EVENT_DELETE, state);
    return kb;
}

}  // namespace ui
