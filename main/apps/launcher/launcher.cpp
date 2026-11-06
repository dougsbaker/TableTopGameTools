#include "apps/launcher/launcher.h"

#include "apps/dice/dice_style.h"
#include "apps/picker/picker_style.h"
#include "apps/timer/timer_style.h"
#include "services/power.h"
#include "ui/battery_status.h"
#include "ui/components/header_bar.h"
#include "ui/icons.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace launcher {
namespace {

constexpr int32_t kIconBoxW = 90;
constexpr int32_t kIconBoxH = 64;
constexpr int32_t kNameGap = 12;
constexpr uint32_t kCalculatorColor = 0xD97706;
constexpr uint32_t kDisabledIcon = 0x3A4352;
constexpr uint32_t kSettingsColor = 0x94A3B8;
constexpr int32_t kBatteryW = 130;
constexpr uint32_t kBatteryRefreshMs = 3000;

lv_obj_t *s_battery = nullptr;

using IconFn = lv_obj_t *(*)(lv_obj_t *parent, lv_color_t color);

lv_obj_t *symbol_icon(lv_obj_t *parent, lv_color_t color, const char *symbol, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *scorekeeper_icon(lv_obj_t *parent, lv_color_t)
{
    return ui::icon_scorekeeper(parent);
}

lv_obj_t *settings_icon(lv_obj_t *parent, lv_color_t color)
{
    return symbol_icon(parent, color, LV_SYMBOL_SETTINGS, &lv_font_montserrat_48);
}

struct Tile {
    const char *name;
    IconFn icon;
    uint32_t icon_color;
    bool available;
    nav::Screen target;
};

const Tile kTiles[] = {
    {"SCORE KEEPER", scorekeeper_icon, 0, true, nav::Screen::kScoreKeeper},
    {"CALCULATOR", ui::icon_calculator, kCalculatorColor, true, nav::Screen::kCalculator},
    {"TIMER", ui::icon_timer, timer::kTimerColor, true, nav::Screen::kTimer},
    {"DICE", ui::icon_dice, dice::kDiceColor, true, nav::Screen::kDice},
    {"FINGER PICKER", ui::icon_picker, picker::kPickerColor, true, nav::Screen::kPicker},
    {"SETTINGS", settings_icon, kSettingsColor, true, nav::Screen::kSettings},
};

const int32_t kGridCols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
const int32_t kGridRows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

void on_tile(lv_event_t *e)
{
    nav::show(static_cast<const Tile *>(lv_event_get_user_data(e))->target);
}

void build_tile(lv_obj_t *grid, const Tile &tile, int32_t col, int32_t row)
{
    lv_obj_t *cell = theme::make_box(grid);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, kNameGap, 0);

    lv_color_t surface = lv_color_hex(theme::kSurface);
    if (tile.available) {
        theme::make_pressable(cell, surface, lv_color_hex(theme::kSurfaceHigh));
        lv_obj_add_event_cb(cell, on_tile, LV_EVENT_CLICKED, const_cast<Tile *>(&tile));
    } else {
        theme::set_bg(cell, surface);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    }

    // A fixed-size box keeps the names lined up whatever each icon's size.
    lv_obj_t *icon_box = theme::make_box(cell);
    lv_obj_set_size(icon_box, kIconBoxW, kIconBoxH);
    lv_obj_remove_flag(icon_box, LV_OBJ_FLAG_CLICKABLE);
    lv_color_t icon_color = lv_color_hex(tile.available ? tile.icon_color : kDisabledIcon);
    lv_obj_center(tile.icon(icon_box, icon_color));

    lv_obj_t *name = lv_label_create(cell);
    lv_label_set_text(name, tile.name);
    lv_obj_set_style_text_color(name, lv_color_hex(tile.available ? theme::kText : theme::kMutedText), 0);

    if (!tile.available) {
        lv_obj_t *soon = lv_label_create(cell);
        lv_label_set_text(soon, "COMING SOON");
        lv_obj_set_style_text_font(soon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(soon, lv_color_hex(kDisabledIcon), 0);
    }
}

void refresh_battery()
{
    power::Status status = power::status();
    char text[24];
    ui::battery_indicator_text(status, text, sizeof(text));
    ui::header_button_set_text(s_battery, text);
    uint32_t color = ui::battery_low(status) ? theme::kDangerText : theme::kMutedText;
    lv_obj_set_style_text_color(s_battery, lv_color_hex(color), 0);
}

void on_battery_refresh(lv_timer_t *)
{
    refresh_battery();
}

}  // namespace

void ui_create(lv_obj_t *screen)
{
    lv_obj_t *bar = ui::header_bar_create(screen);
    lv_obj_t *title = ui::header_button_create(bar, 0, "TABLETOP TOOLS", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    s_battery = ui::header_button_create(bar, kBatteryW, "", nullptr, nullptr);
    refresh_battery();
    lv_timer_create(on_battery_refresh, kBatteryRefreshMs, nullptr);

    lv_obj_t *grid = theme::make_box(screen);
    lv_obj_set_size(grid, theme::kScreenW, theme::kScreenH - theme::kHeaderH);
    lv_obj_set_y(grid, theme::kHeaderH);
    lv_obj_set_style_pad_top(grid, theme::kGap, 0);
    lv_obj_set_style_pad_row(grid, theme::kGap, 0);
    lv_obj_set_style_pad_column(grid, theme::kGap, 0);
    lv_obj_set_grid_dsc_array(grid, kGridCols, kGridRows);

    constexpr int32_t kCols = 2;
    int32_t i = 0;
    for (const Tile &tile : kTiles) {
        build_tile(grid, tile, i % kCols, i / kCols);
        ++i;
    }
}

}  // namespace launcher
