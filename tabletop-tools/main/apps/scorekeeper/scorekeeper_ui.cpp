#include "apps/scorekeeper/scorekeeper_ui.h"

#include "apps/scorekeeper/game_menu.h"
#include "apps/scorekeeper/player_editor.h"
#include "apps/scorekeeper/reset_screen.h"
#include "apps/scorekeeper/scorekeeper_storage.h"
#include "ui/components/header_bar.h"
#include "ui/icons.h"
#include "ui/motion.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace scorekeeper {
namespace {

constexpr int32_t kRowsH = theme::kScreenH - theme::kHeaderH;
constexpr int32_t kScoreW = 150;
constexpr int32_t kButtonW = 76;
constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kNamePadX = 16;
constexpr int32_t kCompactRowH = 72;  // Rows shorter than this use the compact fonts.
constexpr uint32_t kSaveDelayMs = 1000;
constexpr int32_t kBumpPx = 8;
constexpr uint32_t kBumpMs = 180;
constexpr uint32_t kMergeMs = 220;
constexpr int32_t kMergePx = 14;
constexpr uint32_t kCountMs = 450;
constexpr int32_t kScorePadX = 6;
constexpr int32_t kChipGap = 6;
constexpr int32_t kChipPadX = 6;
constexpr int32_t kChipPadY = 2;
constexpr lv_opa_t kButtonShade = 45;  // ± tiles are a darker shade of the player color.
constexpr lv_opa_t kPressedShade = 110;
constexpr lv_opa_t kNamePressedShade = 50;
constexpr lv_opa_t kChipShade = 80;

constexpr size_t kScoreTiers = 3;

struct RowFonts {
    const lv_font_t *name;
    const lv_font_t *symbol;
    // Score and staged-change fonts, largest first; the first pair that fits the cell is used.
    const lv_font_t *score[kScoreTiers];
    const lv_font_t *chip[kScoreTiers];
};

constexpr RowFonts kLargeFonts = {&lv_font_montserrat_28,
                                  &lv_font_montserrat_36,
                                  {&lv_font_montserrat_48, &lv_font_montserrat_36, &lv_font_montserrat_28},
                                  {&lv_font_montserrat_36, &lv_font_montserrat_28, &lv_font_montserrat_20}};
constexpr RowFonts kCompactFonts = {&lv_font_montserrat_20,
                                    &lv_font_montserrat_28,
                                    {&lv_font_montserrat_36, &lv_font_montserrat_28, &lv_font_montserrat_20},
                                    {&lv_font_montserrat_28, &lv_font_montserrat_20, &lv_font_montserrat_14}};

// Widgets for one player's row. A row lives as long as its player, so re-sorting can slide it
// to a new slot instead of rebuilding it.
struct RowView {
    uint8_t player_id;  // 0 = unused slot
    uint8_t color;
    int32_t shown_score;
    int32_t shown_pending;
    lv_obj_t *row;
    lv_obj_t *name_cell;
    lv_obj_t *name_label;
    lv_obj_t *score_cell;
    lv_obj_t *score_label;
    lv_obj_t *pending_label;  // the "+6" chip beside the score
    lv_obj_t *minus;
    lv_obj_t *plus;
};

ScoreKeeper *s_model = nullptr;
lv_obj_t *s_screen = nullptr;
lv_obj_t *s_rows = nullptr;
lv_obj_t *s_add_button = nullptr;
lv_obj_t *s_sort_button = nullptr;
lv_timer_t *s_sort_timer = nullptr;
lv_timer_t *s_save_timer = nullptr;
const RowFonts *s_fonts = &kLargeFonts;

RowView s_views[kMaxPlayers] = {};

void sync_rows(bool animate, uint32_t score_delay, uint32_t move_delay);

int32_t text_width(const char *text, const lv_font_t *font)
{
    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

// Picks the largest fonts at which the score and any staged change fit side by side.
void fit_score(RowView &v)
{
    const char *score = lv_label_get_text(v.score_label);
    const char *pending = lv_label_get_text(v.pending_label);
    bool chip = !lv_obj_has_flag(v.pending_label, LV_OBJ_FLAG_HIDDEN);
    size_t tier = 0;
    for (; tier + 1 < kScoreTiers; ++tier) {
        int32_t width = text_width(score, s_fonts->score[tier]);
        if (chip) {
            width += kChipGap + 2 * kChipPadX + text_width(pending, s_fonts->chip[tier]);
        }
        if (width <= kScoreW - 2 * kScorePadX) {
            break;
        }
    }
    lv_obj_set_style_text_font(v.score_label, s_fonts->score[tier], 0);
    lv_obj_set_style_text_font(v.pending_label, s_fonts->chip[tier], 0);
}

void show_score(RowView &v, int32_t score)
{
    lv_label_set_text_fmt(v.score_label, "%d", static_cast<int>(score));
    fit_score(v);
}

// The score and chip labels carry their RowView as user data.
void exec_score(void *label, int32_t score)
{
    show_score(*static_cast<RowView *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(label))), score);
}

