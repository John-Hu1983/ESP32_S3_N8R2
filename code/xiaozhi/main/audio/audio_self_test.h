#ifndef AUDIO_SELF_TEST_H
#define AUDIO_SELF_TEST_H

#include <cstdint>

class AudioService;
class Display;

class AudioSelfTest {
public:
    struct Config {
        int duration_ms = 6000;
        int sample_rate = 16000;
        int samples_per_chunk = 160; // 10 ms at 16 kHz
        int chart_points = 128;
        bool enable_speaker_test = true;
        int speaker_frequency_hz = 1000;
        int speaker_duration_ms = 800;
        int speaker_amplitude = 12000;
    };

    static void Start(AudioService* audio_service, Display* display, const Config& config);
};

#endif // AUDIO_SELF_TEST_H
