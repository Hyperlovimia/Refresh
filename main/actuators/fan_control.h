/**
 * @file fan_control.h
 * @brief 风扇控制接口定义 - 30mm FAN Module X3 PWM 版本（多风扇）
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化风扇控制
 * 初始化 LEDC PWM 25kHz 8位分辨率
 * 接线:
 *   Fan0: GPIO36 -> FAN0 PWM
 *   Fan1: GPIO37 -> FAN1 PWM
 *   Fan2: GPIO38 -> FAN2 PWM
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t fan_control_init(void);

/**
 * @brief 设置单个风扇状态
 * PWM 占空比:
 *   FAN_OFF:  0
 *   FAN_LOW:  白天~2700rpm (180), 夜间~3600rpm (150)
 *   FAN_HIGH: 白天~4300rpm (255), 夜间~6000rpm (200)
 * @param id 风扇ID (FAN_ID_0/1/2)
 * @param state 风扇状态 OFF/LOW/HIGH
 * @param is_night_mode 夜间模式 22:00-8:00
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t fan_control_set_state(FanId id, FanState state, bool is_night_mode);

/**
 * @brief 设置所有风扇状态（批量操作）
 * @param state 风扇状态 OFF/LOW/HIGH
 * @param is_night_mode 夜间模式 22:00-8:00
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t fan_control_set_all(FanState state, bool is_night_mode);

/**
 * @brief 设置单个风扇 PWM 占空比
 * clamp 到 0 和 150-255 范围
 * @param id 风扇ID (FAN_ID_0/1/2)
 * @param duty PWM 占空比 0-255
 * @return ESP_OK
 */
esp_err_t fan_control_set_pwm(FanId id, uint8_t duty);

/**
 * @brief 获取单个风扇当前状态
 * @param id 风扇ID (FAN_ID_0/1/2)
 * @return 当前 FanState，无效ID返回FAN_OFF
 */
FanState fan_control_get_state(FanId id);

/**
 * @brief 获取单个风扇当前 PWM 占空比
 * @param id 风扇ID (FAN_ID_0/1/2)
 * @return 当前 PWM 值 0-255，无效ID返回0
 */
uint8_t fan_control_get_pwm(FanId id);

#endif // FAN_CONTROL_H
