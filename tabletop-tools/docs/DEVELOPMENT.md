# Tabletop Tools: developer guide

Standalone touchscreen firmware for the **Waveshare ESP32-S3-Touch-LCD-4B** (480×480 ST7701 RGB LCD, GT911 touch). The full specification is in [Goal.md](../../Claude/Goal.md).

This guide covers how every feature behaves in detail, how to build and flash, and the on-device test checklist. For an overview with screenshots and what to buy, see the [README](../README.md).

## Current state: home screen, Score Keeper, Calculator, Timer, Dice, Finger Picker and Settings

### Home screen
- The board boots to **TABLETOP TOOLS**, a 2×3 grid of app tiles with drawn icons.
  - **Score Keeper** and **Calculator** open their apps.
  - **Timer** opens the Timer.
  - **Settings** opens the device settings.
  - **Dice** opens the dice roller.
  - **Finger Picker** opens the finger picker.
- The header shows a **battery indicator** at the right. It reads "USB" when there's no battery, shows the level with a battery symbol, and shows a bolt while charging. It turns red at 15% or below.
- The **waffle** at the top left of every app goes back to the home screen. Screens slide left and right when you switch.
- Every screen is built once at boot and stays in memory. Switching apps never loses Score Keeper players, scores or open views, or the calculator's tape.

### Side buttons
- **PWR:** a short press turns the screen off, and another press or a touch turns it back on.
  - The screen stays off until you wake it, even while the timer runs. The timer's alarm still wakes it.
- **Power off:** hold PWR for 1.5 seconds. A POWERING OFF screen appears, and when you let go the board saves everything and switches fully off.
  - Off, the board draws almost nothing from the battery. A short press of PWR turns it back on, starting at the home screen with your scores and settings kept.
  - The board waits for you to let go because a button still held as the power goes could count as the press that turns it back on.
  - Holding PWR for 6 seconds still powers the board off. The power chip does this by itself whatever the firmware is doing, but it saves nothing, so a change from the last second or two can be lost.
- **BOOT:** goes to the home screen from any app and cancels an open confirmation.
  - Like a touch, a press on a dark screen only wakes it.
  - Don't hold BOOT while the board powers up or restarts, because that puts it into flashing mode.
- Both buttons click when TAP CLICKS is on.

### Timer
- **Readout:** big 96 px digits sit in the middle of a dial made of 60 orange ticks.
  - The readout shows only the characters the time needs: `14:25`, then `9:30`, then `60s` … `1s`.
- **Setting the time by dragging:** while the timer is stopped, drag around the dial.
  - A full turn is **5 minutes**, and each tick is 5 s. The white tick marks the end you're dragging.
  - The first touch jumps straight to your finger.
  - Dragging past 12 o'clock stops at 0 or 5:00. It doesn't wrap around.
- **SET TIME editor:** tap the time to open it.
  - **Minutes and seconds each have their own +** above the value and **−** below it. Hold to repeat. Minutes run 0–99, and seconds wrap 59 → 0 without changing the minutes.
  - The **presets** 30s, 45s, 1m, 2m, 3m, 5m, 10m and 15m are below. Tapping one fills in the time, and the matching preset lights up.
  - **DONE** applies the time and **CANCEL** discards it.
- **Buttons:** **▶ START** fills the dial, which then empties as time runs out.
  - Once it's running, the button becomes **❚❚ PAUSE** (then **▶ RESUME**), and **↻ RESET** slides in beside it.
  - RESET goes back to the set time.
- **When time is up:**
  - The dial flashes, the label reads TIME'S UP, and the speaker plays three beeps, five times over (about 5 s).
  - Tapping the dial, START or RESET silences it.
  - The 🔊 button in the header mutes the alarm. It's the same switch as SETTINGS > TIMER ALARM, and it's saved.
- **Screen:** while the timer is running, the screen may dim but never turns off. The alarm wakes it.
- **Background running:** the countdown uses the hardware clock, not screen refreshes. It keeps running, and the alarm still sounds, while you're on other screens.

### Dice
- **Layout:** a round **ROLL** button sits in the middle, ringed by six dice tiles: d4, d6, d8, d10, d12 and d20, ascending clockwise from the top left.
  - Each tile shows a line drawing of its die, number included, in the die's own color. A chosen die's tile fills with that color and the drawing turns white.
