#include "apps/scorekeeper/game_menu.h"

#include <cstdio>

#include "ui/components/choice_row.h"
#include "ui/components/confirm_dialog.h"
#include "ui/motion.h"
#include "ui/theme.h"

namespace scorekeeper {
namespace {

constexpr int32_t kRowH = 64;
constexpr int32_t kRowPadX = 12;
constexpr int32_t kRowPadY = 8;
constexpr int32_t kTitleW = 150;
constexpr int32_t kPanelH = kRowH * 3 + theme::kGap * 3;
constexpr lv_opa_t kBackdropOpa = 150;
constexpr uint32_t kOpenMs = 240;
constexpr uint32_t kCloseMs = 180;

const char *const kStepLabels[] = {"1", "5", "10", "25"};
const char *const kDelayLabels[] = {"0.8s", "1.5s", "3s"};
static_assert(sizeof(kStepLabels) / sizeof(kStepLabels[0]) == kStepCount, "one label per step");
static_assert(sizeof(kDelayLabels) / sizeof(kDelayLabels[0]) == kCommitDelayCount, "one label per delay");

struct Menu {
    ScoreKeeper *model = nullptr;
    GameMenuActions actions = {};
    lv_obj_t *root = nullptr;  // clips the panel to the area below the header
    lv_obj_t *backdrop = nullptr;
    lv_obj_t *panel = nullptr;
};

Menu s_menu;

void on_step(size_t index, void *)
{
    s_menu.model->set_step_index(static_cast<uint8_t>(index));
    s_menu.actions.on_options_changed();
}

void on_delay(size_t index, void *)
{
    s_menu.model->set_commit_delay_index(static_cast<uint8_t>(index));
    s_menu.actions.on_options_changed();
}

void on_close(lv_event_t *)
{
    game_menu_close();
}

void on_new_game_confirmed(void *)
{
    void (*on_new_game)() = s_menu.actions.on_new_game;
    game_menu_close();
    on_new_game();
}

void on_new_game(lv_event_t *)
{
    char message[160];
    std::snprintf(message, sizeof(message), "Players go back to %s, all at 0.", default_player_names());
    ui::confirm_dialog_show("NEW GAME?", message, LV_SYMBOL_REFRESH " NEW GAME", on_new_game_confirmed, nullptr);
}

lv_obj_t *add_row(const char *title)
{
    lv_obj_t *row = theme::make_box(s_menu.panel);
    lv_obj_set_size(row, LV_PCT(100), kRowH);
    theme::set_bg(row, lv_color_hex(theme::kSurface));
    lv_obj_set_style_pad_hor(row, kRowPadX, 0);
    lv_obj_set_style_pad_ver(row, kRowPadY, 0);
    lv_obj_set_style_pad_column(row, kRowPadX, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (title != nullptr) {
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, title);
        lv_obj_set_width(label, kTitleW);
    }
    return row;
}

void add_button(lv_obj_t *row, const char *text, uint32_t text_color, lv_event_cb_t cb)
{
    lv_obj_t *btn = theme::make_box(row);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    theme::make_pressable(btn, lv_color_hex(theme::kSurfaceHigh), lv_color_hex(theme::kSurfacePressed));
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(text_color), 0);
    lv_obj_center(label);
}

void open(lv_obj_t *screen)
{
    lv_color_t accent = lv_color_hex(theme::kAccent);

    s_menu.root = theme::make_box(screen);
    lv_obj_set_size(s_menu.root, theme::kScreenW, theme::kScreenH - theme::kHeaderH);
    lv_obj_set_y(s_menu.root, theme::kHeaderH);

    // Dims the rows; tapping it closes the menu.
    s_menu.backdrop = theme::make_box(s_menu.root);
    lv_obj_set_size(s_menu.backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_menu.backdrop, lv_color_black(), 0);
    lv_obj_add_event_cb(s_menu.backdrop, on_close, LV_EVENT_CLICKED, nullptr);

    s_menu.panel = theme::make_box(s_menu.root);
    lv_obj_set_size(s_menu.panel, theme::kScreenW, kPanelH);
    theme::set_bg(s_menu.panel, lv_color_hex(theme::kBackground));
    lv_obj_set_style_pad_top(s_menu.panel, theme::kGap, 0);
    lv_obj_set_style_pad_row(s_menu.panel, theme::kGap, 0);
    lv_obj_set_flex_flow(s_menu.panel, LV_FLEX_FLOW_COLUMN);

    ui::choice_row_create(add_row("STEP SIZE"), kStepLabels, kStepCount, s_menu.model->step_index(), accent,
                          on_step, nullptr);
    ui::choice_row_create(add_row("ADD UP AFTER"), kDelayLabels, kCommitDelayCount,
                          s_menu.model->commit_delay_index(), accent, on_delay, nullptr);
    lv_obj_t *buttons = add_row(nullptr);
    lv_obj_set_style_pad_all(buttons, 0, 0);
    lv_obj_set_style_pad_column(buttons, theme::kGap, 0);
    add_button(buttons, LV_SYMBOL_REFRESH "  NEW GAME", theme::kDangerText, on_new_game);
    add_button(buttons, LV_SYMBOL_UP "  CLOSE", theme::kText, on_close);

    ui::animate(s_menu.panel, ui::exec_y, -kPanelH, 0, kOpenMs, lv_anim_path_ease_out);
    ui::animate(s_menu.backdrop, ui::exec_bg_opa, LV_OPA_TRANSP, kBackdropOpa, kOpenMs, lv_anim_path_ease_out);
}

}  // namespace

void game_menu_toggle(lv_obj_t *screen, ScoreKeeper &model, const GameMenuActions &actions)
{
    if (s_menu.root != nullptr) {
        game_menu_close();
        return;
    }
    s_menu.model = &model;
    s_menu.actions = actions;
    open(screen);
}

// Slides back up under the header and deletes itself, so it's safe inside its own callbacks.
void game_menu_close()
{
    if (s_menu.root == nullptr) {
        return;
    }
    ui::make_inert(s_menu.root);
    lv_obj_remove_flag(s_menu.root, LV_OBJ_FLAG_CLICKABLE);  // let touches reach the rows at once
    ui::animate(s_menu.panel, ui::exec_y, lv_obj_get_style_y(s_menu.panel, LV_PART_MAIN), -kPanelH, kCloseMs,
                lv_anim_path_ease_in);
    ui::animate(s_menu.backdrop, ui::exec_bg_opa, lv_obj_get_style_bg_opa(s_menu.backdrop, LV_PART_MAIN),
                LV_OPA_TRANSP, kCloseMs, lv_anim_path_ease_in);
    ui::delete_after(s_menu.root, kCloseMs);
    s_menu.root = nullptr;
}

}  // namespace scorekeeper
