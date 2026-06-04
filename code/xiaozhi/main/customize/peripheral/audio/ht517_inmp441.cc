#include "ht517_inmp441.h"

#include <driver/i2s_std.h>
#include <esp_err.h>
#include <esp_log.h>

#include <limits>

#define TAG "HT517_INMP441"

Ht517Inmp441AudioCodec::Ht517Inmp441AudioCodec(int input_sample_rate, int output_sample_rate, gpio_num_t bclk,
                                               gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
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

    // Use stereo 32-bit slots (64fs); output data on both channels
    i2s_std_config_t tx_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
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
    rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    rx_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    rx_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    rx_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &tx_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &rx_cfg));
    SetInputGain(3.0f);
    ESP_LOGI(TAG, "Stereo I2S configured (HT517)");
}

void Ht517Inmp441AudioCodec::OutputData(std::vector<int16_t>& data) {
    if (data.empty()) {
        return;
    }

    float volume = static_cast<float>(output_volume_) / 100.0f;
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    float gain = volume * volume;

    std::vector<int32_t> interleaved;
    interleaved.reserve(data.size() * 2);
    for (int16_t sample : data) {
        int32_t scaled = static_cast<int32_t>(sample * gain);
        if (scaled > std::numeric_limits<int16_t>::max()) {
            scaled = std::numeric_limits<int16_t>::max();
        } else if (scaled < std::numeric_limits<int16_t>::min()) {
            scaled = std::numeric_limits<int16_t>::min();
        }
        int16_t out16 = static_cast<int16_t>(scaled);
        uint32_t out32u = static_cast<uint32_t>(static_cast<uint16_t>(out16)) << 16;
        int32_t out32 = static_cast<int32_t>(out32u);
        interleaved.push_back(out32);
        interleaved.push_back(out32);
    }

    size_t bytes_written = 0;
    i2s_channel_write(tx_handle_, interleaved.data(), interleaved.size() * sizeof(int32_t),
                      &bytes_written, portMAX_DELAY);
}

void Ht517Inmp441AudioCodec::EnableInput(bool enable) {
    if (enable && !output_enabled_) {
        force_tx_clock_ = true;
        NoAudioCodec::EnableOutput(true);
    }

    NoAudioCodec::EnableInput(enable);

    if (!enable && force_tx_clock_) {
        force_tx_clock_ = false;
        NoAudioCodec::EnableOutput(false);
    }
}

int Ht517Inmp441AudioCodec::Read(int16_t* dest, int samples) {
    if (dest == nullptr || samples <= 0) {
        return 0;
    }

    int raw_samples = samples * 2;
    std::vector<int32_t> raw(static_cast<size_t>(raw_samples));
    size_t bytes_read = 0;
    constexpr uint32_t kReadTimeoutMs = 200;
    esp_err_t err = i2s_channel_read(rx_handle_, raw.data(), raw_samples * sizeof(int32_t),
                                     &bytes_read, kReadTimeoutMs);
    if (err != ESP_OK) {
        return 0;
    }

    constexpr int kMicShift = 10;
    float gain = input_gain_;
    int read_samples = static_cast<int>(bytes_read / sizeof(int32_t));
    int frames = read_samples / 2;
    if (frames <= 0) {
        return 0;
    }

    // INMP441 L/R is tied to GND, so use left channel.
    constexpr int channel_offset = 0;
    for (int i = 0; i < frames; ++i) {
        int32_t sample = raw[static_cast<size_t>(i) * 2 + channel_offset] >> kMicShift;
        if (gain > 0.0f && gain != 1.0f) {
            sample = static_cast<int32_t>(sample * gain);
        }
        if (sample > std::numeric_limits<int16_t>::max()) {
            sample = std::numeric_limits<int16_t>::max();
        } else if (sample < std::numeric_limits<int16_t>::min()) {
            sample = std::numeric_limits<int16_t>::min();
        }
        int16_t out = static_cast<int16_t>(sample);
        dest[i] = out;
    }

    return frames;
}
