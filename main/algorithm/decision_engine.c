/**
 * @file decision_engine.c
 * @brief 决策引擎
 */

#include "decision_engine.h"
#include "local_mode.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "DECISION";

FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode) {
    if (!sensor || !sensor->valid) {
        ESP_LOGW(TAG, "传感器数据无效");
        return FAN_OFF;
    }

    // MODE_SAFE_STOP 模式下强制关闭
    if (mode == MODE_SAFE_STOP) {
        return FAN_OFF;
    }

    // MODE_LOCAL 模式下根据 CO2 决策
    if (mode == MODE_LOCAL) {
        return local_mode_decide(sensor->co2);
    }

    // MODE_NORMAL 或 MODE_DEGRADED 模式下基于 Benefit-Cost 决策
    // 注意: 系统模式契约保证 MODE_DEGRADED 时 weather->valid=true (缓存有效)

    // 1. 归一化计算
    float indoor_quality = (sensor->co2 - CO2_BASELINE) / CO2_RANGE;
    float outdoor_quality = weather->pm25 / PM25_RANGE;
    float temp_diff = fabsf(sensor->temperature - weather->temperature);

    // 2. Benefit-Cost 计算
    float benefit = indoor_quality * VENTILATION_BENEFIT_WEIGHT;
    float cost = outdoor_quality * VENTILATION_PM25_COST_WEIGHT +
                 temp_diff * VENTILATION_TEMP_COST_WEIGHT;
    float index = benefit - cost;

    // 3. 日志记录
    ESP_LOGI(TAG, "决策计算: indoor_q=%.2f outdoor_q=%.2f temp_diff=%.1f benefit=%.2f cost=%.2f index=%.2f",
             indoor_quality, outdoor_quality, temp_diff, benefit, cost, index);

    // 4. 决策逻辑
    if (index > VENTILATION_INDEX_HIGH) {
        ESP_LOGI(TAG, "通风指数%.2f > %.2f,启动高速风扇", index, VENTILATION_INDEX_HIGH);
        return FAN_HIGH;
    } else if (index > VENTILATION_INDEX_LOW) {
        ESP_LOGI(TAG, "通风指数%.2f > %.2f,启动低速风扇", index, VENTILATION_INDEX_LOW);
        return FAN_LOW;
    } else {
        ESP_LOGI(TAG, "通风指数%.2f <= %.2f,关闭风扇", index, VENTILATION_INDEX_LOW);
        return FAN_OFF;
    }
}

SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok) {
    // 系统模式检测
    if (!sensor_ok) {
        return MODE_SAFE_STOP;
    }

    // WiFi 连接正常
    if (wifi_ok) {
        return MODE_NORMAL;
    }

    // WiFi 断开但缓存有效期 <30min
    if (cache_valid) {
        return MODE_DEGRADED;
    }

    // 所有条件失败进入本地模式
    return MODE_LOCAL;
}
