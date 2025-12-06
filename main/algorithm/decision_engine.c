/**
 * @file decision_engine.c
 * @brief 决策引擎
 */

#include "decision_engine.h"
#include "local_mode.h"
#include "esp_log.h"

static const char *TAG = "DECISION";

FanState decision_make(SensorData *sensor, FanState remote_cmd, SystemMode mode) {
    if (!sensor || !sensor->valid) {
        ESP_LOGW(TAG, "传感器数据无效");
        return FAN_OFF;
    }

    if (mode == MODE_SAFE_STOP) {
        return FAN_OFF;
    }

    if (mode == MODE_REMOTE) {
        ESP_LOGI(TAG, "远程模式: fan_state=%d", remote_cmd);
        return remote_cmd;
    }

    // MODE_LOCAL: 根据 CO2 决策
    return local_mode_decide(sensor->pollutants.co2);
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
