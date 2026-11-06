#include "apps/dice/dice_ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "apps/dice/dice_icons.h"
#include "apps/dice/dice_style.h"
#include "ui/components/header_bar.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/motion.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace dice {
namespace {

constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kClearW = 130;

// The dice sit evenly on a circle around the ROLL button, d4 at the top left, ascending clockwise.
// Starting 30 degrees off the top leaves the top and bottom of the circle free, which makes room for
// bigger tiles.
constexpr int32_t kCenterX = theme::kScreenW / 2;
constexpr int32_t kCenterY = 242;
constexpr int32_t kOrbit = 158;
constexpr int32_t kTileSize = 100;
constexpr int32_t kRollSize = 170;
constexpr int32_t kBadgeInset = 4;
constexpr int32_t kBreakdownY = 436;
constexpr int32_t kBreakdownPadX = 16;
constexpr float kTwoPi = 6.28318531f;

constexpr uint32_t kTumbleMs = 700;
constexpr int32_t kTumbleSteps = 1000;
constexpr int32_t kTumbleStepSize = 45;  // a new random total every this many steps of the eased value
constexpr uint32_t kPopMs = 320;
constexpr int32_t kPopShrink = 18;
constexpr int32_t kBumpPx = 6;
constexpr uint32_t kBumpMs = 160;
constexpr lv_opa_t kPressedShade = 60;
constexpr lv_opa_t kBadgeShade = 150;
constexpr int32_t kBreakdownH = 44;  // two lines, then "..."

DiceSet *s_model = nullptr;
lv_obj_t *s_tiles[kTypeCount] = {};
lv_obj_t *s_badges[kTypeCount] = {};
lv_obj_t *s_roll = nullptr;
lv_obj_t *s_roll_main = nullptr;
lv_obj_t *s_roll_sub = nullptr;
lv_obj_t *s_breakdown = nullptr;
lv_obj_t *s_clear = nullptr;
int32_t s_tumble_step = -1;
char s_text[512];

size_t type_of(lv_obj_t *tile)
{
    return reinterpret_cast<uintptr_t>(lv_obj_get_user_data(tile));
}

// The die's line drawing, centred: in its own color, or white on the filled tile once chosen.
void on_tile_draw(lv_event_t *e)
{
    lv_obj_t *tile = lv_event_get_current_target_obj(e);
    size_t type = type_of(tile);
    const lv_image_dsc_t *icon = die_icon(type);
    lv_area_t area;
    lv_obj_get_coords(tile, &area);
    lv_area_t icon_area;
    icon_area.x1 = area.x1 + (lv_area_get_width(&area) - static_cast<int32_t>(icon->header.w)) / 2;
    icon_area.y1 = area.y1 + (lv_area_get_height(&area) - static_cast<int32_t>(icon->header.h)) / 2;
    icon_area.x2 = icon_area.x1 + icon->header.w - 1;
    icon_area.y2 = icon_area.y1 + icon->header.h - 1;

    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);
    dsc.src = icon;
    dsc.recolor = lv_obj_has_state(tile, LV_STATE_CHECKED) ? lv_color_hex(theme::kText) : die_color(type);
    dsc.recolor_opa = LV_OPA_COVER;
    lv_draw_image(lv_event_get_layer(e), &dsc, &icon_area);
}

// "2d6 + d20": the dice that ROLL will throw.
void format_selection(char *out, size_t size)
{
    size_t len = 0;
    out[0] = '\0';
    for (size_t type = 0; type < kTypeCount; ++type) {
        uint8_t n = s_model->count(type);
        if (n == 0) {
            continue;
        }
        const char *sep = len == 0 ? "" : " + ";
        int written = n == 1 ? std::snprintf(out + len, size - len, "%sd%u", sep, kSides[type])
                             : std::snprintf(out + len, size - len, "%s%ud%u", sep, n, kSides[type]);
        if (written < 0 || static_cast<size_t>(written) >= size - len) {
            break;
        }
        len += static_cast<size_t>(written);
    }
}

