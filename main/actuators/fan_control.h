/**
 * @file fan_control.h
 * @brief 风扇控制接口定义 - 30mm FAN Module X1 PWM 版本
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化风扇控制
 * 初始化 LEDC PWM GPIO26 25kHz 8位分辨率
 * 接线:
 *   - ESP32 5V -> FAN 2P-VCC
 *   - ESP32 GND -> FAN 2P-GND
 *   - ESP32 GPIO26 -> FAN 3P-PWM
 * 注意: ESP32-S3 跳过 GPIO22-25，因此使用 GPIO26 而非 GPIO25
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t fan_control_init(void);

/**
 * @brief 设置风扇状态
 * PWM 占空比:
 *   FAN_OFF:  0
 *   FAN_LOW:  白天~2700rpm (180), 夜间~3600rpm
 *   FAN_HIGH: 白天~4300rpm (255), 夜间~6000rpm
 * @param state 风扇状态 OFF/LOW/HIGH
 * @param is_night_mode 夜间模式 22:00-8:00
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t fan_control_set_state(FanState state, bool is_night_mode);

/**
 * @brief 设置 PWM 占空比
 * clamp 到 0 和 150-255 范围
 * @param duty PWM 占空比 0-255
 * @return ESP_OK
 */
esp_err_t fan_control_set_pwm(uint8_t duty);

/**
 * @brief 获取当前风扇状态
 * @return 当前 FanState
 */
FanState fan_control_get_state(void);

/**
 * @brief 获取当前 PWM 占空比
 * @return 当前 PWM 值 0-255
 */
uint8_t fan_control_get_pwm(void);

#endif // FAN_CONTROL_H
