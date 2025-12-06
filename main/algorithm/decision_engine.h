/**
 * @file decision_engine.h
 * @brief 决策引擎接口定义
 */

#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include "main.h"
#include <stdbool.h>

/**
 * @brief 决策风扇状态
 *
 * MODE_REMOTE: 直接返回远程命令指定的风扇状态
 * MODE_LOCAL: 根据 CO2 阈值决策
 * MODE_SAFE_STOP: 强制关闭风扇
 *
 * @param sensor 传感器数据
 * @param remote_cmd 远程命令（MODE_REMOTE 时使用）
 * @param mode 系统运行模式
 * @return FanState 风扇状态
 */
FanState decision_make(SensorData *sensor, FanState remote_cmd, SystemMode mode);

/**
 * @brief 检测系统运行模式
 *   传感器异常 -> MODE_SAFE_STOP
 *   WiFi 可用 -> MODE_REMOTE
 *   WiFi 断开 -> MODE_LOCAL
 * @param wifi_ok WiFi 连接状态
 * @param sensor_ok 传感器健康状态
 * @return 系统模式
 */
SystemMode decision_detect_mode(bool wifi_ok, bool sensor_ok);

#endif // DECISION_ENGINE_H