// "d20: 14   2d6: 3+5": each die's result after a roll.
void format_breakdown(char *out, size_t size)
{
    size_t len = 0;
    out[0] = '\0';
    for (size_t type = 0; type < kTypeCount && len < size; ++type) {
        uint8_t n = s_model->count(type);
        if (n == 0) {
            continue;
        }
        int written = n == 1 ? std::snprintf(out + len, size - len, "%sd%u: ", len == 0 ? "" : "    ", kSides[type])
                             : std::snprintf(out + len, size - len, "%s%ud%u: ", len == 0 ? "" : "    ", n,
                                             kSides[type]);
        if (written < 0 || static_cast<size_t>(written) >= size - len) {
            break;
        }
        len += static_cast<size_t>(written);
        for (uint8_t i = 0; i < n && len < size; ++i) {
            written = std::snprintf(out + len, size - len, i == 0 ? "%u" : "+%u", s_model->result(type, i));
            if (written < 0 || static_cast<size_t>(written) >= size - len) {
                return;
            }
            len += static_cast<size_t>(written);
        }
    }
}

void show_total(uint32_t total)
{
    lv_label_set_text_fmt(s_roll_main, "%u", static_cast<unsigned>(total));
    lv_obj_set_style_text_font(s_roll_main, total < 100 ? ui::font_display() : &lv_font_montserrat_48, 0);
}

