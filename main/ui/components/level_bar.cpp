#include "ui/components/level_bar.h"

#include "ui/theme.h"

namespace ui {
namespace {

struct BarState {
    int32_t segments;
    int32_t min_level;
    int32_t level;
    LevelChangedCb on_change;
    void *user_data;
};

BarState *state_of(lv_obj_t *bar)
{
    return static_cast<BarState *>(lv_obj_get_user_data(bar));
}

void show(lv_obj_t *bar)
{
    BarState *state = state_of(bar);
    for (int32_t i = 0; i < state->segments; ++i) {
        lv_obj_set_state(lv_obj_get_child(bar, i), LV_STATE_CHECKED, i < state->level);
    }
}

// Touching the middle of segment i gives level i + 1; the far left edge gives 0.
void on_touch(lv_event_t *e)
{
    lv_obj_t *bar = lv_event_get_current_target_obj(e);
    BarState *state = state_of(bar);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t area;
    lv_obj_get_coords(bar, &area);
    int32_t width = lv_area_get_width(&area);
    int32_t x = p.x - area.x1;
    x = x < 0 ? 0 : x > width ? width : x;
    int32_t level = (x * state->segments + width / 2) / width;
    level = level < state->min_level ? state->min_level : level;
    if (level != state->level) {
        state->level = level;
        show(bar);
        state->on_change(level, state->user_data);
    }
}

void on_delete(lv_event_t *e)
{
    delete static_cast<BarState *>(lv_event_get_user_data(e));
}

}  // namespace

lv_obj_t *level_bar_create(lv_obj_t *parent, int32_t segments, int32_t min_level, int32_t level,
                           lv_color_t lit, LevelChangedCb on_change, void *user_data)
{
    lv_obj_t *bar = theme::make_box(parent);
    lv_obj_set_height(bar, LV_PCT(100));
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bar, theme::kGap, 0);
    // Keep the press while dragging sideways, and don't hand vertical wobble to a scrolling parent.
    lv_obj_add_flag(bar, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLL_CHAIN);

    for (int32_t i = 0; i < segments; ++i) {
        lv_obj_t *segment = theme::make_box(bar);
        lv_obj_remove_flag(segment, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_height(segment, LV_PCT(100));
        lv_obj_set_flex_grow(segment, 1);
        theme::set_bg(segment, lv_color_hex(theme::kSurfaceHigh));
        theme::set_bg(segment, lit, LV_STATE_CHECKED);
        lv_obj_set_style_transition(segment, theme::state_transition(), 0);
        lv_obj_set_style_transition(segment, theme::state_transition(), LV_STATE_CHECKED);
    }

    auto *state = new BarState{segments, min_level, level, on_change, user_data};
    lv_obj_set_user_data(bar, state);
    lv_obj_add_event_cb(bar, on_touch, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(bar, on_touch, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(bar, on_delete, LV_EVENT_DELETE, state);
    show(bar);
    return bar;
}

void level_bar_set(lv_obj_t *bar, int32_t level)
{
    state_of(bar)->level = level;
    show(bar);
}

}  // namespace ui
