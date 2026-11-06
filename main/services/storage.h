#pragma once

#include <cstddef>

#include "esp_err.h"

// Small-blob persistence in NVS. Writes happen on one low-priority background task, so saving
// never blocks the UI. nvs_flash_init() must have succeeded first.
namespace storage {

constexpr size_t kMaxBlob = 512;

// Starts the writer task. Until then, save_blob() calls are dropped.
void start();

// Queues a copy of `data` for `ns`/`key`. Writes are batched a moment after the last request,
// and a newer request for the same key replaces one that hasn't been written yet.
// Returns false if the blob is too large or there's no free slot.
bool save_blob(const char *ns, const char *key, const void *data, size_t size);

// Writes anything still queued now, on the calling task, after waiting out a write already
// under way. For use just before the power is cut; blocks for as long as the writes take.
void flush();

// Reads a blob now. `*size` is the buffer size on input and the stored size on output.
esp_err_t load_blob(const char *ns, const char *key, void *data, size_t *size);

// Erases everything this app has saved (used by "reset all data", followed by a restart).
esp_err_t erase_all();

}  // namespace storage
