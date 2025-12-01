/**
 * @file fan_control.c
 * @brief ???????
 */

#include "fan_control.h"
#include "esp_log.h"

static const char *TAG = "FAN_CONTROL";
static FanState current_state = FAN_OFF;
static uint8_t current_pwm = 0;

/**
 * @brief PWM ??????? PWM ??
 *
 * @param raw_pwm ?? PWM ?
 * @return ??? PWM ?
 */
static uint8_t fan_clamp_pwm(uint8_t raw_pwm) {
    if (raw_pwm == 0) return 0;           // ????
    if (raw_pwm < 150) return 150;        // ??????
    if (raw_pwm > 255) return 255;        // ??????
    return raw_pwm;
}

/**
 * @brief ?? FanState ? PWM ???
 *
 * @param state ????
 * @param is_night_mode ????
 * @return PWM ???
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
    ESP_LOGI(TAG, "???????");
    // TODO: ?? - ??? LEDC PWM GPIO25 25kHz 8????
    current_state = FAN_OFF;
    current_pwm = 0;
    return ESP_OK;
}

esp_err_t fan_control_set_state(FanState state, bool is_night_mode) {
    uint8_t pwm = fan_control_get_pwm_duty(state, is_night_mode);
    current_state = state;
    current_pwm = pwm;
    ESP_LOGI(TAG, "???????%d??????%d?PWM?%d", state, is_night_mode, pwm);
    // TODO: ?? - ?? ledc_set_duty() ? ledc_update_duty()
    return ESP_OK;
}

esp_err_t fan_control_set_pwm(uint8_t duty) {
    current_pwm = fan_clamp_pwm(duty);
    ESP_LOGD(TAG, "?? PWM?%d -> %d (clamp ?)", duty, current_pwm);
    // TODO: ?? - ?? ledc_set_duty() ? ledc_update_duty()
    return ESP_OK;
}

FanState fan_control_get_state(void) {
    return current_state;
}

uint8_t fan_control_get_pwm(void) {
    return current_pwm;
}
