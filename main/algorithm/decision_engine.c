/**
 * @file decision_engine.c
 * @brief 决策引擎 - 多风扇版本
 */

#include "decision_engine.h"
#include "local_mode.h"
#include "esp_log.h"

static const char *TAG = "DECISION";

void decision_make(SensorData *sensor, const FanState remote_cmd[FAN_COUNT],
                   SystemMode mode, FanState out_fans[FAN_COUNT]) {
    if (!out_fans) {
        ESP_LOGE(TAG, "输出数组为空");
        return;
    }

    // 默认全部关闭
    for (int i = 0; i < FAN_COUNT; i++) {
        out_fans[i] = FAN_OFF;
    }

    if (!sensor || !sensor->valid) {
        ESP_LOGW(TAG, "传感器数据无效");
        return;
    }

    if (mode == MODE_SAFE_STOP) {
        // 全部关闭（已在上面设置）
        return;
    }

    if (mode == MODE_REMOTE) {
        if (!remote_cmd) {
            ESP_LOGW(TAG, "远程模式但命令为空");
            return;
        }
        // 直接使用远程命令
        for (int i = 0; i < FAN_COUNT; i++) {
            out_fans[i] = remote_cmd[i];
        }
        ESP_LOGI(TAG, "远程模式: fan_0=%d, fan_1=%d, fan_2=%d",
                 out_fans[0], out_fans[1], out_fans[2]);
        return;
    }

    // MODE_LOCAL: 3个风扇统一根据 CO2 决策（暂时同步）
    FanState local_decision = local_mode_decide(sensor->pollutants.co2);
    for (int i = 0; i < FAN_COUNT; i++) {
        out_fans[i] = local_decision;
    }
    ESP_LOGI(TAG, "本地模式: CO2=%.0f, 统一决策=%d", sensor->pollutants.co2, local_decision);
}

SystemMode decision_detect_mode(bool wifi_ok, bool sensor_ok) {
    if (!sensor_ok) {
        return MODE_SAFE_STOP;
    }
    if (wifi_ok) {
        return MODE_REMOTE;
    }
    return MODE_LOCAL;
}
