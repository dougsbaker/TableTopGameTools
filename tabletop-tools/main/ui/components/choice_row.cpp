#include "ui/components/choice_row.h"

#include <cstdint>

#include "ui/theme.h"

namespace ui {
namespace {

struct ChoiceState {
    size_t count;
    size_t selected;
    ChoiceChangedCb on_change;
    void *user_data;
};

ChoiceState *state_of(lv_obj_t *row)
{
    return static_cast<ChoiceState *>(lv_obj_get_user_data(row));
}

void show(lv_obj_t *row)
{
    ChoiceState *state = state_of(row);
    for (size_t i = 0; i < state->count; ++i) {
        lv_obj_set_state(lv_obj_get_child(row, static_cast<int32_t>(i)), LV_STATE_CHECKED, i == state->selected);
    }
}

void on_option(lv_event_t *e)
{
    lv_obj_t *option = lv_event_get_current_target_obj(e);
    lv_obj_t *row = lv_obj_get_parent(option);
    ChoiceState *state = state_of(row);
    size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    if (index != state->selected) {
        state->selected = index;
        show(row);
        state->on_change(index, state->user_data);
    }
}

void on_delete(lv_event_t *e)
{
    delete static_cast<ChoiceState *>(lv_event_get_user_data(e));
}

}  // namespace

lv_obj_t *choice_row_create(lv_obj_t *parent, const char *const *labels, size_t count, size_t selected,
                            lv_color_t accent, ChoiceChangedCb on_change, void *user_data)
{
    lv_obj_t *row = theme::make_box(parent);
    lv_obj_set_height(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, theme::kGap, 0);

    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *option = theme::make_box(row);
        lv_obj_set_height(option, LV_PCT(100));
        lv_obj_set_flex_grow(option, 1);
        theme::make_pressable(option, lv_color_hex(theme::kSurfaceHigh), lv_color_hex(theme::kSurfacePressed));
        theme::set_bg(option, accent, LV_STATE_CHECKED);
        lv_obj_set_style_transition(option, theme::state_transition(), LV_STATE_CHECKED);
        lv_obj_add_event_cb(option, on_option, LV_EVENT_CLICKED, reinterpret_cast<void *>(uintptr_t{i}));
        lv_obj_t *label = lv_label_create(option);
        lv_label_set_text(label, labels[i]);
        lv_obj_center(label);
    }

    auto *state = new ChoiceState{count, selected, on_change, user_data};
    lv_obj_set_user_data(row, state);
    lv_obj_add_event_cb(row, on_delete, LV_EVENT_DELETE, state);
    show(row);
    return row;
}

void choice_row_set(lv_obj_t *row, size_t selected)
{
    state_of(row)->selected = selected;
    show(row);
}

}  // namespace ui
