#include "ui/icons.h"

#include <cmath>

#include "ui/theme.h"

namespace ui {
namespace {

lv_obj_t *make_frame(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *frame = theme::make_box(parent);
    lv_obj_set_size(frame, w, h);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    return frame;
}

lv_obj_t *rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color)
{
    lv_obj_t *r = make_frame(parent, w, h);
    lv_obj_set_pos(r, x, y);
    theme::set_bg(r, color);
    return r;
}

}  // namespace

lv_obj_t *icon_waffle(lv_obj_t *parent, lv_color_t color)
{
    constexpr int32_t kCell = 6;
    constexpr int32_t kGap = 3;
    lv_obj_t *icon = make_frame(parent, kCell * 3 + kGap * 2, kCell * 3 + kGap * 2);
    for (int32_t r = 0; r < 3; ++r) {
        for (int32_t c = 0; c < 3; ++c) {
            rect(icon, c * (kCell + kGap), r * (kCell + kGap), kCell, kCell, color);
        }
    }
    return icon;
}

lv_obj_t *icon_scorekeeper(lv_obj_t *parent)
{
    constexpr int32_t kNameW = 48;
    constexpr int32_t kScoreW = 22;
    constexpr int32_t kRowH = 14;
    constexpr int32_t kGap = 4;
    lv_obj_t *icon = make_frame(parent, kNameW + kGap + kScoreW, kRowH * 3 + kGap * 2);
    for (int32_t i = 0; i < 3; ++i) {
        lv_color_t c = theme::player_color(static_cast<uint8_t>(i));
        int32_t y = i * (kRowH + kGap);
        rect(icon, 0, y, kNameW, kRowH, c);
        rect(icon, kNameW + kGap, y, kScoreW, kRowH, theme::shade(c, 70));
    }
    return icon;
}

lv_obj_t *icon_calculator(lv_obj_t *parent, lv_color_t color)
{
    constexpr int32_t kKeyW = 14;
    constexpr int32_t kKeyH = 11;
    constexpr int32_t kGap = 4;
    constexpr int32_t kWidth = kKeyW * 3 + kGap * 2;
    constexpr int32_t kDisplayH = 13;
    lv_obj_t *icon = make_frame(parent, kWidth, kDisplayH + kGap + (kKeyH + kGap) * 3 - kGap);
    rect(icon, 0, 0, kWidth, kDisplayH, color);
    lv_color_t key = theme::shade(color, 70);
    for (int32_t r = 0; r < 3; ++r) {
        for (int32_t c = 0; c < 3; ++c) {
            rect(icon, c * (kKeyW + kGap), kDisplayH + kGap + r * (kKeyH + kGap), kKeyW, kKeyH, key);
        }
    }
    return icon;
}

lv_obj_t *icon_timer(lv_obj_t *parent, lv_color_t color)
{
    constexpr int32_t kSize = 60;
    constexpr int32_t kTick = 8;
    constexpr int32_t kTicks = 12;
    constexpr int32_t kLit = 9;
    constexpr float kRadius = (kSize - kTick) / 2.0f;
    lv_obj_t *icon = make_frame(parent, kSize, kSize);
    lv_color_t dim = lv_color_hex(theme::kSurfacePressed);
    for (int32_t i = 0; i < kTicks; ++i) {
        float a = 6.28318531f * i / kTicks - 1.57079633f;
        int32_t x = static_cast<int32_t>(kRadius + kRadius * std::cos(a));
        int32_t y = static_cast<int32_t>(kRadius + kRadius * std::sin(a));
        rect(icon, x, y, kTick, kTick, i < kLit ? color : dim);
    }
    return icon;
}

lv_obj_t *icon_dice(lv_obj_t *parent, lv_color_t color)
{
    constexpr int32_t kSize = 50;
    constexpr int32_t kBorder = 4;
    constexpr int32_t kPip = 9;
    lv_obj_t *icon = make_frame(parent, kSize, kSize);
    lv_obj_set_style_border_width(icon, kBorder, 0);
    lv_obj_set_style_border_color(icon, color, 0);

    constexpr int32_t kNear = 10;
    constexpr int32_t kFar = kSize - kNear - kPip;
    constexpr int32_t kMid = (kSize - kPip) / 2;
    const int32_t pips[5][2] = {{kNear, kNear}, {kFar, kNear}, {kMid, kMid}, {kNear, kFar}, {kFar, kFar}};
    for (const auto &pip : pips) {
        rect(icon, pip[0], pip[1], kPip, kPip, color);
    }
    return icon;
}

lv_obj_t *icon_picker(lv_obj_t *parent, lv_color_t color)
{
    constexpr int32_t kWidth = 66;
    constexpr int32_t kHeight = 58;
    constexpr int32_t kSpot = 24;
    constexpr int32_t kBorder = 3;
    lv_obj_t *icon = make_frame(parent, kWidth, kHeight);
    const int32_t spots[3][2] = {{(kWidth - kSpot) / 2, 0}, {0, kHeight - kSpot}, {kWidth - kSpot, kHeight - kSpot}};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *spot = make_frame(icon, kSpot, kSpot);
        lv_obj_set_pos(spot, spots[i][0], spots[i][1]);
        lv_obj_set_style_radius(spot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(spot, kBorder, 0);
        lv_obj_set_style_border_color(spot, color, 0);
        if (i == 0) {
            theme::set_bg(spot, color);  // the picked one
        } else {
            lv_obj_set_style_border_opa(spot, LV_OPA_50, 0);
        }
    }
    return icon;
}

}  // namespace ui
