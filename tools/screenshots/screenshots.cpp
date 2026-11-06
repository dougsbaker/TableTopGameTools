// TEMPORARY: walks through the apps with scripted touches and streams screenshots over USB.
#include "diag/screenshots.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "services/touch_controller.h"
#include "ui/navigation.h"

namespace diag {
namespace {

const char *TAG = "shots";
constexpr int kW = BSP_LCD_H_RES;
constexpr int kH = BSP_LCD_V_RES;
constexpr size_t kFrameBytes = kW * kH * 2;

struct PortDisplayContext {  // leading members of esp_lvgl_port's lvgl_port_display_ctx_t
    int disp_type;
    void *io_handle;
    esp_lcd_panel_handle_t panel_handle;
};

uint16_t *s_fb = nullptr;
uint16_t *s_copy = nullptr;
uint8_t *s_packed = nullptr;
esp_lcd_touch_handle_t s_tp = nullptr;

esp_err_t no_read(esp_lcd_touch_handle_t)
{
    return ESP_OK;
}

void wait(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

struct Pt {
    int16_t x, y;
};

void set_fingers(const Pt *pts, uint8_t n)
{
    portENTER_CRITICAL(&s_tp->data.lock);
    s_tp->data.points = n;
    for (uint8_t i = 0; i < n; ++i) {
        s_tp->data.coords[i].x = pts[i].x;
        s_tp->data.coords[i].y = pts[i].y;
        s_tp->data.coords[i].track_id = i + 1;
        s_tp->data.coords[i].strength = 40;
    }
    portEXIT_CRITICAL(&s_tp->data.lock);
}

void tap(int16_t x, int16_t y, uint32_t down_ms = 90, uint32_t after_ms = 160)
{
    Pt p = {x, y};
    set_fingers(&p, 1);
    wait(down_ms);
    set_fingers(nullptr, 0);
    wait(after_ms);
}

void go(nav::Screen s, uint32_t settle_ms = 900)
{
    bsp_display_lock(0);
    nav::show(s);
    bsp_display_unlock();
    wait(settle_ms);
}

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void emit_line(const uint8_t *data, size_t len)
{
    static char line[1100];
    size_t o = 0;
    line[o++] = '@';
    line[o++] = '@';
    line[o++] = 'D';
    line[o++] = ' ';
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = data[i] << 16;
        if (i + 1 < len) v |= data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        line[o++] = kB64[(v >> 18) & 63];
        line[o++] = kB64[(v >> 12) & 63];
        line[o++] = i + 1 < len ? kB64[(v >> 6) & 63] : '=';
        line[o++] = i + 2 < len ? kB64[v & 63] : '=';
    }
    line[o++] = '\n';
    fwrite(line, 1, o, stdout);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(2));  // lets USB keep up, so no line is dropped
}

// PackBits over 16-bit pixels: n < 128 -> n+1 literal pixels; n >= 128 -> next pixel n-125 times.
size_t pack(const uint16_t *px, size_t count, uint8_t *out)
{
    size_t o = 0;
    size_t i = 0;
    while (i < count) {
        size_t run = 1;
        while (i + run < count && px[i + run] == px[i] && run < 130) ++run;
        if (run >= 3) {
            out[o++] = static_cast<uint8_t>(run + 125);
            out[o++] = px[i] & 0xFF;
            out[o++] = px[i] >> 8;
            i += run;
            continue;
        }
        size_t start = i;
        size_t lit = 0;
        while (i < count && lit < 128) {
            if (i + 2 < count && px[i] == px[i + 1] && px[i] == px[i + 2]) break;
            ++i;
            ++lit;
        }
        out[o++] = static_cast<uint8_t>(lit - 1);
        for (size_t k = start; k < start + lit; ++k) {
            out[o++] = px[k] & 0xFF;
            out[o++] = px[k] >> 8;
        }
    }
    return o;
}

void shot(const char *name)
{
    bsp_display_lock(0);
    lv_refr_now(nullptr);
    memcpy(s_copy, s_fb, kFrameBytes);
    bsp_display_unlock();

    size_t n = pack(s_copy, kW * kH, s_packed);
    uint32_t crc = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t *>(s_copy), kFrameBytes);
    for (int copy = 0; copy < 2; ++copy) {  // twice, in case a line is lost anyway
        printf("@@SHOT %s %d %d\n", name, kW, kH);
        for (size_t i = 0; i < n; i += 768) {
            emit_line(s_packed + i, n - i < 768 ? n - i : 768);
        }
        printf("@@END %s %u %08lx\n", name, static_cast<unsigned>(n), static_cast<unsigned long>(crc));
        fflush(stdout);
    }
}