- **Choosing dice:** tap a tile to add one of that die. A count badge (`x2`) appears and the tile fills with its color.
  - Hold a tile to take dice away one at a time.
  - **CLEAR** in the header empties everything.
  - The button shows what will be thrown, for example `2d6 + d20`.
- **Rolling:** **ROLL** flicks through random totals, slowing down, then lands on the real total with a small pop.
  - Below the button, each die's result is listed, for example `d20: 14    2d6: 3+5`.
  - Tap the total to roll the same dice again. Changing the dice puts the button back to ROLL.
- **Randomness:** rolls use the ESP32's hardware random number generator, with rejection sampling so every face is equally likely.
- **Saving:** the chosen dice stay while you use other apps, but they aren't saved through a power loss.

### Finger Picker
- **Joining:** everyone puts a finger anywhere below the header. Each finger lights up in its own color, with a ring, a soft glow and a little pop.
  - Up to 5 fingers, which is the most the touch controller tracks.
- **Countdown:** once two or more fingers are down, a circle in the middle counts down from 5, with a ring that runs out and a soft tick each second.
  - Streaks of light bounce between the fingers. Each one leaves in the color of the finger it came from, and the finger it hits flashes.
  - The streaks speed up as time runs out.
  - If a finger joins or lifts, the countdown starts again from 5, so late fingers count.
- **The pick:** every streak flies to one finger, chosen at random. That finger grows, glows and chimes, and the others fade out.
  - The chosen finger stays lit, gently pulsing, even after everyone lets go.
  - Once every finger has lifted, the next touch starts a new round.
- **How it works:** LVGL itself follows only one finger, so the picker reads every finger straight from the GT911 touch controller.
  - It does that through the touch handle esp_lvgl_port keeps (see `services/touch_controller.cpp`). The boot log says `touch: controller ready: up to 5 fingers` when this works.

### Settings
- **DISPLAY**
  - **Brightness:** a 10-step bar you tap or drag.
  - **Auto-dim:** off, 1m, 2m, 5m or 10m.
  - After that time without a touch the backlight drops to 10%, and a minute later it turns off.
  - The first touch on a dark screen only wakes it and never presses the button underneath.
- **TOUCH**
  - **Sensitivity:** NORMAL, HIGH or MAX. It sets how firmly a finger has to register before the touch chip counts it.
  - NORMAL is the chip's factory setting: press threshold 80, release threshold 60. HIGH is 60/40, and MAX is 45/30.
  - **HIGH is the default.** On NORMAL, taps went missing on battery: unplugged and lying flat on a table, the board isn't grounded, so a finger reads weaker. HIGH fixed it on this board. Raise it to MAX if taps are still missed.
  - The thresholds are written into the touch chip's own configuration, only when they change. The firmware checks them at every boot.
  - The chip's original configuration is kept in [tools/touch/gt911_factory_config.txt](../tools/touch/gt911_factory_config.txt). Choosing NORMAL puts the factory thresholds back. RESET ALL DATA returns to HIGH.
- **SOUND**
  - **Volume:** a 10-step bar, with a **TEST** beep.
  - **Timer alarm:** on or off.
  - **Tap clicks:** a short tick on every touch, off by default. While clicks are on, the speaker amplifier stays powered so ticks play instantly.
- **BATTERY:** live status from the AXP2101 power chip, refreshed every few seconds.
  - Level, plus CHARGING, ON BATTERY, or NO BATTERY - RUNNING ON USB.
  - Battery and USB voltages.
- **SYSTEM**
  - The firmware, ESP-IDF and LVGL versions, and free memory.
  - **RESET ALL DATA** asks for confirmation, then erases players, scores and settings and restarts the device.
- **Saving:** every setting is saved to flash and survives power loss.
- **Low-battery alerts:** a banner slides down over any screen at 15% and again at 5%, unless the battery is charging.

### Calculator (tallying, for example domino pips)
- **Tape:** the top panel shows everything added so far, in order, for example `12 + 24 - 7 + 30`.
  - It wraps onto new lines and scrolls smoothly to the newest number.
  - You can drag it to look back at earlier numbers.
