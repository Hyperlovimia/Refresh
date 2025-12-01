/**
 * @file sht35.h
 * @brief SHT35 温度湿度传感器接口定义 - I2C 版本
 */

#ifndef SHT35_H
#define SHT35_H

#include "esp_err.h"

/**
 * @brief 初始化 SHT35 传感器
 * 初始化 I2C 100kHz, GPIO21 (SDA), GPIO20 (SCL), 地址0x44
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sht35_init(void);

/**
 * @brief 读取温度和湿度
 * 发送命令 0x2C 0x06，等待 15ms，读取 6 字节
 * 温度 = -45 + 175 * (temp_raw / 65535)
 * 湿度 = 100 * (humi_raw / 65535)
 * @param temp 输出温度（摄氏度）
 * @param humi 输出湿度（%）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sht35_read(float *temp, float *humi);

#endif // SHT35_H
