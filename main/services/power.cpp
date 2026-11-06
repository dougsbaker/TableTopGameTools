#include "services/power.h"

#include <atomic>
#include <cstdlib>
#include <cstring>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

namespace power {
namespace {

const char *TAG = "power";

constexpr uint32_t kI2cHz = 400000;
constexpr int kI2cTimeoutMs = 100;
constexpr uint32_t kPollMs = 3000;
constexpr uint32_t kKeyPollMs = 50;
constexpr uint32_t kFirstReadingMs = 100;
// PWR key flags in INTSTS2.
constexpr uint8_t kShortPressBit = XPOWERS_AXP2101_PKEY_SHORT_IRQ >> 8;
constexpr uint8_t kLongPressBit = XPOWERS_AXP2101_PKEY_LONG_IRQ >> 8;
constexpr uint8_t kReleaseBit = XPOWERS_AXP2101_PKEY_POSITIVE_IRQ >> 8;  // the key line rising again
constexpr uint8_t kShutdownBit = 0x01;  // in COMMON_CONFIG
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 2;

XPowersPMU s_pmu;
i2c_master_dev_handle_t s_device = nullptr;
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
Status s_status = {};
std::atomic<bool> s_key_pressed{false};
std::atomic<bool> s_key_held{false};
std::atomic<bool> s_key_released{false};

// XPowersLib register callbacks, on the BSP's shared I2C bus (thread-safe per transaction).
int read_register(uint8_t, uint8_t reg, uint8_t *data, uint8_t len)
{
    esp_err_t err = i2c_master_transmit_receive(s_device, &reg, 1, data, len, kI2cTimeoutMs);
    return err == ESP_OK ? 0 : -1;
}

int write_register(uint8_t, uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t buffer[16];
    if (len + 1u > sizeof(buffer)) {
        return -1;
    }
    buffer[0] = reg;
    std::memcpy(&buffer[1], data, len);
    esp_err_t err = i2c_master_transmit(s_device, buffer, len + 1, kI2cTimeoutMs);
    return err == ESP_OK ? 0 : -1;
}

// Same measurement and charger setup as Waveshare's pmu_init() for this board. Power rails
// are left exactly as they are.
void configure()
{
    s_pmu.enableVbusVoltageMeasure();
    s_pmu.enableBattVoltageMeasure();
    s_pmu.enableSystemVoltageMeasure();
    // The board has no battery temperature sensor; leaving TS detection on breaks charging.
    s_pmu.disableTSPinMeasure();
    s_pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    s_pmu.clearIrqStatus();
    // The PWR button is wired to the chip, which flags each short press (as Waveshare's demo does),
    // a hold of 1.5 s, and the button being let go.
    s_pmu.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_1S5);
    s_pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |
                    XPOWERS_AXP2101_PKEY_POSITIVE_IRQ);
    s_pmu.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    s_pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
    s_pmu.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    s_pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
}

// What a very long hold of PWR does is up to the chip's own settings, which are left as they are.
void log_key_setup()
{
    static const char *const kOffHold[] = {"4", "6", "8", "10"};
    static const char *const kOnHold[] = {"0.128", "0.512", "1", "2"};
    static const char *const kLongPress[] = {"1", "1.5", "2", "2.5"};
    uint8_t off_ctrl = 0;
    uint8_t levels = 0;
    if (read_register(0, XPOWERS_AXP2101_PWROFF_EN, &off_ctrl, 1) != 0 ||
        read_register(0, XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, &levels, 1) != 0) {
        return;
    }
    const char *hold_action = (off_ctrl & 0x02) == 0 ? "nothing" : (off_ctrl & 0x01) ? "restart" : "power off";
    ESP_LOGI(TAG, "PWR key: hold %s s to turn on; long press at %s s; hold %s s to %s", kOnHold[levels & 0x03],
             kLongPress[(levels >> 4) & 0x03], kOffHold[(levels >> 2) & 0x03], hold_action);
}

// One register read per check. A failed read is ignored rather than taken as a press.
void poll_key()
{
    uint8_t flags = 0;
    if (read_register(0, XPOWERS_AXP2101_INTSTS2, &flags, 1) != 0 || flags == 0) {
        return;
    }
    write_register(0, XPOWERS_AXP2101_INTSTS2, &flags, 1);  // write 1 to clear the flags just seen
    if (flags & kShortPressBit) {
        s_key_pressed.store(true);
    }
    if (flags & kLongPressBit) {
        // The hold flag comes while the key is still down, so a release seen alongside it came
        // after it, and any release seen before belonged to an earlier press.
        s_key_held.store(true);
        s_key_released.store((flags & kReleaseBit) != 0);
    } else if (flags & kReleaseBit) {
        s_key_released.store(true);
    }
}

void poll()
{
    Status next = {};
    next.available = true;
    next.usb_present = s_pmu.isVbusIn();
    next.battery_present = s_pmu.isBatteryConnect();
    next.charging = next.battery_present && s_pmu.isCharging();
    next.percent = next.battery_present ? s_pmu.getBatteryPercent() : -1;
    next.battery_mv = s_pmu.getBattVoltage();
    next.usb_mv = s_pmu.getVbusVoltage();
    taskENTER_CRITICAL(&s_lock);
    s_status = next;
    taskEXIT_CRITICAL(&s_lock);
}

void power_task(void *)
{
    TickType_t last_poll = xTaskGetTickCount();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kKeyPollMs));
        poll_key();
        if (xTaskGetTickCount() - last_poll >= pdMS_TO_TICKS(kPollMs)) {
            poll();
            last_poll = xTaskGetTickCount();
        }
    }
}

}  // namespace

bool init()
{
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = AXP2101_SLAVE_ADDRESS;
    config.scl_speed_hz = kI2cHz;
    if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &s_device) != ESP_OK) {
        ESP_LOGW(TAG, "couldn't add the AXP2101 to the I2C bus");
        return false;
    }
    if (!s_pmu.begin(AXP2101_SLAVE_ADDRESS, read_register, write_register)) {
        ESP_LOGW(TAG, "AXP2101 not found; battery status unavailable");
        return false;
    }
    configure();
    vTaskDelay(pdMS_TO_TICKS(kFirstReadingMs));  // the ADC needs a moment after measurements are enabled
    poll();
    Status now = status();
    ESP_LOGI(TAG, "AXP2101 ready: usb %s (%u mV), battery %s (%u mV, %d%%)", now.usb_present ? "yes" : "no",
             static_cast<unsigned>(now.usb_mv), now.battery_present ? "yes" : "no",
             static_cast<unsigned>(now.battery_mv), now.percent);
    log_key_setup();
    xTaskCreate(power_task, "power", kTaskStack, nullptr, kTaskPriority, nullptr);
    return true;
}

Status status()
{
    taskENTER_CRITICAL(&s_lock);
    Status copy = s_status;
    taskEXIT_CRITICAL(&s_lock);
    return copy;
}

bool take_key_press()
{
    return s_key_pressed.exchange(false);
}

bool take_key_hold()
{
    return s_key_held.exchange(false);
}

bool take_key_release()
{
    return s_key_released.exchange(false);
}

bool shutdown()
{
    uint8_t config = 0;
    if (!status().available || read_register(0, XPOWERS_AXP2101_COMMON_CONFIG, &config, 1) != 0) {
        ESP_LOGW(TAG, "can't power off: AXP2101 not answering");
        return false;
    }
    ESP_LOGI(TAG, "powering off");
    config |= kShutdownBit;
    return write_register(0, XPOWERS_AXP2101_COMMON_CONFIG, &config, 1) == 0;
}

}  // namespace power
