/**
 * @file sht35.c
 * @brief SHT35 温度湿度传感器初始化
 */

#include "sht35.h"
#include "esp_log.h"

static const char *TAG = "SHT35";

esp_err_t sht35_init(void) {
    ESP_LOGI(TAG, "初始化 SHT35 温度湿度传感器");
    // TODO: 实现 - 初始化 I2C GPIO21/22 100kHz 地址0x44
    return ESP_OK;
}

esp_err_t sht35_read(float *temp, float *humi) {
    // TODO: 实现 - 读取数据 计算温度 计算湿度
    // 临时数据(测试)
    if (temp) *temp = 24.5f;
    if (humi) *humi = 58.0f;
    return ESP_OK;
}
