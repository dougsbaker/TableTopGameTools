idf_component_register(
    SRCS
        "main.cpp"
        "ui/theme.cpp"
        "ui/motion.cpp"
        "ui/icons.cpp"
        "ui/navigation.cpp"
        "ui/fonts.cpp"
        "ui/battery_status.cpp"
        "ui/side_buttons.cpp"
        "ui/components/header_bar.cpp"
        "ui/components/keyboard.cpp"
        "ui/components/confirm_dialog.cpp"
        "ui/components/level_bar.cpp"
        "ui/components/choice_row.cpp"
        "ui/components/toast.cpp"
        "services/storage.cpp"
        "services/settings.cpp"
        "services/screen_power.cpp"
        "services/sound.cpp"
        "services/power.cpp"
        "services/touch_controller.cpp"
        "apps/launcher/launcher.cpp"
        "apps/scorekeeper/scorekeeper.cpp"
        "apps/scorekeeper/scorekeeper_storage.cpp"
        "apps/scorekeeper/scorekeeper_ui.cpp"
        "apps/scorekeeper/player_editor.cpp"
        "apps/scorekeeper/reset_screen.cpp"
        "apps/scorekeeper/game_menu.cpp"
        "apps/calculator/calculator.cpp"
        "apps/calculator/calculator_ui.cpp"
        "apps/timer/countdown.cpp"
        "apps/timer/timer_ui.cpp"
        "apps/timer/time_editor.cpp"
        "apps/settings/settings_ui.cpp"
        "apps/dice/dice.cpp"
        "apps/dice/dice_ui.cpp"
        "apps/dice/dice_icons.cpp"
        "apps/picker/picker.cpp"
        "apps/picker/picker_ui.cpp"
        "apps/picker/touch_points.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES "assets/Montserrat-Medium.ttf")

# A private apps/scorekeeper/default_players.local.h replaces the published default players,
# except in the public build (idf.py -DTT_PUBLIC_BUILD=1). Run idf.py reconfigure after adding
# or removing that file.
if(NOT TT_PUBLIC_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/apps/scorekeeper/default_players.local.h")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TT_LOCAL_PLAYERS)
endif()
