/**
 * @file oled_display.h
 * @brief OLED 显示器接口定义 - SSD1306 I2C 版本
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "esp_err.h"
#include "main.h"

/**
 * @brief 初始化 OLED 显示器
 * 初始化 u8g2 库，I2C GPIO21/22，地址0x3C
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t oled_display_init(void);

/**
 * @brief 显示主页面
 *   - 第1行：CO2 数值 + 模式徽章
 *   - 第2行：温度 湿度
 *   - 第3-6行：CO2 趋势图（扩展区域）
 *   - 第7-8行：风扇状态 + WiFi 状态
 * @param sensor 传感器数据
 * @param fan 风扇状态
 * @param mode 系统模式
 */
void oled_display_main_page(SensorData *sensor, FanState fan, SystemMode mode);

/**
 * @brief 显示告警页面
 * 全屏显示告警信息
 * @param message 告警消息
 */
void oled_display_alert(const char *message);

/**
 * @brief 添加历史图表点
 * @param sensor 传感器数据
 */
void oled_add_history_point(SensorData *sensor);

#endif // OLED_DISPLAY_H
