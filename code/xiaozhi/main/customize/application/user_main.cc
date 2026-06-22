#include "customize/application/user_main.h"
#include "boards/John_AI_box/config.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "customize/peripheral/g_sensor/sc7a20htr.h"

#define TAG "UserMain"
#define USER_SHOW_PSRAM_STATS 0
#define USER_MAIN_DISABLE_ACCEL 0
#define PULSE_INDUCTOR_METAL 1
#define PULSE_INDUCTOR_DELAY_US 130

#if USER_SHOW_PSRAM_STATS
/*
brief   : Periodically print PSRAM capacity and free-space information.
input   : FreeRTOS task argument, unused.
output  : None; this task runs until deleted.
*/
static void task_show_psram_stats(void* arg) {
    (void)arg;
    while (true) {
        ESP_LOGI(TAG, "PSRAM total: %d bytes", esp_psram_get_size());
        ESP_LOGI(TAG, "PSRAM free : %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "PSRAM largest: %d bytes",
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif

#if USER_MAIN_DISABLE_ACCEL
/*
brief   : Initialize the SC7A20HTR accelerometer and periodically read sample data.
input   : FreeRTOS task argument, unused.
output  : None; the task deletes itself if initialization fails.
*/
static void task_accelerate_sc7a20htr(void* arg) {
    (void)arg;
    if (sc7a20htr_init_device() < 0) {
        ESP_LOGE(TAG, "Failed to initialize SC7A20HTR accelerometer");
        vTaskDelete(nullptr);
        return;
    }
    while (true) {
        sc7a20htr_read_elementary();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
#endif

#if PULSE_INDUCTOR_METAL
static void task_pulse_inductor(void* arg) {
    (void)arg;
    gpio_config_t io_cfg = {};
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << PULSE_INDUCTOR_GPIO;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    while (true) {
        gpio_set_level(PULSE_INDUCTOR_GPIO, 1);
        esp_rom_delay_us(PULSE_INDUCTOR_DELAY_US);
        gpio_set_level(PULSE_INDUCTOR_GPIO, 0);
        esp_rom_delay_us(PULSE_INDUCTOR_DELAY_US);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
#endif

/*
brief   : Start optional user customization tasks enabled by compile-time switches.
input   : None.
output  : None.
*/
void StartUserMainTask(void) {
#if USER_SHOW_PSRAM_STATS
    xTaskCreate(task_show_psram_stats, "show_psram_stats", 4096, nullptr, 5, nullptr);
#endif

#if USER_MAIN_DISABLE_ACCEL
    xTaskCreate(task_accelerate_sc7a20htr, "accel_sc7a20htr", 4096, nullptr, 5, nullptr);
#endif

#if PULSE_INDUCTOR_METAL
    xTaskCreate(task_pulse_inductor, "pulse_inductor", 4096, nullptr, 5, nullptr);
#endif
}
