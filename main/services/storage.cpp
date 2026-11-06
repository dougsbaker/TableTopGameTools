#include "services/storage.h"

#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace storage {
namespace {

const char *TAG = "storage";

constexpr size_t kSlots = 4;
constexpr size_t kNameLen = 16;  // NVS namespace and key names are at most 15 characters
constexpr uint32_t kBatchMs = 300;
constexpr uint32_t kWriterStack = 4096;
constexpr UBaseType_t kWriterPriority = 2;  // below LVGL, so saving never delays drawing

struct Slot {
    bool pending;
    char ns[kNameLen];
    char key[kNameLen];
    size_t size;
    uint8_t data[kMaxBlob];
};

Slot s_slots[kSlots] = {};
SemaphoreHandle_t s_lock = nullptr;        // guards s_slots
SemaphoreHandle_t s_write_lock = nullptr;  // held while writing, so flush() waits out a write under way
TaskHandle_t s_task = nullptr;

void write(const Slot &slot)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(slot.ns, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, slot.key, slot.data, slot.size);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "saved %s/%s (%u bytes)", slot.ns, slot.key, static_cast<unsigned>(slot.size));
    } else {
        ESP_LOGE(TAG, "saving %s/%s failed: %s", slot.ns, slot.key, esp_err_to_name(err));
    }
}

// Writes every queued slot. Call with s_write_lock held.
void write_pending()
{
    static Slot copy;  // static: too big for comfort on a task stack
    for (size_t i = 0; i < kSlots; ++i) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool pending = s_slots[i].pending;
        if (pending) {
            copy = s_slots[i];
            s_slots[i].pending = false;
        }
        xSemaphoreGive(s_lock);
        if (pending) {
            write(copy);
        }
    }
}

void writer_task(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(kBatchMs));  // let a burst of changes settle into one write
        xSemaphoreTake(s_write_lock, portMAX_DELAY);
        write_pending();
        xSemaphoreGive(s_write_lock);
    }
}

}  // namespace

void start()
{
    s_lock = xSemaphoreCreateMutex();
    s_write_lock = xSemaphoreCreateMutex();
    xTaskCreate(writer_task, "storage", kWriterStack, nullptr, kWriterPriority, &s_task);
}

bool save_blob(const char *ns, const char *key, const void *data, size_t size)
{
    if (s_task == nullptr || size > kMaxBlob) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    Slot *target = nullptr;
    for (Slot &slot : s_slots) {
        if (slot.pending && std::strcmp(slot.ns, ns) == 0 && std::strcmp(slot.key, key) == 0) {
            target = &slot;
            break;
        }
        if (!slot.pending && target == nullptr) {
            target = &slot;
        }
    }
    if (target != nullptr) {
        std::strncpy(target->ns, ns, kNameLen - 1);
        target->ns[kNameLen - 1] = '\0';
        std::strncpy(target->key, key, kNameLen - 1);
        target->key[kNameLen - 1] = '\0';
        std::memcpy(target->data, data, size);
        target->size = size;
        target->pending = true;
    }
    xSemaphoreGive(s_lock);
    if (target == nullptr) {
        ESP_LOGE(TAG, "no free slot for %s/%s", ns, key);
        return false;
    }
    xTaskNotifyGive(s_task);
    return true;
}

void flush()
{
    if (s_task == nullptr) {
        return;
    }
    xSemaphoreTake(s_write_lock, portMAX_DELAY);
    write_pending();
    xSemaphoreGive(s_write_lock);
}

esp_err_t load_blob(const char *ns, const char *key, void *data, size_t *size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_blob(handle, key, data, size);
    nvs_close(handle);
    return err;
}

esp_err_t erase_all()
{
    return nvs_flash_erase();
}

}  // namespace storage
