#include "application.h"
#include "audio/codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "customize/sys_supervision/system_survey.h"
#include "customize/test_myself/test_self_mic.h"
#include "customize/test_myself/test_self_speaker.h"
#include "display/lcd_display.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <limits>

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

namespace {
constexpr char kAudioTag[] = "A1JohnNoAudio";
constexpr char kBoardTag[] = "JohnAI";
}  // namespace

class A1JohnNoAudioCodecDuplex : public NoAudioCodec {
public:
    A1JohnNoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk,
                             gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
        duplex_ = true;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;
        input_channels_ = 1;
        output_channels_ = 2;  // Use stereo frame for HT517

        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        // Use stereo 32-bit slots (64fs); output data on both channels
        i2s_std_config_t tx_cfg = {
            .clk_cfg =
                {
                    .sample_rate_hz = (uint32_t)output_sample_rate_,
                    .clk_src = I2S_CLK_SRC_DEFAULT,
                    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                },
            .slot_cfg =
                {
                    .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                    .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                    .slot_mode = I2S_SLOT_MODE_STEREO,
                    .slot_mask = I2S_STD_SLOT_BOTH,
                    .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                    .ws_pol = false,
                    .bit_shift = true,
                },
            .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                         .bclk = bclk,
                         .ws = ws,
                         .dout = dout,
                         .din = din,
                         .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

        i2s_std_config_t rx_cfg = tx_cfg;
        rx_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_;
        rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
        rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        rx_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
        rx_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
        rx_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &tx_cfg));
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &rx_cfg));
        SetInputGain(3.0f);
        ESP_LOGI(kAudioTag, "Stereo I2S configured (HT517)");
    }

    void OutputData(std::vector<int16_t>& data) override {
        if (data.empty())
            return;

        float volume = static_cast<float>(output_volume_) / 100.0f;
        if (volume < 0.0f) {
            volume = 0.0f;
        } else if (volume > 1.0f) {
            volume = 1.0f;
        }
        float gain = volume * volume;

        std::vector<int32_t> interleaved;
        interleaved.reserve(data.size() * 2);
        for (int16_t sample : data) {
            int32_t scaled = static_cast<int32_t>(sample * gain);
            if (scaled > std::numeric_limits<int16_t>::max()) {
                scaled = std::numeric_limits<int16_t>::max();
            } else if (scaled < std::numeric_limits<int16_t>::min()) {
                scaled = std::numeric_limits<int16_t>::min();
            }
            int16_t out16 = static_cast<int16_t>(scaled);
            uint32_t out32u = static_cast<uint32_t>(static_cast<uint16_t>(out16)) << 16;
            int32_t out32 = static_cast<int32_t>(out32u);
            interleaved.push_back(out32);
            interleaved.push_back(out32);
        }

        size_t bytes_written = 0;
        i2s_channel_write(tx_handle_, interleaved.data(), interleaved.size() * sizeof(int32_t),
                          &bytes_written, portMAX_DELAY);
    }

    void EnableInput(bool enable) override {
        if (enable && !output_enabled_) {
            force_tx_clock_ = true;
            NoAudioCodec::EnableOutput(true);
        }

        NoAudioCodec::EnableInput(enable);

        if (!enable && force_tx_clock_) {
            force_tx_clock_ = false;
            NoAudioCodec::EnableOutput(false);
        }
    }

protected:
    int Read(int16_t* dest, int samples) override {
        if (dest == nullptr || samples <= 0) {
            return 0;
        }

        int raw_samples = samples * 2;
        std::vector<int32_t> raw(static_cast<size_t>(raw_samples));
        size_t bytes_read = 0;
        constexpr uint32_t kReadTimeoutMs = 200;
        esp_err_t err = i2s_channel_read(rx_handle_, raw.data(), raw_samples * sizeof(int32_t),
                                         &bytes_read, kReadTimeoutMs);
        if (err != ESP_OK) {
            return 0;
        }

        constexpr int kMicShift = 10;
        float gain = input_gain_;
        int read_samples = static_cast<int>(bytes_read / sizeof(int32_t));
        int frames = read_samples / 2;
        if (frames <= 0) {
            return 0;
        }

        // INMP441 L/R is tied to GND, so use left channel.
        constexpr int channel_offset = 0;
        for (int i = 0; i < frames; ++i) {
            int32_t sample = raw[static_cast<size_t>(i) * 2 + channel_offset] >> kMicShift;
            if (gain > 0.0f && gain != 1.0f) {
                sample = static_cast<int32_t>(sample * gain);
            }
            if (sample > std::numeric_limits<int16_t>::max()) {
                sample = std::numeric_limits<int16_t>::max();
            } else if (sample < std::numeric_limits<int16_t>::min()) {
                sample = std::numeric_limits<int16_t>::min();
            }
            int16_t out = static_cast<int16_t>(sample);
            dest[i] = out;
        }

        return frames;
    }

private:
    bool force_tx_clock_ = false;
};

class JohnAIBoard : public WifiBoard {
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

        ESP_LOGD(kBoardTag, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        ESP_LOGD(kBoardTag, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.flags.reset_active_high = !PERIPHERAL_RESET_ACTIVE_LOW;

#if HAVE_NV3041_PANEL
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3041(panel_io, &panel_config, &panel));
#else
        ESP_LOGW(kBoardTag, "NV3041 panel driver not found; falling back to ST7789");
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
    JohnAIBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePeripheralsReset();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();

        {
            Settings settings("audio", true);
            int volume = settings.GetInt("output_volume", 10);
            if (volume < 20) {
                settings.SetInt("output_volume", 20);
            }
        }

#if SYSTEM_SUPERVISION_ENABLED
        StartSystemSurveyTask();
#endif

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

DECLARE_BOARD(JohnAIBoard);
