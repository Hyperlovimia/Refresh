/**
 * @file wifi_manager.h
 * @brief WiFi 管理器接口定义
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 初始化 WiFi 管理器
 * 初始化 WiFi 栈，配置参数，NVS 存储
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 启动配网
 * SmartConfig 和 BLE 配网
 * (需要先初始化 WiFi)
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t wifi_manager_start_provisioning(void);

/**
 * @brief 检查 WiFi 连接状态
 * @return true 连接，false 未连接
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
