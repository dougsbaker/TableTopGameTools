#pragma once

#include <cstdint>

// The players a new game starts with, top to bottom: name and color index (theme::player_color).
//
// To start with your own players without publishing their names, copy this file to
// default_players.local.h in this folder and edit the copy. That file is git-ignored and, when
// it exists, replaces this one in every build except the public one (docs/DEVELOPMENT.md).
namespace scorekeeper {

struct DefaultPlayer {
    const char *name;
    uint8_t color;
};

constexpr DefaultPlayer kDefaultPlayers[] = {
    {"Alex", 0},
    {"Sam", 1},
    {"Jordan", 2},
    {"Riley", 3},
};

}  // namespace scorekeeper
