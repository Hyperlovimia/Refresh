/**
 * @file weather_api.c
 * @brief ?? API ??????
 */

#include "weather_api.h"
#include "esp_log.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "WEATHER_API";
static WeatherData cached_data = {0};

esp_err_t weather_api_init(void) {
    ESP_LOGI(TAG, "????? API ???");
    // TODO: ?? - ??? HTTPS ????API Key ??
    memset(&cached_data, 0, sizeof(WeatherData));
    cached_data.valid = false;
    return ESP_OK;
}

esp_err_t weather_api_fetch(WeatherData *data) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "??????");
    // TODO: ?? - HTTPS ????? JSON ??

    // ??????
    data->pm25 = 35.0f;
    data->temperature = 22.0f;
    data->wind_speed = 8.5f;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;
    data->valid = true;

    // ????
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
