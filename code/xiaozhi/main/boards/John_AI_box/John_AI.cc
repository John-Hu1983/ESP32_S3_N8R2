#include "application.h"
#include "button.h"
#include "config.h"
#include "customize/peripheral/audio/ht517_inmp441.h"
#include "customize/peripheral/g_sensor/sc7a20htr.h"
#include "customize/peripheral/monitor/user_lcd_display.h"
#include "customize/peripheral/touch/tsc2046_touch.h"
#include "customize/sys_supervision/system_survey.h"
#include "customize/test_myself/test_self_mic.h"
#include "customize/test_myself/test_self_speaker.h"
#include "display/lcd_display.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include <cmath>
#include <limits>

#define TAG "JohnAI"

/*
brief   : Reset shared peripherals into a known startup state.
input   : None
output  : None
*/
static void peripheral_init_reset() {
    gpio_num_t reset_pin = PERIPHERAL_RESET_GPIO;
    if (reset_pin == GPIO_NUM_NC) {
        return;
    }

    gpio_config_t io_cfg = {};
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << reset_pin;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    int active_level = PERIPHERAL_RESET_ACTIVE_LOW ? 0 : 1;
    int inactive_level = PERIPHERAL_RESET_ACTIVE_LOW ? 1 : 0;
    gpio_set_level(reset_pin, active_level);
    vTaskDelay(pdMS_TO_TICKS(PERIPHERAL_RESET_PULSE_MS));
    gpio_set_level(reset_pin, inactive_level);
}

/*
brief   : Lock the power by setting the power lock GPIO pin
input   : None
output  : None
*/
static void power_lock_itself() {
    gpio_config_t io_cfg = {};
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << POWER_LOCK_GPIO;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    // Set the power lock pin to the active level to lock power
    gpio_set_level(POWER_LOCK_GPIO, 1);
}

/*
brief   : Initialize I2C bus for peripherals
input   : None
output  : None
*/
static void peripheral_init_i2c0() {
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PERIPHERAL_I2C_SDA_PIN,
        .scl_io_num = PERIPHERAL_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
}

/*
brief   : Initialize SPI bus for LCD display
input   : None
output  : None
*/
static void peripheral_init_spi2(void) {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = DISPLAY_MISO_PIN;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

/*
brief   : Read touch data for LVGL input device.
input   : LVGL input device and data buffer
output  : None
*/
static void peripheral_touch_read_callback(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* touch = static_cast<Tsc2046Touch*>(lv_indev_get_user_data(indev));
    bool pressed = false;
    uint16_t x = 0;
    uint16_t y = 0;

    if (touch != nullptr && touch->Read(&x, &y, &pressed)) {
        data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        if (pressed) {
            data->point.x = static_cast<lv_coord_t>(x);
            data->point.y = static_cast<lv_coord_t>(y);
            ESP_LOGW(TAG, "Touch x=%u y=%u raw_x=%u raw_y=%u", x, y, touch->last_raw_x(),
                     touch->last_raw_y());
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/*
brief   : Initialize LCD panel and display object.
input   : Display pointer owned by board
output  : None
*/
static void peripheral_init_lcd_display(LcdDisplay*& display) {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = 16 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
    panel_config.flags.reset_active_high = !PERIPHERAL_RESET_ACTIVE_LOW;

#if HAVE_NV3041_PANEL
    ESP_ERROR_CHECK(esp_lcd_new_panel_nv3041(panel_io, &panel_config, &panel));
#else
    ESP_LOGW(TAG, "NV3041 panel driver not found; falling back to ST7789");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    esp_lcd_panel_disp_on_off(panel, true);

#if JOHN_AI_USE_USER_UI
    display = new UserLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                 DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                 DISPLAY_SWAP_XY);
#else
    display = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                DISPLAY_SWAP_XY);
#endif
}

/*
brief   : Initialize touch controller and LVGL input device.
input   : Touch object and LVGL input pointer owned by board
output  : None
*/
static void peripheral_init_touch(Tsc2046Touch& touch, lv_indev_t*& touch_indev) {
    if (!touch.Init(SPI3_HOST, TOUCH_CLK_PIN, TOUCH_MOSI_PIN, TOUCH_MISO_PIN, TOUCH_CS_PIN,
                    TOUCH_IRQ_PIN, DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
        ESP_LOGW(TAG, "TSC2046 touch init failed");
        return;
    }

    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, peripheral_touch_read_callback);
    lv_indev_set_user_data(touch_indev, &touch);
}

/*
brief   : Bind boot button click behavior.
input   : Button object and board instance
output  : None
*/
static void peripheral_init_button(Button& boot_button, WifiBoard* board) {
    boot_button.OnClick([board]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            board->EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState();
    });
}

/*
brief   : Set a safe default speaker volume.
input   : None
output  : None
*/
static void peripheral_default_volume() {
    Settings settings("audio", true);
    int volume = settings.GetInt("output_volume", 10);
    if (volume < 50) {
        settings.SetInt("output_volume", 50);
    }
}

class JohnAIBoard : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    Tsc2046Touch touch_;
    lv_indev_t* touch_indev_ = nullptr;

public:
    JohnAIBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        peripheral_init_reset();
        peripheral_init_i2c0();
        peripheral_init_spi2();
        peripheral_init_lcd_display(display_);
        peripheral_init_touch(touch_, touch_indev_);
        peripheral_init_button(boot_button_, this);
        peripheral_default_volume();
        power_lock_itself();

        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Ht517Inmp441AudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                                  AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                                  AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(JohnAIBoard);
