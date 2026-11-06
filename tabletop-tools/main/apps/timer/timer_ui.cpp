#include "apps/timer/timer_ui.h"

#include <cmath>
#include <cstdint>

#include "apps/timer/time_editor.h"
#include "apps/timer/timer_style.h"
#include "esp_timer.h"
#include "services/screen_power.h"
#include "services/settings.h"
#include "services/sound.h"
#include "ui/components/header_bar.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/motion.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace timer {
namespace {

constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kSoundW = 100;

// Dial: 60 radial ticks; while setting, each tick is 5 s, so a full turn is 5 minutes.
constexpr int32_t kRingSize = 344;
constexpr int32_t kRingTop = theme::kHeaderH + 6;
constexpr int32_t kTicks = 60;
constexpr int32_t kTickOuter = 168;
constexpr int32_t kTickInner = 144;
constexpr int32_t kTickWidth = 6;
constexpr uint32_t kSecondsPerTick = 5;
constexpr int32_t kDragBandInner = 105;  // touches nearer the center than this aren't drags
constexpr float kTwoPi = 6.28318531f;

constexpr int32_t kTimeHitW = 270;
constexpr int32_t kTimeHitH = 150;
constexpr int32_t kTimeY = -10;
constexpr int32_t kStatusY = 56;

constexpr int32_t kButtonsH = 66;
constexpr int32_t kHalfButtonW = (theme::kScreenW - theme::kGap) / 2;

constexpr uint32_t kTickMs = 50;
constexpr uint32_t kFlashMs = 300;
constexpr uint32_t kFillMs = 400;
constexpr lv_opa_t kPressedShade = 60;
constexpr lv_opa_t kDisabledOpa = LV_OPA_40;
constexpr lv_opa_t kTimePressedOpa = LV_OPA_60;

struct TickLine {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
};

Countdown *s_model = nullptr;
lv_obj_t *s_screen = nullptr;
lv_obj_t *s_ring = nullptr;
lv_obj_t *s_time_label = nullptr;
lv_obj_t *s_status_label = nullptr;
lv_obj_t *s_primary = nullptr;
lv_obj_t *s_reset = nullptr;
lv_obj_t *s_sound_button = nullptr;

TickLine s_tick_lines[kTicks] = {};  // relative to the dial's center
int32_t s_lit = 0;                   // ticks lit, clockwise from 12 o'clock
uint32_t s_shown_seconds = UINT32_MAX;
bool s_dragging = false;
int32_t s_drag_steps = 0;
bool s_flash_on = false;
uint32_t s_flash_elapsed = 0;

void init_tick_lines()
{
    for (int32_t i = 0; i < kTicks; ++i) {
        float a = kTwoPi * (static_cast<float>(i) + 0.5f) / kTicks - kTwoPi / 4;
        float c = std::cos(a);
        float s = std::sin(a);
        s_tick_lines[i] = {static_cast<int16_t>(std::lround(c * kTickInner)),
                           static_cast<int16_t>(std::lround(s * kTickInner)),
                           static_cast<int16_t>(std::lround(c * kTickOuter)),
                           static_cast<int16_t>(std::lround(s * kTickOuter))};
    }
}

void set_lit(int32_t lit)
{
    if (lit != s_lit) {
        s_lit = lit;
        lv_obj_invalidate(s_ring);
    }
}

void exec_lit(void *, int32_t lit)
{
    set_lit(lit);
}

void animate_lit(int32_t target, uint32_t delay)
{
    ui::animate(s_ring, exec_lit, s_lit, target, kFillMs, lv_anim_path_ease_in_out, delay);
}

// While setting: the duration on the 5-minute dial (anything longer fills it).
int32_t idle_lit()
{
    int32_t ticks = static_cast<int32_t>((s_model->duration_s() + kSecondsPerTick - 1) / kSecondsPerTick);
    return ticks < kTicks ? ticks : kTicks;
}

// While counting: the share of the set time that's left, so the dial empties to zero.
int32_t counting_lit(int64_t remaining_us)
{
    int64_t total_us = static_cast<int64_t>(s_model->duration_s()) * 1000000;
    if (total_us <= 0) {
        return 0;
    }
    return static_cast<int32_t>((remaining_us * kTicks + total_us - 1) / total_us);
}

void show_seconds(uint32_t seconds)
{
    if (seconds == s_shown_seconds) {
        return;
    }
    char text[12];
    format_time(text, sizeof(text), seconds);
    lv_label_set_text(s_time_label, text);
    s_shown_seconds = seconds;
}

void on_ring_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(s_ring, &area);
    int32_t cx = area.x1 + kRingSize / 2;
    int32_t cy = area.y1 + kRingSize / 2;

