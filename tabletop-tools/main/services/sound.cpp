#include "services/sound.h"

#include <cmath>
#include <cstdint>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sound {
namespace {

const char *TAG = "sound";

constexpr uint32_t kSampleRate = 22050;  // the BSP's default I2S configuration
constexpr float kAmplitude = 12000.0f;
constexpr uint32_t kRampSamples = kSampleRate / 250;  // 4 ms fade in/out avoids clicks
constexpr float kAlarmHz = 1760.0f;
constexpr uint32_t kBeepMs = 120;
constexpr uint32_t kGapMs = 80;
constexpr uint32_t kRestMs = 500;
constexpr int kBeepsPerRound = 3;
constexpr int kRounds = 5;
constexpr uint32_t kTestMs = 200;
constexpr float kClickHz = 2600.0f;
constexpr uint32_t kClickMs = 12;
constexpr float kClickAmplitude = 6000.0f;
constexpr float kTickHz = 1320.0f;
constexpr uint32_t kTickMs = 35;
constexpr float kChimeHz[] = {1047.0f, 1319.0f, 1568.0f, 2093.0f};  // C6 E6 G6 C7
constexpr uint32_t kChimeNoteMs = 85;
constexpr uint32_t kChimeLastMs = 280;
constexpr uint32_t kAmpSettleMs = 20;
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 3;
constexpr float kTwoPi = 6.28318531f;

// Requests, as task notification bits.
constexpr uint32_t kAlarmBit = 1u << 0;
constexpr uint32_t kTestBit = 1u << 1;
constexpr uint32_t kClickBit = 1u << 2;
constexpr uint32_t kAmpBit = 1u << 3;  // clicks were switched on or off
constexpr uint32_t kTickBit = 1u << 4;
constexpr uint32_t kChimeBit = 1u << 5;

esp_codec_dev_handle_t s_speaker = nullptr;
TaskHandle_t s_task = nullptr;
volatile bool s_stop = false;
volatile uint8_t s_volume = 80;
volatile bool s_clicks = false;
bool s_amp_on = false;  // owned by the sound task
int s_applied_volume = -1;

// Writes one tone with soft edges. Returns false if stop() was called meanwhile.
bool tone(float hz, uint32_t ms, float amplitude)
{
    const uint32_t samples = kSampleRate * ms / 1000;
    const uint32_t ramp = samples / 4 < kRampSamples ? samples / 4 : kRampSamples;
    const float step = kTwoPi * hz / kSampleRate;
    int16_t chunk[256];
    float phase = 0.0f;
    for (uint32_t done = 0; done < samples;) {
        if (s_stop) {
            return false;
        }
        uint32_t n = 0;
        for (; n < sizeof(chunk) / sizeof(chunk[0]) && done < samples; ++n, ++done) {
            uint32_t edge = done < samples - done ? done : samples - done;
            float gain = edge < ramp ? static_cast<float>(edge) / ramp : 1.0f;
            chunk[n] = static_cast<int16_t>(amplitude * gain * std::sin(phase));
            phase += step;
            if (phase > kTwoPi) {
                phase -= kTwoPi;
            }
        }
        esp_codec_dev_write(s_speaker, chunk, static_cast<int>(n * sizeof(chunk[0])));
    }
    return true;
}

// Waits `ms`, returning false early if stop() is called.
bool pause_for(uint32_t ms)
{
    for (uint32_t waited = 0; waited < ms; waited += 10) {
        if (s_stop) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

void play_alarm_pattern()
{
    for (int round = 0; round < kRounds; ++round) {
        for (int i = 0; i < kBeepsPerRound; ++i) {
            if (!tone(kAlarmHz, kBeepMs, kAmplitude) || !pause_for(kGapMs)) {
                return;
            }
        }
        if (!pause_for(kRestMs)) {
            return;
        }
    }
}

void play_chime_notes()
{
    constexpr size_t kNotes = sizeof(kChimeHz) / sizeof(kChimeHz[0]);
    for (size_t i = 0; i < kNotes; ++i) {
        if (!tone(kChimeHz[i], i + 1 == kNotes ? kChimeLastMs : kChimeNoteMs, kAmplitude)) {
            return;
        }
    }
}

void set_amp(bool on)
{
    if (on == s_amp_on) {
        return;
    }
    bsp_audio_poweramp_enable(on);
    s_amp_on = on;
    if (on) {
        vTaskDelay(pdMS_TO_TICKS(kAmpSettleMs));
    }
}

void apply_volume()
{
    int volume = s_volume;
    if (volume != s_applied_volume) {
        esp_codec_dev_set_out_vol(s_speaker, volume);
        s_applied_volume = volume;
    }
}

void sound_task(void *)
{
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
        apply_volume();
        if (bits & (kAlarmBit | kTestBit)) {
            s_stop = false;
            set_amp(true);
            if (bits & kAlarmBit) {
                play_alarm_pattern();
            } else {
                tone(kAlarmHz, kTestMs, kAmplitude);
            }
        } else if (bits & (kChimeBit | kTickBit)) {
            s_stop = false;
            set_amp(true);
            if (bits & kChimeBit) {
                play_chime_notes();
            } else {
                tone(kTickHz, kTickMs, kClickAmplitude);
            }
        } else if ((bits & kClickBit) && s_clicks) {
            s_stop = false;  // a stop() aimed at an earlier alarm mustn't swallow this click
            set_amp(true);
            tone(kClickHz, kClickMs, kClickAmplitude);
        }
        // Clicks need the amplifier ready at all times; otherwise it rests between sounds.
        vTaskDelay(pdMS_TO_TICKS(kAmpSettleMs));
        set_amp(s_clicks);
    }
}

void request(uint32_t bit)
{
    if (s_task != nullptr) {
        xTaskNotify(s_task, bit, eSetBits);
    }
}

}  // namespace

bool init()
{
    s_speaker = bsp_audio_codec_speaker_init();
    if (s_speaker == nullptr) {
        ESP_LOGW(TAG, "speaker unavailable; sounds are disabled");
        return false;
    }
    esp_codec_dev_sample_info_t format = {};
    format.bits_per_sample = 16;
    format.channel = 1;
    format.sample_rate = kSampleRate;
    if (esp_codec_dev_open(s_speaker, &format) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "speaker open failed; sounds are disabled");
        s_speaker = nullptr;
        return false;
    }
    bsp_audio_poweramp_enable(false);
    xTaskCreate(sound_task, "sound", kTaskStack, nullptr, kTaskPriority, &s_task);
    ESP_LOGI(TAG, "speaker ready");
    return true;
}

void set_volume(uint8_t percent)
{
    s_volume = percent > 100 ? 100 : percent;
}

void set_clicks_enabled(bool enabled)
{
    s_clicks = enabled;
    request(kAmpBit);
}

void play_alarm()
{
    s_stop = true;  // Ends a pattern that's already playing; the task then starts over.
    request(kAlarmBit);
}

void play_test()
{
    s_stop = true;
    request(kTestBit);
}

void play_tick()
{
    request(kTickBit);
}

void play_chime()
{
    request(kChimeBit);
}

void play_click()
{
    if (s_clicks) {
        request(kClickBit);
    }
}

void stop()
{
    s_stop = true;
}

}  // namespace sound
