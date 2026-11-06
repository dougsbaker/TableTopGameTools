#include "ui/navigation.h"

#include "ui/theme.h"

namespace nav {
namespace {

struct Entry {
    BuildFn build;
    lv_obj_t *screen;
};

Entry s_entries[kScreenCount] = {};
bool s_started = false;
Screen s_current = Screen::kHome;

lv_obj_t *get_screen(Screen id)
{
    Entry &entry = s_entries[static_cast<size_t>(id)];
    if (entry.screen == nullptr) {
        entry.screen = lv_obj_create(nullptr);
        theme::style_screen(entry.screen);
        entry.build(entry.screen);
    }
    return entry.screen;
}

}  // namespace

void register_screen(Screen id, BuildFn build)
{
    s_entries[static_cast<size_t>(id)].build = build;
}

void build_all()
{
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (s_entries[i].build != nullptr) {
            get_screen(static_cast<Screen>(i));
        }
    }
}

void show(Screen id)
{
    if (s_started && id == s_current) {
        return;
    }
    lv_obj_t *screen = get_screen(id);
    if (!s_started) {
        // Replace the display's empty default screen.
        lv_obj_t *initial = lv_screen_active();
        lv_screen_load(screen);
        if (initial != nullptr && initial != screen) {
            lv_obj_delete(initial);
        }
        s_started = true;
    } else {
        lv_screen_load_anim_t anim = id == Screen::kHome ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT : LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
        lv_screen_load_anim(screen, anim, theme::kScreenMs, 0, false);
    }
    s_current = id;
}

}  // namespace nav