    State state = s_model->state();
    bool show_handle = state == State::kIdle && s_lit > 0;  // the lit end is what you drag
    lv_color_t lit = lv_color_hex(kTimerColor);
    lv_color_t unlit = lv_color_hex(theme::kSurfaceHigh);
    lv_color_t handle = lv_color_hex(theme::kText);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = kTickWidth;
    for (int32_t i = 0; i < kTicks; ++i) {
        bool on = state == State::kFinished ? s_flash_on : i < s_lit;
        dsc.color = show_handle && i == s_lit - 1 ? handle : on ? lit : unlit;
        const TickLine &t = s_tick_lines[i];
        dsc.p1.x = cx + t.x1;
        dsc.p1.y = cy + t.y1;
        dsc.p2.x = cx + t.x2;
        dsc.p2.y = cy + t.y2;
        lv_draw_line(layer, &dsc);
    }
}

void set_button(lv_obj_t *btn, const char *text, uint32_t color)
{
    lv_label_set_text(lv_obj_get_child(btn, 0), text);
    lv_color_t c = lv_color_hex(color);
    lv_color_t pressed = color == theme::kSurfaceHigh ? lv_color_hex(theme::kSurfacePressed)
                                                      : theme::shade(c, kPressedShade);
    theme::make_pressable(btn, c, pressed);
}

// One wide START while idle; otherwise the primary button shrinks and RESET slides in.
void layout_buttons(bool animate)
{
    bool two = s_model->state() != State::kIdle;
    int32_t primary_w = two ? kHalfButtonW : theme::kScreenW;
    int32_t reset_x = two ? kHalfButtonW + theme::kGap : theme::kScreenW;
    if (!animate) {
        lv_obj_set_width(s_primary, primary_w);
        lv_obj_set_x(s_reset, reset_x);
        return;
    }
    ui::animate(s_primary, ui::exec_width, lv_obj_get_style_width(s_primary, LV_PART_MAIN), primary_w,
                theme::kMoveMs, lv_anim_path_ease_in_out);
    ui::animate(s_reset, ui::exec_x, lv_obj_get_style_x(s_reset, LV_PART_MAIN), reset_x, theme::kMoveMs,
                lv_anim_path_ease_in_out);
}

