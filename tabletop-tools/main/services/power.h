#pragma once

#include <cstdint>

// Battery and USB status from the board's AXP2101 power-management chip, read with the
// XPowersLib driver from Waveshare's 01_AXP2101 example. Readings are refreshed every few
// seconds on a background task; status() just returns the latest copy.
namespace power {

struct Status {
    bool available;        // the AXP2101 answered on I2C
    bool usb_present;      // VBUS (USB power) connected
    bool battery_present;
    bool charging;
    int percent;           // battery level, or -1 if unknown / no battery
    uint16_t battery_mv;
    uint16_t usb_mv;
};

// Call after the BSP has started I2C (bsp_display_start does). Applies Waveshare's charger
// settings for this board and starts polling. Returns false if the chip doesn't answer.
bool init();

Status status();

// True once for each short press of the PWR button since the last call (checked every 50 ms).
bool take_key_press();

// True once each time PWR has been held down for 1.5 s, while it's still held. Holding on to
// 6 s still makes the power chip cut the power by itself.
bool take_key_hold();

// True once PWR has been let go since the hold last reported by take_key_hold().
bool take_key_release();

// Switches every power rail off, as a 6 s hold would; a press of PWR turns the board back on,
// which starts the firmware afresh. Returns false if the power chip couldn't be told. Nothing is
// saved first: flush storage before calling.
bool shutdown();

}  // namespace power
