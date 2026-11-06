#include "services/settings.h"

#include <cstring>

#include "esp_log.h"
#include "services/screen_power.h"
#include "services/sound.h"
#include "services/storage.h"

namespace settings {
namespace {

const char *TAG = "settings";

constexpr const char *kNamespace = "settings";
constexpr const char *kKey = "device";
constexpr uint16_t kSchemaVersion = 1;
constexpr uint8_t kMinBrightness = 10;

// On-flash layout, without implicit padding.
struct Stored {
    uint16_t version;
    uint8_t brightness;
    uint8_t dim_index;
    uint8_t volume;
    uint8_t alarm_sound;
    uint8_t tap_clicks;
    uint8_t touch;  // was reserved and always 0, which is kNormal
};
static_assert(sizeof(Stored) == 8, "Stored must not contain padding");

// Touch defaults to High: on Normal, the factory setting, taps go missing on battery.
constexpr Values kDefaults = {100, 2, 80, true, false, touch_controller::Sensitivity::kHigh};

Values s_values = kDefaults;

uint8_t clamp_percent(uint8_t value, uint8_t min)
{
    return value < min ? min : value > 100 ? 100 : value;
}

void save()
{
    Stored stored = {};
    stored.version = kSchemaVersion;
    stored.brightness = s_values.brightness;
    stored.dim_index = s_values.dim_index;
    stored.volume = s_values.volume;
    stored.alarm_sound = s_values.alarm_sound;
    stored.tap_clicks = s_values.tap_clicks;
    stored.touch = static_cast<uint8_t>(s_values.touch);
    storage::save_blob(kNamespace, kKey, &stored, sizeof(stored));
}

void load()
{
    Stored stored = {};
    size_t size = sizeof(stored);
    esp_err_t err = storage::load_blob(kNamespace, kKey, &stored, &size);
    if (err != ESP_OK || size != sizeof(stored) || stored.version != kSchemaVersion) {
        ESP_LOGI(TAG, "using default settings");
        s_values = kDefaults;
        return;
    }
    s_values.brightness = clamp_percent(stored.brightness, kMinBrightness);
    s_values.dim_index = stored.dim_index < kDimOptionCount ? stored.dim_index : kDefaults.dim_index;
    s_values.volume = clamp_percent(stored.volume, 0);
    s_values.alarm_sound = stored.alarm_sound != 0;
    s_values.tap_clicks = stored.tap_clicks != 0;
    s_values.touch = stored.touch < touch_controller::kSensitivityCount
                         ? static_cast<touch_controller::Sensitivity>(stored.touch)
                         : kDefaults.touch;
    ESP_LOGI(TAG, "loaded: brightness %u%%, volume %u%%, tap clicks %s, touch sensitivity %u",
             static_cast<unsigned>(s_values.brightness), static_cast<unsigned>(s_values.volume),
             s_values.tap_clicks ? "on" : "off", static_cast<unsigned>(s_values.touch));
}

}  // namespace

void load_and_apply()
{
    load();
    screen_power::init(s_values.brightness, kDimAfterMs[s_values.dim_index]);
    sound::set_volume(s_values.volume);
    sound::set_clicks_enabled(s_values.tap_clicks);
    touch_controller::set_sensitivity(s_values.touch);
}

const Values &get()
{
    return s_values;
}

void set_brightness(uint8_t percent)
{
    s_values.brightness = clamp_percent(percent, kMinBrightness);
    screen_power::set_brightness(s_values.brightness);
    save();
}

void set_dim_index(uint8_t index)
{
    s_values.dim_index = index < kDimOptionCount ? index : kDefaults.dim_index;
    screen_power::set_dim_after(kDimAfterMs[s_values.dim_index]);
    save();
}

void set_volume(uint8_t percent)
{
    s_values.volume = clamp_percent(percent, 0);
    sound::set_volume(s_values.volume);
    save();
}

void set_alarm_sound(bool on)
{
    s_values.alarm_sound = on;
    if (!on) {
        sound::stop();
    }
    save();
}

void set_tap_clicks(bool on)
{
    s_values.tap_clicks = on;
    sound::set_clicks_enabled(on);
    save();
}

bool set_touch_sensitivity(touch_controller::Sensitivity level)
{
    if (!touch_controller::set_sensitivity(level)) {
        return false;
    }
    s_values.touch = level;
    save();
    return true;
}

}  // namespace settings
