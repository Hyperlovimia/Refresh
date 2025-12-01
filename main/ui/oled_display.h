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
 *   - 第1行：CO2 温度 湿度
 *   - 第2-3行：风扇状态 系统模式
 *   - 第4行：WiFi 状态
 * @param sensor 传感器数据
 * @param weather 天气数据
 * @param fan 风扇状态
 * @param mode 系统模式
 */
void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode);

/**
 * @brief 显示告警页面
 * 全屏显示告警信息
 * @param message 告警消息
 */
void oled_display_alert(const char *message);

/**
 * @brief 添加历史图表点
 * @param co2 CO2 浓度 ppm
 */
void oled_add_history_point(float co2);

#endif // OLED_DISPLAY_H
