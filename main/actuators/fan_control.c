/**
 * @file fan_control.c
 * @brief 风扇控制 - 多风扇版本
 */

#include "fan_control.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "FAN_CONTROL";
static FanState current_state[FAN_COUNT] = {FAN_OFF, FAN_OFF, FAN_OFF};
static uint8_t current_pwm[FAN_COUNT] = {0, 0, 0};
static bool s_ledc_ready = false;

// GPIO 映射
static const gpio_num_t FAN_GPIO[FAN_COUNT] = {
    GPIO_NUM_36,  // Fan0
    GPIO_NUM_37,  // Fan1
    GPIO_NUM_38   // Fan2
};

// LEDC 通道映射
static const ledc_channel_t FAN_CHANNEL[FAN_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2
};

#define FAN_PWM_FREQ_HZ      25000
#define FAN_PWM_RESOLUTION   LEDC_TIMER_8_BIT
#define FAN_LEDC_TIMER       LEDC_TIMER_0
#define FAN_LEDC_SPEED_MODE  LEDC_LOW_SPEED_MODE

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
    if (s_ledc_ready) {
        return ESP_OK;
    }

    // 配置 LEDC 定时器（共用）
    ledc_timer_config_t timer_cfg = {
        .speed_mode = FAN_LEDC_SPEED_MODE,
        .duty_resolution = FAN_PWM_RESOLUTION,
        .timer_num = FAN_LEDC_TIMER,
        .freq_hz = FAN_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config 失败 (%d)", err);
        return err;
    }

    // 配置3个 LEDC 通道
    for (int i = 0; i < FAN_COUNT; i++) {
        ledc_channel_config_t channel_cfg = {
            .gpio_num = FAN_GPIO[i],
            .speed_mode = FAN_LEDC_SPEED_MODE,
            .channel = FAN_CHANNEL[i],
            .timer_sel = FAN_LEDC_TIMER,
            .duty = 0,  // 初始占空比 0（关闭）
            .hpoint = 0
        };

        err = ledc_channel_config(&channel_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config 失败 (%d)", i, err);
            return err;
        }
    }

    s_ledc_ready = true;
    for (int i = 0; i < FAN_COUNT; i++) {
        current_state[i] = FAN_OFF;
        current_pwm[i] = 0;
    }
    ESP_LOGI(TAG, "初始化风扇控制完成 GPIO36/37/38 25kHz 8位");
    return ESP_OK;
}

esp_err_t fan_control_set_state(FanId id, FanState state, bool is_night_mode) {
    if (!s_ledc_ready) {
        ESP_LOGE(TAG, "LEDC 未初始化");
        return ESP_FAIL;
    }

    if (id >= FAN_COUNT) {
        ESP_LOGE(TAG, "无效的风扇ID %d", id);
        return ESP_FAIL;
    }

    uint8_t pwm = fan_control_get_pwm_duty(state, is_night_mode);
    current_state[id] = state;
    current_pwm[id] = pwm;

    // 设置 LEDC 占空比
    esp_err_t err = ledc_set_duty(FAN_LEDC_SPEED_MODE, FAN_CHANNEL[id], pwm);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置占空比失败 Fan%d (%d)", id, err);
        return err;
    }

    // 更新占空比
    err = ledc_update_duty(FAN_LEDC_SPEED_MODE, FAN_CHANNEL[id]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "更新占空比失败 Fan%d (%d)", id, err);
        return err;
    }

    ESP_LOGD(TAG, "设置 Fan%d 状态为%d，夜间模式%d，PWM为%d", id, state, is_night_mode, pwm);
    return ESP_OK;
}

esp_err_t fan_control_set_all(FanState state, bool is_night_mode) {
    esp_err_t err = ESP_OK;
    for (int i = 0; i < FAN_COUNT; i++) {
        esp_err_t ret = fan_control_set_state((FanId)i, state, is_night_mode);
        if (ret != ESP_OK) {
            err = ret;
        }
    }
    return err;
}

esp_err_t fan_control_set_pwm(FanId id, uint8_t duty) {
    if (!s_ledc_ready) {
        ESP_LOGE(TAG, "LEDC 未初始化");
        return ESP_FAIL;
    }

    if (id >= FAN_COUNT) {
        ESP_LOGE(TAG, "无效的风扇ID %d", id);
        return ESP_FAIL;
    }

    current_pwm[id] = fan_clamp_pwm(duty);

    // 设置 LEDC 占空比
    esp_err_t err = ledc_set_duty(FAN_LEDC_SPEED_MODE, FAN_CHANNEL[id], current_pwm[id]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置占空比失败 Fan%d (%d)", id, err);
        return err;
    }

    // 更新占空比
    err = ledc_update_duty(FAN_LEDC_SPEED_MODE, FAN_CHANNEL[id]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "更新占空比失败 Fan%d (%d)", id, err);
        return err;
    }

    ESP_LOGD(TAG, "设置 Fan%d PWM %d -> %d (clamp 后)", id, duty, current_pwm[id]);
    return ESP_OK;
}

FanState fan_control_get_state(FanId id) {
    if (id >= FAN_COUNT) {
        return FAN_OFF;
    }
    return current_state[id];
}

uint8_t fan_control_get_pwm(FanId id) {
    if (id >= FAN_COUNT) {
        return 0;
    }
    return current_pwm[id];
}
