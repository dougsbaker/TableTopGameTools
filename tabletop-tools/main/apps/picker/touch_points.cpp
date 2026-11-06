#include "apps/picker/touch_points.h"

#include "bsp/esp-bsp.h"
#include "services/touch_controller.h"

namespace picker {

bool multitouch_available()
{
    return touch_controller::handle() != nullptr;
}

size_t read_touch_points(TouchPoint *out, size_t max)
{
    esp_lcd_touch_handle_t tp = touch_controller::handle();
    if (tp == nullptr) {
        lv_indev_t *indev = bsp_display_get_input_dev();
        if (max == 0 || indev == nullptr || lv_indev_get_state(indev) != LV_INDEV_STATE_PRESSED) {
            return 0;
        }
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        out[0] = {0, static_cast<int16_t>(point.x), static_cast<int16_t>(point.y)};
        return 1;
    }

    // The panel isn't mirrored or rotated, so the controller's coordinates are screen coordinates.
    // The controller keeps the last set until it reports a new one, and reports none once every
    // finger has lifted.
    size_t count = 0;
    portENTER_CRITICAL(&tp->data.lock);
    for (uint8_t i = 0; i < tp->data.points && count < max; ++i) {
        const esp_lcd_touch_point_data_t &point = tp->data.coords[i];
        out[count++] = {point.track_id, static_cast<int16_t>(point.x), static_cast<int16_t>(point.y)};
    }
    portEXIT_CRITICAL(&tp->data.lock);
    return count;
}

}  // namespace picker
