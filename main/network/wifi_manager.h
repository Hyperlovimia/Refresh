/**
 * @file wifi_manager.h
 * @brief WiFi Þ¥¡ŒMQ¥ã
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Ë WiFi ¡h
 *
 * Ë WiFi q¨Œ‹ö
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief /¨MQŸýSmartConfig  BLE	
 *
 * (Ž–!MnÍ°Mn WiFi
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t wifi_manager_start_provisioning(void);

/**
 * @brief Àå WiFi Þ¥¶
 *
 * @return true òÞ¥false *Þ¥
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
