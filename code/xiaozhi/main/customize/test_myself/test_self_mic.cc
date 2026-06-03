#include "customize/test_myself/test_self_mic.h"

#if TEST_MIC
#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <vector>

#define TAG "MicSelfTest"

static void MicSelfTestTask(void* arg) {
    (void)arg;
    AudioService* audio_service = &Application::GetInstance().GetAudioService();
#if HAVE_LVGL
    Display* display = Board::GetInstance().GetDisplay();
    while (!audio_service->IsInitialized() || (display != nullptr && !display->IsSetupUICalled())) {
        vTaskDelay(pdMS_TO_TICKS(50));
        display = Board::GetInstance().GetDisplay();
    }
#else
    while (!audio_service->IsInitialized()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
#endif

    AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        return;
    }

    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Mic raw logging task start");

    int sample_rate = codec->input_sample_rate();
    const int samples_per_read = 256;
    const int kWaveUpdateDelayMs = 50;
    const int kLogEveryN = 10;
    std::vector<int16_t> samples;

#if HAVE_LVGL
    lv_obj_t* canvas = nullptr;
    lv_obj_t* label = nullptr;
    lv_color_t* canvas_buf = nullptr;
    static const int kPointCount = 160;
    static lv_point_t points[kPointCount];
    static int16_t wave_buffer[kPointCount];
    int write_index = 0;
    int32_t scale_peak = 1;
    int plot_width = 0;
    int plot_height = 0;
    int plot_x = 0;
    int plot_y = 0;
    int log_counter = 0;

    if (display != nullptr) {
        DisplayLockGuard lock(display);
        lv_obj_t* screen = lv_scr_act();
        plot_width = display->width();
        plot_height = display->height() * 2 / 3;
        plot_x = 0;
        plot_y = (display->height() - plot_height) / 2;

        for (int i = 0; i < kPointCount; ++i) {
            points[i].x = 0;
            points[i].y = 0;
            wave_buffer[i] = 0;
        }

        canvas = lv_canvas_create(screen);
        lv_obj_set_pos(canvas, plot_x, plot_y);
        lv_obj_set_size(canvas, plot_width, plot_height);
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

        size_t buf_size = LV_CANVAS_BUF_SIZE(plot_width, plot_height, 16, LV_DRAW_BUF_STRIDE_ALIGN);
        canvas_buf = static_cast<lv_color_t*>(
            heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (canvas_buf == nullptr) {
            canvas_buf = static_cast<lv_color_t*>(
                heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        }
        if (canvas_buf == nullptr) {
            canvas_buf = static_cast<lv_color_t*>(heap_caps_malloc(buf_size, MALLOC_CAP_8BIT));
        }
        if (canvas_buf != nullptr) {
            lv_canvas_set_buffer(canvas, canvas_buf, plot_width, plot_height,
                                 LV_COLOR_FORMAT_RGB565);
            lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
        } else {
            ESP_LOGW(TAG, "Mic canvas buffer alloc failed");
            canvas = nullptr;
        }

        label = lv_label_create(screen);
        lv_label_set_text(label, "MIC WAVEFORM");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
    }
#endif

    while (true) {
        if (audio_service->ReadAudioData(samples, sample_rate, samples_per_read)) {
            if (samples.empty()) {
                vTaskDelay(pdMS_TO_TICKS(kWaveUpdateDelayMs));
                continue;
            }
            const int16_t* sample_data = samples.data();
            size_t sample_count = samples.size();
            int16_t min_value = sample_data[0];
            int16_t max_value = sample_data[0];
            int64_t sum_abs = 0;
            int32_t max_abs = 1;
            for (size_t i = 0; i < sample_count; ++i) {
                int16_t value = sample_data[i];
                if (value < min_value) {
                    min_value = value;
                }
                if (value > max_value) {
                    max_value = value;
                }
                int32_t abs_value = value;
                if (abs_value < 0) {
                    abs_value = -abs_value;
                }
                sum_abs += abs_value;
                if (abs_value > max_abs) {
                    max_abs = abs_value;
                }
            }
            int avg_abs = static_cast<int>(sum_abs / sample_count);

            char sample_text[192] = {0};
            int offset = 0;
            int log_count = sample_count < 16 ? static_cast<int>(sample_count) : 16;
            for (int i = 0; i < log_count; ++i) {
                int written = snprintf(sample_text + offset, sizeof(sample_text) - offset,
                                       i == 0 ? "%d" : " %d", sample_data[static_cast<size_t>(i)]);
                if (written < 0 || written >= static_cast<int>(sizeof(sample_text) - offset)) {
                    break;
                }
                offset += written;
            }

            if ((log_counter++ % kLogEveryN) == 0) {
                ESP_LOGI(TAG, "MIC: min=%d max=%d avg_abs=%d samples=%s", min_value, max_value,
                         avg_abs, sample_text);
            }

#if HAVE_LVGL
            if (display != nullptr && canvas != nullptr && plot_width > 0 && plot_height > 0) {
                if (max_abs > scale_peak) {
                    scale_peak = max_abs;
                } else {
                    scale_peak = (scale_peak * 9 + max_abs) / 10;
                    if (scale_peak < 1) {
                        scale_peak = 1;
                    }
                }

                wave_buffer[write_index] = static_cast<int16_t>(avg_abs);
                write_index = (write_index + 1) % kPointCount;

                float scale =
                    static_cast<float>(plot_height / 2 - 1) / static_cast<float>(scale_peak);
                for (int i = 0; i < kPointCount; ++i) {
                    int index = write_index + i;
                    if (index >= kPointCount) {
                        index -= kPointCount;
                    }
                    int x = (kPointCount <= 1) ? 0 : (i * (plot_width - 1)) / (kPointCount - 1);
                    int y = plot_height / 2 -
                            static_cast<int>(wave_buffer[static_cast<size_t>(index)] * scale);
                    if (y < 0) {
                        y = 0;
                    } else if (y >= plot_height) {
                        y = plot_height - 1;
                    }
                    points[static_cast<size_t>(i)].x = static_cast<lv_coord_t>(x);
                    points[static_cast<size_t>(i)].y = static_cast<lv_coord_t>(y);
                }
                DisplayLockGuard lock(display);
                lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

                lv_layer_t layer;
                lv_canvas_init_layer(canvas, &layer);

                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.width = 2;
                line_dsc.color = lv_color_hex(0x00FF00);

                for (int i = 1; i < kPointCount; ++i) {
                    line_dsc.p1.x = points[static_cast<size_t>(i - 1)].x;
                    line_dsc.p1.y = points[static_cast<size_t>(i - 1)].y;
                    line_dsc.p2.x = points[static_cast<size_t>(i)].x;
                    line_dsc.p2.y = points[static_cast<size_t>(i)].y;
                    lv_draw_line(&layer, &line_dsc);
                }

                lv_canvas_finish_layer(canvas, &layer);
                lv_obj_invalidate(canvas);
            }
#endif
        } else {
            ESP_LOGW(TAG, "MIC: read timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(kWaveUpdateDelayMs));
    }
}

void StartMicSelfTestTask() {
    xTaskCreate(MicSelfTestTask, "mic_self_test", 4096, nullptr, 3, nullptr);
}
#endif
