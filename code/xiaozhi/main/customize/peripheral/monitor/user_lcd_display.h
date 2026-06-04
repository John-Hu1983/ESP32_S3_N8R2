#pragma once

#include "boards/John_AI_box/config.h"
#include "display/lcd_display.h"

#include <lvgl.h>

#include <string>

#if JOHN_AI_USE_USER_UI
class UserLcdDisplay final : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    ~UserLcdDisplay() override;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void ShowNotification(const std::string& notification, int duration_ms = 3000) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetTheme(Theme* theme) override;
    void SetPowerSaveMode(bool on) override;
    void UpdateStatusBar(bool update_all) override;

private:
    static void OnTopTimer(lv_timer_t* timer);
    void UpdateTopLabel(const char* text);
    void UpdateMicLabel(const char* text);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* grid_ = nullptr;
    lv_obj_t* bottom_bar_ = nullptr;
    lv_obj_t* top_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* mic_label_ = nullptr;
    lv_obj_t* wifi_icon_ = nullptr;
    lv_timer_t* top_timer_ = nullptr;
    std::string status_text_;
    std::string last_message_;
};
#endif
