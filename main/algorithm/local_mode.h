/**
 * @file local_mode.h
 * @brief 本地模式决策接口定义
 */

#ifndef LOCAL_MODE_H
#define LOCAL_MODE_H

#include "main.h"

/**
 * @brief 本地模式决策
 *   CO2 > 1200ppm -> FAN_HIGH
 *   CO2 > 1000ppm -> FAN_LOW
 *   CO2 <= 1000ppm -> FAN_OFF
 * @param co2_ppm CO2 浓度 ppm
 * @return 风扇状态 OFF/LOW/HIGH
 */
FanState local_mode_decide(float co2_ppm);

#endif // LOCAL_MODE_H
