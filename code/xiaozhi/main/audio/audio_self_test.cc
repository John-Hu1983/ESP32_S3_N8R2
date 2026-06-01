#include "audio_self_test.h"

#include "audio_service.h"
#include "display.h"
#include "lvgl_display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>
#include <vector>

#define TAG "AudioSelfTest"

namespace {
struct AudioSelfTestContext {
    AudioService* audio_service = nullptr;
    Display* display = nullptr;
    int duration_ms = 0;
    int sample_rate = 0;
    int samples_per_chunk = 0;
    int chart_points = 0;
    lv_obj_t* container = nullptr;
    lv_obj_t* plot = nullptr;
    lv_obj_t* line = nullptr;
    std::vector<lv_point_precise_t> points;
};

int16_t ClampToRange(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) {
        return static_cast<int16_t>(min_value);
    }
    if (value > max_value) {
        return static_cast<int16_t>(max_value);
    }
    return static_cast<int16_t>(value);
}

void PlaySpeakerTest(AudioService* audio_service, const AudioSelfTest::Config& config) {
    if (audio_service == nullptr || !config.enable_speaker_test) {
        return;
    }

    int sample_rate = audio_service->GetOutputSampleRate();
    if (sample_rate <= 0) {
        return;
    }

    int frequency_hz = std::max(1, config.speaker_frequency_hz);
    int duration_ms = std::max(1, config.speaker_duration_ms);
    int total_samples = static_cast<int>((static_cast<int64_t>(sample_rate) * duration_ms) / 1000);
    if (total_samples <= 0) {
        return;
    }

    int half_period = std::max(1, sample_rate / (frequency_hz * 2));
    int16_t amplitude = ClampToRange(config.speaker_amplitude, 0, 30000);
    if (amplitude == 0) {
        return;
    }

    std::vector<int16_t> pcm(total_samples);
    for (int i = 0; i < total_samples; ++i) {
        bool high = ((i / half_period) % 2) == 0;
        pcm[i] = high ? amplitude : static_cast<int16_t>(-amplitude);
    }

    ESP_LOGI(TAG, "Speaker test: %d Hz, %d ms, %d samples", frequency_hz, duration_ms, total_samples);
    audio_service->PlayPcm(std::move(pcm));
}