void set_chip_opa(lv_obj_t *chip, lv_opa_t opa)
{
    lv_obj_set_style_text_opa(chip, opa, 0);
    lv_obj_set_style_bg_opa(chip, opa, 0);
}

void exec_chip_opa(void *chip, int32_t opa)
{
    set_chip_opa(static_cast<lv_obj_t *>(chip), static_cast<lv_opa_t>(opa));
}

// Shows the staged change beside the score ("+6", "-5"), or hides it when there is none.
// `sign` nudges it in from the direction of the latest tap; 0 shows it without motion.
void show_pending(RowView &v, int32_t pending, int32_t sign)
{
    lv_obj_t *chip = v.pending_label;
    lv_anim_delete(chip, nullptr);  // A new tap cancels a merge in progress.
    lv_obj_set_style_translate_x(chip, 0, 0);
    lv_obj_set_style_translate_y(chip, 0, 0);
    set_chip_opa(chip, LV_OPA_COVER);
    v.shown_pending = pending;
    if (pending == 0) {
        lv_obj_add_flag(chip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text_fmt(chip, pending > 0 ? "+%d" : "%d", static_cast<int>(pending));
        lv_obj_remove_flag(chip, LV_OBJ_FLAG_HIDDEN);
        if (sign != 0) {
            ui::animate(chip, ui::exec_translate_y, sign * kBumpPx, 0, kBumpMs, lv_anim_path_ease_out);
        }
    }
    fit_score(v);
}

void on_merge_done(lv_anim_t *a)
{
    auto *chip = static_cast<lv_obj_t *>(lv_anim_get_user_data(a));
    lv_obj_add_flag(chip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(chip, 0, 0);
    set_chip_opa(chip, LV_OPA_COVER);
    fit_score(*static_cast<RowView *>(lv_obj_get_user_data(chip)));
}

// The chip slides into the score and fades away; the total then counts up (see sync_rows).
void merge_pending(RowView &v)
{
    v.shown_pending = 0;
    lv_obj_t *chip = v.pending_label;
    ui::animate(chip, ui::exec_translate_x, 0, -kMergePx, kMergeMs, lv_anim_path_ease_in);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, chip);
    lv_anim_set_exec_cb(&a, exec_chip_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, kMergeMs);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_user_data(&a, chip);
    lv_anim_set_completed_cb(&a, on_merge_done);
    lv_anim_start(&a);
}

// Player ids are never 0, so find_view(0) returns a free slot.
RowView *find_view(uint8_t player_id)
{
    for (RowView &v : s_views) {
        if (v.player_id == player_id) {
            return &v;
        }
    }
    return nullptr;
}

// Re-sorting waits until scoring pauses, so rows don't move away from a finger mid-tapping.
void schedule_sort()
{
    lv_timer_reset(s_sort_timer);
    lv_timer_resume(s_sort_timer);
}

// Every saved change goes through here. Saving waits for 1 s without changes, which keeps
// flash writes rare while losing at most about a second on power loss.
void mark_changed()
{
    lv_timer_reset(s_save_timer);
    lv_timer_resume(s_save_timer);
}

// Folds every staged change into its total at once: the chips merge into the scores, the
// totals count to their new values, and then the rows slide into the new order.
void commit_staged()
{
    lv_timer_pause(s_sort_timer);
    if (!s_model->has_pending()) {
        sync_rows(true, 0, 0);
        return;
    }
    s_model->commit_pending();
    for (RowView &v : s_views) {
        if (v.player_id != 0 && v.shown_pending != 0) {
            merge_pending(v);
        }
    }
    sync_rows(true, kMergeMs, kMergeMs + kCountMs);
    mark_changed();
}

void on_sort_timer(lv_timer_t *)
{
    commit_staged();
}

void on_save_timer(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    storage_save(*s_model);
}

void on_step(lv_event_t *e)
{
    auto *v = static_cast<RowView *>(lv_event_get_user_data(e));
    int index = s_model->find(v->player_id);
    if (index < 0) {
        return;
    }
    // Taps are staged beside the score and added to it when the sort timer fires.
    int32_t sign = lv_event_get_current_target_obj(e) == v->plus ? 1 : -1;
    show_pending(*v, s_model->stage(index, sign * s_model->increment()), sign);
    schedule_sort();
}

// Overlays call this after changing players or scores; rows animate once the overlay is away.
void on_players_changed()
{
    sync_rows(true, theme::kOverlayOutMs, theme::kOverlayOutMs);
    mark_changed();
}

void on_name_clicked(lv_event_t *e)
{
    auto *v = static_cast<RowView *>(lv_event_get_user_data(e));
    game_menu_close();
    player_editor_open(s_screen, *s_model, v->player_id, on_players_changed);
}

void on_add(lv_event_t *)
{
    game_menu_close();
    player_editor_open(s_screen, *s_model, 0, on_players_changed);
}

void on_reset_done()
{
    lv_timer_pause(s_sort_timer);
    on_players_changed();
}

void on_reset(lv_event_t *)
{
    game_menu_close();
    reset_screen_open(s_screen, *s_model, on_reset_done);
}

// The Score Keeper screen stays alive in the background, so nothing is lost by leaving.
void on_home(lv_event_t *)
{
    game_menu_close();
    nav::go_home();
}

void on_options_changed()
{
    lv_timer_set_period(s_sort_timer, s_model->commit_delay_ms());
    mark_changed();
}

void on_new_game()
{
    lv_timer_pause(s_sort_timer);
    s_model->load_defaults();
    sync_rows(true, 0, 0);  // old rows slide out, the default players slide in
    mark_changed();
}

const GameMenuActions kMenuActions = {on_options_changed, on_new_game};

void on_gear(lv_event_t *)
{
    game_menu_toggle(s_screen, *s_model, kMenuActions);
}

void update_sort_button()
{
    bool high_first = s_model->sort_mode() == SortMode::kHighFirst;
    ui::header_button_set_text(s_sort_button, high_first ? LV_SYMBOL_DOWN " 321" : LV_SYMBOL_UP " 123");
}

void on_sort(lv_event_t *)
{
    bool high_first = s_model->sort_mode() == SortMode::kHighFirst;
    s_model->set_sort_mode(high_first ? SortMode::kLowFirst : SortMode::kHighFirst);
    update_sort_button();
    commit_staged();
    mark_changed();
}

void build_header(lv_obj_t *parent)
{
    lv_obj_t *bar = ui::header_bar_create(parent);

    lv_obj_t *waffle = ui::header_button_create(bar, kHeaderIconW, nullptr, on_home, nullptr);
    lv_obj_center(ui::icon_waffle(waffle, lv_color_hex(theme::kText)));

    s_add_button = ui::header_button_create(bar, 0, LV_SYMBOL_PLUS " ADD", on_add, nullptr);
    s_sort_button = ui::header_button_create(bar, 0, "", on_sort, nullptr);
    ui::header_button_create(bar, 0, LV_SYMBOL_REFRESH " RESET", on_reset, nullptr);
    ui::header_button_create(bar, kHeaderIconW, LV_SYMBOL_SETTINGS, on_gear, nullptr);
    update_sort_button();
}

lv_obj_t *make_row_cell(lv_obj_t *row, int32_t width)
{
    lv_obj_t *cell = theme::make_box(row);
    lv_obj_set_height(cell, LV_PCT(100));
    if (width > 0) {
        lv_obj_set_width(cell, width);
    } else {
        lv_obj_set_flex_grow(cell, 1);
    }
    return cell;
}

lv_obj_t *make_step_button(RowView &v, const char *symbol)
{
    lv_obj_t *btn = make_row_cell(v.row, kButtonW);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);

    // A step lands as soon as the finger touches down, rather than when it lifts, so taps
    // feel immediate; holding repeats it from the long-press on.
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_PRESSED, &v);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED, &v);
    lv_obj_add_event_cb(btn, on_step, LV_EVENT_LONG_PRESSED_REPEAT, &v);
    return btn;
}

