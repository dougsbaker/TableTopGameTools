#pragma once

#include "lvgl.h"

namespace ui {

using ConfirmCb = void (*)(void *user_data);

// Modal confirmation shown above everything else. `on_confirm` runs only when the confirm
// button is tapped; CANCEL just closes the dialog.
void confirm_dialog_show(const char *title, const char *message, const char *confirm_text,
                         ConfirmCb on_confirm, void *user_data);

// Closes the open dialog, if any, as if CANCEL was tapped.
void confirm_dialog_cancel();

}  // namespace ui
