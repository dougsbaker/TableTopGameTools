#include "ui/battery_status.h"

#include <cstdio>

#include "ui/components/toast.h"
#include "ui/theme.h"

namespace ui {
namespace {

constexpr int kCriticalPercent = 5;
constexpr int kRearmPercent = 20;  // alerts can fire again once the battery has recovered
constexpr uint32_t kCheckMs = 5000;

bool s_warned_low = false;
bool s_warned_critical = false;

const char *battery_symbol(int percent)
{
    if (percent >= 90) {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (percent >= 60) {
        return LV_SYMBOL_BATTERY_3;
    }
    if (percent >= 35) {
        return LV_SYMBOL_BATTERY_2;
    }
    if (percent >= 10) {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

void on_check(lv_timer_t *)
{
    power::Status status = power::status();
    if (!status.battery_present || status.percent < 0) {
        return;
    }
    if (status.charging || status.percent >= kRearmPercent) {
        s_warned_low = false;
        s_warned_critical = false;
        return;
    }
    char text[48];
    if (status.percent <= kCriticalPercent && !s_warned_critical) {
        s_warned_critical = true;
        s_warned_low = true;
        std::snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_EMPTY "  BATTERY %d%% - PLUG IN SOON", status.percent);
        toast_show(text, lv_color_hex(theme::kDanger));
    } else if (status.percent <= kLowBatteryPercent && !s_warned_low) {
        s_warned_low = true;
        std::snprintf(text, sizeof(text), LV_SYMBOL_BATTERY_1 "  BATTERY LOW - %d%%", status.percent);
        toast_show(text, lv_color_hex(theme::kDanger));
    }
}

}  // namespace

void battery_indicator_text(const power::Status &status, char *out, size_t size)
{
    if (!status.available) {
        std::snprintf(out, size, "%s", "");
    } else if (status.battery_present && status.percent >= 0) {
        std::snprintf(out, size, "%s %d%%", status.charging ? LV_SYMBOL_CHARGE : battery_symbol(status.percent),
                      status.percent);
    } else if (status.usb_present) {
        std::snprintf(out, size, "%s", LV_SYMBOL_USB " USB");
    } else {
        std::snprintf(out, size, "%s", "");
    }
}

bool battery_low(const power::Status &status)
{
    return status.battery_present && !status.charging && status.percent >= 0 &&
           status.percent <= kLowBatteryPercent;
}

void battery_alerts_start()
{
    lv_timer_create(on_check, kCheckMs, nullptr);
}

}  // namespace ui
