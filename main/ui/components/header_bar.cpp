#include "ui/components/header_bar.h"

#include "ui/theme.h"

namespace ui {

lv_obj_t *header_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = theme::make_box(parent);
    lv_obj_set_size(bar, theme::kScreenW, theme::kHeaderH);
    theme::set_bg(bar, lv_color_hex(theme::kBackground));
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bar, theme::kGap, 0);
    return bar;
}

lv_obj_t *header_button_create(lv_obj_t *bar, int32_t width, const char *text,
                               lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *cell = theme::make_box(bar);
    lv_obj_set_height(cell, LV_PCT(100));
    if (width > 0) {
        lv_obj_set_width(cell, width);
    } else {
        lv_obj_set_flex_grow(cell, 1);
    }
    theme::make_pressable(cell, lv_color_hex(theme::kSurface), lv_color_hex(theme::kSurfacePressed));

    if (text != nullptr) {
        lv_obj_t *label = lv_label_create(cell);
        lv_label_set_text(label, text);
        lv_obj_center(label);
    }
    if (on_click != nullptr) {
        lv_obj_add_event_cb(cell, on_click, LV_EVENT_CLICKED, user_data);
    }
    header_button_set_enabled(cell, on_click != nullptr);
    return cell;
}

void header_button_set_text(lv_obj_t *button, const char *text)
{
    lv_label_set_text(lv_obj_get_child(button, 0), text);
}

void header_button_set_enabled(lv_obj_t *button, bool enabled)
{
    if (enabled) {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_text_color(button, lv_color_hex(enabled ? theme::kText : theme::kMutedText), 0);
}

}  // namespace ui