void refresh_controls()
{
    State state = s_model->state();
    const char *status = "";
    switch (state) {
    case State::kIdle:
        set_button(s_primary, LV_SYMBOL_PLAY " START", kTimerColor);
        status = "DRAG THE RING OR TAP THE TIME";
        break;
    case State::kRunning:
        set_button(s_primary, LV_SYMBOL_PAUSE " PAUSE", theme::kSurfaceHigh);
        break;
    case State::kPaused:
        set_button(s_primary, LV_SYMBOL_PLAY " RESUME", kTimerColor);
        status = "PAUSED";
        break;
    case State::kFinished:
        set_button(s_primary, LV_SYMBOL_PLAY " START", kTimerColor);
        status = "TIME'S UP";
        break;
    }
    lv_label_set_text(s_status_label, status);
    uint32_t status_color = state == State::kFinished ? kTimerColor : theme::kMutedText;
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(status_color), 0);

    bool can_start = state != State::kIdle || s_model->duration_s() > 0;
    if (can_start) {
        lv_obj_add_flag(s_primary, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(s_primary, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_text_opa(s_primary, can_start ? lv_opa_t{LV_OPA_COVER} : kDisabledOpa, 0);
    lv_obj_invalidate(s_ring);  // the drag handle only shows while idle
}

void on_state_changed()
{
    refresh_controls();
    layout_buttons(true);
    // A running countdown may dim the screen but never lets it go dark.
    screen_power::hold_awake(s_model->state() == State::kRunning);
}

void on_finished()
{
    screen_power::wake();
    if (settings::get().alarm_sound) {
        sound::play_alarm();
    }
    show_seconds(0);
    s_flash_on = true;
    s_flash_elapsed = 0;
    on_state_changed();
}

void on_tick(lv_timer_t *)
{
    int64_t now = esp_timer_get_time();
    if (s_model->update(now)) {
        on_finished();
    }
    State state = s_model->state();
    if (state == State::kRunning || state == State::kPaused) {
        int64_t remaining = s_model->remaining_us(now);
        show_seconds(seconds_left(remaining));
        if (lv_anim_get(s_ring, exec_lit) == nullptr) {
            set_lit(counting_lit(remaining));
        }
    } else if (state == State::kFinished) {
        s_flash_elapsed += kTickMs;
        if (s_flash_elapsed >= kFlashMs) {
            s_flash_elapsed = 0;
            s_flash_on = !s_flash_on;
            lv_obj_invalidate(s_ring);
        }
    }
}

// Position on the dial in ticks (0 at 12 o'clock, clockwise), or false if the touch is in the
// middle rather than on the ring.
bool touch_steps(int32_t *steps)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t area;
    lv_obj_get_coords(s_ring, &area);
    int32_t dx = p.x - (area.x1 + kRingSize / 2);
    int32_t dy = p.y - (area.y1 + kRingSize / 2);
    if (dx * dx + dy * dy < kDragBandInner * kDragBandInner) {
        return false;
    }
    float angle = std::atan2(static_cast<float>(dx), static_cast<float>(-dy));
    if (angle < 0) {
        angle += kTwoPi;
    }
    *steps = static_cast<int32_t>(std::lround(angle / kTwoPi * kTicks));
    return true;
}

void on_ring_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_dragging = false;
        return;
    }
    if (code == LV_EVENT_PRESSED && s_model->state() == State::kFinished) {
        sound::stop();
        return;
    }
    if (s_model->state() != State::kIdle) {
        return;
    }

    int32_t steps = 0;
    if (code == LV_EVENT_PRESSED) {
        // The first touch jumps straight to where the finger is.
        s_dragging = touch_steps(&steps);
        if (!s_dragging) {
            return;
        }
    } else if (code == LV_EVENT_PRESSING && s_dragging) {
        if (!touch_steps(&steps)) {
            return;  // drifted into the middle: hold the current value
        }
        // Crossing 12 o'clock would jump between empty and full; stop at the end instead.
        if (s_drag_steps > kTicks * 3 / 4 && steps < kTicks / 4) {
            steps = kTicks;
        } else if (s_drag_steps < kTicks / 4 && steps > kTicks * 3 / 4) {
            steps = 0;
        }
    } else {
        return;
    }
    s_drag_steps = steps;
    s_model->set_duration(static_cast<uint32_t>(steps) * kSecondsPerTick);
    lv_anim_delete(s_ring, exec_lit);
    set_lit(steps);
    show_seconds(s_model->duration_s());
    refresh_controls();
}

void on_edit_done(uint32_t seconds)
{
    s_model->set_duration(seconds);
    show_seconds(s_model->duration_s());
    refresh_controls();
    animate_lit(idle_lit(), theme::kOverlayOutMs);
}

void on_time_clicked(lv_event_t *)
{
    State state = s_model->state();
    if (state == State::kIdle) {
        time_editor_open(s_screen, s_model->duration_s(), on_edit_done);
    } else if (state == State::kFinished) {
        sound::stop();
    }
}

void on_primary(lv_event_t *)
{
    int64_t now = esp_timer_get_time();
    switch (s_model->state()) {
    case State::kIdle:
    case State::kFinished:
        sound::stop();
        s_model->start(now);
        animate_lit(kTicks, 0);  // the dial fills, then empties as time runs down
        show_seconds(s_model->duration_s());
        break;
    case State::kRunning:
        s_model->pause(now);
        break;
    case State::kPaused:
        s_model->resume(now);
        break;
    }
    on_state_changed();
}

