#include "services/touch_controller.h"

#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace touch_controller {
namespace {

const char *TAG = "touch";

// esp_lvgl_port stores the touch handle as the first member of the context it keeps as the
// indev's driver data (lvgl_port_touch_ctx_t in esp_lvgl_port 2.x); the BSP doesn't expose the
// handle any other way. The resolution check below guards against that layout changing.
struct PortTouchContext {
    esp_lcd_touch_handle_t handle;
};

// GT911 configuration: 184 bytes from 0x8047, followed by their checksum and a flag that makes
// the controller load them.
constexpr int kConfigReg = 0x8047;
constexpr size_t kConfigSize = 184;
constexpr size_t kChecksumAt = kConfigSize;
constexpr size_t kLoadFlagAt = kConfigSize + 1;
constexpr size_t kXMaxAt = 0x8048 - kConfigReg;  // little-endian
constexpr size_t kYMaxAt = 0x804A - kConfigReg;
constexpr size_t kPressLevelAt = 0x8053 - kConfigReg;
constexpr size_t kReleaseLevelAt = 0x8054 - kConfigReg;
constexpr uint32_t kLoadMs = 100;  // time for the controller to take up a new configuration

struct Thresholds {
    uint8_t press;
    uint8_t release;
};

// Normal is this board's factory setting (config version 0x4F).
constexpr Thresholds kThresholds[kSensitivityCount] = {{80, 60}, {60, 40}, {45, 30}};

using Config = uint8_t[kConfigSize + 2];

uint8_t checksum(const Config config)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < kConfigSize; ++i) {
        sum += config[i];
    }
    return static_cast<uint8_t>(~sum + 1);
}

uint16_t read_u16(const Config config, size_t at)
{
    return static_cast<uint16_t>(config[at] | (config[at + 1] << 8));
}

// Reads the configuration and checks it against its checksum and the screen size, so a misread
// is never written back.
bool read_config(esp_lcd_touch_handle_t tp, Config config)
{
    if (esp_lcd_panel_io_rx_param(tp->io, kConfigReg, config, sizeof(Config)) != ESP_OK) {
        ESP_LOGW(TAG, "couldn't read the controller's configuration");
        return false;
    }
    if (config[kChecksumAt] != checksum(config) || read_u16(config, kXMaxAt) != BSP_LCD_H_RES ||
        read_u16(config, kYMaxAt) != BSP_LCD_V_RES) {
        ESP_LOGW(TAG, "controller configuration didn't read back intact (version 0x%02X, checksum 0x%02X)",
                 config[0], config[kChecksumAt]);
        return false;
    }
    return true;
}

}  // namespace

esp_lcd_touch_handle_t handle()
{
    static bool s_looked = false;
    static esp_lcd_touch_handle_t s_handle = nullptr;
    if (!s_looked) {
        s_looked = true;
        lv_indev_t *indev = bsp_display_get_input_dev();
        auto *context = indev != nullptr ? static_cast<PortTouchContext *>(lv_indev_get_driver_data(indev)) : nullptr;
        esp_lcd_touch_handle_t tp = context != nullptr ? context->handle : nullptr;
        if (tp != nullptr && tp->config.x_max == BSP_LCD_H_RES && tp->config.y_max == BSP_LCD_V_RES) {
            s_handle = tp;
            ESP_LOGI(TAG, "controller ready: up to %d fingers", CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
        } else {
            ESP_LOGW(TAG, "controller not found; one finger only, fixed sensitivity");
        }
    }
    return s_handle;
}

bool set_sensitivity(Sensitivity level)
{
    esp_lcd_touch_handle_t tp = handle();
    Config config;
    if (tp == nullptr || !read_config(tp, config)) {
        return false;
    }
    const Thresholds &want = kThresholds[static_cast<size_t>(level)];
    const Thresholds had = {config[kPressLevelAt], config[kReleaseLevelAt]};
    if (had.press == want.press && had.release == want.release) {
        ESP_LOGI(TAG, "thresholds: press %u, release %u", want.press, want.release);
        return true;
    }

    config[kPressLevelAt] = want.press;
    config[kReleaseLevelAt] = want.release;
    config[kChecksumAt] = checksum(config);
    config[kLoadFlagAt] = 1;
    if (esp_lcd_panel_io_tx_param(tp->io, kConfigReg, config, sizeof(Config)) != ESP_OK) {
        ESP_LOGW(TAG, "couldn't write the controller's configuration");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(kLoadMs));

    Config check = {};
    bool taken = read_config(tp, check) && check[kPressLevelAt] == want.press && check[kReleaseLevelAt] == want.release;
    ESP_LOGI(TAG, "thresholds: press %u -> %u, release %u -> %u: %s (load flag %u)", had.press, want.press,
             had.release, want.release, taken ? "taken" : "NOT taken", check[kLoadFlagAt]);
    return taken;
}

}  // namespace touch_controller