void apply_color(RowView &v, uint8_t color)
{
    lv_color_t c = theme::player_color(color);
    lv_color_t button = theme::shade(c, kButtonShade);
    lv_color_t pressed = theme::shade(c, kPressedShade);
    theme::make_pressable(v.name_cell, c, theme::shade(c, kNamePressedShade));
    theme::set_bg(v.score_cell, c);
    lv_obj_set_style_bg_color(v.pending_label, theme::shade(c, kChipShade), 0);
    theme::make_pressable(v.minus, button, pressed);
    theme::make_pressable(v.plus, button, pressed);
    v.color = color;
}

void apply_fonts(RowView &v)
{
    lv_obj_set_style_text_font(v.name_label, s_fonts->name, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(v.minus, 0), s_fonts->symbol, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(v.plus, 0), s_fonts->symbol, 0);
    fit_score(v);
}

RowView &create_view(const Player &p)
{
    // A free slot always exists: there are at most kMaxPlayers players, and rows of removed
    // players release their slot before new rows are created.
    RowView &v = *find_view(0);
    v.player_id = p.id;
    v.shown_score = p.score;
    v.shown_pending = 0;

    v.row = theme::make_box(s_rows);
    lv_obj_set_width(v.row, theme::kScreenW);
    lv_obj_set_flex_flow(v.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(v.row, theme::kGap, 0);

    v.name_cell = make_row_cell(v.row, 0);
    lv_obj_set_style_pad_hor(v.name_cell, kNamePadX, 0);
    lv_obj_add_event_cb(v.name_cell, on_name_clicked, LV_EVENT_CLICKED, &v);
    v.name_label = lv_label_create(v.name_cell);
    lv_label_set_long_mode(v.name_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(v.name_label, LV_PCT(100));
    lv_obj_align(v.name_label, LV_ALIGN_LEFT_MID, 0, 0);

    // Score and staged-change chip sit side by side, centered; a hidden chip takes no space.
    v.score_cell = make_row_cell(v.row, kScoreW);
    lv_obj_set_flex_flow(v.score_cell, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(v.score_cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(v.score_cell, kChipGap, 0);
    v.score_label = lv_label_create(v.score_cell);
    lv_obj_set_user_data(v.score_label, &v);
    lv_label_set_text_fmt(v.score_label, "%d", static_cast<int>(p.score));
    v.pending_label = lv_label_create(v.score_cell);
    lv_obj_set_user_data(v.pending_label, &v);
    lv_obj_set_style_pad_hor(v.pending_label, kChipPadX, 0);
    lv_obj_set_style_pad_ver(v.pending_label, kChipPadY, 0);
    lv_obj_add_flag(v.pending_label, LV_OBJ_FLAG_HIDDEN);

    v.minus = make_step_button(v, LV_SYMBOL_MINUS);
    v.plus = make_step_button(v, LV_SYMBOL_PLUS);
    apply_color(v, p.color);
    apply_fonts(v);
    show_pending(v, p.pending, 0);
    return v;
}

// The row slides out to the left and deletes itself; its slot is free immediately.
void retire_view(RowView &v, bool animate, uint32_t delay)
{
    lv_obj_t *row = v.row;
    // These animations reach the RowView through user data, and the slot is about to be reused.
    lv_anim_delete(v.score_label, nullptr);
    lv_anim_delete(v.pending_label, nullptr);
    v = RowView{};
    ui::make_inert(row);
    if (animate) {
        ui::animate(row, ui::exec_x, lv_obj_get_style_x(row, LV_PART_MAIN), -theme::kScreenW, theme::kMoveMs,
                    lv_anim_path_ease_in, delay, lv_obj_delete_anim_completed_cb);
    } else {
        lv_obj_delete(row);
    }
}

void update_score(RowView &v, int32_t score, bool animate, uint32_t delay)
{
    if (animate) {
        ui::animate(v.score_label, exec_score, v.shown_score, score, kCountMs, lv_anim_path_ease_out, delay);
    } else {
        lv_anim_delete(v.score_label, exec_score);
        show_score(v, score);
    }
    v.shown_score = score;
}

// Eases `obj` from `from` to `to`, or stops it where it is if it's already there.
void move(lv_obj_t *obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t delay)
{
    if (from == to) {
        lv_anim_delete(obj, exec);
        return;
    }
    ui::animate(obj, exec, from, to, theme::kMoveMs, lv_anim_path_ease_in_out, delay);
}

void place_row(RowView &v, int32_t y, int32_t h, bool is_new, bool animate, uint32_t delay)
{
    if (is_new || !animate) {
        // New rows start just past the right edge of their final slot and slide in.
        lv_obj_set_pos(v.row, is_new && animate ? theme::kScreenW : 0, y);
        lv_obj_set_height(v.row, h);
        if (is_new && animate) {
            ui::animate(v.row, ui::exec_x, theme::kScreenW, 0, theme::kMoveMs, lv_anim_path_ease_out, delay);
        }
        return;
    }
    move(v.row, ui::exec_y, lv_obj_get_style_y(v.row, LV_PART_MAIN), y, delay);
    move(v.row, ui::exec_height, lv_obj_get_style_height(v.row, LV_PART_MAIN), h, delay);
}

// Brings the rows in line with the model. With `animate`, changed scores count to their new
// value after `score_delay`, and after `move_delay` rows slide and resize into their new slots,
// new players slide in from the right and removed players slide out to the left. The delays let
// an overlay get out of the way, or the totals finish counting, first.
// Called only from the header, timers and overlay callbacks, never from a row's own event, so a
// row is never deleted inside its own callback.
void sync_rows(bool animate, uint32_t score_delay, uint32_t move_delay)
{
    for (RowView &v : s_views) {
        if (v.player_id != 0 && s_model->find(v.player_id) < 0) {
            retire_view(v, animate, move_delay);
        }
    }

    size_t order[kMaxPlayers];
    size_t n = s_model->display_order(order);
    int32_t count = static_cast<int32_t>(n);
    int32_t avail = kRowsH - theme::kGap * count;  // one gap above each row
    const RowFonts *fonts = count > 0 && avail / count < kCompactRowH ? &kCompactFonts : &kLargeFonts;
    bool fonts_changed = fonts != s_fonts;
    s_fonts = fonts;

    for (size_t slot = 0; slot < n; ++slot) {
        const Player &p = s_model->player(order[slot]);
        int32_t i = static_cast<int32_t>(slot);
        int32_t top = theme::kGap * (i + 1) + avail * i / count;
        int32_t bottom = theme::kGap * (i + 1) + avail * (i + 1) / count;

        RowView *v = find_view(p.id);
        bool is_new = v == nullptr;
        if (is_new) {
            v = &create_view(p);
        } else if (fonts_changed) {
            apply_fonts(*v);
        }
        lv_label_set_text(v->name_label, p.name);
        if (v->color != p.color) {
            apply_color(*v, p.color);
        }
        if (v->shown_score != p.score) {
            update_score(*v, p.score, animate, score_delay);
        }
        if (v->shown_pending != p.pending) {
            show_pending(*v, p.pending, 0);
        }
        place_row(*v, top, bottom - top, is_new, animate, move_delay);
    }
    ui::header_button_set_enabled(s_add_button, !s_model->full());
}

}  // namespace

void ui_create(lv_obj_t *parent, ScoreKeeper &model)
{
    s_model = &model;
    s_screen = parent;
    build_header(parent);

    // Rows are positioned absolutely (no flex) so they can be animated; the background shows
    // through the gaps between them. Rows outside this box are clipped while sliding.
    s_rows = theme::make_box(parent);
    lv_obj_set_size(s_rows, theme::kScreenW, kRowsH);
    lv_obj_set_y(s_rows, theme::kHeaderH);
    theme::set_bg(s_rows, lv_color_hex(theme::kBackground));

    s_sort_timer = lv_timer_create(on_sort_timer, s_model->commit_delay_ms(), nullptr);
    lv_timer_pause(s_sort_timer);
    s_save_timer = lv_timer_create(on_save_timer, kSaveDelayMs, nullptr);
    lv_timer_pause(s_save_timer);
    sync_rows(false, 0, 0);
}

}  // namespace scorekeeper