void on_reset(lv_event_t *)
{
    sound::stop();
    s_model->reset();
    show_seconds(s_model->duration_s());
    animate_lit(idle_lit(), 0);
    on_state_changed();
}

void on_home(lv_event_t *)
{
    nav::go_home();
}

void show_sound_setting()
{
    ui::header_button_set_text(s_sound_button, settings::get().alarm_sound ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
}

// Same switch as SETTINGS > TIMER ALARM.
void on_sound(lv_event_t *)
{
    settings::set_alarm_sound(!settings::get().alarm_sound);
    show_sound_setting();
}

// The alarm can also be switched in Settings, so re-read it whenever the timer is shown.
void on_screen_load(lv_event_t *)
{
    show_sound_setting();
}

void build_header(lv_obj_t *screen)
{
    lv_obj_t *bar = ui::header_bar_create(screen);
    lv_obj_t *waffle = ui::header_button_create(bar, kHeaderIconW, nullptr, on_home, nullptr);
    lv_obj_center(ui::icon_waffle(waffle, lv_color_hex(theme::kText)));
    lv_obj_t *title = ui::header_button_create(bar, 0, "TIMER", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    s_sound_button = ui::header_button_create(bar, kSoundW, LV_SYMBOL_VOLUME_MAX, on_sound, nullptr);
}

void build_dial(lv_obj_t *screen)
{
    s_ring = theme::make_box(screen);
    lv_obj_set_size(s_ring, kRingSize, kRingSize);
    lv_obj_set_pos(s_ring, (theme::kScreenW - kRingSize) / 2, kRingTop);
    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_PRESS_LOCK);  // keep dragging even off the dial
    lv_obj_add_event_cb(s_ring, on_ring_draw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(s_ring, on_ring_event, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s_ring, on_ring_event, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(s_ring, on_ring_event, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(s_ring, on_ring_event, LV_EVENT_PRESS_LOST, nullptr);

    // The middle is its own target: tapping the time opens the editor.
    lv_obj_t *time_hit = theme::make_box(s_ring);
    lv_obj_set_size(time_hit, kTimeHitW, kTimeHitH);
    lv_obj_center(time_hit);
    lv_obj_set_style_text_opa(time_hit, kTimePressedOpa, LV_STATE_PRESSED);
    lv_obj_add_event_cb(time_hit, on_time_clicked, LV_EVENT_CLICKED, nullptr);

    s_time_label = lv_label_create(time_hit);
    lv_obj_set_style_text_font(s_time_label, ui::font_display(), 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, kTimeY);

    s_status_label = lv_label_create(time_hit);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, kStatusY);
}

lv_obj_t *make_action_button(lv_obj_t *row, lv_event_cb_t cb)
{
    lv_obj_t *btn = theme::make_box(row);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_center(label);
    return btn;
}

void build_buttons(lv_obj_t *screen)
{
    // Positioned by hand (no flex) so the buttons can slide; RESET waits off the right edge.
    lv_obj_t *row = theme::make_box(screen);
    lv_obj_set_size(row, theme::kScreenW, kButtonsH);
    lv_obj_set_y(row, theme::kScreenH - kButtonsH);
    s_primary = make_action_button(row, on_primary);
    s_reset = make_action_button(row, on_reset);
    lv_obj_set_width(s_reset, theme::kScreenW - kHalfButtonW - theme::kGap);
    set_button(s_reset, LV_SYMBOL_REFRESH " RESET", theme::kSurfaceHigh);
}

}  // namespace

void ui_create(lv_obj_t *screen, Countdown &model)
{
    s_model = &model;
    s_screen = screen;
    init_tick_lines();
    build_header(screen);
    build_dial(screen);
    build_buttons(screen);

    s_lit = idle_lit();
    show_seconds(s_model->duration_s());
    refresh_controls();
    layout_buttons(false);
    show_sound_setting();
    lv_obj_add_event_cb(screen, on_screen_load, LV_EVENT_SCREEN_LOAD_START, nullptr);
    lv_timer_create(on_tick, kTickMs, nullptr);
}

}  // namespace timer
