#include "sc7a20htr.h"
#include <esp_err.h>
#include <esp_log.h>

#define TAG "SC7A20HTR"
#define GRAVITY 9.81f

static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static Sc7a20htr* s_sensor = nullptr;

Sc7a20htr::Sc7a20htr(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : I2cDevice(i2c_bus, addr), i2c_bus_(i2c_bus), addr_(addr), sens_g(1.0f) {}

bool Sc7a20htr::SelfTest(int timeout_ms) {
    return i2c_master_probe(i2c_bus_, addr_, timeout_ms) == ESP_OK;
}

esp_err_t Sc7a20htr::WriteReg(uint8_t reg, uint8_t val) {
    uint8_t tmp[2] = {reg, val};
    return i2c_master_transmit(i2c_device_, tmp, 2, 100);
}

esp_err_t Sc7a20htr::ReadReg(uint8_t reg, uint8_t* buf, uint8_t len) {
    return i2c_master_transmit_receive(i2c_device_, &reg, 1, buf, len, 100);
}

esp_err_t Sc7a20htr::Init(SC7A20_Odr_t odr, SC7A20_Range_t range) {
    uint8_t id;
    auto ret = ReadReg(SC7A20_WHO_AM_I, &id);
    if (ret != ESP_OK || id != 0x11)
        return ESP_ERR_NOT_FOUND;

    // CTRL1: ODR + XYZ enable (default xyz=111)
    uint8_t c1 = odr | 0x07;
    ret |= WriteReg(SC7A20_CTRL1, c1);

    // CTRL4: HR 12-bit + range + BDU latch
    uint8_t c4 = 0x10 | (range << 2);  // HR=1(bit4)
    ret |= WriteReg(SC7A20_CTRL4, c4);

    // Update sensitivity (mg/LSB)
    switch (range) {
        case SC7A20_RANGE_2G:
            sens_g = 1.0f;
            break;
        case SC7A20_RANGE_4G:
            sens_g = 2.0f;
            break;
        case SC7A20_RANGE_8G:
            sens_g = 4.0f;
            break;
        case SC7A20_RANGE_16G:
            sens_g = 8.0f;
            break;
    }
    return ret;
}

esp_err_t Sc7a20htr::SetRange(SC7A20_Range_t range) {
    uint8_t val;
    auto ret = ReadReg(SC7A20_CTRL4, &val);
    val &= ~(0x0C);
    val |= (range << 2);
    ret |= WriteReg(SC7A20_CTRL4, val);

    switch (range) {
        case SC7A20_RANGE_2G:
            sens_g = 1.0f;
            break;
        case SC7A20_RANGE_4G:
            sens_g = 2.0f;
            break;
        case SC7A20_RANGE_8G:
            sens_g = 4.0f;
            break;
        case SC7A20_RANGE_16G:
            sens_g = 8.0f;
            break;
    }
    return ret;
}

esp_err_t Sc7a20htr::SetOdr(SC7A20_Odr_t odr) {
    uint8_t val;
    auto ret = ReadReg(SC7A20_CTRL1, &val);
    if (ret != ESP_OK) {
        return ret;
    }
    val &= 0x07;
    val |= odr;
    return WriteReg(SC7A20_CTRL1, val);
}

esp_err_t Sc7a20htr::ReadAcc(SC7A20_AccData_t* acc) {
    uint8_t buf[6];
    int16_t rawX, rawY, rawZ;
    uint8_t reg = SC7A20_XL | 0x80;
    auto ret = ReadReg(reg, buf, 6);
    if (ret != ESP_OK)
        return ret;

    rawX = (int16_t)((buf[1] << 8) | buf[0]) >> 4;  // 12-bit, shift right 4
    rawY = (int16_t)((buf[3] << 8) | buf[2]) >> 4;
    rawZ = (int16_t)((buf[5] << 8) | buf[4]) >> 4;

    // mg -> g -> m/s^2
    acc->ax = rawX * sens_g / 1000.0f;
    acc->ay = rawY * sens_g / 1000.0f;
    acc->az = rawZ * sens_g / 1000.0f;

    // Convert to m/s^2
    acc->ax *= GRAVITY;
    acc->ay *= GRAVITY;
    acc->az *= GRAVITY;
    return ESP_OK;
}

void Sc7a20htr::CalcAngle(const SC7A20_AccData_t& acc, SC7A20_Angle_t* ang) {
    // Note: accelerometer alone cannot compute true yaw; yaw fixed to 0.
    float ax = acc.ax / GRAVITY;
    float ay = acc.ay / GRAVITY;
    float az = acc.az / GRAVITY;

    // Pitch on X axis (-90..+90 deg)
    ang->pitch = atan2f(ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    // Roll on Y axis (-180..+180 deg)
    ang->roll = atan2f(-ay, az) * 180.0f / M_PI;
    // No yaw from accel-only
    ang->yaw = 0.0f;
}

int sc7a20htr_init_device() {
    esp_err_t ret;
    if (s_sensor != nullptr) {
        return 0;
    }

    ret = i2c_master_get_bus_handle(I2C_NUM_0, &s_i2c_bus);
    if (ret != ESP_OK || s_i2c_bus == nullptr) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(ret));
        return -1;
    }

    s_sensor = new Sc7a20htr(s_i2c_bus);
    ret = s_sensor->Init(SC7A20_ODR_100HZ, SC7A20_RANGE_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SC7A20 init failed: %s", esp_err_to_name(ret));
        delete s_sensor;
        s_sensor = nullptr;
        return -1;
    }
    return 0;
}

void sc7a20htr_read_elementary() {
    esp_err_t ret;
    SC7A20_AccData_t acc = {};
    SC7A20_Angle_t ang = {};

    if (s_sensor == nullptr) {
        ESP_LOGW(TAG, "SC7A20 not initialized");
        return;
    }

    ret = s_sensor->ReadAcc(&acc);
    if (ret == ESP_OK) {
        s_sensor->CalcAngle(acc, &ang);
        ESP_LOGI(TAG,
                 "Acc(m/s2): x=%.3f y=%.3f z=%.3f | Angle(deg): pitch=%.2f "
                 "roll=%.2f yaw=%.2f",
                 acc.ax, acc.ay, acc.az, ang.pitch, ang.roll, ang.yaw);
    }
}