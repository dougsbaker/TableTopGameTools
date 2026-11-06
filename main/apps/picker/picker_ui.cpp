#include "apps/picker/picker_ui.h"

#include <cmath>
#include <cstdio>

#include "apps/picker/touch_points.h"
#include "esp_random.h"
#include "services/sound.h"
#include "ui/components/header_bar.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/motion.h"
#include "ui/navigation.h"
#include "ui/theme.h"

namespace picker {
namespace {

static_assert(kColorCount == theme::kPlayerColorCount, "fingers take their colors from the player palette");

constexpr int32_t kHeaderIconW = 56;
constexpr int32_t kAreaY = theme::kHeaderH;  // fingers in the header are left to the header
constexpr int32_t kAreaH = theme::kScreenH - theme::kHeaderH;
constexpr int32_t kCenterX = theme::kScreenW / 2;
constexpr int32_t kCenterY = kAreaY + kAreaH / 2;
constexpr int32_t kHintBottom = 14;
constexpr int32_t kHintPadX = 16;
constexpr uint32_t kFrameMs = 16;
constexpr uint32_t kTextFadeMs = 200;

// A finger's light: a translucent disc with a solid ring, inside a soft halo.
constexpr int32_t kSpotR = 46;
constexpr int32_t kRingW = 6;
constexpr int32_t kHaloPad = 16;
constexpr int32_t kFlashPad = 16;   // the halo swells when a streak hits
constexpr int32_t kPulsePad = 10;   // the winner's halo breathes by this much
constexpr float kWinnerScale = 1.3f;
constexpr float kVanishScale = 0.25f;
constexpr lv_opa_t kFillOpa = 110;
constexpr lv_opa_t kWinnerFillOpa = 210;
constexpr lv_opa_t kHaloOpa = 50;
constexpr lv_opa_t kFlashOpa = 90;
constexpr uint32_t kAppearMs = 260;
constexpr uint32_t kVanishMs = 240;
constexpr uint32_t kFlashMs = 320;
constexpr uint32_t kRevealMs = 420;
constexpr uint32_t kPulseMs = 1500;
constexpr size_t kMaxSpots = kMaxFingers * 2;  // fingers, plus ones still fading out

// The countdown circle in the middle.
constexpr int32_t kCountR = 62;
constexpr int32_t kCountRingW = 8;
constexpr uint32_t kCountFadeMs = 250;

// Streaks of light: a white head trailing a tapering tail in the color it left from. Hops speed
// up as the countdown runs out; at the end every streak flies to the winner.
constexpr size_t kMaxStreaks = 3;
constexpr size_t kTrail = 9;  // head plus this many frames of tail, minus one
constexpr int32_t kStreakW = 9;
constexpr int32_t kHeadR = 6;
constexpr int32_t kHeadGlowR = 12;
constexpr lv_opa_t kHeadGlowOpa = 120;
constexpr uint32_t kHopSlowMs = 560;
constexpr uint32_t kHopFastMs = 170;
constexpr uint32_t kHopStaggerMs = 90;
constexpr uint32_t kFinalHopMs = kSettleMs - 120;

constexpr float kTwoPi = 6.28318531f;

// What a spot looked like when last drawn, so only changes are redrawn.
struct Look {
    int16_t x;
    int16_t y;
    int16_t r;  // 0 = not drawn
    int16_t halo;
    lv_opa_t opa;
    lv_opa_t fill_opa;
    lv_opa_t halo_opa;

    bool operator==(const Look &other) const
    {
        return x == other.x && y == other.y && r == other.r && halo == other.halo && opa == other.opa &&
               fill_opa == other.fill_opa && halo_opa == other.halo_opa;
    }
};

struct Spot {
    bool used;
    uint8_t id;
    int16_t x;
    int16_t y;
    lv_color_t color;
    uint32_t born;
    bool leaving;
    uint32_t left_at;
    bool flashed;
    uint32_t flash_at;
    Look look;
};

struct Streak {
    bool active;
    bool final_hop;  // heading for the winner
    bool landed;     // the final hop is over; the tail is catching up
    uint8_t to_id;
    lv_point_t from;
    uint32_t start;
    uint32_t duration;
    lv_color_t color;
    lv_point_t trail[kTrail];  // trail[0] is the head
    uint8_t trail_len;
    bool drawn;
    lv_area_t drawn_area;
};

struct CountLook {
    lv_opa_t opa;
    uint8_t digit;  // 0 = none
    int16_t sweep;  // degrees of ring left

