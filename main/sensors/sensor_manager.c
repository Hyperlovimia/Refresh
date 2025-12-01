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
static float last_valid_co2 = -1.0f;  // 上次有效的 CO₂ 浓度值
static bool has_valid_cache = false;  // 是否有有效的缓存值

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

    // 清空失败计数和缓存
    failure_count = 0;
    last_valid_co2 = -1.0f;
    has_valid_cache = false;
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(SensorData *data) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    // 读取 CO2
    float co2_value = co2_sensor_read_ppm();
    bool co2_valid = (co2_value >= CO2_MIN_VALID && co2_value <= CO2_MAX_VALID);
    bool using_cache = false;  // 是否使用缓存值

    // 如果 CO₂ 读取失败，尝试使用缓存值
    if (!co2_valid) {
        failure_count++;
        ESP_LOGW(TAG, "CO₂ 读取失败或超出范围，失败计数：%d", failure_count);

        // 连续失败 3 次后，使用上次有效值
        if (failure_count >= 3 && has_valid_cache) {
            ESP_LOGW(TAG, "使用缓存的 CO₂ 值：%.1f ppm", last_valid_co2);
            data->co2 = last_valid_co2;
            using_cache = true;  // 标记使用缓存
        } else {
            data->co2 = co2_value;  // 使用无效值（-1.0f）
        }
    } else {
        // CO₂ 读取成功，更新缓存
        data->co2 = co2_value;
        last_valid_co2 = co2_value;
        has_valid_cache = true;
        failure_count = 0;
    }

    // 读取 SHT35 温湿度（加 I2C 锁保护）
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    esp_err_t sht_ret = ESP_FAIL;
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sht_ret = sht35_read(&data->temperature, &data->humidity);
        xSemaphoreGive(i2c_mutex);
    } else {
        ESP_LOGE(TAG, "获取 I2C 锁超时");
        return ESP_FAIL;
    }

    if (sht_ret != ESP_OK) {
        ESP_LOGW(TAG, "SHT35 读取失败");
        return ESP_FAIL;
    }

    // 填充时间戳
    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;

    // 数据有效性检查
    bool temp_valid = (data->temperature >= TEMP_MIN_VALID && data->temperature <= TEMP_MAX_VALID);
    bool humi_valid = (data->humidity >= HUMI_MIN_VALID && data->humidity <= HUMI_MAX_VALID);

    // 如果使用缓存且温湿度有效，数据仍然可用
    if (using_cache) {
        data->valid = temp_valid && humi_valid;  // 缓存数据+正常温湿度=可用
        return ESP_FAIL;  // 但返回 ESP_FAIL 表示 CO₂ 传感器失败
    }

    // 正常情况：所有传感器数据都有效
    data->valid = co2_valid && temp_valid && humi_valid;
    return ESP_OK;
}

bool sensor_manager_is_healthy(void) {
    // 连续失败次数小于 3 次视为健康
    return (failure_count < 3);
}

esp_err_t sensor_manager_reinit(void) {
    ESP_LOGI(TAG, "重新初始化传感器管理器");

    // 清空连续失败计数器和缓存
    failure_count = 0;
    last_valid_co2 = -1.0f;
    has_valid_cache = false;

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
