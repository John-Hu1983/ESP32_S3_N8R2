#include <stdint.h>
#include "esp_log.h"

void lcd_init(void);
void lcd_fill_color(uint16_t color);
void tsc2046_init(void);
void tsc2046_start_task(void);

static const char *TAG = "NV3041_FIX";

// ========================== MAIN ==========================
void app_main(void)
{
    lcd_init();
    tsc2046_init();
    lcd_fill_color(0xF800);
    tsc2046_start_task();

    ESP_LOGI(TAG, "Color fix + fast fill done");
}