    bool operator==(const CountLook &other) const
    {
        return opa == other.opa && digit == other.digit && sweep == other.sweep;
    }
};

Round *s_round = nullptr;
lv_obj_t *s_area = nullptr;
lv_obj_t *s_title = nullptr;
lv_obj_t *s_hint = nullptr;
lv_timer_t *s_timer = nullptr;
Spot s_spots[kMaxSpots] = {};
Streak s_streaks[kMaxStreaks] = {};
CountLook s_count = {};
uint8_t s_ticked_digit = 0;
char s_digit_text[4] = "";
const char *s_hint_text = nullptr;
bool s_title_shown = true;

float clamp01(float t)
{
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

// Fraction of `duration` elapsed since `start`; 0 before it, 1 after.
float progress(uint32_t now, uint32_t start, uint32_t duration)
{
    return clamp01(static_cast<float>(static_cast<int32_t>(now - start)) / static_cast<float>(duration));
}

float ease_in_out(float t)
{
    return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}

float ease_out_back(float t)
{
    constexpr float kOvershoot = 1.70158f;
    float u = t - 1.0f;
    return 1.0f + (kOvershoot + 1.0f) * u * u * u + kOvershoot * u * u;
}

lv_area_t circle_area(int32_t x, int32_t y, int32_t r)
{
    return {x - r, y - r, x + r, y + r};
}

void invalidate(const lv_area_t &area)
{
    lv_obj_invalidate_area(s_area, &area);
}

lv_point_t finger_point(const Finger &finger)
{
    return {finger.x, finger.y};
}

// A random finger other than `exclude`, or nullptr if there's none.
const Finger *random_other(uint8_t exclude)
{
    const Finger *options[kMaxFingers];
    size_t n = 0;
    for (size_t i = 0; i < s_round->count(); ++i) {
        if (s_round->finger(i).id != exclude) {
            options[n++] = &s_round->finger(i);
        }
    }
    return n == 0 ? nullptr : options[esp_random() % n];
}

// ---- Spots ----

Spot *find_spot(uint8_t id)
{
    for (Spot &spot : s_spots) {
        if (spot.used && !spot.leaving && spot.id == id) {
            return &spot;
        }
    }
    return nullptr;
}

void flash_spot(uint8_t id, uint32_t now)
{
    Spot *spot = find_spot(id);
    if (spot != nullptr) {
        spot->flashed = true;
        spot->flash_at = now;
    }
}

void release_all_spots(uint32_t now)
{
    for (Spot &spot : s_spots) {
        if (spot.used && !spot.leaving) {
            spot.leaving = true;
            spot.left_at = now;
        }
    }
}

// Spots follow the round's fingers: new fingers light up, fingers that left fade away.
void sync_spots(uint32_t now)
{
    for (Spot &spot : s_spots) {
        if (spot.used && !spot.leaving && s_round->find(spot.id) == nullptr) {
            spot.leaving = true;
            spot.left_at = now;
        }
    }
    for (size_t i = 0; i < s_round->count(); ++i) {
        const Finger &finger = s_round->finger(i);
        Spot *spot = find_spot(finger.id);
        if (spot == nullptr) {
            for (Spot &free_spot : s_spots) {
                if (!free_spot.used) {
                    free_spot = {};
                    free_spot.used = true;
                    free_spot.id = finger.id;
                    free_spot.color = theme::player_color(finger.color);
                    free_spot.born = now;
                    spot = &free_spot;
                    break;
                }
            }
        }
        if (spot != nullptr) {
            spot->x = finger.x;
            spot->y = finger.y;
        }
    }
}

bool is_winner(const Spot &spot)
{
    Phase phase = s_round->phase();
    return !spot.leaving && (phase == Phase::kSettling || phase == Phase::kPicked) &&
           spot.id == s_round->winner_id();
}

Look spot_look(const Spot &spot, uint32_t now)
{
    float scale = ease_out_back(progress(now, spot.born, kAppearMs));
    float opa = 1.0f;
    float halo_extra = 0.0f;
    float fill = kFillOpa;
    float halo_opa = kHaloOpa;

    if (spot.leaving) {
        float t = progress(now, spot.left_at, kVanishMs);
        scale *= 1.0f + kVanishScale * t;
        opa = 1.0f - t;
    } else if (s_round->phase() == Phase::kPicked) {
        uint32_t picked_at = s_round->phase_started();
        float t = progress(now, picked_at, kRevealMs);
        if (is_winner(spot)) {
            scale *= 1.0f + (kWinnerScale - 1.0f) * ease_out_back(t);
            fill = kFillOpa + (kWinnerFillOpa - kFillOpa) * t;
            float phase = kTwoPi * static_cast<float>((now - picked_at) % kPulseMs) / kPulseMs;
            halo_extra += kPulsePad * t * 0.5f * (1.0f - std::cos(phase));
        } else {
            opa = 1.0f - t;  // only the winner stays lit
        }
    }
    if (spot.flashed) {
        float boost = 1.0f - progress(now, spot.flash_at, kFlashMs);
        halo_extra += kFlashPad * boost;
        halo_opa += kFlashOpa * boost;
    }

    Look look = {};
    look.x = spot.x;
    look.y = spot.y;
    if (opa > 0.0f && scale > 0.0f) {
        look.r = static_cast<int16_t>(std::lround(kSpotR * scale));
        look.halo = static_cast<int16_t>(look.r + kHaloPad + std::lround(halo_extra));
        look.opa = static_cast<lv_opa_t>(std::lround(255.0f * opa));
        look.fill_opa = static_cast<lv_opa_t>(std::lround(fill));
        look.halo_opa = static_cast<lv_opa_t>(std::lround(halo_opa));
    }
    return look;
}

lv_area_t spot_area(const Look &look)
{
    int32_t extent = look.halo > look.r ? look.halo : look.r;
    return circle_area(look.x, look.y, extent + 2);
}

void refresh_spots(uint32_t now)
{
    for (Spot &spot : s_spots) {
        if (!spot.used) {
            continue;
        }
        Look look = spot_look(spot, now);
        if (!(look == spot.look)) {
            if (spot.look.r > 0) {
                invalidate(spot_area(spot.look));
            }
            if (look.r > 0) {
                invalidate(spot_area(look));
            }
            spot.look = look;
        }
        if (spot.leaving && progress(now, spot.left_at, kVanishMs) >= 1.0f) {
            spot.used = false;
        }
    }
}

void draw_spot(lv_layer_t *layer, const Spot &spot)
{
    const Look &look = spot.look;
    if (look.r <= 0 || look.opa == 0) {
        return;
    }
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = spot.color;
    dsc.bg_opa = static_cast<lv_opa_t>(look.halo_opa * look.opa / 255);
    lv_area_t area = circle_area(look.x, look.y, look.halo);
    lv_draw_rect(layer, &dsc, &area);

    dsc.bg_opa = static_cast<lv_opa_t>(look.fill_opa * look.opa / 255);
    dsc.border_width = kRingW;
    dsc.border_color = spot.color;
    dsc.border_opa = look.opa;
    area = circle_area(look.x, look.y, look.r);
    lv_draw_rect(layer, &dsc, &area);
}

// ---- Streaks ----

uint32_t hop_ms(uint32_t now)
{
    float left = static_cast<float>(s_round->remaining_ms(now)) / kCountdownMs;
    return kHopFastMs + static_cast<uint32_t>((kHopSlowMs - kHopFastMs) * left);
}

void stop_streak(Streak &streak)
{
    if (streak.drawn) {
        invalidate(streak.drawn_area);
    }
    streak = {};
}

void stop_all_streaks()
{
    for (Streak &streak : s_streaks) {
        stop_streak(streak);
    }
}

// Sends a streak off from `from` (a finger's position) toward finger `to`.
void hop(Streak &streak, lv_point_t from, lv_color_t color, uint8_t to, uint32_t start, uint32_t duration)
{
    streak.from = from;
    streak.color = color;
    streak.to_id = to;
    streak.start = start;
    streak.duration = duration;
}

void launch_streak(Streak &streak, size_t index, uint32_t now)
{
    const Finger &from = s_round->finger(index % s_round->count());
    const Finger *to = random_other(from.id);
    if (to == nullptr) {
        return;
    }
    streak = {};
    streak.active = true;
    hop(streak, finger_point(from), theme::player_color(from.color), to->id, now + index * kHopStaggerMs,
        hop_ms(now));
}

// One streak per finger, up to kMaxStreaks, while counting down.
void balance_streaks(uint32_t now)
{
    size_t wanted = s_round->count() < kMaxStreaks ? s_round->count() : kMaxStreaks;
    for (size_t i = 0; i < kMaxStreaks; ++i) {
        if (i < wanted && !s_streaks[i].active) {
            launch_streak(s_streaks[i], i, now);
        } else if (i >= wanted && s_streaks[i].active) {
            stop_streak(s_streaks[i]);
        }
    }
}

// At the end of the countdown every streak turns toward the winner.
void send_streaks_to_winner(uint32_t now)
{
    for (Streak &streak : s_streaks) {
        if (streak.active) {
            lv_point_t head = streak.trail_len > 0 ? streak.trail[0] : streak.from;
            streak.final_hop = true;
            hop(streak, head, streak.color, s_round->winner_id(), now, kFinalHopMs);
        }
    }
}

lv_area_t streak_area(const Streak &streak)
{
    lv_area_t area = {streak.trail[0].x, streak.trail[0].y, streak.trail[0].x, streak.trail[0].y};
    for (uint8_t i = 1; i < streak.trail_len; ++i) {
        area.x1 = LV_MIN(area.x1, streak.trail[i].x);
        area.y1 = LV_MIN(area.y1, streak.trail[i].y);
        area.x2 = LV_MAX(area.x2, streak.trail[i].x);
        area.y2 = LV_MAX(area.y2, streak.trail[i].y);
    }
    constexpr int32_t kPad = (kHeadGlowR > kStreakW ? kHeadGlowR : kStreakW) + 2;
    return {area.x1 - kPad, area.y1 - kPad, area.x2 + kPad, area.y2 + kPad};
}

void update_streak(Streak &streak, uint32_t now)
{
    if (!streak.active) {
        return;
    }
    lv_point_t head = streak.trail_len > 0 ? streak.trail[0] : streak.from;
    if (!streak.landed) {
        const Finger *target = s_round->find(streak.to_id);
        if (target == nullptr) {  // its finger let go: head somewhere else
            const Finger *next = random_other(streak.to_id);
            if (next == nullptr) {
                stop_streak(streak);
                return;
            }
            hop(streak, head, streak.color, next->id, now, hop_ms(now));
            target = next;
        }
        float t = progress(now, streak.start, streak.duration);
        float eased = streak.final_hop ? t * t : ease_in_out(t);
        head.x = streak.from.x + static_cast<int32_t>(std::lround((target->x - streak.from.x) * eased));
        head.y = streak.from.y + static_cast<int32_t>(std::lround((target->y - streak.from.y) * eased));
        if (t >= 1.0f) {
            flash_spot(target->id, now);
            if (streak.final_hop) {
                streak.landed = true;
            } else {
                const Finger *next = random_other(target->id);
                if (next != nullptr) {
                    hop(streak, finger_point(*target), theme::player_color(target->color), next->id, now, hop_ms(now));
                }
            }
        }
    }

    for (size_t i = kTrail - 1; i > 0; --i) {
        streak.trail[i] = streak.trail[i - 1];
    }
    streak.trail[0] = head;
    if (streak.trail_len < kTrail) {
        ++streak.trail_len;
    }

    if (streak.drawn) {
        invalidate(streak.drawn_area);
    }
    streak.drawn_area = streak_area(streak);
    streak.drawn = true;
    invalidate(streak.drawn_area);

    if (streak.landed && streak.trail[kTrail - 1].x == head.x && streak.trail[kTrail - 1].y == head.y) {
        stop_streak(streak);  // the tail has caught up with the head
    }
}

void draw_streak(lv_layer_t *layer, const Streak &streak)
{
    if (!streak.active || streak.trail_len == 0) {
        return;
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = streak.color;
    line.round_start = 1;
    line.round_end = 1;
    for (uint8_t i = 0; i + 1 < streak.trail_len; ++i) {
        const lv_point_t &a = streak.trail[i];
        const lv_point_t &b = streak.trail[i + 1];
        if (a.x == b.x && a.y == b.y) {
            continue;
        }
        float fade = 1.0f - static_cast<float>(i) / (kTrail - 1);
        line.width = LV_MAX(1, static_cast<int32_t>(std::lround(kStreakW * fade)));
        line.opa = static_cast<lv_opa_t>(std::lround(255.0f * fade));
        line.p1.x = a.x;
        line.p1.y = a.y;
        line.p2.x = b.x;
        line.p2.y = b.y;
        lv_draw_line(layer, &line);
    }
    if (streak.landed) {
        return;
    }
    const lv_point_t &head = streak.trail[0];
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    dot.bg_color = streak.color;
    dot.bg_opa = kHeadGlowOpa;
    lv_area_t area = circle_area(head.x, head.y, kHeadGlowR);
    lv_draw_rect(layer, &dot, &area);
    dot.bg_color = lv_color_white();
    dot.bg_opa = LV_OPA_COVER;
    area = circle_area(head.x, head.y, kHeadR);
    lv_draw_rect(layer, &dot, &area);
}

// ---- Countdown ----

void refresh_countdown(uint32_t now)
{
    CountLook look = {};
    Phase phase = s_round->phase();
    if (phase == Phase::kCounting) {
        uint32_t left = s_round->remaining_ms(now);
        look.opa = LV_OPA_COVER;
        look.digit = static_cast<uint8_t>((left + 999) / 1000);
        look.sweep = static_cast<int16_t>(360 * left / kCountdownMs);
    } else if (phase == Phase::kSettling) {
        look.opa = static_cast<lv_opa_t>(std::lround(255.0f * (1.0f - progress(now, s_round->phase_started(), kCountFadeMs))));
    }

    if (phase == Phase::kCounting && look.digit != s_ticked_digit) {
        sound::play_tick();  // each new second, and again whenever the countdown starts over
    }
    s_ticked_digit = phase == Phase::kCounting ? look.digit : 0;

    if (!(look == s_count)) {
        s_count = look;
        std::snprintf(s_digit_text, sizeof(s_digit_text), "%u", static_cast<unsigned>(look.digit));
        invalidate(circle_area(kCenterX, kCenterY, kCountR + 2));
    }
}

void draw_countdown(lv_layer_t *layer)
{
    if (s_count.opa == 0) {
        return;
    }
    lv_draw_rect_dsc_t disc;
    lv_draw_rect_dsc_init(&disc);
    disc.radius = LV_RADIUS_CIRCLE;
    disc.bg_color = lv_color_hex(theme::kSurface);
    disc.bg_opa = s_count.opa;
    lv_area_t area = circle_area(kCenterX, kCenterY, kCountR);
    lv_draw_rect(layer, &disc, &area);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center = {kCenterX, kCenterY};
    arc.radius = kCountR;
    arc.width = kCountRingW;
    arc.opa = s_count.opa;
    arc.color = lv_color_hex(theme::kSurfaceHigh);
    arc.start_angle = 0;
    arc.end_angle = 360;
    lv_draw_arc(layer, &arc);
    if (s_count.sweep > 0) {  // what's left of the five seconds, from the top, clockwise
        arc.color = lv_color_hex(theme::kText);
        arc.start_angle = 270;
        arc.end_angle = 270 + s_count.sweep;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
    }

    if (s_count.digit > 0) {
        const lv_font_t *font = ui::font_display();
        int32_t line_h = lv_font_get_line_height(font);
        lv_draw_label_dsc_t label;
        lv_draw_label_dsc_init(&label);
        label.font = font;
        label.color = lv_color_hex(theme::kText);
        label.opa = s_count.opa;
        label.align = LV_TEXT_ALIGN_CENTER;
        label.text = s_digit_text;
        lv_area_t text_area = {kCenterX - kCountR, kCenterY - line_h / 2, kCenterX + kCountR,
                               kCenterY - line_h / 2 + line_h - 1};
        lv_draw_label(layer, &label, &text_area);
    }
}

// ---- Text ----

void exec_opa(void *obj, int32_t value)
{
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value), 0);
}

void refresh_text()
{
    const char *hint = "";
    bool title = false;
    switch (s_round->phase()) {
    case Phase::kWaiting:
        title = s_round->count() == 0;
        hint = title ? "One finger will be picked at random" : "Waiting for more fingers...";
        break;
    case Phase::kCounting:
    case Phase::kSettling:
        hint = "Keep holding...";
        break;
    case Phase::kPicked:
        hint = s_round->ready_for_next() ? "Touch the screen to pick again" : "Lift your fingers, then touch to go again";
        break;
    }
    if (hint != s_hint_text) {
        lv_label_set_text_static(s_hint, hint);
        s_hint_text = hint;
    }
    if (title != s_title_shown) {
        ui::animate(s_title, exec_opa, title ? LV_OPA_TRANSP : LV_OPA_COVER, title ? LV_OPA_COVER : LV_OPA_TRANSP,
                    kTextFadeMs, lv_anim_path_ease_out);
        s_title_shown = title;
    }
}

// ---- Frame loop ----

void on_phase_change(Phase before, Phase after, uint32_t now)
{
    if (before == Phase::kPicked) {
        release_all_spots(now);  // a new round: the last one's lights fade out
    }
    if (after == Phase::kSettling) {
        send_streaks_to_winner(now);
    } else if (after == Phase::kPicked) {
        sound::play_chime();
    } else if (after != Phase::kCounting) {
        stop_all_streaks();
    }
}

void on_frame(lv_timer_t *)
{
    uint32_t now = lv_tick_get();
    TouchPoint touches[kMaxFingers];
    size_t touched = read_touch_points(touches, kMaxFingers);
    size_t count = 0;
    for (size_t i = 0; i < touched; ++i) {
        if (touches[i].y >= kAreaY) {
            touches[count++] = touches[i];
        }
    }

    Phase before = s_round->phase();
    s_round->update(touches, count, now);
    if (s_round->phase() != before) {
        on_phase_change(before, s_round->phase(), now);
    }

    sync_spots(now);
    if (s_round->phase() == Phase::kCounting) {
        balance_streaks(now);
    }
    for (Streak &streak : s_streaks) {
        update_streak(streak, now);
    }
    refresh_spots(now);
    refresh_countdown(now);
    refresh_text();
}

void on_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    draw_countdown(layer);
    const Spot *winner = nullptr;
    for (const Spot &spot : s_spots) {
        if (!spot.used) {
            continue;
        }
        if (is_winner(spot)) {
            winner = &spot;  // drawn last, on top
        } else {
            draw_spot(layer, spot);
        }
    }
    if (winner != nullptr) {
        draw_spot(layer, *winner);
    }
    for (const Streak &streak : s_streaks) {
        draw_streak(layer, streak);
    }
}

// Every visit starts a fresh round, and nothing animates while the screen is away.
void clear_round()
{
    s_round->reset();
    for (Spot &spot : s_spots) {
        spot = {};
    }
    for (Streak &streak : s_streaks) {
        streak = {};
    }
    s_count = {};
    s_ticked_digit = 0;
    lv_obj_invalidate(s_area);
}

void on_screen_load(lv_event_t *)
{
    clear_round();
    refresh_text();
    lv_timer_resume(s_timer);
}

void on_screen_unload(lv_event_t *)
{
    lv_timer_pause(s_timer);
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
    lv_obj_t *title = ui::header_button_create(bar, 0, "FINGER PICKER", nullptr, nullptr);
    lv_obj_set_style_text_color(title, lv_color_hex(theme::kText), 0);
    ui::header_button_create(bar, kHeaderIconW, nullptr, nullptr, nullptr);  // balances the waffle
}

}  // namespace

