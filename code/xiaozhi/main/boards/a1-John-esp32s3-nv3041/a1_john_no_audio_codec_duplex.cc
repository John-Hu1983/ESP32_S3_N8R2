#include "a1_john_no_audio_codec_duplex.h"

#include "board.h"
#include "display/display.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#define TAG "A1JohnNoAudio"

int A1JohnNoAudioCodecDuplex::Read(int16_t* dest, int samples) {
    size_t bytes_read = 0;
    constexpr uint32_t kReadTimeoutMs = 200;
    if (i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read,
                         kReadTimeoutMs) != ESP_OK) {
        return 0;
    }
    return bytes_read / sizeof(int16_t);
}

A1JohnNoAudioCodecDuplex::A1JohnNoAudioCodecDuplex(int input_sample_rate, int output_sample_rate,
                                                   gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,
                                                   gpio_num_t din) {
    duplex_ = true;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_channels_ = 1;
    output_channels_ = 2;  // Use stereo frame for HT517

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    // Use stereo 16-bit slots (32fs); output data on both channels
    i2s_std_config_t tx_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol = false,
                .bit_shift = true,
            },
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                     .bclk = bclk,
                     .ws = ws,
                     .dout = dout,
                     .din = din,
                     .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    i2s_std_config_t rx_cfg = tx_cfg;
    rx_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_;
    rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &tx_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &rx_cfg));
    ESP_LOGI(TAG, "🔊 立体声 I2S 配置完成 (HT517 专用)");
}

void A1JohnNoAudioCodecDuplex::OutputData(std::vector<int16_t>& data) {
    if (data.empty())
        return;

    // output_volume_ is inherited from AudioCodec (0-100) and set via SetOutputVolume.
    // We apply a squared curve to make low volumes more usable.
    float volume = static_cast<float>(output_volume_) / 100.0f;
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    float gain = volume * volume;

    std::vector<int16_t> interleaved;
    interleaved.reserve(data.size() * 2);
    for (int16_t sample : data) {
        int32_t scaled = static_cast<int32_t>(sample * gain);
        if (scaled > std::numeric_limits<int16_t>::max()) {
            scaled = std::numeric_limits<int16_t>::max();
        } else if (scaled < std::numeric_limits<int16_t>::min()) {
            scaled = std::numeric_limits<int16_t>::min();
        }
        int16_t out = static_cast<int16_t>(scaled);
        interleaved.push_back(out);
        interleaved.push_back(out);
    }

    size_t bytes_written = 0;
    i2s_channel_write(tx_handle_, interleaved.data(), interleaved.size() * sizeof(int16_t),
                      &bytes_written, portMAX_DELAY);
}

void A1JohnNoAudioCodecDuplex::SelfTestMic(int duration_ms, int samples_per_frame,
                                           bool show_waveform) {
    if (duration_ms <= 0 || samples_per_frame <= 0) {
        ESP_LOGW(TAG, "SelfTestMic skipped: invalid duration or frame size");
        return;
    }

    bool was_input_enabled = input_enabled_;
    EnableInput(true);

    std::vector<int16_t> samples(static_cast<size_t>(samples_per_frame));

    Display* display = nullptr;
#if HAVE_LVGL
    lv_obj_t* chart = nullptr;
    lv_chart_series_t* series = nullptr;
    lv_obj_t* label = nullptr;
    const int point_count = 128;

    if (show_waveform) {
        display = Board::GetInstance().GetDisplay();
        if (display != nullptr) {
            DisplayLockGuard lock(display);
            lv_obj_t* screen = lv_scr_act();

#if defined(LV_USE_CHART) && LV_USE_CHART
            chart = lv_chart_create(screen);
            lv_obj_set_size(chart, display->width(), display->height() * 2 / 3);
            lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
            lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -32768, 32767);
            lv_chart_set_point_count(chart, point_count);
            series = lv_chart_add_series(chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);
#endif

            label = lv_label_create(screen);
            lv_label_set_text(label, "MIC SELF-TEST");
            lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 6);
        }
    }
#else
    (void)show_waveform;
#endif

    int64_t end_time_us = esp_timer_get_time() + static_cast<int64_t>(duration_ms) * 1000;
    while (esp_timer_get_time() < end_time_us) {
        int read_count = Read(samples.data(), samples_per_frame);
        if (read_count <= 0) {
            ESP_LOGW(TAG, "SelfTestMic: read timeout");
            continue;
        }

        int16_t min_value = samples[0];
        int16_t max_value = samples[0];
        int64_t sum_abs = 0;
        for (int i = 0; i < read_count; ++i) {
            int16_t value = samples[static_cast<size_t>(i)];
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
        }
        int avg_abs = static_cast<int>(sum_abs / read_count);

        char sample_text[192] = {0};
        int offset = 0;
        int log_count = std::min(read_count, 16);
        for (int i = 0; i < log_count; ++i) {
            int written = snprintf(sample_text + offset, sizeof(sample_text) - offset,
                                   i == 0 ? "%d" : " %d", samples[static_cast<size_t>(i)]);
            if (written < 0 || written >= static_cast<int>(sizeof(sample_text) - offset)) {
                break;
            }
            offset += written;
        }

        ESP_LOGI(TAG, "SelfTestMic: read=%d min=%d max=%d avg_abs=%d samples=%s", read_count,
                 min_value, max_value, avg_abs, sample_text);

#if HAVE_LVGL
#if defined(LV_USE_CHART) && LV_USE_CHART
        if (chart != nullptr && series != nullptr && display != nullptr) {
            DisplayLockGuard lock(display);
            int step = read_count / point_count;
            if (step < 1) {
                step = 1;
            }
            for (int i = 0; i < point_count; ++i) {
                int index = i * step;
                if (index >= read_count) {
                    index = read_count - 1;
                }
                lv_chart_set_value_by_id(chart, series, i, samples[static_cast<size_t>(index)]);
            }
            lv_chart_refresh(chart);
        }
#endif
#endif
    }

#if HAVE_LVGL
    if (display != nullptr) {
        DisplayLockGuard lock(display);
        if (label != nullptr) {
            lv_obj_del(label);
        }
        if (chart != nullptr) {
            lv_obj_del(chart);
        }
    }
#endif

    if (!was_input_enabled) {
        EnableInput(false);
    }
}
