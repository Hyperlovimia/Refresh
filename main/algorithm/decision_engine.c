/**
 * @file decision_engine.c
 * @brief ???????
 */

#include "decision_engine.h"
#include "local_mode.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "DECISION";

FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode) {
    if (!sensor || !sensor->valid) {
        ESP_LOGW(TAG, "???????");
        return FAN_OFF;
    }

    // MODE_SAFE_STOP???????
    if (mode == MODE_SAFE_STOP) {
        return FAN_OFF;
    }

    // MODE_LOCAL???? CO2 ??
    if (mode == MODE_LOCAL) {
        return local_mode_decide(sensor->co2);
    }

    // MODE_NORMAL ? MODE_DEGRADED?????????
    // TODO: ?? - Benefit-Cost ??
    // ???? CO2 ????
    if (sensor->co2 > CO2_THRESHOLD_HIGH) {
        return FAN_HIGH;
    } else if (sensor->co2 > CO2_THRESHOLD_LOW) {
        return FAN_LOW;
    } else {
        return FAN_OFF;
    }
}

SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok) {
    // ???????
    if (!sensor_ok) {
        return MODE_SAFE_STOP;
    }

    // WiFi ???????
    if (wifi_ok) {
        return MODE_NORMAL;
    }

    // WiFi ??????? <30min
    if (cache_valid) {
        return MODE_DEGRADED;
    }

    // ??????????
    return MODE_LOCAL;
}
