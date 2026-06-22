#include "customize/peripheral/monitor/user_lcd_display.h"

#include "board.h"

#include <esp_log.h>
#include <font_awesome.h>

#include <cstring>
#include <ctime>

#define USER_LCD_TAG "UserLcdDisplay"

LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

#if JOHN_AI_USE_USER_UI
namespace {
const lv_color_t kAppIconColor = lv_color_hex(0x0F172A);
const lv_color_t kAppIconColorActive = lv_color_hex(0xF8FAFC);
const lv_color_t kAppCardBg = lv_color_hex(0xF8FAFC);
const lv_color_t kAppCardBgActive = lv_color_hex(0x0F172A);

void AppCardEventCallback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }

    lv_obj_t* card = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* icon = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (card == nullptr || icon == nullptr) {
        return;
    }

    bool active = (code == LV_EVENT_PRESSED);
    lv_obj_set_style_text_color(icon, active ? kAppIconColorActive : kAppIconColor, 0);
    lv_obj_set_style_bg_color(card, active ? kAppCardBgActive : kAppCardBg, 0);
}
}  // namespace

UserLcdDisplay::~UserLcdDisplay() {
    if (top_timer_ != nullptr) {
        lv_timer_del(top_timer_);
        top_timer_ = nullptr;
    }
}

