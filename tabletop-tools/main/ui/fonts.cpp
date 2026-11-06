#include "ui/fonts.h"

#include <cstdint>

#include "esp_log.h"

// Provided by EMBED_FILES in main/CMakeLists.txt.
extern const uint8_t kMontserratTtfStart[] asm("_binary_Montserrat_Medium_ttf_start");
extern const uint8_t kMontserratTtfEnd[] asm("_binary_Montserrat_Medium_ttf_end");

namespace ui {
namespace {

const char *TAG = "fonts";
constexpr int32_t kDisplaySize = 96;

lv_font_t *create_display_font()
{
    lv_font_t *font = lv_tiny_ttf_create_data(kMontserratTtfStart, kMontserratTtfEnd - kMontserratTtfStart,
                                              kDisplaySize);
    if (font != nullptr) {
        ESP_LOGI(TAG, "display font ready (%d px, line height %d)", static_cast<int>(kDisplaySize),
                 static_cast<int>(font->line_height));
    } else {
        ESP_LOGW(TAG, "display font unavailable; using the 48 px built-in font");
    }
    return font;
}

}  // namespace

const lv_font_t *font_display()
{
    static lv_font_t *font = create_display_font();
    return font != nullptr ? font : &lv_font_montserrat_48;
}

}  // namespace ui