// Drags around the timer ring, 12 o'clock = 0 degrees, clockwise.
void drag_ring(float from_deg, float to_deg)
{
    constexpr float kCx = 240, kCy = 228, kR = 150, kRad = 3.14159265f / 180;
    for (float a = from_deg; a <= to_deg; a += 5) {
        Pt p = {static_cast<int16_t>(kCx + kR * std::sin(a * kRad)), static_cast<int16_t>(kCy - kR * std::cos(a * kRad))};
        set_fingers(&p, 1);
        wait(25);
    }
    set_fingers(nullptr, 0);
    wait(200);
}

void run_script()
{
    go(nav::Screen::kHome);
    shot("home");

    go(nav::Screen::kScoreKeeper);
    for (int i = 0; i < 5; ++i) tap(442, 320);  // third row +5
    for (int i = 0; i < 2; ++i) tap(363, 427);  // bottom row -2
    shot("scorekeeper");
    wait(4500);  // chips added, rows re-sorted
    shot("scorekeeper_sorted");
    tap(80, 320);  // the third row's name opens the editor
    wait(900);
    shot("player_editor");

    go(nav::Screen::kCalculator);
    const Pt keys[] = {{60, 389}, {180, 389}, {420, 420},   // 1 2 +
                       {180, 389}, {60, 327}, {420, 420},   // 2 4 +
                       {60, 265}, {420, 420},               // 7 +
                       {60, 389}, {180, 327}, {420, 420}};  // 1 5 +
    for (const Pt &k : keys) tap(k.x, k.y);
    wait(900);
    shot("calculator");

    go(nav::Screen::kTimer);
    drag_ring(6, 216);  // 3:00
    shot("timer_set");
    tap(240, 447);  // START
    wait(2300);
    shot("timer");

    go(nav::Screen::kDice);
    tap(319, 105);  // d6
    tap(319, 105);  // d6
    tap(398, 242);  // d8
    tap(82, 242);   // d20
    shot("dice_chosen");
    tap(240, 242);  // ROLL
    wait(1800);
    shot("dice");

    go(nav::Screen::kPicker);
    const Pt fingers[] = {{130, 160}, {350, 140}, {115, 365}, {355, 375}};
    set_fingers(fingers, 4);
    wait(2200);
    shot("picker_countdown");
    wait(5200);
    shot("picker_chosen");
    set_fingers(nullptr, 0);
    wait(1500);

    go(nav::Screen::kSettings);
    shot("settings");
}

void task(void *)
{
    wait(1500);
    esp_log_level_set("*", ESP_LOG_WARN);
    run_script();
    printf("@@DONE\n");
    fflush(stdout);
    vTaskDelete(nullptr);
}

}  // namespace

void screenshots_start()
{
    lv_display_t *display = lv_display_get_default();
    auto *ctx = static_cast<PortDisplayContext *>(lv_display_get_driver_data(display));
    void *fb = nullptr;
    if (ctx == nullptr || esp_lcd_rgb_panel_get_frame_buffer(ctx->panel_handle, 1, &fb) != ESP_OK || fb == nullptr) {
        ESP_LOGE(TAG, "no frame buffer");
        return;
    }
    s_fb = static_cast<uint16_t *>(fb);
    s_copy = static_cast<uint16_t *>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM));
    s_packed = static_cast<uint8_t *>(heap_caps_malloc(kFrameBytes * 2, MALLOC_CAP_SPIRAM));
    s_tp = touch_controller::handle();
    if (s_copy == nullptr || s_packed == nullptr || s_tp == nullptr) {
        ESP_LOGE(TAG, "setup failed");
        return;
    }
    s_tp->read_data = no_read;  // real touches are ignored; the script supplies them
    set_fingers(nullptr, 0);
    xTaskCreatePinnedToCore(task, "shots", 6144, nullptr, 2, nullptr, 0);
}

}  // namespace diag
