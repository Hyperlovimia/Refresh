/**
 * @file decision_engine.h
 * @brief 决策引擎接口定义 - Benefit-Cost 分析
 */

#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include "main.h"
#include <stdbool.h>

/**
 * @brief 决策风扇状态(Benefit-Cost 分析)
 *
 * 算法流程:
 * 1. 检查传感器和系统模式,处理异常情况
 * 2. 归一化室内外质量指数(CO₂、PM2.5)
 * 3. 计算收益(CO₂ 改善潜力)和成本(PM2.5 污染+温差损失)
 * 4. 根据通风指数决策风扇状态
 *
 * 算法公式:
 *   indoor_quality = (co2 - 400) / 1600
 *   outdoor_quality = pm25 / 100
 *   benefit = indoor_quality × 10
 *   cost = outdoor_quality × 5 + temp_diff × 2
 *   index = benefit - cost
 *
 * 决策规则:
 *   index > 3.0 → FAN_HIGH (高收益低成本,强制通风)
 *   1.0 < index ≤ 3.0 → FAN_LOW (中等收益,适度通风)
 *   index ≤ 1.0 → FAN_OFF (低收益高成本,停止通风)
 *
 * @param sensor 传感器数据(CO₂、温度、湿度)
 * @param weather 天气数据(PM2.5、室外温度)
 * @param mode 系统运行模式
 * @return FanState 风扇状态(OFF/LOW/HIGH)
 */
FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode);

/**
 * @brief 检测系统运行模式
 *   传感器异常 -> MODE_SAFE_STOP
 *   WiFi 可用 -> MODE_NORMAL
 *   WiFi 断开但缓存有效 <30min -> MODE_DEGRADED
 *   WiFi 断开且缓存过期 -> MODE_LOCAL
 * @param wifi_ok WiFi 连接状态
 * @param cache_valid 天气缓存有效
 * @param sensor_ok 传感器健康状态
 * @return 系统模式
 */
SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok);

#endif // DECISION_ENGINE_H
