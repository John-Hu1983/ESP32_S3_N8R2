#include "sc7a20htr.h"
#include <esp_err.h>
#define GRAVITY 9.81f

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

    // CTRL1: ODR + XYZ使能(默认xyz=111)
    uint8_t c1 = odr | 0x07;
    ret |= WriteReg(SC7A20_CTRL1, c1);

    // CTRL4: HR高精度12bit=1 + 量程 + BDU锁存
    uint8_t c4 = 0x10 | (range << 2);  // HR=1(bit4)
    ret |= WriteReg(SC7A20_CTRL4, c4);

    // 更新灵敏度 mg/lsb
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

    rawX = (int16_t)((buf[1] << 8) | buf[0]) >> 4;  // 12bit右移4
    rawY = (int16_t)((buf[3] << 8) | buf[2]) >> 4;
    rawZ = (int16_t)((buf[5] << 8) | buf[4]) >> 4;

    // mg -> g -> m/s²
    acc->ax = rawX * sens_g / 1000.0f;
    acc->ay = rawY * sens_g / 1000.0f;
    acc->az = rawZ * sens_g / 1000.0f;

    // 转m/s²
    acc->ax *= GRAVITY;
    acc->ay *= GRAVITY;
    acc->az *= GRAVITY;
    return ESP_OK;
}

void Sc7a20htr::CalcAngle(const SC7A20_AccData_t& acc, SC7A20_Angle_t* ang) {
    // 注：单加速度**无法解算真实YAW(航向)，无陀螺无地磁**，yaw固定0
    float ax = acc.ax / GRAVITY;
    float ay = acc.ay / GRAVITY;
    float az = acc.az / GRAVITY;

    // pitch X轴俯仰(-90~+90°)
    ang->pitch = atan2f(ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    // roll Y轴横滚(-180~+180°)
    ang->roll = atan2f(-ay, az) * 180.0f / M_PI;
    // 单加速不能算yaw
    ang->yaw = 0.0f;
}