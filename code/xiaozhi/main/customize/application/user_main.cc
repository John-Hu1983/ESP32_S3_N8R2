#include "customize/application/user_main.h"
#include "customize/peripheral/g_sensor/sc7a20htr.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr char kUserMainTag[] = "UserMain";

void UserMainTask(void* arg) {
    (void)arg;
    ESP_LOGI(kUserMainTag, "User main task started");

    i2c_master_bus_handle_t i2c_bus = nullptr;
    esp_err_t ret = i2c_master_get_bus_handle(I2C_NUM_0, &i2c_bus);
    if (ret != ESP_OK || i2c_bus == nullptr) {
        ESP_LOGE(kUserMainTag, "Failed to get I2C bus handle: %s", esp_err_to_name(ret));
        vTaskDelete(nullptr);
        return;
    }

    Sc7a20htr sensor(i2c_bus);
    ret = sensor.Init(SC7A20_ODR_100HZ, SC7A20_RANGE_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(kUserMainTag, "SC7A20 init failed: %s", esp_err_to_name(ret));
        vTaskDelete(nullptr);
        return;
    }

    SC7A20_AccData_t acc = {};
    SC7A20_Angle_t ang = {};

    for (;;) {
        bool read_ok = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            ret = sensor.ReadAcc(&acc);
            if (ret == ESP_OK) {
                read_ok = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (read_ok) {
            sensor.CalcAngle(acc, &ang);
            ESP_LOGI(kUserMainTag,
                     "Acc(m/s2): x=%.3f y=%.3f z=%.3f | Angle(deg): pitch=%.2f roll=%.2f yaw=%.2f",
                     acc.ax, acc.ay, acc.az, ang.pitch, ang.roll, ang.yaw);
        } else {
            ESP_LOGW(kUserMainTag, "SC7A20 read failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
}  // namespace

void StartUserMainTask() { xTaskCreate(UserMainTask, "user_main", 4096, nullptr, 5, nullptr); }
