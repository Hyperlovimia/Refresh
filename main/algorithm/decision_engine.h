/**
 * @file decision_engine.h
 * @brief 决策引擎接口定义
 */

#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include "main.h"
#include <stdbool.h>

/**
 * @brief 决策风扇状态（多风扇版本）
 *
 * MODE_REMOTE: 直接使用远程命令数组
 * MODE_LOCAL: 3个风扇统一根据 CO2 阈值决策（暂时同步）
 * MODE_SAFE_STOP: 强制关闭所有风扇
 *
 * @param sensor 传感器数据
 * @param remote_cmd 远程命令数组（MODE_REMOTE 时使用）
 * @param mode 系统运行模式
 * @param out_fans 输出3个风扇状态数组
 */
void decision_make(SensorData *sensor, const FanState remote_cmd[FAN_COUNT],
                   SystemMode mode, FanState out_fans[FAN_COUNT]);

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
