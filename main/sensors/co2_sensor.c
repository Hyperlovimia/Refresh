/**
 * @file co2_sensor.c
 * @brief CO2 ??????
 */

#include "co2_sensor.h"
#include "esp_log.h"

static const char *TAG = "CO2_SENSOR";

esp_err_t co2_sensor_init(void) {
    ESP_LOGI(TAG, "??? CO2 ???");
    // TODO: ?? - ??? UART GPIO16/17 9600, 8N1
    return ESP_OK;
}

float co2_sensor_read_ppm(void) {
    // TODO: ?? - ?? UART ?? ASCII ??
    // ????(??)
    return 850.0f;
}

bool co2_sensor_is_ready(void) {
    // TODO: ?? - ???????
    return true;
}

esp_err_t co2_sensor_calibrate(void) {
    ESP_LOGW(TAG, "????????");
    return ESP_ERR_NOT_SUPPORTED;
}
