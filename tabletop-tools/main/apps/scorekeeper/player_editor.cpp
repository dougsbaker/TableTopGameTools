#include "apps/scorekeeper/player_editor.h"

#include <cstdio>
#include <cstring>

#include "ui/components/confirm_dialog.h"
#include "ui/components/header_bar.h"
#include "ui/components/keyboard.h"
#include "ui/motion.h"
#include "ui/theme.h"

namespace scorekeeper {
namespace {

constexpr int32_t kBarButtonW = 150;
constexpr int32_t kPreviewH = 70;
constexpr int32_t kPreviewPadX = 16;
constexpr int32_t kPaletteH = 56;
constexpr int32_t kSwatchBorder = 5;
constexpr lv_opa_t kPlaceholderOpa = LV_OPA_60;

struct Editor {
    ScoreKeeper *model = nullptr;
    uint8_t player_id = 0;  // 0 while creating a new player
    EditorDoneCb on_done = nullptr;
    char name[kMaxNameLen + 1] = {};
    uint8_t color = 0;
    lv_obj_t *root = nullptr;
    lv_obj_t *preview = nullptr;
    lv_obj_t *name_label = nullptr;
    lv_obj_t *swatches[theme::kPlayerColorCount] = {};
};

Editor s_editor;

void refresh()
{
    theme::set_bg(s_editor.preview, theme::player_color(s_editor.color));

    lv_obj_t *label = s_editor.name_label;
    if (s_editor.name[0] == '\0') {
        lv_label_set_text(label, "TYPE A NAME");
        lv_obj_set_style_text_opa(label, kPlaceholderOpa, 0);
    } else {
        lv_label_set_text_fmt(label, "%s_", s_editor.name);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
    // Keep the end of a long name (where typing happens) visible.
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_update_layout(label);
    if (lv_obj_get_width(label) > lv_obj_get_content_width(s_editor.preview)) {
        lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    for (uint8_t i = 0; i < theme::kPlayerColorCount; ++i) {
        lv_obj_set_state(s_editor.swatches[i], LV_STATE_CHECKED, i == s_editor.color);
    }
}

// Slides away and deletes itself afterwards, so it's safe inside a child's event callback.
void close()
{
    ui::overlay_slide_out(s_editor.root);
    s_editor.root = nullptr;
}

void finish()
{
    EditorDoneCb on_done = s_editor.on_done;
    close();
    if (on_done != nullptr) {
        on_done();
    }
}

void on_name_changed(void *)
{
    refresh();
}

void on_swatch(lv_event_t *e)
{
    s_editor.color = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    refresh();
}

void on_cancel(lv_event_t *)
{
    if (s_editor.root != nullptr) {
        close();
    }
}

void on_save(lv_event_t *)
{
    if (s_editor.root == nullptr) {
        return;
    }
    size_t len = std::strlen(s_editor.name);
    while (len > 0 && s_editor.name[len - 1] == ' ') {
        s_editor.name[--len] = '\0';
    }

    ScoreKeeper &model = *s_editor.model;
    if (s_editor.player_id == 0) {
        if (len == 0) {
            std::snprintf(s_editor.name, sizeof(s_editor.name), "PLAYER %u",
                          static_cast<unsigned>(model.count() + 1));
        }
        model.add_player(s_editor.name, s_editor.color);
    } else {
        int index = model.find(s_editor.player_id);
        if (index >= 0) {
            if (len > 0) {
                model.set_name(index, s_editor.name);
            }
            model.set_color(index, s_editor.color);
        }
    }
    finish();
}

void on_remove_confirmed(void *)
{
    if (s_editor.root == nullptr) {
        return;
    }
    int index = s_editor.model->find(s_editor.player_id);
    if (index >= 0) {
        s_editor.model->remove_player(index);
    }
    finish();
}

void on_remove(lv_event_t *)
{
    int index = s_editor.model->find(s_editor.player_id);
    if (index < 0) {
        return;
    }
    char title[kMaxNameLen + 16];
    std::snprintf(title, sizeof(title), "REMOVE %s?", s_editor.model->player(index).name);
    ui::confirm_dialog_show(title, "Their score will be deleted. Other players are not affected.",
                            LV_SYMBOL_TRASH " REMOVE", on_remove_confirmed, nullptr);
}

void build_top_bar(bool editing)
{
    lv_obj_t *bar = ui::header_bar_create(s_editor.root);
    ui::header_button_create(bar, kBarButtonW, LV_SYMBOL_CLOSE " CANCEL", on_cancel, nullptr);
    if (editing && s_editor.model->count() > 1) {
        lv_obj_t *remove = ui::header_button_create(bar, 0, LV_SYMBOL_TRASH " REMOVE", on_remove, nullptr);
        lv_obj_set_style_text_color(remove, lv_color_hex(theme::kDangerText), 0);
    } else {
        ui::header_button_create(bar, 0, editing ? "EDIT PLAYER" : "NEW PLAYER", nullptr, nullptr);
    }
    ui::header_button_create(bar, kBarButtonW, LV_SYMBOL_OK " SAVE", on_save, nullptr);
}

void build_preview()
{
    s_editor.preview = theme::make_box(s_editor.root);
    lv_obj_set_size(s_editor.preview, LV_PCT(100), kPreviewH);
    lv_obj_set_style_pad_hor(s_editor.preview, kPreviewPadX, 0);
    s_editor.name_label = lv_label_create(s_editor.preview);
    lv_obj_set_style_text_font(s_editor.name_label, &lv_font_montserrat_28, 0);
}

void build_palette()
{
    lv_obj_t *palette = theme::make_box(s_editor.root);
    lv_obj_set_size(palette, LV_PCT(100), kPaletteH);
    lv_obj_set_flex_flow(palette, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(palette, theme::kGap, 0);
    for (uint8_t i = 0; i < theme::kPlayerColorCount; ++i) {
        lv_obj_t *swatch = theme::make_box(palette);
        lv_obj_set_height(swatch, LV_PCT(100));
        lv_obj_set_flex_grow(swatch, 1);
        theme::set_bg(swatch, theme::player_color(i));
        // The selected swatch is CHECKED; its white inner border grows in smoothly.
        lv_obj_set_style_border_color(swatch, lv_color_hex(theme::kText), 0);
        lv_obj_set_style_border_width(swatch, kSwatchBorder, LV_STATE_CHECKED);
        lv_obj_set_style_transition(swatch, theme::state_transition(), 0);
        lv_obj_set_style_transition(swatch, theme::state_transition(), LV_STATE_CHECKED);
        lv_obj_add_event_cb(swatch, on_swatch, LV_EVENT_CLICKED, reinterpret_cast<void *>(uintptr_t{i}));
        s_editor.swatches[i] = swatch;
    }
}

}  // namespace

void player_editor_open(lv_obj_t *parent, ScoreKeeper &model, uint8_t player_id, EditorDoneCb on_done)
{
    if (s_editor.root != nullptr) {
        return;
    }
    s_editor = Editor{};
    s_editor.model = &model;
    s_editor.on_done = on_done;

    int index = model.find(player_id);
    bool editing = index >= 0;
    if (editing) {
        const Player &p = model.player(index);
        s_editor.player_id = p.id;
        std::memcpy(s_editor.name, p.name, sizeof(s_editor.name));
        s_editor.color = p.color;
    } else {
        s_editor.color = model.next_free_color(theme::kPlayerColorCount);
    }

    // Opaque and clickable, so it fully covers (and blocks touches to) the Score Keeper.
    s_editor.root = theme::make_box(parent);
    lv_obj_set_size(s_editor.root, theme::kScreenW, theme::kScreenH);
    theme::set_bg(s_editor.root, lv_color_hex(theme::kBackground));
    lv_obj_set_flex_flow(s_editor.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_editor.root, theme::kGap, 0);

    build_top_bar(editing);
    build_preview();
    build_palette();
    ui::keyboard_create(s_editor.root, s_editor.name, kMaxNameLen, on_name_changed, nullptr);
    refresh();
    ui::overlay_slide_in(s_editor.root);
}

}  // namespace scorekeeper
