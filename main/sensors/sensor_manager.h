/**
 * @file sensor_manager.h
 * @brief 传感器管理器接口定义
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>

/**
 * @brief 初始化传感器管理器
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief 读取所有传感器数据到 SensorData 结构
 * 数据有效性检查
 * @param data 输出数据结构
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sensor_manager_read_all(SensorData *data);

/**
 * @brief 检查传感器健康状态
 * 连续失败次数 < 3
 * @return true 健康，false 异常
 */
bool sensor_manager_is_healthy(void);

/**
 * @brief 重新初始化传感器管理器（错误恢复时使用）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sensor_manager_reinit(void);

/**
 * @brief 污染物类型枚举
 */
typedef enum {
    POLLUTANT_PM,    ///< 颗粒物
    POLLUTANT_VOC,   ///< 挥发性有机物
    POLLUTANT_HCHO   ///< 甲醛
} PollutantType;

/**
 * @brief 手动设置污染物数据（用于测试和模拟）
 * @param type 污染物类型
 * @param value 污染物数值
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t sensor_manager_set_pollutant(PollutantType type, float value);

#endif // SENSOR_MANAGER_H
