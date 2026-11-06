#include "ui/components/toast.h"

#include "ui/motion.h"
#include "ui/theme.h"

namespace ui {
namespace {

constexpr int32_t kHeight = 56;
constexpr uint32_t kSlideMs = 250;
constexpr uint32_t kShowMs = 3500;

// Once it has slid in, it waits and then slides back out, deleting itself at the end.
void on_shown(lv_anim_t *a)
{
    auto *toast = static_cast<lv_obj_t *>(lv_anim_get_user_data(a));
    animate(toast, exec_y, 0, -kHeight, kSlideMs, lv_anim_path_ease_in, kShowMs, lv_obj_delete_anim_completed_cb);
}

}  // namespace

void toast_show(const char *text, lv_color_t color)
{
    lv_obj_t *toast = theme::make_box(lv_layer_top());
    lv_obj_set_size(toast, theme::kScreenW, kHeight);
    lv_obj_set_y(toast, -kHeight);
    theme::set_bg(toast, color);
    lv_obj_remove_flag(toast, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *label = lv_label_create(toast);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_exec_cb(&a, exec_y);
    lv_anim_set_values(&a, -kHeight, 0);
    lv_anim_set_duration(&a, kSlideMs);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_user_data(&a, toast);
    lv_anim_set_completed_cb(&a, on_shown);
    lv_anim_start(&a);
}

}  // namespace ui
