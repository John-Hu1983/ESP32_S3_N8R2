#include "customize/test_myself/test_self_speaker.h"

#if TEST_SPEAKER
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "audio_service.h"
#include "board.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "SpeakerSelfTest"

static void SpeakerSelfTestTask(void* arg) {
    Board& board = Board::GetInstance();
    AudioService* audio_service = static_cast<AudioService*>(arg);
    AudioCodec* codec = board.GetAudioCodec();
    if (audio_service == nullptr || codec == nullptr) {
        return;
    }

    while (!audio_service->IsInitialized()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    while (true) {
        audio_service->PlaySound(Lang::Sounds::OGG_WELCOME);
        audio_service->WaitForPlaybackQueueEmpty();
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP_LOGI(TAG, "volume = %d", codec->output_volume());
    }
}

void StartSpeakerSelfTestTask(AudioService* audio_service) {
    xTaskCreate(SpeakerSelfTestTask, "speaker_test", 4096, audio_service, 4, nullptr);
}
#endif
