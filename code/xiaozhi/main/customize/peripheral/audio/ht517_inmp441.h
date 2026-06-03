#ifndef HT517_INMP441_H
#define HT517_INMP441_H

#include "audio/codecs/no_audio_codec.h"

#include <vector>

class Ht517Inmp441AudioCodec : public NoAudioCodec {
public:
    Ht517Inmp441AudioCodec(int input_sample_rate, int output_sample_rate, gpio_num_t bclk,
                           gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    void OutputData(std::vector<int16_t>& data) override;
    void EnableInput(bool enable) override;

protected:
    int Read(int16_t* dest, int samples) override;

private:
    bool force_tx_clock_ = false;
};

#endif  // HT517_INMP441_H
