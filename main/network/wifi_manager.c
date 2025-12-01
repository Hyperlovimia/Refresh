/**
 * @file wifi_manager.c
 * @brief WiFi 连接管理模块
 */

#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MGR";

esp_err_t wifi_manager_init(void) {
    ESP_LOGI(TAG, "初始化 WiFi 管理器");
    // TODO: 实现 - 初始化 WiFi 驱动、配置网络参数、从 NVS 读取配置
    return ESP_OK;
}

esp_err_t wifi_manager_start_provisioning(void) {
    ESP_LOGI(TAG, "启动配网流程");
    // TODO: 实现 - SmartConfig 或 BLE 配网
    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    // TODO: 实现 - 检查 WiFi 连接状态
    // 临时返回 false 用于测试
    return false;
}
