#ifndef SC7A20HTR_H
#define SC7A20HTR_H
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <cmath>
#include <cstdint>

//SC7A20 寄存器地址
#define SC7A20_WHO_AM_I    0x0F
#define SC7A20_CTRL1       0x20
#define SC7A20_CTRL4       0x23
#define SC7A20_XL          0x28
#define SC7A20_XH          0x29
#define SC7A20_YL          0x2A
#define SC7A20_YH          0x2B
#define SC7A20_ZL          0x2C
#define SC7A20_ZH          0x2D

//量程枚举 FS1 FS0(CTRL4 bit3 bit2)
typedef enum {
    SC7A20_RANGE_2G  = 0x00,
    SC7A20_RANGE_4G  = 0x01,
    SC7A20_RANGE_8G  = 0x02,
    SC7A20_RANGE_16G = 0x03,
} SC7A20_Range_t;

//输出速率ODR(CTRL1 D7~D4)
typedef enum {
    SC7A20_ODR_1HZ    = 0x1 <<4,
    SC7A20_ODR_10HZ   = 0x2 <<4,
    SC7A20_ODR_25HZ   = 0x3 <<4,
    SC7A20_ODR_50HZ   = 0x4 <<4,
    SC7A20_ODR_100HZ  = 0x5 <<4,
    SC7A20_ODR_200HZ  = 0x6 <<4,
    SC7A20_ODR_400HZ  = 0x7 <<4,
} SC7A20_Odr_t;

//加速度结构体
typedef struct {
    float ax;
    float ay;
    float az;
} SC7A20_AccData_t;

//欧拉角
typedef struct {
    float pitch;    //俯仰 X
    float roll;     //横滚 Y
    float yaw;      //偏航(单加速度无法真实yaw,重力无解算,估算值)
} SC7A20_Angle_t;

class Sc7a20htr : public I2cDevice {
public:
    static constexpr uint8_t kDefaultI2cAddress = 0x18;
    Sc7a20htr(i2c_master_bus_handle_t i2c_bus, uint8_t addr = kDefaultI2cAddress);

    //设备探测
    bool SelfTest(int timeout_ms = 100);
    //芯片初始化
    esp_err_t Init(SC7A20_Odr_t odr = SC7A20_ODR_100HZ, SC7A20_Range_t range = SC7A20_RANGE_2G);
    //设置量程
    esp_err_t SetRange(SC7A20_Range_t range);
    //设置采样速率
    esp_err_t SetOdr(SC7A20_Odr_t odr);
    //读取原始12bit -> 实际g值
    esp_err_t ReadAcc(SC7A20_AccData_t* acc);
    //由加速度计算 pitch roll yaw
    void CalcAngle(const SC7A20_AccData_t& acc, SC7A20_Angle_t* ang);

private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    uint8_t addr_ = kDefaultI2cAddress;
    float sens_g;   //当前量程灵敏度 mg/LSB
    //底层读写
    esp_err_t WriteReg(uint8_t reg, uint8_t val);
    esp_err_t ReadReg(uint8_t reg, uint8_t* buf, uint8_t len = 1);
};
#endif