- **ENTRY** shows the number you're typing, up to 5 digits. **TOTAL**, in blue, counts up to the new sum each time you add.
- **Keypad:**
  - Digits, ⌫, and − (subtracts the entry, for corrections).
  - The tall blue **+** adds the entry to the tape.
  - **CLEAR** empties the tape and the total.
- **UNDO** in the header takes back the last number. Right after a CLEAR, it brings the whole tape back.
- The tape holds up to 99 numbers. It stays in memory while you use other apps, but it isn't saved through a power loss.

### Score Keeper
- It picks up where you left off, even after a power loss.
  - On the very first boot, or if the saved data can't be read, it starts with the default players at 0. The published defaults are **Alex, Sam, Jordan and Riley**. See [Your own default players](#your-own-default-players) to use your own names without publishing them.
- **Saved sessions:** players (names, colors, order), scores, the sort direction and the last reset value are all saved to flash (NVS).
  - Saving happens 1 s after the last change, so pulling the power loses at most the last second.
  - The write runs on a low-priority background task, and it's skipped when nothing changed, which keeps flash wear low.
  - Reflashing the firmware with `idf.py flash` keeps the saved session.
  - The saved data has a version number so future firmware can upgrade it. Data from an unknown version, or that fails validation, is ignored in favor of the defaults.
- Each player row has its own gradient and four columns: name, score, −, +.
- **Staged changes:** tap − or + to change a score by 1. The step lands the moment your finger touches down, not when it lifts. Hold either one to auto-repeat.
  - The change appears as a chip beside the score, such as `54 +6` or `35 -5`, and the score itself doesn't move yet.
  - The score and chip shrink to a smaller font when both wouldn't fit.
  - Once scoring pauses for 1.5 s, or when the sort button is pressed, every chip is added at once. The chips slide into their scores, the totals count to their new values, and then the rows slide into order.
  - Only those added totals are saved. Changes still staged when the power is lost are dropped.
- Scores are clamped to −99999…99999.
- **Edit a player:** tap a name to open the editor. It has a name preview, 8 colors to choose from, and an on-screen keyboard (A–Z, SPACE, backspace, CLEAR). Use CANCEL or SAVE to close it. REMOVE asks for confirmation first, and the last remaining player can't be removed.
- **Add a player:** tap **ADD** to open the same editor. A new player starts at 0, on the first color nobody is using. ADD turns gray once there are 8 players.
- **Auto-sort:** the sort button switches between **▼ 321** (highest score on top, the default) and **▲ 123** (lowest score on top). Rows are always sorted. Rows re-sort only when the staged changes are added, so a row never moves out from under your finger. Tied players keep the order they were added.
- **Reset:** **↻ RESET** opens a full-screen reset view.
  - The left side has a "−  value  +" stepper with ✕ (cancel) and ✓ (confirm) below it.
  - The right side previews every player at the chosen value.
  - Tap − or + to change the value by 1. Hold to repeat, and after about 2 s the steps jump by 10.
  - ✓ sets every score to that value and keeps the players and their colors. ✕ changes nothing.
  - The next reset starts from the last value you used.
- Rows resize to fit 1–8 players. At 6 or more players, the fonts get smaller.
- **Look:** flat, modern tiles on a near-black background, with no gradients and square corners.
  - Players get 8 saturated hues that all work with white text: blue, red, green, amber, violet, cyan, pink and lime.
  - The ± tiles are a darker shade of the row's color.
- **Motion:**
  - **Re-sorting:** rows slide to their new positions, taking 320 ms with ease-in-out.
  - **Adding or removing a player:** a new player's row slides in from the right, and a removed player's row slides out to the left. The other rows resize smoothly to fill the space.
  - **Editor and reset screens:** they slide up from the bottom to open and back down to close.
  - **Confirmation pop-up:** it fades in while the panel rises slightly.
  - **Buttons:** colors fade on press and release, and the selected color swatch's outline grows in.
  - **Scores:** a staged-change chip eases in from the direction of each tap, merges into the score when it's added, and the total counts to its new value. After a reset, every score counts to its new value.
