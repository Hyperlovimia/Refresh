/**
 * @file oled_display.c
 * @brief OLED 显示驱动模块
 */

#include "oled_display.h"
#include "../main.h"
#include "esp_log.h"

static const char *TAG = "OLED_DISPLAY";

esp_err_t oled_display_init(void) {
    ESP_LOGI(TAG, "初始化 OLED 显示器");
    // TODO: 实现 - 初始化 u8g2 库，配置 I2C GPIO21/22，设备地址 0x3C
    return ESP_OK;
}

void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode) {
    // 获取 I2C 锁保护显示操作
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // TODO: 实现 - 使用 u8g2 绘制主页面
        ESP_LOGD(TAG, "刷新主页面");
        xSemaphoreGive(i2c_mutex);
    } else {
        ESP_LOGW(TAG, "获取 I2C 锁超时，跳过本次刷新");
    }
}

void oled_display_alert(const char *message) {
    if (!message) {
        return;
    }

    ESP_LOGW(TAG, "显示告警: %s", message);

    // 获取 I2C 锁保护显示操作
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // TODO: 实现 - 使用 u8g2 绘制告警页面
        xSemaphoreGive(i2c_mutex);
    } else {
        ESP_LOGW(TAG, "获取 I2C 锁超时，跳过告警显示");
    }
}

void oled_add_history_point(SensorData *sensor) {
    // TODO: 实现 - 添加历史数据点到环形缓冲区
    ESP_LOGD(TAG, "添加历史点: %.1f ppm", sensor->pollutants.co2);
}
