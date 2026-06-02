#pragma once

#include "codecs/no_audio_codec.h"

#include <cstdint>
#include <vector>

class A1JohnNoAudioCodecDuplex : public NoAudioCodec {
public:
    A1JohnNoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk,
                             gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    void OutputData(std::vector<int16_t>& data) override;
    void SelfTestMic(int duration_ms = 2000, int samples_per_frame = 256, bool show_waveform = true);

protected:
    int Read(int16_t* dest, int samples) override;
};