- **Game options (gear ⚙):** a panel slides down from under the header.
  - **STEP SIZE:** 1, 5, 10 or 25 points per tap.
  - **ADD UP AFTER:** 0.8 s, 1.5 s or 3 s, for how long scoring must pause before staged changes are added.
  - **NEW GAME:** asks for confirmation, then goes back to the default players at 0. The dialog names them.
  - Tap outside the panel, CLOSE, or the gear again to close it. Both options are saved with the session.

Known limitations:
- To wipe the saved session and go back to the defaults, run `idf.py -p COMx erase-flash flash`. There's no on-screen "new session" option yet.
- The Score Keeper has no score keypad or undo yet.
- **Battery support:** this is only verified with no battery connected. The AXP2101 reports USB present and no battery. Charger settings (400 mA, 4.2 V, temperature sensing off) match Waveshare's 01_AXP2101 example, but charging and the percentage still need checking with a real cell.
- The Timer's set time isn't saved through a power loss.
- `components/XPowersLib` is vendored unchanged from Waveshare's `examples/esp-idf/01_AXP2101`. It's MIT-licensed; see its LICENSE file.
- **Fonts:** the timer digits are rendered at runtime with LVGL Tiny TTF from `main/assets/Montserrat-Medium.ttf`, licensed under SIL OFL; see `Montserrat-OFL.txt`. Each digit takes a moment to render the first time it's shown, then it's cached.
- **Dice drawings:** `main/apps/dice/dice_icons.cpp` is generated from the sheet `tools/dice_icons/dice-icons.png`. To change the drawings, replace the sheet, keeping the same tile layout (d6, d4, d12, d10, d8, then d20 on a second row; black lines on light tiles), and run `python tools/dice_icons/make_dice_icons.py tools/dice_icons/dice-icons.png main/apps/dice/dice_icons.cpp`. Add `--preview out.png` to get a mock of the dice screen. The script needs only the Python standard library.
- Fonts are LVGL's built-in Montserrat, which is medium weight rather than bold.

## Dependencies

| Item | Version | Source |
|---|---|---|
| ESP-IDF | v5.5.5 | Installed with Espressif EIM |
| Board support | `waveshare/esp32_s3_touch_lcd_4b` 2.0.0 | IDF Component Registry |
| LVGL | 9.5.0 | IDF Component Registry |

These versions match Waveshare's validated `examples/esp-idf/02_lvgl_demo_v9`. All display, touch and backlight setup comes from the board support package, and this project defines no GPIOs of its own.

Three changes from Waveshare's display defaults make redraws about twice as fast (measured on the board: a full-screen frame went from about 100 ms to 45 ms):
- LVGL draws into a 48-line buffer in internal RAM. The board support package lets the buffer land in PSRAM, which is much slower to draw into.
- LVGL uses one draw thread (`CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=1`). Two were slower on this board.
- The LVGL task is pinned to core 1, away from the panel's refresh interrupts on core 0.

## Toolchain setup (Windows, one time)

```powershell
winget install --id Espressif.EIM-CLI -e
eim install -i v5.5.5 -t esp32s3 -p C:\Espressif -n true -a true
```

To get a build shell, open a PowerShell window and dot-source the activation script that EIM created in `C:\Espressif`. You can also use the "ESP-IDF" Start-menu shortcut.

## Build and flash

Either USB-C port can be used to flash the board. On the development PC:
- The native USB port shows up as "USB Serial Device" on **COM3**.
- The USB-to-UART port shows up as "USB-Enhanced-SERIAL CH343" on **COM4**.

Use whichever port the board is plugged into.

```powershell
cd tabletop-tools
idf.py set-target esp32s3   # first time only
idf.py build
idf.py -p COM3 flash monitor
```

If flashing can't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and run the command again. Flashing replaces Waveshare's factory demo. You can restore it from the `firmware/` folder in Waveshare's GitHub repo.

### Your own default players

New games, and first boots, start with the players in [main/apps/scorekeeper/default_players.h](../main/apps/scorekeeper/default_players.h). The published list is Alex, Sam, Jordan and Riley.

To use your own names without publishing them:
1. Copy that file to `default_players.local.h` in the same folder, and edit the names and colors in the copy.
2. Run `idf.py reconfigure`, then build and flash as usual.

