/**
 * @file mqtt_client.c
 * @brief MQTT 客户端实现（桩函数）
 */

#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT_CLIENT";

esp_err_t mqtt_client_init(void) {
    ESP_LOGI(TAG, "初始化 MQTT 客户端（桩函数）");
    // TODO: 实际实现 - 连接到 EMQX Cloud Broker（TLS）
    return ESP_OK;
}

esp_err_t mqtt_publish_status(SensorData *sensor, FanState fan) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "发布状态: CO2=%f ppm, 风扇=%d（桩函数）", (double)sensor->co2, fan);
    // TODO: 实际实现 - 构建 JSON，发布到 home/ventilation/status
    return ESP_OK;
}

esp_err_t mqtt_publish_alert(const char *message) {
    if (!message) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "发布告警: %s（桩函数）", message);
    // TODO: 实际实现 - 发布到 home/ventilation/alert（QoS 1）
    return ESP_OK;
}
