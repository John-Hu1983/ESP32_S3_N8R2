#include "customize/application/user_main.h"
#include "customize/peripheral/g_sensor/sc7a20htr.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "UserMain"
#define USER_MAIN_DISABLE_ACCEL 0

static void UserMainTask(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "User main task started");

#if USER_MAIN_DISABLE_ACCEL
    ESP_LOGW(TAG, "Accelerometer disabled (temporary)");
    while (true) {
        ESP_LOGI(TAG, "PSRAM total: %d bytes", esp_psram_get_size());
        ESP_LOGI(TAG, "PSRAM free : %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "PSRAM largest: %d bytes",
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
#endif

    if (sc7a20htr_init_device() < 0) {
        vTaskDelete(nullptr);
        return;
    }

    for (;;) {
        sc7a20htr_read_elementary();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void StartUserMainTask() { xTaskCreate(UserMainTask, "user_main", 4096, nullptr, 5, nullptr); }
