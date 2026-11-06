#pragma once

#include <cstdint>

#include "lvgl.h"

// Small animation helpers shared by every screen. All run on the LVGL task.
namespace ui {

// Exec callbacks for lv_anim; `var` must be an lv_obj_t*.
void exec_x(void *obj, int32_t value);
void exec_y(void *obj, int32_t value);
void exec_width(void *obj, int32_t value);
void exec_height(void *obj, int32_t value);
void exec_translate_x(void *obj, int32_t value);
void exec_translate_y(void *obj, int32_t value);
void exec_bg_opa(void *obj, int32_t value);

// Starts an animation of `obj` from `from` to `to`. It replaces any running animation of the
// same object and exec callback, so retargeting mid-flight is smooth. If `on_done` is
// lv_obj_delete_anim_completed_cb the object deletes itself when the animation finishes.
void animate(lv_obj_t *obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t duration,
             lv_anim_path_cb_t path, uint32_t delay = 0, lv_anim_completed_cb_t on_done = nullptr);

// Deletes `obj` after `ms`, e.g. once its own exit animations have finished.
void delete_after(lv_obj_t *obj, uint32_t ms);

// Makes every descendant of `root` ignore touches (root itself keeps swallowing them), so a
// view can't be operated while it animates away.
void make_inert(lv_obj_t *root);

// Full-screen overlays slide up from the bottom edge to open, and back down to close.
// overlay_slide_out() makes the overlay inert and deletes it once it is off screen.
void overlay_slide_in(lv_obj_t *overlay);
void overlay_slide_out(lv_obj_t *overlay);

}  // namespace ui
