#include "customize/application/user_main.h"
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "boards/John_AI_box/config.h"
#include "customize/peripheral/g_sensor/sc7a20htr.h"

#define TAG "UserMain"
#define USER_SHOW_PSRAM_STATS 0
#define USER_MAIN_DISABLE_ACCEL 0
#define PULSE_INDUCTOR_METAL 1
#define PULSE_INDUCTOR_HIGH_US 130
#define PULSE_INDUCTOR_LOW_US 1500
#define PULSE_ADC_UNIT ADC_UNIT_1
#define PULSE_ADC_CHANNEL ADC_CHANNEL_3
#define PULSE_ADC_BURST_SAMPLES 10

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
static volatile int s_pulse_adc_avg_raw = 0;
static TaskHandle_t s_pulse_adc_task = nullptr;
static gptimer_handle_t s_pulse_timer = nullptr;
static volatile bool s_pulse_high_phase = false;

static bool IRAM_ATTR pulse_timer_on_alarm_cb(gptimer_handle_t timer,
                                              const gptimer_alarm_event_data_t* edata,
                                              void* user_ctx) {
    (void)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;

    gptimer_alarm_config_t next_alarm = {
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = false,
        },
    };

    if (s_pulse_high_phase) {
        gpio_set_level(PULSE_INDUCTOR_GPIO, 0);
        s_pulse_high_phase = false;
        next_alarm.alarm_count = edata->count_value + PULSE_INDUCTOR_LOW_US;

        if (s_pulse_adc_task != nullptr) {
            vTaskNotifyGiveFromISR(s_pulse_adc_task, &high_task_wakeup);
        }
    } else {
        gpio_set_level(PULSE_INDUCTOR_GPIO, 1);
        s_pulse_high_phase = true;
        next_alarm.alarm_count = edata->count_value + PULSE_INDUCTOR_HIGH_US;
    }

    gptimer_set_alarm_action(timer, &next_alarm);
    return high_task_wakeup == pdTRUE;
}

static void task_pulse_adc_reader(void* arg) {
    (void)arg;

    adc_oneshot_unit_handle_t adc_handle = nullptr;
    adc_oneshot_unit_init_cfg_t adc_unit_cfg = {
        .unit_id = PULSE_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t adc_channel_cfg = {};
    adc_channel_cfg.atten = ADC_ATTEN_DB_12;
    adc_channel_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, PULSE_ADC_CHANNEL, &adc_channel_cfg));

    int print_space = 0;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int adc_sum = 0;
        int adc_count = 0;
        for (int i = 0; i < PULSE_ADC_BURST_SAMPLES; ++i) {
            int adc_raw = 0;
            esp_err_t err = adc_oneshot_read(adc_handle, PULSE_ADC_CHANNEL, &adc_raw);
            if (err != ESP_OK) {
                continue;
            }
            adc_sum += adc_raw;
            ++adc_count;
        }

        if (adc_count > 0) {
            s_pulse_adc_avg_raw = adc_sum / adc_count;
        }

        if (print_space++ > 100) {
            print_space = 0;
            ESP_LOGI(TAG, "Pulse inductor ADC average raw value: %d", s_pulse_adc_avg_raw);
        }
    }
}

static void task_pulse_inductor(void* arg) {
    (void)arg;
    gpio_config_t io_cfg = {};
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << PULSE_INDUCTOR_GPIO;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    ESP_ERROR_CHECK(gpio_set_level(PULSE_INDUCTOR_GPIO, 0));

    xTaskCreate(task_pulse_adc_reader, "pulse_adc_reader", 4096, nullptr, 6, &s_pulse_adc_task);

    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &s_pulse_timer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = pulse_timer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_pulse_timer, &cbs, nullptr));
    ESP_ERROR_CHECK(gptimer_enable(s_pulse_timer));

    s_pulse_high_phase = true;
    ESP_ERROR_CHECK(gpio_set_level(PULSE_INDUCTOR_GPIO, 1));

    gptimer_alarm_config_t first_alarm = {
        .alarm_count = PULSE_INDUCTOR_HIGH_US,
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = false,
        },
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_pulse_timer, &first_alarm));
    ESP_ERROR_CHECK(gptimer_start(s_pulse_timer));

    vTaskDelete(nullptr);
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
