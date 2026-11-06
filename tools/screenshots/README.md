# Screenshot capture

The images in [docs/screenshots](../../docs/screenshots) come straight from the board's framebuffer. A temporary firmware build plays scripted touches through the apps and streams each screen over USB, and `capture_shots.py` turns the stream into PNGs.

To take them again:

1. Copy `screenshots.cpp` and `screenshots.h` into `main/diag/`, and add `"diag/screenshots.cpp"` to the sources in `main/CMakeLists.txt`.
2. Make these temporary changes in `main/main.cpp`:
   - Don't call `storage::start()`. Without the storage writer, nothing the script does is saved.
   - Replace the saved Score Keeper session with demo players. The images used `load_defaults()`, then staged and committed the scores 142, 187, 96 and 163.
   - Call `diag::screenshots_start()` right after the last `bsp_display_unlock()`.
3. Build and flash the public build, so the screenshots show the published default players and never your private list:
   ```
   idf.py -B build-public -DTT_PUBLIC_BUILD=1 -p COM3 build flash
   ```
   Then, from the same ESP-IDF shell, run:
   ```
   python capture_shots.py COM3 <output folder> 150
   ```
   The script resets the board, collects every screen, checks each one against a CRC from the device, and writes PNGs. Each screen is sent twice, and the first intact copy is kept.
4. Copy the PNGs you want into `docs/screenshots/`. Then undo the changes and flash your normal build with `idf.py -p COM3 flash`. Your saved players come back, because nothing was written.

The tap positions in `run_script()` match the layouts as of November 2026. If a screen's layout changes, update its taps.
