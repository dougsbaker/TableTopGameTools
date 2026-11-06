#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs_flash.h"

#include "apps/calculator/calculator.h"
#include "apps/calculator/calculator_ui.h"
#include "apps/dice/dice.h"
#include "apps/dice/dice_ui.h"
#include "apps/launcher/launcher.h"
#include "apps/picker/picker.h"
#include "apps/picker/picker_ui.h"
#include "apps/scorekeeper/scorekeeper.h"
#include "apps/scorekeeper/scorekeeper_storage.h"
#include "apps/scorekeeper/scorekeeper_ui.h"
#include "apps/settings/settings_ui.h"
#include "apps/timer/countdown.h"
#include "apps/timer/timer_ui.h"
#include "services/power.h"
#include "services/settings.h"
#include "services/sound.h"
#include "services/storage.h"
#include "ui/battery_status.h"
#include "ui/navigation.h"
#include "ui/side_buttons.h"
#include "ui/theme.h"

static const char *TAG = "tabletop";

// App models live for the whole run, independent of which screen is showing.
static scorekeeper::ScoreKeeper s_scorekeeper;
static calculator::Tally s_tally;
static timer::Countdown s_countdown;
static dice::DiceSet s_dice;
static picker::Round s_round;

// Returns false if NVS can't be used; the app then runs without saving.
static bool init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erasing (%s)", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s); nothing will be saved", esp_err_to_name(err));
        return false;
    }
    return true;
}

// The BSP lets LVGL's draw buffer land in PSRAM, which makes every redraw several times slower.
// Drawing happens in strips this tall, in internal RAM; the PSRAM buffer is simply left unused.
constexpr int32_t kDrawBufferLines = 48;

static void use_internal_draw_buffer(lv_display_t *display)
{
    uint32_t size = lv_display_get_horizontal_resolution(display) * kDrawBufferLines *
                    lv_color_format_get_size(lv_display_get_color_format(display));
    void *buffer = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buffer == nullptr) {
        ESP_LOGW(TAG, "no internal RAM for the draw buffer; drawing stays in PSRAM");
        return;
    }
    lv_display_set_buffers(display, buffer, nullptr, size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

// Touch feedback: sound::play_click() does nothing unless TAP CLICKS is on.
static void on_touch(lv_event_t *)
{
    sound::play_click();
}

extern "C" void app_main(void)
{
    bool nvs_ok = init_nvs();
    if (nvs_ok) {
        storage::start();
    }
    if (!nvs_ok || !scorekeeper::storage_load(s_scorekeeper)) {
        s_scorekeeper.load_defaults();
    }

    // Waveshare BSP: ST7701 RGB panel, GT911 touch, TCA9554 expander and the LVGL task. The panel
    // is set up from here, on core 0, so its refresh interrupts land there; LVGL gets core 1.
    bsp_display_cfg_t display_cfg = {};
    display_cfg.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    display_cfg.lvgl_port_cfg.task_affinity = 1;
    lv_display_t *display = bsp_display_start_with_config(&display_cfg);
    if (display == nullptr) {
        ESP_LOGE(TAG, "display init failed");
        return;
    }
    bsp_display_backlight_on();
    sound::init();
    power::init();

    bsp_display_lock(0);
    use_internal_draw_buffer(display);
    theme::init();
    settings::load_and_apply();
    nav::register_screen(nav::Screen::kHome, launcher::ui_create);
    nav::register_screen(nav::Screen::kScoreKeeper,
                         [](lv_obj_t *screen) { scorekeeper::ui_create(screen, s_scorekeeper); });
    nav::register_screen(nav::Screen::kCalculator,
                         [](lv_obj_t *screen) { calculator::ui_create(screen, s_tally); });
    nav::register_screen(nav::Screen::kTimer, [](lv_obj_t *screen) { timer::ui_create(screen, s_countdown); });
    nav::register_screen(nav::Screen::kSettings, settings_app::ui_create);
    nav::register_screen(nav::Screen::kDice, [](lv_obj_t *screen) { dice::ui_create(screen, s_dice); });
    nav::register_screen(nav::Screen::kPicker, [](lv_obj_t *screen) { picker::ui_create(screen, s_round); });
    nav::build_all();
    nav::show(nav::Screen::kHome);
    lv_indev_add_event_cb(bsp_display_get_input_dev(), on_touch, LV_EVENT_PRESSED, nullptr);
    ui::battery_alerts_start();
    ui::side_buttons_start([] { scorekeeper::storage_save(s_scorekeeper); });
    bsp_display_unlock();

    ESP_LOGI(TAG, "ready: %u players, free internal %u B, free PSRAM %u B",
             static_cast<unsigned>(s_scorekeeper.count()),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}