`default_players.local.h` is listed in `.gitignore`, so it never reaches GitHub. When it exists, your own builds use it. The public build below always ignores it.

### Updating the browser-flash file

The README's browser instructions use [firmware/tabletop-tools.bin](../firmware/tabletop-tools.bin). It's one image holding the bootloader, partition table and app, written at `0x0`. Regenerate it after every firmware change you want to share.

Build it in its own `build-public` folder, with `TT_PUBLIC_BUILD` set, so it gets the published default players and never your private ones:

```powershell
idf.py -B build-public -DTT_PUBLIC_BUILD=1 build
idf.py -B build-public merge-bin -o tabletop-tools.bin   # writes build-public/tabletop-tools.bin
copy build-public\tabletop-tools.bin firmware\tabletop-tools.bin
```

The first public build compiles everything from scratch, which takes a few minutes. Don't copy the image from `build/` instead, because that one contains your private names.

The image fills the gap between the partition table and the app with `0xFF`, and that gap holds the NVS partition. Flashing it therefore erases saved players, scores and settings, which is fine for a new board. `idf.py flash` writes only the three parts and keeps NVS.

## Device test checklist

- [ ] The UI fills the full screen, with the header at the top and gradient rows below.
- [ ] No rounded corners, and dark dividers separate every cell.
- [ ] + and − change only their own row. Check rows at the top and bottom and both button columns.
- [ ] Holding − auto-repeats, and negative scores display correctly.
- [ ] Rapid tapping stays responsive with no crash. The serial log shows `scorekeeper ready` and no errors.
- [ ] On the first boot, or after `erase-flash`, the rows show the default players (ALEX, SAM, JORDAN, RILEY, or your private list), all at 0, and the sort button shows ▼ 321.
- [ ] Tap + on the fourth row 6 times and − on the second row 5 times. The rows show `0 +6` and `0 -5`, and nothing moves while you tap.
- [ ] About 1.5 s after the last tap, both chips merge at the same moment, the totals count to 6 and -5, and then the fourth row's player slides to the top and the second row's to the bottom.
- [ ] Tap + then − on the same row. The chip disappears and nothing is added.
- [ ] Change some scores, rename someone, wait 2 s, then unplug the board and plug it back in. Everything comes back: players, names, colors, scores and the sort direction. The serial log shows `sk_storage: restored N players`.
- [ ] Change a score, then cut power right away (in under 1 s). After the reboot, at most that last change is missing.
- [ ] The sort button switches between ▼ 321 and ▲ 123, and each tap flips the row order immediately.
- [ ] Tap a name, type a new one, pick a color, and tap SAVE. The row updates with the new name and color. CANCEL changes nothing.
- [ ] Names cap at 20 characters, and long names end in "…" in the row.
- [ ] ADD creates a player at 0. After 8 players, ADD goes gray, and rows stay usable with the smaller fonts.
- [ ] REMOVE asks for confirmation. CANCEL keeps the player, and REMOVE deletes only that player.
- [ ] RESET opens the split view at 0, and the preview lists every player with their own color.
- [ ] ± changes the value, and every preview score follows it. Holding + for more than 2 s jumps by 10.
- [ ] ✕ returns with every score unchanged. ✓ sets every score to the value, and players and colors stay the same.
- [ ] Opening RESET again starts at the last value used.
- [ ] Sort changes slide the rows smoothly, with no jumping and no visible tearing.
- [ ] On battery, with the board flat on the table, quick taps on + and − all count, and each one shows the moment your finger lands.
  - Try each TOUCH SENSITIVITY level: NORMAL, HIGH (the default) and MAX. On NORMAL, taps go missing on battery.
  - On MAX, watch for touches the board registers by itself. If you see any, go back to HIGH.
