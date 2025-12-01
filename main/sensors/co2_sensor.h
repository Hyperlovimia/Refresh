/**
 * @file co2_sensor.h
 * @brief CO2 传感器接口定义 - JX-CO2-102-5K UART 版本
 */

#ifndef CO2_SENSOR_H
#define CO2_SENSOR_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 初始化 CO2 传感器
 * 初始化 UART 9600, 8N1, RX GPIO16 (RX), GPIO17 (TX)
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t co2_sensor_init(void);

/**
 * @brief 读取 CO2 浓度值
 * 通过 UART 读取 ASCII 数据
 * 格式: " xxxx ppm\r\n"
 * 解析: 跳过空格 + 4位数字 + "ppm" + 回车换行
 * @return CO2 浓度 ppm，失败返回 -1.0f
 */
float co2_sensor_read_ppm(void);

/**
 * @brief 检查传感器是否就绪
 * @return true 就绪，false 未就绪
 */
bool co2_sensor_is_ready(void);

/**
 * @brief 校准传感器
 * @return ESP_OK 成功，ESP_ERR_NOT_SUPPORTED 不支持
 */
esp_err_t co2_sensor_calibrate(void);

#endif // CO2_SENSOR_H