void ui_create(lv_obj_t *screen, Round &round)
{
    s_round = &round;
    build_header(screen);

    // The text sits under the drawing surface, so lit fingers cover it.
    s_title = lv_label_create(screen);
    lv_label_set_text(s_title, "EVERYONE PUT A FINGER\nON THE SCREEN");
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, kCenterY - theme::kScreenH / 2);

    s_hint = lv_label_create(screen);
    lv_obj_set_width(s_hint, theme::kScreenW - 2 * kHintPadX);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(theme::kMutedText), 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -kHintBottom);

    s_area = theme::make_box(screen);
    lv_obj_set_size(s_area, theme::kScreenW, kAreaH);
    lv_obj_set_pos(s_area, 0, kAreaY);
    lv_obj_remove_flag(s_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_area, on_draw, LV_EVENT_DRAW_MAIN, nullptr);

    s_timer = lv_timer_create(on_frame, kFrameMs, nullptr);
    lv_timer_pause(s_timer);
    lv_obj_add_event_cb(screen, on_screen_load, LV_EVENT_SCREEN_LOAD_START, nullptr);
    lv_obj_add_event_cb(screen, on_screen_unload, LV_EVENT_SCREEN_UNLOAD_START, nullptr);
    multitouch_available();
    refresh_text();
}

}  // namespace picker
