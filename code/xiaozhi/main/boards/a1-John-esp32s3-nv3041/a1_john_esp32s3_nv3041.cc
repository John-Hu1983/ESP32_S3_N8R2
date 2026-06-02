#include "a1_john_no_audio_codec_duplex.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "customize/test_myself/test_self_mic.h"
#include "customize/test_myself/test_self_speaker.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(__has_include)
#if __has_include("esp_lcd_panel_nv3041.h")
#include "esp_lcd_panel_nv3041.h"
#define HAVE_NV3041_PANEL 1
#else
#include "esp_lcd_panel_st7789.h"
#define HAVE_NV3041_PANEL 0
#endif
#else
#include "esp_lcd_panel_st7789.h"
#define HAVE_NV3041_PANEL 0
#endif

#define TAG "A1JohnNv3041"
class A1JohnEsp32S3Nv3041Board : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_ = nullptr;

    void InitializePeripheralsReset() {
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

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 80 * 1000 * 1000;
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

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    A1JohnEsp32S3Nv3041Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePeripheralsReset();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
#if TEST_MIC
        Application::GetInstance().SetWakeWordDisabled(true);
        StartMicSelfTestTask();
#endif

#if TEST_SPEAKER
        StartSpeakerSelfTestTask(&Application::GetInstance().GetAudioService());
#endif
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static A1JohnNoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
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

DECLARE_BOARD(A1JohnEsp32S3Nv3041Board);
