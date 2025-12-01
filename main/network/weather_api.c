/**
 * @file weather_api.c
 * @brief 天气 API 客户端
 */

#include "weather_api.h"
#include "esp_log.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "WEATHER_API";
static WeatherData cached_data = {0};

esp_err_t weather_api_init(void) {
    ESP_LOGI(TAG, "初始化天气 API 客户端");
    // TODO: 初始化 - 配置 HTTPS 客户端和 API Key
    memset(&cached_data, 0, sizeof(WeatherData));
    cached_data.valid = false;
    return ESP_OK;
}

esp_err_t weather_api_fetch(WeatherData *data) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "获取天气数据");
    // TODO: HTTPS 请求并解析 JSON

    // 模拟数据
    data->pm25 = 35.0f;
    data->temperature = 22.0f;
    data->wind_speed = 8.5f;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;
    data->valid = true;

    // 缓存数据
    memcpy(&cached_data, data, sizeof(WeatherData));

    return ESP_OK;
}

void weather_api_get_cached(WeatherData *data) {
    if (data) {
        memcpy(data, &cached_data, sizeof(WeatherData));
    }
}

bool weather_api_is_cache_stale(void) {
    if (!cached_data.valid) {
        return true;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t elapsed = tv.tv_sec - cached_data.timestamp;

    return (elapsed > WEATHER_CACHE_VALID_SEC);
}
