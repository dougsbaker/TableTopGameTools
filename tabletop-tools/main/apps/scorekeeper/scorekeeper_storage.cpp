#include "apps/scorekeeper/scorekeeper_storage.h"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "services/storage.h"

namespace scorekeeper {
namespace {

const char *TAG = "sk_storage";

constexpr const char *kNamespace = "scorekeeper";
constexpr const char *kKey = "session";

// Bump when StoredSession changes, and keep a loader for each older version.
// v1: players, sort mode, starting score.  v2: adds step size and add-up delay.
constexpr uint16_t kSchemaVersion = 2;

// On-flash layout. Fixed-size fields with no implicit padding, so equal sessions compare
// equal byte-for-byte.
struct StoredPlayer {
    int32_t score;
    uint8_t id;
    uint8_t color;
    char name[kMaxNameLen + 1];
    uint8_t reserved;
};

struct StoredSessionV1 {
    uint16_t version;
    uint8_t count;
    uint8_t sort_mode;
    int32_t starting_score;
    StoredPlayer players[kMaxPlayers];
};

struct StoredSession {
    uint16_t version;
    uint8_t count;
    uint8_t sort_mode;
    int32_t starting_score;
    uint8_t step_index;
    uint8_t commit_delay_index;
    uint8_t reserved[2];
    StoredPlayer players[kMaxPlayers];
};

static_assert(sizeof(StoredPlayer) == 28, "StoredPlayer must not contain padding");
static_assert(sizeof(StoredSessionV1) == 8 + 28 * kMaxPlayers, "StoredSessionV1 must not contain padding");
static_assert(sizeof(StoredSession) == 12 + 28 * kMaxPlayers, "StoredSession must not contain padding");
static_assert(sizeof(StoredSession) <= storage::kMaxBlob, "StoredSession is too big for storage");

StoredSession s_last_saved = {};

void snapshot(const ScoreKeeper &model, StoredSession &out)
{
    std::memset(&out, 0, sizeof(out));
    out.version = kSchemaVersion;
    out.count = static_cast<uint8_t>(model.count());
    out.sort_mode = static_cast<uint8_t>(model.sort_mode());
    out.starting_score = model.starting_score();
    out.step_index = model.step_index();
    out.commit_delay_index = model.commit_delay_index();
    for (size_t i = 0; i < model.count(); ++i) {
        const Player &p = model.player(i);
        StoredPlayer &sp = out.players[i];
        sp.score = p.score;
        sp.id = p.id;
        sp.color = p.color;
        std::memcpy(sp.name, p.name, sizeof(sp.name));
    }
}

// Upgrades an older blob in place. Returns false for formats this firmware doesn't know.
bool migrate(StoredSession &session, size_t size)
{
    if (size == sizeof(StoredSession) && session.version == kSchemaVersion) {
        return true;
    }
    if (size == sizeof(StoredSessionV1) && session.version == 1) {
        StoredSessionV1 v1;
        std::memcpy(&v1, &session, sizeof(v1));
        std::memset(&session, 0, sizeof(session));
        session.version = kSchemaVersion;
        session.count = v1.count;
        session.sort_mode = v1.sort_mode;
        session.starting_score = v1.starting_score;
        session.step_index = 0;
        session.commit_delay_index = kDefaultCommitDelay;
        std::memcpy(session.players, v1.players, sizeof(session.players));
        ESP_LOGI(TAG, "upgraded saved session from v1");
        return true;
    }
    return false;
}

}  // namespace

bool storage_load(ScoreKeeper &model)
{
    StoredSession session;
    size_t size = sizeof(session);
    esp_err_t err = storage::load_blob(kNamespace, kKey, &session, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "no saved session");
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "saved session unreadable (%s)", esp_err_to_name(err));
        return false;
    }
    if (!migrate(session, size)) {
        ESP_LOGW(TAG, "saved session has unknown format (version %u, %u bytes)",
                 static_cast<unsigned>(session.version), static_cast<unsigned>(size));
        return false;
    }

    Player players[kMaxPlayers] = {};
    size_t count = session.count <= kMaxPlayers ? session.count : 0;
    for (size_t i = 0; i < count; ++i) {
        const StoredPlayer &sp = session.players[i];
        players[i].id = sp.id;
        players[i].color = sp.color;
        players[i].score = sp.score;
        std::memcpy(players[i].name, sp.name, sizeof(players[i].name));
    }
    if (!model.restore(players, count, session.starting_score, static_cast<SortMode>(session.sort_mode))) {
        ESP_LOGW(TAG, "saved session failed validation");
        return false;
    }
    model.set_step_index(session.step_index);
    model.set_commit_delay_index(session.commit_delay_index);
    snapshot(model, s_last_saved);
    ESP_LOGI(TAG, "restored %u players", static_cast<unsigned>(count));
    return true;
}

void storage_save(const ScoreKeeper &model)
{
    StoredSession session;
    snapshot(model, session);
    if (std::memcmp(&session, &s_last_saved, sizeof(session)) == 0) {
        return;  // nothing changed since the last save
    }
    if (storage::save_blob(kNamespace, kKey, &session, sizeof(session))) {
        s_last_saved = session;
    }
}

}  // namespace scorekeeper
