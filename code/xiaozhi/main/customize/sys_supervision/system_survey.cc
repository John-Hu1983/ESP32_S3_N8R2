#include "customize/sys_supervision/system_survey.h"

#include "application.h"
#include "audio_codec.h"
#include "board.h"

#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if SYSTEM_SUPERVISION_ENABLED
#define TAG "SystemSurvey"
#define SYSTEM_SURVEY_PERIOD_MS 500

static const char* DeviceStateName(DeviceState state) {
    switch (state) {
        case kDeviceStateUnknown:
            return "unknown";
        case kDeviceStateStarting:
            return "starting";
        case kDeviceStateWifiConfiguring:
            return "wifi_config";
        case kDeviceStateIdle:
            return "idle";
        case kDeviceStateConnecting:
            return "connecting";
        case kDeviceStateListening:
            return "listening";
        case kDeviceStateSpeaking:
            return "speaking";
        case kDeviceStateUpgrading:
            return "upgrading";
        case kDeviceStateActivating:
            return "activating";
        case kDeviceStateAudioTesting:
            return "audio_test";
        case kDeviceStateFatalError:
            return "fatal";
        default:
            return "unknown";
    }
}

static void SystemSurveyTask(void* arg) {
    (void)arg;
    AudioService* audio_service = &Application::GetInstance().GetAudioService();

    while (!audio_service->IsInitialized()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    int last_vad = -1;
    int last_wake = -1;
    int last_afe = -1;
    int last_proc = -1;
    DeviceState last_state = kDeviceStateUnknown;
    bool auto_start_listening = false;

    while (true) {
        AudioCodec* codec = Board::GetInstance().GetAudioCodec();
        if (codec != nullptr) {
            DeviceState state = Application::GetInstance().GetDeviceState();
            int vad = audio_service->IsVoiceDetected() ? 1 : 0;
            int wake = audio_service->IsWakeWordRunning() ? 1 : 0;
            int afe = audio_service->IsAfeWakeWord() ? 1 : 0;
            int proc = audio_service->IsAudioProcessorRunning() ? 1 : 0;

            if (!auto_start_listening && state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "Auto start listening");
                Application::GetInstance().StartListening();
                auto_start_listening = true;
            }

            if (vad != last_vad) {
                ESP_LOGI(TAG, "VAD: %s", vad ? "speaking" : "silence");
                last_vad = vad;
            }
            if (wake != last_wake || afe != last_afe) {
                ESP_LOGI(TAG, "WAKE: running=%d afe=%d", wake, afe);
                last_wake = wake;
                last_afe = afe;
            }
            if (proc != last_proc || state != last_state) {
                ESP_LOGI(TAG, "STATE: %s proc=%d", DeviceStateName(state), proc);
                last_proc = proc;
                last_state = state;
            }

            ESP_LOGI(
                TAG,
                "MIC: enabled=%d rate=%d ch=%d gain=%.2f ref=%d duplex=%d vad=%d wake=%d afe=%d state=%s proc=%d",
                codec->input_enabled(), codec->input_sample_rate(), codec->input_channels(),
                static_cast<double>(codec->input_gain()), codec->input_reference(), codec->duplex(),
                vad, wake, afe, DeviceStateName(state), proc);
        } else {
            ESP_LOGW(TAG, "MIC: codec not ready");
        }

        vTaskDelay(pdMS_TO_TICKS(SYSTEM_SURVEY_PERIOD_MS));
    }
}

void StartSystemSurveyTask() {
    Application::GetInstance().SetIncomingJsonObserver([](const cJSON* root) {
        char* json_text = cJSON_PrintUnformatted(root);
        if (json_text != nullptr) {
            ESP_LOGI(TAG, "Cloud JSON: %s", json_text);
            cJSON_free(json_text);
        }

        const cJSON* type = cJSON_GetObjectItem(root, "type");
        if (cJSON_IsString(type) && strcmp(type->valuestring, "stt") == 0) {
            const cJSON* text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                ESP_LOGI(TAG, "STT: %s", text->valuestring);
            }
        }
    });
    xTaskCreate(SystemSurveyTask, "system_survey", 4096, nullptr, 2, nullptr);
}
#else
void StartSystemSurveyTask() {}
#endif
