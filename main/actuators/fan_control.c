/**
 * @file fan_control.c
 * @brief 风扇控制
 */

#include "fan_control.h"
#include "esp_log.h"

static const char *TAG = "FAN_CONTROL";
static FanState current_state = FAN_OFF;
static uint8_t current_pwm = 0;

/**
 * @brief PWM 限制函数 PWM 值
 *
 * @param raw_pwm 原始 PWM 值
 * @return 限制后的 PWM 值
 */
static uint8_t fan_clamp_pwm(uint8_t raw_pwm) {
    if (raw_pwm == 0) return 0;           // 关闭
    if (raw_pwm < 150) return 150;        // 最低值
    if (raw_pwm > 255) return 255;        // 最高值
    return raw_pwm;
}

/**
 * @brief 根据 FanState 获取 PWM 占空比
 *
 * @param state 风扇状态
 * @param is_night_mode 夜间模式
 * @return PWM 占空比
 */
static uint8_t fan_control_get_pwm_duty(FanState state, bool is_night_mode) {
    switch (state) {
        case FAN_OFF:  return 0;
        case FAN_LOW:  return is_night_mode ? 150 : 180;
        case FAN_HIGH: return is_night_mode ? 200 : 255;
        default:       return 0;
    }
}

esp_err_t fan_control_init(void) {
    ESP_LOGI(TAG, "初始化风扇控制");
    // TODO: 初始化 - 配置 LEDC PWM GPIO25 25kHz 8位
    current_state = FAN_OFF;
    current_pwm = 0;
    return ESP_OK;
}

esp_err_t fan_control_set_state(FanState state, bool is_night_mode) {
    uint8_t pwm = fan_control_get_pwm_duty(state, is_night_mode);
    current_state = state;
    current_pwm = pwm;
    ESP_LOGI(TAG, "设置风扇状态为%d，夜间模式%d，PWM为%d", state, is_night_mode, pwm);
    // TODO: 调用 - 调用 ledc_set_duty() 和 ledc_update_duty()
    return ESP_OK;
}

esp_err_t fan_control_set_pwm(uint8_t duty) {
    current_pwm = fan_clamp_pwm(duty);
    ESP_LOGD(TAG, "设置 PWM %d -> %d (clamp 后)", duty, current_pwm);
    // TODO: 调用 - 调用 ledc_set_duty() 和 ledc_update_duty()
    return ESP_OK;
}

FanState fan_control_get_state(void) {
    return current_state;
}

uint8_t fan_control_get_pwm(void) {
    return current_pwm;
}
