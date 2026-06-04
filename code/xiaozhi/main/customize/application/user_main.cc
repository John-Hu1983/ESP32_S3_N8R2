#include "customize/application/user_main.h"
#include "customize/peripheral/g_sensor/sc7a20htr.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "UserMain"

static i2c_master_bus_handle_t g_i2c_bus = nullptr;
static Sc7a20htr* g_sensor = nullptr;

static int accelerate_init_device(void) {
    esp_err_t ret = i2c_master_get_bus_handle(I2C_NUM_0, &g_i2c_bus);
    if (ret != ESP_OK || g_i2c_bus == nullptr) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(ret));
        return -1;
    }

    g_sensor = new Sc7a20htr(g_i2c_bus);
    ret = g_sensor->Init(SC7A20_ODR_100HZ, SC7A20_RANGE_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SC7A20 init failed: %s", esp_err_to_name(ret));
        delete g_sensor;
        g_sensor = nullptr;
        return -1;
    }

    return 0;
}

static void UserMainTask(void* arg) {
    (void)arg;
    esp_err_t ret;
    SC7A20_AccData_t acc = {};
    SC7A20_Angle_t ang = {};
    ESP_LOGI(TAG, "User main task started");

    if (accelerate_init_device() < 0) {
        vTaskDelete(nullptr);
        return;
    }

    for (;;) {
        bool read_ok = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            ret = g_sensor->ReadAcc(&acc);
            if (ret == ESP_OK) {
                read_ok = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (read_ok) {
            g_sensor->CalcAngle(acc, &ang);
            ESP_LOGI(TAG,
                     "Acc(m/s2): x=%.3f y=%.3f z=%.3f | Angle(deg): pitch=%.2f roll=%.2f yaw=%.2f",
                     acc.ax, acc.ay, acc.az, ang.pitch, ang.roll, ang.yaw);
        } else {
            ESP_LOGW(TAG, "SC7A20 read failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void StartUserMainTask() { xTaskCreate(UserMainTask, "user_main", 4096, nullptr, 5, nullptr); }