void UserLcdDisplay::SetupUI() {
    if (IsSetupUICalled()) {
        ESP_LOGW(USER_LCD_TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();
    DisplayLockGuard lock(this);

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);

    root_ = lv_obj_create(screen);
    lv_obj_set_size(root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0xE95420), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    const int top_bar_h = 26;
    const int bottom_bar_h = 28;
    const int grid_pad = 4;
    const int grid_gap = 4;

    top_bar_ = lv_obj_create(root_);
    lv_obj_set_size(top_bar_, LV_PCT(100), top_bar_h);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_bg_color(top_bar_, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(top_bar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(top_bar_, 6, 0);
    lv_obj_set_style_pad_right(top_bar_, 6, 0);
    lv_obj_set_style_pad_top(top_bar_, 3, 0);
    lv_obj_set_style_pad_bottom(top_bar_, 3, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    status_text_ = "WiFi: -- | State: --";
    lv_obj_t* top_left = lv_obj_create(top_bar_);
    lv_obj_set_style_bg_opa(top_left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_left, 0, 0);
    lv_obj_set_style_pad_all(top_left, 0, 0);
    lv_obj_set_style_pad_column(top_left, 6, 0);
    lv_obj_set_flex_flow(top_left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(top_left, LV_PCT(75));
    lv_obj_set_scrollbar_mode(top_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(top_left, LV_OBJ_FLAG_SCROLLABLE);

    wifi_icon_ = lv_label_create(top_left);
    lv_label_set_text(wifi_icon_, FONT_AWESOME_WIFI);
    lv_obj_set_style_text_font(wifi_icon_, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(wifi_icon_, lv_color_hex(0x0F172A), 0);

    top_label_ = lv_label_create(top_left);
    lv_obj_set_style_flex_grow(top_label_, 1, 0);
    lv_label_set_long_mode(top_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(top_label_, status_text_.c_str());
    lv_obj_set_style_text_color(top_label_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_font(top_label_, &BUILTIN_TEXT_FONT, 0);

    time_label_ = lv_label_create(top_bar_);
    lv_obj_set_width(time_label_, LV_PCT(25));
    lv_label_set_text(time_label_, "--:--");
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_font(time_label_, &BUILTIN_TEXT_FONT, 0);

    grid_ = lv_obj_create(root_);
    lv_obj_set_width(grid_, LV_PCT(100));
    int grid_h = LV_VER_RES - top_bar_h - bottom_bar_h - 1;
    if (grid_h < 0) {
        grid_h = 0;
    }
    lv_obj_set_height(grid_, grid_h);
    lv_obj_set_style_border_width(grid_, 0, 0);
    lv_obj_set_style_bg_color(grid_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(grid_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(grid_, grid_pad, 0);
    lv_obj_set_style_pad_row(grid_, grid_gap, 0);
    lv_obj_set_style_pad_column(grid_, grid_gap, 0);
    lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    const int columns = 5;
    const int rows = 2;
    int available_w = LV_HOR_RES - grid_pad * 2 - grid_gap * (columns - 1);
    int available_h = grid_h - grid_pad * 2 - grid_gap * (rows - 1);
    int cell_w = available_w / columns;
    int cell_h = available_h / rows;

    struct AppItem {
        const char* icon;
        const char* name;
    };

    static const AppItem kApps[] = {
        {FONT_AWESOME_HOUSE, "Home"},    {FONT_AWESOME_WIFI, "WiFi"},
        {FONT_AWESOME_MUSIC, "Music"},   {FONT_AWESOME_PLAY, "Video"},
        {FONT_AWESOME_IMAGE, "Gallery"}, {FONT_AWESOME_PEN_TO_SQUARE, "Notes"},
        {FONT_AWESOME_GEAR, "Settings"}, {FONT_AWESOME_LOCATION_DOT, "Map"},
        {FONT_AWESOME_BLUETOOTH, "BT"},  {FONT_AWESOME_ALARM_CLOCK, "Alarm"},
    };

    for (size_t i = 0; i < sizeof(kApps) / sizeof(kApps[0]); ++i) {
        lv_obj_t* card = lv_obj_create(grid_);
        lv_obj_set_size(card, cell_w, cell_h);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xE2E8F0), 0);
        lv_obj_set_style_bg_color(card, kAppCardBg, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t* icon = lv_label_create(card);
        lv_label_set_text(icon, kApps[i].icon);
        lv_obj_set_style_text_color(icon, kAppIconColor, 0);
        lv_obj_set_style_text_font(icon, &BUILTIN_ICON_FONT, 0);

        lv_obj_add_event_cb(card, AppCardEventCallback, LV_EVENT_PRESSED, icon);
        lv_obj_add_event_cb(card, AppCardEventCallback, LV_EVENT_RELEASED, icon);
        lv_obj_add_event_cb(card, AppCardEventCallback, LV_EVENT_PRESS_LOST, icon);

        lv_obj_t* label = lv_label_create(card);
        lv_label_set_text(label, kApps[i].name);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x334155), 0);
        lv_obj_set_style_text_font(label, &BUILTIN_TEXT_FONT, 0);
    }

    bottom_bar_ = lv_obj_create(root_);
    lv_obj_set_size(bottom_bar_, LV_PCT(100), bottom_bar_h);
    lv_obj_set_style_radius(bottom_bar_, 0, 0);
    lv_obj_set_style_border_width(bottom_bar_, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(bottom_bar_, 6, 0);
    lv_obj_set_style_pad_right(bottom_bar_, 6, 0);
    lv_obj_set_style_pad_top(bottom_bar_, 4, 0);
    lv_obj_set_style_pad_bottom(bottom_bar_, 4, 0);
    lv_obj_set_flex_flow(bottom_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_bar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    mic_label_ = lv_label_create(bottom_bar_);
    lv_obj_set_width(mic_label_, LV_PCT(95));
    lv_label_set_long_mode(mic_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(mic_label_, "Say something...");
    lv_obj_set_style_text_color(mic_label_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_font(mic_label_, &BUILTIN_TEXT_FONT, 0);

    static lv_anim_t mic_scroll_anim;
    lv_anim_init(&mic_scroll_anim);
    lv_anim_set_delay(&mic_scroll_anim, 1000);
    lv_anim_set_repeat_count(&mic_scroll_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(mic_label_, &mic_scroll_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(mic_label_, lv_anim_speed_clamped(60, 300, 60000),
                                   LV_PART_MAIN);
}

void UserLcdDisplay::SetStatus(const char* status) {
    if (status == nullptr || status[0] == '\0') {
        return;
    }
    status_text_ = status;
    UpdateTopLabel(status_text_.c_str());
}

void UserLcdDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (notification == nullptr || notification[0] == '\0') {
        return;
    }
    UpdateTopLabel(notification);
    if (duration_ms > 0) {
        if (top_timer_ != nullptr) {
            lv_timer_del(top_timer_);
            top_timer_ = nullptr;
        }
        top_timer_ = lv_timer_create(UserLcdDisplay::OnTopTimer, duration_ms, this);
    }
}

void UserLcdDisplay::ShowNotification(const std::string& notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void UserLcdDisplay::SetEmotion(const char* emotion) { (void)emotion; }

void UserLcdDisplay::SetChatMessage(const char* role, const char* content) {
    if (content == nullptr || content[0] == '\0') {
        return;
    }
    const char* prefix = "";
    if (role != nullptr) {
        if (std::strcmp(role, "user") == 0) {
            prefix = "U: ";
        } else if (std::strcmp(role, "assistant") == 0) {
            prefix = "A: ";
        } else if (std::strcmp(role, "system") == 0) {
            prefix = "S: ";
        } else {
            prefix = "MSG: ";
        }
    }
    last_message_.assign(prefix);
    last_message_.append(content);
    UpdateMicLabel(last_message_.c_str());
}

void UserLcdDisplay::ClearChatMessages() { UpdateMicLabel("Say something..."); }

void UserLcdDisplay::SetTheme(Theme* theme) { (void)theme; }

void UserLcdDisplay::SetPowerSaveMode(bool on) { (void)on; }

void UserLcdDisplay::UpdateStatusBar(bool update_all) {
    (void)update_all;
    if (time_label_ == nullptr) {
        return;
    }
    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    if (tm_now == nullptr) {
        return;
    }
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", tm_now);
    const char* wifi_icon = Board::GetInstance().GetNetworkStateIcon();
    DisplayLockGuard lock(this);

    const char* old_time = lv_label_get_text(time_label_);
    if (update_all || old_time == nullptr || std::strcmp(old_time, time_str) != 0) {
        lv_label_set_text(time_label_, time_str);
    }

    if (wifi_icon_ != nullptr && wifi_icon != nullptr) {
        const char* old_wifi_icon = lv_label_get_text(wifi_icon_);
        if (!update_all && old_wifi_icon != nullptr && std::strcmp(old_wifi_icon, wifi_icon) == 0) {
            return;
        }
        lv_label_set_text(wifi_icon_, wifi_icon);
    }
}

void UserLcdDisplay::OnTopTimer(lv_timer_t* timer) {
    auto* self = static_cast<UserLcdDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    self->UpdateTopLabel(self->status_text_.c_str());
    self->top_timer_ = nullptr;
    lv_timer_del(timer);
}

void UserLcdDisplay::UpdateTopLabel(const char* text) {
    if (text == nullptr || top_label_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    const char* old_text = lv_label_get_text(top_label_);
    if (old_text != nullptr && std::strcmp(old_text, text) == 0) {
        return;
    }
    lv_label_set_text(top_label_, text);
}

void UserLcdDisplay::UpdateMicLabel(const char* text) {
    if (text == nullptr || mic_label_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    const char* old_text = lv_label_get_text(mic_label_);
    if (old_text != nullptr && std::strcmp(old_text, text) == 0) {
        return;
    }
    lv_label_set_text(mic_label_, text);
}
#endif
