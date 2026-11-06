#include "apps/scorekeeper/scorekeeper.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

// A private list of default players, if there is one, replaces the published one; see
// default_players.h. main/CMakeLists.txt decides which.
#ifdef TT_LOCAL_PLAYERS
#include "apps/scorekeeper/default_players.local.h"
#else
#include "apps/scorekeeper/default_players.h"
#endif

namespace scorekeeper {

constexpr size_t kDefaultCount = sizeof(kDefaultPlayers) / sizeof(kDefaultPlayers[0]);
static_assert(kDefaultCount >= 1 && kDefaultCount <= kMaxPlayers, "1 to kMaxPlayers default players");

const char *default_player_names()
{
    static char names[kMaxPlayers * (kMaxNameLen + 2) + 8] = "";
    if (names[0] == '\0') {
        size_t used = 0;
        for (size_t i = 0; i < kDefaultCount; ++i) {
            const char *sep = i == 0 ? "" : i + 1 == kDefaultCount ? " and " : ", ";
            used += std::snprintf(names + used, sizeof(names) - used, "%s%s", sep, kDefaultPlayers[i].name);
        }
    }
    return names;
}

int32_t clamp_score(int64_t value)
{
    if (value > kMaxScore) {
        return kMaxScore;
    }
    if (value < kMinScore) {
        return kMinScore;
    }
    return static_cast<int32_t>(value);
}

void ScoreKeeper::load_defaults()
{
    count_ = 0;
    next_id_ = 1;
    starting_score_ = 0;
    for (const DefaultPlayer &p : kDefaultPlayers) {
        add_player(p.name, p.color);
    }
}

bool ScoreKeeper::restore(const Player *players, size_t count, int32_t starting_score, SortMode sort_mode)
{
    if (count == 0 || count > kMaxPlayers) {
        return false;
    }
    uint8_t max_id = 0;
    for (size_t i = 0; i < count; ++i) {
        const Player &p = players[i];
        if (p.id == 0 || std::memchr(p.name, '\0', sizeof(p.name)) == nullptr) {
            return false;
        }
        for (size_t j = 0; j < i; ++j) {
            if (players[j].id == p.id) {
                return false;
            }
        }
        max_id = std::max(max_id, p.id);
    }

    count_ = count;
    for (size_t i = 0; i < count; ++i) {
        players_[i] = players[i];
        players_[i].score = clamp_score(players[i].score);
        players_[i].pending = 0;
    }
    for (size_t i = count; i < kMaxPlayers; ++i) {
        players_[i] = {};
    }
    next_id_ = static_cast<uint8_t>(max_id + 1);
    starting_score_ = clamp_score(starting_score);
    sort_mode_ = sort_mode == SortMode::kLowFirst ? SortMode::kLowFirst : SortMode::kHighFirst;
    return true;
}

uint8_t ScoreKeeper::allocate_id()
{
    // Skips 0 and ids still in use, so wrapping past 255 never produces a duplicate.
    for (;;) {
        uint8_t id = next_id_++;
        if (id != 0 && find(id) < 0) {
            return id;
        }
    }
}

uint8_t ScoreKeeper::add_player(const char *name, uint8_t color, int32_t score)
{
    if (full()) {
        return 0;
    }
    uint8_t id = allocate_id();
    Player &p = players_[count_++];
    p.id = id;
    p.color = color;
    p.score = score;
    p.pending = 0;
    set_name(count_ - 1, name);
    return p.id;
}

void ScoreKeeper::remove_player(size_t index)
{
    if (index >= count_) {
        return;
    }
    for (size_t i = index + 1; i < count_; ++i) {
        players_[i - 1] = players_[i];
    }
    --count_;
    players_[count_] = {};
}

void ScoreKeeper::set_name(size_t index, const char *name)
{
    // Names are stored uppercase, which is how every screen shows them (Goal.md §4).
    Player &p = players_[index];
    size_t i = 0;
    for (; name[i] != '\0' && i < kMaxNameLen; ++i) {
        p.name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
    }
    p.name[i] = '\0';
}

void ScoreKeeper::set_color(size_t index, uint8_t color)
{
    players_[index].color = color;
}

int32_t ScoreKeeper::stage(size_t index, int32_t delta)
{
    Player &p = players_[index];
    int32_t target = clamp_score(static_cast<int64_t>(p.score) + p.pending + delta);
    p.pending = target - p.score;
    return p.pending;
}

bool ScoreKeeper::has_pending() const
{
    for (size_t i = 0; i < count_; ++i) {
        if (players_[i].pending != 0) {
            return true;
        }
    }
    return false;
}

void ScoreKeeper::commit_pending()
{
    for (size_t i = 0; i < count_; ++i) {
        Player &p = players_[i];
        p.score = clamp_score(static_cast<int64_t>(p.score) + p.pending);
        p.pending = 0;
    }
}

void ScoreKeeper::reset_scores(int32_t value)
{
    starting_score_ = clamp_score(value);
    for (size_t i = 0; i < count_; ++i) {
        players_[i].score = starting_score_;
        players_[i].pending = 0;
    }
}

int ScoreKeeper::find(uint8_t id) const
{
    for (size_t i = 0; i < count_; ++i) {
        if (players_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint8_t ScoreKeeper::next_free_color(uint8_t palette_size) const
{
    for (uint8_t color = 0; color < palette_size; ++color) {
        bool used = false;
        for (size_t i = 0; i < count_ && !used; ++i) {
            used = players_[i].color == color;
        }
        if (!used) {
            return color;
        }
    }
    return static_cast<uint8_t>(count_ % palette_size);
}

size_t ScoreKeeper::display_order(size_t out[kMaxPlayers]) const
{
    for (size_t i = 0; i < count_; ++i) {
        out[i] = i;
    }
    // Stable, so tied players keep the order they were added in.
    if (sort_mode_ == SortMode::kHighFirst) {
        std::stable_sort(out, out + count_,
                         [this](size_t a, size_t b) { return players_[a].score > players_[b].score; });
    } else {
        std::stable_sort(out, out + count_,
                         [this](size_t a, size_t b) { return players_[a].score < players_[b].score; });
    }
    return count_;
}

}  // namespace scorekeeper