// The button's face and the line underneath, from the current selection and result.
void refresh()
{
    bool any = s_model->dice_count() > 0;
    for (size_t type = 0; type < kTypeCount; ++type) {
        uint8_t n = s_model->count(type);
        lv_obj_set_state(s_tiles[type], LV_STATE_CHECKED, n > 0);
        if (n > 0) {
            lv_label_set_text_fmt(s_badges[type], "x%u", n);
            lv_obj_remove_flag(s_badges[type], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_badges[type], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_color_t color = any ? lv_color_hex(kDiceColor) : lv_color_hex(theme::kSurfaceHigh);
    theme::make_pressable(s_roll, color, theme::shade(color, kPressedShade));
    lv_obj_set_style_radius(s_roll, LV_RADIUS_CIRCLE, 0);
    ui::header_button_set_enabled(s_clear, any);

    if (s_model->has_result()) {
        show_total(s_model->total());
        lv_label_set_text(s_roll_sub, "TAP TO ROLL AGAIN");
        format_breakdown(s_text, sizeof(s_text));
        lv_label_set_text(s_breakdown, s_text);
        return;
    }
    lv_label_set_text(s_roll_main, "ROLL");
    lv_obj_set_style_text_font(s_roll_main, &lv_font_montserrat_36, 0);
    if (any) {
        format_selection(s_text, sizeof(s_text));
        lv_label_set_text(s_roll_sub, s_text);
        lv_label_set_text(s_breakdown, "");
    } else {
        lv_label_set_text(s_roll_sub, "ADD DICE");
        lv_label_set_text(s_breakdown, "Tap a die to add it. Hold it to take one away.");
    }
}

// Tap adds a die; holding takes them away one by one. SHORT_CLICKED isn't sent after a long
// press, so letting go of a hold doesn't add one back.
void on_tile(lv_event_t *e)
{
    lv_obj_t *tile = lv_event_get_current_target_obj(e);
    size_t type = type_of(tile);
    bool adding = lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED;
    bool changed = adding ? s_model->add(type) : s_model->remove(type);
    if (!changed) {
        return;
    }
    lv_anim_delete(s_roll_main, nullptr);  // a change mid-tumble cancels the roll
    refresh();
    ui::animate(s_badges[type], ui::exec_translate_y, adding ? kBumpPx : -kBumpPx, 0, kBumpMs, lv_anim_path_ease_out);
}

void on_clear(lv_event_t *)
{
    lv_anim_delete(s_roll_main, nullptr);
    s_model->clear();
    refresh();
}

void exec_size(void *obj, int32_t size)
{
    lv_obj_set_size(static_cast<lv_obj_t *>(obj), size, size);
}

// While tumbling, the face flicks through random totals of the chosen dice, slowing down as the
// eased value approaches the end.
void exec_tumble(void *, int32_t value)
{
    int32_t step = value / kTumbleStepSize;
    if (step == s_tumble_step) {
        return;
    }
    s_tumble_step = step;
    uint32_t total = 0;
    for (size_t type = 0; type < kTypeCount; ++type) {
        for (uint8_t i = 0; i < s_model->count(type); ++i) {
            total += roll_die(kSides[type]);
        }
    }
    show_total(total);
}

void on_tumble_done(lv_anim_t *)
{
    refresh();
    ui::animate(s_roll, exec_size, kRollSize - kPopShrink, kRollSize, kPopMs, lv_anim_path_overshoot);
}

void on_roll(lv_event_t *)
{
    if (s_model->dice_count() == 0) {
        return;
    }
    s_model->roll();
    lv_label_set_text(s_roll_sub, "");
    lv_label_set_text(s_breakdown, "");
    s_tumble_step = -1;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_roll_main);
    lv_anim_set_exec_cb(&a, exec_tumble);
    lv_anim_set_values(&a, 0, kTumbleSteps);
    lv_anim_set_duration(&a, kTumbleMs);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, on_tumble_done);
    lv_anim_start(&a);
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
    lv_obj_t *title = ui::header_button_create(bar, 0, "DICE", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    s_clear = ui::header_button_create(bar, kClearW, LV_SYMBOL_CLOSE " CLEAR", on_clear, nullptr);
}

void build_tile(lv_obj_t *screen, size_t type)
{
    float angle = kTwoPi * static_cast<float>(type) / kTypeCount - kTwoPi / 3;  // d4 at -120 degrees
    int32_t x = kCenterX + static_cast<int32_t>(std::lround(kOrbit * std::cos(angle))) - kTileSize / 2;
    int32_t y = kCenterY + static_cast<int32_t>(std::lround(kOrbit * std::sin(angle))) - kTileSize / 2;

    lv_obj_t *tile = theme::make_box(screen);
    lv_obj_set_size(tile, kTileSize, kTileSize);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_user_data(tile, reinterpret_cast<void *>(uintptr_t{type}));
    lv_color_t color = die_color(type);
    theme::make_pressable(tile, lv_color_hex(theme::kSurface), lv_color_hex(theme::kSurfaceHigh));
    theme::set_bg(tile, color, LV_STATE_CHECKED);
    theme::set_bg(tile, theme::shade(color, kPressedShade),
                  static_cast<lv_style_selector_t>(LV_STATE_CHECKED) | LV_STATE_PRESSED);
    lv_obj_set_style_transition(tile, theme::state_transition(), LV_STATE_CHECKED);
    lv_obj_add_event_cb(tile, on_tile_draw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(tile, on_tile, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(tile, on_tile, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_add_event_cb(tile, on_tile, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    lv_obj_t *badge = lv_label_create(tile);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_14, 0);
    theme::set_bg(badge, theme::shade(color, kBadgeShade));
    lv_obj_set_style_pad_hor(badge, 5, 0);
    lv_obj_set_style_pad_ver(badge, 1, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -kBadgeInset, kBadgeInset);
    s_tiles[type] = tile;
    s_badges[type] = badge;
}

void build_roll_button(lv_obj_t *screen)
{
    s_roll = theme::make_box(screen);
    lv_obj_set_size(s_roll, kRollSize, kRollSize);
    lv_obj_align(s_roll, LV_ALIGN_CENTER, 0, kCenterY - theme::kScreenH / 2);
    lv_obj_add_event_cb(s_roll, on_roll, LV_EVENT_CLICKED, nullptr);

    s_roll_main = lv_label_create(s_roll);
    lv_obj_align(s_roll_main, LV_ALIGN_CENTER, 0, -8);
    s_roll_sub = lv_label_create(s_roll);
    lv_obj_set_style_text_font(s_roll_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(s_roll_sub, LV_OPA_80, 0);
    lv_label_set_long_mode(s_roll_sub, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(s_roll_sub, kRollSize - 40);
    lv_obj_set_style_text_align(s_roll_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_roll_sub, LV_ALIGN_CENTER, 0, 44);
}

}  // namespace

void ui_create(lv_obj_t *screen, DiceSet &model)
{
    s_model = &model;
    build_header(screen);
    for (size_t type = 0; type < kTypeCount; ++type) {
        build_tile(screen, type);
    }
    build_roll_button(screen);

    s_breakdown = lv_label_create(screen);
    lv_obj_set_size(s_breakdown, theme::kScreenW - 2 * kBreakdownPadX, kBreakdownH);
    lv_label_set_long_mode(s_breakdown, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_breakdown, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_breakdown, lv_color_hex(theme::kMutedText), 0);
    lv_obj_set_pos(s_breakdown, kBreakdownPadX, kBreakdownY);

    refresh();
}

}  // namespace dice
