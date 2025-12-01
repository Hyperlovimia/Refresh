/**
 * @file sensor_manager.c
 * @brief 传感器管理器模块
 */

#include "sensor_manager.h"
#include "co2_sensor.h"
#include "sht35.h"
#include "../main.h"
#include "esp_log.h"
#include <sys/time.h>

static const char *TAG = "SENSOR_MGR";
static int failure_count = 0;

esp_err_t sensor_manager_init(void) {
    ESP_LOGI(TAG, "初始化传感器管理器");

    esp_err_t ret;

    ret = co2_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CO2 传感器初始化失败");
        return ESP_FAIL;
    }

    ret = sht35_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT35 初始化失败");
        return ESP_FAIL;
    }

    failure_count = 0;
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(SensorData *data) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: 实现 - 完整的传感器数据采集流程

    // 读取 CO2
    data->co2 = co2_sensor_read_ppm();

    // 读取 SHT35 温湿度（加 I2C 锁保护）
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    esp_err_t sht_ret = ESP_FAIL;
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sht_ret = sht35_read(&data->temperature, &data->humidity);
        xSemaphoreGive(i2c_mutex);
    } else {
        ESP_LOGE(TAG, "获取 I2C 锁超时");
        failure_count++;
        return ESP_FAIL;
    }

    if (sht_ret != ESP_OK) {
        ESP_LOGW(TAG, "SHT35 读取失败");
        failure_count++;
        return ESP_FAIL;
    }

    // 填充时间戳
    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;

    // 数据有效性检查
    data->valid = (data->co2 >= CO2_MIN_VALID && data->co2 <= CO2_MAX_VALID) &&
                  (data->temperature >= TEMP_MIN_VALID && data->temperature <= TEMP_MAX_VALID) &&
                  (data->humidity >= HUMI_MIN_VALID && data->humidity <= HUMI_MAX_VALID);

    if (!data->valid) {
        failure_count++;
        ESP_LOGW(TAG, "传感器数据超出有效范围，失败计数：%d", failure_count);
        return ESP_FAIL;
    }

    failure_count = 0;
    return ESP_OK;
}

bool sensor_manager_is_healthy(void) {
    // 连续失败次数小于 3 次视为健康
    return (failure_count < 3);
}

esp_err_t sensor_manager_reinit(void) {
    ESP_LOGI(TAG, "重新初始化传感器管理器");

    // 清空连续失败计数器
    failure_count = 0;

    // 重新初始化 CO2 传感器
    esp_err_t ret = co2_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CO2 传感器重新初始化失败");
        return ESP_FAIL;
    }

    // 重新初始化 SHT35
    ret = sht35_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT35 重新初始化失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器管理器重新初始化成功");
    return ESP_OK;
}
