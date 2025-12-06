/**
 * @file mqtt_wrapper.h
 * @brief MQTT 客户端包装接口定义 - EMQX Cloud TLS 版本
 */

#ifndef MQTT_WRAPPER_H
#define MQTT_WRAPPER_H

#include "esp_err.h"
#include "main.h"

/**
 * @brief 初始化 MQTT 客户端
 * 连接到 EMQX Cloud Broker mqtts://xxx.emqxsl.cn:8883
 * 使用 TLS esp_crt_bundle_attach
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief 发布状态到 home/ventilation/status
 * JSON 格式:
 * {
 *   "co2": 850,
 *   "temp": 24.5,
 *   "humi": 58,
 *   "fan_state": "LOW",
 *   "mode": "NORMAL"
 * }
 * QoS: 0
 * @param sensor 传感器数据
 * @param fan 风扇状态
 * @param mode 系统运行模式
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t mqtt_publish_status(SensorData *sensor, FanState fan, SystemMode mode);

/**
 * @brief 发布告警到 home/ventilation/alert
 * QoS: 1
 * @param message 告警消息
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t mqtt_publish_alert(const char *message);

/**
 * @brief 获取远程风扇控制命令
 * 从 home/ventilation/command 主题接收的最新命令
 * @param[out] cmd 输出风扇状态
 * @return true 有新命令，false 无命令或未连接
 */
bool mqtt_get_remote_command(FanState *cmd);

#endif // MQTT_WRAPPER_H
