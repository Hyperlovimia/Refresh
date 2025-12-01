/**
 * @file fan_control.h
 * @brief ÎG§6q¨¥ã30FAN Module X1PWM 	
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief ËÎG§6
 *
 * Mn LEDC PWMGPIO2525kHz8M¨‡
 * löÞ¥
 *   - ESP32 5V ’ ÎG!W 2P-VCC
 *   - ESP32 GND ’ ÎG!W 2P-GND
 *   - ESP32 GPIO25 ’ ÎG!W 3P-PWM
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t fan_control_init(void);

/**
 * @brief ¾nÎG¶›³VÎ(	
 *
 * ê¨9n<! PWMcM
 *   FAN_OFF:  0
 *   FAN_LOW:  ô150~2700rpm	})180~3600rpm	
 *   FAN_HIGH: ô200~4300rpm	})255~6000rpm	
 *
 * @param state ÎG¶OFF/LOW/HIGH	
 * @param is_night_mode /&:ô!22:00-8:00	
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t fan_control_set_state(FanState state, bool is_night_mode);

/**
 * @brief ô¥¾nPWM`zÔ›KÕ(	
 *
 * ê¨clamp0	Hô0sí	 150-255ÐL	
 *
 * @param duty PWM`zÔ0-255	
 * @return ESP_OK Ÿ
 */
esp_err_t fan_control_set_pwm(uint8_t duty);

/**
 * @brief ·ÖSMÎG¶
 *
 * @return SMÎG¶
 */
FanState fan_control_get_state(void);

/**
 * @brief ·ÖSMPWM`zÔ
 *
 * @return SMPWM<0-255	
 */
uint8_t fan_control_get_pwm(void);

#endif // FAN_CONTROL_H