void CreateWaveformUi(AudioSelfTestContext* ctx) {
    auto* lvgl_display = dynamic_cast<LvglDisplay*>(ctx->display);
    if (lvgl_display == nullptr) {
        return;
    }

    DisplayLockGuard lock(ctx->display);

    lv_obj_t* screen = lv_screen_active();
    ctx->container = lv_obj_create(screen);
    lv_obj_set_size(ctx->container, LV_HOR_RES, LV_VER_RES / 3);
    lv_obj_align(ctx->container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(ctx->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ctx->container, 6, 0);
    lv_obj_set_style_pad_row(ctx->container, 4, 0);
    lv_obj_set_style_border_width(ctx->container, 0, 0);
    lv_obj_set_style_bg_color(ctx->container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->container, LV_OPA_80, 0);

    lv_obj_t* title = lv_label_create(ctx->container);
    lv_label_set_text(title, "MIC TEST");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    ctx->plot = lv_obj_create(ctx->container);
    lv_obj_set_width(ctx->plot, LV_HOR_RES);
    lv_obj_set_flex_grow(ctx->plot, 1);
    lv_obj_set_style_bg_opa(ctx->plot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->plot, 0, 0);
    lv_obj_set_style_pad_all(ctx->plot, 0, 0);

    ctx->line = lv_line_create(ctx->plot);
    lv_obj_set_size(ctx->line, LV_HOR_RES, LV_VER_RES / 3);
    lv_obj_set_style_line_color(ctx->line, lv_color_hex(0x00FF7F), 0);
    lv_obj_set_style_line_width(ctx->line, 2, 0);
    lv_obj_set_style_line_rounded(ctx->line, true, 0);

    ctx->points.assign(ctx->chart_points, lv_point_precise_t{0, 0});
    int width = lv_obj_get_width(ctx->plot);
    int height = lv_obj_get_height(ctx->plot);
    int mid_y = height / 2;
    for (int i = 0; i < ctx->chart_points; ++i) {
        int x = (ctx->chart_points > 1) ? (i * (width - 1) / (ctx->chart_points - 1)) : 0;
        ctx->points[i].x = x;
        ctx->points[i].y = mid_y;
    }
    lv_line_set_points(ctx->line, ctx->points.data(), ctx->points.size());
}

void DestroyWaveformUi(AudioSelfTestContext* ctx) {
    if (ctx->container == nullptr) {
        return;
    }
    DisplayLockGuard lock(ctx->display);
    lv_obj_del(ctx->container);
    ctx->container = nullptr;
    ctx->plot = nullptr;
    ctx->line = nullptr;
    ctx->points.clear();
}

void UpdateWaveform(AudioSelfTestContext* ctx, const std::vector<int16_t>& data) {
    if (ctx->plot == nullptr || ctx->line == nullptr) {
        return;
    }

    int frames = static_cast<int>(data.size());
    if (frames <= 0) {
        return;
    }

    int step = std::max(1, frames / ctx->chart_points);

    DisplayLockGuard lock(ctx->display);
    int width = lv_obj_get_width(ctx->plot);
    int height = lv_obj_get_height(ctx->plot);
    int mid_y = height / 2;
    int amplitude = std::max(1, height / 2 - 2);

    for (int i = 0; i < ctx->chart_points; ++i) {
        int idx = i * step;
        if (idx >= frames) {
            idx = frames - 1;
        }
        int32_t sample = data[idx];
        int32_t scaled = sample / 256; // Map 16-bit PCM into roughly -128..127
        int16_t value = ClampToRange(scaled, -128, 127);
        int x = (ctx->chart_points > 1) ? (i * (width - 1) / (ctx->chart_points - 1)) : 0;
        int y = mid_y - (value * amplitude / 128);
        if (y < 0) {
            y = 0;
        } else if (y > height - 1) {
            y = height - 1;
        }
        ctx->points[i].x = x;
        ctx->points[i].y = y;
    }
    lv_line_set_points(ctx->line, ctx->points.data(), ctx->points.size());
}

void AudioSelfTestTask(void* arg) {
    auto* ctx = static_cast<AudioSelfTestContext*>(arg);
    CreateWaveformUi(ctx);

    int64_t start_us = esp_timer_get_time();
    int64_t duration_us = static_cast<int64_t>(ctx->duration_ms) * 1000;

    while (esp_timer_get_time() - start_us < duration_us) {
        if (ctx->audio_service->IsWakeWordRunning() || ctx->audio_service->IsAudioProcessorRunning()) {
            break;
        }
        std::vector<int16_t> data;
        if (ctx->audio_service->ReadAudioData(data, ctx->sample_rate, ctx->samples_per_chunk)) {
            UpdateWaveform(ctx, data);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    DestroyWaveformUi(ctx);
    delete ctx;
    vTaskDelete(NULL);
}
} // namespace

void AudioSelfTest::Start(AudioService* audio_service, Display* display, const Config& config) {
    if (audio_service == nullptr || display == nullptr) {
        return;
    }
    if (dynamic_cast<LvglDisplay*>(display) == nullptr) {
        return;
    }

    PlaySpeakerTest(audio_service, config);

    auto* ctx = new AudioSelfTestContext();
    ctx->audio_service = audio_service;
    ctx->display = display;
    ctx->duration_ms = config.duration_ms;
    ctx->sample_rate = config.sample_rate;
    ctx->samples_per_chunk = config.samples_per_chunk;
    ctx->chart_points = config.chart_points;

    xTaskCreate(AudioSelfTestTask, "audio_self_test", 4096, ctx, 4, nullptr);
}
