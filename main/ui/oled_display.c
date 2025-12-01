/**
 * @file oled_display.c
 * @brief OLED ??????
 */

#include "oled_display.h"
#include "esp_log.h"

static const char *TAG = "OLED_DISPLAY";

esp_err_t oled_display_init(void) {
    ESP_LOGI(TAG, "??? OLED ???");
    // TODO: ?? - ??? u8g2 ??I2C GPIO21/22???0x3C
    return ESP_OK;
}

void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode) {
    // TODO: ?? - ?? u8g2 ?????
    ESP_LOGD(TAG, "?????");
}

void oled_display_alert(const char *message) {
    if (!message) {
        return;
    }

    ESP_LOGW(TAG, "?????%s", message);
    // TODO: ?? - ?? u8g2 ??????
}

void oled_add_history_point(float co2) {
    // TODO: ?? - ??????????
    ESP_LOGD(TAG, "????????%.1f ppm", co2);
}
