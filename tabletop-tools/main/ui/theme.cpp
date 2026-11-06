#include "ui/theme.h"

namespace theme {
namespace {

// Distinct, saturated hues spread around the color wheel, dark enough for white text.
constexpr uint32_t kPlayerColors[kPlayerColorCount] = {
    0x2563EB,  // blue
    0xDC2626,  // red
    0x16A34A,  // green
    0xD97706,  // amber
    0x7C3AED,  // violet
    0x0891B2,  // cyan
    0xDB2777,  // pink
    0x65A30D,  // lime
};

const lv_style_prop_t kTransitionProps[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_WIDTH, LV_STYLE_PROP_INV};
lv_style_transition_dsc_t s_press_transition;
lv_style_transition_dsc_t s_release_transition;

}  // namespace

lv_color_t player_color(uint8_t index)
{
    return lv_color_hex(kPlayerColors[index % kPlayerColorCount]);
}

lv_color_t shade(lv_color_t c, lv_opa_t amount)
{
    return lv_color_darken(c, amount);
}

void init()
{
    lv_style_transition_dsc_init(&s_press_transition, kTransitionProps, lv_anim_path_ease_out, kPressMs, 0,
                                 nullptr);
    lv_style_transition_dsc_init(&s_release_transition, kTransitionProps, lv_anim_path_ease_out, kReleaseMs,
                                 0, nullptr);
}

void style_screen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kText), 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *make_box(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

void set_bg(lv_obj_t *obj, lv_color_t color, lv_style_selector_t selector)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, selector);
    lv_obj_set_style_bg_color(obj, color, selector);
}

void make_pressable(lv_obj_t *obj, lv_color_t normal, lv_color_t pressed)
{
    set_bg(obj, normal, 0);
    set_bg(obj, pressed, LV_STATE_PRESSED);
    // LVGL uses the transition of the state being entered.
    lv_obj_set_style_transition(obj, &s_release_transition, 0);
    lv_obj_set_style_transition(obj, &s_press_transition, LV_STATE_PRESSED);
}

const lv_style_transition_dsc_t *state_transition()
{
    return &s_release_transition;
}

}  // namespace theme
