/**
 * @file wifi_manager.c
 * @brief WiFi ??????
 */

#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MGR";

esp_err_t wifi_manager_init(void) {
    ESP_LOGI(TAG, "??? WiFi ???");
    // TODO: ?? - ??? WiFi ???????NVS ??
    return ESP_OK;
}

esp_err_t wifi_manager_start_provisioning(void) {
    ESP_LOGI(TAG, "????");
    // TODO: ?? - SmartConfig ? BLE ??
    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    // TODO: ?? - ?? WiFi ????
    // ???????
    return false;
}
