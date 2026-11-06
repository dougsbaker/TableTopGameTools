#include "ui/motion.h"

#include "ui/theme.h"

namespace ui {
namespace {

lv_obj_tree_walk_res_t make_inert_cb(lv_obj_t *obj, void *root)
{
    if (obj != root) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

}  // namespace

void exec_x(void *obj, int32_t value)
{
    lv_obj_set_x(static_cast<lv_obj_t *>(obj), value);
}

void exec_y(void *obj, int32_t value)
{
    lv_obj_set_y(static_cast<lv_obj_t *>(obj), value);
}

void exec_width(void *obj, int32_t value)
{
    lv_obj_set_width(static_cast<lv_obj_t *>(obj), value);
}

void exec_height(void *obj, int32_t value)
{
    lv_obj_set_height(static_cast<lv_obj_t *>(obj), value);
}

void exec_translate_x(void *obj, int32_t value)
{
    lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(obj), value, 0);
}

void exec_translate_y(void *obj, int32_t value)
{
    lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj), value, 0);
}

void exec_bg_opa(void *obj, int32_t value)
{
    lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value), 0);
}

void animate(lv_obj_t *obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t duration,
             lv_anim_path_cb_t path, uint32_t delay, lv_anim_completed_cb_t on_done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, path);
    lv_anim_set_delay(&a, delay);
    if (on_done != nullptr) {
        lv_anim_set_completed_cb(&a, on_done);
    }
    lv_anim_start(&a);
}

void delete_after(lv_obj_t *obj, uint32_t ms)
{
    // An animation with no exec callback is just a timer tied to the object's lifetime.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_completed_cb(&a, lv_obj_delete_anim_completed_cb);
    lv_anim_start(&a);
}

void make_inert(lv_obj_t *root)
{
    lv_obj_tree_walk(root, make_inert_cb, root);
}

void overlay_slide_in(lv_obj_t *overlay)
{
    lv_obj_set_y(overlay, theme::kScreenH);
    animate(overlay, exec_y, theme::kScreenH, 0, theme::kOverlayInMs, lv_anim_path_ease_out);
}

void overlay_slide_out(lv_obj_t *overlay)
{
    make_inert(overlay);
    animate(overlay, exec_y, lv_obj_get_style_y(overlay, LV_PART_MAIN), theme::kScreenH, theme::kOverlayOutMs,
            lv_anim_path_ease_in, 0, lv_obj_delete_anim_completed_cb);
}

}  // namespace ui
