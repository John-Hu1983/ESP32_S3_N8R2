#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sccb.h"
#include "bf30a2.h"
#include "bf30a2_regs.h"
#include "bf30a2_settings.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
static const char *TAG = "bf30a2";
#endif

// #define REG_DEBUG_ON
// #define DEBUG_PRINT_REG

static int read_reg(uint8_t slv_addr, const uint16_t reg)
{
    int ret = SCCB_Read(slv_addr, reg);
    // ESP_LOGI(TAG, "READ Register 0x%02x VALUE: 0x%02x", reg, ret);
#ifdef REG_DEBUG_ON
    if (ret < 0) {
        ESP_LOGE(TAG, "READ REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

static int write_reg(uint8_t slv_addr, const uint16_t reg, uint8_t value)
{
    int ret = SCCB_Write(slv_addr, reg, value);
#ifdef REG_DEBUG_ON
    if (ret < 0) {
        ESP_LOGE(TAG, "WRITE REG 0x%04x FAILED: %d", reg, ret);
    }
#endif
    return ret;
}

static int set_reg_bits(sensor_t *sensor, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;

    ret = SCCB_Read(sensor->slv_addr, reg);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = SCCB_Write(sensor->slv_addr, reg & 0xFF, value);
    return ret;
}

static int read_regs(uint8_t slv_addr, const uint16_t(*regs)[2])
{
    int i = 0, ret = 0;
    while (regs[i][0] != REGLIST_TAIL) {
        if (regs[i][0] == REG_DLY) {
            vTaskDelay(regs[i][1] / portTICK_PERIOD_MS);
        } else {
            ret = read_reg(slv_addr, regs[i][0]);
        }
        i++;
    }
    return ret;
}

static int write_regs(uint8_t slv_addr, const uint16_t(*regs)[2])
{
    int i = 0, ret = 0;
    while (!ret && regs[i][0] != REGLIST_TAIL) {
        if (regs[i][0] == REG_DLY) {
            vTaskDelay(regs[i][1] / portTICK_PERIOD_MS);
        } else {
            ret = write_reg(slv_addr, regs[i][0], regs[i][1]);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static int get_reg(sensor_t *sensor, int reg, int mask)
{
    int ret = 0;
    if (mask > 0xFF) {
        ESP_LOGE(TAG, "mask should not more than 0xff");
    } else {
        ret = read_reg(sensor->slv_addr, reg);
    }
    if (ret > 0) {
        ret &= mask;
    }
    return ret;
}

static int set_reg(sensor_t *sensor, int reg, int mask, int value)
{
    int ret = 0;
    if (mask > 0xFF) {
        ESP_LOGE(TAG, "mask should not more than 0xff");
    } else {
        ret = read_reg(sensor->slv_addr, reg);
    }
    if (ret < 0) {
        return ret;
    }
    value = (ret & ~mask) | (value & mask);

    if (mask > 0xFF) {

    } else {
        ret = write_reg(sensor->slv_addr, reg, value);
    }
    return ret;
}

static int set_dummy(sensor_t *sensor, int val)
{
    ESP_LOGW(TAG, "dummy Unsupported");
    return -1;
}

static int set_dummy_gainceiling(sensor_t *sensor, gainceiling_t val)
{
    return set_dummy(sensor, (int)val);
}

static int reset(sensor_t *sensor)
{
    int ret;
    // Software Reset: clear all registers and reset them to their default values
    ret = write_reg(sensor->slv_addr, RESET_RELATED, 0x01);
    if (ret) {
        ESP_LOGE(TAG, "Software Reset FAILED!");
        return ret;
    }
    ESP_LOGD(TAG, "reset all regs");
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ret = write_regs(sensor->slv_addr, bf30a2_default_init_regs);
    if (ret == 0) {
        ESP_LOGI(TAG, "Camera defaults loaded");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // int reg_value = read_reg(sensor->slv_addr, 0xe0);
    // ESP_LOGI(TAG, "0xe0=0x%02x", reg_value);
    // set_power_down(sensor, 0x00); // software power down
    // reg_value = read_reg(sensor->slv_addr, 0xe0);
    // ESP_LOGW(TAG, "0xe0=0x%02x", reg_value);
    // set_colorbar(sensor, 1);

    // int test_value = read_regs(sensor->slv_addr, bf30a2_default_init_regs);

    return ret;
}

static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    int ret = 0;
    if (framesize > FRAMESIZE_QVGA) {
        return -1;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;

    sensor->status.framesize = framesize;

    ESP_LOGD(TAG, "w: %d,h: %d", w, h);  // 240, 320 (QVGA_R)

    // Write MSBs
    ret |= SCCB_Write(sensor->slv_addr, 0x17, 0);  // Horizontal Frame (HREF column)  240
    ret |= SCCB_Write(sensor->slv_addr, 0x18, w);

    ret |= SCCB_Write(sensor->slv_addr, 0x19, 0);  // Vertical Frame (row)  320
    ret |= SCCB_Write(sensor->slv_addr, 0x1a, h >> 1);

    // Write LSBs
    ret |= set_reg_bits(sensor, 0x12, 7, 0x01, h & 0x01);

    // Delay
    vTaskDelay(30 / portTICK_PERIOD_MS);

    return ret;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret = 0;
    switch (pixformat) {
    case PIXFORMAT_YUV422:
        ret = set_reg_bits(sensor, 0x12, 0, 3, 0x00);
        break;
    case PIXFORMAT_RAW:
        ret = set_reg_bits(sensor, 0x12, 0, 3, 0x01);
        break;
    case PIXFORMAT_GRAYSCALE:
        ret = set_reg_bits(sensor, 0x12, 0, 3, 0x03);
        break;
    default:
        ESP_LOGW(TAG, "set_pix unsupport format");
        ret = -1;
        break;
    }
    if (ret == 0) {
        sensor->pixformat = pixformat;
        ESP_LOGD(TAG, "Set pixformat to: %u", pixformat);
    }

    return ret;
}

// control sensor power down. 0: enable output, 1: diable output
static int set_special_effect(sensor_t *sensor, int enable)
{
    int ret = 0;
    ret = SCCB_Write(sensor->slv_addr, 0xcf, enable);

    if (ret == 0) {
        ESP_LOGI(TAG, "Set power down to: %d", enable);
    }
    return ret;
}

static int init_status(sensor_t *sensor)
{
    // write_reg(sensor->slv_addr, 0xfe, 0x00);
    sensor->status.brightness = SCCB_Read(sensor->slv_addr, 0x5a);
    sensor->status.contrast = SCCB_Read(sensor->slv_addr, 0x56);
    sensor->status.saturation = 0;
    sensor->status.sharpness = SCCB_Read(sensor->slv_addr, 0x70);
    sensor->status.denoise = 0;
    sensor->status.ae_level = 0;
    sensor->status.gainceiling = SCCB_Read(sensor->slv_addr, 0x13);
    sensor->status.awb = 0;
    sensor->status.dcw = 0;
    sensor->status.agc = 0;
    sensor->status.aec = 0;
    sensor->status.hmirror = 0;// check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x01);
    sensor->status.vflip = 0;// check_reg_mask(sensor->slv_addr, P0_CISCTL_MODE1, 0x02);
    sensor->status.colorbar = 0;
    sensor->status.bpc = 0;
    sensor->status.wpc = 0;
    sensor->status.raw_gma = 0;
    sensor->status.lenc = 0;
    sensor->status.quality = 0;
    sensor->status.special_effect = 0;
    sensor->status.wb_mode = 0;
    sensor->status.awb_gain = 0;
    sensor->status.agc_gain = 0;
    sensor->status.aec_value = 0;
    sensor->status.aec2 = 0;
    return 0;
}

int bf30a2_detect(int slv_addr, sensor_id_t *id)
{
    // printf("slv_addr: %x\r\n", slv_addr);
    if (BF30A2_SCCB_ADDR == slv_addr) {
        uint8_t MIDL = SCCB_Read(slv_addr, SENSOR_ID_LOW);
        uint8_t MIDH = SCCB_Read(slv_addr, SENSOR_ID_HIGH);
        uint16_t PID = MIDH << 8 | MIDL;
        if (BF30A2_PID == PID) {
            id->PID = PID;
            ESP_LOGW(TAG, "Attached BF30A2");
            return PID;
        } else {
            ESP_LOGE(TAG, "Mismatch PID=0x%x", PID);
        }
    }
    return 0;
}

int bf30a2_init(sensor_t *sensor)
{
    sensor->init_status = init_status;
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
    sensor->set_framesize = set_framesize;
    sensor->set_contrast = set_dummy;
    sensor->set_brightness = set_dummy;
    sensor->set_saturation = set_dummy;
    sensor->set_sharpness = set_dummy;  // set_sharpness;
    sensor->set_denoise = set_dummy;
    sensor->set_gainceiling = set_dummy_gainceiling;  // set_gainceiling_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_colorbar = set_dummy;  // set_colorbar;
    sensor->set_whitebal = set_dummy;
    sensor->set_gain_ctrl = set_dummy;
    sensor->set_exposure_ctrl = set_dummy;
    sensor->set_hmirror = set_dummy;  // set_hmirror;
    sensor->set_vflip = set_dummy;  // set_vflip;

    sensor->set_aec2 = set_dummy;
    sensor->set_awb_gain = set_dummy;
    sensor->set_agc_gain = set_dummy;
    sensor->set_aec_value = set_dummy;

    sensor->set_special_effect = set_special_effect;
    sensor->set_wb_mode = set_dummy;
    sensor->set_ae_level = set_dummy;

    sensor->set_dcw = set_dummy;
    sensor->set_bpc = set_dummy;
    sensor->set_wpc = set_dummy;

    sensor->set_raw_gma = set_dummy;
    sensor->set_lenc = set_dummy;

    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_res_raw = NULL;
    sensor->set_pll = NULL;
    sensor->set_xclk = NULL;

    ESP_LOGD(TAG, "BF30A2 Attached");
    return 0;
}