- [ ] Add a player and remove one. Rows slide in and out, and the others resize smoothly.
- [ ] The editor and reset screens slide up and down smoothly. Tapping during the closing slide does nothing.
- [ ] The board boots to the home screen. Score Keeper and Calculator open with a slide, and the four COMING SOON tiles don't respond.
- [ ] In the Score Keeper, change some scores, go home through the waffle, open the Calculator, then go back. Every score is unchanged.
- [ ] In the Calculator, type 12 and tap +, then 24 and +, then 7 and +. The tape reads `12 + 24 + 7` and TOTAL counts up to 43.
- [ ] Type 5 and tap −. The tape reads `12 + 24 + 7 - 5` and TOTAL is 38.
- [ ] UNDO removes the last number. CLEAR empties everything, and UNDO right after it brings the tape back.
- [ ] Add more than three lines of numbers. The tape scrolls to the newest, and you can drag it back up.
- [ ] Leave the Calculator and come back. The tape is still there.
- [ ] The Timer opens at `60s` with 12 ticks lit. Dragging around the dial changes the time in 5 s steps, up to 5:00, and won't wrap past 12 o'clock.
- [ ] Tap the time. +/− on minutes and seconds work independently, presets fill in the time, DONE applies it, and CANCEL doesn't.
- [ ] Set 1:05 and press START. The display goes `1:05` → `1:01` → `60s` → `59s`, and the dial empties steadily.
- [ ] PAUSE freezes the timer, RESUME continues it, and RESET returns to the set time. RESET slides in and out smoothly.
- [ ] At zero, the dial flashes and the speaker beeps. Tapping the dial silences it, and with 🔊 muted it stays silent.
- [ ] Start a timer, go home and open another app. The alarm still sounds on time.
- [ ] Settings: dragging the brightness bar changes the backlight live. With auto-dim at 1m and no touch, the screen dims after 1 minute and goes dark a minute later. A tap wakes it without pressing anything.
- [ ] Settings: the volume bar plus TEST changes the beep level, and TIMER ALARM OFF silences the timer. The timer's 🔊 icon follows that setting, and the other way round.
- [ ] Settings: TAP CLICKS ON ticks on every touch.
- [ ] Settings: switching TOUCH SENSITIVITY between NORMAL, HIGH and MAX takes effect at once, and survives a restart. The boot log line `touch: thresholds: press …, release …` shows the active values.
- [ ] Settings: BATTERY reads NO BATTERY - RUNNING ON USB, with a USB voltage of about 4.7-5.1 V.
- [ ] Change brightness and volume, then power-cycle the board. Both are restored.
- [ ] Score Keeper gear: STEP SIZE 5 makes each tap stage ±5, and ADD UP AFTER 3s waits longer before adding. Both survive a reboot.
- [ ] NEW GAME asks first, naming the default players, then shows them at 0.
- [ ] RESET ALL DATA asks first, then restarts with the default players and settings.
- [ ] The Dice tile opens the dice screen, with six tiles around a gray ROLL button labelled ADD DICE.
- [ ] Tap d6 twice and d20 once. The badges read x2 and x1, and the button turns violet and reads `2d6 + d20`. Holding d6 takes it back down to x1.
- [ ] ROLL tumbles and then lands on a total between 3 and 32, and the breakdown below adds up to it. Tapping the total rolls again.
- [ ] CLEAR empties every tile and goes back to ADD DICE.
- [ ] PWR: a short press turns the screen off. Another press turns it back on, and so does a touch, which doesn't press whatever is under your finger.
- [ ] PWR while the timer runs: the screen stays off until woken, and the alarm wakes it at the end.
- [ ] Hold PWR on battery for 1.5 s: POWERING OFF appears. Let go, and the screen goes dark and the board stays off. A short press of PWR boots it to the home screen with scores and settings as you left them.
- [ ] Change a score, wait for the chip to add up, then power off by holding PWR right away: the new score is there after it boots.
- [ ] Power off with USB plugged in: the board stays off and the battery keeps charging.
- [ ] BOOT from Dice, Timer or Score Keeper goes home, sliding right. In Settings, open RESET ALL DATA and press BOOT: the dialog closes and nothing is erased.
- [ ] Finger Picker: one finger lights up and the hint says it's waiting for more fingers. A second finger, in a different color, starts the 5-second countdown with ticks, and streaks bounce between the two.
- [ ] Add a third finger mid-countdown: it gets its own color, the count goes back to 5, and a third streak joins. Lift one: its light fades and the count restarts.
- [ ] At 0 the streaks converge on one finger, which grows and chimes while the rest fade. It stays lit after everyone lets go, and the next touch starts a new round.
- [ ] Try 5 fingers at once, then several rounds in a row, and check that different fingers win.
