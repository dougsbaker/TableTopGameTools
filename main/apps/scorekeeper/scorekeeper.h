#pragma once

#include <cstddef>
#include <cstdint>

// Score Keeper data model. No LVGL or NVS here, so the UI and persistence stay separate.
namespace scorekeeper {

constexpr size_t kMaxPlayers = 8;
constexpr size_t kMaxNameLen = 20;
constexpr int32_t kMinScore = -99999;
constexpr int32_t kMaxScore = 99999;

struct Player {
    uint8_t id;  // Unique among current players; 0 means "no player".
    char name[kMaxNameLen + 1];
    uint8_t color;
    int32_t score;
    int32_t pending;  // Staged change, shown beside the score until commit_pending(). Not saved.
};

// Points per ± tap, and how long scoring must pause before staged changes are added.
constexpr int32_t kSteps[] = {1, 5, 10, 25};
constexpr uint32_t kCommitDelaysMs[] = {800, 1500, 3000};
constexpr uint8_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);
constexpr uint8_t kCommitDelayCount = sizeof(kCommitDelaysMs) / sizeof(kCommitDelaysMs[0]);
constexpr uint8_t kDefaultCommitDelay = 1;

// Clamps `value` to [kMinScore, kMaxScore].
int32_t clamp_score(int64_t value);

// The default players' names for display, such as "Alex, Sam, Jordan and Riley".
const char *default_player_names();

enum class SortMode : uint8_t {
    kHighFirst,
    kLowFirst,
};

class ScoreKeeper {
public:
    // Replaces the players with the defaults from default_players.h, all at 0. Step size,
    // add-up delay and sort direction are kept.
    void load_defaults();

    // Replaces the session with saved data. Returns false (model unchanged) if the data is
    // invalid: no players, too many, missing/duplicate ids or unterminated names.
    // Out-of-range scores are clamped and an unknown sort mode falls back to high-first.
    bool restore(const Player *players, size_t count, int32_t starting_score, SortMode sort_mode);

    // Returns the new player's id, or 0 when the player list is full.
    uint8_t add_player(const char *name, uint8_t color, int32_t score = 0);
    void remove_player(size_t index);
    void set_name(size_t index, const char *name);
    void set_color(size_t index, uint8_t color);

    // Adds `delta` to player `index`'s staged change, so that score + pending stays within
    // [kMinScore, kMaxScore]. Returns the new staged change.
    int32_t stage(size_t index, int32_t delta);
    bool has_pending() const;
    // Adds every staged change to its player's score at once.
    void commit_pending();

    // Sets every player's score to `value` (clamped) and drops staged changes, keeping the
    // players and their colors.
    // `value` is remembered as the starting score offered by the next reset.
    void reset_scores(int32_t value);
    int32_t starting_score() const { return starting_score_; }

    // Index of the player with `id`, or -1 if there is none.
    int find(uint8_t id) const;

    // First color slot no player is using; cycles through the palette when all are taken.
    uint8_t next_free_color(uint8_t palette_size) const;

    // Writes player indices in display order (per the sort mode) to `out`; returns count().
    size_t display_order(size_t out[kMaxPlayers]) const;

    size_t count() const { return count_; }
    bool full() const { return count_ >= kMaxPlayers; }
    const Player &player(size_t index) const { return players_[index]; }
    int32_t increment() const { return kSteps[step_index_]; }
    uint8_t step_index() const { return step_index_; }
    void set_step_index(uint8_t index) { step_index_ = index < kStepCount ? index : 0; }
    uint32_t commit_delay_ms() const { return kCommitDelaysMs[commit_delay_index_]; }
    uint8_t commit_delay_index() const { return commit_delay_index_; }
    void set_commit_delay_index(uint8_t index)
    {
        commit_delay_index_ = index < kCommitDelayCount ? index : kDefaultCommitDelay;
    }
    SortMode sort_mode() const { return sort_mode_; }
    void set_sort_mode(SortMode mode) { sort_mode_ = mode; }

private:
    uint8_t allocate_id();

    Player players_[kMaxPlayers] = {};
    size_t count_ = 0;
    uint8_t next_id_ = 1;
    uint8_t step_index_ = 0;
    uint8_t commit_delay_index_ = kDefaultCommitDelay;
    int32_t starting_score_ = 0;
    SortMode sort_mode_ = SortMode::kHighFirst;
};

}  // namespace scorekeeper
