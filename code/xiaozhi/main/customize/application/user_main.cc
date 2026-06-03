#include "customize/application/user_main.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr char kUserMainTag[] = "UserMain";

void UserMainTask(void* arg) {
    (void)arg;
    ESP_LOGI(kUserMainTag, "User main task started");
    int16_t counter = 0;
    for (;;) {
        // TODO: Add your custom logic or create more tasks here.
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(kUserMainTag, "Counter: %d", counter++);
    }
}
}  // namespace

void StartUserMainTask() { xTaskCreate(UserMainTask, "user_main", 4096, nullptr, 5, nullptr); }
