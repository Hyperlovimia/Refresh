/**
 * @file sensor_manager.c
 * @brief ?????????
 */

#include "sensor_manager.h"
#include "co2_sensor.h"
#include "sht35.h"
#include "esp_log.h"
#include <sys/time.h>

static const char *TAG = "SENSOR_MGR";
static int failure_count = 0;

esp_err_t sensor_manager_init(void) {
    ESP_LOGI(TAG, "?????????");

    esp_err_t ret;

    ret = co2_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CO2 ????????");
        return ESP_FAIL;
    }

    ret = sht35_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT35 ????????");
        return ESP_FAIL;
    }

    failure_count = 0;
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(SensorData *data) {
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: ?? - ?????????

    // ?? CO2
    data->co2 = co2_sensor_read_ppm();

    // ?????
    sht35_read(&data->temperature, &data->humidity);

    // ?????
    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;

    // ???????
    data->valid = (data->co2 >= CO2_MIN_VALID && data->co2 <= CO2_MAX_VALID) &&
                  (data->temperature >= TEMP_MIN_VALID && data->temperature <= TEMP_MAX_VALID) &&
                  (data->humidity >= HUMI_MIN_VALID && data->humidity <= HUMI_MAX_VALID);

    if (!data->valid) {
        failure_count++;
        ESP_LOGW(TAG, "?????????????%d", failure_count);
        return ESP_FAIL;
    }

    failure_count = 0;
    return ESP_OK;
}

bool sensor_manager_is_healthy(void) {
    // ??????????? < 3
    return (failure_count < 3);
}
